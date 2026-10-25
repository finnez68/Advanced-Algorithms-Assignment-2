// miller_rabin_random.cpp -- Miller-Rabin with random bases
// Primary author: Dino

#include "primality.h"

namespace {

using u64 = std::uint64_t;

inline u64 addmod(u64 a, u64 b, u64 m) {
    return a >= m - b ? a - (m - b) : a + b;
}

inline u64 mulmod(u64 a, u64 b, u64 m) {
    u64 result = 0;
    a %= m;
    while (b) {
        if (b & 1) result = addmod(result, a, m);
        b >>= 1;
        if (b) a = addmod(a, a, m);
    }
    return result;
}

u64 powmod(u64 a, u64 e, u64 m) {
    u64 result = 1 % m;
    a %= m;
    while (e) {
        if (e & 1) result = mulmod(result, a, m);
        e >>= 1;
        if (e) a = mulmod(a, a, m);
    }
    return result;
}

// n odd, n > 3, n - 1 = 2^s * d with d odd.
// Returns true if base a PROVES n composite.
bool isWitness(u64 a, u64 n, u64 d, int s) {
    u64 x = powmod(a, d, n);
    if (x == 1 || x == n - 1) return false;
    for (int i = 1; i < s; i++) {
        x = mulmod(x, x, n);
        if (x == n - 1) return false;
    }
    return true;
}

} // namespace

Verdict millerRabinRandom(std::uint64_t n, int rounds, std::mt19937_64& rng) {
    if (n < 2) return Verdict::Composite;
    if (n < 4) return Verdict::Prime;              // 2 and 3 are definite
    if (n % 2 == 0) return Verdict::Composite;

    // n - 1 = 2^s * d, d odd
    u64 d = n - 1;
    int s = 0;
    while ((d & 1) == 0) { d >>= 1; s++; }

    std::uniform_int_distribution<u64> pick(2, n - 2);   // uniform in [2, n-2]
    if (rounds < 1) rounds = 1;

    for (int i = 0; i < rounds; i++) {
        if (isWitness(pick(rng), n, d, s))
            return Verdict::Composite;                    // certain
    }
    return Verdict::ProbablyPrime;                        // error <= 4^-rounds
}

bool millerRabinRandomIsPlaceholder() { return false; }