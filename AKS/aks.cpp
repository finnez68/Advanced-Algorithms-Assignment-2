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
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../common/modular_arithmetic.h"

using u64 = uint64_t;

// ---------------------------------------------------------------- helpers

static u64 gcd64(u64 a, u64 b) {
    while (b) {
        u64 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// min(a^b, cap + 1), computed without overflow.
static u64 pow_saturating(u64 a, int b, u64 cap) {
    u64 r = 1;
    for (int i = 0; i < b; i++) {
        if (a && r > cap / a) return cap + 1;
        r *= a;
    }
    return r;
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
    for (size_t i = 0; i < r; i++) {
        if (!A[i]) continue;
        for (size_t j = 0; j < r; j++) {
            if (!B[j]) continue;
            size_t k = i + j;
            if (k >= r) k -= r;
            C[k] = mod_arith::addmod(C[k], mod_arith::mulmod(A[i], B[j], n), n);
        }
    }
    return C;
}

// Optional cooperative cancellation. A caller that wants to be able to stop
// a long run points g_aks_cancel at a flag; aks_is_prime() then throws
// AksCancelled shortly after the flag becomes true. A running thread can't
// be stopped safely from outside, so AKS checks the flag itself, once per
// polynomial squaring (at most ~0.1 s apart even for 64-bit n). Left null,
// it costs one pointer test per squaring and changes nothing.
struct AksCancelled {};
static thread_local const std::atomic<bool>* g_aks_cancel = nullptr;

static inline void aks_check_cancel() {
    if (g_aks_cancel && g_aks_cancel->load(std::memory_order_relaxed)) throw AksCancelled{};
}

// (X + a)^e  mod (X^r - 1, n), by square-and-multiply.
static Poly poly_pow_x_plus_a(u64 a, u64 e, u64 n, size_t r) {
    Poly base(r, 0), result(r, 0);
    base[0] = a % n;
    base[1 % r] = (base[1 % r] + 1) % n;  // r >= 2 always, so this is X
    result[0] = 1;
    while (e) {
        aks_check_cancel();
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

static bool aks_is_prime(u64 n, AksStats* stats = &g_stats) {
    AksStats& st = *stats;
    st = AksStats{};
    if (n < 2) { st.reason = "n < 2"; return false; }

    // Step 1
    if (is_perfect_power(n)) { st.reason = "step 1: perfect power"; return false; }

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
    st.r = r;

    // Step 3
    for (u64 a = 2; a <= std::min<u64>(r, n - 1); a++) {
        u64 g = gcd64(a, n);
        if (g > 1 && g < n) { st.reason = "step 3: shared factor"; return false; }
    }

    // Step 4
    if (n <= r) { st.reason = "step 4: n <= r"; return true; }

    // Step 5
    const u64 phi = euler_phi(r);
    const u64 bound = (u64)ceill(sqrtl((long double)phi) * L);
    st.bound = bound;
    const size_t rr = (size_t)r;
    for (u64 a = 1; a <= bound; a++) {
        Poly lhs = poly_pow_x_plus_a(a, n, n, rr);
        Poly rhs(rr, 0);
        rhs[n % r] = 1;
        rhs[0] = mod_arith::addmod(rhs[0], a % n, n);
        st.checked = a;
        if (lhs != rhs) { st.reason = "step 5: congruence failed"; return false; }
    }

    // Step 6
    st.reason = "step 6: all congruences hold";
    return true;
}

