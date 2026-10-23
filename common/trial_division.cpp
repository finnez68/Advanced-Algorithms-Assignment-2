// trial_division.cpp -- reference oracle (not one of the three compared algorithms)
//
// Checks divisors 2, 3, then 6k +/- 1 up to sqrt(n). O(sqrt n): instant for
// small n, but several seconds for a prime near 2^64 (~1.4 billion
// candidate divisors). Correct by construction, which is the point.

#include "primality.h"

const char* ToString(Verdict v) {
    switch (v) {
        case Verdict::Neither:       return "Neither";
        case Verdict::Prime:         return "Prime";
        case Verdict::Composite:     return "Composite";
        case Verdict::ProbablyPrime: return "Probably prime";
    }
    return "?";
}

Verdict trialDivision(std::uint64_t n) {
    if (n < 2) return Verdict::Neither;
    if (n < 4) return Verdict::Prime;  // 2, 3
    if (n % 2 == 0 || n % 3 == 0) return Verdict::Composite;
    // i <= n / i instead of i * i <= n: i * i overflows for n near 2^64.
    for (std::uint64_t i = 5; i <= n / i; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return Verdict::Composite;
    }
    return Verdict::Prime;
}
