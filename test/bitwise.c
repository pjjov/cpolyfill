/*
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
*/

#include "../include/pf_assert.h"
#include "../include/pf_bitwise.h"
#include "../include/pf_test.h"
#include <limits.h>
#include <stdint.h>

#ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Woverflow"
#endif

#ifdef __clang__
    #pragma clang diagnostic ignored "-Woverflow"
#endif

#ifdef _MSC_VER
    #pragma warning(disable : 4307; disable : 4756)
#endif

/* All bits set, without the int promotion of ~(narrow type). */
#define ALL_ONES(m_type) ((m_type) ~(m_type)0)
#define WIDTH(m_type) ((unsigned)(sizeof(m_type) * CHAR_BIT))

int test_bitwise_clz(int seed, int rep) {
#define TEST_CLZ(m_type, m_fn)                                             \
    pf_assert((m_type)0 == pf_clz##m_fn((~(m_type)0)));                    \
    pf_assert((m_type)0 == pf_clz##m_fn((~(m_type)0x7F)));                 \
    pf_assert(sizeof(m_type) * 8 - 1 == pf_clz##m_fn((m_type)1));          \
    pf_assert(sizeof(m_type) * 8 - 2 == pf_clz##m_fn((m_type)2));          \
    pf_assert(sizeof(m_type) * 8 == pf_clz##m_fn((m_type)0));              \
    if (sizeof(m_type) > 2) {                                              \
        pf_assert(sizeof(m_type) * 8 - 4 == pf_clz##m_fn((m_type)15));     \
        pf_assert(16 == pf_clz##m_fn((~(m_type)0 >> 16)));                 \
        for (m_type i = 0; i < sizeof(m_type) * 8; i++) {                  \
            pf_assert(i == pf_clz##m_fn(~(m_type)0 >> i));                 \
            pf_assert(                                                     \
                sizeof(m_type) * 8 - i - 1 == pf_clz##m_fn((m_type)1 << i) \
            );                                                             \
        }                                                                  \
    }

    TEST_CLZ(unsigned int, );
    TEST_CLZ(unsigned long, l);
    TEST_CLZ(unsigned long long, ll);
    TEST_CLZ(size_t, size);
    TEST_CLZ(uintptr_t, ptr);
    TEST_CLZ(uintmax_t, max);
    TEST_CLZ(uint8_t, 8);
    TEST_CLZ(uint16_t, 16);
    TEST_CLZ(uint32_t, 32);
    TEST_CLZ(uint64_t, 64);
    return 0;
}

int test_bitwise_ctz(int seed, int rep) {
#define TEST_CTZ(m_type, m_fn)                                  \
    pf_assert((m_type)0 == pf_ctz##m_fn(~(m_type)0));           \
    pf_assert((m_type)0 == pf_ctz##m_fn((m_type)1));            \
    pf_assert((m_type)1 == pf_ctz##m_fn((m_type)2));            \
    pf_assert((m_type)0 == pf_ctz##m_fn((m_type)15));           \
    pf_assert((m_type)4 == pf_ctz##m_fn((m_type)16));           \
    pf_assert((m_type)7 == pf_ctz##m_fn(~(m_type)0x7F));        \
    pf_assert(sizeof(m_type) * 8 == pf_ctz##m_fn(0));           \
    for (m_type i = 0; i < sizeof(m_type) * 8; i++) {           \
        pf_assert(i == pf_ctz##m_fn((m_type) ~(m_type)0 << i)); \
        pf_assert(i == pf_ctz##m_fn((m_type)1 << i));           \
    }

    TEST_CTZ(unsigned int, );
    TEST_CTZ(unsigned long, l);
    TEST_CTZ(unsigned long long, ll);
    TEST_CTZ(size_t, size);
    TEST_CTZ(uintptr_t, ptr);
    TEST_CTZ(uintmax_t, max);
    TEST_CTZ(uint8_t, 8);
    TEST_CTZ(uint16_t, 16);
    TEST_CTZ(uint32_t, 32);
    TEST_CTZ(uint64_t, 64);
    return 0;
}

int test_bitwise_pow2ceil(int seed, int rep) {
#define TEST_POW2CEIL(m_type, m_fn)                                           \
    pf_assert((m_type)0 == pf_pow2ceil##m_fn((m_type)0));                     \
    pf_assert((m_type)1 == pf_pow2ceil##m_fn((m_type)1));                     \
    pf_assert((m_type)2 == pf_pow2ceil##m_fn((m_type)2));                     \
    pf_assert((m_type)4 == pf_pow2ceil##m_fn((m_type)3));                     \
    pf_assert((m_type)16 == pf_pow2ceil##m_fn((m_type)15));                   \
    for (m_type i = 2; i < sizeof(m_type) * 8 - 1; i++) {                     \
        pf_assert(((m_type)1 << i) == pf_pow2ceil##m_fn((m_type)1 << i));     \
        pf_assert(                                                            \
            ((m_type)1 << i) == pf_pow2ceil##m_fn(((m_type)1 << i) - 1)       \
        );                                                                    \
        pf_assert(                                                            \
            ((m_type)1 << (i + 1)) == pf_pow2ceil##m_fn(((m_type)1 << i) + 1) \
        );                                                                    \
    }

    TEST_POW2CEIL(unsigned int, );
    TEST_POW2CEIL(unsigned long, l);
    TEST_POW2CEIL(unsigned long long, ll);
    TEST_POW2CEIL(size_t, size);
    TEST_POW2CEIL(uintptr_t, ptr);
    TEST_POW2CEIL(uintmax_t, max);
    TEST_POW2CEIL(uint8_t, 8);
    TEST_POW2CEIL(uint16_t, 16);
    TEST_POW2CEIL(uint32_t, 32);
    TEST_POW2CEIL(uint64_t, 64);
    return 0;
}

int test_bitwise_pow2floor(int seed, int rep) {
#define TEST_POW2FLOOR(m_type, m_fn)                                           \
    pf_assert((m_type)0 == pf_pow2floor##m_fn((m_type)0));                     \
    pf_assert((m_type)1 == pf_pow2floor##m_fn((m_type)1));                     \
    pf_assert((m_type)2 == pf_pow2floor##m_fn((m_type)2));                     \
    pf_assert((m_type)2 == pf_pow2floor##m_fn((m_type)3));                     \
    pf_assert((m_type)8 == pf_pow2floor##m_fn((m_type)15));                    \
    for (m_type i = 2; i < sizeof(m_type) * 8 - 1; i++) {                      \
        pf_assert(((m_type)1 << i) == pf_pow2floor##m_fn((m_type)1 << i));     \
        pf_assert(                                                             \
            ((m_type)1 << (i - 1)) == pf_pow2floor##m_fn(((m_type)1 << i) - 1) \
        );                                                                     \
        pf_assert(                                                             \
            ((m_type)1 << i) == pf_pow2floor##m_fn(((m_type)1 << i) + 1)       \
        );                                                                     \
    }

    TEST_POW2FLOOR(unsigned int, );
    TEST_POW2FLOOR(unsigned long, l);
    TEST_POW2FLOOR(unsigned long long, ll);
    TEST_POW2FLOOR(size_t, size);
    TEST_POW2FLOOR(uintptr_t, ptr);
    TEST_POW2FLOOR(uintmax_t, max);
    TEST_POW2FLOOR(uint8_t, 8);
    TEST_POW2FLOOR(uint16_t, 16);
    TEST_POW2FLOOR(uint32_t, 32);
    TEST_POW2FLOOR(uint64_t, 64);
    return 0;
}

int test_bitwise_popcount(int seed, int rep) {
#define TEST_POPCOUNT(m_type, m_fn)                                        \
    pf_assert(0 == pf_popcount##m_fn((m_type)0));                          \
    pf_assert(1 == pf_popcount##m_fn((m_type)1));                          \
    pf_assert(1 == pf_popcount##m_fn((m_type)2));                          \
    pf_assert(4 == pf_popcount##m_fn((m_type)15));                         \
    pf_assert(8 == pf_popcount##m_fn((m_type)0xFF));                       \
    pf_assert(sizeof(m_type) * 8 == pf_popcount##m_fn((m_type)~0));        \
    pf_assert(sizeof(m_type) * 8 - 8 == pf_popcount##m_fn((m_type)~0xFF));

    TEST_POPCOUNT(unsigned int, );
    TEST_POPCOUNT(unsigned long, l);
    TEST_POPCOUNT(unsigned long long, ll);
    TEST_POPCOUNT(size_t, size);
    TEST_POPCOUNT(uintptr_t, ptr);
    TEST_POPCOUNT(uintmax_t, max);
    TEST_POPCOUNT(uint8_t, 8);
    TEST_POPCOUNT(uint16_t, 16);
    TEST_POPCOUNT(uint32_t, 32);
    TEST_POPCOUNT(uint64_t, 64);
    return 0;
}

int test_bitwise_parity(int seed, int rep) {
#define TEST_PARITY(m_type, m_fn)                    \
    pf_assert(0 == pf_parity##m_fn((m_type)0));      \
    pf_assert(1 == pf_parity##m_fn((m_type)1));      \
    pf_assert(1 == pf_parity##m_fn((m_type)2));      \
    pf_assert(0 == pf_parity##m_fn((m_type)3));      \
    pf_assert(0 == pf_parity##m_fn((m_type)15));     \
    pf_assert(1 == pf_parity##m_fn((m_type)16));     \
    pf_assert(0 == pf_parity##m_fn((m_type)0xFF));   \
    pf_assert(0 == pf_parity##m_fn((m_type)~0));     \
    pf_assert(1 == pf_parity##m_fn((m_type)~0 - 1));

    TEST_PARITY(unsigned int, );
    TEST_PARITY(unsigned long, l);
    TEST_PARITY(unsigned long long, ll);
    TEST_PARITY(size_t, size);
    TEST_PARITY(uintptr_t, ptr);
    TEST_PARITY(uintmax_t, max);
    TEST_PARITY(uint8_t, 8);
    TEST_PARITY(uint16_t, 16);
    TEST_PARITY(uint32_t, 32);
    TEST_PARITY(uint64_t, 64);
    return 0;
}

int test_bitwise_rotr(int seed, int rep) {
#define TEST_ROTR(m_type, m_fn)                                               \
    pf_assert((m_type)0 == pf_rotr##m_fn((m_type)0, 0));                      \
    pf_assert((m_type)0 == pf_rotr##m_fn((m_type)0, 1));                      \
    pf_assert((m_type)0 == pf_rotr##m_fn((m_type)0, sizeof(m_type) * 8));     \
    pf_assert((m_type)0 == pf_rotr##m_fn((m_type)0, sizeof(m_type) * 8 + 1)); \
    pf_assert((m_type)2 == pf_rotr##m_fn((m_type)2, 0));                      \
    pf_assert((m_type)1 == pf_rotr##m_fn((m_type)2, 1));                      \
    pf_assert((m_type)2 == pf_rotr##m_fn((m_type)2, sizeof(m_type) * 8));     \
    pf_assert((m_type)1 == pf_rotr##m_fn((m_type)2, sizeof(m_type) * 8 + 1)); \
    pf_assert((m_type)0xF0 == pf_rotr##m_fn((m_type)0xF0, 0));                \
    pf_assert((m_type)0x0F == pf_rotr##m_fn((m_type)0xF0, 4));                \
    pf_assert(                                                                \
        (m_type)0xF0 == pf_rotr##m_fn((m_type)0xF0, sizeof(m_type) * 8)       \
    );                                                                        \
    pf_assert(                                                                \
        (m_type)0x0F == pf_rotr##m_fn((m_type)0xF0, sizeof(m_type) * 8 + 4)   \
    );                                                                        \
    pf_assert((m_type)~0 == pf_rotr##m_fn((m_type)~0, 0));                    \
    pf_assert((m_type)~0 == pf_rotr##m_fn((m_type)~0, 1));                    \
    pf_assert((m_type)~0 == pf_rotr##m_fn((m_type)~0, sizeof(m_type) * 8));   \
    pf_assert((m_type)~0 == pf_rotr##m_fn((m_type)~0, sizeof(m_type) * 8 + 1));

    TEST_ROTR(unsigned int, );
    TEST_ROTR(unsigned long, l);
    TEST_ROTR(unsigned long long, ll);
    TEST_ROTR(size_t, size);
    TEST_ROTR(uintptr_t, ptr);
    TEST_ROTR(uintmax_t, max);
    TEST_ROTR(uint8_t, 8);
    TEST_ROTR(uint16_t, 16);
    TEST_ROTR(uint32_t, 32);
    TEST_ROTR(uint64_t, 64);
    return 0;
}

int test_bitwise_rotl(int seed, int rep) {
#define TEST_ROTL(m_type, m_fn)                                               \
    pf_assert((m_type)0 == pf_rotl##m_fn((m_type)0, 0));                      \
    pf_assert((m_type)0 == pf_rotl##m_fn((m_type)0, 1));                      \
    pf_assert((m_type)0 == pf_rotl##m_fn((m_type)0, sizeof(m_type) * 8));     \
    pf_assert((m_type)0 == pf_rotl##m_fn((m_type)0, sizeof(m_type) * 8 + 1)); \
    pf_assert((m_type)1 == pf_rotl##m_fn((m_type)1, 0));                      \
    pf_assert((m_type)2 == pf_rotl##m_fn((m_type)1, 1));                      \
    pf_assert((m_type)1 == pf_rotl##m_fn((m_type)1, sizeof(m_type) * 8));     \
    pf_assert((m_type)2 == pf_rotl##m_fn((m_type)1, sizeof(m_type) * 8 + 1)); \
    pf_assert((m_type)0x07 == pf_rotl##m_fn((m_type)0x07, 0));                \
    pf_assert((m_type)0x70 == pf_rotl##m_fn((m_type)0x07, 4));                \
    pf_assert(                                                                \
        (m_type)0x07 == pf_rotl##m_fn((m_type)0x07, sizeof(m_type) * 8)       \
    );                                                                        \
    pf_assert(                                                                \
        (m_type)0x70 == pf_rotl##m_fn((m_type)0x07, sizeof(m_type) * 8 + 4)   \
    );                                                                        \
    if (sizeof(m_type) > 2) {                                                 \
        pf_assert(~(m_type)0 == pf_rotl##m_fn(~(m_type)0, 0));                \
        pf_assert(~(m_type)0 == pf_rotl##m_fn(~(m_type)0, 1));                \
        pf_assert(                                                            \
            ~(m_type)0 == pf_rotl##m_fn(~(m_type)0, sizeof(m_type) * 8)       \
        );                                                                    \
        pf_assert(                                                            \
            ~(m_type)0 == pf_rotl##m_fn(~(m_type)0, sizeof(m_type) * 8 + 1)   \
        );                                                                    \
    }

    TEST_ROTL(unsigned int, );
    TEST_ROTL(unsigned long, l);
    TEST_ROTL(unsigned long long, ll);
    TEST_ROTL(size_t, size);
    TEST_ROTL(uintptr_t, ptr);
    TEST_ROTL(uintmax_t, max);
    TEST_ROTL(uint8_t, 8);
    TEST_ROTL(uint16_t, 16);
    TEST_ROTL(uint32_t, 32);
    TEST_ROTL(uint64_t, 64);

    pf_assert(0xFF == pf_rotl8(0xFF, 0));
    pf_assert(0xFF == pf_rotl8(0xFF, 1));
    pf_assert(0xFF == pf_rotl8(0xFF, 8));
    pf_assert(0xFF == pf_rotl8(0xFF, 9));
    pf_assert(0xFFFF == pf_rotl16(0xFFFF, 0));
    pf_assert(0xFFFF == pf_rotl16(0xFFFF, 1));
    pf_assert(0xFFFF == pf_rotl16(0xFFFF, 16));
    pf_assert(0xFFFF == pf_rotl16(0xFFFF, 17));
    return 0;
}

int test_bitwise_clo_cto(int seed, int rep) {
#define TEST_CLO_CTO(m_type, m_fn)                                             \
    pf_assert(0 == pf_clo##m_fn((m_type)0));                                   \
    pf_assert(0 == pf_cto##m_fn((m_type)0));                                   \
    pf_assert(WIDTH(m_type) == (unsigned)pf_clo##m_fn(ALL_ONES(m_type)));      \
    pf_assert(WIDTH(m_type) == (unsigned)pf_cto##m_fn(ALL_ONES(m_type)));      \
    pf_assert(3 == pf_cto##m_fn((m_type)7));                                   \
    pf_assert(0 == pf_cto##m_fn((m_type)2));                                   \
    for (unsigned i = 1; i <= WIDTH(m_type); i++) {                            \
        /* top i bits set / low i bits set */                                  \
        pf_assert(                                                             \
            (int)i                                                             \
            == pf_clo##m_fn((m_type)(ALL_ONES(m_type) << (WIDTH(m_type) - i))) \
        );                                                                     \
        pf_assert(                                                             \
            (int)i                                                             \
            == pf_cto##m_fn((m_type)(ALL_ONES(m_type) >> (WIDTH(m_type) - i))) \
        );                                                                     \
    }

    TEST_CLO_CTO(unsigned int, );
    TEST_CLO_CTO(unsigned long, l);
    TEST_CLO_CTO(unsigned long long, ll);
    TEST_CLO_CTO(size_t, size);
    TEST_CLO_CTO(uintptr_t, ptr);
    TEST_CLO_CTO(uintmax_t, max);
    TEST_CLO_CTO(uint8_t, 8);
    TEST_CLO_CTO(uint16_t, 16);
    TEST_CLO_CTO(uint32_t, 32);
    TEST_CLO_CTO(uint64_t, 64);
    return 0;
}

/* 1-based positions; 0 when there is no such bit (like ffs). */
int test_bitwise_find(int seed, int rep) {
#define TEST_FIND(m_type, m_fn)                                            \
    pf_assert(0 == pf_flo##m_fn((m_type)0));                               \
    pf_assert(0 == pf_fto##m_fn((m_type)0));                               \
    pf_assert(0 == pf_flz##m_fn(ALL_ONES(m_type)));                        \
    pf_assert(0 == pf_ftz##m_fn(ALL_ONES(m_type)));                        \
    pf_assert(1 == pf_ftz##m_fn((m_type)0));                               \
    pf_assert(1 == pf_fto##m_fn((m_type)1));                               \
    pf_assert(3 == pf_fto##m_fn((m_type)4));                               \
    pf_assert(3 == pf_ftz##m_fn((m_type)3));                               \
    for (unsigned b = 0; b < WIDTH(m_type); b++) {                         \
        m_type bit = (m_type)((m_type)1 << b);                             \
        pf_assert((int)(WIDTH(m_type) - b) == pf_flo##m_fn(bit));          \
        pf_assert((int)(b + 1) == pf_fto##m_fn(bit));                      \
        pf_assert((int)(WIDTH(m_type) - b) == pf_flz##m_fn((m_type)~bit)); \
        pf_assert((int)(b + 1) == pf_ftz##m_fn((m_type)~bit));             \
    }

    TEST_FIND(unsigned int, );
    TEST_FIND(unsigned long, l);
    TEST_FIND(unsigned long long, ll);
    TEST_FIND(size_t, size);
    TEST_FIND(uintptr_t, ptr);
    TEST_FIND(uintmax_t, max);
    TEST_FIND(uint8_t, 8);
    TEST_FIND(uint16_t, 16);
    TEST_FIND(uint32_t, 32);
    TEST_FIND(uint64_t, 64);
    return 0;
}

int test_bitwise_zerocount(int seed, int rep) {
#define TEST_ZEROCOUNT(m_type, m_fn)                                         \
    pf_assert(WIDTH(m_type) == (unsigned)pf_zerocount##m_fn((m_type)0));     \
    pf_assert(0 == pf_zerocount##m_fn(ALL_ONES(m_type)));                    \
    pf_assert(WIDTH(m_type) - 1 == (unsigned)pf_zerocount##m_fn((m_type)1)); \
    pf_assert(WIDTH(m_type) - 4 == (unsigned)pf_zerocount##m_fn((m_type)0xF0));

    TEST_ZEROCOUNT(unsigned int, );
    TEST_ZEROCOUNT(unsigned long, l);
    TEST_ZEROCOUNT(unsigned long long, ll);
    TEST_ZEROCOUNT(size_t, size);
    TEST_ZEROCOUNT(uintptr_t, ptr);
    TEST_ZEROCOUNT(uintmax_t, max);
    TEST_ZEROCOUNT(uint8_t, 8);
    TEST_ZEROCOUNT(uint16_t, 16);
    TEST_ZEROCOUNT(uint32_t, 32);
    TEST_ZEROCOUNT(uint64_t, 64);
    return 0;
}

int test_bitwise_ispow2(int seed, int rep) {
#define TEST_ISPOW2(m_type, m_fn)                             \
    pf_assert(0 == pf_ispow2##m_fn((m_type)0));               \
    pf_assert(1 == pf_ispow2##m_fn((m_type)1));               \
    pf_assert(1 == pf_ispow2##m_fn((m_type)2));               \
    pf_assert(0 == pf_ispow2##m_fn((m_type)3));               \
    pf_assert(1 == pf_ispow2##m_fn((m_type)4));               \
    pf_assert(0 == pf_ispow2##m_fn((m_type)5));               \
    pf_assert(0 == pf_ispow2##m_fn((m_type)6));               \
    pf_assert(0 == pf_ispow2##m_fn((m_type)7));               \
    pf_assert(0 == pf_ispow2##m_fn(ALL_ONES(m_type)));        \
    for (unsigned b = 0; b < WIDTH(m_type); b++) {            \
        m_type p = (m_type)((m_type)1 << b);                  \
        pf_assert(1 == pf_ispow2##m_fn(p));                   \
        if (b > 1) {                                          \
            pf_assert(0 == pf_ispow2##m_fn((m_type)(p + 1))); \
            pf_assert(0 == pf_ispow2##m_fn((m_type)(p - 1))); \
        }                                                     \
    }

    TEST_ISPOW2(unsigned int, );
    TEST_ISPOW2(unsigned long, l);
    TEST_ISPOW2(unsigned long long, ll);
    TEST_ISPOW2(size_t, size);
    TEST_ISPOW2(uintptr_t, ptr);
    TEST_ISPOW2(uintmax_t, max);
    TEST_ISPOW2(uint8_t, 8);
    TEST_ISPOW2(uint16_t, 16);
    TEST_ISPOW2(uint32_t, 32);
    TEST_ISPOW2(uint64_t, 64);
    return 0;
}

/* pow2ceil returns 0 when the result does not fit. */
int test_bitwise_pow2ceil_overflow(int seed, int rep) {
#define TEST_POW2CEIL_OVF(m_type, m_fn)                               \
    {                                                                 \
        m_type top = (m_type)((m_type)1 << (WIDTH(m_type) - 1));      \
        pf_assert(top == pf_pow2ceil##m_fn(top));                     \
        pf_assert(top == pf_pow2ceil##m_fn((m_type)(top - 1)));       \
        pf_assert((m_type)0 == pf_pow2ceil##m_fn((m_type)(top + 1))); \
        pf_assert((m_type)0 == pf_pow2ceil##m_fn(ALL_ONES(m_type)));  \
    }

    TEST_POW2CEIL_OVF(unsigned int, );
    TEST_POW2CEIL_OVF(unsigned long, l);
    TEST_POW2CEIL_OVF(unsigned long long, ll);
    TEST_POW2CEIL_OVF(size_t, size);
    TEST_POW2CEIL_OVF(uintptr_t, ptr);
    TEST_POW2CEIL_OVF(uintmax_t, max);
    TEST_POW2CEIL_OVF(uint8_t, 8);
    TEST_POW2CEIL_OVF(uint16_t, 16);
    TEST_POW2CEIL_OVF(uint32_t, 32);
    TEST_POW2CEIL_OVF(uint64_t, 64);
    return 0;
}

int test_bitwise_bitreverse(int seed, int rep) {
#define TEST_BITREVERSE(m_type, m_fn)                                      \
    pf_assert((m_type)0 == pf_bitreverse##m_fn((m_type)0));                \
    pf_assert(ALL_ONES(m_type) == pf_bitreverse##m_fn(ALL_ONES(m_type)));  \
    pf_assert(                                                             \
        (m_type)((m_type)1 << (WIDTH(m_type) - 1))                         \
        == pf_bitreverse##m_fn((m_type)1)                                  \
    );                                                                     \
    pf_assert(                                                             \
        (m_type)0x01                                                       \
        == pf_bitreverse##m_fn((m_type)((m_type)1 << (WIDTH(m_type) - 1))) \
    );                                                                     \
    /* 0x0F in the low byte becomes 0xF0 in the top byte */                \
    pf_assert(                                                             \
        (m_type)((m_type)0xF0 << (WIDTH(m_type) - 8))                      \
        == pf_bitreverse##m_fn((m_type)0x0F)                               \
    );                                                                     \
    for (unsigned b = 0; b < WIDTH(m_type); b++) {                         \
        m_type bit = (m_type)((m_type)1 << b);                             \
        m_type rev = (m_type)((m_type)1 << (WIDTH(m_type) - 1 - b));       \
        pf_assert(rev == pf_bitreverse##m_fn(bit));                        \
        pf_assert(bit == pf_bitreverse##m_fn(rev));                        \
        pf_assert((m_type)~rev == pf_bitreverse##m_fn((m_type)~bit));      \
    }                                                                      \
    {                                                                      \
        m_type v = (m_type)0x1234ABCDu;                                    \
        pf_assert(v == pf_bitreverse##m_fn(pf_bitreverse##m_fn(v)));       \
    }

    TEST_BITREVERSE(unsigned int, );
    TEST_BITREVERSE(unsigned long, l);
    TEST_BITREVERSE(unsigned long long, ll);
    TEST_BITREVERSE(size_t, size);
    TEST_BITREVERSE(uintptr_t, ptr);
    TEST_BITREVERSE(uintmax_t, max);
    TEST_BITREVERSE(uint8_t, 8);
    TEST_BITREVERSE(uint16_t, 16);
    TEST_BITREVERSE(uint32_t, 32);
    TEST_BITREVERSE(uint64_t, 64);

    pf_assert(0x80 == pf_bitreverse8(0x01));
    pf_assert(0x0F == pf_bitreverse8(0xF0));
    pf_assert(0x5555 == pf_bitreverse16(0xAAAA));
    pf_assert(0x80000000u == pf_bitreverse32(1u));
    pf_assert(0x1E6A2C48u == pf_bitreverse32(0x12345678u));
    return 0;
}

#ifdef PF_HAVE_GENERIC
/* The type-generic macros must pick the right width and keep the type. */
int test_bitwise_generic(int seed, int rep) {
    pf_assert(31 == PF_CLZ(1u));
    pf_assert(63 == PF_CLZ(1ull));
    pf_assert(7 == PF_CLZ((uint8_t)1));
    pf_assert(15 == PF_CLZ((uint16_t)1));
    pf_assert(3 == PF_CTZ(8ul));
    pf_assert(3 == PF_CTZ((uint64_t)8));
    pf_assert(8 == PF_POPCOUNT(0xFFul));
    pf_assert(8 == PF_POPCOUNT((uint8_t)0xFF));
    pf_assert(0 == PF_PARITY(3u));
    pf_assert(1 == PF_PARITY((uint16_t)1));
    pf_assert(3 == PF_CTO(7u));
    pf_assert(3 == PF_CLO((uint8_t)0xE0));
    pf_assert(1 == PF_FTO(1u));
    pf_assert(1 == PF_FLO((uint8_t)0x80));
    pf_assert(1 == PF_FTZ(0u));
    pf_assert(1 == PF_FLZ((uint8_t)0x7F));
    pf_assert(1 == PF_ISPOW2(64ull));
    pf_assert(0 == PF_ISPOW2((uint8_t)6));
    pf_assert(7 == PF_ZEROCOUNT((uint8_t)1));

    /* Result type == argument type (no silent widening). */
    pf_assert(sizeof(PF_POW2CEIL((uint8_t)3)) == 1);
    pf_assert(sizeof(PF_POW2FLOOR((uint16_t)3)) == 2);
    pf_assert(sizeof(PF_ROTL((uint8_t)1, 1)) == 1);
    pf_assert(sizeof(PF_BITREVERSE((uint64_t)1)) == sizeof(uint64_t));

    pf_assert(4 == PF_POW2CEIL((uint8_t)3));
    pf_assert(2 == PF_POW2FLOOR((uint16_t)3));
    pf_assert(0x80 == PF_ROTR((uint8_t)1, 1));
    pf_assert(0x0003 == PF_ROTL((uint16_t)0x8001, 1));
    pf_assert(0x80000000u == PF_BITREVERSE(1u));
    pf_assert(0x80 == PF_BITREVERSE((uint8_t)1));
    pf_assert(0x8000000000000000ull == PF_ROTR(1ull, 1));
    return 0;
}
#endif

pf_test_t suite_bitwise[] = {
    { test_bitwise_clz, "/bitwise/clz" },
    { test_bitwise_ctz, "/bitwise/ctz" },
    { test_bitwise_pow2ceil, "/bitwise/pow2ceil" },
    { test_bitwise_pow2floor, "/bitwise/pow2floor" },
    { test_bitwise_popcount, "/bitwise/popcount" },
    { test_bitwise_parity, "/bitwise/parity" },
    { test_bitwise_rotr, "/bitwise/rotr" },
    { test_bitwise_rotl, "/bitwise/rotl" },
    { test_bitwise_clo_cto, "/bitwise/clo_cto" },
    { test_bitwise_find, "/bitwise/find" },
    { test_bitwise_zerocount, "/bitwise/zerocount" },
    { test_bitwise_ispow2, "/bitwise/ispow2" },
    { test_bitwise_pow2ceil_overflow, "/bitwise/pow2ceil_overflow" },
    { test_bitwise_bitreverse, "/bitwise/bitreverse" },
#ifdef PF_HAVE_GENERIC
    { test_bitwise_generic, "/bitwise/generic" },
#endif
    { 0 },
};
