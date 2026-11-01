// test_algorithms.cpp -- checks every algorithm against trial division.
//
// The hard cases are the composites that fool weak primality tests
// (Carmichael numbers, strong pseudoprimes), prime powers, and primes near
// 2^64 where 64-bit overflow in modular multiplication would show up.
// The MillerRabinRandom tests also check the properties specific to random
// bases: one-sided error, the 1/4 strong-liar bound, and seed reproducibility.

#include "primality.h"
#include <gtest/gtest.h>
#include <vector>

namespace {

const std::vector<std::uint64_t> kPrimes = {
    2, 3, 5, 7, 13, 97, 7919, 1000000007ULL,
    127, 8191, 131071, 524287,    // Mersenne primes
    10007, 100003,
    4294967291ULL,                // largest 32-bit prime
    2305843009213693951ULL,       // 2^61 - 1
    18446744073709551557ULL,      // largest 64-bit prime
};

const std::vector<std::uint64_t> kComposites = {
    4, 9, 15, 561, 1105, 1729, 2465, 2821, 6601, 8911, // Carmichael numbers
    703,                                                // 19 * 37, about 23% strong liars (near the 1/4 bound)
    2047, 3277, 4033, 4681, 8321,                       // strong pseudoprimes to base 2
    1373653ULL,                                         // strong pseudoprime to bases 2, 3
    25326001ULL,                                        // strong pseudoprime to bases 2, 3, 5
    3215031751ULL,                                      // strong pseudoprime to bases 2, 3, 5, 7
    18446743979220271189ULL,                            // 4294967291 * 4294967279
    18446744073709551615ULL,                            // 2^64 - 1
    41041, 825265,                                      // more Carmichael numbers
    1024, 3125, 6561, 10201,                            // prime powers
    10403, 1022117,                                     // products of close primes
    3825123056546413051ULL,                             // strong pseudoprime to every prime base 2..31
    18446744030759878681ULL,                            // 4294967291^2
};

} // namespace

TEST(TrialDivision, SmallCases) {
    EXPECT_EQ(trialDivision(1), Verdict::Neither);
    EXPECT_EQ(trialDivision(2), Verdict::Prime);
    EXPECT_EQ(trialDivision(4), Verdict::Composite);
    for (auto p : kPrimes) EXPECT_EQ(trialDivision(p), Verdict::Prime) << p;
    for (auto c : kComposites) EXPECT_EQ(trialDivision(c), Verdict::Composite) << c;
}

TEST(MillerRabinFixed, AgreesWithTrialDivisionUpTo200000) {
    for (std::uint64_t n = 1; n <= 200000; n++)
        ASSERT_EQ(millerRabinFixed(n), trialDivision(n)) << "n = " << n;
}

TEST(MillerRabinFixed, HardCases) {
    for (auto p : kPrimes) EXPECT_EQ(millerRabinFixed(p), Verdict::Prime) << p;
    for (auto c : kComposites) EXPECT_EQ(millerRabinFixed(c), Verdict::Composite) << c;
}

TEST(MillerRabinRandom, AgreesWithTrialDivisionUpTo200000) {
    std::mt19937_64 rng(12345);
    for (std::uint64_t n = 1; n <= 200000; n++)
        ASSERT_EQ(Normalised(millerRabinRandom(n, 20, rng)), trialDivision(n)) << "n = " << n;
}

TEST(MillerRabinRandom, NeverClaimsCertaintyAbove3) {
    // 2 and 3 are handled before any random rounds run, so a definite
    // "Prime" is correct there. Every larger prime goes through the random
    // rounds and must only ever be reported as "probably" prime.
    std::mt19937_64 rng(12345);
    for (auto p : kPrimes) {
        if (p < 4) {
            EXPECT_EQ(Normalised(millerRabinRandom(p, 20, rng)), Verdict::Prime) << p;
        } else {
            EXPECT_EQ(millerRabinRandom(p, 20, rng), Verdict::ProbablyPrime) << p;
        }
    }
}

TEST(MillerRabinRandom, HardCases) {
    std::mt19937_64 rng(12345);
    for (auto c : kComposites) EXPECT_EQ(millerRabinRandom(c, 20, rng), Verdict::Composite) << c;
}

TEST(MillerRabinRandom, EdgeCases) {
    std::mt19937_64 rng(1);
    EXPECT_EQ(millerRabinRandom(0, 20, rng), Verdict::Neither);
    EXPECT_EQ(millerRabinRandom(1, 20, rng), Verdict::Neither);
    EXPECT_EQ(millerRabinRandom(4, 20, rng), Verdict::Composite);
    EXPECT_EQ(millerRabinRandom(5, 20, rng), Verdict::ProbablyPrime);  // smallest base range [2, 3]
    EXPECT_EQ(millerRabinRandom(97, 0, rng), Verdict::ProbablyPrime);  // rounds clamped to 1
    EXPECT_EQ(millerRabinRandom(97, -5, rng), Verdict::ProbablyPrime);
}

// One-sided error: a prime must never be reported composite, for any seed,
// even with a single round.
TEST(MillerRabinRandom, PrimesNeverCompositeAnySeed) {
    for (std::uint64_t seed = 0; seed < 1000; seed++) {
        std::mt19937_64 rng(seed);
        for (auto p : kPrimes)
            ASSERT_NE(millerRabinRandom(p, 1, rng), Verdict::Composite) << p << " seed " << seed;
    }
}

// 703 = 19 * 37 has 160 strong liars among bases 2..701 (about 22.9%), close to
// the Monier-Rabin bound of 1/4. One round should be fooled sometimes, never more than 1/4.
TEST(MillerRabinRandom, LiarRateBelowQuarter) {
    std::mt19937_64 rng(42);
    const int trials = 20000;
    int fooled = 0;
    for (int i = 0; i < trials; i++)
        if (millerRabinRandom(703, 1, rng) != Verdict::Composite) fooled++;
    const double rate = static_cast<double>(fooled) / trials;
    EXPECT_GT(rate, 0.0);   // liars exist, so the bases really are random
    EXPECT_LT(rate, 0.25);  // Monier-Rabin bound
}

// With 20 rounds, the chance 703 slips through is about 0.229^20, roughly 1e-13.
// Fails at once if the rounds loop is broken and only one base is tried.
TEST(MillerRabinRandom, MoreRoundsCatchLiars) {
    std::mt19937_64 rng(7);
    for (int i = 0; i < 10000; i++)
        for (auto c : kComposites)
            ASSERT_EQ(millerRabinRandom(c, 20, rng), Verdict::Composite) << c;
}

TEST(MillerRabinRandom, SameSeedSameResults) {
    std::mt19937_64 a(999), b(999);
    for (int i = 0; i < 1000; i++)
        for (auto c : kComposites)
            ASSERT_EQ(millerRabinRandom(c, 1, a), millerRabinRandom(c, 1, b)) << c;
}

// The fixed version is exact for every 64-bit n, so use it as the expected
// answer on inputs too large for trial division.
TEST(MillerRabinRandom, AgreesWithFixedOnRandom64Bit) {
    std::mt19937_64 gen(2024), rng(5);
    for (int i = 0; i < 100000; i++) {
        std::uint64_t n = gen() | 1;  // odd
        ASSERT_EQ(Normalised(millerRabinRandom(n, 20, rng)), Normalised(millerRabinFixed(n))) << n;
    }
}

// AKS is slow, so a smaller range (the same 0..1000 AKS/aks.cpp self-tests).
TEST(Aks, AgreesWithTrialDivisionUpTo1000) {
    for (std::uint64_t n = 1; n <= 1000; n++)
        ASSERT_EQ(aks(n), trialDivision(n)) << "n = " << n;
}

// AKS gets the same hard cases as the other algorithms, limited to what it can
// finish quickly: composites below 2^32 (decided at step 1, step 3, or the first
// step-5 congruence) and primes up to 127.
TEST(Aks, HardCases) {
    for (auto c : kComposites)
        if (c < (1ULL << 32)) EXPECT_EQ(aks(c), Verdict::Composite) << c;
    for (auto p : kPrimes)
        if (p <= 127) EXPECT_EQ(aks(p), Verdict::Prime) << p;
}

// The rest of the primes AKS can finish: they reach step 6 and take seconds to
// minutes each. Run with: --gtest_also_run_disabled_tests
TEST(Aks, DISABLED_SlowPrimes) {
    for (auto p : kPrimes)
        if (p > 127 && p < (1ULL << 20)) EXPECT_EQ(aks(p), Verdict::Prime) << p;
}