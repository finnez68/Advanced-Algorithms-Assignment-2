// benchmark.h -- runs a fixed suite of inputs through all three algorithms,
// timed the same way as the GUI rows, and writes one CSV row per
// (input, algorithm, rounds).

#pragma once
#include "primality.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct BenchInput {
    std::uint64_t n = 0;
    int bits = 0;
    std::string type;                  // "prime", "semiprime", "small factor", or a hard-case label
    Verdict expected = Verdict::Neither;
};

struct BenchRow {
    BenchInput input;
    std::string algorithm;
    int rounds = 0;                    // random Miller-Rabin only
    Verdict verdict = Verdict::Neither;
    bool correct = false;
    double timeNs = 0;
    long long calls = 0;
    bool hasAks = false;               // AKS rows only
    AksInfo aks;
};

struct BenchSettings {
    int bitStep = 4;                   // bit lengths 8, 8 + step, ..., up to 64
    int aksMaxPrimeBits = 16;          // AKS on larger primes takes minutes each
    int aksMaxCompositeBits = 32;
    std::vector<int> rounds = {1, 5, 10, 20, 40};
    std::uint64_t seed = 12345;
};

struct BenchmarkRun {
    BenchSettings settings;
    std::vector<BenchInput> inputs;    // fixed before the worker starts
    std::atomic<int> current{0};       // index of the input being measured
    std::atomic<bool> cancel{false};
    std::atomic<bool> done{false};
    std::atomic<bool> waitingForRow{false}; // waiting for a row's Go! measurement to finish
    std::vector<BenchRow> rows;        // written by the worker; read only once done is true
};

std::vector<BenchInput> MakeBenchmarkInputs(int bitStep);
std::shared_ptr<BenchmarkRun> StartBenchmark(const BenchSettings& settings);
bool WriteBenchmarkCsv(const BenchmarkRun& run, const std::string& path);
