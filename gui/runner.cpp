#include "runner.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <thread>
#include <vector>

const std::array<AlgorithmInfo, kAlgorithmCount> kAlgorithms = {{
    {"Miller-Rabin (fixed)", "Atnm", &millerRabinFixedIsPlaceholder},
    {"Miller-Rabin (random)", "Dino", &millerRabinRandomIsPlaceholder},
    {"AKS", "Finley", &aksIsPlaceholder},
}};

long long SteadyNowNs() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

namespace {

// Stops the compiler from deleting calls whose results are never used.
volatile int g_sink = 0;

struct Measurement {
    Verdict verdict = Verdict::Neither;
    double secondsPerCall = 0.0;
    long long calls = 1;
};

// One timed call first. If it took under 1 ms, a single reading would be
// mostly timer noise and cold-cache cost, so (if enabled) run 7 batches,
// each sized to take roughly 1 ms, and report the median per-call time.
// Slow calls (AKS on large n) are reported as a single run.
Measurement Measure(const std::function<Verdict()>& call, const std::atomic<bool>& cancel, bool repeatFast) {
    using clock = std::chrono::steady_clock;
    Measurement m;

    auto t0 = clock::now();
    m.verdict = call();
    double first = std::chrono::duration<double>(clock::now() - t0).count();
    m.secondsPerCall = first;
    m.calls = 1;

    constexpr double kFastThreshold = 1e-3;
    if (!repeatFast || first >= kFastThreshold) return m;

    const long long batchSize = std::clamp<long long>(
        static_cast<long long>(kFastThreshold / std::max(first, 1e-9)), 1, 1'000'000);
    std::vector<double> perCall;
    for (int b = 0; b < 7 && !cancel.load(); b++) {
        auto bt0 = clock::now();
        int acc = 0;
        for (long long i = 0; i < batchSize; i++) acc += static_cast<int>(call());
        double elapsed = std::chrono::duration<double>(clock::now() - bt0).count();
        g_sink = g_sink + acc;
        perCall.push_back(elapsed / static_cast<double>(batchSize));
    }
    if (perCall.empty()) return m;
    std::nth_element(perCall.begin(), perCall.begin() + perCall.size() / 2, perCall.end());
    m.secondsPerCall = perCall[perCall.size() / 2];
    m.calls = batchSize * static_cast<long long>(perCall.size());
    return m;
}

void Worker(std::shared_ptr<Run> run) {
    const std::uint64_t n = run->n;
    const RunSettings s = run->settings;
    std::mt19937_64 rng(s.seed);

    const std::array<std::function<Verdict()>, kAlgorithmCount> calls = {
        [n] { return millerRabinFixed(n); },
        [n, &rng, &s] { return millerRabinRandom(n, s.mrRounds, rng); },
        [n] { return aks(n); },
    };

    for (int i = 0; i < kAlgorithmCount; i++) {
        AlgoSlot& slot = run->slots[i];
        if (run->cancel.load()) {
            slot.state.store(SlotState::Cancelled, std::memory_order_release);
            continue;
        }
        slot.startNs.store(SteadyNowNs());
        slot.state.store(SlotState::Running, std::memory_order_release);
        try {
            Measurement m = Measure(calls[i], run->cancel, s.repeatFastRuns);
            // A call can't be interrupted part-way; if Cancel was pressed
            // while it ran, its (late) result is discarded.
            if (run->cancel.load()) {
                slot.state.store(SlotState::Cancelled, std::memory_order_release);
                continue;
            }
            slot.verdict = m.verdict;
            slot.secondsPerCall = m.secondsPerCall;
            slot.calls = m.calls;
            slot.state.store(SlotState::Done, std::memory_order_release);
        } catch (const std::exception& e) {
            slot.error = e.what();
            slot.state.store(SlotState::Error, std::memory_order_release);
        } catch (...) {
            slot.error = "unknown exception";
            slot.state.store(SlotState::Error, std::memory_order_release);
        }
    }
}

} // namespace

std::shared_ptr<Run> StartRun(std::uint64_t n, const RunSettings& settings) {
    auto run = std::make_shared<Run>();
    run->n = n;
    run->settings = settings;
    std::thread(Worker, run).detach();
    return run;
}
