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
#include "../include/pf_overflow.h"
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

#define TEST(m_name, m_op, m_bool, m_a, m_b)           \
    pf_assert_##m_bool(m_name((m_a), (m_b), &result)); \
    pf_assert(result == (m_a)m_op(m_b));

#define TEST_OF_SIGNED(m_name, m_type, m_postfix, m_min, m_max)   \
    int m_name(int seed, int repetition) {                        \
        m_type result;                                            \
        TEST(pf_overflow_sadd##m_postfix, +, false, 1, 1)         \
        TEST(pf_overflow_sadd##m_postfix, +, false, -1000, 1)     \
        TEST(pf_overflow_sadd##m_postfix, +, true, 1, m_max)      \
        TEST(pf_overflow_sadd##m_postfix, +, true, m_max, m_max)  \
        TEST(pf_overflow_sadd##m_postfix, +, true, m_min, -1)     \
        TEST(pf_overflow_sadd##m_postfix, +, true, m_min, m_min)  \
        TEST(pf_overflow_ssub##m_postfix, -, false, 1, 1)         \
        TEST(pf_overflow_ssub##m_postfix, -, false, 1, m_max)     \
        TEST(pf_overflow_ssub##m_postfix, -, false, m_min, m_min) \
        TEST(pf_overflow_ssub##m_postfix, -, false, 0, m_max)     \
        TEST(pf_overflow_ssub##m_postfix, -, true, m_min, 1)      \
        TEST(pf_overflow_ssub##m_postfix, -, true, m_min, m_max)  \
        TEST(pf_overflow_smul##m_postfix, *, false, 5, 5)         \
        TEST(pf_overflow_smul##m_postfix, *, true, m_max, 2)      \
        TEST(pf_overflow_smul##m_postfix, *, true, m_min, 2)      \
        TEST(pf_overflow_smul##m_postfix, *, true, m_max, -2)     \
        TEST(pf_overflow_smul##m_postfix, *, true, m_min, -2)     \
        TEST(pf_overflow_smul##m_postfix, *, true, m_max, m_min)  \
        return 0;                                                 \
    }

#define TEST_OF_UNSIGNED(m_name, m_type, m_postfix, m_max)       \
    int m_name(int seed, int repetition) {                       \
        m_type result;                                           \
        TEST(pf_overflow_uadd##m_postfix, +, false, 0, 1)        \
        TEST(pf_overflow_uadd##m_postfix, +, true, m_max, 1)     \
        TEST(pf_overflow_uadd##m_postfix, +, true, m_max, m_max) \
        TEST(pf_overflow_usub##m_postfix, -, false, 1, 1)        \
        TEST(pf_overflow_usub##m_postfix, -, true, 0, 1)         \
        TEST(pf_overflow_usub##m_postfix, -, true, 1, m_max)     \
        TEST(pf_overflow_umul##m_postfix, *, false, 5, 5)        \
        TEST(pf_overflow_umul##m_postfix, *, true, m_max, 2)     \
        TEST(pf_overflow_umul##m_postfix, *, true, m_max, m_max) \
        return 0;                                                \
    }

#define TEST_SA_SIGNED(m_name, m_postfix, m_min, m_max)              \
    int m_name(int seed, int repetition) {                           \
        pf_assert(-1 == pf_saturated_sadd##m_postfix(-2, 1));        \
        pf_assert(-1 == pf_saturated_sadd##m_postfix(1, -2));        \
        pf_assert(m_max == pf_saturated_sadd##m_postfix(1, m_max));  \
        pf_assert(m_min == pf_saturated_sadd##m_postfix(m_min, -1)); \
        pf_assert(-1 == pf_saturated_ssub##m_postfix(1, 2));         \
        pf_assert(1 == pf_saturated_ssub##m_postfix(-2, -3));        \
        pf_assert(m_min == pf_saturated_ssub##m_postfix(-1, m_max)); \
        pf_assert(m_min == pf_saturated_ssub##m_postfix(-2, m_max)); \
        pf_assert(m_max == pf_saturated_ssub##m_postfix(0, m_min));  \
        pf_assert(4 == pf_saturated_smul##m_postfix(2, 2));          \
        pf_assert(-4 == pf_saturated_smul##m_postfix(-2, 2));        \
        pf_assert(m_max == pf_saturated_smul##m_postfix(2, m_max));  \
        pf_assert(m_min == pf_saturated_smul##m_postfix(2, m_min));  \
        pf_assert(m_max == pf_saturated_smul##m_postfix(-2, m_min)); \
        return 0;                                                    \
    }

#define TEST_SA_UNSIGNED(m_name, m_postfix, m_max)                  \
    int m_name(int seed, int repetition) {                          \
        pf_assert(2 == pf_saturated_uadd##m_postfix(1, 1));         \
        pf_assert(m_max == pf_saturated_uadd##m_postfix(m_max, 1)); \
        pf_assert(0 == pf_saturated_usub##m_postfix(1, 1));         \
        pf_assert(0 == pf_saturated_usub##m_postfix(1, m_max));     \
        pf_assert(4 == pf_saturated_umul##m_postfix(2, 2));         \
        pf_assert(m_max == pf_saturated_umul##m_postfix(2, m_max)); \
        return 0;                                                   \
    }

TEST_OF_SIGNED(test_overflow_int, int, , INT_MIN, INT_MAX)
TEST_OF_SIGNED(test_overflow_long, long, l, LONG_MIN, LONG_MAX)
TEST_OF_SIGNED(test_overflow_llong, long long, ll, LLONG_MIN, LLONG_MAX)
TEST_OF_SIGNED(test_overflow_int32, int32_t, 32, INT32_MIN, INT32_MAX)
TEST_OF_SIGNED(test_overflow_int64, int64_t, 64, INT64_MIN, INT64_MAX)
TEST_OF_SIGNED(test_overflow_intmax, intmax_t, max, INTMAX_MIN, INTMAX_MAX)
TEST_OF_SIGNED(test_overflow_intptr, intptr_t, ptr, INTPTR_MIN, INTPTR_MAX)
TEST_OF_SIGNED(test_overflow_ptrdiff, ptrdiff_t, diff, PTRDIFF_MIN, PTRDIFF_MAX)

TEST_OF_UNSIGNED(test_overflow_uint, unsigned int, , UINT_MAX)
TEST_OF_UNSIGNED(test_overflow_ulong, unsigned long, l, ULONG_MAX)
TEST_OF_UNSIGNED(test_overflow_ullong, unsigned long long, ll, ULLONG_MAX)
TEST_OF_UNSIGNED(test_overflow_uint32, uint32_t, 32, UINT32_MAX)
TEST_OF_UNSIGNED(test_overflow_uint64, uint64_t, 64, UINT64_MAX)
TEST_OF_UNSIGNED(test_overflow_uintmax, uintmax_t, max, UINTMAX_MAX)
TEST_OF_UNSIGNED(test_overflow_uintptr, uintptr_t, ptr, UINTPTR_MAX)
TEST_OF_UNSIGNED(test_overflow_size, size_t, size, SIZE_MAX)

TEST_SA_SIGNED(test_saturated_int, , INT_MIN, INT_MAX)
TEST_SA_SIGNED(test_saturated_long, l, LONG_MIN, LONG_MAX)
TEST_SA_SIGNED(test_saturated_llong, ll, LLONG_MIN, LLONG_MAX)
TEST_SA_SIGNED(test_saturated_int32, 32, INT32_MIN, INT32_MAX)
TEST_SA_SIGNED(test_saturated_int64, 64, INT64_MIN, INT64_MAX)
TEST_SA_SIGNED(test_saturated_intmax, max, INTMAX_MIN, INTMAX_MAX)
TEST_SA_SIGNED(test_saturated_intptr, ptr, INTPTR_MIN, INTPTR_MAX)
TEST_SA_SIGNED(test_saturated_ptrdiff, diff, PTRDIFF_MIN, PTRDIFF_MAX)

TEST_SA_UNSIGNED(test_saturated_uint, , UINT_MAX)
TEST_SA_UNSIGNED(test_saturated_ulong, l, ULONG_MAX)
TEST_SA_UNSIGNED(test_saturated_ullong, ll, ULLONG_MAX)
TEST_SA_UNSIGNED(test_saturated_uint32, 32, UINT32_MAX)
TEST_SA_UNSIGNED(test_saturated_uint64, 64, UINT64_MAX)
TEST_SA_UNSIGNED(test_saturated_uintmax, max, UINTMAX_MAX)
TEST_SA_UNSIGNED(test_saturated_uintptr, ptr, UINTPTR_MAX)
TEST_SA_UNSIGNED(test_saturated_size, size, SIZE_MAX)

static pf_test suite_overflow[] = {
    { &test_overflow_int, "/overflow/int", 1 },
    { &test_overflow_long, "/overflow/long", 1 },
    { &test_overflow_llong, "/overflow/llong", 1 },
    { &test_overflow_int32, "/overflow/int32", 1 },
    { &test_overflow_int64, "/overflow/int64", 1 },
    { &test_overflow_intmax, "/overflow/intmax", 1 },
    { &test_overflow_intptr, "/overflow/intptr", 1 },
    { &test_overflow_ptrdiff, "/overflow/ptrdiff", 1 },
    { &test_overflow_uint, "/overflow/uint", 1 },
    { &test_overflow_ulong, "/overflow/ulong", 1 },
    { &test_overflow_ullong, "/overflow/ullong", 1 },
    { &test_overflow_uint32, "/overflow/uint32", 1 },
    { &test_overflow_uint64, "/overflow/uint64", 1 },
    { &test_overflow_uintmax, "/overflow/uintmax", 1 },
    { &test_overflow_uintptr, "/overflow/uintptr", 1 },
    { &test_overflow_size, "/overflow/size_t", 1 },

    { &test_saturated_int, "/saturated/int", 1 },
    { &test_saturated_long, "/saturated/long", 1 },
    { &test_saturated_llong, "/saturated/llong", 1 },
    { &test_saturated_int32, "/saturated/int32", 1 },
    { &test_saturated_int64, "/saturated/int64", 1 },
    { &test_saturated_intmax, "/saturated/intmax", 1 },
    { &test_saturated_intptr, "/saturated/intptr", 1 },
    { &test_saturated_ptrdiff, "/saturated/ptrdiff", 1 },
    { &test_saturated_uint, "/saturated/uint", 1 },
    { &test_saturated_ulong, "/saturated/ulong", 1 },
    { &test_saturated_ullong, "/saturated/ullong", 1 },
    { &test_saturated_uint32, "/saturated/uint32", 1 },
    { &test_saturated_uint64, "/saturated/uint64", 1 },
    { &test_saturated_uintmax, "/saturated/uintmax", 1 },
    { &test_saturated_uintptr, "/saturated/uintptr", 1 },
    { &test_saturated_size, "/saturated/size_t", 1 },
    { 0 },
};

int main(int argc, char *argv[]) {
    return pf_suite_run(suite_overflow, argc == 2 ? atoi(argv[1]) : 0);
}
