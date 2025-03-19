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

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Woverflow"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Woverflow"
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4307; disable: 4756)
#endif

int test_overflow_int(int seed, int repetition) {
    int result;
    pf_assert(0 == pf_overflow_sadd(1, 1, &result));
    pf_assert(result == 1 + 1);

    pf_assert(0 == pf_overflow_sadd(-1000, 1, &result));
    pf_assert(result == -1000 + 1);

    pf_assert(1 == pf_overflow_sadd(1, INT_MAX, &result));
    pf_assert(result == 1 + INT_MAX);

    pf_assert(1 == pf_overflow_sadd(INT_MAX, INT_MAX, &result));
    pf_assert(result == INT_MAX + INT_MAX);

    pf_assert(1 == pf_overflow_sadd(INT_MIN, -1, &result));
    pf_assert(result == INT_MIN + (-1));

    pf_assert(1 == pf_overflow_sadd(INT_MIN, INT_MIN, &result));
    pf_assert(result == INT_MIN + INT_MIN);

    pf_assert(0 == pf_overflow_ssub(1, 1, &result));
    pf_assert(result == 1 - 1);

    pf_assert(0 == pf_overflow_ssub(1, INT_MAX, &result));
    pf_assert(result == 1 - INT_MAX);

    pf_assert(0 == pf_overflow_ssub(INT_MIN, INT_MIN, &result));
    pf_assert(result == INT_MIN - INT_MIN);

    pf_assert(0 == pf_overflow_ssub(0, INT_MAX, &result));
    pf_assert(result == 0 - INT_MAX);

    pf_assert(1 == pf_overflow_ssub(INT_MIN, 1, &result));
    pf_assert(result == INT_MIN - 1);

    pf_assert(1 == pf_overflow_ssub(INT_MIN, INT_MAX, &result));
    pf_assert(result == INT_MIN - INT_MAX);

    pf_assert(0 == pf_overflow_smul(5, 5, &result));
    pf_assert(result == 5 * 5);

    pf_assert(1 == pf_overflow_smul(INT_MAX, 2, &result));
    pf_assert(result == INT_MAX * 2);

    pf_assert(1 == pf_overflow_smul(INT_MIN, 2, &result));
    pf_assert(result == INT_MIN * 2);

    pf_assert(1 == pf_overflow_smul(INT_MAX, -2, &result));
    pf_assert(result == INT_MAX * (-2));

    pf_assert(1 == pf_overflow_smul(INT_MIN, -2, &result));
    pf_assert(result == INT_MIN * (-2));

    pf_assert(1 == pf_overflow_smul(INT_MAX, INT_MIN, &result));
    pf_assert(result == INT_MAX * INT_MIN);
    return 0;
}

int test_overflow_long(int seed, int repetition) {
    long result;
    pf_assert(0 == pf_overflow_saddl(1, 1, &result));
    pf_assert(result == 1 + 1);

    pf_assert(0 == pf_overflow_saddl(-1000, 1, &result));
    pf_assert(result == -1000 + 1);

    pf_assert(1 == pf_overflow_saddl(1, LONG_MAX, &result));
    pf_assert(result == 1 + LONG_MAX);

    pf_assert(1 == pf_overflow_saddl(LONG_MAX, LONG_MAX, &result));
    pf_assert(result == LONG_MAX + LONG_MAX);

    pf_assert(1 == pf_overflow_saddl(LONG_MIN, -1, &result));
    pf_assert(result == LONG_MIN + (-1));

    pf_assert(1 == pf_overflow_saddl(LONG_MIN, LONG_MIN, &result));
    pf_assert(result == LONG_MIN + LONG_MIN);

    pf_assert(0 == pf_overflow_ssubl(1, 1, &result));
    pf_assert(result == 1 - 1);

    pf_assert(0 == pf_overflow_ssubl(1, LONG_MAX, &result));
    pf_assert(result == 1 - LONG_MAX);

    pf_assert(0 == pf_overflow_ssubl(LONG_MIN, LONG_MIN, &result));
    pf_assert(result == LONG_MIN - LONG_MIN);

    pf_assert(0 == pf_overflow_ssubl(0, LONG_MAX, &result));
    pf_assert(result == 0 - LONG_MAX);

    pf_assert(1 == pf_overflow_ssubl(LONG_MIN, 1, &result));
    pf_assert(result == LONG_MIN - 1);

    pf_assert(1 == pf_overflow_ssubl(LONG_MIN, LONG_MAX, &result));
    pf_assert(result == LONG_MIN - LONG_MAX);

    pf_assert(0 == pf_overflow_smull(5, 5, &result));
    pf_assert(result == 5 * 5);

    pf_assert(1 == pf_overflow_smull(LONG_MAX, 2, &result));
    pf_assert(result == LONG_MAX * 2);

    pf_assert(1 == pf_overflow_smull(LONG_MIN, 2, &result));
    pf_assert(result == LONG_MIN * 2);

    pf_assert(1 == pf_overflow_smull(LONG_MAX, -2, &result));
    pf_assert(result == LONG_MAX * (-2));

    pf_assert(1 == pf_overflow_smull(LONG_MIN, -2, &result));
    pf_assert(result == LONG_MIN * (-2));

    pf_assert(1 == pf_overflow_smull(LONG_MAX, LONG_MIN, &result));
    pf_assert(result == LONG_MAX * LONG_MIN);
    return 0;
}

int test_overflow_long_long(int seed, int repetition) {
    long long result;
    pf_assert(0 == pf_overflow_saddll(1, 1, &result));
    pf_assert(result == 1 + 1);

    pf_assert(0 == pf_overflow_saddll(-1000, 1, &result));
    pf_assert(result == -1000 + 1);

    pf_assert(1 == pf_overflow_saddll(1, LLONG_MAX, &result));
    pf_assert(result == 1 + LLONG_MAX);

    pf_assert(1 == pf_overflow_saddll(LLONG_MAX, LLONG_MAX, &result));
    pf_assert(result == LLONG_MAX + LLONG_MAX);

    pf_assert(1 == pf_overflow_saddll(LLONG_MIN, -1, &result));
    pf_assert(result == LLONG_MIN + (-1));

    pf_assert(1 == pf_overflow_saddll(LLONG_MIN, LLONG_MIN, &result));
    pf_assert(result == LLONG_MIN + LLONG_MIN);

    pf_assert(0 == pf_overflow_ssubll(1, 1, &result));
    pf_assert(result == 1 - 1);

    pf_assert(0 == pf_overflow_ssubll(1, LLONG_MAX, &result));
    pf_assert(result == 1 - LLONG_MAX);

    pf_assert(0 == pf_overflow_ssubll(LLONG_MIN, LLONG_MIN, &result));
    pf_assert(result == LLONG_MIN - LLONG_MIN);

    pf_assert(0 == pf_overflow_ssubll(0, LLONG_MAX, &result));
    pf_assert(result == 0 - LLONG_MAX);

    pf_assert(1 == pf_overflow_ssubll(LLONG_MIN, 1, &result));
    pf_assert(result == LLONG_MIN - 1);

    pf_assert(1 == pf_overflow_ssubll(LLONG_MIN, LLONG_MAX, &result));
    pf_assert(result == LLONG_MIN - LLONG_MAX);

    pf_assert(0 == pf_overflow_smulll(5, 5, &result));
    pf_assert(result == 5 * 5);

    pf_assert(1 == pf_overflow_smulll(LLONG_MAX, 2, &result));
    pf_assert(result == LLONG_MAX * 2);

    pf_assert(1 == pf_overflow_smulll(LLONG_MIN, 2, &result));
    pf_assert(result == LLONG_MIN * 2);

    pf_assert(1 == pf_overflow_smulll(LLONG_MAX, -2, &result));
    pf_assert(result == LLONG_MAX * (-2));

    pf_assert(1 == pf_overflow_smulll(LLONG_MIN, -2, &result));
    pf_assert(result == LLONG_MIN * (-2));

    pf_assert(1 == pf_overflow_smulll(LLONG_MAX, LLONG_MIN, &result));
    pf_assert(result == LLONG_MAX * LLONG_MIN);
    return 0;
}

int test_overflow_uint(int seed, int repetition) {
    unsigned int result;

    pf_assert(0 == pf_overflow_uadd(0, 1, &result));
    pf_assert(result == 0 + 1);

    pf_assert(1 == pf_overflow_uadd(UINT_MAX, 1, &result));
    pf_assert(result == UINT_MAX + 1);

    pf_assert(1 == pf_overflow_uadd(UINT_MAX, UINT_MAX, &result));
    pf_assert(result == UINT_MAX + UINT_MAX);

    pf_assert(0 == pf_overflow_usub(1, 1, &result));
    pf_assert(result == 1 - 1);

    pf_assert(1 == pf_overflow_usub(0, 1, &result));
    pf_assert(result == 0 - 1);

    pf_assert(1 == pf_overflow_usub(1, UINT_MAX, &result));
    pf_assert(result == 1 - UINT_MAX);

    pf_assert(0 == pf_overflow_umul(5, 5, &result));
    pf_assert(result == 5 * 5);

    pf_assert(1 == pf_overflow_umul(UINT_MAX, 2, &result));
    pf_assert(result == UINT_MAX * 2);

    pf_assert(1 == pf_overflow_umul(UINT_MAX, UINT_MAX, &result));
    pf_assert(result == UINT_MAX * UINT_MAX);
    return 0;
}

int test_overflow_ulong(int seed, int repetition) {
    unsigned long result;

    pf_assert(0 == pf_overflow_uaddl(0, 1, &result));
    pf_assert(result == 0 + 1);

    pf_assert(1 == pf_overflow_uaddl(ULONG_MAX, 1, &result));
    pf_assert(result == ULONG_MAX + 1);

    pf_assert(1 == pf_overflow_uaddl(ULONG_MAX, ULONG_MAX, &result));
    pf_assert(result == ULONG_MAX + ULONG_MAX);

    pf_assert(0 == pf_overflow_usubl(1, 1, &result));
    pf_assert(result == 1 - 1);

    pf_assert(1 == pf_overflow_usubl(0, 1, &result));
    pf_assert(result == 0 - 1);

    pf_assert(1 == pf_overflow_usubl(1, ULONG_MAX, &result));
    pf_assert(result == 1 - ULONG_MAX);

    pf_assert(0 == pf_overflow_umull(5, 5, &result));
    pf_assert(result == 5 * 5);

    pf_assert(1 == pf_overflow_umull(ULONG_MAX, 2, &result));
    pf_assert(result == ULONG_MAX * 2);

    pf_assert(1 == pf_overflow_umull(ULONG_MAX, ULONG_MAX, &result));
    pf_assert(result == ULONG_MAX * ULONG_MAX);
    return 0;
}

int test_overflow_ulong_long(int seed, int repetition) {
    unsigned long long result;

    pf_assert(0 == pf_overflow_uaddll(0, 1, &result));
    pf_assert(result == 0 + 1);

    pf_assert(1 == pf_overflow_uaddll(ULLONG_MAX, 1, &result));
    pf_assert(result == ULLONG_MAX + 1);

    pf_assert(1 == pf_overflow_uaddll(ULLONG_MAX, ULLONG_MAX, &result));
    pf_assert(result == ULLONG_MAX + ULLONG_MAX);

    pf_assert(0 == pf_overflow_usubll(1, 1, &result));
    pf_assert(result == 1 - 1);

    pf_assert(1 == pf_overflow_usubll(0, 1, &result));
    pf_assert(result == 0 - 1);

    pf_assert(1 == pf_overflow_usubll(1, ULLONG_MAX, &result));
    pf_assert(result == 1 - ULLONG_MAX);

    pf_assert(0 == pf_overflow_umulll(5, 5, &result));
    pf_assert(result == 5 * 5);

    pf_assert(1 == pf_overflow_umulll(ULLONG_MAX, 2, &result));
    pf_assert(result == ULLONG_MAX * 2);

    pf_assert(1 == pf_overflow_umulll(ULLONG_MAX, ULLONG_MAX, &result));
    pf_assert(result == ULLONG_MAX * ULLONG_MAX);
    return 0;
}

int test_saturated_long(int seed, int repetition) {
    pf_assert(-1 == pf_saturated_saddl(-2, 1));
    pf_assert(-1 == pf_saturated_saddl(1, -2));
    pf_assert(LONG_MAX == pf_saturated_saddl(1, LONG_MAX));
    pf_assert(LONG_MIN == pf_saturated_saddl(LONG_MIN, -1));

    pf_assert(-1 == pf_saturated_ssubl(1, 2));
    pf_assert(1 == pf_saturated_ssubl(-2, -3));
    pf_assert(LONG_MIN == pf_saturated_ssubl(-1, LONG_MAX));
    pf_assert(LONG_MIN == pf_saturated_ssubl(-2, LONG_MAX));
    pf_assert(LONG_MAX == pf_saturated_ssubl(0, LONG_MIN));

    pf_assert(4 == pf_saturated_smull(2, 2));
    pf_assert(-4 == pf_saturated_smull(-2, 2));
    pf_assert(LONG_MAX == pf_saturated_smull(2, LONG_MAX));
    pf_assert(LONG_MIN == pf_saturated_smull(2, LONG_MIN));
    pf_assert(LONG_MAX == pf_saturated_smull(-2, LONG_MIN));
    return 0;
}

int test_saturated_long_long(int seed, int repetition) {
    pf_assert(-1 == pf_saturated_saddll(-2, 1));
    pf_assert(-1 == pf_saturated_saddll(1, -2));
    pf_assert(LLONG_MAX == pf_saturated_saddll(1, LLONG_MAX));
    pf_assert(LLONG_MIN == pf_saturated_saddll(LLONG_MIN, -1));

    pf_assert(-1 == pf_saturated_ssubll(1, 2));
    pf_assert(1 == pf_saturated_ssubll(-2, -3));
    pf_assert(LLONG_MIN == pf_saturated_ssubll(-1, LLONG_MAX));
    pf_assert(LLONG_MIN == pf_saturated_ssubll(-2, LLONG_MAX));
    pf_assert(LLONG_MAX == pf_saturated_ssubll(0, LLONG_MIN));

    pf_assert(4 == pf_saturated_smulll(2, 2));
    pf_assert(-4 == pf_saturated_smulll(-2, 2));
    pf_assert(LLONG_MAX == pf_saturated_smulll(2, LLONG_MAX));
    pf_assert(LLONG_MIN == pf_saturated_smulll(2, LLONG_MIN));
    pf_assert(LLONG_MAX == pf_saturated_smulll(-2, LLONG_MIN));
    return 0;
}

int test_saturated_int(int seed, int repetition) {
    pf_assert(-1 == pf_saturated_sadd(-2, 1));
    pf_assert(-1 == pf_saturated_sadd(1, -2));
    pf_assert(INT_MAX == pf_saturated_sadd(1, INT_MAX));
    pf_assert(INT_MIN == pf_saturated_sadd(INT_MIN, -1));

    pf_assert(-1 == pf_saturated_ssub(1, 2));
    pf_assert(1 == pf_saturated_ssub(-2, -3));
    pf_assert(INT_MIN == pf_saturated_ssub(-1, INT_MAX));
    pf_assert(INT_MIN == pf_saturated_ssub(-2, INT_MAX));
    pf_assert(INT_MAX == pf_saturated_ssub(0, INT_MIN));

    pf_assert(4 == pf_saturated_smul(2, 2));
    pf_assert(-4 == pf_saturated_smul(-2, 2));
    pf_assert(INT_MAX == pf_saturated_smul(2, INT_MAX));
    pf_assert(INT_MIN == pf_saturated_smul(2, INT_MIN));
    pf_assert(INT_MAX == pf_saturated_smul(-2, INT_MIN));
    return 0;
}

int test_saturated_uint(int seed, int repetition) {
    pf_assert(2 == pf_saturated_uadd(1, 1));
    pf_assert(UINT_MAX == pf_saturated_uadd(UINT_MAX, 1));

    pf_assert(0 == pf_saturated_usub(1, 1));
    pf_assert(0 == pf_saturated_usub(1, UINT_MAX));

    pf_assert(4 == pf_saturated_umul(2, 2));
    pf_assert(UINT_MAX == pf_saturated_umul(2, UINT_MAX));
    return 0;
}

int test_saturated_ulong(int seed, int repetition) {
    pf_assert(2 == pf_saturated_uadd(1, 1));
    pf_assert(ULONG_MAX == pf_saturated_uaddl(ULONG_MAX, 1));

    pf_assert(0 == pf_saturated_usubl(1, 1));
    pf_assert(0 == pf_saturated_usubl(1, ULONG_MAX));

    pf_assert(4 == pf_saturated_umul(2, 2));
    pf_assert(ULONG_MAX == pf_saturated_umull(2, ULONG_MAX));
    return 0;
}

int test_saturated_ulong_long(int seed, int repetition) {
    pf_assert(2 == pf_saturated_uaddll(1, 1));
    pf_assert(ULLONG_MAX == pf_saturated_uaddll(ULLONG_MAX, 1));

    pf_assert(0 == pf_saturated_usubll(1, 1));
    pf_assert(0 == pf_saturated_usubll(1, ULLONG_MAX));

    pf_assert(4 == pf_saturated_umulll(2, 2));
    pf_assert(ULLONG_MAX == pf_saturated_umulll(2, ULLONG_MAX));
    return 0;
}

static pf_test suite_overflow[] = {
    { &test_overflow_int,         "/overflow/int",         1 },
    { &test_overflow_long,        "/overflow/long",        1 },
    { &test_overflow_long_long,   "/overflow/long_long",   1 },
    { &test_overflow_uint,        "/overflow/uint",        1 },
    { &test_overflow_ulong,       "/overflow/ulong",       1 },
    { &test_overflow_ulong_long,  "/overflow/ulong_long",  1 },
    { &test_saturated_int,        "/saturated/int",        1 },
    { &test_saturated_long,       "/saturated/long",       1 },
    { &test_saturated_long_long,  "/saturated/long_long",  1 },
    { &test_saturated_uint,       "/saturated/uint",       1 },
    { &test_saturated_ulong,      "/saturated/ulong",      1 },
    { &test_saturated_ulong_long, "/saturated/ulong_long", 1 },
    { 0 }
};

int main(int argc, char *argv[]) {
    return pf_suite_run(
        suite_overflow,
        argc == 2 ? atoi(argv[1]) : 0
    );
}
