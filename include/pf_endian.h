/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides byte swapping and endianness conversion functions
    similar to those from '<byteswap.h>' and '<endian.h>' headers in Linux.

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

#ifndef POLYFILL_ENDIAN
#define POLYFILL_ENDIAN

#ifndef __has_builtin
    #define __has_builtin(...) 0
#endif

#ifndef __has_include
    #define __has_include(...) 0
#endif

#if __has_include(<byteswap.h>) || defined(__GNUC__)
    #include <byteswap.h>
#endif

#ifdef _MSC_VER
    #include <stdlib.h>
#endif

#include <limits.h>
#include <stdint.h>
#include <string.h>

static inline uint16_t pf_bswap16(uint16_t x) {
#if __has_include(<byteswap.h>) || defined(__GNUC__)
    return bswap_16(x);
#elif __has_builtin(__builtin_bswap16)
    return __builtin_bswap16(x);
#elif defined(_MSC_VER) && (USHRT_MAX == UINT16_MAX)
    return (uint32_t)_byteswap_ushort((unsigned short)x);
#else
    return ((x & 0xff00u) >> 8) | ((x & 0x00ffu) << 8);
#endif
}

static inline uint32_t pf_bswap32(uint32_t x) {
#if __has_include(<byteswap.h>) || defined(__GNUC__)
    return bswap_32(x);
#elif __has_builtin(__builtin_bswap32)
    return __builtin_bswap32(x);
#elif defined(_MSC_VER) && (ULONG_MAX == UINT32_MAX)
    return (uint32_t)_byteswap_ulong((unsigned long)x);
#else
    return ((x & 0xff000000u) >> 24) | ((x & 0x00ff0000u) >> 8)
        | ((x & 0x0000ff00u) << 8) | ((x & 0x000000ffu) << 24);
#endif
}

static inline uint64_t pf_bswap64(uint64_t x) {
#if __has_include(<byteswap.h>) || defined(__GNUC__)
    return bswap_64(x);
#elif __has_builtin(__builtin_bswap64)
    return __builtin_bswap64(x);
#elif defined(_MSC_VER)
    return (uint64_t)_byteswap_uint64((unsigned __int64)x);
#else
    return ((x & 0xff00000000000000ull) >> 56)
        | ((x & 0x00ff000000000000ull) >> 40)
        | ((x & 0x0000ff0000000000ull) >> 24)
        | ((x & 0x000000ff00000000ull) >> 8)
        | ((x & 0x00000000ff000000ull) << 8)
        | ((x & 0x0000000000ff0000ull) << 24)
        | ((x & 0x000000000000ff00ull) << 40)
        | ((x & 0x00000000000000ffull) << 56)
#endif
}

union pf__endian {
    int bytes;
    char little;
};

#ifdef _MSC_VER
    #define PF_BIG_ENDIAN (0)
    #define PF_LITTLE_ENDIAN (1)
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define PF_BIG_ENDIAN (0)
    #define PF_LITTLE_ENDIAN (1)
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define PF_BIG_ENDIAN (1)
    #define PF_LITTLE_ENDIAN (0)
#else
    #define PF_BIG_ENDIAN (!((union pf__endian) { .bytes = 1 }).little)
    #define PF_LITTLE_ENDIAN (((union pf__endian) { .bytes = 1 }).little)
#endif

#if __has_include(<endian.h>)
    #include <endian.h>
#else

    #define PF__IMPL(name, size, check)                       \
        static inline uint##size##_t name(uint##size##_t x) { \
            return (check) ? pf_bswap##size(x) : x;           \
        }

    #define PF__IMPL_BE(name, size) PF__IMPL(name, size, PF_BIG_ENDIAN)
    #define PF__IMPL_LE(name, size) PF__IMPL(name, size, !PF_BIG_ENDIAN)

PF__IMPL_BE(htobe16, 16)
PF__IMPL_BE(be16toh, 16)
PF__IMPL_LE(htole16, 16)
PF__IMPL_LE(le16toh, 16)

PF__IMPL_BE(htobe32, 32)
PF__IMPL_BE(be32toh, 32)
PF__IMPL_LE(htole32, 32)
PF__IMPL_LE(le32toh, 32)

PF__IMPL_BE(htobe64, 64)
PF__IMPL_BE(be64toh, 64)
PF__IMPL_LE(htole64, 64)
PF__IMPL_LE(le64toh, 64)

    #undef PF__IMPL_LE
    #undef PF__IMPL_BE
    #undef PF__IMPL

#endif

#define PF__IMPL_X(m_name, m_check, m_f16, m_f32, m_f64)                   \
    void m_name(void *out, const void *in, size_t size) {                  \
        if (size == 2)                                                     \
            *(uint16_t *)out = m_f16(*(const uint16_t *)in);               \
        if (size == 4)                                                     \
            *(uint32_t *)out = m_f32(*(const uint32_t *)in);               \
        if (size == 8)                                                     \
            *(uint64_t *)out = m_f64(*(const uint64_t *)in);               \
                                                                           \
        if ((m_check)) {                                                   \
            memcpy(out, in, size);                                         \
        } else {                                                           \
            for (size_t i = 0; i < size; i++)                              \
                ((uint8_t *)out)[i] = ((const uint8_t *)in)[size - i - 1]; \
        }                                                                  \
    }

PF__IMPL_X(htobex, PF_BIG_ENDIAN, htobe16, htobe32, htobe64);
PF__IMPL_X(htolex, !PF_BIG_ENDIAN, htole16, htole32, htole64);
PF__IMPL_X(bextoh, PF_BIG_ENDIAN, be16toh, be32toh, be64toh);
PF__IMPL_X(lextoh, !PF_BIG_ENDIAN, be16toh, be32toh, be64toh);

#undef PF__IMPL_X

#endif
