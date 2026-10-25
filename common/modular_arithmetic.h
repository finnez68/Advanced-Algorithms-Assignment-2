//
// Created by iadna on 25/10/2026.
//

#ifndef PRIMALITYTESTING_MODULAR_ARITHMETIC_H
#define PRIMALITYTESTING_MODULAR_ARITHMETIC_H
#include <cstdint>

using u64 = uint64_t;

class modular_arithmetic {
    static u64 mulmod(u64 a, u64 b, u64 m);
    static u64 addmod(u64 a, u64 b, u64 m);
    static u64 powmod(u64 a, u64 b, u64 m);
};


#endif //PRIMALITYTESTING_MODULAR_ARITHMETIC_H