//
// Created by iadna on 25/10/2026.
//

#include "modular_arithmetic.h"

u64 modular_arithmetic::mulmod(u64 a, u64 b, u64 m) {
#ifdef __SIZEOF_INT128__
    return static_cast<unsigned __int128>(a) * b % m;
#else
    u64 result = 0;
    a %= m;
    while (b) {
        if (b & 1) result = addmod(result, a, m);
        b >>= 1;
        if (b) a = addmod(a, a, m);
    }
    return result;
#endif
}

u64 modular_arithmetic::addmod(u64 a, u64 b, u64 m) {

}

u64 modular_arithmetic::powmod(u64 a, u64 b, u64 m) {

}
