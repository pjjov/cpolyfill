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

    You can also define functions for new types using PF_IMPL_BITWISE macro.
    Define PF_BITWISE_SKIP_DEFAULT to omit generating predefined functions.

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
    #define PF__CLZ return (stdc_leading_zeros(x));
    #define PF__CTZ return (stdc_trailing_zeros(x));
    #define PF__POW2CEIL return stdc_bit_floor(x) != x ? stdc_bit_ceil(x) : x;
    #define PF__POW2FLOOR return (stdc_bit_floor(x));
    #define PF__POPCOUNT return (stdc_count_ones(x));
    #define PF__PARITY return (stdc_count_ones(x) & 1);
    #define PF__ISPOW2 return (stdc_has_single_bit(x));
#else
    #if pf_has_builtin(__builtin_stdc_bit_ceil)
        #define PF__POW2CEIL                        \
            return __builtin_stdc_bit_floor(x) != x \
                ? __builtin_stdc_bit_ceil(x)        \
                : x;
    #else
        #define PF__POW2CEIL                            \
            x--;                                        \
            for (int i = 1; i < sizeof(x) * 8; i <<= 1) \
                x |= x >> i;                            \
            return ++x;
    #endif

    #if pf_has_builtin(__builtin_stdc_bit_floor)
        #define PF__POW2FLOOR return (__builtin_stdc_bit_floor(x));
    #else
        #define PF__POW2FLOOR                           \
            for (int i = 1; i < sizeof(x) * 8; i <<= 1) \
                x |= x >> i;                            \
            return x - (x >> 1);
    #endif

    #if pf_has_builtin(__builtin_stdc_has_single_bit)
        #define PF__ISPOW2 return __builtin_stdc_has_single_bit(x);
    #else
        #define PF__ISPOW2 return (x && (x & (x - 1)));
    #endif

    #if pf_has_builtin(__builtin_ctzg)
        #define PF__CTZ return (x != 0 ? __builtin_ctzg(x) : sizeof(x) * 8);
    #else
        #define PF__CTZ                 \
            int out = sizeof((x)) * 8;  \
            if (x != 0) {               \
                x = (x ^ (x - 1)) >> 1; \
                for (out = 0; x; out++) \
                    x >>= 1;            \
            }                           \
            return out;
    #endif

    #if pf_has_builtin(__builtin_clzg)
        #define PF__CLZ return (x != 0 ? __builtin_clzg(x) : sizeof(x) * 8);
    #else
        #define PF__CLZ                                  \
            int out = sizeof(x) * 8;                     \
            if (x != 0) {                                \
                for (int i = out / 2; i != 0; i >>= 1) { \
                    if ((x >> i) != 0) {                 \
                        out -= i;                        \
                        x = x >> i;                      \
                    }                                    \
                }                                        \
            }                                            \
            return out;
    #endif

    #if pf_has_builtin(__builtin_popcountg)
        #define PF__POPCOUNT return (__builtin_popcountg(x));
    #else
        #define PF__POPCOUNT        \
            for (out = 0; x; out++) \
                x &= x - 1;         \
            return out;
    #endif

    #if pf_has_builtin(__builtin_parityg)
        #define PF__PARITY return (__builtin_parityg(x));
    #else
        #define PF__PARITY                    \
            int out;                          \
            for (out = 0; x; x = x & (x - 1)) \
                out = !out;                   \
            return out;
    #endif
#endif

#if pf_has_builtin(__builtin_stdc_rotate_right)
    #define PF__ROTR return (__builtin_stdc_rotate_right(x, i));
#else
    #define PF__ROTR                 \
        i &= (sizeof(x) * 8 - 1);    \
        lo = x & ((1ull << i) - 1);  \
        hi = x & ~((1ull << i) - 1); \
        lo <<= (sizeof(x) * 8 - i);  \
        hi >>= i;                    \
        return hi | lo;
#endif

#if pf_has_builtin(__builtin_stdc_rotate_left)
    #define PF__ROTL return (__builtin_stdc_rotate_left(x, i));
#else
    #define PF__ROTL           \
        i = sizeof(x) * 8 - i; \
        PF__ROTR(x, i);
#endif

#define PF_IMPL_BITWISE(m_post, m_type, m_max)                             \
    PF_API m_type pf_pow2ceil##m_post(m_type x) { PF__POW2CEIL; }          \
    PF_API m_type pf_pow2floor##m_post(m_type x) { PF__POW2FLOOR; }        \
    PF_API int pf_ispow2##m_post(m_type x) { PF__ISPOW2; }                 \
    PF_API int pf_clz##m_post(m_type x) { PF__CLZ; }                       \
    PF_API int pf_ctz##m_post(m_type x) { PF__CTZ; }                       \
    PF_API int pf_clo##m_post(m_type x) { return pf_clz##m_post(~x); }     \
    PF_API int pf_cto##m_post(m_type x) { return pf_ctz##m_post(~x); }     \
    PF_API int pf_flz##m_post(m_type x) { return pf_clz##m_post(~x) + 1; } \
    PF_API int pf_flo##m_post(m_type x) { return pf_clz##m_post(x) + 1; }  \
    PF_API int pf_ftz##m_post(m_type x) { return pf_ctz##m_post(~x) + 1; } \
    PF_API int pf_fto##m_post(m_type x) { return pf_ctz##m_post(~x) + 1; } \
    PF_API int pf_popcount##m_post(m_type x) { PF__POPCOUNT; }             \
    PF_API int pf_parity##m_post(m_type x) { PF__PARITY; }                 \
    PF_API int pf_zerocount##m_post(m_type x) {                            \
        return pf_popcount##m_post(~x);                                    \
    }                                                                      \
    PF_API m_type pf_rotl##m_post(m_type x, unsigned i) {                  \
        m_type lo, hi;                                                     \
        (void)hi;                                                          \
        (void)lo;                                                          \
        PF__ROTL;                                                          \
    }                                                                      \
    PF_API m_type pf_rotr##m_post(m_type x, unsigned i) {                  \
        m_type lo, hi;                                                     \
        (void)hi;                                                          \
        (void)lo;                                                          \
        PF__ROTR;                                                          \
    }

#ifndef PF_BITWISE_SKIP_DEFAULT

/* clang-format off */
PF_IMPL_BITWISE(,     unsigned int,       UINT_MAX);
PF_IMPL_BITWISE(l,    unsigned long,      ULONG_MAX);
PF_IMPL_BITWISE(ll,   unsigned long long, ULLONG_MAX);
PF_IMPL_BITWISE(size, size_t,             SIZE_MAX);
PF_IMPL_BITWISE(ptr,  uintptr_t,          UINTPTR_MAX);
PF_IMPL_BITWISE(max,  uintmax_t,          UINTMAX_MAX);
PF_IMPL_BITWISE(8,    uint8_t,            UINT8_MAX);
PF_IMPL_BITWISE(16,   uint16_t,           UINT16_MAX);
PF_IMPL_BITWISE(32,   uint32_t,           UINT32_MAX);
PF_IMPL_BITWISE(64,   uint64_t,           UINT64_MAX);
    /* clang-format on */

#endif

#if defined(__cplusplus)
}
#endif

#endif
