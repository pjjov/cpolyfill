/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides portable implementations of bitwise operations present
    in many compiler extensions. Builtin macros are used if available.

        pf_ctz*          Returns the number of trailing '0' bit.
        pf_clz*          Returns the number of leading '0' bit.
        pf_cto*          Returns the number of trailing '1' bit.
        pf_clo*          Returns the number of leading '1' bit.
        pf_ftz*          Finds the position of the first trailing '0' bit.
        pf_flz*          Finds the position of the first leading '0' bit.
        pf_fto*          Finds the position of the first trailing '1' bit.
        pf_flo*          Finds the position of the first leading '1' bit.
        pf_ispow2*       Returns true if a given number is a power of 2.
        pf_pow2ceil*     Rounds up to the next highest power of 2.
        pf_pow2floor*    Rounds up to the next lowest power of 2.
        pf_popcount*     Counts the number of '1' bits.
        pf_zerocount*    Counts the number of '0' bits.
        pf_parity*       Returns zero if the number of '1' bits is even.
        pf_rotr*         Rotates the bits to the right.
        pf_rotl*         Rotates the bits to the left.

    Depending on the integer type, the '*' should be swapped to a different
    postfix. If we wanted to use the `clz` function for example we'd use:

        unsigned int       => pf_ctz
        unsigned long      => pf_ctzl
        unsigned long long => pf_ctzll
        size_t             => pf_ctzsize
        uintmax_t          => pf_ctzmax
        uint8_t            => pf_ctz8
        uint16_t           => pf_ctz16
        uint32_t           => pf_ctz32
        uint64_t           => pf_ctz64

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

#ifndef POLYFILL_BITWISE
#define POLYFILL_BITWISE

#if defined(__cplusplus)
extern "C" {
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

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
    #include <stdbit.h>
    #define pf__clz(x, out) out = (stdc_leading_zeros(x));
    #define pf__ctz(x, out) out = (stdc_trailing_zeros(x));
    #define pf__flz(x, out) out = (stdc_first_leading_zero(x));
    #define pf__ftz(x, out) out = (stdc_first_trailing_zero(x));
    #define pf__flo(x, out) out = (stdc_first_leading_one(x));
    #define pf__fto(x, out) out = (stdc_first_trailing_one(x));
    #define pf__pow2ceil(x) x = stdc_bit_floor(x) != x ? stdc_bit_ceil(x) : x;
    #define pf__pow2floor(x) x = (stdc_bit_floor(x));
    #define pf__popcount(x, out) out = (stdc_count_ones(x));
    #define pf__zerocount(x, out) out = (stdc_count_zeros(x));
    #define pf__parity(x, out) out = (stdc_count_ones(x) & 1);
    #define pf__ispow2(x, out) out = (stdc_has_single_bit(x));
#else
    #if pf_has_builtin(__builtin_stdc_bit_ceil)
        #define pf__pow2ceil(x)                                               \
            x = __builtin_stdc_bit_floor(x) != x ? __builtin_stdc_bit_ceil(x) \
                                                 : x;
    #else
        #define pf__pow2ceil(x)                         \
            x--;                                        \
            for (int i = 1; i < sizeof(x) * 8; i <<= 1) \
                x |= x >> i;                            \
            x++;
    #endif

    #if pf_has_builtin(__builtin_stdc_bit_floor)
        #define pf__pow2floor(x) x = (__builtin_stdc_bit_floor(x));
    #else
        #define pf__pow2floor(x)                        \
            for (int i = 1; i < sizeof(x) * 8; i <<= 1) \
                x |= x >> i;                            \
            x -= x >> 1;
    #endif

    #if pf_has_builtin(__builtin_stdc_has_single_bit)
        #define pf__ispow2(x, out) out = __builtin_stdc_has_single_bit(x);
    #else
        #define pf__ispow2(x, out) out = (x && (x & (x - 1)));
    #endif

    #if pf_has_builtin(__builtin_ctzg)
        #define pf__ctz(x, out)                                 \
            out = (x != 0 ? __builtin_ctzg(x) : sizeof(x) * 8);
    #else
        #define pf__ctz(x, out)         \
            out = sizeof((x)) * 8;      \
            if (x != 0) {               \
                x = (x ^ (x - 1)) >> 1; \
                for (out = 0; x; out++) \
                    x >>= 1;            \
            }
    #endif

    #if pf_has_builtin(__builtin_clzg)
        #define pf__clz(x, out)                                 \
            out = (x != 0 ? __builtin_clzg(x) : sizeof(x) * 8);
    #else
        #define pf__clz(x, out)                          \
            out = sizeof(x) * 8;                         \
            if (x != 0) {                                \
                for (int i = out / 2; i != 0; i >>= 1) { \
                    if ((x >> i) != 0) {                 \
                        out -= i;                        \
                        x = x >> i;                      \
                    }                                    \
                }                                        \
            }
    #endif

    #if pf_has_builtin(__builtin_stdc_first_leading_zero)
        #define pf__flz(x, out) out = (__builtin_stdc_first_leading_zero(x));
    #else
        #define pf__flz(x, out)            \
            x = ~x;                        \
            pf__clz(x, out);               \
            if (out != 0)                  \
                out = sizeof(x) * 8 - out;
    #endif

    #if pf_has_builtin(__builtin_stdc_first_trailing_zero)
        #define pf__ftz(x, out) out = (__builtin_stdc_first_trailing_zero(x));
    #else
        #define pf__ftz(x, out)         \
            x = ~x;                     \
            pf__ctz(x, out);            \
            if (out++ == sizeof(x) * 8) \
                out = 0;
    #endif

    #if pf_has_builtin(__builtin_stdc_first_leading_one)
        #define pf__flo(x, out) out = (__builtin_stdc_first_leading_one(x))
    #else
        #define pf__flo(x, out)    \
            pf__clz(x, out);       \
            out = sizeof(x) - out;
    #endif

    #if pf_has_builtin(__builtin_stdc_first_trailing_one)
        #define pf__fto(x, out) out = (__builtin_stdc_first_trailing_one(x));
    #else
        #define pf__fto(x, out)         \
            x = ~x;                     \
            pf__ctz(x, out);            \
            if (out++ == sizeof(x) * 8) \
                out = 0;
    #endif

    #if pf_has_builtin(__builtin_popcountg)
        #define pf__popcount(x, out) out = (__builtin_popcountg(x));
    #else
        #define pf__popcount(x, out) \
            for (out = 0; x; out++)  \
                x &= x - 1;          \
            return out;
    #endif

    #if pf_has_builtin(__builtin_popcountg)
        #define pf__zerocount(x, out)                       \
            out = (sizeof(x) * 8 - __builtin_popcountg(x));
    #else
        #define pf__zerocount(x, out)  \
            for (out = 0; x; out++)    \
                x &= x - 1;            \
            out = sizeof(x) * 8 - out;
    #endif

    #if pf_has_builtin(__builtin_parityg)
        #define pf__parity(x, out) out = (__builtin_parityg(x));
    #else
        #define pf__parity(x, out)            \
            for (out = 0; x; x = x & (x - 1)) \
                out = !out;
    #endif
#endif

#if pf_has_builtin(__builtin_stdc_rotate_right)
    #define pf__rotr(x, i) x = (__builtin_stdc_rotate_right(x, i));
#else
    #define pf__rotr(x, i)           \
        i &= (sizeof(x) * 8 - 1);    \
        lo = x & ((1ull << i) - 1);  \
        hi = x & ~((1ull << i) - 1); \
        lo <<= (sizeof(x) * 8 - i);  \
        hi >>= i;                    \
        x = hi | lo;
#endif

#if pf_has_builtin(__builtin_stdc_rotate_left)
    #define pf__rotl(x, i) x = (__builtin_stdc_rotate_left(x, i));
#else
    #define pf__rotl(x, i)     \
        i = sizeof(x) * 8 - i; \
        pf__rotr(x, i);
#endif

#define PF__IMPL_BITWISE(m_post, m_type, m_max)           \
    PF_API m_type pf_pow2ceil##m_post(m_type x) {         \
        pf__pow2ceil(x);                                  \
        return x;                                         \
    }                                                     \
    PF_API m_type pf_pow2floor##m_post(m_type x) {        \
        pf__pow2floor(x);                                 \
        return x;                                         \
    }                                                     \
    PF_API int pf_ispow2##m_post(m_type x) {              \
        int out;                                          \
        pf__ispow2(x, out);                               \
        return out;                                       \
    }                                                     \
    PF_API int pf_clz##m_post(m_type x) {                 \
        int out;                                          \
        pf__clz(x, out);                                  \
        return out;                                       \
    }                                                     \
    PF_API int pf_ctz##m_post(m_type x) {                 \
        int out;                                          \
        pf__ctz(x, out);                                  \
        return out;                                       \
    }                                                     \
    PF_API int pf_flz##m_post(m_type x) {                 \
        int out;                                          \
        pf__flz(x, out);                                  \
        return out;                                       \
    }                                                     \
    PF_API int pf_ftz##m_post(m_type x) {                 \
        int out;                                          \
        pf__ftz(x, out);                                  \
        return out;                                       \
    }                                                     \
    PF_API int pf_flo##m_post(m_type x) {                 \
        int out;                                          \
        pf__flo(x, out);                                  \
        return out;                                       \
    }                                                     \
    PF_API int pf_fto##m_post(m_type x) {                 \
        int out;                                          \
        pf__fto(x, out);                                  \
        return out;                                       \
    }                                                     \
    PF_API int pf_popcount##m_post(m_type x) {            \
        int out;                                          \
        pf__popcount(x, out);                             \
        return out;                                       \
    }                                                     \
    PF_API int pf_zerocount##m_post(m_type x) {           \
        int out;                                          \
        pf__zerocount(x, out);                            \
        return out;                                       \
    }                                                     \
    PF_API int pf_parity##m_post(m_type x) {              \
        int out;                                          \
        pf__parity(x, out);                               \
        return out;                                       \
    }                                                     \
    PF_API m_type pf_rotl##m_post(m_type x, unsigned i) { \
        m_type lo, hi;                                    \
        (void)hi;                                         \
        (void)lo;                                         \
        pf__rotl(x, i);                                   \
        return x;                                         \
    }                                                     \
    PF_API m_type pf_rotr##m_post(m_type x, unsigned i) { \
        m_type lo, hi;                                    \
        (void)hi;                                         \
        (void)lo;                                         \
        pf__rotr(x, i);                                   \
        return x;                                         \
    }

/* clang-format off */
PF__IMPL_BITWISE(,     unsigned int,       UINT_MAX);
PF__IMPL_BITWISE(l,    unsigned long,      ULONG_MAX);
PF__IMPL_BITWISE(ll,   unsigned long long, ULLONG_MAX);
PF__IMPL_BITWISE(size, size_t,             SIZE_MAX);
PF__IMPL_BITWISE(ptr,  uintptr_t,          UINTPTR_MAX);
PF__IMPL_BITWISE(max,  uintmax_t,          UINTMAX_MAX);
PF__IMPL_BITWISE(8,    uint8_t,            UINT8_MAX);
PF__IMPL_BITWISE(16,   uint16_t,           UINT16_MAX);
PF__IMPL_BITWISE(32,   uint32_t,           UINT32_MAX);
PF__IMPL_BITWISE(64,   uint64_t,           UINT64_MAX);
/* clang-format on */

#if defined(__cplusplus)
}
#endif

#endif
