/** cpolyfill - Filling the gaps between C standards and compilers

    Bit operations on unsigned integers, using compiler builtins where
    available.

        pf_clz*          Number of leading (most significant) 0 bits.
        pf_ctz*          Number of trailing (least significant) 0 bits.
        pf_clo*          Number of leading 1 bits.
        pf_cto*          Number of trailing 1 bits.
        pf_flz*          Position of the first leading 0 bit.
        pf_flo*          Position of the first leading 1 bit.
        pf_ftz*          Position of the first trailing 0 bit.
        pf_fto*          Position of the first trailing 1 bit.
        pf_popcount*     Number of 1 bits.
        pf_zerocount*    Number of 0 bits.
        pf_parity*       1 if the number of 1 bits is odd, else 0.
        pf_ispow2*       1 if the value is a power of 2, else 0.
        pf_pow2ceil*     Smallest power of 2 not less than the value.
        pf_pow2floor*    Largest power of 2 not greater than the value.
        pf_bitwidth*     Number of bits needed to represent the value.
        pf_bitreverse*   Reverses the order of the bits.
        pf_rotl*(x, n)   Rotates the bits left by `n` (unsigned).
        pf_rotr*(x, n)   Rotates the bits right by `n` (unsigned).

    The `*` is a suffix naming the argument type:

        (none) unsigned int    l  unsigned long    ll  unsigned long long
        8   uint8_t     16  uint16_t     32  uint32_t     64  uint64_t
        size  size_t    ptr  uintptr_t   max  uintmax_t

    The counting and position functions return `int`; pow2ceil, pow2floor,
    bitreverse, rotl and rotr return the argument type.

    Edge cases, the same with every implementation:

        clz/ctz(0), clo/cto(all ones)   the width of the type
        ftz/fto                         1-based position counted from the
                                        least significant bit (like ffs()),
                                        0 if there is no such bit
        flz/flo                         1-based position counted from the
                                        most significant bit, 0 if none
        ispow2(0), pow2floor(0)         0
        pow2ceil(x)                     0 if x is 0 or the result does not
                                        fit in the type
        bitwidth(0)                     0
        rotl/rotr                       the count is taken modulo the width

    Type-generic macros pick the function from the argument type:

        PF_CLZ(x)  PF_CTZ(x)  PF_CLO(x)  PF_CTO(x)  PF_FLZ(x)  PF_FLO(x)
        PF_FTZ(x)  PF_FTO(x)  PF_POPCOUNT(x)  PF_ZEROCOUNT(x)  PF_PARITY(x)
        PF_ISPOW2(x)  PF_POW2CEIL(x)  PF_POW2FLOOR(x)  PF_BITWIDTH(x)
        PF_BITREVERSE(x)  PF_ROTL(x, n)  PF_ROTR(x, n)

    They exist when PF_HAS_BITWISE_GENERIC is 1: in C11, in C with GCC or
    Clang, and in C++. The argument must be an unsigned char, short, int,
    long or long long. Signed types fail to compile, and so do promoted
    expressions (`a & b` with uint8_t operands is an int): cast them
    first, e.g. PF_CTZ((uint8_t)(a & b)) or PF_CTZ(8u). Value-returning
    macros return the type of the argument.

    Implementation, best first and per operation: C23 <stdbit.h>, the
    type-generic builtins of GCC and Clang (__builtin_ctzg, ...), their
    classic builtins, MSVC intrinsics, portable C. The functions are
    `constexpr` in C++14 and later, except with MSVC intrinsics.

    Configuration, defined before including:
        PF_BITWISE_PORTABLE     Use only the portable C implementation.
        PF_MSVC_POPCNT          Let MSVC use __popcnt on x86 and x64. Off
                                by default, because it faults on CPUs
                                without the POPCNT instruction. Implied by
                                /arch:AVX and above.

    PF_IMPL_BITWISE(suffix, type, max) defines the functions for another
    unsigned type, e.g. PF_IMPL_BITWISE(128, unsigned __int128, ...).

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_BITWISE (C only):
        When <stdbit.h> exists, it is included. Otherwise the C23 functions
        `stdc_leading_zeros_uc` ... `stdc_bit_ceil_ull` (suffixes `_uc`,
        `_us`, `_ui`, `_ul`, `_ull`), the type-generic `stdc_*` macros
        (where PF_HAS_BITWISE_GENERIC is 1) and `__STDC_ENDIAN_LITTLE__`,
        `__STDC_ENDIAN_BIG__` and `__STDC_ENDIAN_NATIVE__` are defined.
        They follow C23, not the functions above: they return `unsigned
        int` (`bool` for stdc_has_single_bit), and stdc_bit_ceil(0) is 1.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_BITWISE_H
#define CPOLYFILL_BITWISE_H

#include <cpolyfill/platform.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Implementation tiers --------------------------------------------- */

#ifdef PF_BITWISE_PORTABLE
    #define PF__BIT_GNU 0
    #define PF__BIT_MSVC 0
    #define PF__BIT_STDBIT 0
#else
    #if PF_CC_GNU || PF_CC_CLANG
        #define PF__BIT_GNU 1
    #else
        #define PF__BIT_GNU 0
    #endif
    #define PF__BIT_MSVC PF_CC_MSVC
    #if PF_STD_C23 && defined(__has_include) \
        && !defined(PF__FORCE_STD_NAMES_POLYFILL)
        #if __has_include(<stdbit.h>)
            #include <stdbit.h>
            #define PF__BIT_STDBIT 1
        #endif
    #endif
    #ifndef PF__BIT_STDBIT
        #define PF__BIT_STDBIT 0
    #endif
#endif

/* A builtin, only when builtins are in use. */
#if PF__BIT_GNU
    #define PF__BIT_BUILTIN(x) PF_HAS_BUILTIN(x)
#else
    #define PF__BIT_BUILTIN(x) 0
#endif

#if PF__BIT_MSVC
    #include <intrin.h>
    #if PF_ARCH_X86_64 || PF_ARCH_ARM64
        #define PF__BIT_MSVC64 1
    #else
        #define PF__BIT_MSVC64 0
    #endif
    #if (defined(PF_MSVC_POPCNT) || defined(__AVX__)) \
        && (PF_ARCH_X86 || PF_ARCH_X86_64)
        #define PF__BIT_POPCNT 1
    #else
        #define PF__BIT_POPCNT 0
    #endif
#endif

#if PF_CPP >= 201402L && !PF__BIT_MSVC
    #define PF__BIT_FN PF_INLINE constexpr
#else
    #define PF__BIT_FN PF_INLINE
#endif

/* Width of a type in bits. */
#define PF__BITS(type) ((unsigned)(sizeof(type) * CHAR_BIT))

/* ---- Portable bodies -------------------------------------------------- */

/* Every PF__P_* is a complete function body over `x` (and `n`). */

#define PF__P_CLZ(type)                                 \
    {                                                   \
        int r = (int)PF__BITS(type) - 1;                \
        unsigned s = 0;                                 \
        if (x == 0)                                     \
            return (int)PF__BITS(type);                 \
        for (s = PF__BITS(type) / 2; s != 0; s >>= 1) { \
            if ((x >> s) != 0) {                        \
                r -= (int)s;                            \
                x = (type)(x >> s);                     \
            }                                           \
        }                                               \
        return r;                                       \
    }

#define PF__P_CTZ(type)                 \
    {                                   \
        int r = 0;                      \
        if (x == 0)                     \
            return (int)PF__BITS(type); \
        while ((x & 1u) == 0) {         \
            x = (type)(x >> 1);         \
            r++;                        \
        }                               \
        return r;                       \
    }

#define PF__P_POPCOUNT(type)         \
    {                                \
        int r = 0;                   \
        for (; x != 0; r++)          \
            x = (type)(x & (x - 1)); \
        return r;                    \
    }

#define PF__P_PARITY(type)                   \
    {                                        \
        int r = 0;                           \
        for (; x != 0; x = (type)(x & (x - 1))) \
            r ^= 1;                          \
        return r;                            \
    }

#define PF__P_POW2FLOOR(type)                    \
    {                                            \
        unsigned s = 0;                          \
        for (s = 1; s < PF__BITS(type); s <<= 1) \
            x = (type)(x | (x >> s));            \
        return (type)(x - (x >> 1));             \
    }

/* Smears the bits of x - 1 down, then adds one. The caller has excluded 0
   and results that do not fit. */
#define PF__P_POW2CEIL(type)                     \
    {                                            \
        unsigned s = 0;                          \
        x = (type)(x - 1);                       \
        for (s = 1; s < PF__BITS(type); s <<= 1) \
            x = (type)(x | (x >> s));            \
        return (type)(x + 1);                    \
    }

/* Swaps halves, then quarters, ... down to single bits. */
#define PF__P_BITREVERSE(type)                                        \
    {                                                                 \
        type mask = (type) ~(type)0;                                  \
        unsigned s = PF__BITS(type);                                  \
        while ((s >>= 1) != 0) {                                      \
            mask = (type)(mask ^ (type)(mask << s));                  \
            x = (type)(((x >> s) & mask) | ((x << s) & (type)~mask)); \
        }                                                             \
        return x;                                                     \
    }

/* The count is reduced modulo the width, and so is the complementary
   shift, so a shift by the full width never happens. */
#define PF__P_ROTL(type)                                    \
    return (type)((x << (n & (PF__BITS(type) - 1u)))        \
                  | (x >> ((0u - n) & (PF__BITS(type) - 1u))))
#define PF__P_ROTR(type)                                    \
    return (type)((x >> (n & (PF__BITS(type) - 1u)))        \
                  | (x << ((0u - n) & (PF__BITS(type) - 1u))))

/* ---- Classic GCC and Clang builtins ----------------------------------- */

/* Each is a series of `if (...) return ...;` picked by the size of the
   type, falling through to the portable body for wider types. */
#if PF__BIT_GNU
    #define PF__G_CLZ(type)                                            \
        if (x == 0)                                                    \
            return (int)PF__BITS(type);                                \
        if (sizeof(type) <= sizeof(unsigned int))                      \
            return __builtin_clz((unsigned int)x)                      \
                - (int)(PF__BITS(unsigned int) - PF__BITS(type));      \
        if (sizeof(type) <= sizeof(unsigned long))                     \
            return __builtin_clzl((unsigned long)x)                    \
                - (int)(PF__BITS(unsigned long) - PF__BITS(type));     \
        if (sizeof(type) <= sizeof(unsigned long long))                \
            return __builtin_clzll((unsigned long long)x)              \
                - (int)(PF__BITS(unsigned long long) - PF__BITS(type));
    #define PF__G_CTZ(type)                                \
        if (x == 0)                                        \
            return (int)PF__BITS(type);                    \
        if (sizeof(type) <= sizeof(unsigned int))          \
            return __builtin_ctz((unsigned int)x);         \
        if (sizeof(type) <= sizeof(unsigned long))         \
            return __builtin_ctzl((unsigned long)x);       \
        if (sizeof(type) <= sizeof(unsigned long long))    \
            return __builtin_ctzll((unsigned long long)x);
    #define PF__G_POPCOUNT(type)                                \
        if (sizeof(type) <= sizeof(unsigned int))               \
            return __builtin_popcount((unsigned int)x);         \
        if (sizeof(type) <= sizeof(unsigned long))              \
            return __builtin_popcountl((unsigned long)x);       \
        if (sizeof(type) <= sizeof(unsigned long long))         \
            return __builtin_popcountll((unsigned long long)x);
    #define PF__G_PARITY(type)                                \
        if (sizeof(type) <= sizeof(unsigned int))             \
            return __builtin_parity((unsigned int)x);         \
        if (sizeof(type) <= sizeof(unsigned long))            \
            return __builtin_parityl((unsigned long)x);       \
        if (sizeof(type) <= sizeof(unsigned long long))       \
            return __builtin_parityll((unsigned long long)x);
#else
    #define PF__G_CLZ(type)
    #define PF__G_CTZ(type)
    #define PF__G_POPCOUNT(type)
    #define PF__G_PARITY(type)
#endif

/* ---- MSVC intrinsics -------------------------------------------------- */

#if PF__BIT_MSVC
    #define PF__M_CLZ32(type)                              \
        if (sizeof(type) <= 4) {                           \
            unsigned long i = 0;                           \
            return _BitScanReverse(&i, (unsigned long)x)   \
                ? (int)(PF__BITS(type) - 1u - i)           \
                : (int)PF__BITS(type);                     \
        }
    #define PF__M_CTZ32(type)                              \
        if (sizeof(type) <= 4) {                           \
            unsigned long i = 0;                           \
            return _BitScanForward(&i, (unsigned long)x)   \
                ? (int)i                                   \
                : (int)PF__BITS(type);                     \
        }
    #if PF__BIT_MSVC64
        #define PF__M_CLZ64(type)                                   \
            if (sizeof(type) <= 8) {                                \
                unsigned long i = 0;                                \
                return _BitScanReverse64(&i, (unsigned __int64)x)   \
                    ? (int)(PF__BITS(type) - 1u - i)                \
                    : (int)PF__BITS(type);                          \
            }
        #define PF__M_CTZ64(type)                                   \
            if (sizeof(type) <= 8) {                                \
                unsigned long i = 0;                                \
                return _BitScanForward64(&i, (unsigned __int64)x)   \
                    ? (int)i                                        \
                    : (int)PF__BITS(type);                          \
            }
    #else
        #define PF__M_CLZ64(type)
        #define PF__M_CTZ64(type)
    #endif
    #define PF__M_CLZ(type) PF__M_CLZ32(type) PF__M_CLZ64(type)
    #define PF__M_CTZ(type) PF__M_CTZ32(type) PF__M_CTZ64(type)

    #if PF__BIT_POPCNT && PF_ARCH_X86_64
        #define PF__M_POPCOUNT(type)                       \
            if (sizeof(type) <= 4)                         \
                return (int)__popcnt((unsigned int)x);     \
            if (sizeof(type) <= 8)                         \
                return (int)__popcnt64((unsigned __int64)x);
    #elif PF__BIT_POPCNT
        #define PF__M_POPCOUNT(type)                   \
            if (sizeof(type) <= 4)                     \
                return (int)__popcnt((unsigned int)x);
    #else
        #define PF__M_POPCOUNT(type)
    #endif

    /* The rotate intrinsics take the count masked to the width. */
    #define PF__M_ROT(type, dir)                                         \
        if (sizeof(type) == 1)                                           \
            return (type)_rot##dir##8(                                   \
                (unsigned char)x, (unsigned char)(n & 7u)                \
            );                                                           \
        if (sizeof(type) == 2)                                           \
            return (type)_rot##dir##16(                                  \
                (unsigned short)x, (unsigned char)(n & 15u)              \
            );                                                           \
        if (sizeof(type) == 4)                                           \
            return (type)_rot##dir((unsigned int)x, (int)(n & 31u));     \
        if (sizeof(type) == 8)                                           \
            return (type)_rot##dir##64((unsigned __int64)x, (int)(n & 63u));
    #define PF__M_ROTL(type) PF__M_ROT(type, l)
    #define PF__M_ROTR(type) PF__M_ROT(type, r)
#else
    #define PF__M_CLZ(type)
    #define PF__M_CTZ(type)
    #define PF__M_POPCOUNT(type)
    #define PF__M_ROTL(type)
    #define PF__M_ROTR(type)
#endif

/* ---- Bodies, by tier -------------------------------------------------- */

/* Every PF__B_* is a complete function body that always returns. */

#if PF__BIT_STDBIT
    #define PF__B_CLZ(type) return (int)stdc_leading_zeros(x);
    #define PF__B_CTZ(type) return (int)stdc_trailing_zeros(x);
    #define PF__B_POPCOUNT(type) return (int)stdc_count_ones(x);
    #define PF__B_PARITY(type) return (int)(stdc_count_ones(x) & 1u);
    #define PF__B_POW2FLOOR(type) return (type)stdc_bit_floor(x);
    #define PF__B_POW2CEIL(type) return (type)stdc_bit_ceil(x);
    #define PF__B_ISPOW2(type) return stdc_has_single_bit(x) ? 1 : 0;
#else
    #if PF__BIT_BUILTIN(__builtin_clzg)
        #define PF__B_CLZ(type)                                  \
            return (int)__builtin_clzg(x, (int)PF__BITS(type));
    #else
        #define PF__B_CLZ(type) PF__G_CLZ(type) PF__M_CLZ(type) PF__P_CLZ(type)
    #endif

    #if PF__BIT_BUILTIN(__builtin_ctzg)
        #define PF__B_CTZ(type)                                  \
            return (int)__builtin_ctzg(x, (int)PF__BITS(type));
    #else
        #define PF__B_CTZ(type) PF__G_CTZ(type) PF__M_CTZ(type) PF__P_CTZ(type)
    #endif

    #if PF__BIT_BUILTIN(__builtin_popcountg)
        #define PF__B_POPCOUNT(type) return (int)__builtin_popcountg(x);
    #else
        #define PF__B_POPCOUNT(type)                                 \
            PF__G_POPCOUNT(type) PF__M_POPCOUNT(type) PF__P_POPCOUNT(type)
    #endif

    #if PF__BIT_BUILTIN(__builtin_parityg)
        #define PF__B_PARITY(type) return (int)__builtin_parityg(x);
    #else
        #define PF__B_PARITY(type) PF__G_PARITY(type) PF__P_PARITY(type)
    #endif

    #if PF__BIT_BUILTIN(__builtin_stdc_bit_floor)
        #define PF__B_POW2FLOOR(type) return (type)__builtin_stdc_bit_floor(x);
    #else
        #define PF__B_POW2FLOOR(type) PF__P_POW2FLOOR(type)
    #endif

    #if PF__BIT_BUILTIN(__builtin_stdc_bit_ceil)
        #define PF__B_POW2CEIL(type) return (type)__builtin_stdc_bit_ceil(x);
    #else
        #define PF__B_POW2CEIL(type) PF__P_POW2CEIL(type)
    #endif

    #define PF__B_ISPOW2(type) return x != 0 && (x & (x - 1u)) == 0;
#endif

#if PF__BIT_BUILTIN(__builtin_stdc_rotate_left)
    #define PF__B_ROTL(type) return (type)__builtin_stdc_rotate_left(x, n);
    #define PF__B_ROTR(type) return (type)__builtin_stdc_rotate_right(x, n);
#else
    #define PF__B_ROTL(type) PF__M_ROTL(type) PF__P_ROTL(type);
    #define PF__B_ROTR(type) PF__M_ROTR(type) PF__P_ROTR(type);
#endif

#if CHAR_BIT == 8 && PF__BIT_BUILTIN(__builtin_bitreverse8) \
    && PF__BIT_BUILTIN(__builtin_bitreverse16)              \
    && PF__BIT_BUILTIN(__builtin_bitreverse32)              \
    && PF__BIT_BUILTIN(__builtin_bitreverse64)
    #define PF__B_BITREVERSE(type)                                        \
        if (sizeof(type) == 1)                                            \
            return (type)__builtin_bitreverse8((unsigned char)x);         \
        if (sizeof(type) == 2)                                            \
            return (type)__builtin_bitreverse16((unsigned short)x);       \
        if (sizeof(type) == 4)                                            \
            return (type)__builtin_bitreverse32((unsigned int)x);         \
        if (sizeof(type) == 8)                                            \
            return (type)__builtin_bitreverse64((unsigned long long)x);   \
        PF__P_BITREVERSE(type)
#else
    #define PF__B_BITREVERSE(type) PF__P_BITREVERSE(type)
#endif

/* ---- Function generator ----------------------------------------------- */

PF_BEGIN_DECLS

#define PF_IMPL_BITWISE(suffix, type, max)                                    \
    PF__BIT_FN int pf_clz##suffix(type x) { PF__B_CLZ(type) }                 \
    PF__BIT_FN int pf_ctz##suffix(type x) { PF__B_CTZ(type) }                 \
    PF__BIT_FN int pf_clo##suffix(type x) { return pf_clz##suffix((type)~x); } \
    PF__BIT_FN int pf_cto##suffix(type x) { return pf_ctz##suffix((type)~x); } \
    PF__BIT_FN int pf_flz##suffix(type x) {                                   \
        return (type)~x != 0 ? pf_clz##suffix((type)~x) + 1 : 0;              \
    }                                                                         \
    PF__BIT_FN int pf_flo##suffix(type x) {                                   \
        return x != 0 ? pf_clz##suffix(x) + 1 : 0;                            \
    }                                                                         \
    PF__BIT_FN int pf_ftz##suffix(type x) {                                   \
        return (type)~x != 0 ? pf_ctz##suffix((type)~x) + 1 : 0;              \
    }                                                                         \
    PF__BIT_FN int pf_fto##suffix(type x) {                                   \
        return x != 0 ? pf_ctz##suffix(x) + 1 : 0;                            \
    }                                                                         \
    PF__BIT_FN int pf_popcount##suffix(type x) { PF__B_POPCOUNT(type) }       \
    PF__BIT_FN int pf_zerocount##suffix(type x) {                             \
        return pf_popcount##suffix((type)~x);                                 \
    }                                                                         \
    PF__BIT_FN int pf_parity##suffix(type x) { PF__B_PARITY(type) }           \
    PF__BIT_FN int pf_ispow2##suffix(type x) { PF__B_ISPOW2(type) }           \
    PF__BIT_FN type pf_pow2floor##suffix(type x) { PF__B_POW2FLOOR(type) }    \
    PF__BIT_FN type pf__pow2ceil##suffix(type x) { PF__B_POW2CEIL(type) }     \
    PF__BIT_FN type pf_pow2ceil##suffix(type x) {                             \
        return x == 0 || x > ((max) >> 1) + 1 ? (type)0                       \
                                              : pf__pow2ceil##suffix(x);      \
    }                                                                         \
    PF__BIT_FN int pf_bitwidth##suffix(type x) {                              \
        return (int)PF__BITS(type) - pf_clz##suffix(x);                       \
    }                                                                         \
    PF__BIT_FN type pf_bitreverse##suffix(type x) { PF__B_BITREVERSE(type) }  \
    PF__BIT_FN type pf_rotl##suffix(type x, unsigned n) { PF__B_ROTL(type) }  \
    PF__BIT_FN type pf_rotr##suffix(type x, unsigned n) { PF__B_ROTR(type) }

/* clang-format off */
PF_IMPL_BITWISE(,     unsigned int,       UINT_MAX)
PF_IMPL_BITWISE(l,    unsigned long,      ULONG_MAX)
PF_IMPL_BITWISE(ll,   unsigned long long, ULLONG_MAX)
PF_IMPL_BITWISE(8,    uint8_t,            UINT8_MAX)
PF_IMPL_BITWISE(16,   uint16_t,           UINT16_MAX)
PF_IMPL_BITWISE(32,   uint32_t,           UINT32_MAX)
PF_IMPL_BITWISE(64,   uint64_t,           UINT64_MAX)
PF_IMPL_BITWISE(size, size_t,             SIZE_MAX)
PF_IMPL_BITWISE(ptr,  uintptr_t,          UINTPTR_MAX)
PF_IMPL_BITWISE(max,  uintmax_t,          UINTMAX_MAX)
/* clang-format on */

PF_END_DECLS

/* ---- Type-generic macros ---------------------------------------------- */

/* unsigned char and short map to the 8 and 16-bit functions. */
#if UCHAR_MAX == 0xFF && USHRT_MAX == 0xFFFF

    #if defined(__cplusplus)

        #define PF_HAS_BITWISE_GENERIC 1

        /* Overloads; a signed argument is ambiguous between them. */
        #define PF__BIT_OVERLOAD(op, uc, us, ui, ul, ull)                    \
            PF__BIT_FN uc pf__g_##op(unsigned char x) { return pf_##op##8(x); } \
            PF__BIT_FN us pf__g_##op(unsigned short x) {                     \
                return pf_##op##16(x);                                       \
            }                                                                \
            PF__BIT_FN ui pf__g_##op(unsigned int x) { return pf_##op(x); }  \
            PF__BIT_FN ul pf__g_##op(unsigned long x) { return pf_##op##l(x); } \
            PF__BIT_FN ull pf__g_##op(unsigned long long x) {                \
                return pf_##op##ll(x);                                       \
            }
        #define PF__BIT_OVERLOAD_INT(op) \
            PF__BIT_OVERLOAD(op, int, int, int, int, int)
        #define PF__BIT_OVERLOAD_SAME(op)                                    \
            PF__BIT_OVERLOAD(                                                \
                op, unsigned char, unsigned short, unsigned int,             \
                unsigned long, unsigned long long                            \
            )
        #define PF__BIT_OVERLOAD_ROT(op)                                     \
            PF__BIT_FN unsigned char pf__g_##op(unsigned char x, unsigned n) { \
                return pf_##op##8(x, n);                                     \
            }                                                                \
            PF__BIT_FN unsigned short pf__g_##op(unsigned short x, unsigned n) { \
                return pf_##op##16(x, n);                                    \
            }                                                                \
            PF__BIT_FN unsigned int pf__g_##op(unsigned int x, unsigned n) { \
                return pf_##op(x, n);                                        \
            }                                                                \
            PF__BIT_FN unsigned long pf__g_##op(unsigned long x, unsigned n) { \
                return pf_##op##l(x, n);                                     \
            }                                                                \
            PF__BIT_FN unsigned long long pf__g_##op(                        \
                unsigned long long x, unsigned n                             \
            ) {                                                              \
                return pf_##op##ll(x, n);                                    \
            }

extern "C++" {
PF__BIT_OVERLOAD_INT(clz)
PF__BIT_OVERLOAD_INT(ctz)
PF__BIT_OVERLOAD_INT(clo)
PF__BIT_OVERLOAD_INT(cto)
PF__BIT_OVERLOAD_INT(flz)
PF__BIT_OVERLOAD_INT(flo)
PF__BIT_OVERLOAD_INT(ftz)
PF__BIT_OVERLOAD_INT(fto)
PF__BIT_OVERLOAD_INT(popcount)
PF__BIT_OVERLOAD_INT(zerocount)
PF__BIT_OVERLOAD_INT(parity)
PF__BIT_OVERLOAD_INT(ispow2)
PF__BIT_OVERLOAD_INT(bitwidth)
PF__BIT_OVERLOAD_SAME(pow2ceil)
PF__BIT_OVERLOAD_SAME(pow2floor)
PF__BIT_OVERLOAD_SAME(bitreverse)
PF__BIT_OVERLOAD_ROT(rotl)
PF__BIT_OVERLOAD_ROT(rotr)
}

        #define PF__BIT_GENERIC(op, x) pf__g_##op

    #elif PF_STD_C11

        #define PF_HAS_BITWISE_GENERIC 1

        /* No association for signed types: a compile error. */
        #define PF__BIT_SELECT(x, uc, us, ui, ul, ull) \
            _Generic(                                  \
                (x),                                   \
                unsigned char: uc,                     \
                unsigned short: us,                    \
                unsigned int: ui,                      \
                unsigned long: ul,                     \
                unsigned long long: ull                \
            )

    #elif PF_CC_GNU || PF_CC_CLANG

        #define PF_HAS_BITWISE_GENERIC 1

/* Calling it with an argument does not compile, which is the point. */
extern void pf__bitwise_argument_must_be_unsigned(void);

        #define PF__BIT_IS(x, type) \
            __builtin_types_compatible_p(__typeof__(x), type)
        #define PF__BIT_SELECT(x, uc, us, ui, ul, ull)                 \
            __builtin_choose_expr(                                     \
                PF__BIT_IS(x, unsigned char), uc,                      \
            __builtin_choose_expr(                                     \
                PF__BIT_IS(x, unsigned short), us,                     \
            __builtin_choose_expr(                                     \
                PF__BIT_IS(x, unsigned int), ui,                       \
            __builtin_choose_expr(                                     \
                PF__BIT_IS(x, unsigned long), ul,                      \
            __builtin_choose_expr(                                     \
                PF__BIT_IS(x, unsigned long long), ull,                \
                pf__bitwise_argument_must_be_unsigned)))))

    #endif

    #ifndef __cplusplus
        #define PF__BIT_GENERIC(op, x)                                   \
            PF__BIT_SELECT(                                              \
                x, pf_##op##8, pf_##op##16, pf_##op, pf_##op##l, pf_##op##ll \
            )
    #endif
#endif

#ifndef PF_HAS_BITWISE_GENERIC
    #define PF_HAS_BITWISE_GENERIC 0
#else
    #define PF_CLZ(x) PF__BIT_GENERIC(clz, x)(x)
    #define PF_CTZ(x) PF__BIT_GENERIC(ctz, x)(x)
    #define PF_CLO(x) PF__BIT_GENERIC(clo, x)(x)
    #define PF_CTO(x) PF__BIT_GENERIC(cto, x)(x)
    #define PF_FLZ(x) PF__BIT_GENERIC(flz, x)(x)
    #define PF_FLO(x) PF__BIT_GENERIC(flo, x)(x)
    #define PF_FTZ(x) PF__BIT_GENERIC(ftz, x)(x)
    #define PF_FTO(x) PF__BIT_GENERIC(fto, x)(x)
    #define PF_POPCOUNT(x) PF__BIT_GENERIC(popcount, x)(x)
    #define PF_ZEROCOUNT(x) PF__BIT_GENERIC(zerocount, x)(x)
    #define PF_PARITY(x) PF__BIT_GENERIC(parity, x)(x)
    #define PF_ISPOW2(x) PF__BIT_GENERIC(ispow2, x)(x)
    #define PF_POW2CEIL(x) PF__BIT_GENERIC(pow2ceil, x)(x)
    #define PF_POW2FLOOR(x) PF__BIT_GENERIC(pow2floor, x)(x)
    #define PF_BITWIDTH(x) PF__BIT_GENERIC(bitwidth, x)(x)
    #define PF_BITREVERSE(x) PF__BIT_GENERIC(bitreverse, x)(x)
    #define PF_ROTL(x, n) PF__BIT_GENERIC(rotl, x)(x, n)
    #define PF_ROTR(x, n) PF__BIT_GENERIC(rotr, x)(x, n)
#endif

/* ---- Standard names --------------------------------------------------- */

#if ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_BITWISE)) \
     || defined(PF_STD_NAMES_BITWISE))                             \
    && !defined(__cplusplus)

    /* PF__FORCE_STD_NAMES_POLYFILL is for the tests only. */
    #if defined(__has_include) && !defined(PF__FORCE_STD_NAMES_POLYFILL)
        #if __has_include(<stdbit.h>)
            #include <stdbit.h>
            #define PF__BIT_HAVE_STDBIT 1
        #endif
    #endif

    #if !defined(PF__BIT_HAVE_STDBIT) && UCHAR_MAX == 0xFF \
        && USHRT_MAX == 0xFFFF

        #include <stdbool.h>

        #ifndef __STDC_ENDIAN_LITTLE__
            #define __STDC_ENDIAN_LITTLE__ 1234
        #endif
        #ifndef __STDC_ENDIAN_BIG__
            #define __STDC_ENDIAN_BIG__ 4321
        #endif
        #ifndef __STDC_ENDIAN_NATIVE__
            #if PF_ENDIAN_LITTLE
                #define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_LITTLE__
            #elif PF_ENDIAN_BIG
                #define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_BIG__
            #else
                #define __STDC_ENDIAN_NATIVE__ 3412
            #endif
        #endif

        #define PF__STDBIT_ONE(sfx, type, pfsfx)                              \
            PF_INLINE unsigned int stdc_leading_zeros##sfx(type x) {         \
                return (unsigned int)pf_clz##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_leading_ones##sfx(type x) {          \
                return (unsigned int)pf_clo##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_trailing_zeros##sfx(type x) {        \
                return (unsigned int)pf_ctz##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_trailing_ones##sfx(type x) {         \
                return (unsigned int)pf_cto##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_first_leading_zero##sfx(type x) {    \
                return (unsigned int)pf_flz##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_first_leading_one##sfx(type x) {     \
                return (unsigned int)pf_flo##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_first_trailing_zero##sfx(type x) {   \
                return (unsigned int)pf_ftz##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_first_trailing_one##sfx(type x) {    \
                return (unsigned int)pf_fto##pfsfx(x);                       \
            }                                                                \
            PF_INLINE unsigned int stdc_count_zeros##sfx(type x) {           \
                return (unsigned int)pf_zerocount##pfsfx(x);                 \
            }                                                                \
            PF_INLINE unsigned int stdc_count_ones##sfx(type x) {            \
                return (unsigned int)pf_popcount##pfsfx(x);                  \
            }                                                                \
            PF_INLINE bool stdc_has_single_bit##sfx(type x) {                \
                return pf_ispow2##pfsfx(x) != 0;                             \
            }                                                                \
            PF_INLINE unsigned int stdc_bit_width##sfx(type x) {             \
                return (unsigned int)pf_bitwidth##pfsfx(x);                  \
            }                                                                \
            PF_INLINE type stdc_bit_floor##sfx(type x) {                     \
                return (type)pf_pow2floor##pfsfx(x);                         \
            }                                                                \
            PF_INLINE type stdc_bit_ceil##sfx(type x) {                      \
                return x <= 1 ? (type)1 : (type)pf_pow2ceil##pfsfx(x);       \
            }

PF__STDBIT_ONE(_uc, unsigned char, 8)
PF__STDBIT_ONE(_us, unsigned short, 16)
PF__STDBIT_ONE(_ui, unsigned int, )
PF__STDBIT_ONE(_ul, unsigned long, l)
PF__STDBIT_ONE(_ull, unsigned long long, ll)

        #if PF_HAS_BITWISE_GENERIC
            #define PF__STDBIT_G(name, x) \
                PF__BIT_SELECT(x, name##_uc, name##_us, name##_ui, name##_ul, name##_ull)(x)
            #define stdc_leading_zeros(x) PF__STDBIT_G(stdc_leading_zeros, x)
            #define stdc_leading_ones(x) PF__STDBIT_G(stdc_leading_ones, x)
            #define stdc_trailing_zeros(x) PF__STDBIT_G(stdc_trailing_zeros, x)
            #define stdc_trailing_ones(x) PF__STDBIT_G(stdc_trailing_ones, x)
            #define stdc_first_leading_zero(x) PF__STDBIT_G(stdc_first_leading_zero, x)
            #define stdc_first_leading_one(x) PF__STDBIT_G(stdc_first_leading_one, x)
            #define stdc_first_trailing_zero(x) PF__STDBIT_G(stdc_first_trailing_zero, x)
            #define stdc_first_trailing_one(x) PF__STDBIT_G(stdc_first_trailing_one, x)
            #define stdc_count_zeros(x) PF__STDBIT_G(stdc_count_zeros, x)
            #define stdc_count_ones(x) PF__STDBIT_G(stdc_count_ones, x)
            #define stdc_has_single_bit(x) PF__STDBIT_G(stdc_has_single_bit, x)
            #define stdc_bit_width(x) PF__STDBIT_G(stdc_bit_width, x)
            #define stdc_bit_floor(x) PF__STDBIT_G(stdc_bit_floor, x)
            #define stdc_bit_ceil(x) PF__STDBIT_G(stdc_bit_ceil, x)
        #endif

    #endif
#endif

#endif /* CPOLYFILL_BITWISE_H */
