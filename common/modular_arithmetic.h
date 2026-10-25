//
// Created by iadna on 25/10/2026.
//

#ifndef PRIMALITYTESTING_MODULAR_ARITHMETIC_H
#define PRIMALITYTESTING_MODULAR_ARITHMETIC_H
#include <cstdint>

using u64 = uint64_t;

namespace mod_arith {
    inline u64 mulmod(u64 a, u64 b, u64 m){
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

    inline u64 addmod(u64 a, u64 b, u64 m){
        return a >= m - b ? a - (m - b) : a + b;
    }

    inline u64 powmod(u64 a, u64 e, u64 m){
        u64 result = 1 % m;
        a %= m;
        while (e) {
            if (e & 1) result = mulmod(result, a, m);
            e >>= 1;
            if (e) a = mulmod(a, a, m);
        }
        return result;
    }
};


#endif //PRIMALITYTESTING_MODULAR_ARITHMETIC_H