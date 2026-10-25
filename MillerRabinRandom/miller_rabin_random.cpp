// miller_rabin_random.cpp -- Miller-Rabin with random bases
// Primary author: Dino
//
// PLACEHOLDER: the body below just calls trialDivision() so the GUI and
// tests have something to run. Replace it with the real implementation and
// make millerRabinRandomIsPlaceholder() return false.

#include "primality.h"

Verdict millerRabinRandom(std::uint64_t n, int rounds, std::mt19937_64& rng) {
    (void)rounds;
    (void)rng;
    Verdict v = trialDivision(n); // PLACEHOLDER
    // The real algorithm can only ever say "probably" prime; mirror that.
    return v == Verdict::Prime ? Verdict::ProbablyPrime : v;
}

bool millerRabinRandomIsPlaceholder() { return true; }
