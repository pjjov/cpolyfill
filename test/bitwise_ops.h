/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Every function of <cpolyfill/bitwise.h> behind one dispatcher, so the
   same calls can be compiled with and without PF_BITWISE_PORTABLE and
   compared. Define BITWISE_OPS_NAME before including. */

#ifndef BITWISE_OPS_NAME
    #error "define BITWISE_OPS_NAME"
#endif

#include <cpolyfill/bitwise.h>

#include <stdint.h>

#ifndef BITWISE_OPS_ENUM
#define BITWISE_OPS_ENUM

enum bit_op {
    OP_CLZ,
    OP_CTZ,
    OP_CLO,
    OP_CTO,
    OP_FLZ,
    OP_FLO,
    OP_FTZ,
    OP_FTO,
    OP_POPCOUNT,
    OP_ZEROCOUNT,
    OP_PARITY,
    OP_ISPOW2,
    OP_POW2CEIL,
    OP_POW2FLOOR,
    OP_BITWIDTH,
    OP_BITREVERSE,
    OP_ROTL,
    OP_ROTR,
    OP_COUNT
};

/* The argument types, in the order of `bit_types`. */
enum bit_type {
    T_UINT,
    T_ULONG,
    T_ULLONG,
    T_8,
    T_16,
    T_32,
    T_64,
    T_SIZE,
    T_PTR,
    T_MAX,
    T_COUNT
};

#endif

#define BITWISE_OPS_FN(name, sfx, type)                          \
    static uint64_t name(int op, uint64_t v, unsigned n) {       \
        type x = (type)v;                                        \
        switch (op) {                                            \
        case OP_CLZ: return (uint64_t)pf_clz##sfx(x);            \
        case OP_CTZ: return (uint64_t)pf_ctz##sfx(x);            \
        case OP_CLO: return (uint64_t)pf_clo##sfx(x);            \
        case OP_CTO: return (uint64_t)pf_cto##sfx(x);            \
        case OP_FLZ: return (uint64_t)pf_flz##sfx(x);            \
        case OP_FLO: return (uint64_t)pf_flo##sfx(x);            \
        case OP_FTZ: return (uint64_t)pf_ftz##sfx(x);            \
        case OP_FTO: return (uint64_t)pf_fto##sfx(x);            \
        case OP_POPCOUNT: return (uint64_t)pf_popcount##sfx(x);  \
        case OP_ZEROCOUNT: return (uint64_t)pf_zerocount##sfx(x); \
        case OP_PARITY: return (uint64_t)pf_parity##sfx(x);      \
        case OP_ISPOW2: return (uint64_t)pf_ispow2##sfx(x);      \
        case OP_POW2CEIL: return (uint64_t)pf_pow2ceil##sfx(x);  \
        case OP_POW2FLOOR: return (uint64_t)pf_pow2floor##sfx(x); \
        case OP_BITWIDTH: return (uint64_t)pf_bitwidth##sfx(x);  \
        case OP_BITREVERSE: return (uint64_t)pf_bitreverse##sfx(x); \
        case OP_ROTL: return (uint64_t)pf_rotl##sfx(x, n);       \
        case OP_ROTR: return (uint64_t)pf_rotr##sfx(x, n);       \
        }                                                        \
        return 0;                                                \
    }

BITWISE_OPS_FN(bit_uint, , unsigned int)
BITWISE_OPS_FN(bit_ulong, l, unsigned long)
BITWISE_OPS_FN(bit_ullong, ll, unsigned long long)
BITWISE_OPS_FN(bit_8, 8, uint8_t)
BITWISE_OPS_FN(bit_16, 16, uint16_t)
BITWISE_OPS_FN(bit_32, 32, uint32_t)
BITWISE_OPS_FN(bit_64, 64, uint64_t)
BITWISE_OPS_FN(bit_size, size, size_t)
BITWISE_OPS_FN(bit_ptr, ptr, uintptr_t)
BITWISE_OPS_FN(bit_max, max, uintmax_t)

/* The result of `op` on the low bits of `v` as the type `type`. */
uint64_t BITWISE_OPS_NAME(int type, int op, uint64_t v, unsigned n);
uint64_t BITWISE_OPS_NAME(int type, int op, uint64_t v, unsigned n) {
    switch (type) {
    case T_UINT: return bit_uint(op, v, n);
    case T_ULONG: return bit_ulong(op, v, n);
    case T_ULLONG: return bit_ullong(op, v, n);
    case T_8: return bit_8(op, v, n);
    case T_16: return bit_16(op, v, n);
    case T_32: return bit_32(op, v, n);
    case T_64: return bit_64(op, v, n);
    case T_SIZE: return bit_size(op, v, n);
    case T_PTR: return bit_ptr(op, v, n);
    case T_MAX: return bit_max(op, v, n);
    }
    return 0;
}
