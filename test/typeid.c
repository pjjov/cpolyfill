/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/errno.h>
#include <cpolyfill/typeid.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "check.h"

#define IGN(u, l, t, f, i)

/* ---- Identifiers ------------------------------------------------------- */

static int test_values(int seed, int i) {
    (void)seed, (void)i;
    /* The numbers of v1, which must never change. */
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_ERROR, -1);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_VOID, 0);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_PTR, 1);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_CHAR, 2);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_CSTRING, 3);
    PF_ASSERT_ALWAYS(PF_TYPE_SCHAR == 4 && PF_TYPE_UCHAR == 5);
    PF_ASSERT_ALWAYS(PF_TYPE_SHRT == 6 && PF_TYPE_USHRT == 7);
    PF_ASSERT_ALWAYS(PF_TYPE_INT == 8 && PF_TYPE_UINT == 9);
    PF_ASSERT_ALWAYS(PF_TYPE_LONG == 10 && PF_TYPE_ULONG == 11);
    PF_ASSERT_ALWAYS(PF_TYPE_LLONG == 12 && PF_TYPE_ULLONG == 13);
    PF_ASSERT_ALWAYS(PF_TYPE_PTRDIFF == 14 && PF_TYPE_SIZE == 15);
    PF_ASSERT_ALWAYS(PF_TYPE_MAX_ALIGN == 16 && PF_TYPE_NULLPTR == 17);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_BOOL, 18);
    PF_ASSERT_ALWAYS(
        PF_TYPE_FLOAT == 21 && PF_TYPE_DOUBLE == 22 && PF_TYPE_LDOUBLE == 23
    );
    PF_ASSERT_ALWAYS(PF_TYPE_MFLOAT == 24 && PF_TYPE_MDOUBLE == 25);
    PF_ASSERT_ALWAYS(PF_TYPE_INTPTR == 28 && PF_TYPE_UINTMAX == 31);
    PF_ASSERT_ALWAYS(PF_TYPE_INT8 == 32 && PF_TYPE_UINT8 == 33);
    PF_ASSERT_ALWAYS(PF_TYPE_INT64 == 38 && PF_TYPE_UINT64 == 39);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_UINT_LEAST64, 55);
    PF_ASSERT_ALWAYS(PF_TYPE_INT128 == 56 && PF_TYPE_UINT128 == 57);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_FILE, 100);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_MB_STATE, 112);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_VA_LIST, 122);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_ATOMIC_BOOL, 150);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_ATOMIC_UINTMAX, 187);
    PF_ASSERT_ALWAYS_EQ(PF_TYPE_USER, 1024);
    return 0;
}

static int test_unique(int seed, int i) {
    static const int ids[] = {
#define X(u, l, t, f, i) PF_TYPE_##u,
        PF_TYPEID(X, X, X, X, X)
#undef X
    };
    size_t a, b, n = sizeof(ids) / sizeof(ids[0]);
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_GT(n, 100);
    for (a = 0; a < n; a++) {
        PF_ASSERT_ALWAYS(ids[a] >= 0 && ids[a] < PF_TYPE_USER);
        for (b = a + 1; b < n; b++)
            PF_ASSERT_ALWAYS_NE(ids[a], ids[b]);
    }
    return 0;
}

/* Signed types are even, and the unsigned partner is the next number. */
static int test_pairs(int seed, int i) {
#define PAIR(S, U)                                                           \
    PF_ASSERT_ALWAYS(PF_TYPE_##S % 2 == 0 && PF_TYPE_##U == PF_TYPE_##S + 1)
    (void)seed, (void)i;
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
    PAIR(INT_FAST64, UINT_FAST64);
    PAIR(INT_LEAST8, UINT_LEAST8);
    PAIR(INT_LEAST64, UINT_LEAST64);
    PAIR(INT128, UINT128);
#undef PAIR
    return 0;
}

/* The flags column marks the atomic types, and only those. */
static int test_flags(int seed, int i) {
#define A(u, l, t, f, i)                                        \
    PF_ASSERT_ALWAYS(f == 1 && pf_type_is_atomic(PF_TYPE_##u));
#define N(u, l, t, f, i)                                         \
    PF_ASSERT_ALWAYS(f == 0 && !pf_type_is_atomic(PF_TYPE_##u));
    (void)seed, (void)i;
    PF_TYPEID(N, N, N, A, N)
#undef A
#undef N
    return 0;
}

/* ---- Classification ---------------------------------------------------- */

static int test_classes(int seed, int i) {
#define S(u, l, t, f, i)                                 \
    PF_ASSERT_ALWAYS(pf_type_is_integer(PF_TYPE_##u));   \
    PF_ASSERT_ALWAYS(pf_type_is_signed(PF_TYPE_##u));    \
    PF_ASSERT_ALWAYS(!pf_type_is_unsigned(PF_TYPE_##u)); \
    PF_ASSERT_ALWAYS(!pf_type_is_float(PF_TYPE_##u));
#define U(u, l, t, f, i)                                \
    PF_ASSERT_ALWAYS(pf_type_is_integer(PF_TYPE_##u));  \
    PF_ASSERT_ALWAYS(!pf_type_is_signed(PF_TYPE_##u));  \
    PF_ASSERT_ALWAYS(pf_type_is_unsigned(PF_TYPE_##u)); \
    PF_ASSERT_ALWAYS(!pf_type_is_float(PF_TYPE_##u));
#define F(u, l, t, f, i)                                 \
    PF_ASSERT_ALWAYS(!pf_type_is_integer(PF_TYPE_##u));  \
    PF_ASSERT_ALWAYS(!pf_type_is_signed(PF_TYPE_##u));   \
    PF_ASSERT_ALWAYS(!pf_type_is_unsigned(PF_TYPE_##u)); \
    PF_ASSERT_ALWAYS(pf_type_is_float(PF_TYPE_##u));
#define N(u, l, t, f, i)                                 \
    PF_ASSERT_ALWAYS(!pf_type_is_integer(PF_TYPE_##u));  \
    PF_ASSERT_ALWAYS(!pf_type_is_signed(PF_TYPE_##u));   \
    PF_ASSERT_ALWAYS(!pf_type_is_unsigned(PF_TYPE_##u)); \
    PF_ASSERT_ALWAYS(!pf_type_is_float(PF_TYPE_##u));
    static const int bad[] = { PF_TYPE_ERROR, -100,         19,    99,
                               188,           PF_TYPE_USER, 100000 };
    size_t k;
    (void)seed, (void)i;

    PF_TYPEID(S, U, F, N, IGN)
    /* char is no integer: its signedness depends on the platform. */
    N(CHAR, char, char, 0, 2)
    N(BOOL, bool, bool, 0, 18)
    N(WCHAR, wchar, wchar_t, 0, 116)
    N(PTR, ptr, void *, 0, 1)
    for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++) {
        PF_ASSERT_ALWAYS(!pf_type_is_integer(bad[k]));
        PF_ASSERT_ALWAYS(!pf_type_is_signed(bad[k]));
        PF_ASSERT_ALWAYS(!pf_type_is_unsigned(bad[k]));
        PF_ASSERT_ALWAYS(!pf_type_is_float(bad[k]));
        PF_ASSERT_ALWAYS(!pf_type_is_atomic(bad[k]));
        PF_ASSERT_ALWAYS_NULL(pf_type_name(bad[k]));
        PF_ASSERT_ALWAYS_EQ(pf_type_size(bad[k]), 0);
    }
#undef S
#undef U
#undef F
#undef N
    return 0;
}

/* ---- Names and sizes --------------------------------------------------- */

static int test_names(int seed, int i) {
#define NAME(u, l, t, f, i)                                 \
    PF_ASSERT_ALWAYS_STR_EQ(pf_type_name(PF_TYPE_##u), #l);
    (void)seed, (void)i;
    PF_TYPEID(NAME, NAME, NAME, NAME, NAME)
#undef NAME
    PF_ASSERT_ALWAYS_STR_EQ(pf_type_name(PF_TYPE_BOOL), "bool");
    PF_ASSERT_ALWAYS_STR_EQ(pf_type_name(PF_TYPE_UINT8), "uint8");
    PF_ASSERT_ALWAYS_STR_EQ(pf_type_name(PF_TYPE_INT128), "int128");
    return 0;
}

static int test_sizes(int seed, int i) {
    /* Every XS, XU and XF type exists once the header is included. */
#define SZ(u, l, t, f, i)                                      \
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_##u), sizeof(t));
    (void)seed, (void)i;
    PF_TYPEID(SZ, SZ, SZ, IGN, IGN)
#undef SZ
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_VOID), 0);
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_PTR), sizeof(void *));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_CHAR), 1);
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_CSTRING), sizeof(char *));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_BOOL), sizeof(bool));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_FILE), sizeof(FILE *));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_TIME), sizeof(time_t));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_WCHAR), sizeof(wchar_t));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_CHAR16), 2);
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_CHAR32), 4);
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_THRD), 0);
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_ATOMIC_INT), 0);
    return 0;
}

/* ---- Limits ------------------------------------------------------------ */

static int test_limits(int seed, int i) {
#define MAX(u, l, t, f, i)                                      \
    PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_##u), u##_MAX);
#define MIN(u, l, t, f, i)                                      \
    PF_ASSERT_ALWAYS_EQ(pf_type_int_min(PF_TYPE_##u), u##_MIN);
#define ZERO(u, l, t, f, i)                               \
    PF_ASSERT_ALWAYS_EQ(pf_type_int_min(PF_TYPE_##u), 0);
    (void)seed, (void)i;
    PF_TYPEID(MAX, MAX, IGN, IGN, IGN)
    PF_TYPEID(MIN, ZERO, IGN, IGN, IGN)
#undef MAX
#undef MIN
#undef ZERO
    PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_UINT8), 255);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_min(PF_TYPE_INT16), -32768);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_FLOAT), 0);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_min(PF_TYPE_FLOAT), 0);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_CHAR), 0);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_ERROR), 0);
    return 0;
}

/* ---- Loads and stores -------------------------------------------------- */

static int test_load_store(int seed, int i) {
    uint8_t u8 = 0xAB;
    uint16_t u16 = 0xABCD;
    uint32_t u32 = 0xDEADBEEFu;
    uint64_t u64 = 0x0123456789ABCDEFull;
    int8_t i8 = -5;
    int16_t i16 = -300;
    int32_t i32 = -70000;
    int64_t i64 = -5000000000ll;
    long l = -42;
    uintmax_t v = 0;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(
        pf_type_int_load(PF_TYPE_UINT8, &u8, &v) == 0 && v == 0xAB
    );
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_UINT16, &u16, &v) == 0);
    PF_ASSERT_ALWAYS_EQ(v, 0xABCD);
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_UINT32, &u32, &v) == 0);
    PF_ASSERT_ALWAYS_EQ(v, 0xDEADBEEFu);
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_UINT64, &u64, &v) == 0);
    PF_ASSERT_ALWAYS_EQ(v, 0x0123456789ABCDEFull);

    /* Signed values keep their value (v1 gave only the low bits). */
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_INT8, &i8, &v) == 0);
    PF_ASSERT_ALWAYS_EQ((intmax_t)v, -5);
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_INT16, &i16, &v) == 0);
    PF_ASSERT_ALWAYS_EQ((intmax_t)v, -300);
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_INT32, &i32, &v) == 0);
    PF_ASSERT_ALWAYS_EQ((intmax_t)v, -70000);
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_INT64, &i64, &v) == 0);
    PF_ASSERT_ALWAYS_EQ((intmax_t)v, -5000000000ll);
    PF_ASSERT_ALWAYS(pf_type_int_load(PF_TYPE_LONG, &l, &v) == 0);
    PF_ASSERT_ALWAYS_EQ((intmax_t)v, -42);

    /* Stores are cut to the type. */
    v = 0xFFFFFFFFFFull;
    PF_ASSERT_ALWAYS(pf_type_int_store(PF_TYPE_UINT16, &v, &u16) == 0);
    PF_ASSERT_ALWAYS_EQ(u16, 0xFFFF);
    v = 0x1FF;
    PF_ASSERT_ALWAYS(pf_type_int_store(PF_TYPE_UINT8, &v, &u8) == 0);
    PF_ASSERT_ALWAYS_EQ(u8, 0xFF);
    v = (uintmax_t)(intmax_t)-9;
    PF_ASSERT_ALWAYS(pf_type_int_store(PF_TYPE_INT8, &v, &i8) == 0);
    PF_ASSERT_ALWAYS_EQ(i8, -9);
    v = 0x180; /* low byte 0x80 */
    PF_ASSERT_ALWAYS(pf_type_int_store(PF_TYPE_INT8, &v, &i8) == 0);
    PF_ASSERT_ALWAYS_EQ(i8, -128);
    v = (uintmax_t)(intmax_t)-123456;
    PF_ASSERT_ALWAYS(pf_type_int_store(PF_TYPE_INT32, &v, &i32) == 0);
    PF_ASSERT_ALWAYS_EQ(i32, -123456);
    v = (uintmax_t)(intmax_t)-1;
    PF_ASSERT_ALWAYS(pf_type_int_store(PF_TYPE_INT64, &v, &i64) == 0);
    PF_ASSERT_ALWAYS_EQ(i64, -1);
    return 0;
}

/* Every integer type: a value drawn from the seed comes back. */
static int test_roundtrip(int seed, int rep) {
#define RT(u, l, t, f, i)                                                     \
    {                                                                         \
        t in = (t)(0x5A + (seed + rep) % 100), out = 0;                       \
        uintmax_t v = 0;                                                      \
        PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_##u, &in, &v), PF_OK);   \
        PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_##u, &v, &out), PF_OK); \
        PF_ASSERT_ALWAYS(in == out);                                          \
        in = (t)(pf_type_is_signed(PF_TYPE_##u) ? -(seed % 50) - 1 : seed);   \
        PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_##u, &in, &v), PF_OK);   \
        PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_##u, &v, &out), PF_OK); \
        PF_ASSERT_ALWAYS(in == out);                                          \
    }
    PF_TYPEID(RT, RT, IGN, IGN, IGN)
#undef RT
    return 0;
}

static int test_load_store_errors(int seed, int i) {
    float f = 1.0f;
    void *p = &f;
    char c = 'a';
    uintmax_t v = 1;
    int n = 0;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_FLOAT, &f, &v), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_PTR, &p, &v), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_CHAR, &c, &v), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_ERROR, &f, &v), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_INT, NULL, &v), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_INT, &n, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_FLOAT, &v, &f), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_PTR, &v, &p), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_INT, NULL, &n), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_INT, &v, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(v, 1);
    PF_ASSERT_ALWAYS_EQ(n, 0);
    return 0;
}

/* A store never touches more than the type's own bytes. */
static int test_neighbouring_bytes(int seed, int i) {
    static const int types[] = {
        PF_TYPE_UINT8, PF_TYPE_UINT16, PF_TYPE_UINT32, PF_TYPE_UINT64,
        PF_TYPE_INT8,  PF_TYPE_INT16,  PF_TYPE_INT32,  PF_TYPE_INT64,
    };
    union {
        unsigned char b[8];
        uint64_t align;
    } buf;
    uintmax_t v = UINTMAX_MAX;
    size_t k, j;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(types) / sizeof(types[0]); k++) {
        size_t size = pf_type_size(types[k]);
        memset(buf.b, 0x11, sizeof(buf.b));
        PF_ASSERT_ALWAYS_EQ(pf_type_int_store(types[k], &v, buf.b), PF_OK);
        for (j = 0; j < size; j++)
            PF_ASSERT_ALWAYS_EQ(buf.b[j], 0xFF);
        for (j = size; j < sizeof(buf.b); j++)
            PF_ASSERT_ALWAYS_EQ(buf.b[j], 0x11);
    }
    return 0;
}

static int test_int128(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_STR_EQ(pf_type_name(PF_TYPE_UINT128), "uint128");
#ifdef __SIZEOF_INT128__
    {
        __extension__ typedef __int128 i128;
        __extension__ typedef unsigned __int128 u128;
        i128 s = -7, big;
        u128 u = 12345, ubig;
        uintmax_t v = 0;

        PF_ASSERT_ALWAYS(pf_type_is_integer(PF_TYPE_INT128));
        PF_ASSERT_ALWAYS(pf_type_is_signed(PF_TYPE_INT128));
        PF_ASSERT_ALWAYS(pf_type_is_unsigned(PF_TYPE_UINT128));
        PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_INT128), 16);
        PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_INT128), INTMAX_MAX);
        PF_ASSERT_ALWAYS_EQ(pf_type_int_min(PF_TYPE_INT128), INTMAX_MIN);
        PF_ASSERT_ALWAYS_EQ(pf_type_int_max(PF_TYPE_UINT128), UINTMAX_MAX);
        PF_ASSERT_ALWAYS_EQ(pf_type_int_min(PF_TYPE_UINT128), 0);

        PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_INT128, &s, &v), PF_OK);
        PF_ASSERT_ALWAYS_EQ((intmax_t)v, -7);
        PF_ASSERT_ALWAYS_EQ(pf_type_int_load(PF_TYPE_UINT128, &u, &v), PF_OK);
        PF_ASSERT_ALWAYS_EQ(v, 12345);

        /* Beyond what uintmax_t carries. */
        big = (i128)INTMAX_MAX + 1;
        PF_ASSERT_ALWAYS_EQ(
            pf_type_int_load(PF_TYPE_INT128, &big, &v), PF_ERANGE
        );
        big = (i128)INTMAX_MIN - 1;
        PF_ASSERT_ALWAYS_EQ(
            pf_type_int_load(PF_TYPE_INT128, &big, &v), PF_ERANGE
        );
        ubig = (u128)UINTMAX_MAX + 1;
        PF_ASSERT_ALWAYS_EQ(
            pf_type_int_load(PF_TYPE_UINT128, &ubig, &v), PF_ERANGE
        );

        /* Stores extend by sign or zeros. */
        v = (uintmax_t)(intmax_t)-3;
        PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_INT128, &v, &big), PF_OK);
        PF_ASSERT_ALWAYS(big == -3);
        PF_ASSERT_ALWAYS_EQ(
            pf_type_int_store(PF_TYPE_UINT128, &v, &ubig), PF_OK
        );
        PF_ASSERT_ALWAYS(ubig == (u128)UINTMAX_MAX - 2);
        v = (uintmax_t)INTMAX_MAX;
        PF_ASSERT_ALWAYS_EQ(pf_type_int_store(PF_TYPE_INT128, &v, &big), PF_OK);
        PF_ASSERT_ALWAYS(big == INTMAX_MAX);

        /* No basic or fixed-width type of 16 bytes. */
        PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_INT128), PF_TYPE_ERROR);
        PF_ASSERT_ALWAYS_EQ(
            pf_type_int_as_fixed(PF_TYPE_UINT128), PF_TYPE_ERROR
        );
    }
#else
    PF_ASSERT_ALWAYS(!pf_type_is_integer(PF_TYPE_INT128));
    PF_ASSERT_ALWAYS_EQ(pf_type_size(PF_TYPE_INT128), 0);
#endif
    return 0;
}

/* ---- Equivalent types -------------------------------------------------- */

static int test_as_fixed(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_INT), PF_TYPE_INT32);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_UINT), PF_TYPE_UINT32);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_SCHAR), PF_TYPE_INT8);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_UCHAR), PF_TYPE_UINT8);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_SHRT), PF_TYPE_INT16);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_LLONG), PF_TYPE_INT64);
    PF_ASSERT_ALWAYS_EQ(
        pf_type_int_as_fixed(PF_TYPE_SIZE),
        sizeof(size_t) == 8 ? PF_TYPE_UINT64 : PF_TYPE_UINT32
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_type_int_as_fixed(PF_TYPE_LONG),
        sizeof(long) == 8 ? PF_TYPE_INT64 : PF_TYPE_INT32
    );
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_FLOAT), PF_TYPE_ERROR);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_PTR), PF_TYPE_ERROR);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_fixed(PF_TYPE_CHAR), PF_TYPE_ERROR);
    return 0;
}

static int test_as_base(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_INT32), PF_TYPE_INT);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_UINT32), PF_TYPE_UINT);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_INT16), PF_TYPE_SHRT);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_UINT8), PF_TYPE_UCHAR);
    PF_ASSERT_ALWAYS_EQ(
        pf_type_int_as_base(PF_TYPE_INT64),
        sizeof(long) == 8 ? PF_TYPE_LONG : PF_TYPE_LLONG
    );
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_FLOAT), PF_TYPE_ERROR);
    PF_ASSERT_ALWAYS_EQ(pf_type_int_as_base(PF_TYPE_PTR), PF_TYPE_ERROR);
    return 0;
}

/* Whatever the mapping, size and signedness are kept. */
static int test_equivalent_properties(int seed, int i) {
#define PROP(u, l, t, f, i)                                              \
    {                                                                    \
        int to[2];                                                       \
        int k;                                                           \
        to[0] = pf_type_int_as_fixed(PF_TYPE_##u);                       \
        to[1] = pf_type_int_as_base(PF_TYPE_##u);                        \
        for (k = 0; k < 2; k++) {                                        \
            PF_ASSERT_ALWAYS(pf_type_is_integer(to[k]));                 \
            PF_ASSERT_ALWAYS_EQ(pf_type_size(to[k]), sizeof(t));         \
            PF_ASSERT_ALWAYS_EQ(                                         \
                pf_type_is_signed(to[k]), pf_type_is_signed(PF_TYPE_##u) \
            );                                                           \
        }                                                                \
    }
    (void)seed, (void)i;
    PF_TYPEID(PROP, PROP, IGN, IGN, IGN)
#undef PROP
    return 0;
}

pf_test_t suite_typeid[] = {
    { test_values, "values" },
    { test_unique, "unique" },
    { test_pairs, "pairs" },
    { test_flags, "flags" },
    { test_classes, "classes" },
    { test_names, "names" },
    { test_sizes, "sizes" },
    { test_limits, "limits" },
    { test_load_store, "load_store" },
    { test_roundtrip, "roundtrip" },
    { test_load_store_errors, "load_store_errors" },
    { test_neighbouring_bytes, "neighbouring_bytes" },
    { test_int128, "int128" },
    { test_as_fixed, "as_fixed" },
    { test_as_base, "as_base" },
    { test_equivalent_properties, "equivalent_properties" },
    { 0 },
};
