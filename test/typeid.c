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

#include "check.h"
#include "../include/pf_threads.h"
#include "../include/pf_atomic.h"
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#define PF_TYPE_HELPERS
#include "../include/pf_typeid.h"

/* ---------------------------------------------------------------- */
/* Identifiers                                                       */
/* ---------------------------------------------------------------- */

static int test_typeid_values(int seed, int rep) {
    CHECK(PF_TYPE_ERROR == -1);
    CHECK(PF_TYPE_VOID == 0);
    CHECK(PF_TYPE_PTR == 1);
    CHECK(PF_TYPE_CHAR == 2);
    CHECK(PF_TYPE_CSTRING == 3);
    CHECK(PF_TYPE_SCHAR == 4 && PF_TYPE_UCHAR == 5);
    CHECK(PF_TYPE_SHRT == 6 && PF_TYPE_USHRT == 7);
    CHECK(PF_TYPE_INT == 8 && PF_TYPE_UINT == 9);
    CHECK(PF_TYPE_LONG == 10 && PF_TYPE_ULONG == 11);
    CHECK(PF_TYPE_LLONG == 12 && PF_TYPE_ULLONG == 13);
    CHECK(PF_TYPE_PTRDIFF == 14 && PF_TYPE_SIZE == 15);
    CHECK(PF_TYPE_FLOAT == 21 && PF_TYPE_DOUBLE == 22 && PF_TYPE_LDOUBLE == 23);
    CHECK(PF_TYPE_INT8 == 32 && PF_TYPE_UINT8 == 33);
    CHECK(PF_TYPE_INT64 == 38 && PF_TYPE_UINT64 == 39);
    CHECK(PF_TYPE_FILE == 100);
    CHECK(PF_TYPE_ATOMIC_BOOL == 150);
    CHECK(PF_TYPE_ATOMIC_UINTMAX == 187);
    CHECK(PF__TYPE_STANDARD == 1024);
    return 0;
}

static int test_typeid_unique(int seed, int rep) {
    static const int ids[] = {
#define X(u, l, t, f, i) PF_TYPE_##u,
        PF_TYPEID(X, X, X, X, X)
#undef X
    };
    size_t i, j, n = sizeof ids / sizeof ids[0];

    CHECK(n > 100);
    for (i = 0; i < n; i++) {
        CHECK(ids[i] >= 0 && ids[i] < PF__TYPE_STANDARD);
        for (j = i + 1; j < n; j++)
            CHECK(ids[i] != ids[j]);
    }
    return 0;
}

/* Signed types are even, and the unsigned partner is the next number. */
static int test_typeid_pairs(int seed, int rep) {
#define PAIR(S, U) CHECK(PF_TYPE_##S % 2 == 0 && PF_TYPE_##U == PF_TYPE_##S + 1)
    PAIR(SCHAR, UCHAR);
    PAIR(SHRT, USHRT);
    PAIR(INT, UINT);
    PAIR(LONG, ULONG);
    PAIR(LLONG, ULLONG);
    PAIR(PTRDIFF, SIZE);
    PAIR(INTPTR, UINTPTR);
    PAIR(INTMAX, UINTMAX);
    PAIR(INT8, UINT8);
    PAIR(INT16, UINT16);
    PAIR(INT32, UINT32);
    PAIR(INT64, UINT64);
    PAIR(INT_FAST8, UINT_FAST8);
    PAIR(INT_FAST16, UINT_FAST16);
    PAIR(INT_FAST32, UINT_FAST32);
    PAIR(INT_FAST64, UINT_FAST64);
    PAIR(INT_LEAST8, UINT_LEAST8);
    PAIR(INT_LEAST16, UINT_LEAST16);
    PAIR(INT_LEAST32, UINT_LEAST32);
    PAIR(INT_LEAST64, UINT_LEAST64);
#undef PAIR
    return 0;
}

/* The flags column marks atomic types, and only those. */
static int test_typeid_flags(int seed, int rep) {
    static const struct {
        int id, flags;
    } all[] = {
#define X(u, l, t, f, i) { PF_TYPE_##u, f },
        PF_TYPEID(X, X, X, X, X)
#undef X
    };
    size_t i;

    for (i = 0; i < sizeof all / sizeof all[0]; i++) {
        if (all[i].id >= PF_TYPE_ATOMIC_BOOL
            && all[i].id <= PF_TYPE_ATOMIC_UINTMAX)
            CHECK(all[i].flags == 1);
        else
            CHECK(all[i].flags == 0);
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Classification                                                    */
/* ---------------------------------------------------------------- */

static int test_typeid_is_integer(int seed, int rep) {
#define S(u, l, t, f, i) CHECK(pf_type_is_integer(PF_TYPE_##u));
#define N(u, l, t, f, i) CHECK(!pf_type_is_integer(PF_TYPE_##u));
#define IGN(...)
    PF_TYPEID(S, S, N, N, N)
#undef S
#undef N
#undef IGN
    CHECK(!pf_type_is_integer(PF_TYPE_ERROR));
    CHECK(!pf_type_is_integer(-100));
    CHECK(!pf_type_is_integer(PF__TYPE_STANDARD));
    CHECK(!pf_type_is_integer(100000));
    return 0;
}

static int test_typeid_is_float(int seed, int rep) {
#define F(u, l, t, f, i) CHECK(pf_type_is_float(PF_TYPE_##u));
#define N(u, l, t, f, i) CHECK(!pf_type_is_float(PF_TYPE_##u));
    PF_TYPEID(N, N, F, N, N)
#undef F
#undef N
    CHECK(!pf_type_is_float(PF_TYPE_ERROR));
    CHECK(!pf_type_is_float(PF__TYPE_STANDARD));
    return 0;
}

static int test_typeid_is_unsigned(int seed, int rep) {
#define SIGNED(u, l, t, f, i) CHECK(pf_type_is_unsigned(PF_TYPE_##u) == 0);
#define UNSIGNED(u, l, t, f, i) CHECK(pf_type_is_unsigned(PF_TYPE_##u) == 1);
#define IGN(...)
    PF_TYPEID(SIGNED, UNSIGNED, IGN, IGN, IGN)
#undef SIGNED
#undef UNSIGNED
#undef IGN
    /* Other types are neither: an error. */
    CHECK(pf_type_is_unsigned(PF_TYPE_FLOAT) == PF_TYPE_ERROR);
    CHECK(pf_type_is_unsigned(PF_TYPE_VOID) == PF_TYPE_ERROR);
    CHECK(pf_type_is_unsigned(PF_TYPE_PTR) == PF_TYPE_ERROR);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Sizes and limits                                                  */
/* ---------------------------------------------------------------- */

static int test_typeid_int_size(int seed, int rep) {
#define SZ(u, l, t, f, i) CHECK(pf_type_int_size(PF_TYPE_##u) == sizeof(t));
#define IGN(...)
    PF_TYPEID(SZ, SZ, IGN, IGN, IGN)
#undef SZ
#undef IGN
    CHECK(pf_type_int_size(PF_TYPE_FLOAT) == 0);
    CHECK(pf_type_int_size(PF_TYPE_PTR) == 0);
    CHECK(pf_type_int_size(PF_TYPE_ERROR) == 0);
    return 0;
}

static int test_typeid_int_max_unsigned(int seed, int rep) {
    CHECK(pf_type_int_max(PF_TYPE_UCHAR) == UCHAR_MAX);
    CHECK(pf_type_int_max(PF_TYPE_USHRT) == USHRT_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINT) == UINT_MAX);
    CHECK(pf_type_int_max(PF_TYPE_ULONG) == ULONG_MAX);
    CHECK(pf_type_int_max(PF_TYPE_ULLONG) == ULLONG_MAX);
    CHECK(pf_type_int_max(PF_TYPE_SIZE) == SIZE_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINTPTR) == UINTPTR_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINTMAX) == UINTMAX_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINT8) == UINT8_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINT16) == UINT16_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINT32) == UINT32_MAX);
    CHECK(pf_type_int_max(PF_TYPE_UINT64) == UINT64_MAX);
    CHECK(pf_type_int_max(PF_TYPE_FLOAT) == 0);
    CHECK(pf_type_int_max(PF_TYPE_ERROR) == 0);
    return 0;
}

static int test_typeid_int_max_signed(int seed, int rep) {
    CHECK(pf_type_int_max(PF_TYPE_SCHAR) == SCHAR_MAX);
    CHECK(pf_type_int_max(PF_TYPE_SHRT) == SHRT_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INT) == INT_MAX);
    CHECK(pf_type_int_max(PF_TYPE_LONG) == LONG_MAX);
    CHECK(pf_type_int_max(PF_TYPE_LLONG) == LLONG_MAX);
    CHECK(pf_type_int_max(PF_TYPE_PTRDIFF) == PTRDIFF_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INTPTR) == INTPTR_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INTMAX) == INTMAX_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INT8) == INT8_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INT16) == INT16_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INT32) == INT32_MAX);
    CHECK(pf_type_int_max(PF_TYPE_INT64) == INT64_MAX);
    return 0;
}

static int test_typeid_int_min(int seed, int rep) {
    /* Unsigned types start at zero. */
    CHECK(pf_type_int_min(PF_TYPE_UCHAR) == 0);
    CHECK(pf_type_int_min(PF_TYPE_UINT) == 0);
    CHECK(pf_type_int_min(PF_TYPE_SIZE) == 0);
    CHECK(pf_type_int_min(PF_TYPE_UINT64) == 0);

    CHECK(pf_type_int_min(PF_TYPE_SCHAR) == SCHAR_MIN);
    CHECK(pf_type_int_min(PF_TYPE_SHRT) == SHRT_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INT) == INT_MIN);
    CHECK(pf_type_int_min(PF_TYPE_LONG) == LONG_MIN);
    CHECK(pf_type_int_min(PF_TYPE_LLONG) == LLONG_MIN);
    CHECK(pf_type_int_min(PF_TYPE_PTRDIFF) == PTRDIFF_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INTPTR) == INTPTR_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INTMAX) == INTMAX_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INT8) == INT8_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INT16) == INT16_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INT32) == INT32_MIN);
    CHECK(pf_type_int_min(PF_TYPE_INT64) == INT64_MIN);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Loading and storing                                               */
/* ---------------------------------------------------------------- */

static int test_typeid_load_store_unsigned(int seed, int rep) {
    uint8_t u8 = 0xAB;
    uint16_t u16 = 0xABCD;
    uint32_t u32 = 0xDEADBEEFu;
    uint64_t u64 = 0x0123456789ABCDEFull;
    unsigned int ui = 123456u;
    size_t sz = 98765;
    uintmax_t v = 0;

    CHECK(pf_type_int_load(PF_TYPE_UINT8, &u8, &v) == 0 && v == 0xAB);
    CHECK(pf_type_int_load(PF_TYPE_UINT16, &u16, &v) == 0 && v == 0xABCD);
    CHECK(pf_type_int_load(PF_TYPE_UINT32, &u32, &v) == 0 && v == 0xDEADBEEFu);
    CHECK(pf_type_int_load(PF_TYPE_UINT64, &u64, &v) == 0);
    CHECK(v == 0x0123456789ABCDEFull);
    CHECK(pf_type_int_load(PF_TYPE_UINT, &ui, &v) == 0 && v == 123456u);
    CHECK(pf_type_int_load(PF_TYPE_SIZE, &sz, &v) == 0 && v == 98765);

    v = 0x42;
    u8 = 0;
    CHECK(pf_type_int_store(PF_TYPE_UINT8, &v, &u8) == 0 && u8 == 0x42);
    v = 0xFFFFFFFFFFull; /* stores are cut to the size of the type */
    u16 = 0;
    CHECK(pf_type_int_store(PF_TYPE_UINT16, &v, &u16) == 0 && u16 == 0xFFFF);
    u32 = 0;
    CHECK(pf_type_int_store(PF_TYPE_UINT32, &v, &u32) == 0);
    CHECK(u32 == 0xFFFFFFFFu);
    u64 = 0;
    v = 0xFEDCBA9876543210ull;
    CHECK(pf_type_int_store(PF_TYPE_UINT64, &v, &u64) == 0);
    CHECK(u64 == 0xFEDCBA9876543210ull);
    return 0;
}

static int test_typeid_load_store_signed(int seed, int rep) {
    int8_t i8 = -5;
    int16_t i16 = -300;
    int32_t i32 = -70000;
    int64_t i64 = -5000000000ll;
    uintmax_t v = 0;

    CHECK(pf_type_int_load(PF_TYPE_INT8, &i8, &v) == 0);
    CHECK((uint8_t)v == (uint8_t)-5);
    CHECK(pf_type_int_load(PF_TYPE_INT16, &i16, &v) == 0);
    CHECK((uint16_t)v == (uint16_t)-300);
    CHECK(pf_type_int_load(PF_TYPE_INT32, &i32, &v) == 0);
    CHECK((uint32_t)v == (uint32_t)-70000);
    CHECK(pf_type_int_load(PF_TYPE_INT64, &i64, &v) == 0);
    CHECK((int64_t)v == -5000000000ll);

    /* Stores truncate to the type, so the bits come back. */
    i8 = 0;
    v = (uintmax_t)(intmax_t)-9;
    CHECK(pf_type_int_store(PF_TYPE_INT8, &v, &i8) == 0 && i8 == -9);
    i32 = 0;
    v = (uintmax_t)(intmax_t)-123456;
    CHECK(pf_type_int_store(PF_TYPE_INT32, &v, &i32) == 0 && i32 == -123456);
    i64 = 0;
    v = (uintmax_t)(intmax_t)-1;
    CHECK(pf_type_int_store(PF_TYPE_INT64, &v, &i64) == 0 && i64 == -1);
    return 0;
}

static int test_typeid_load_store_roundtrip(int seed, int rep) {
#define RT(u, l, t, f, i)                                     \
    {                                                         \
        t in = (t)(0x5A + seed % 100), out = 0;               \
        uintmax_t v = 0;                                      \
        CHECK(pf_type_int_load(PF_TYPE_##u, &in, &v) == 0);   \
        CHECK(pf_type_int_store(PF_TYPE_##u, &v, &out) == 0); \
        CHECK(in == out);                                     \
    }
#define IGN(...)
    PF_TYPEID(RT, RT, IGN, IGN, IGN)
#undef RT
#undef IGN
    return 0;
}

static int test_typeid_load_store_errors(int seed, int rep) {
    float f = 1.0f;
    void *p = &f;
    uintmax_t v = 1;

    CHECK(pf_type_int_load(PF_TYPE_FLOAT, &f, &v) == PF_TYPE_ERROR);
    CHECK(pf_type_int_load(PF_TYPE_PTR, &p, &v) == PF_TYPE_ERROR);
    CHECK(pf_type_int_load(PF_TYPE_ERROR, &f, &v) == PF_TYPE_ERROR);
    CHECK(pf_type_int_store(PF_TYPE_FLOAT, &v, &f) == PF_TYPE_ERROR);
    CHECK(pf_type_int_store(PF_TYPE_PTR, &v, &p) == PF_TYPE_ERROR);
    CHECK(v == 1);
    return 0;
}

static int test_typeid_neighbouring_bytes(int seed, int rep) {
    /* A store never touches more than the type's own bytes. */
    unsigned char buf[8];
    uintmax_t v = 0xFFFFFFFFFFFFFFFFull;
    int type[] = {
        PF_TYPE_UINT8, PF_TYPE_UINT16, PF_TYPE_UINT32, PF_TYPE_UINT64
    };
    size_t i;

    for (i = 0; i < 4; i++) {
        size_t size = pf_type_int_size(type[i]), j;
        memset(buf, 0x11, sizeof buf);
        CHECK(pf_type_int_store(type[i], &v, buf) == 0);
        for (j = 0; j < size; j++)
            CHECK(buf[j] == 0xFF);
        for (j = size; j < sizeof buf; j++)
            CHECK(buf[j] == 0x11);
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Conversions between families                                      */
/* ---------------------------------------------------------------- */

static int test_typeid_as_fixed(int seed, int rep) {
    CHECK(pf_type_int_as_fixed(PF_TYPE_INT) == PF_TYPE_INT32);
    CHECK(pf_type_int_as_fixed(PF_TYPE_UINT) == PF_TYPE_UINT32);
    CHECK(pf_type_int_as_fixed(PF_TYPE_SCHAR) == PF_TYPE_INT8);
    CHECK(pf_type_int_as_fixed(PF_TYPE_UCHAR) == PF_TYPE_UINT8);
    CHECK(pf_type_int_as_fixed(PF_TYPE_SHRT) == PF_TYPE_INT16);
    CHECK(pf_type_int_as_fixed(PF_TYPE_USHRT) == PF_TYPE_UINT16);
    CHECK(pf_type_int_as_fixed(PF_TYPE_LLONG) == PF_TYPE_INT64);
    CHECK(pf_type_int_as_fixed(PF_TYPE_ULLONG) == PF_TYPE_UINT64);

    CHECK(
        pf_type_int_as_fixed(PF_TYPE_SIZE)
        == (sizeof(size_t) == 8 ? PF_TYPE_UINT64 : PF_TYPE_UINT32)
    );
    CHECK(
        pf_type_int_as_fixed(PF_TYPE_INTPTR)
        == (sizeof(void *) == 8 ? PF_TYPE_INT64 : PF_TYPE_INT32)
    );
    CHECK(
        pf_type_int_as_fixed(PF_TYPE_LONG)
        == (sizeof(long) == 8 ? PF_TYPE_INT64 : PF_TYPE_INT32)
    );
    CHECK(
        pf_type_int_as_fixed(PF_TYPE_ULONG)
        == (sizeof(long) == 8 ? PF_TYPE_UINT64 : PF_TYPE_UINT32)
    );

    CHECK(pf_type_int_as_fixed(PF_TYPE_FLOAT) == PF_TYPE_ERROR);
    CHECK(pf_type_int_as_fixed(PF_TYPE_PTR) == PF_TYPE_ERROR);
    return 0;
}

/* Whatever the mapping, size and signedness must be kept. */
static int test_typeid_fixed_properties(int seed, int rep) {
#define PROP(u, l, t, f, i)                                                    \
    {                                                                          \
        int fixed = pf_type_int_as_fixed(PF_TYPE_##u);                         \
        CHECK(pf_type_is_integer(fixed));                                      \
        CHECK(pf_type_int_size(fixed) == sizeof(t));                           \
        CHECK(pf_type_is_unsigned(fixed) == pf_type_is_unsigned(PF_TYPE_##u)); \
    }
#define IGN(...)
    PF_TYPEID(PROP, PROP, IGN, IGN, IGN)
#undef PROP
#undef IGN
    return 0;
}

static int test_typeid_as_base(int seed, int rep) {
    CHECK(pf_type_int_as_base(PF_TYPE_INT32) == PF_TYPE_INT);
    CHECK(pf_type_int_as_base(PF_TYPE_UINT32) == PF_TYPE_UINT);
    CHECK(pf_type_int_as_base(PF_TYPE_INT16) == PF_TYPE_SHRT);
    CHECK(pf_type_int_as_base(PF_TYPE_UINT16) == PF_TYPE_USHRT);
    CHECK(pf_type_int_as_base(PF_TYPE_INT8) == PF_TYPE_SCHAR);
    CHECK(pf_type_int_as_base(PF_TYPE_UINT8) == PF_TYPE_UCHAR);
    CHECK(pf_type_int_as_base(PF_TYPE_FLOAT) == PF_TYPE_ERROR);
    CHECK(pf_type_int_as_base(PF_TYPE_PTR) == PF_TYPE_ERROR);
    return 0;
}

/* The base type has the size and signedness of the original. */
static int test_typeid_base_properties(int seed, int rep) {
#define PROP(u, l, t, f, i)                                                   \
    {                                                                         \
        int base = pf_type_int_as_base(PF_TYPE_##u);                          \
        CHECK(pf_type_is_integer(base));                                      \
        CHECK(pf_type_int_size(base) == sizeof(t));                           \
        CHECK(pf_type_is_unsigned(base) == pf_type_is_unsigned(PF_TYPE_##u)); \
    }
#define IGN(...)
    PF_TYPEID(PROP, PROP, IGN, IGN, IGN)
#undef PROP
#undef IGN
    return 0;
}

pf_test_t suite_typeid[] = {
    { test_typeid_values, "/typeid/values" },
    { test_typeid_unique, "/typeid/unique" },
    { test_typeid_pairs, "/typeid/pairs" },
    { test_typeid_flags, "/typeid/flags" },
    { test_typeid_is_integer, "/typeid/is_integer" },
    { test_typeid_is_float, "/typeid/is_float" },
    { test_typeid_is_unsigned, "/typeid/is_unsigned" },
    { test_typeid_int_size, "/typeid/int_size" },
    { test_typeid_int_max_unsigned, "/typeid/int_max_unsigned" },
    { test_typeid_int_max_signed, "/typeid/int_max_signed" },
    { test_typeid_int_min, "/typeid/int_min" },
    { test_typeid_load_store_unsigned, "/typeid/load_store_unsigned" },
    { test_typeid_load_store_signed, "/typeid/load_store_signed" },
    { test_typeid_load_store_roundtrip, "/typeid/load_store_roundtrip" },
    { test_typeid_load_store_errors, "/typeid/load_store_errors" },
    { test_typeid_neighbouring_bytes, "/typeid/neighbouring_bytes" },
    { test_typeid_as_fixed, "/typeid/as_fixed" },
    { test_typeid_fixed_properties, "/typeid/fixed_properties" },
    { test_typeid_as_base, "/typeid/as_base" },
    { test_typeid_base_properties, "/typeid/base_properties" },
    { 0 },
};
