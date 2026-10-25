// miller_rabin_fixed.cpp -- Miller-Rabin with fixed witness sets
// Primary author: Atnm
//
// PLACEHOLDER: the body below just calls trialDivision() so the GUI and
// tests have something to run. Replace it with the real implementation and
// make millerRabinFixedIsPlaceholder() return false.

#include "primality.h"
#include "modular_arithmetic.h"

Verdict millerRabinFixed(std::uint64_t n) {
    return trialDivision(n); // PLACEHOLDER
}

bool millerRabinFixedIsPlaceholder() { return true; }
