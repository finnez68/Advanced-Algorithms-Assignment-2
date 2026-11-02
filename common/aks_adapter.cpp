// aks_adapter.cpp -- connects Finley's AKS (AKS/aks.cpp) to the shared
// interface in primality.h, without changing his file.
//
// AKS/aks.cpp is included directly so its static functions and statistics
// (g_stats, the g_aks_cancel hook) are reachable from here.


#define AKS_NO_MAIN
#include "../AKS/aks.cpp"

#include "primality.h"

Verdict aks(std::uint64_t n) {
    // aks_is_prime() returns false for n < 2; the shared interface reports
    // that case as "neither" rather than "composite".
    if (n < 2) return Verdict::Neither;
    return aks_is_prime(n) ? Verdict::Prime : Verdict::Composite;
}

namespace {

// Runs `body` with aks.cpp's cancel hook pointed at `cancel`, translating
// AKS's own AksCancelled into the shared AlgorithmCancelled.
template <typename F>
Verdict WithCancel(const std::atomic<bool>* cancel, F body) {
    g_aks_cancel = cancel;
    try {
        const Verdict v = body();
        g_aks_cancel = nullptr;
        return v;
    } catch (const AksCancelled&) {
        g_aks_cancel = nullptr;
        throw AlgorithmCancelled{};
    } catch (...) {
        g_aks_cancel = nullptr;
        throw;
    }
}

} // namespace

Verdict aksCancellable(std::uint64_t n, const std::atomic<bool>& cancel) {
    return WithCancel(&cancel, [n] { return aks(n); });
}

Verdict aksWithInfo(std::uint64_t n, AksInfo& info, const std::atomic<bool>* cancel) {
    const Verdict v = WithCancel(cancel, [n] { return aks(n); });
    info = {g_stats.r, g_stats.bound, g_stats.checked, g_stats.reason};
    return v;
}

bool aksIsPlaceholder() { return false; }
