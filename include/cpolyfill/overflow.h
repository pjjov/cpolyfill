/** cpolyfill - Filling the gaps between C standards and compilers

    Integer arithmetic with overflow detection, checked and saturated
    arithmetic.

        bool pf_overflow_<op>(T a, T b, T *out)
                                Stores the result, wrapped around on
                                overflow, in `*out` and returns true if it
                                overflowed.
        T pf_checked_<op>(T a, T b)
                                The result; an overflow fails an assertion
                                (PF_ASSERT_ALWAYS, so it goes through the
                                handler of <cpolyfill/assert.h> and is not
                                compiled out by NDEBUG).
        T pf_saturated_<op>(T a, T b)
                                The result, clamped to the range of T.

    `<op>` is the operation, `s` (signed) or `u` (unsigned), then the type
    suffix, as in GCC's overflow builtins:

        | type               | add      | sub      | mul      |
        |--------------------|----------|----------|----------|
        | int                | sadd     | ssub     | smul     |
        | long               | saddl    | ssubl    | smull    |
        | long long          | saddll   | ssubll   | smulll   |
        | unsigned int       | uadd     | usub     | umul     |
        | unsigned long      | uaddl    | usubl    | umull    |
        | unsigned long long | uaddll   | usubll   | umulll   |
        | int8_t ... int64_t | sadd8 ... sadd64 (8, 16, 32, 64)  |
        | uint8_t...uint64_t | uadd8 ... uadd64                  |
        | intmax_t           | saddmax  | ssubmax  | smulmax  |
        | uintmax_t          | uaddmax  | usubmax  | umulmax  |
        | intptr_t           | saddptr  | ssubptr  | smulptr  |
        | uintptr_t          | uaddptr  | usubptr  | umulptr  |
        | size_t             | uaddsize | usubsize | umulsize |
        | ptrdiff_t          | sadddiff | ssubdiff | smuldiff |

    Type-generic checked arithmetic, as C23 <stdckdint.h>:

        bool PF_CKD_ADD(R *r, a, b)
        bool PF_CKD_SUB(R *r, a, b)
        bool PF_CKD_MUL(R *r, a, b)
                                Computes `a op b` exactly, stores it in
                                `*r` wrapped to R, and returns true if it
                                did not fit. `a`, `b` and `*r` may have
                                different integer types.

    These exist when PF_HAS_CKD is 1: with C23 <stdckdint.h> or with GCC
    and Clang. There is no fallback, because one cannot get mixed types
    right.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_OVERFLOW (C only):
        When <stdckdint.h> exists it is included. Otherwise `ckd_add`,
        `ckd_sub` and `ckd_mul` are defined where PF_HAS_CKD is 1.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_OVERFLOW_H
#define CPOLYFILL_OVERFLOW_H

#include <cpolyfill/assert.h>
#include <cpolyfill/platform.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Implementation --------------------------------------------------- */

#if (PF_HAS_BUILTIN(__builtin_add_overflow) || PF_GCC_PREREQ(5, 0)) \
    && !defined(PF__OVF_PORTABLE)
    #define PF__OVF_BUILTIN 1
#else
    #define PF__OVF_BUILTIN 0
#endif

#if !PF__OVF_BUILTIN && PF_STD_C23 && defined(__has_include) \
    && !defined(PF__OVF_PORTABLE)
    #if __has_include(<stdckdint.h>)
        #include <stdckdint.h>
        #define PF__OVF_STDCKDINT 1
    #endif
#endif
#ifndef PF__OVF_STDCKDINT
    #define PF__OVF_STDCKDINT 0
#endif

#if PF__OVF_BUILTIN
    #define PF__OVF_ADD(type, utype, min, max) \
        return __builtin_add_overflow(a, b, out);
    #define PF__OVF_SUB(type, utype, min, max) \
        return __builtin_sub_overflow(a, b, out);
    #define PF__OVF_MUL(type, utype, min, max) \
        return __builtin_mul_overflow(a, b, out);
    #define PF__OVF_UADD PF__OVF_ADD
    #define PF__OVF_USUB PF__OVF_SUB
    #define PF__OVF_UMUL PF__OVF_MUL
#elif PF__OVF_STDCKDINT
    #define PF__OVF_ADD(type, utype, min, max) return ckd_add(out, a, b);
    #define PF__OVF_SUB(type, utype, min, max) return ckd_sub(out, a, b);
    #define PF__OVF_MUL(type, utype, min, max) return ckd_mul(out, a, b);
    #define PF__OVF_UADD PF__OVF_ADD
    #define PF__OVF_USUB PF__OVF_SUB
    #define PF__OVF_UMUL PF__OVF_MUL
#else
    /* The wrapped result is computed in unsigned arithmetic, where it is
       defined. `0u +` keeps types narrower than int from being promoted to
       (signed) int, where a multiplication could overflow. */
    #define PF__OVF_WRAP(type, utype, op) \
        (*out = (type)(utype)((0u + (utype)a) op (utype)b))

    #define PF__OVF_UADD(type, utype, min, max) \
        PF__OVF_WRAP(type, utype, +);           \
        return *out < a;
    #define PF__OVF_USUB(type, utype, min, max) \
        PF__OVF_WRAP(type, utype, -);           \
        return a < b;
    #define PF__OVF_UMUL(type, utype, min, max) \
        PF__OVF_WRAP(type, utype, *);           \
        return a != 0 && *out / a != b;

    #define PF__OVF_ADD(type, utype, min, max)                     \
        PF__OVF_WRAP(type, utype, +);                              \
        return (b > 0 && a > (max) - b) || (b < 0 && a < (min) - b);
    #define PF__OVF_SUB(type, utype, min, max)                     \
        PF__OVF_WRAP(type, utype, -);                              \
        return (b < 0 && a > (max) + b) || (b > 0 && a < (min) + b);
    #define PF__OVF_MUL(type, utype, min, max)  \
        PF__OVF_WRAP(type, utype, *);           \
        if (a > 0)                              \
            return b > 0 ? a > (max) / b        \
                         : b < (min) / a;       \
        return b > 0 ? a < (min) / b            \
                     : a != 0 && b < (max) / a;
#endif

/* ---- Function generators ---------------------------------------------- */

#define PF__OVF_CHECKED(name, type)                                   \
    PF_INLINE type pf_checked_##name(type a, type b) {                \
        type r = 0;                                                   \
        bool overflow = pf_overflow_##name(a, b, &r);                 \
        PF_ASSERT_ALWAYS_MSG(!overflow, "pf_checked_" #name ": integer " \
                                        "overflow");                  \
        return r;                                                     \
    }

#define PF__OVF_SIGNED(suffix, type, utype, min, max)                       \
    PF_INLINE bool pf_overflow_sadd##suffix(type a, type b, type *out) {    \
        PF__OVF_ADD(type, utype, min, max)                                  \
    }                                                                       \
    PF_INLINE bool pf_overflow_ssub##suffix(type a, type b, type *out) {    \
        PF__OVF_SUB(type, utype, min, max)                                  \
    }                                                                       \
    PF_INLINE bool pf_overflow_smul##suffix(type a, type b, type *out) {    \
        PF__OVF_MUL(type, utype, min, max)                                  \
    }                                                                       \
    PF__OVF_CHECKED(sadd##suffix, type)                                     \
    PF__OVF_CHECKED(ssub##suffix, type)                                     \
    PF__OVF_CHECKED(smul##suffix, type)                                     \
    PF_INLINE type pf_saturated_sadd##suffix(type a, type b) {              \
        type r = 0;                                                         \
        if (pf_overflow_sadd##suffix(a, b, &r))                             \
            return b > 0 ? (max) : (min);                                   \
        return r;                                                           \
    }                                                                       \
    PF_INLINE type pf_saturated_ssub##suffix(type a, type b) {              \
        type r = 0;                                                         \
        if (pf_overflow_ssub##suffix(a, b, &r))                             \
            return b < 0 ? (max) : (min);                                   \
        return r;                                                           \
    }                                                                       \
    PF_INLINE type pf_saturated_smul##suffix(type a, type b) {              \
        type r = 0;                                                         \
        if (pf_overflow_smul##suffix(a, b, &r))                             \
            return (a < 0) == (b < 0) ? (max) : (min);                      \
        return r;                                                           \
    }

#define PF__OVF_UNSIGNED(suffix, type, max)                                 \
    PF_INLINE bool pf_overflow_uadd##suffix(type a, type b, type *out) {    \
        PF__OVF_UADD(type, type, 0, max)                                    \
    }                                                                       \
    PF_INLINE bool pf_overflow_usub##suffix(type a, type b, type *out) {    \
        PF__OVF_USUB(type, type, 0, max)                                    \
    }                                                                       \
    PF_INLINE bool pf_overflow_umul##suffix(type a, type b, type *out) {    \
        PF__OVF_UMUL(type, type, 0, max)                                    \
    }                                                                       \
    PF__OVF_CHECKED(uadd##suffix, type)                                     \
    PF__OVF_CHECKED(usub##suffix, type)                                     \
    PF__OVF_CHECKED(umul##suffix, type)                                     \
    PF_INLINE type pf_saturated_uadd##suffix(type a, type b) {              \
        type r = 0;                                                         \
        return pf_overflow_uadd##suffix(a, b, &r) ? (type)(max) : r;        \
    }                                                                       \
    PF_INLINE type pf_saturated_usub##suffix(type a, type b) {              \
        type r = 0;                                                         \
        return pf_overflow_usub##suffix(a, b, &r) ? (type)0 : r;            \
    }                                                                       \
    PF_INLINE type pf_saturated_umul##suffix(type a, type b) {              \
        type r = 0;                                                         \
        return pf_overflow_umul##suffix(a, b, &r) ? (type)(max) : r;        \
    }

PF_BEGIN_DECLS

/* clang-format off */
PF__OVF_SIGNED(,     int,       unsigned int,       INT_MIN,     INT_MAX)
PF__OVF_SIGNED(l,    long,      unsigned long,      LONG_MIN,    LONG_MAX)
PF__OVF_SIGNED(ll,   long long, unsigned long long, LLONG_MIN,   LLONG_MAX)
PF__OVF_SIGNED(8,    int8_t,    uint8_t,            INT8_MIN,    INT8_MAX)
PF__OVF_SIGNED(16,   int16_t,   uint16_t,           INT16_MIN,   INT16_MAX)
PF__OVF_SIGNED(32,   int32_t,   uint32_t,           INT32_MIN,   INT32_MAX)
PF__OVF_SIGNED(64,   int64_t,   uint64_t,           INT64_MIN,   INT64_MAX)
PF__OVF_SIGNED(max,  intmax_t,  uintmax_t,          INTMAX_MIN,  INTMAX_MAX)
PF__OVF_SIGNED(ptr,  intptr_t,  uintptr_t,          INTPTR_MIN,  INTPTR_MAX)
PF__OVF_SIGNED(diff, ptrdiff_t, size_t,             PTRDIFF_MIN, PTRDIFF_MAX)

PF__OVF_UNSIGNED(,     unsigned int,       UINT_MAX)
PF__OVF_UNSIGNED(l,    unsigned long,      ULONG_MAX)
PF__OVF_UNSIGNED(ll,   unsigned long long, ULLONG_MAX)
PF__OVF_UNSIGNED(8,    uint8_t,            UINT8_MAX)
PF__OVF_UNSIGNED(16,   uint16_t,           UINT16_MAX)
PF__OVF_UNSIGNED(32,   uint32_t,           UINT32_MAX)
PF__OVF_UNSIGNED(64,   uint64_t,           UINT64_MAX)
PF__OVF_UNSIGNED(max,  uintmax_t,          UINTMAX_MAX)
PF__OVF_UNSIGNED(ptr,  uintptr_t,          UINTPTR_MAX)
PF__OVF_UNSIGNED(size, size_t,             SIZE_MAX)
/* clang-format on */

PF_END_DECLS

/* ---- Type-generic ----------------------------------------------------- */

#if PF__OVF_BUILTIN
    #define PF_HAS_CKD 1
    #define PF_CKD_ADD(r, a, b) __builtin_add_overflow(a, b, r)
    #define PF_CKD_SUB(r, a, b) __builtin_sub_overflow(a, b, r)
    #define PF_CKD_MUL(r, a, b) __builtin_mul_overflow(a, b, r)
#elif PF__OVF_STDCKDINT
    #define PF_HAS_CKD 1
    #define PF_CKD_ADD(r, a, b) ckd_add(r, a, b)
    #define PF_CKD_SUB(r, a, b) ckd_sub(r, a, b)
    #define PF_CKD_MUL(r, a, b) ckd_mul(r, a, b)
#else
    #define PF_HAS_CKD 0
#endif

/* ---- Standard names --------------------------------------------------- */

#if ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_OVERFLOW)) \
     || defined(PF_STD_NAMES_OVERFLOW))                             \
    && !defined(__cplusplus)

    /* PF__FORCE_STD_NAMES_POLYFILL is for the tests only. */
    #if defined(__has_include) && !defined(PF__FORCE_STD_NAMES_POLYFILL)
        #if __has_include(<stdckdint.h>)
            #include <stdckdint.h>
        #endif
    #endif

    #if PF_HAS_CKD && !defined(ckd_add)
        #define ckd_add(r, a, b) PF_CKD_ADD(r, a, b)
        #define ckd_sub(r, a, b) PF_CKD_SUB(r, a, b)
        #define ckd_mul(r, a, b) PF_CKD_MUL(r, a, b)
    #endif

#endif

#endif /* CPOLYFILL_OVERFLOW_H */
