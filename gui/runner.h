// runner.h -- runs the three algorithms on one number, off the UI thread.
//
// Pressing Go! creates a Run and starts one background thread for it. The
// thread runs the algorithms one after another (fixed, random, then AKS),
// so their timings don't compete with each other for the CPU. The UI only
// reads a slot's result fields after it sees that slot's state become Done
// or Error (an acquire load pairing with the worker's release store), so
// no locks are needed.

#pragma once
#include "primality.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

enum class SlotState : int { Pending, Running, Done, Error, Cancelled };

struct AlgoSlot {
    std::atomic<SlotState> state{SlotState::Pending};
    std::atomic<long long> startNs{0}; // steady_clock time the algorithm started, for the live timer

    // Written by the worker before it publishes Done/Error; read-only after.
    Verdict verdict = Verdict::Neither;
    double secondsPerCall = 0.0;
    long long calls = 0;    // 1 = single run; >1 = median of repeated batches
    std::string error;
};

struct RunSettings {
    int mrRounds = 20;
    std::uint64_t seed = 0;     // seed actually used for the random Miller-Rabin RNG
    bool repeatFastRuns = true; // repeat sub-millisecond runs and report the median
};

constexpr int kAlgorithmCount = 3;

struct Run {
    std::uint64_t n = 0;
    RunSettings settings;
    std::atomic<bool> cancel{false};
    std::array<AlgoSlot, kAlgorithmCount> slots;
};

struct AlgorithmInfo {
    const char* name;
    const char* author;
    bool (*isPlaceholder)();
};
extern const std::array<AlgorithmInfo, kAlgorithmCount> kAlgorithms;

// Starts the background thread and returns immediately. The thread keeps
// its own reference to the Run, so the UI can drop it at any time.
std::shared_ptr<Run> StartRun(std::uint64_t n, const RunSettings& settings);

long long SteadyNowNs();
