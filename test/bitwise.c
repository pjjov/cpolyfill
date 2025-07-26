/*
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
#define TEST_CTZ(m_type, m_fn)                           \
    pf_assert((m_type)0 == pf_ctz##m_fn(~(m_type)0));    \
    pf_assert((m_type)0 == pf_ctz##m_fn((m_type)1));     \
    pf_assert((m_type)1 == pf_ctz##m_fn((m_type)2));     \
    pf_assert((m_type)0 == pf_ctz##m_fn((m_type)15));    \
    pf_assert((m_type)4 == pf_ctz##m_fn((m_type)16));    \
    pf_assert((m_type)7 == pf_ctz##m_fn(~(m_type)0x7F)); \
    pf_assert(sizeof(m_type) * 8 == pf_ctz##m_fn(0));    \
    for (m_type i = 0; i < sizeof(m_type) * 8; i++) {    \
        pf_assert(i == pf_ctz##m_fn(~(m_type)0 << i));   \
        pf_assert(i == pf_ctz##m_fn((m_type)1 << i));    \
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

pf_test suite_bitwise[] = {
    { test_bitwise_clz, "/bitwise/clz", 1 },
    { test_bitwise_ctz, "/bitwise/ctz", 1 },
    { test_bitwise_pow2ceil, "/bitwise/pow2ceil", 1 },
    { test_bitwise_pow2floor, "/bitwise/pow2floor", 1 },
    { test_bitwise_popcount, "/bitwise/popcount", 1 },
    { test_bitwise_parity, "/bitwise/parity", 1 },
    { test_bitwise_rotr, "/bitwise/rotr", 1 },
    { test_bitwise_rotl, "/bitwise/rotl", 1 },
    { 0 },
};
