#include "runner.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <exception>
#include <functional>
#include <thread>
#include <utility>
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

// The measurement turn: at most one timed measurement runs at a time,
// program-wide. A plain mutex isn't enough: the benchmark releases it and
// re-takes it within microseconds, so a waiting row would almost never get
// in. Waiters therefore queue, and the turn goes to the front of the queue.
std::mutex g_turnMutex;
std::condition_variable g_turnFreed;
bool g_turnBusy = false;
std::deque<unsigned long long> g_turnQueue; // ids of waiting threads, oldest first
unsigned long long g_nextWaiterId = 0;

} // namespace

MeasurementTurn::MeasurementTurn(MeasurementTurn&& other) noexcept
    : owns_(std::exchange(other.owns_, false)) {}

MeasurementTurn& MeasurementTurn::operator=(MeasurementTurn&& other) noexcept {
    if (this != &other) {
        unlock();
        owns_ = std::exchange(other.owns_, false);
    }
    return *this;
}

void MeasurementTurn::unlock() {
    if (!owns_) return;
    owns_ = false;
    {
        std::lock_guard<std::mutex> lk(g_turnMutex);
        g_turnBusy = false;
    }
    g_turnFreed.notify_all();
}

MeasurementTurn AcquireMeasurementTurn(const std::atomic<bool>& cancel,
                                       const std::function<void()>& onWait) {
    std::unique_lock<std::mutex> lk(g_turnMutex);
    if (!g_turnBusy && g_turnQueue.empty()) {
        g_turnBusy = true;
        return MeasurementTurn(true);
    }

    // Busy, or others are already waiting: join the back of the queue.
    const unsigned long long id = g_nextWaiterId++;
    g_turnQueue.push_back(id);
    if (onWait) {
        lk.unlock();
        onWait();
        lk.lock();
    }
    for (;;) {
        if (cancel.load()) {
            // Leave the queue so the threads behind us aren't stuck.
            g_turnQueue.erase(std::find(g_turnQueue.begin(), g_turnQueue.end(), id));
            lk.unlock();
            g_turnFreed.notify_all();
            return MeasurementTurn(false);
        }
        if (!g_turnBusy && g_turnQueue.front() == id) {
            g_turnQueue.pop_front();
            g_turnBusy = true;
            return MeasurementTurn(true);
        }
        // Timed wait so a cancel is noticed within a few milliseconds.
        g_turnFreed.wait_for(lk, std::chrono::milliseconds(2));
    }
}

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

namespace {
    
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
        // Wait for any other measurement (another row, or the benchmark) to
        // finish, so this one is timed on an otherwise idle program.
        MeasurementTurn turn = AcquireMeasurementTurn(run->cancel, [&] {
            slot.state.store(SlotState::Queued, std::memory_order_release);
        });
        if (!turn.owns_lock()) {
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
