/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides functions for detecting integer overflow and saturated
    arithmetic. Builtin macros are used for better performance if available.

    pf_overflow_*   Returns 1 if operation overflowed, or 0 if not,
                    and stores the result in 'out'.

    pf_checked_*    Asserts that operation has not overflowed. Custom
                    assertions can be defined with pf_overflow_assert.

    pf_saturated_*  Performs saturated arithmetic, i.e. overflow returns
                    the maximum value and underflow returns the minimum.

    Function naming scheme is borrowed from gcc's overflow builtins.
    Depending on the type and operation, you should use following functions:

    |--------------------|----------|----------|----------|
    | Type Name          | ++++++++ | -------- | ******** |
    |--------------------|----------|----------|----------|
    | int                | sadd     | ssub     | smul     |
    | long               | saddl    | ssubl    | smull    |
    | long long          | saddll   | subll    | mulll    |
    | unsigned int       | uadd     | usub     | umul     |
    | unsigned long      | uaddl    | usubl    | umull    |
    | unsigned long long | uaddll   | usubll   | umulll   |
    |--------------------|----------|----------|----------|
    | int32_t            | sadd32   | ssub32   | smul32   |
    | int64_t            | sadd64   | ssub64   | smul64   |
    | intmax_t           | saddmax  | ssubmax  | smulmax  |
    | intptr_t           | saddptr  | ssubptr  | smulptr  |
    | uint32_t           | uadd32   | usub32   | umul32   |
    | uint64_t           | uadd64   | usub64   | umul64   |
    | uintmax_t          | uaddmax  | usubmax  | umulmax  |
    | uintptr_t          | uaddptr  | usubptr  | umulptr  |
    |--------------------|----------|----------|----------|
    | size_t             | uaddsize | usubsize | umulsize |
    | ptrdiff_t          | uadddiff | usubdiff | umuldiff |
    |--------------------|----------|----------|----------|

    Generic builtins are currently not supported.
    The fallback algorithm is not optimal and should be improved upon.

    Resources used:
    - learn.microsoft.com/en-us/windows/win32/api/intsafe/
    - gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
    - en.cppreference.com/w/c/header/stdckdint.html

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef POLYFILL_OVERFLOW
#define POLYFILL_OVERFLOW

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifndef pf_overflow_assert
    #include <assert.h>
    #define pf_overflow_assert assert
#endif

#ifndef PF_BOOL
    #define PF_BOOL
    #if defined(bool)
typedef bool pf_bool;
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef bool pf_bool;
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
typedef _Bool pf_bool;
    #else
typedef int pf_bool;
    #endif
#endif

#if SIZE_MAX == UINT_MAX
    #define PF_OVERFLOW_SIZE PF_OVERFLOW_UINT
#elif SIZE_MAX == ULONG_MAX
    #define PF_OVERFLOW_SIZE PF_OVERFLOW_ULONG
#elif SIZE_MAX == ULLONG_MAX
    #define PF_OVERFLOW_SIZE PF_OVERFLOW_ULLONG
#endif

#if PTRDIFF_MAX == INT_MAX
    #define PF_OVERFLOW_PTRDIFF PF_OVERFLOW_INT
#elif PTRDIFF_MAX == LONG_MAX
    #define PF_OVERFLOW_PTRDIFF PF_OVERFLOW_LONG
#elif PTRDIFF_MAX == LLONG_MAX
    #define PF_OVERFLOW_PTRDIFF PF_OVERFLOW_LLONG
#endif

#if UINT32_MAX == UINT_MAX
    #define PF_OVERFLOW_INT32 PF_OVERFLOW_INT
    #define PF_OVERFLOW_UINT32 PF_OVERFLOW_UINT
#elif UINT32_MAX == ULONG_MAX
    #define PF_OVERFLOW_INT32 PF_OVERFLOW_LONG
    #define PF_OVERFLOW_UINT32 PF_OVERFLOW_ULONG
#elif UINT32_MAX == ULLONG_MAX
    #define PF_OVERFLOW_INT32 PF_OVERFLOW_LLONG
    #define PF_OVERFLOW_UINT32 PF_OVERFLOW_ULLONG
#endif

#if UINT64_MAX == UINT_MAX
    #define PF_OVERFLOW_INT64 PF_OVERFLOW_INT
    #define PF_OVERFLOW_UINT64 PF_OVERFLOW_UINT
#elif UINT64_MAX == ULONG_MAX
    #define PF_OVERFLOW_INT64 PF_OVERFLOW_LONG
    #define PF_OVERFLOW_UINT64 PF_OVERFLOW_ULONG
#elif UINT64_MAX == ULLONG_MAX
    #define PF_OVERFLOW_INT64 PF_OVERFLOW_LLONG
    #define PF_OVERFLOW_UINT64 PF_OVERFLOW_ULLONG
#endif

#if UINTMAX_MAX == UINT_MAX
    #define PF_OVERFLOW_INTMAX PF_OVERFLOW_INT
    #define PF_OVERFLOW_UINTMAX PF_OVERFLOW_UINT
#elif UINTMAX_MAX == ULONG_MAX
    #define PF_OVERFLOW_INTMAX PF_OVERFLOW_LONG
    #define PF_OVERFLOW_UINTMAX PF_OVERFLOW_ULONG
#elif UINTMAX_MAX == ULLONG_MAX
    #define PF_OVERFLOW_INTMAX PF_OVERFLOW_LLONG
    #define PF_OVERFLOW_UINTMAX PF_OVERFLOW_ULLONG
#endif

#if UINTPTR_MAX == UINT_MAX
    #define PF_OVERFLOW_INTPTR PF_OVERFLOW_INT
    #define PF_OVERFLOW_UINTPTR PF_OVERFLOW_UINT
#elif UINTPTR_MAX == ULONG_MAX
    #define PF_OVERFLOW_INTPTR PF_OVERFLOW_LONG
    #define PF_OVERFLOW_UINTPTR PF_OVERFLOW_ULONG
#elif UINTPTR_MAX == ULLONG_MAX
    #define PF_OVERFLOW_INTPTR PF_OVERFLOW_LLONG
    #define PF_OVERFLOW_UINTPTR PF_OVERFLOW_ULLONG
#endif

#ifndef PF_API
    #define PF_API static inline
#endif

#ifndef pf_has_builtin
    #ifdef __has_builtin
        #define pf_has_builtin __has_builtin
    #else
        #define pf_has_builtin(...) 0
    #endif
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #include <stdckdint.h>
    #define PF__IMPL_OF_ADD(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
        ckd_add(out, a, b)
    #define PF__IMPL_OF_SUB(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
        ckd_sub(out, a, b)
    #define PF__IMPL_OF_MUL(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
        ckd_mul(out, a, b)
#elif defined(_MSC_VER)
    #include <intsafe.h>
    #define PF__IMPL_OF_ADD(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
        m_msvc##Add(a, b, out)
    #define PF__IMPL_OF_SUB(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
        m_msvc##Sub(a, b, out)
    #define PF__IMPL_OF_MUL(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
        m_msvc##Mult(a, b, out)
#else
    #if pf_has_builtin(__builtin_add_overflow)
        #define PF__IMPL_OF_ADD(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
            __builtin_add_overflow(a, b, out)
    #else
        #define PF__IMPL_OF_ADD(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
            (b > 0 && a > (m_max) - b) || (b < 0 && a < (m_min) - b)
    #endif
    #if pf_has_builtin(__builtin_sub_overflow)
        #define PF__IMPL_OF_SUB(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
            __builtin_sub_overflow(a, b, out)
    #else
        #define PF__IMPL_OF_SUB(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
            (b > 0 && a < (m_min) + b) || (b < 0 && a > (m_max) + b)
    #endif
    #if pf_has_builtin(__builtin_mul_overflow)
        #define PF__IMPL_OF_MUL(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
            __builtin_mul_overflow(a, b, out)
    #else
        #define PF__IMPL_OF_MUL(m_max, m_min, m_msvc, m_pre, m_post, m_type) \
            (a > (m_max) / b || a < (m_min) / b)
    #endif
#endif

#define PF__IMPL_OF(m_macro, m_type, m_name, m_impl, m_op)                 \
    PF_API pf_bool pf_overflow_##m_name(m_type a, m_type b, m_type *out) { \
        *out = a m_op b;                                                   \
        return m_macro(m_impl, m_type);                                    \
    }

#define PF__IMPL_CKD(m_type, m_name)                         \
    PF_API m_type pf_checked_##m_name(m_type a, m_type b) {  \
        m_type out;                                          \
        pf_bool overflow = pf_overflow_##m_name(a, b, &out); \
        pf_overflow_assert(!overflow);                       \
        return out;                                          \
    }

#define PF__IMPL_SA_(m_max, m_min, _1, _2, _3, m_type, m_name, m_impl) \
    PF_API m_type pf_saturated_##m_name(m_type a, m_type b) {          \
        m_type result;                                                 \
        if (pf_overflow_##m_name(a, b, &result))                       \
            return m_impl ? m_max : m_min;                             \
        return result;                                                 \
    }

#define PF__IMPL_SA(m_macro, m_type, m_name, m_impl) \
    m_macro(PF__IMPL_SA_, m_type, m_name, m_impl)

#define PF_IMPL_OVERFLOW(m_macro, m_type, m_pre, m_post)                   \
    PF__IMPL_OF(m_macro, m_type, m_pre##add##m_post, PF__IMPL_OF_ADD, +)   \
    PF__IMPL_OF(m_macro, m_type, m_pre##sub##m_post, PF__IMPL_OF_SUB, -)   \
    PF__IMPL_OF(m_macro, m_type, m_pre##mul##m_post, PF__IMPL_OF_MUL, *)   \
    PF__IMPL_CKD(m_type, m_pre##add##m_post)                               \
    PF__IMPL_CKD(m_type, m_pre##sub##m_post)                               \
    PF__IMPL_CKD(m_type, m_pre##mul##m_post)                               \
    PF__IMPL_SA(m_macro, m_type, m_pre##add##m_post, (a > 0))              \
    PF__IMPL_SA(m_macro, m_type, m_pre##sub##m_post, (!(a < -1 && b > 0))) \
    PF__IMPL_SA(                                                           \
        m_macro,                                                           \
        m_type,                                                            \
        m_pre##mul##m_post,                                                \
        ((a > 0 && b > 0) || (a < 0 && b < 0))                             \
    )

/* clang-format off */

#define PF_OVERFLOW_INT(X, ...)    X(INT_MAX,    INT_MIN,    Int,   s, ,   __VA_ARGS__)
#define PF_OVERFLOW_LONG(X, ...)   X(LONG_MAX,   LONG_MIN,   Long,  s, l,  __VA_ARGS__)
#define PF_OVERFLOW_LLONG(X, ...)  X(LLONG_MAX,  LLONG_MIN,  Long,  s, ll, __VA_ARGS__)
#define PF_OVERFLOW_UINT(X, ...)   X(UINT_MAX,   0,          UInt,  u, ,   __VA_ARGS__)
#define PF_OVERFLOW_ULONG(X, ...)  X(ULONG_MAX,  0,          ULong, u, l,  __VA_ARGS__)
#define PF_OVERFLOW_ULLONG(X, ...) X(ULLONG_MAX, 0,          ULong, u, ll, __VA_ARGS__)

PF_IMPL_OVERFLOW(PF_OVERFLOW_INT,       int,                s,     )
PF_IMPL_OVERFLOW(PF_OVERFLOW_LONG,      long,               s, l   )
PF_IMPL_OVERFLOW(PF_OVERFLOW_LLONG,     long long,          s, ll  )
PF_IMPL_OVERFLOW(PF_OVERFLOW_UINT,      unsigned int,       u,     )
PF_IMPL_OVERFLOW(PF_OVERFLOW_ULONG,     unsigned long,      u, l   )
PF_IMPL_OVERFLOW(PF_OVERFLOW_ULLONG,    unsigned long long, u, ll  )
PF_IMPL_OVERFLOW(PF_OVERFLOW_SIZE,      size_t,             u, size)
PF_IMPL_OVERFLOW(PF_OVERFLOW_PTRDIFF,   ptrdiff_t,          s, diff)
PF_IMPL_OVERFLOW(PF_OVERFLOW_INT32,     int32_t,            s, 32  )
PF_IMPL_OVERFLOW(PF_OVERFLOW_INT64,     int64_t,            s, 64  )
PF_IMPL_OVERFLOW(PF_OVERFLOW_INTMAX,    intmax_t,           s, max )
PF_IMPL_OVERFLOW(PF_OVERFLOW_INTPTR,    intptr_t,           s, ptr )
PF_IMPL_OVERFLOW(PF_OVERFLOW_UINT32,    uint32_t,           u, 32  )
PF_IMPL_OVERFLOW(PF_OVERFLOW_UINT64,    uint64_t,           u, 64  )
PF_IMPL_OVERFLOW(PF_OVERFLOW_UINTMAX,   uintmax_t,          u, max )
PF_IMPL_OVERFLOW(PF_OVERFLOW_UINTPTR,   uintptr_t,          u, ptr )

/* clang-format on */

#endif
