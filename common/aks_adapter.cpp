// aks_adapter.cpp -- connects Finley's AKS (AKS/aks.cpp) to the shared
// interface in primality.h, without changing his file.
//
// AKS/aks.cpp is a standalone program with its own main(). Defining
// AKS_NO_MAIN before including it drops that main -- the same trick
// AKS/aks_testing.cpp uses -- leaving aks_is_prime() to call.

#define AKS_NO_MAIN
#include "../AKS/aks.cpp"

#include "primality.h"

Verdict aks(std::uint64_t n) {
    // aks_is_prime() returns false for n < 2; the shared interface reports
    // that case as "neither" rather than "composite".
    if (n < 2) return Verdict::Neither;
    return aks_is_prime(n) ? Verdict::Prime : Verdict::Composite;
}

bool aksIsPlaceholder() { return false; }
