/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides functions for detecting integer overflow and saturated
    arithmetic. Builtin macros are used for better performance if available.

    Function naming scheme is borrowed from gcc's overflow builtins.
    Signed integers are prefixed with 's' and unsigned are postfixed with 'u'.
    Long integers are postfixed with 'l' and long longs are postfixed with 'll'.
    Addition, subtraction and multiplication is supported ('add', 'sub', 'mul').

    pf_overflow_*   Returns 1 if operation overflowed, or 0 if not,
                    and stores the result in 'out'.

    pf_checked_*    Asserts that operation has not overflowed. Custom
                    assertions can be defined with pf_overflow_assert.

    pf_saturated_*  Performs saturated arithmetic, i.e. overflow returns
                    the maximum value and underflow returns the minimum.

    C23's checked arithmetic and generic builtins are currently not supported.
    The fallback algorithm is not optimal and should be improved upon.

    Resources used:
    - https://learn.microsoft.com/en-us/windows/win32/api/intsafe/
    - https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html

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

#ifndef pf_overflow_assert
    #include <assert.h>
    #define pf_overflow_assert assert
#endif

#ifndef PF_BOOL
    #define PF_BOOL
    #if defined(bool) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef bool pf_bool;
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
typedef _Bool pf_bool;
    #else
typedef int pf_bool;
    #endif
#endif

#ifdef _MSC_VER
    #include <intsafe.h>
    #define PF_IMPL_OVERFLOW(m_name, m_msvc, m_type, m_op, m_check) \
        static inline pf_bool pf_overflow_##m_name(m_type a, m_type b, m_type *out) {\
            return m_msvc(a, b, out);\
        }
#elif defined(__GNUC__)
    #define PF_IMPL_OVERFLOW(m_name, m_msvc, m_type, m_op, m_check) \
        static inline pf_bool pf_overflow_##m_name(m_type a, m_type b, m_type *out) {\
            return __builtin_##m_name##_overflow(a, b, out);\
        }
#else
    #define PF_IMPL_OVERFLOW(m_name, m_msvc, m_type, m_op, m_check) \
        static inline pf_bool pf_overflow_##m_name(m_type a, m_type b, m_type *out) {\
            *out = a m_op b; return (m_check);
        }
#endif

#define PF_IMPL_CHECKED(m_name, m_type) \
    static inline m_type pf_checked_##m_name(m_type a, m_type b) {\
        m_type out; \
        pf_overflow_assert( \
            !pf_overflow_##m_name(a, b, &out) \
        );      \
        return out; \
    }

#define PF_IMPL_SATURATED(m_name, m_type, m_min, m_max, ...) \
    static inline m_type pf_saturated_##m_name(m_type x, m_type y) { \
        m_type result; \
        if (pf_overflow_##m_name(x, y, &result)) \
            return (__VA_ARGS__) ? m_max : m_min; \
        return result; \
    }

#define PF_IMPL_ADD(m_name, m_msvc, m_type, m_min, m_max) \
    PF_IMPL_OVERFLOW(m_name, m_msvc, m_type, +, (b > 0 && a > (m_max) - b) || (b < 0 && a < (m_min) - b)) \
    PF_IMPL_SATURATED(m_name, m_type, m_min, m_max, x > 0) \
    PF_IMPL_CHECKED(m_name, m_type)
#define PF_IMPL_SUB(m_name, m_msvc, m_type, m_min, m_max) \
    PF_IMPL_OVERFLOW(m_name, m_msvc, m_type, -, (b > 0 && a < (m_min) + b) || (b < 0 && a > (m_max) + b)) \
    PF_IMPL_SATURATED(m_name, m_type, m_min, m_max, !(x < -1 && y > 0)) \
    PF_IMPL_CHECKED(m_name, m_type)
#define PF_IMPL_MUL(m_name, m_msvc, m_type, m_min, m_max) \
    PF_IMPL_OVERFLOW(m_name, m_msvc, m_type, *, a > (m_max) / b || a < (m_min) / b) \
    PF_IMPL_SATURATED(m_name, m_type, m_min, m_max, (x > 0 && y > 0) || (x < 0 && y < 0)) \
    PF_IMPL_CHECKED(m_name, m_type)

PF_IMPL_ADD(sadd,   IntAdd,        int, INT_MIN, INT_MAX)
PF_IMPL_ADD(saddl,  LongAdd,       long, LONG_MIN, LONG_MAX)
PF_IMPL_ADD(saddll, LongLongAdd,   long long, LLONG_MIN, LLONG_MAX)
PF_IMPL_ADD(uadd,   UIntAdd,       unsigned int, 0, UINT_MAX)
PF_IMPL_ADD(uaddl,  ULongAdd,      unsigned long, 0, ULONG_MAX)
PF_IMPL_ADD(uaddll, ULongLongAdd,  unsigned long long, 0, ULLONG_MAX)

PF_IMPL_SUB(ssub,   IntSub,        int, INT_MIN, INT_MAX)
PF_IMPL_SUB(ssubl,  LongSub,       long, LONG_MIN, LONG_MAX)
PF_IMPL_SUB(ssubll, LongLongSub,   long long, LLONG_MIN, LLONG_MAX)
PF_IMPL_SUB(usub,   UIntSub,       unsigned int, 0, UINT_MAX)
PF_IMPL_SUB(usubl,  ULongSub,      unsigned long, 0, ULONG_MAX)
PF_IMPL_SUB(usubll, ULongLongSub,  unsigned long long, 0, ULLONG_MAX)

PF_IMPL_MUL(smul,   IntMult,       int, INT_MIN, INT_MAX)
PF_IMPL_MUL(smull,  LongMult,      long, LONG_MIN, LONG_MAX)
PF_IMPL_MUL(smulll, LongLongMult,  long long, LLONG_MIN, LLONG_MAX)
PF_IMPL_MUL(umul,   UIntMult,      unsigned int, 0, UINT_MAX)
PF_IMPL_MUL(umull,  ULongMult,     unsigned long, 0, ULONG_MAX)
PF_IMPL_MUL(umulll, ULongLongMult, unsigned long long, 0, ULLONG_MAX)

#undef PF_IMPL_ADD
#undef PF_IMPL_SUB
#undef PF_IMPL_MUL
#undef PF_IMPL_SATURATED
#undef PF_IMPL_CHECKED
#undef PF_IMPL_OVERFLOW

#endif
