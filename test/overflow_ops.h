/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The functions of <cpolyfill/overflow.h> behind one dispatcher, so the
   same calls can be compiled with and without the builtins and compared.
   Define OVERFLOW_OPS_NAME before including. */

#ifndef OVERFLOW_OPS_NAME
    #error "define OVERFLOW_OPS_NAME"
#endif

#include <cpolyfill/overflow.h>

#include <stdint.h>

#ifndef OVERFLOW_OPS_ENUM
#define OVERFLOW_OPS_ENUM

enum ovf_op { OP_ADD, OP_SUB, OP_MUL, OP_COUNT };

/* Overflow (returns the flag) or saturated (returns 0). */
enum ovf_family { FAM_OVERFLOW, FAM_SATURATED };

/* Signed types first, in the order of `ovf_types`. */
enum ovf_type {
    T_INT,
    T_LONG,
    T_LLONG,
    T_I8,
    T_I16,
    T_I32,
    T_I64,
    T_IMAX,
    T_IPTR,
    T_DIFF,
    T_UINT,
    T_ULONG,
    T_ULLONG,
    T_U8,
    T_U16,
    T_U32,
    T_U64,
    T_UMAX,
    T_UPTR,
    T_SIZE,
    T_COUNT
};

#endif

/* Values travel as the bits of the type, sign-extended for signed
   types. */
#define OVERFLOW_OPS_FN(name, s, sfx, type, wide)                        \
    static int name(int fam, int op, uint64_t ua, uint64_t ub, uint64_t *o) { \
        type a = (type)(wide)ua, b = (type)(wide)ub, r = 0;              \
        int f = 0;                                                       \
        if (fam == FAM_OVERFLOW) {                                       \
            if (op == OP_ADD)                                            \
                f = pf_overflow_##s##add##sfx(a, b, &r);                 \
            else if (op == OP_SUB)                                       \
                f = pf_overflow_##s##sub##sfx(a, b, &r);                 \
            else                                                         \
                f = pf_overflow_##s##mul##sfx(a, b, &r);                 \
        } else {                                                         \
            if (op == OP_ADD)                                            \
                r = pf_saturated_##s##add##sfx(a, b);                    \
            else if (op == OP_SUB)                                       \
                r = pf_saturated_##s##sub##sfx(a, b);                    \
            else                                                         \
                r = pf_saturated_##s##mul##sfx(a, b);                    \
        }                                                                \
        *o = (uint64_t)(wide)r;                                          \
        return f;                                                        \
    }

OVERFLOW_OPS_FN(ovf_int, s, , int, int64_t)
OVERFLOW_OPS_FN(ovf_long, s, l, long, int64_t)
OVERFLOW_OPS_FN(ovf_llong, s, ll, long long, int64_t)
OVERFLOW_OPS_FN(ovf_i8, s, 8, int8_t, int64_t)
OVERFLOW_OPS_FN(ovf_i16, s, 16, int16_t, int64_t)
OVERFLOW_OPS_FN(ovf_i32, s, 32, int32_t, int64_t)
OVERFLOW_OPS_FN(ovf_i64, s, 64, int64_t, int64_t)
OVERFLOW_OPS_FN(ovf_imax, s, max, intmax_t, int64_t)
OVERFLOW_OPS_FN(ovf_iptr, s, ptr, intptr_t, int64_t)
OVERFLOW_OPS_FN(ovf_diff, s, diff, ptrdiff_t, int64_t)
OVERFLOW_OPS_FN(ovf_uint, u, , unsigned int, uint64_t)
OVERFLOW_OPS_FN(ovf_ulong, u, l, unsigned long, uint64_t)
OVERFLOW_OPS_FN(ovf_ullong, u, ll, unsigned long long, uint64_t)
OVERFLOW_OPS_FN(ovf_u8, u, 8, uint8_t, uint64_t)
OVERFLOW_OPS_FN(ovf_u16, u, 16, uint16_t, uint64_t)
OVERFLOW_OPS_FN(ovf_u32, u, 32, uint32_t, uint64_t)
OVERFLOW_OPS_FN(ovf_u64, u, 64, uint64_t, uint64_t)
OVERFLOW_OPS_FN(ovf_umax, u, max, uintmax_t, uint64_t)
OVERFLOW_OPS_FN(ovf_uptr, u, ptr, uintptr_t, uint64_t)
OVERFLOW_OPS_FN(ovf_size, u, size, size_t, uint64_t)

int OVERFLOW_OPS_NAME(
    int type, int fam, int op, uint64_t a, uint64_t b, uint64_t *out
);
int OVERFLOW_OPS_NAME(
    int type, int fam, int op, uint64_t a, uint64_t b, uint64_t *out
) {
    static int (*const fns[T_COUNT])(int, int, uint64_t, uint64_t, uint64_t *) = {
        ovf_int,  ovf_long,  ovf_llong,  ovf_i8,  ovf_i16, ovf_i32,  ovf_i64,
        ovf_imax, ovf_iptr,  ovf_diff,   ovf_uint, ovf_ulong, ovf_ullong,
        ovf_u8,   ovf_u16,   ovf_u32,    ovf_u64, ovf_umax, ovf_uptr, ovf_size,
    };
    return fns[type](fam, op, a, b, out);
}
