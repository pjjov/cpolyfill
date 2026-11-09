/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/macro.h>

#include <stdint.h>

#include "check.h"

static int test_arithmetic(int seed, int i) {
    int a = 3, b = -7, n = 0;
    unsigned u = 5;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(PF_MIN(a, b), -7);
    PF_ASSERT_ALWAYS_EQ(PF_MAX(a, b), 3);
    PF_ASSERT_ALWAYS_EQ(PF_MIN(PF_MIN(a, b), PF_MAX(a, 10)), -7);
    PF_ASSERT_ALWAYS_EQ((int)(PF_MAX(2.5, 1.0) * 2), 5);
    PF_ASSERT_ALWAYS_EQ(PF_ABS(b), 7);
    PF_ASSERT_ALWAYS_EQ(PF_ABS(a), 3);
    PF_ASSERT_ALWAYS_EQ(PF_ABS(u), 5);
    PF_ASSERT_ALWAYS_EQ(PF_DIFF(a, b), 10);
    PF_ASSERT_ALWAYS_EQ(PF_DIFF(b, a), 10);
    PF_ASSERT_ALWAYS_EQ(PF_DIFF(2u, 7u), 5);
    PF_ASSERT_ALWAYS_EQ(PF_CLAMP(15, 0, 10), 10);
    PF_ASSERT_ALWAYS_EQ(PF_CLAMP(-5, 0, 10), 0);
    PF_ASSERT_ALWAYS_EQ(PF_CLAMP(5, 0, 10), 5);

#if PF_CC_GNU || PF_CC_CLANG
    /* Each argument once. */
    PF_ASSERT_ALWAYS_EQ(PF_MAX(n++, -1), 0);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(PF_ABS(--n - 1), 1);
    PF_ASSERT_ALWAYS_EQ(n, 0);
#endif
    (void)n;
    return 0;
}

struct pair {
    int a;
    char b[3];
};

static int test_swap(int seed, int i) {
    int x = 1, y = 2;
    const char *s = "s", *t = "t";
    struct pair p = { 1, "ab" }, q = { 2, "cd" };
    int arr[2] = { 5, 6 }, k = 0;
    (void)seed, (void)i;

    PF_SWAP(x, y);
    PF_ASSERT_ALWAYS_EQ(x, 2);
    PF_ASSERT_ALWAYS_EQ(y, 1);
    PF_SWAP(s, t);
    PF_ASSERT_ALWAYS_STR_EQ(s, "t");
    PF_SWAP(p, q);
    PF_ASSERT_ALWAYS_EQ(p.a, 2);
    PF_ASSERT_ALWAYS_STR_EQ(q.b, "ab");
    PF_SWAP(arr[k], arr[k + 1]);
    PF_ASSERT_ALWAYS_EQ(arr[0], 6);
    PF_ASSERT_ALWAYS_EQ(arr[1], 5);

    /* As the body of an if without braces. */
    if (x)
        PF_SWAP(x, y);
    else
        PF_SWAP(y, x);
    PF_ASSERT_ALWAYS_EQ(x, 1);
    return 0;
}

static int test_alignment(int seed, int i) {
    char buf[16];
    uint64_t big = 0x100000007ULL;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(PF_OFFSET(buf, 5) == (void *)&buf[5]);
    PF_ASSERT_ALWAYS_EQ(PF_PTRDIFF(&buf[7], &buf[2]), 5);
    PF_ASSERT_ALWAYS_EQ(PF_PTRDIFF(&buf[2], &buf[7]), -5);

    PF_ASSERT_ALWAYS(PF_IS_ALIGNED(16, 8));
    PF_ASSERT_ALWAYS(!PF_IS_ALIGNED(12, 8));
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_PAD(13, 8), 3);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_PAD(16, 8), 0);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_UP(13, 8), 16);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_UP(16, 8), 16);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_DOWN(13, 8), 8);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_UP(0, 4096), 0);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_UP((size_t)1, (size_t)4096), 4096);

    /* A 64-bit value with a 32-bit unsigned alignment: the high bits must
       survive (a mask built as ~(align - 1) would be 32 bits wide). */
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_DOWN(big, 8u), 0x100000000ULL);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_UP(big, 8u), 0x100000008ULL);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_PAD(big, 8u), 1);
    PF_ASSERT_ALWAYS(!PF_IS_ALIGNED(big, 8u));
    PF_ASSERT_ALWAYS(PF_IS_ALIGNED((uintptr_t)&big, PF_ALIGNOF(uint64_t)));
    return 0;
}

static int test_container_of(int seed, int i) {
    struct pair p;
    char (*member)[3] = &p.b;
    int *first = &p.a;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(PF_CONTAINER_OF(member, struct pair, b) == &p);
    PF_ASSERT_ALWAYS(PF_CONTAINER_OF(first, struct pair, a) == &p);
    PF_ASSERT_ALWAYS(PF_CONTAINER_OF(&p.a, struct pair, a) == &p);
    return 0;
}

static int test_flags(int seed, int i) {
    unsigned char c = 0xF0;
    uint64_t w = 0xFFFFFFFF00000003ULL;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(PF_FLAG_SET(0x10, 0x01), 0x11);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_CLEAR(0x11, 0x01), 0x10);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_TOGGLE(0x11, 0x03), 0x12);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_TEST(0x11, 0x01), 1);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_TEST(0x11, 0x06), 0);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_TEST(0x11, 0x03), 1);

    /* Clearing with a narrower unsigned flag keeps the high bits. */
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_CLEAR(w, 0x1u), 0xFFFFFFFF00000002ULL);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_TEST(w, (uint64_t)1 << 40), 1);
    PF_ASSERT_ALWAYS_EQ((unsigned char)PF_FLAG_CLEAR(c, 0x30), 0xC0);

    PF_ASSERT_ALWAYS_EQ(PF_BIT(uint64_t, 63), 0x8000000000000000ULL);
    PF_ASSERT_ALWAYS_EQ(PF_BIT(unsigned char, 7), 0x80);
    PF_ASSERT_ALWAYS_EQ(PF_MASK(unsigned, 0), 0);
    PF_ASSERT_ALWAYS_EQ(PF_MASK(unsigned, 4), 0xF);
    PF_ASSERT_ALWAYS_EQ(PF_MASK(unsigned char, 8), 0xFF);
    PF_ASSERT_ALWAYS_EQ(PF_MASK(uint64_t, 64), UINT64_MAX);
    PF_ASSERT_ALWAYS_EQ(PF_MASK(uint64_t, 33), 0x1FFFFFFFFULL);

    PF_ASSERT_ALWAYS_EQ(PF_BIT_SET(w, 40), w | 0x10000000000ULL);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_SET(0u, 3), 8);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_CLEAR(w, 63), 0x7FFFFFFF00000003ULL);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_CLEAR(w, 0), 0xFFFFFFFF00000002ULL);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_TOGGLE(w, 1), 0xFFFFFFFF00000001ULL);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_TEST(w, 63), 1);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_TEST(w, 2), 0);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_TEST(c, 7), 1);
    return 0;
}

enum color { color_red, color_green, color_blue, color_sentinel };

static const char *color_name(enum color c) {
    PF_ENUM_GUARD(color, 3);
    switch (c) {
    case color_red:
        return "red";
    case color_green:
        return "green";
    case color_blue:
        return "blue";
    default:
        return "?";
    }
}

static int test_countof(int seed, int i) {
    int seven[7];
    char matrix[3][5];
    struct pair pairs[4];
    static const char *names[] = { "a", "b" };
    char sized[PF_COUNTOF(seven)]; /* a constant expression */
    (void)seed, (void)i, (void)seven, (void)matrix, (void)pairs;

    PF_ASSERT_ALWAYS_EQ(PF_COUNTOF(seven), 7);
    PF_ASSERT_ALWAYS_EQ(PF_COUNTOF(matrix), 3);
    PF_ASSERT_ALWAYS_EQ(PF_COUNTOF(matrix[0]), 5);
    PF_ASSERT_ALWAYS_EQ(PF_COUNTOF(pairs), 4);
    PF_ASSERT_ALWAYS_EQ(PF_COUNTOF(names), 2);
    PF_ASSERT_ALWAYS_EQ(sizeof(sized), 7);
    PF_ASSERT_ALWAYS_STR_EQ(color_name(color_blue), "blue");
    return 0;
}

static int test_hints(int seed, int i) {
    int n = seed | 1;
    int buf[4] = { 1, 2, 3, 4 };
    int *p;
    (void)i;

    PF_ASSERT_ALWAYS_EQ(PF_EXPECT(n, 1), 1);
    PF_ASSERT_ALWAYS_EQ(PF_EXPECT(n - n, 0), 0);
    PF_ASSERT_ALWAYS_EQ(PF_EXPECT_P(n, 1, 0.9), 1);
    PF_ASSERT_ALWAYS(PF_LIKELY(n != 0));
    PF_ASSERT_ALWAYS(!PF_UNLIKELY(n == 0));
    if (PF_UNLIKELY(n == 0))
        return 1;

    PF_ASSUME(n != 0);
    PF_ASSERT_ALWAYS_NE(n, 0);

    p = (int *)PF_ASSUME_ALIGNED(buf, PF_ALIGNOF(int));
    PF_ASSERT_ALWAYS(p == buf);
    PF_ASSERT_ALWAYS_EQ(p[3], 4);
    return 0;
}

#define TEST_VALUE 42
#define TEST_NAME value

static int test_preprocessor(int seed, int i) {
    int PF_CONCAT(TEST_NAME, _1) = 1;                 /* value_1 */
    PF_UNUSED int PF_UNIQUE(u) = 1, PF_UNIQUE(u) = 2; /* distinct names */
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_STR_EQ(PF_STRINGIFY(TEST_VALUE), "42");
    PF_ASSERT_ALWAYS_STR_EQ(PF_STRINGIFY(a, b), "a, b");
    PF_ASSERT_ALWAYS_EQ(value_1, 1);

    PF_ASSERT_ALWAYS_EQ(PF_NARGS(a), 1);
    PF_ASSERT_ALWAYS_EQ(PF_NARGS(a, b, c), 3);
    PF_ASSERT_ALWAYS_EQ(PF_NARGS((a, b), c), 2);
    PF_ASSERT_ALWAYS_EQ(
        PF_NARGS(
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
            10,
            11,
            12,
            13,
            14,
            15,
            16,
            17,
            18,
            19,
            20,
            21,
            22,
            23,
            24,
            25,
            26,
            27,
            28,
            29,
            30,
            31,
            32,
            33,
            34,
            35,
            36,
            37,
            38,
            39,
            40,
            41,
            42,
            43,
            44,
            45,
            46,
            47,
            48,
            49,
            50,
            51,
            52,
            53,
            54,
            55,
            56,
            57,
            58,
            59,
            60,
            61,
            62,
            63,
            64
        ),
        64
    );
#if PF_STD_C23 || PF_CPP >= 202002L
    PF_ASSERT_ALWAYS_EQ(PF_NARGS(), 0);
#endif
    return 0;
}

pf_test_t suite_macro[] = {
    { test_arithmetic, "arithmetic" },
    { test_swap, "swap" },
    { test_alignment, "alignment" },
    { test_container_of, "container_of" },
    { test_flags, "flags" },
    { test_countof, "countof" },
    { test_hints, "hints" },
    { test_preprocessor, "preprocessor" },
    { 0 },
};
