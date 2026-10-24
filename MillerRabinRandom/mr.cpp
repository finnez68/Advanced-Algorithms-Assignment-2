// Miller-Rabin primality test (random bases) for 64-bit integers.
//
// Build:  g++ -O2 -std=c++17 -o mr mr.cpp
// Usage:  ./mr            -> self-test against trial division
//         ./mr 561 1009   -> test the given numbers (k = 20 rounds)

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

using u64 = uint64_t;

// ---------------------------------------------------------------- helpers
// Same mulmod/addmod as aks.cpp, so timing differences come from the
// algorithms and not the arithmetic.

static inline u64 addmod(u64 a, u64 b, u64 m) {
    return a >= m - b ? a - (m - b) : a + b;
}

static inline u64 mulmod(u64 a, u64 b, u64 m) {
    u64 result = 0;
    a %= m;
    while (b) {
        if (b & 1) result = addmod(result, a, m);
        b >>= 1;
        if (b) a = addmod(a, a, m);
    }
    return result;
}

// a^e mod m, square-and-multiply, reducing at every step.
static u64 powmod(u64 a, u64 e, u64 m) {
    u64 result = 1 % m;
    a %= m;
    while (e) {
        if (e & 1) result = mulmod(result, a, m);
        e >>= 1;
        if (e) a = mulmod(a, a, m);
    }
    return result;
}

// ------------------------------------------------------------ witness test

// n odd, n > 3, n - 1 = 2^s * d with d odd.
// Returns true if base a PROVES n composite (a is a witness).
static bool is_witness(u64 a, u64 n, u64 d, int s) {
    u64 x = powmod(a, d, n);
    if (x == 1 || x == n - 1) return false;      // chain starts at 1, or -1 at i = 0
    for (int i = 1; i < s; i++) {
        x = mulmod(x, x, n);
        if (x == n - 1) return false;            // -1 appeared in the chain
    }
    return true;                                 // chain broke the rules: composite
}

// ------------------------------------------------------------------ stats

struct MrStats {
    u64 rounds = 0;            // bases actually tried
    u64 witness = 0;           // the base that exposed n (0 if none)
    const char* reason = "";   // how the answer was decided
};
static MrStats g_stats;

static std::mt19937_64 g_rng(12345);             // fixed seed = reproducible runs

// ------------------------------------------------------- Miller-Rabin (random)

static bool mr_random(u64 n, int k) {
    g_stats = MrStats{};

    if (n < 2)      { g_stats.reason = "n < 2";       return false; }
    if (n < 4)      { g_stats.reason = "n is 2 or 3"; return true;  }
    if (n % 2 == 0) { g_stats.reason = "even";        return false; }

    // n - 1 = 2^s * d, d odd
    u64 d = n - 1;
    int s = 0;
    while (d % 2 == 0) { d /= 2; s++; }

    for (int i = 0; i < k; i++) {
        u64 a = 2 + g_rng() % (n - 3);           // uniform in [2, n-2]
        g_stats.rounds++;
        if (is_witness(a, n, d, s)) {
            g_stats.witness = a;
            g_stats.reason = "witness found";
            return false;                         // definitely composite
        }
    }
    g_stats.reason = "all rounds passed";
    return true;                                  // probably prime
}

// ------------------------------------------------------------ test harness

static bool trial_division(u64 n) {
    if (n < 2) return false;
    for (u64 d = 2; d * d <= n; d++)
        if (n % d == 0) return false;
    return true;
}

#ifndef MR_NO_MAIN
int main(int argc, char** argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            u64 n = std::strtoull(argv[i], nullptr, 10);
            bool r = mr_random(n, 20);
            std::printf("%llu  %-9s rounds=%llu  witness=%llu  (%s)\n",
                        (unsigned long long)n, r ? "PRIME" : "composite",
                        (unsigned long long)g_stats.rounds,
                        (unsigned long long)g_stats.witness, g_stats.reason);
        }
        return 0;
    }

    // Agreement with trial division on small n.
    const u64 LIMIT = 100000;
    int mismatches = 0;
    for (u64 n = 0; n <= LIMIT; n++) {
        if (mr_random(n, 20) != trial_division(n)) {
            std::printf("MISMATCH at n = %llu\n", (unsigned long long)n);
            mismatches++;
        }
    }
    std::printf("0..%llu vs trial division: %d mismatches (k = 20)\n",
                (unsigned long long)LIMIT, mismatches);
    return mismatches ? 1 : 0;
}
#endif