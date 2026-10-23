// test_algorithms.cpp -- checks every algorithm against trial division.
//
// These pass trivially while the placeholders are in (they ARE trial
// division); they start meaning something as each real implementation
// replaces its placeholder. The hard cases are the composites that fool
// weak primality tests, plus primes near 2^64 where 64-bit overflow in
// modular multiplication would show up.

#include "primality.h"
#include <gtest/gtest.h>
#include <vector>

namespace {

const std::vector<std::uint64_t> kPrimes = {
    2, 3, 5, 7, 13, 97, 7919, 1000000007ULL,
    4294967291ULL,                // largest 32-bit prime
    2305843009213693951ULL,       // 2^61 - 1
    18446744073709551557ULL,      // largest 64-bit prime
};

const std::vector<std::uint64_t> kComposites = {
    4, 9, 15, 561, 1105, 1729, 2465, 2821, 6601, 8911, // Carmichael numbers
    2047, 3277, 4033, 4681, 8321,                       // strong pseudoprimes to base 2
    1373653ULL,                                         // strong pseudoprime to bases 2, 3
    25326001ULL,                                        // strong pseudoprime to bases 2, 3, 5
    3215031751ULL,                                      // strong pseudoprime to bases 2, 3, 5, 7
    18446743979220271189ULL,                            // 4294967291 * 4294967279
    18446744073709551615ULL,                            // 2^64 - 1
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

TEST(MillerRabinRandom, NeverClaimsCertainty) {
    std::mt19937_64 rng(12345);
    for (auto p : kPrimes) EXPECT_EQ(millerRabinRandom(p, 20, rng), Verdict::ProbablyPrime) << p;
}

TEST(MillerRabinRandom, HardCases) {
    std::mt19937_64 rng(12345);
    for (auto c : kComposites) EXPECT_EQ(millerRabinRandom(c, 20, rng), Verdict::Composite) << c;
}

// AKS is slow, so a smaller range (the same 0..1000 AKS/aks.cpp self-tests).
TEST(Aks, AgreesWithTrialDivisionUpTo1000) {
    for (std::uint64_t n = 1; n <= 1000; n++)
        ASSERT_EQ(aks(n), trialDivision(n)) << "n = " << n;
}
