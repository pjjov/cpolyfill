/** cpolyfill - Filling the gaps between C standards and compilers

    Byte swapping, byte order conversion and unaligned loads and stores.
    The byte order of the target is PF_ENDIAN_LITTLE / PF_ENDIAN_BIG from
    <cpolyfill/platform.h>, usable in `#if`. Mixed-endian targets are not
    supported by this header.

        pf_bswap16(x), pf_bswap32(x), pf_bswap64(x)
                                Reverse the bytes of an integer.
        PF_BSWAP16(c), PF_BSWAP32(c), PF_BSWAP64(c)
                                The same as constant expressions, for
                                static initialisers and `case` labels. They
                                evaluate the argument more than once.

        pf_htobe16(x)  pf_htole16(x)  pf_be16toh(x)  pf_le16toh(x)
        pf_htobe32(x)  pf_htole32(x)  pf_be32toh(x)  pf_le32toh(x)
        pf_htobe64(x)  pf_htole64(x)  pf_be64toh(x)  pf_le64toh(x)
                                Convert between host and big-endian (be) or
                                little-endian (le) byte order.

        pf_load_le16(p)  pf_load_be16(p)  pf_load16(p)
        pf_load_le32(p)  pf_load_be32(p)  pf_load32(p)
        pf_load_le64(p)  pf_load_be64(p)  pf_load64(p)
                                Read an integer stored in that byte order
                                (none: host order) from `const void *p`,
                                which need not be aligned.
        pf_store_le16(p, v)  pf_store_be16(p, v)  pf_store16(p, v)
        ... and 32, 64          Write an integer in that byte order to
                                `void *p`, which need not be aligned.

        pf_memreverse(p, n)     Reverse the order of `n` bytes at `p`.

    The functions are `constexpr` in C++14 and later, except the loads,
    stores and pf_memreverse, and except with MSVC intrinsics.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_ENDIAN, where the
    system does not have them (Windows, macOS):
        `htobe16` ... `le64toh` of <endian.h> (Linux) and <sys/endian.h>
        (BSD), and `bswap_16`, `bswap_32` and `bswap_64` of <byteswap.h>.
        The system header is included when it exists.

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

#ifndef CPOLYFILL_ENDIAN_H
#define CPOLYFILL_ENDIAN_H

#include <cpolyfill/platform.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !PF_ENDIAN_LITTLE && !PF_ENDIAN_BIG
    #error "<cpolyfill/endian.h> does not support mixed-endian targets"
#endif

#if PF_CC_MSVC && !PF_CC_CLANG
    #include <stdlib.h>
    #define PF__BSWAP_MSVC 1
#else
    #define PF__BSWAP_MSVC 0
#endif

/* constexpr where the bodies allow it. */
#if PF_CPP >= 201402L && !PF__BSWAP_MSVC
    #define PF__ENDIAN_FN PF_INLINE constexpr
#else
    #define PF__ENDIAN_FN PF_INLINE
#endif

#define PF_BSWAP16(c)                                          \
    ((uint16_t)((((uint16_t)(c) & 0xFFu) << 8)                 \
                | (((uint16_t)(c) >> 8) & 0xFFu)))
#define PF_BSWAP32(c)                                          \
    ((uint32_t)((((uint32_t)(c) & UINT32_C(0x000000FF)) << 24) \
                | (((uint32_t)(c) & UINT32_C(0x0000FF00)) << 8) \
                | (((uint32_t)(c) & UINT32_C(0x00FF0000)) >> 8) \
                | (((uint32_t)(c) & UINT32_C(0xFF000000)) >> 24)))
#define PF_BSWAP64(c)                                                   \
    ((uint64_t)((((uint64_t)(c) & UINT64_C(0x00000000000000FF)) << 56)  \
                | (((uint64_t)(c) & UINT64_C(0x000000000000FF00)) << 40) \
                | (((uint64_t)(c) & UINT64_C(0x0000000000FF0000)) << 24) \
                | (((uint64_t)(c) & UINT64_C(0x00000000FF000000)) << 8)  \
                | (((uint64_t)(c) & UINT64_C(0x000000FF00000000)) >> 8)  \
                | (((uint64_t)(c) & UINT64_C(0x0000FF0000000000)) >> 24) \
                | (((uint64_t)(c) & UINT64_C(0x00FF000000000000)) >> 40) \
                | (((uint64_t)(c) & UINT64_C(0xFF00000000000000)) >> 56)))

PF_BEGIN_DECLS

PF__ENDIAN_FN uint16_t pf_bswap16(uint16_t x) {
#if PF_HAS_BUILTIN(__builtin_bswap16) || PF_GCC_PREREQ(4, 8)
    return __builtin_bswap16(x);
#elif PF__BSWAP_MSVC
    return (uint16_t)_byteswap_ushort((unsigned short)x);
#else
    return PF_BSWAP16(x);
#endif
}

PF__ENDIAN_FN uint32_t pf_bswap32(uint32_t x) {
#if PF_HAS_BUILTIN(__builtin_bswap32) || PF_GCC_PREREQ(4, 3)
    return __builtin_bswap32(x);
#elif PF__BSWAP_MSVC
    return (uint32_t)_byteswap_ulong((unsigned long)x);
#else
    return PF_BSWAP32(x);
#endif
}

PF__ENDIAN_FN uint64_t pf_bswap64(uint64_t x) {
#if PF_HAS_BUILTIN(__builtin_bswap64) || PF_GCC_PREREQ(4, 3)
    return __builtin_bswap64(x);
#elif PF__BSWAP_MSVC
    return (uint64_t)_byteswap_uint64((unsigned __int64)x);
#else
    return PF_BSWAP64(x);
#endif
}

#if PF_ENDIAN_BIG
    #define PF__ENDIAN_BE(bits, x) (x)
    #define PF__ENDIAN_LE(bits, x) pf_bswap##bits(x)
#else
    #define PF__ENDIAN_BE(bits, x) pf_bswap##bits(x)
    #define PF__ENDIAN_LE(bits, x) (x)
#endif

#define PF__ENDIAN_CONVERT(bits)                                       \
    PF__ENDIAN_FN uint##bits##_t pf_htobe##bits(uint##bits##_t x) {    \
        return PF__ENDIAN_BE(bits, x);                                 \
    }                                                                  \
    PF__ENDIAN_FN uint##bits##_t pf_be##bits##toh(uint##bits##_t x) {  \
        return PF__ENDIAN_BE(bits, x);                                 \
    }                                                                  \
    PF__ENDIAN_FN uint##bits##_t pf_htole##bits(uint##bits##_t x) {    \
        return PF__ENDIAN_LE(bits, x);                                 \
    }                                                                  \
    PF__ENDIAN_FN uint##bits##_t pf_le##bits##toh(uint##bits##_t x) {  \
        return PF__ENDIAN_LE(bits, x);                                 \
    }                                                                  \
    PF_INLINE uint##bits##_t pf_load##bits(const void *p) {            \
        uint##bits##_t x;                                              \
        memcpy(&x, p, sizeof(x));                                      \
        return x;                                                      \
    }                                                                  \
    PF_INLINE uint##bits##_t pf_load_be##bits(const void *p) {         \
        return pf_be##bits##toh(pf_load##bits(p));                     \
    }                                                                  \
    PF_INLINE uint##bits##_t pf_load_le##bits(const void *p) {         \
        return pf_le##bits##toh(pf_load##bits(p));                     \
    }                                                                  \
    PF_INLINE void pf_store##bits(void *p, uint##bits##_t x) {         \
        memcpy(p, &x, sizeof(x));                                      \
    }                                                                  \
    PF_INLINE void pf_store_be##bits(void *p, uint##bits##_t x) {      \
        pf_store##bits(p, pf_htobe##bits(x));                          \
    }                                                                  \
    PF_INLINE void pf_store_le##bits(void *p, uint##bits##_t x) {      \
        pf_store##bits(p, pf_htole##bits(x));                          \
    }

PF__ENDIAN_CONVERT(16)
PF__ENDIAN_CONVERT(32)
PF__ENDIAN_CONVERT(64)

PF_INLINE void pf_memreverse(void *p, size_t n) {
    unsigned char *lo = (unsigned char *)p, *hi = lo + n;
    unsigned char t;
    if (n < 2)
        return;
    for (hi--; lo < hi; lo++, hi--) {
        t = *lo;
        *lo = *hi;
        *hi = t;
    }
}

PF_END_DECLS

/* ---- Standard names --------------------------------------------------- */

#if (defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_ENDIAN)) \
    || defined(PF_STD_NAMES_ENDIAN)

    /* Include the system headers first: their definitions win, and their
       include guards keep them from redefining ours later.
       PF__FORCE_STD_NAMES_POLYFILL is for the tests only. */
    #if defined(__has_include) && !defined(PF__FORCE_STD_NAMES_POLYFILL)
        #if __has_include(<endian.h>)
            #include <endian.h>
        #elif __has_include(<sys/endian.h>)
            #include <sys/endian.h>
        #endif
        #if __has_include(<byteswap.h>)
            #include <byteswap.h>
        #endif
    #endif

    #ifndef htobe16
        #define htobe16(x) pf_htobe16(x)
        #define htole16(x) pf_htole16(x)
        #define be16toh(x) pf_be16toh(x)
        #define le16toh(x) pf_le16toh(x)
    #endif
    #ifndef htobe32
        #define htobe32(x) pf_htobe32(x)
        #define htole32(x) pf_htole32(x)
        #define be32toh(x) pf_be32toh(x)
        #define le32toh(x) pf_le32toh(x)
    #endif
    #ifndef htobe64
        #define htobe64(x) pf_htobe64(x)
        #define htole64(x) pf_htole64(x)
        #define be64toh(x) pf_be64toh(x)
        #define le64toh(x) pf_le64toh(x)
    #endif
    #ifndef bswap_16
        #define bswap_16(x) pf_bswap16(x)
    #endif
    #ifndef bswap_32
        #define bswap_32(x) pf_bswap32(x)
    #endif
    #ifndef bswap_64
        #define bswap_64(x) pf_bswap64(x)
    #endif

#endif

#endif /* CPOLYFILL_ENDIAN_H */
