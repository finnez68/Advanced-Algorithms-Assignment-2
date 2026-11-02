#include "benchmark.h"
#include "runner.h"
#include <algorithm>
#include <cstdio>
#include <thread>

namespace {

using u64 = std::uint64_t;

// millerRabinFixed is exact for every 64-bit n (checked by the test suite),
// so it is used to pick the inputs and their expected answers.
bool IsPrime(u64 n) { return millerRabinFixed(n) == Verdict::Prime; }

u64 LargestPrimeAtMost(u64 n) {
    while (n >= 2 && !IsPrime(n)) n--;
    return n;
}

int BitLength(u64 n) {
    int b = 0;
    while (n) { b++; n >>= 1; }
    return b;
}

void Add(std::vector<BenchInput>& v, u64 n, const char* type) {
    v.push_back({n, BitLength(n), type, millerRabinFixed(n)});
}

void BenchWorker(std::shared_ptr<BenchmarkRun> run) {
    const BenchSettings& s = run->settings;
    for (size_t i = 0; i < run->inputs.size() && !run->cancel.load(); i++) {
        run->current.store(static_cast<int>(i));
        const BenchInput& in = run->inputs[i];
        const u64 n = in.n;

        auto record = [&](const char* algorithm, int rounds, const Measurement& m) -> BenchRow& {
            BenchRow r;
            r.input = in;
            r.algorithm = algorithm;
            r.rounds = rounds;
            r.verdict = m.verdict;
            r.correct = Normalised(m.verdict) == in.expected;
            r.timeNs = m.secondsPerCall * 1e9;
            r.calls = m.calls;
            run->rows.push_back(std::move(r));
            return run->rows.back();
        };

        // Every measurement holds the program-wide measurement lock, so a row's
        // Go! never runs at the same time (it waits as "queued", and this
        // waits for it in turn). Returns a lock that doesn't own the turn if
        // the benchmark was cancelled while waiting.
        auto takeTurn = [&] {
            auto turn = AcquireMeasurementTurn(run->cancel, [&] { run->waitingForRow.store(true); });
            run->waitingForRow.store(false);
            return turn;
        };

        {
            auto turn = takeTurn();
            if (!turn.owns_lock()) break;
            record("Miller-Rabin (fixed)", 0,
                   Measure([n] { return millerRabinFixed(n); }, run->cancel, true));
        }

        for (int t : s.rounds) {
            auto turn = takeTurn();
            if (!turn.owns_lock()) break;
            std::mt19937_64 rng(s.seed);  // same seed for every (n, t): reproducible
            record("Miller-Rabin (random)", t,
                   Measure([n, t, &rng] { return millerRabinRandom(n, t, rng); }, run->cancel, true));
        }

        const int aksLimit = in.expected == Verdict::Prime ? s.aksMaxPrimeBits : s.aksMaxCompositeBits;
        if (in.bits <= aksLimit && !run->cancel.load()) {
            auto turn = takeTurn();
            if (!turn.owns_lock()) break;
            try {
                const Measurement m = Measure([&] { return aksCancellable(n, run->cancel); }, run->cancel, true);
                // Untimed extra call that only reads r, bound, checked and step.
                // It stays inside the same turn because those come from aks.cpp's
                // shared statistics, which another AKS call would overwrite.
                AksInfo info;
                aksWithInfo(n, info, &run->cancel);
                BenchRow& r = record("AKS", 0, m);
                r.hasAks = true;
                r.aks = info;
            } catch (const AlgorithmCancelled&) {
                break;  // benchmark cancelled during AKS: stop without a partial row
            }
        }
    }
    run->done.store(true, std::memory_order_release);
}

} // namespace

std::vector<BenchInput> MakeBenchmarkInputs(int bitStep) {
    std::vector<BenchInput> v;
    for (int b = 8; b <= 64; b += std::max(1, bitStep)) {
        const u64 top = b == 64 ? ~0ULL : (1ULL << b) - 1;  // largest b-bit value
        Add(v, LargestPrimeAtMost(top), "prime");

        // Two primes just below 2^(b/2): the hardest case for trial division.
        const u64 q1 = LargestPrimeAtMost((1ULL << (b / 2)) - 1);
        const u64 q2 = LargestPrimeAtMost(q1 - 1);
        Add(v, q1 * q2, "semiprime");

        Add(v, 3 * LargestPrimeAtMost(top / 3), "small factor");
    }

    const struct { u64 n; const char* type; } hard[] = {
        {561, "Carmichael"}, {1105, "Carmichael"}, {8911, "Carmichael"}, {825265, "Carmichael"},
        {703, "high strong-liar rate"},
        {2047, "strong pseudoprime base 2"},
        {1373653, "strong pseudoprime bases 2,3"},
        {25326001, "strong pseudoprime bases 2,3,5"},
        {3215031751ULL, "strong pseudoprime bases 2,3,5,7"},
        {3825123056546413051ULL, "strong pseudoprime bases 2..31"},
        {18446744030759878681ULL, "prime square"},
    };
    for (const auto& h : hard) Add(v, h.n, h.type);
    return v;
}

std::shared_ptr<BenchmarkRun> StartBenchmark(const BenchSettings& settings) {
    auto run = std::make_shared<BenchmarkRun>();
    run->settings = settings;
    run->inputs = MakeBenchmarkInputs(settings.bitStep);  // fast: a few hundred Miller-Rabin calls
    std::thread(BenchWorker, run).detach();
    return run;
}

bool WriteBenchmarkCsv(const BenchmarkRun& run, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    // Build details as comment lines (pandas: pd.read_csv(path, comment='#')).
#if defined(__clang__)
    std::fprintf(f, "# compiler: Clang %s\n", __clang_version__);
#elif defined(__GNUC__)
    std::fprintf(f, "# compiler: GCC %s\n", __VERSION__);
#elif defined(_MSC_VER)
    std::fprintf(f, "# compiler: MSVC %d\n", _MSC_VER);
#endif
#ifdef __SIZEOF_INT128__
    std::fprintf(f, "# mulmod: unsigned __int128\n");
#else
    std::fprintf(f, "# mulmod: double-and-add fallback\n");
#endif
#ifdef NDEBUG
    std::fprintf(f, "# build: optimised\n");
#else
    std::fprintf(f, "# build: debug -- timings not meaningful\n");
#endif
    std::fprintf(f, "# seed: %llu\n", static_cast<unsigned long long>(run.settings.seed));

    std::fprintf(f, "n,bits,type,expected,algorithm,rounds,verdict,correct,time_ns,calls,"
                    "aks_r,aks_bound,aks_congruences_checked,aks_deciding_step\n");
    for (const BenchRow& r : run.rows) {
        std::fprintf(f, "%llu,%d,\"%s\",%s,%s,%d,%s,%d,%.1f,%lld,",
                     static_cast<unsigned long long>(r.input.n), r.input.bits, r.input.type.c_str(),
                     ToString(r.input.expected), r.algorithm.c_str(), r.rounds,
                     ToString(r.verdict), r.correct ? 1 : 0, r.timeNs, r.calls);
        if (r.hasAks)
            std::fprintf(f, "%llu,%llu,%llu,\"%s\"\n", static_cast<unsigned long long>(r.aks.r),
                         static_cast<unsigned long long>(r.aks.bound),
                         static_cast<unsigned long long>(r.aks.checked), r.aks.step.c_str());
        else
            std::fprintf(f, ",,,\n");  // Miller-Rabin rows leave the AKS columns empty
    }
    std::fclose(f);
    return true;
}