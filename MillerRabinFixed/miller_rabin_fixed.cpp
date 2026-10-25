// miller_rabin_fixed.cpp -- Miller-Rabin with fixed witness sets
// Primary author: Atnm

#include "primality.h"
#include "modular_arithmetic.h"

using u64 = uint64_t;

bool doesBasePass(u64 n, u64 a_mod_n, u64 nMinusOneFactor, int nMinusOneHalvedByThisCount) {
    u64 x = mod_arith::powmod(a_mod_n, nMinusOneFactor, n);
    if (x == 1 || x == n-1) {
        return true;
    }
    for (int i = 1; i <= nMinusOneHalvedByThisCount-1; i++) {
        x = mod_arith::mulmod(x, x, n);
        if (x == n-1) {
            return true;
        }
    }
    return false;
}

Verdict millerRabinFixed(std::uint64_t n) {
    if (n < 2) return Verdict::Neither;
    if (n == 2 || n == 3) return Verdict::Prime;
    if (n % 2 == 0) return Verdict::Composite;

    u64 nMinusOne = n - 1; //must be even
    u64 nMinusOneFactor = nMinusOne;
    int nMinusOneHalvedByThisCount = 0;
    while (nMinusOneFactor % 2 == 0) {
        nMinusOneHalvedByThisCount++;
        nMinusOneFactor /= 2;
    }
    constexpr u64 fixedWitnessSet[] = {2,3,5,7,11,13,17,19,23,29,31,37};
    for (u64 a : fixedWitnessSet) {
        u64 a_mod_n = a % n;
        if (a_mod_n == 0) continue;
        if (!doesBasePass(n, a_mod_n, nMinusOneFactor, nMinusOneHalvedByThisCount)) return Verdict::Composite;
    }
    return Verdict::Prime;
}

bool millerRabinFixedIsPlaceholder() { return false; }
