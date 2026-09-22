/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides portable implementations of bitwise operations present
    in many compiler extensions. Builtin macros are used if available.

        pf_ctz*          Returns the number of trailing '0' bits.
        pf_clz*          Returns the number of leading '0' bits.
        pf_cto*          Returns the number of trailing '1' bits.
        pf_clo*          Returns the number of leading '1' bits.
        pf_ftz*          Finds the position of the first trailing '0' bit.
        pf_flz*          Finds the position of the first leading '0' bit.
        pf_fto*          Finds the position of the first trailing '1' bit.
        pf_flo*          Finds the position of the first leading '1' bit.
        pf_ispow2*       Returns true if a given number is a power of 2.
        pf_pow2ceil*     Rounds up to the next power of 2.
        pf_pow2floor*    Rounds down to the previous power of 2.
        pf_popcount*     Counts the number of '1' bits.
        pf_zerocount*    Counts the number of '0' bits.
        pf_parity*       Returns zero if the number of '1' bits is even.
        pf_bitreverse*   Reverses the order of all bits.
        pf_rotr*         Rotates the bits to the right.
        pf_rotl*         Rotates the bits to the left.

    Depending on the integer type, the '*' should be swapped to a different
    postfix. If we wanted to use the `ctz` function for example we'd use:

        unsigned int       => pf_ctz
        unsigned long      => pf_ctzl
        unsigned long long => pf_ctzll
        size_t             => pf_ctzsize
        uintptr_t          => pf_ctzptr
        uintmax_t          => pf_ctzmax
        uint8_t            => pf_ctz8
        uint16_t           => pf_ctz16
        uint32_t           => pf_ctz32
        uint64_t           => pf_ctz64

    Type-generic macros (C11 and later, C only) pick the right function from
    the type of the argument. They use the upper-case name of the operation:

        PF_CTZ(x)   PF_CLZ(x)   PF_CTO(x)   PF_CLO(x)   PF_FTZ(x)   PF_FLZ(x)
        PF_FTO(x)   PF_FLO(x)   PF_ISPOW2(x)  PF_POW2CEIL(x)  PF_POW2FLOOR(x)
        PF_POPCOUNT(x)  PF_ZEROCOUNT(x)  PF_PARITY(x)  PF_BITREVERSE(x)
        PF_ROTL(x, n)   PF_ROTR(x, n)

    The argument must have exactly one of the types unsigned char, unsigned
    short, unsigned int, unsigned long or unsigned long long. Signed types and
    integer-promoted expressions are rejected at compile time; cast them
    first, e.g. PF_CTZ((uint8_t)(a & b)) or PF_CTZ(8u). The result of the
    value-returning macros has the same type as the argument.

    Semantics for edge cases (identical on every code path):

        ctz/clz(0)         => number of bits in the type
        cto/clo(all ones)  => number of bits in the type
        ftz/fto            => 1-based position counted from the LSB, or 0 if
                              no such bit exists (same convention as ffs()).
        flz/flo            => 1-based position counted from the MSB, or 0 if
                              no such bit exists.
        pow2floor(0)       => 0
        pow2ceil(x)        => 0 if x is 0 or the result is not representable.
        ispow2(0)          => 0
        rotl/rotr          => the count is taken modulo the bit width.

    Implementation tiers, best first, chosen per operation:

        1. C23 <stdbit.h>
        2. GCC/Clang type-generic builtins (__builtin_ctzg, ...)
        3. GCC/Clang classic builtins (__builtin_ctz/ctzl/ctzll, ...) picked
           by type size; also Clang's __builtin_bitreverse*
        4. MSVC intrinsics (_BitScanForward/Reverse[64], _rotl*, __popcnt*)
        5. Portable C

    Configuration macros (define before including this file):

        PF_API                      Linkage/specifiers of every function.
                                    Defaults to `static inline`, or
                                    `static inline constexpr` in C++14+ when
                                    the compiler tier allows it.
        PF_USE_CLASSIC=0/1          Force the classic builtin tier on or off.
        PF_USE_MSVC=0/1             Force the MSVC intrinsic tier on or off.
        PF_MSVC_POPCNT              Allow __popcnt/__popcnt64 on x86/x64. Off by
                                    default because they fault on CPUs without
                                    the POPCNT instruction.
        PF_BITWISE_NO_GENERIC       Do not define the PF_CLZ(x)-style macros.
        PF_BITWISE_SKIP_DEFAULT     Do not generate the predefined functions
                                    (this also disables the generic macros).
        pf_has_builtin(x)           Override builtin detection.

    You can also define functions for new types using PF_IMPL_BITWISE macro.
    Note that the MSVC tier is not constexpr: with it, C++ gets plain
    `static inline` functions.

    Last-updated: August 2026
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025-2026 Предраг Јовановић

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

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Tier selection --------------------------------------------------- */

#ifndef PF_USE_CLASSIC
    #if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
        #define PF_USE_CLASSIC 1
    #else
        #define PF_USE_CLASSIC 0
    #endif
#endif

#ifndef PF_USE_MSVC
    #if defined(_MSC_VER) && !PF_USE_CLASSIC
        #define PF_USE_MSVC 1
    #else
        #define PF_USE_MSVC 0
    #endif
#endif

#if PF_USE_MSVC
    #include <intrin.h>
    /* _BitScan*64 and __popcnt64 only exist on 64-bit targets. */
    #if defined(_M_X64) || defined(_M_ARM64) || defined(_M_ARM64EC)
        #define PF__MSVC_64 1
    #else
        #define PF__MSVC_64 0
    #endif
    #if defined(PF_MSVC_POPCNT) && (defined(_M_X64) || defined(_M_IX86))
        #define PF__MSVC_POPCNT 1
    #else
        #define PF__MSVC_POPCNT 0
    #endif
#endif

#if defined(_MSVC_LANG)
    #define PF__CPP _MSVC_LANG
#elif defined(__cplusplus)
    #define PF__CPP __cplusplus
#else
    #define PF__CPP 0L
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef PF_API
    /* Uninitialised locals are avoided everywhere so C++14 constexpr works. */
    #if PF__CPP >= 201402L && !PF_USE_MSVC
        #define PF_API static inline constexpr
    #else
        #define PF_API static inline
    #endif
#endif

#ifndef pf_has_builtin
    #ifdef __has_builtin
        #define pf_has_builtin __has_builtin
    #else
        #define pf_has_builtin(...) 0
    #endif
#endif

/* Only use <stdbit.h> when the standard library actually ships it. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L \
    && defined(__has_include)
    #if __has_include(<stdbit.h>)
        #include <stdbit.h>
        #define PF__HAVE_STDBIT 1
    #endif
#endif

/* Width of a type in bits. */
#define PF__BITS(m_type) ((unsigned)(sizeof(m_type) * CHAR_BIT))

/* ---- Portable bodies (always available) -------------------------------- */

#define PF__P_CLZ(m_type)                                 \
    {                                                     \
        int out = (int)PF__BITS(m_type) - 1;              \
        unsigned i = 0;                                   \
        if (x == 0)                                       \
            return (int)PF__BITS(m_type);                 \
        for (i = PF__BITS(m_type) / 2; i != 0; i >>= 1) { \
            if ((x >> i) != 0) {                          \
                out -= (int)i;                            \
                x = (m_type)(x >> i);                     \
            }                                             \
        }                                                 \
        return out;                                       \
    }

#define PF__P_CTZ(m_type)                 \
    {                                     \
        int out = 0;                      \
        if (x == 0)                       \
            return (int)PF__BITS(m_type); \
        while ((x & 1) == 0) {            \
            x = (m_type)(x >> 1);         \
            out++;                        \
        }                                 \
        return out;                       \
    }

#define PF__P_POPCOUNT(m_type)         \
    {                                  \
        int out = 0;                   \
        for (; x; out++)               \
            x = (m_type)(x & (x - 1)); \
        return out;                    \
    }

#define PF__P_PARITY(m_type)                 \
    {                                        \
        int out = 0;                         \
        for (; x; x = (m_type)(x & (x - 1))) \
            out = !out;                      \
        return out;                          \
    }

#define PF__P_POW2CEIL(m_type)                     \
    {                                              \
        unsigned i = 0;                            \
        x--;                                       \
        for (i = 1; i < PF__BITS(m_type); i <<= 1) \
            x = (m_type)(x | (x >> i));            \
        return (m_type)(x + 1);                    \
    }

#define PF__P_POW2FLOOR(m_type)                    \
    {                                              \
        unsigned i = 0;                            \
        for (i = 1; i < PF__BITS(m_type); i <<= 1) \
            x = (m_type)(x | (x >> i));            \
        return (m_type)(x - (x >> 1));             \
    }

/* Swaps halves, then quarters, ... down to single bits. */
#define PF__P_BITREVERSE(m_type)                                          \
    {                                                                     \
        m_type mask = (m_type) ~(m_type)0;                                \
        unsigned s = PF__BITS(m_type);                                    \
        while ((s >>= 1) != 0) {                                          \
            mask = (m_type)(mask ^ (m_type)(mask << s));                  \
            x = (m_type)(((x >> s) & mask) | ((x << s) & (m_type)~mask)); \
        }                                                                 \
        return x;                                                         \
    }

/* Rotations: the count is reduced modulo the width and the complementary
 * shift is masked too, so a shift by the full width never happens. */
#define PF__P_ROTR(m_type)                                         \
    return (m_type)((x >> (i & (PF__BITS(m_type) - 1u)))           \
                    | (x << ((0u - i) & (PF__BITS(m_type) - 1u))))

#define PF__P_ROTL(m_type)                                         \
    return (m_type)((x << (i & (PF__BITS(m_type) - 1u)))           \
                    | (x >> ((0u - i) & (PF__BITS(m_type) - 1u))))

/* ---- Classic GCC/Clang builtins, dispatched on sizeof ------------------
 * Each fragment is a series of `if (...) return ...;` statements that fall
 * through to the next tier when the type is wider than unsigned long long. */

#if PF_USE_CLASSIC
    #define PF__CLASSIC_CLZ(m_type)                                       \
        if (x == 0)                                                       \
            return (int)PF__BITS(m_type);                                 \
        if (sizeof(m_type) <= sizeof(unsigned int))                       \
            return __builtin_clz((unsigned int)x)                         \
                - (int)(PF__BITS(unsigned int) - PF__BITS(m_type));       \
        if (sizeof(m_type) <= sizeof(unsigned long))                      \
            return __builtin_clzl((unsigned long)x)                       \
                - (int)(PF__BITS(unsigned long) - PF__BITS(m_type));      \
        if (sizeof(m_type) <= sizeof(unsigned long long))                 \
            return __builtin_clzll((unsigned long long)x)                 \
                - (int)(PF__BITS(unsigned long long) - PF__BITS(m_type));

    #define PF__CLASSIC_CTZ(m_type)                        \
        if (x == 0)                                        \
            return (int)PF__BITS(m_type);                  \
        if (sizeof(m_type) <= sizeof(unsigned int))        \
            return __builtin_ctz((unsigned int)x);         \
        if (sizeof(m_type) <= sizeof(unsigned long))       \
            return __builtin_ctzl((unsigned long)x);       \
        if (sizeof(m_type) <= sizeof(unsigned long long))  \
            return __builtin_ctzll((unsigned long long)x);

    #define PF__CLASSIC_POPCOUNT(m_type)                        \
        if (sizeof(m_type) <= sizeof(unsigned int))             \
            return __builtin_popcount((unsigned int)x);         \
        if (sizeof(m_type) <= sizeof(unsigned long))            \
            return __builtin_popcountl((unsigned long)x);       \
        if (sizeof(m_type) <= sizeof(unsigned long long))       \
            return __builtin_popcountll((unsigned long long)x);

    #define PF__CLASSIC_PARITY(m_type)                        \
        if (sizeof(m_type) <= sizeof(unsigned int))           \
            return __builtin_parity((unsigned int)x);         \
        if (sizeof(m_type) <= sizeof(unsigned long))          \
            return __builtin_parityl((unsigned long)x);       \
        if (sizeof(m_type) <= sizeof(unsigned long long))     \
            return __builtin_parityll((unsigned long long)x);
#else
    #define PF__CLASSIC_CLZ(m_type)
    #define PF__CLASSIC_CTZ(m_type)
    #define PF__CLASSIC_POPCOUNT(m_type)
    #define PF__CLASSIC_PARITY(m_type)
#endif

/* ---- MSVC intrinsics --------------------------------------------------- */

#if PF_USE_MSVC
    #define PF__MSVC_CLZ32(m_type)                         \
        if (sizeof(m_type) <= 4) {                         \
            unsigned long idx = 0;                         \
            return _BitScanReverse(&idx, (unsigned long)x) \
                ? (int)(PF__BITS(m_type) - 1u - idx)       \
                : (int)PF__BITS(m_type);                   \
        }
    #define PF__MSVC_CTZ32(m_type)                         \
        if (sizeof(m_type) <= 4) {                         \
            unsigned long idx = 0;                         \
            return _BitScanForward(&idx, (unsigned long)x) \
                ? (int)idx                                 \
                : (int)PF__BITS(m_type);                   \
        }
    #if PF__MSVC_64
        #define PF__MSVC_CLZ64(m_type)                              \
            if (sizeof(m_type) <= 8) {                              \
                unsigned long idx = 0;                              \
                return _BitScanReverse64(&idx, (unsigned __int64)x) \
                    ? (int)(PF__BITS(m_type) - 1u - idx)            \
                    : (int)PF__BITS(m_type);                        \
            }
        #define PF__MSVC_CTZ64(m_type)                              \
            if (sizeof(m_type) <= 8) {                              \
                unsigned long idx = 0;                              \
                return _BitScanForward64(&idx, (unsigned __int64)x) \
                    ? (int)idx                                      \
                    : (int)PF__BITS(m_type);                        \
            }
    #else
        #define PF__MSVC_CLZ64(m_type)
        #define PF__MSVC_CTZ64(m_type)
    #endif
    #define PF__MSVC_CLZ(m_type) PF__MSVC_CLZ32(m_type) PF__MSVC_CLZ64(m_type)
    #define PF__MSVC_CTZ(m_type) PF__MSVC_CTZ32(m_type) PF__MSVC_CTZ64(m_type)

    #if PF__MSVC_POPCNT
        #if PF__MSVC_64
            #define PF__MSVC_POPCNT64(m_type)                    \
                if (sizeof(m_type) <= 8)                         \
                    return (int)__popcnt64((unsigned __int64)x);
        #else
            #define PF__MSVC_POPCNT64(m_type)
        #endif
        #define PF__MSVC_POPCOUNT(m_type)              \
            if (sizeof(m_type) <= 4)                   \
                return (int)__popcnt((unsigned int)x); \
            PF__MSVC_POPCNT64(m_type)
        #define PF__MSVC_PARITY(m_type)                       \
            if (sizeof(m_type) <= 4)                          \
                return (int)(__popcnt((unsigned int)x) & 1u); \
            PF__MSVC_PARITY64(m_type)
        #if PF__MSVC_64
            #define PF__MSVC_PARITY64(m_type)                           \
                if (sizeof(m_type) <= 8)                                \
                    return (int)(__popcnt64((unsigned __int64)x) & 1u);
        #else
            #define PF__MSVC_PARITY64(m_type)
        #endif
    #else
        #define PF__MSVC_POPCOUNT(m_type)
        #define PF__MSVC_PARITY(m_type)
    #endif

    /* The rotate intrinsics take the count masked to the width. */
    #define PF__MSVC_ROTL(m_type)                                              \
        if (sizeof(m_type) == 1)                                               \
            return (m_type)_rotl8(                                             \
                (unsigned char)x, (unsigned char)(i & (PF__BITS(m_type) - 1u)) \
            );                                                                 \
        if (sizeof(m_type) == 2)                                               \
            return (m_type)_rotl16(                                            \
                (unsigned short)x,                                             \
                (unsigned char)(i & (PF__BITS(m_type) - 1u))                   \
            );                                                                 \
        if (sizeof(m_type) == 4)                                               \
            return (m_type)_rotl(                                              \
                (unsigned int)x, (int)(i & (PF__BITS(m_type) - 1u))            \
            );                                                                 \
        if (sizeof(m_type) == 8)                                               \
            return (m_type)_rotl64(                                            \
                (unsigned __int64)x, (int)(i & (PF__BITS(m_type) - 1u))        \
            );
    #define PF__MSVC_ROTR(m_type)                                              \
        if (sizeof(m_type) == 1)                                               \
            return (m_type)_rotr8(                                             \
                (unsigned char)x, (unsigned char)(i & (PF__BITS(m_type) - 1u)) \
            );                                                                 \
        if (sizeof(m_type) == 2)                                               \
            return (m_type)_rotr16(                                            \
                (unsigned short)x,                                             \
                (unsigned char)(i & (PF__BITS(m_type) - 1u))                   \
            );                                                                 \
        if (sizeof(m_type) == 4)                                               \
            return (m_type)_rotr(                                              \
                (unsigned int)x, (int)(i & (PF__BITS(m_type) - 1u))            \
            );                                                                 \
        if (sizeof(m_type) == 8)                                               \
            return (m_type)_rotr64(                                            \
                (unsigned __int64)x, (int)(i & (PF__BITS(m_type) - 1u))        \
            );
#else
    #define PF__MSVC_CLZ(m_type)
    #define PF__MSVC_CTZ(m_type)
    #define PF__MSVC_POPCOUNT(m_type)
    #define PF__MSVC_PARITY(m_type)
    #define PF__MSVC_ROTL(m_type)
    #define PF__MSVC_ROTR(m_type)
#endif

/* ---- Tier dispatch ------------------------------------------------------
 * Every PF__BODY_* is the complete body of a function taking `x` (and `i`
 * for rotations) and always returns. */

#ifdef PF__HAVE_STDBIT
    #define PF__BODY_CLZ(m_type, m_max) return (int)stdc_leading_zeros(x)
    #define PF__BODY_CTZ(m_type, m_max) return (int)stdc_trailing_zeros(x)
    #define PF__BODY_POW2CEIL_RAW(m_type) return (m_type)stdc_bit_ceil(x)
    #define PF__BODY_POW2FLOOR(m_type, m_max) return (m_type)stdc_bit_floor(x)
    #define PF__BODY_POPCOUNT(m_type, m_max) return (int)stdc_count_ones(x)
    #define PF__BODY_PARITY(m_type, m_max) return (int)(stdc_count_ones(x) & 1u)
    #define PF__BODY_ISPOW2(m_type, m_max) return (int)stdc_has_single_bit(x)
#else
    #if pf_has_builtin(__builtin_stdc_bit_ceil)
        #define PF__BODY_POW2CEIL_RAW(m_type)         \
            return (m_type)__builtin_stdc_bit_ceil(x)
    #else
        #define PF__BODY_POW2CEIL_RAW(m_type) PF__P_POW2CEIL(m_type)
    #endif

    #if pf_has_builtin(__builtin_stdc_bit_floor)
        #define PF__BODY_POW2FLOOR(m_type, m_max)      \
            return (m_type)__builtin_stdc_bit_floor(x)
    #else
        #define PF__BODY_POW2FLOOR(m_type, m_max) PF__P_POW2FLOOR(m_type)
    #endif

    #if pf_has_builtin(__builtin_stdc_has_single_bit)
        #define PF__BODY_ISPOW2(m_type, m_max)           \
            return (int)__builtin_stdc_has_single_bit(x)
    #else
        #define PF__BODY_ISPOW2(m_type, m_max)  \
            return x != 0 && (x & (x - 1)) == 0
    #endif

    #if pf_has_builtin(__builtin_ctzg)
        #define PF__BODY_CTZ(m_type, m_max)                      \
            return (int)__builtin_ctzg(x, (int)PF__BITS(m_type))
    #else
        #define PF__BODY_CTZ(m_type, m_max)                                \
            PF__CLASSIC_CTZ(m_type) PF__MSVC_CTZ(m_type) PF__P_CTZ(m_type)
    #endif

    #if pf_has_builtin(__builtin_clzg)
        #define PF__BODY_CLZ(m_type, m_max)                      \
            return (int)__builtin_clzg(x, (int)PF__BITS(m_type))
    #else
        #define PF__BODY_CLZ(m_type, m_max)                                \
            PF__CLASSIC_CLZ(m_type) PF__MSVC_CLZ(m_type) PF__P_CLZ(m_type)
    #endif

    #if pf_has_builtin(__builtin_popcountg)
        #define PF__BODY_POPCOUNT(m_type, m_max) \
            return (int)__builtin_popcountg(x)
    #else
        #define PF__BODY_POPCOUNT(m_type, m_max)             \
            PF__CLASSIC_POPCOUNT(m_type)                     \
            PF__MSVC_POPCOUNT(m_type) PF__P_POPCOUNT(m_type)
    #endif

    #if pf_has_builtin(__builtin_parityg)
        #define PF__BODY_PARITY(m_type, m_max) return (int)__builtin_parityg(x)
    #else
        #define PF__BODY_PARITY(m_type, m_max)           \
            PF__CLASSIC_PARITY(m_type)                   \
            PF__MSVC_PARITY(m_type) PF__P_PARITY(m_type)
    #endif
#endif

/* Guard shared by every backend: 0 and unrepresentable results give 0. */
#define PF__BODY_POW2CEIL(m_type, m_max)  \
    if (x == 0 || x > ((m_max) >> 1) + 1) \
        return 0;                         \
    PF__BODY_POW2CEIL_RAW(m_type)

#if pf_has_builtin(__builtin_stdc_rotate_right)
    #define PF__BODY_ROTR(m_type)                        \
        return (m_type)__builtin_stdc_rotate_right(x, i)
#else
    #define PF__BODY_ROTR(m_type) PF__MSVC_ROTR(m_type) PF__P_ROTR(m_type)
#endif

#if pf_has_builtin(__builtin_stdc_rotate_left)
    #define PF__BODY_ROTL(m_type)                       \
        return (m_type)__builtin_stdc_rotate_left(x, i)
#else
    #define PF__BODY_ROTL(m_type) PF__MSVC_ROTL(m_type) PF__P_ROTL(m_type)
#endif

#if CHAR_BIT == 8 && pf_has_builtin(__builtin_bitreverse8) \
    && pf_has_builtin(__builtin_bitreverse16)              \
    && pf_has_builtin(__builtin_bitreverse32)              \
    && pf_has_builtin(__builtin_bitreverse64)
    #define PF__BODY_BITREVERSE(m_type, m_max)                            \
        if (sizeof(m_type) == 1)                                          \
            return (m_type)__builtin_bitreverse8((unsigned char)x);       \
        if (sizeof(m_type) == 2)                                          \
            return (m_type)__builtin_bitreverse16((unsigned short)x);     \
        if (sizeof(m_type) == 4)                                          \
            return (m_type)__builtin_bitreverse32((unsigned int)x);       \
        if (sizeof(m_type) == 8)                                          \
            return (m_type)__builtin_bitreverse64((unsigned long long)x); \
        PF__P_BITREVERSE(m_type)
#else
    #define PF__BODY_BITREVERSE(m_type, m_max) PF__P_BITREVERSE(m_type)
#endif

/* ---- Function generators ---------------------------------------------- */

#define PF_IMPL_BITWISE_POW2(m_post, m_type, m_max)                            \
    PF_API m_type pf_pow2ceil##m_post(m_type x) {                              \
        PF__BODY_POW2CEIL(m_type, m_max);                                      \
    }                                                                          \
    PF_API m_type pf_pow2floor##m_post(m_type x) {                             \
        PF__BODY_POW2FLOOR(m_type, m_max);                                     \
    }                                                                          \
    PF_API int pf_ispow2##m_post(m_type x) { PF__BODY_ISPOW2(m_type, m_max); }

#define PF_IMPL_BITWISE_COUNT(m_post, m_type, m_max)                           \
    PF_API int pf_clz##m_post(m_type x) { PF__BODY_CLZ(m_type, m_max); }       \
    PF_API int pf_ctz##m_post(m_type x) { PF__BODY_CTZ(m_type, m_max); }       \
    PF_API int pf_clo##m_post(m_type x) { return pf_clz##m_post((m_type)~x); } \
    PF_API int pf_cto##m_post(m_type x) { return pf_ctz##m_post((m_type)~x); } \
    PF_API int pf_flz##m_post(m_type x) {                                      \
        return (m_type)~x ? pf_clz##m_post((m_type)~x) + 1 : 0;                \
    }                                                                          \
    PF_API int pf_flo##m_post(m_type x) {                                      \
        return x ? pf_clz##m_post(x) + 1 : 0;                                  \
    }                                                                          \
    PF_API int pf_ftz##m_post(m_type x) {                                      \
        return (m_type)~x ? pf_ctz##m_post((m_type)~x) + 1 : 0;                \
    }                                                                          \
    PF_API int pf_fto##m_post(m_type x) {                                      \
        return x ? pf_ctz##m_post(x) + 1 : 0;                                  \
    }                                                                          \
    PF_API int pf_popcount##m_post(m_type x) {                                 \
        PF__BODY_POPCOUNT(m_type, m_max);                                      \
    }                                                                          \
    PF_API int pf_parity##m_post(m_type x) { PF__BODY_PARITY(m_type, m_max); } \
    PF_API int pf_zerocount##m_post(m_type x) {                                \
        return pf_popcount##m_post((m_type)~x);                                \
    }

#define PF_IMPL_BITWISE_ROT(m_post, m_type, m_max)        \
    PF_API m_type pf_rotl##m_post(m_type x, unsigned i) { \
        PF__BODY_ROTL(m_type);                            \
    }                                                     \
    PF_API m_type pf_rotr##m_post(m_type x, unsigned i) { \
        PF__BODY_ROTR(m_type);                            \
    }

#define PF_IMPL_BITWISE_REV(m_post, m_type, m_max)  \
    PF_API m_type pf_bitreverse##m_post(m_type x) { \
        PF__BODY_BITREVERSE(m_type, m_max);         \
    }

#define PF_IMPL_BITWISE(m_post, m_type, m_max)   \
    PF_IMPL_BITWISE_POW2(m_post, m_type, m_max)  \
    PF_IMPL_BITWISE_COUNT(m_post, m_type, m_max) \
    PF_IMPL_BITWISE_ROT(m_post, m_type, m_max)   \
    PF_IMPL_BITWISE_REV(m_post, m_type, m_max)

#ifndef PF_BITWISE_SKIP_DEFAULT

/* clang-format off */
PF_IMPL_BITWISE(,     unsigned int,       UINT_MAX)
PF_IMPL_BITWISE(l,    unsigned long,      ULONG_MAX)
PF_IMPL_BITWISE(ll,   unsigned long long, ULLONG_MAX)
PF_IMPL_BITWISE(size, size_t,             SIZE_MAX)
PF_IMPL_BITWISE(max,  uintmax_t,          UINTMAX_MAX)
#ifdef UINTPTR_MAX
PF_IMPL_BITWISE(ptr,  uintptr_t,          UINTPTR_MAX)
#endif
#ifdef UINT8_MAX
PF_IMPL_BITWISE(8,    uint8_t,            UINT8_MAX)
#endif
#ifdef UINT16_MAX
PF_IMPL_BITWISE(16,   uint16_t,           UINT16_MAX)
#endif
#ifdef UINT32_MAX
PF_IMPL_BITWISE(32,   uint32_t,           UINT32_MAX)
#endif
#ifdef UINT64_MAX
PF_IMPL_BITWISE(64,   uint64_t,           UINT64_MAX)
#endif
    /* clang-format on */

    /* ---- Type-generic macros (C11) -----------------------------------------
 * Upper-case names on purpose: the lower-case pf_clz etc. are real functions
 * that must stay callable with any integer argument. Only the standard
 * unsigned types are listed, because uint8_t..uint64_t, size_t, uintptr_t
 * and uintmax_t are aliases of them and _Generic rejects duplicates. */

    #if !defined(PF_BITWISE_NO_GENERIC) && !defined(__cplusplus)    \
        && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L

        #define PF_HAVE_GENERIC 1

        #if defined(UINT8_MAX) && UCHAR_MAX == 0xFF
            #define PF__G8(m_op) , unsigned char : pf_##m_op##8
        #else
            #define PF__G8(m_op)
        #endif

        #if defined(UINT16_MAX) && USHRT_MAX == 0xFFFF
            #define PF__G16(m_op) , unsigned short : pf_##m_op##16
        #else
            #define PF__G16(m_op)
        #endif

        #define PF__GENERIC(m_op, m_x)                                       \
            _Generic(                                                        \
                (m_x),                                                       \
                unsigned int: pf_##m_op,                                     \
                unsigned long: pf_##m_op##l,                                 \
                unsigned long long: pf_##m_op##ll PF__G8(m_op) PF__G16(m_op) \
            )

        #define PF_CLZ(x) PF__GENERIC(clz, x)(x)
        #define PF_CTZ(x) PF__GENERIC(ctz, x)(x)
        #define PF_CLO(x) PF__GENERIC(clo, x)(x)
        #define PF_CTO(x) PF__GENERIC(cto, x)(x)
        #define PF_FLZ(x) PF__GENERIC(flz, x)(x)
        #define PF_FLO(x) PF__GENERIC(flo, x)(x)
        #define PF_FTZ(x) PF__GENERIC(ftz, x)(x)
        #define PF_FTO(x) PF__GENERIC(fto, x)(x)
        #define PF_ISPOW2(x) PF__GENERIC(ispow2, x)(x)
        #define PF_POW2CEIL(x) PF__GENERIC(pow2ceil, x)(x)
        #define PF_POW2FLOOR(x) PF__GENERIC(pow2floor, x)(x)
        #define PF_POPCOUNT(x) PF__GENERIC(popcount, x)(x)
        #define PF_ZEROCOUNT(x) PF__GENERIC(zerocount, x)(x)
        #define PF_PARITY(x) PF__GENERIC(parity, x)(x)
        #define PF_BITREVERSE(x) PF__GENERIC(bitreverse, x)(x)
        #define PF_ROTL(x, n) PF__GENERIC(rotl, x)(x, n)
        #define PF_ROTR(x, n) PF__GENERIC(rotr, x)(x, n)

    #endif /* generic */

#endif /* PF_BITWISE_SKIP_DEFAULT */

#if defined(__cplusplus)
}
#endif

#endif /* POLYFILL_BITWISE */