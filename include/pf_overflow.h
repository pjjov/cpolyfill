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

#ifndef PF_API
    #define PF_API static inline
#endif

#define PF_IMPL_OF_add(m_min, m_max)                         \
    (b > 0 && a > (m_max) - b) || (b < 0 && a < (m_min) - b)
#define PF_IMPL_OF_sub(m_min, m_max)                         \
    (b > 0 && a < (m_min) + b) || (b < 0 && a > (m_max) + b)
#define PF_IMPL_OF_mul(m_min, m_max) (a > (m_max) / b || a < (m_min) / b)
#define PF_IMPL_SA_add (a > 0)
#define PF_IMPL_SA_sub (!(a < -1 && b > 0))
#define PF_IMPL_SA_mul ((a > 0 && b > 0) || (a < 0 && b < 0))

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #include <stdckdint.h>
    #define PF_IMPL_OF(m_op, m_ch, m_gcc, m_msvc, m_type, m_min, m_max) \
        return ckd_##m_op((m_type *)out, (m_type)a, (m_type)b)
#elif defined(_MSC_VER)
    #include <intsafe.h>
    #define PF_IMPL_OF(m_op, m_ch, m_gcc, m_msvc, m_type, m_min, m_max) \
        return m_msvc(a, b, out)
#elif defined(__GNUC__)
    #define PF_IMPL_OF(m_op, m_ch, m_gcc, m_msvc, m_type, m_min, m_max) \
        return __builtin_##m_gcc##_overflow(a, b, out)
#else
    #define PF_IMPL_OF(m_op, m_ch, m_gcc, m_msvc, m_type, m_min, m_max) \
        *out = a m_ch b;                                                \
        return PF_IMPL_OF_##m_op(m_min, m_max)
#endif

#define PF_IMPL(m_op, m_ch, m_gcc, m_msvc, m_type, m_min, m_max)          \
    PF_API pf_bool pf_overflow_##m_gcc(m_type a, m_type b, m_type *out) { \
        PF_IMPL_OF(m_op, m_ch, m_gcc, m_msvc, m_type, m_min, m_max);      \
    }                                                                     \
    PF_API m_type pf_checked_##m_gcc(m_type a, m_type b) {                \
        m_type out;                                                       \
        pf_bool overflow = pf_overflow_##m_gcc(a, b, &out);               \
        pf_overflow_assert(!overflow);                                    \
        return out;                                                       \
    }                                                                     \
    PF_API m_type pf_saturated_##m_gcc(m_type a, m_type b) {              \
        m_type result;                                                    \
        if (pf_overflow_##m_gcc(a, b, &result))                           \
            return (PF_IMPL_SA_##m_op) ? m_max : m_min;                   \
        return result;                                                    \
    }

/* clang-format off */

PF_IMPL(add, +, sadd,   IntAdd,        int,          INT_MIN, INT_MAX)
PF_IMPL(add, +, saddl,  LongAdd,       long,        LONG_MIN, LONG_MAX)
PF_IMPL(add, +, saddll, LongLongAdd,   long long,  LLONG_MIN, LLONG_MAX)
PF_IMPL(add, +, uadd,   UIntAdd,       unsigned int,       0, UINT_MAX)
PF_IMPL(add, +, uaddl,  ULongAdd,      unsigned long,      0, ULONG_MAX)
PF_IMPL(add, +, uaddll, ULongLongAdd,  unsigned long long, 0, ULLONG_MAX)

PF_IMPL(sub, -, ssub,   IntSub,        int,          INT_MIN, INT_MAX)
PF_IMPL(sub, -, ssubl,  LongSub,       long,        LONG_MIN, LONG_MAX)
PF_IMPL(sub, -, ssubll, LongLongSub,   long long,  LLONG_MIN, LLONG_MAX)
PF_IMPL(sub, -, usub,   UIntSub,       unsigned int,       0, UINT_MAX)
PF_IMPL(sub, -, usubl,  ULongSub,      unsigned long,      0, ULONG_MAX)
PF_IMPL(sub, -, usubll, ULongLongSub,  unsigned long long, 0, ULLONG_MAX)

PF_IMPL(mul, *, smul,   IntMult,       int,          INT_MIN, INT_MAX)
PF_IMPL(mul, *, smull,  LongMult,      long,        LONG_MIN, LONG_MAX)
PF_IMPL(mul, *, smulll, LongLongMult,  long long,  LLONG_MIN, LLONG_MAX)
PF_IMPL(mul, *, umul,   UIntMult,      unsigned int,       0, UINT_MAX)
PF_IMPL(mul, *, umull,  ULongMult,     unsigned long,      0, ULONG_MAX)
PF_IMPL(mul, *, umulll, ULongLongMult, unsigned long long, 0, ULLONG_MAX)

/* clang-format on */

#undef PF_IMPL
#undef PF_IMPL_OF
#undef PF_IMPL_OF_add
#undef PF_IMPL_OF_sub
#undef PF_IMPL_OF_mul
#undef PF_IMPL_SA_add
#undef PF_IMPL_SA_sub
#undef PF_IMPL_SA_mul

#define PF__IMPL(m_name, m_impl, m_type)                                   \
    PF_API pf_bool pf_overflow_##m_name(m_type a, m_type b, m_type *out) { \
        return pf_overflow_##m_impl(a, b, out);                            \
    }                                                                      \
    PF_API m_type pf_checked_##m_name(m_type a, m_type b) {                \
        return pf_checked_##m_impl(a, b);                                  \
    }                                                                      \
    PF_API m_type pf_saturated_##m_name(m_type a, m_type b) {              \
        return pf_saturated_##m_impl(a, b);                                \
    }

#define PF_IMPL(m_type, m_pre1, m_post1, m_pre2, m_post2)        \
    PF__IMPL(m_pre1##add##m_post1, m_pre2##add##m_post2, m_type) \
    PF__IMPL(m_pre1##sub##m_post1, m_pre2##sub##m_post2, m_type) \
    PF__IMPL(m_pre1##mul##m_post1, m_pre2##mul##m_post2, m_type)

#if SIZE_MAX == UINT_MAX
PF_IMPL(size_t, u, size, u, )
#elif SIZE_MAX == ULONG_MAX
PF_IMPL(size_t, u, size, u, l)
#elif SIZE_MAX == ULLONG_MAX
PF_IMPL(size_t, u, size, u, ll)
#endif

#if PTRDIFF_MAX == INT_MAX
PF_IMPL(ptrdiff_t, s, diff, s, )
#elif PTRDIFF_MAX == LONG_MAX
PF_IMPL(ptrdiff_t, s, diff, s, l)
#elif PTRDIFF_MAX == LLONG_MAX
PF_IMPL(ptrdiff_t, s, diff, s, ll)
#endif

#if UINT32_MAX == UINT_MAX
PF_IMPL(int32_t, s, 32, s, )
PF_IMPL(uint32_t, u, 32, u, )
#elif UINT32_MAX == ULONG_MAX
PF_IMPL(int32_t, s, 32, s, l)
PF_IMPL(uint32_t, u, 32, u, l)
#elif UINT32_MAX == ULLONG_MAX
PF_IMPL(int32_t, s, 32, s, ll)
PF_IMPL(uint32_t, u, 32, u, ll)
#endif

#if UINT64_MAX == UINT_MAX
PF_IMPL(int64_t, s, 64, s, )
PF_IMPL(uint64_t, u, 64, u, )
#elif UINT64_MAX == ULONG_MAX
PF_IMPL(int64_t, s, 64, s, l)
PF_IMPL(uint64_t, u, 64, u, l)
#elif UINT64_MAX == ULLONG_MAX
PF_IMPL(int64_t, s, 64, s, ll)
PF_IMPL(uint64_t, u, 64, u, ll)
#endif

#if UINTMAX_MAX == UINT_MAX
PF_IMPL(intmax_t, s, max, s, )
PF_IMPL(uintmax_t, u, max, u, )
#elif UINTMAX_MAX == ULONG_MAX
PF_IMPL(intmax_t, s, max, s, l)
PF_IMPL(uintmax_t, u, max, u, l)
#elif UINTMAX_MAX == ULLONG_MAX
PF_IMPL(intmax_t, s, max, s, ll)
PF_IMPL(uintmax_t, u, max, u, ll)
#endif

#if UINTPTR_MAX == UINT_MAX
PF_IMPL(intptr_t, s, ptr, s, )
PF_IMPL(uintptr_t, u, ptr, u, )
#elif UINTPTR_MAX == ULONG_MAX
PF_IMPL(intptr_t, s, ptr, s, l)
PF_IMPL(uintptr_t, u, ptr, u, l)
#elif UINTPTR_MAX == ULLONG_MAX
PF_IMPL(intptr_t, s, ptr, s, ll)
PF_IMPL(uintptr_t, u, ptr, u, ll)
#endif

#undef PF_IMPL
#undef PF__IMPL

#endif
