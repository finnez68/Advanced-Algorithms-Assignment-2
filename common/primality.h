// primality.h
//
// The shared interface every primality algorithm in this repo implements.
// This is the contract between the algorithm modules and everything that
// uses them (the GUI, the tests, and later the benchmark harness), so
// change it only by agreement with the whole group.
//
// All algorithms take n as an unsigned 64-bit integer. That range is
// deliberate: the fixed witness sets used by millerRabinFixed() are only
// PROVEN correct below a bound, and the standard sets cover all of 64-bit.

#pragma once
#include <atomic>
#include <cstdint>
#include <exception>
#include <random>
#include <string>

enum class Verdict {
    Neither,       // n == 1 (and n == 0, though the GUI never passes 0): neither prime nor composite
    Prime,         // definitely prime
    Composite,     // definitely composite
    ProbablyPrime, // passed every randomised round -- prime with bounded error probability
};

const char* ToString(Verdict v);

// "Prime" and "ProbablyPrime" count as the same answer when checking
// whether algorithms agree with each other.
inline Verdict Normalised(Verdict v) { return v == Verdict::ProbablyPrime ? Verdict::Prime : v; }

// --- Miller-Rabin with fixed witness sets ---------------- author: Atnm ---
// Deterministic for every 64-bit n, given a proven witness set.
Verdict millerRabinFixed(std::uint64_t n);

// --- Miller-Rabin with random bases ---------------------- author: Dino ---
// `rounds` independent random bases drawn from `rng`. Returns ProbablyPrime
// when n passes every round (error probability <= 4^-rounds); a definite
// Prime only for n = 2 and 3, which are decided before any rounds run.
// The caller owns the RNG so runs can be made reproducible with a seed.
Verdict millerRabinRandom(std::uint64_t n, int rounds, std::mt19937_64& rng);

// --- AKS ------------------------------------------------- author: Finley -
// Deterministic, polynomial time, impractically slow. Implemented in
// AKS/aks.cpp; common/aks_adapter.cpp exposes it through this interface.
Verdict aks(std::uint64_t n);

// Thrown by the cancellable calls below when their cancel flag is set.
struct AlgorithmCancelled : std::exception {
    const char* what() const noexcept override { return "cancelled"; }
};

// Same as aks(), but stops within about 0.1 s of `cancel` becoming true by
// throwing AlgorithmCancelled. Used by the GUI so Cancel really stops AKS.
Verdict aksCancellable(std::uint64_t n, const std::atomic<bool>& cancel);

// AKS's internal statistics for one call, for the benchmark CSV.
struct AksInfo {
    std::uint64_t r = 0;        // modulus exponent found in step 2
    std::uint64_t bound = 0;    // congruences step 5 would check for a prime
    std::uint64_t checked = 0;  // congruences actually evaluated
    std::string step;           // which step decided the answer
};
// Same as aks(), but also fills `info`. Reads aks.cpp's global stats, so
// call it from one thread at a time (the benchmark uses a single worker).
// If `cancel` is given, it can be stopped like aksCancellable().
Verdict aksWithInfo(std::uint64_t n, AksInfo& info, const std::atomic<bool>* cancel = nullptr);

// --- Trial division (reference oracle, not one of the three) ------------
// Simple and obviously correct. Used by the tests to check the other three,
// and as the temporary stand-in body for any algorithm not written yet.
Verdict trialDivision(std::uint64_t n);

// Each algorithm module reports whether it still contains the temporary
// placeholder body (which just calls trialDivision). The GUI labels those
// rows "(placeholder)" so nobody mistakes a stand-in for the real thing.
// Flip yours to `return false;` when your real implementation goes in.
bool millerRabinFixedIsPlaceholder();
bool millerRabinRandomIsPlaceholder();
bool aksIsPlaceholder();