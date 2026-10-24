// Test and data-collection harness for the Miller-Rabin implementation in mr.cpp.
//
// Build:  g++ -O2 -std=c++17 -o mr_test mr_testing.cpp
// Run:    ./mr_test [--limit N] [--max-bits B] [--k K] [--csv FILE]
//
//   --limit N     exhaustively test every n in [0, N]            (default 2000)
//   --max-bits B  scaling test up to B-bit inputs, 8 <= B <= 48  (default 22)
//   --k K         number of random-base rounds per test          (default 20)
//   --csv FILE    write one row per test to FILE                 (default mr_results.csv)
//
// Three test groups, each tagged in the CSV "group" column (same as aks_testing.cpp):
//   exhaustive  every n in [0, N], compared with a sieve of Eratosthenes
//   hard        named edge cases: Carmichael numbers, strong pseudoprimes,
//               prime powers, products of close primes, Mersenne primes, ...
//   scaling     for each bit length: the largest prime below 2^bits, a prime
//               square, a product of two primes near sqrt(2^bits), and a
//               composite with a small factor
//
// Ground truth for the hard and scaling groups comes from trial division.
// The program exits with status 1 if any test disagrees with ground truth.
// Note: Miller-Rabin can in principle return a false "prime" for a composite
// (probability <= 4^-k), so a FAIL line is rare but not impossible for small k.
//
// Output: a CSV with per-test data (n, expected, got, correct, k, rounds used,
// witness, deciding step, time in microseconds) and a printed summary.

#define MR_NO_MAIN
#include "mr.cpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

static double seconds_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

static int g_k = 20;

// ----------------------------------------------------------- ground truth

static bool trial_division_truth(u64 n) {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0) return false;
    for (u64 d = 3; d * d <= n; d += 2)
        if (n % d == 0) return false;
    return true;
}

static u64 largest_prime_below(u64 n) {
    for (u64 m = n - 1; m >= 2; m--)
        if (trial_division_truth(m)) return m;
    return 2;
}

static u64 smallest_prime_above(u64 n) {
    for (u64 m = n + 1;; m++)
        if (trial_division_truth(m)) return m;
}

// ---------------------------------------------------------------- results

struct TestRecord {
    std::string group;
    std::string label;
    u64 n = 0;
    bool expected = false;
    bool got = false;
    int k = 0;
    u64 rounds = 0;
    u64 witness = 0;
    std::string step;
    double micros = 0;
    int bits = 0;
    bool correct() const { return expected == got; }
};

static std::vector<TestRecord> g_records;

static int bit_length(u64 n) {
    int b = 0;
    while (n) { b++; n >>= 1; }
    return b;
}

static void run_test(const std::string& group, const std::string& label, u64 n, bool expected) {
    auto t0 = std::chrono::steady_clock::now();
    bool got = mr_random(n, g_k);
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();

    TestRecord rec;
    rec.group = group;
    rec.label = label;
    rec.n = n;
    rec.expected = expected;
    rec.got = got;
    rec.k = g_k;
    rec.rounds = g_stats.rounds;
    rec.witness = g_stats.witness;
    rec.step = g_stats.reason;
    rec.micros = us;
    rec.bits = bit_length(n);
    g_records.push_back(rec);

    if (!rec.correct())
        std::printf("FAIL  %-10s %-28s n=%llu expected=%s got=%s\n", group.c_str(), label.c_str(),
                    (unsigned long long)n, expected ? "prime" : "composite", got ? "prime" : "composite");
}

// ------------------------------------------------------------ test groups

static void test_exhaustive(u64 limit) {
    std::vector<bool> is_p(limit + 1, true);
    is_p[0] = false;
    if (limit >= 1) is_p[1] = false;
    for (u64 i = 2; i * i <= limit; i++)
        if (is_p[i])
            for (u64 j = i * i; j <= limit; j += i) is_p[j] = false;
    for (u64 n = 0; n <= limit; n++) run_test("exhaustive", "all n", n, is_p[n]);
}

static void test_hard_cases() {
    struct Case { u64 n; const char* label; };
    const Case cases[] = {
        {0, "zero"}, {1, "one"}, {2, "smallest prime"}, {3, "prime"}, {4, "smallest composite"},
        {561, "Carmichael"}, {1105, "Carmichael"}, {1729, "Carmichael (Hardy-Ramanujan)"},
        {2465, "Carmichael"}, {2821, "Carmichael"}, {6601, "Carmichael"}, {8911, "Carmichael"},
        {41041, "Carmichael"}, {825265, "Carmichael"},
        {2047, "strong pseudoprime base 2"}, {3277, "strong pseudoprime base 2"},
        {1373653, "strong pseudoprime bases 2,3"}, {25326001, "strong pseudoprime bases 2,3,5"},
        {3215031751ULL, "strong pseudoprime bases 2,3,5,7"},
        {1024, "power of two"}, {3125, "5^5"}, {6561, "3^8"}, {10201, "101^2"},
        {127, "Mersenne prime 2^7-1"}, {8191, "Mersenne prime 2^13-1"},
        {131071, "Mersenne prime 2^17-1"}, {524287, "Mersenne prime 2^19-1"},
        {10007, "prime"}, {100003, "prime"},
        {10403, "101 * 103 (twin primes)"}, {1022117, "1009 * 1013"},
    };
    for (const Case& c : cases) run_test("hard", c.label, c.n, trial_division_truth(c.n));
}

static void test_scaling(int max_bits) {
    for (int bits = 8; bits <= max_bits; bits++) {
        const u64 top = 1ULL << bits;
        const u64 p = largest_prime_below(top);
        run_test("scaling", "largest prime below 2^bits", p, true);

        u64 s = (u64)std::sqrt((long double)top);
        u64 sp = largest_prime_below(s);
        run_test("scaling", "prime square", sp * sp, false);

        // Product of two primes near sqrt(2^bits): hardest composites for trial division.
        u64 q1 = largest_prime_below(s), q2 = smallest_prime_above(s);
        if (q1 != q2) run_test("scaling", "semiprime near sqrt", q1 * q2, false);

        // Composite with a tiny factor.
        run_test("scaling", "3 * prime", 3 * largest_prime_below(top / 3), false);
    }
}

// --------------------------------------------------------------- reporting

static double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    double idx = p * (v.size() - 1);
    size_t lo = (size_t)idx;
    size_t hi = std::min(lo + 1, v.size() - 1);
    return v[lo] + (v[hi] - v[lo]) * (idx - lo);
}

static void print_time_stats(const char* name, const std::vector<double>& us) {
    if (us.empty()) return;
    double sum = 0;
    for (double x : us) sum += x;
    std::printf("  %-22s n=%-6zu min=%-10.1f median=%-10.1f p95=%-10.1f max=%-12.1f total=%.3fs\n", name,
                us.size(), percentile(us, 0), percentile(us, 0.5), percentile(us, 0.95), percentile(us, 1.0),
                sum / 1e6);
}

static void write_csv(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        std::printf("Could not open %s for writing\n", path.c_str());
        return;
    }
    std::fprintf(f, "group,label,n,bits,expected,got,correct,k,rounds,witness,deciding_step,time_us\n");
    for (const TestRecord& t : g_records)
        std::fprintf(f, "%s,\"%s\",%llu,%d,%s,%s,%d,%d,%llu,%llu,\"%s\",%.1f\n", t.group.c_str(),
                     t.label.c_str(), (unsigned long long)t.n, t.bits, t.expected ? "prime" : "composite",
                     t.got ? "prime" : "composite", t.correct() ? 1 : 0, t.k, (unsigned long long)t.rounds,
                     (unsigned long long)t.witness, t.step.c_str(), t.micros);
    std::fclose(f);
}

static void print_summary(const std::string& csv_path) {
    std::printf("\n============== MILLER-RABIN TEST SUMMARY (k = %d) ==============\n", g_k);

    std::map<std::string, std::pair<int, int>> by_group;  // group -> (passed, total)
    for (const TestRecord& t : g_records) {
        by_group[t.group].second++;
        if (t.correct()) by_group[t.group].first++;
    }
    int total = 0, passed = 0;
    std::printf("\nCorrectness\n");
    for (auto& kv : by_group) {
        std::printf("  %-12s %d / %d passed\n", kv.first.c_str(), kv.second.first, kv.second.second);
        total += kv.second.second;
        passed += kv.second.first;
    }
    std::printf("  %-12s %d / %d passed  -> %s\n", "ALL", passed, total, passed == total ? "OK" : "FAILURES");

    std::map<std::string, int> steps;
    for (const TestRecord& t : g_records) steps[t.step]++;
    std::printf("\nDeciding step (how each answer was reached)\n");
    for (auto& kv : steps) std::printf("  %-34s %d\n", kv.first.c_str(), kv.second);

    // Rounds used by composites: how quickly witnesses are found
    std::map<u64, int> round_hist;
    for (const TestRecord& t : g_records)
        if (!t.expected && t.step == "witness found") round_hist[t.rounds]++;
    if (!round_hist.empty()) {
        std::printf("\nRounds needed to expose a composite (witness found)\n");
        for (auto& kv : round_hist) std::printf("  round %-4llu %d composites\n", (unsigned long long)kv.first, kv.second);
    }

    std::vector<double> all, primes, comps;
    for (const TestRecord& t : g_records) {
        all.push_back(t.micros);
        (t.expected ? primes : comps).push_back(t.micros);
    }
    std::printf("\nTiming (microseconds)\n");
    print_time_stats("all tests", all);
    print_time_stats("primes", primes);
    print_time_stats("composites", comps);

    struct Row { double prime_us = 0, comp_us = 0; u64 prime_rounds = 0; std::string comp_step; };
    std::map<int, Row> rows;
    for (const TestRecord& t : g_records) {
        if (t.group != "scaling") continue;
        Row& row = rows[t.bits];
        if (t.expected) {
            row.prime_us = std::max(row.prime_us, t.micros);
            row.prime_rounds = t.rounds;
        } else if (t.micros >= row.comp_us) {
            row.comp_us = t.micros;
            row.comp_step = t.step;
        }
    }
    if (!rows.empty()) {
        std::printf("\nScaling by bit length (scaling group; times in microseconds)\n");
        std::printf("  %4s  %8s  %12s  %14s  %s\n", "bits", "rounds", "prime time", "slowest comp.",
                    "slowest composite decided by");
        for (auto& kv : rows)
            std::printf("  %4d  %8llu  %12.0f  %14.0f  %s\n", kv.first, (unsigned long long)kv.second.prime_rounds,
                        kv.second.prime_us, kv.second.comp_us, kv.second.comp_step.c_str());
    }

    std::vector<const TestRecord*> sorted;
    for (const TestRecord& t : g_records) sorted.push_back(&t);
    std::sort(sorted.begin(), sorted.end(),
              [](const TestRecord* a, const TestRecord* b) { return a->micros > b->micros; });
    std::printf("\nSlowest 5 tests\n");
    for (size_t i = 0; i < sorted.size() && i < 5; i++) {
        const TestRecord* t = sorted[i];
        std::printf("  n=%-14llu %-9s rounds=%-3llu %10.0f us  %s\n", (unsigned long long)t->n,
                    t->expected ? "prime" : "composite", (unsigned long long)t->rounds, t->micros,
                    t->label.c_str());
    }

    std::printf("\nPer-test data written to %s (%zu rows)\n", csv_path.c_str(), g_records.size());
}

// -------------------------------------------------------------------- main

int main(int argc, char** argv) {
    u64 limit = 2000;
    int max_bits = 22;
    std::string csv_path = "mr_results.csv";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--limit" && i + 1 < argc) limit = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--max-bits" && i + 1 < argc) max_bits = std::atoi(argv[++i]);
        else if (a == "--k" && i + 1 < argc) g_k = std::max(1, std::atoi(argv[++i]));
        else if (a == "--csv" && i + 1 < argc) csv_path = argv[++i];
        else {
            std::printf("Usage: %s [--limit N] [--max-bits B] [--k K] [--csv FILE]\n", argv[0]);
            return 2;
        }
    }
    max_bits = std::max(8, std::min(48, max_bits));

    std::printf("Miller-Rabin tests (k = %d): exhaustive 0..%llu, hard cases, scaling to %d bits\n", g_k,
                (unsigned long long)limit, max_bits);

    auto t0 = std::chrono::steady_clock::now();
    test_exhaustive(limit);
    std::printf("  exhaustive group done\n");
    test_hard_cases();
    std::printf("  hard cases done\n");
    test_scaling(max_bits);
    std::printf("  scaling group done (%.1fs elapsed)\n", seconds_since(t0));

    write_csv(csv_path);
    print_summary(csv_path);

    for (const TestRecord& t : g_records)
        if (!t.correct()) return 1;
    return 0;
}
