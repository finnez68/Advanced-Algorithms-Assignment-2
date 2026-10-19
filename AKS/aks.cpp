// AKS primality test (Agrawal-Kayal-Saxena, 2002) for 64-bit integers.
//
// Build:  g++ -O2 -std=c++17 -o aks aks.cpp
// Usage:  ./aks            -> run self-test (vs. trial division) and benchmarks
//         ./aks 1000000007 -> test the given numbers
//
// Algorithm (n > 1):
//   1. If n = a^b for some a > 1, b > 1            -> composite
//   2. Find the smallest r with ord_r(n) > log2(n)^2
//   3. If 1 < gcd(a, n) < n for some a <= r         -> composite
//   4. If n <= r                                    -> prime
//   5. For a = 1 .. floor(sqrt(phi(r)) * log2(n)):
//          if (X + a)^n != X^n + a  (mod X^r - 1, n) -> composite
//   6. Otherwise                                    -> prime
//
// Polynomials mod (X^r - 1) are stored as vectors of r coefficients in
// Z_n, multiplied by cyclic schoolbook convolution (O(r^2) mulmods).
// This is deliberately the simple version: it keeps the code short and the
// cost structure easy to analyse. Using FFT/Kronecker substitution would
// bring the theoretical bound down to roughly O~(log^6 n).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using u64 = uint64_t;
using u128 = unsigned __int128;

// ---------------------------------------------------------------- helpers

static u64 gcd64(u64 a, u64 b) {
    while (b) {
        u64 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static inline u64 mulmod(u64 a, u64 b, u64 m) {
    return (u64)((u128)a * b % m);
}

// min(a^b, cap + 1), computed without overflow.
static u64 pow_saturating(u64 a, int b, u64 cap) {
    u128 r = 1;
    for (int i = 0; i < b; i++) {
        r *= a;  // r <= cap < 2^64 and a <= 2^32 here, so this fits in u128
        if (r > cap) return cap + 1;
    }
    return (u64)r;
}

// Step 1: is n = a^b for integers a >= 2, b >= 2 ?
static bool is_perfect_power(u64 n) {
    for (int b = 2; b < 64; b++) {
        if (pow_saturating(2, b, n) > n) break;  // 2^b > n: no larger b can work
        u64 lo = 2, hi = 1ULL << 32;
        while (lo <= hi) {
            u64 mid = lo + (hi - lo) / 2;
            u64 p = pow_saturating(mid, b, n);
            if (p == n) return true;
            if (p < n) lo = mid + 1;
            else hi = mid - 1;
        }
    }
    return false;
}

// Euler's totient by trial division (r is small, polylog in n).
static u64 euler_phi(u64 r) {
    u64 result = r, m = r;
    for (u64 p = 2; p * p <= m; p++) {
        if (m % p == 0) {
            while (m % p == 0) m /= p;
            result -= result / p;
        }
    }
    if (m > 1) result -= result / m;
    return result;
}

// ------------------------------------------------- polynomials mod X^r - 1

using Poly = std::vector<u64>;

// C = A * B  mod (X^r - 1, n)
static Poly poly_mul(const Poly& A, const Poly& B, u64 n, size_t r) {
    Poly C(r, 0);
    if (n < (1ULL << 32)) {
        // Fast path: every coefficient is < 2^32, so each product is < 2^64 and
        // a sum of up to r products fits in a u128. Reduce once per output
        // coefficient instead of once per multiply-add.
        std::vector<u128> acc(r, 0);
        for (size_t i = 0; i < r; i++) {
            if (!A[i]) continue;
            const u64 ai = A[i];
            for (size_t j = 0; j < r; j++) {
                size_t k = i + j;
                if (k >= r) k -= r;
                acc[k] += (u128)(ai * B[j]);  // ai * B[j] < 2^64: no overflow
            }
        }
        for (size_t k = 0; k < r; k++) C[k] = (u64)(acc[k] % n);
        return C;
    }
    for (size_t i = 0; i < r; i++) {
        if (!A[i]) continue;
        for (size_t j = 0; j < r; j++) {
            if (!B[j]) continue;
            size_t k = i + j;
            if (k >= r) k -= r;
            C[k] = (u64)(((u128)C[k] + mulmod(A[i], B[j], n)) % n);
        }
    }
    return C;
}

// (X + a)^e  mod (X^r - 1, n), by square-and-multiply.
static Poly poly_pow_x_plus_a(u64 a, u64 e, u64 n, size_t r) {
    Poly base(r, 0), result(r, 0);
    base[0] = a % n;
    base[1 % r] = (base[1 % r] + 1) % n;  // r >= 2 always, so this is X
    result[0] = 1;
    while (e) {
        if (e & 1) result = poly_mul(result, base, n, r);
        e >>= 1;
        if (e) base = poly_mul(base, base, n, r);
    }
    return result;
}

// ------------------------------------------------------------------- AKS

struct AksStats {
    u64 r = 0;          // the modulus exponent found in step 2
    u64 bound = 0;      // number of congruences that must be checked
    u64 checked = 0;    // number of congruences actually evaluated
    const char* reason = "";  // which step decided the answer
};

static AksStats g_stats;

static bool aks_is_prime(u64 n) {
    g_stats = AksStats{};
    if (n < 2) { g_stats.reason = "n < 2"; return false; }

    // Step 1
    if (is_perfect_power(n)) { g_stats.reason = "step 1: perfect power"; return false; }

    // Step 2: smallest r with ord_r(n) > floor(log2(n)^2)
    const long double L = log2l((long double)n);
    const u64 maxk = (u64)floorl(L * L);
    u64 r = 2;
    for (;; r++) {
        if (gcd64(n, r) != 1) continue;  // step 3 will detect it if r < n
        u64 nr = n % r, x = 1;
        bool order_small = false;
        for (u64 k = 1; k <= maxk; k++) {
            x = x * nr % r;
            if (x == 1) { order_small = true; break; }
        }
        if (!order_small) break;
    }
    g_stats.r = r;

    // Step 3
    for (u64 a = 2; a <= std::min<u64>(r, n - 1); a++) {
        u64 g = gcd64(a, n);
        if (g > 1 && g < n) { g_stats.reason = "step 3: shared factor"; return false; }
    }

    // Step 4
    if (n <= r) { g_stats.reason = "step 4: n <= r"; return true; }

    // Step 5
    const u64 phi = euler_phi(r);
    const u64 bound = (u64)ceill(sqrtl((long double)phi) * L);
    g_stats.bound = bound;
    const size_t rr = (size_t)r;
    for (u64 a = 1; a <= bound; a++) {
        Poly lhs = poly_pow_x_plus_a(a, n, n, rr);
        Poly rhs(rr, 0);
        rhs[n % r] = 1;
        rhs[0] = (u64)(((u128)rhs[0] + a) % n);
        g_stats.checked = a;
        if (lhs != rhs) { g_stats.reason = "step 5: congruence failed"; return false; }
    }

    // Step 6
    g_stats.reason = "step 6: all congruences hold";
    return true;
}

// ------------------------------------------------------------ test harness

static bool trial_division(u64 n) {
    if (n < 2) return false;
    for (u64 d = 2; d * d <= n; d++)
        if (n % d == 0) return false;
    return true;
}

static double seconds_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

static void report(u64 n) {
    auto t0 = std::chrono::steady_clock::now();
    bool p = aks_is_prime(n);
    double s = seconds_since(t0);
    std::printf("%20llu  %-9s r=%-5llu checked=%llu/%llu  %.4fs  (%s)\n",
                (unsigned long long)n, p ? "PRIME" : "composite",
                (unsigned long long)g_stats.r, (unsigned long long)g_stats.checked,
                (unsigned long long)g_stats.bound, s, g_stats.reason);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) report(std::strtoull(argv[i], nullptr, 10));
        return 0;
    }

    // 1. Exhaustive agreement with trial division on small n.
    const u64 LIMIT = 1000;
    int mismatches = 0;
    for (u64 n = 0; n <= LIMIT; n++) {
        if (aks_is_prime(n) != trial_division(n)) {
            std::printf("MISMATCH at n = %llu\n", (unsigned long long)n);
            mismatches++;
        }
    }
    std::printf("Checked 0..%llu against trial division: %d mismatches\n\n",
                (unsigned long long)LIMIT, mismatches);

    // 2. Hard cases for probabilistic tests, plus some larger primes.
    std::printf("Selected cases:\n");
    const u64 cases[] = {
        561,         // smallest Carmichael number
        1105, 1729, 2465, 6601, 8911,
        10007,
        100003,
        // Inputs near 2^31 take minutes with schoolbook multiplication (r is
        // about 1000 and ~1000 congruences must be checked). Try them from the
        // command line, e.g.:  ./aks 1000003 3215031751 2147483647
    };
    for (u64 n : cases) report(n);

    return mismatches ? 1 : 0;
}