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
#include "../include/pf_macro.h"
#include "../include/pf_test.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

int test_macro_math(int seed, int rep) {
    pf_assert(PF_ABS(0) == 0);
    pf_assert(PF_ABS(1) == 1);
    pf_assert(PF_ABS(-1) == 1);
    pf_assert(PF_ABS(INT_MAX) == INT_MAX);
    pf_assert(PF_ABS(INT_MIN) == INT_MIN);
    pf_assert(PF_MIN(0, 1) == 0);
    pf_assert(PF_MIN(1, 1) == 1);
    pf_assert(PF_MIN(INT_MIN, LLONG_MIN) == LLONG_MIN);
    pf_assert(PF_MAX(0, 1) == 1);
    pf_assert(PF_MAX(1, 1) == 1);
    pf_assert(PF_MAX(INT_MAX, LLONG_MAX) == LLONG_MAX);

    return 0;
}

int test_macro_countof(int seed, int rep) {
    int i[1], j[2], k[3], *l[3];

    pf_assert(PF_COUNTOF(i) == 1);
    pf_assert(PF_COUNTOF(j) == 2);
    pf_assert(PF_COUNTOF(k) == 3);
    pf_assert(PF_COUNTOF(l) == 3);

    return 0;
}

int test_macro_math_types(int seed, int rep) {
    unsigned u = 3, v = 5;
    double d = -2.5;

    pf_assert(PF_MAX(1, 2.5) == 2.5);
    pf_assert(PF_MIN(1, 2.5) == 1);
    pf_assert(PF_MAX(-1, -2) == -1);
    pf_assert(PF_MIN(-1, -2) == -2);
    pf_assert(PF_ABS(d) == 2.5);
    pf_assert(PF_ABS(-7L) == 7L);
    pf_assert(PF_ABS(7LL) == 7LL);

    /* Differences never wrap around, whatever the order or the signedness. */
    pf_assert(PF_DIFF(u, v) == 2u);
    pf_assert(PF_DIFF(v, u) == 2u);
    pf_assert(PF_DIFF(5, 3) == 2);
    pf_assert(PF_DIFF(-5, 3) == 8);
    pf_assert(PF_DIFF(3, -5) == 8);
    pf_assert(PF_DIFF(4, 4) == 0);
    pf_assert(PF_DIFF(0.5, 2.0) == 1.5);

    /* Nested use. */
    pf_assert(PF_MAX(PF_MAX(1, 2), 3) == 3);
    pf_assert(PF_MAX(3, PF_MAX(1, 2)) == 3);
    pf_assert(PF_MIN(PF_MIN(5, 2), PF_MAX(1, 9)) == 2);
    pf_assert(PF_MIN(PF_ABS(-4), PF_ABS(3)) == 3);
    return 0;
}

#ifdef PF_HAS_STMT_EXPR
int test_macro_single_evaluation(int seed, int rep) {
    int i = 0, j = 10;

    pf_assert(PF_MAX(i++, 5) == 5);
    pf_assert(i == 1);
    pf_assert(PF_MIN(j--, 20) == 10);
    pf_assert(j == 9);
    pf_assert(PF_ABS(j--) == 9);
    pf_assert(j == 8);
    pf_assert(PF_DIFF(i++, j--) == 7);
    pf_assert(i == 2 && j == 7);
    return 0;
}
#endif

int test_macro_clamp(int seed, int rep) {
    pf_assert(PF_CLAMP(5, 0, 10) == 5);
    pf_assert(PF_CLAMP(-5, 0, 10) == 0);
    pf_assert(PF_CLAMP(15, 0, 10) == 10);
    pf_assert(PF_CLAMP(0, 0, 10) == 0);
    pf_assert(PF_CLAMP(10, 0, 10) == 10);
    pf_assert(PF_CLAMP(2.5, 1.0, 2.0) == 2.0);
    pf_assert(PF_CLAMP(7, 7, 7) == 7);
    pf_assert(PF_CLAMP(INT_MIN, -3, 3) == -3);
    pf_assert(PF_CLAMP(INT_MAX, -3, 3) == 3);
    return 0;
}

#if defined(__GNUC__) || defined(__clang__)
int test_macro_swap(int seed, int rep) {
    int a = 1, b = 2;
    double x = 1.5, y = -2.5;
    char *p = "first", *q = "second";

    PF_SWAP(a, b);
    pf_assert(a == 2 && b == 1);
    PF_SWAP(x, y);
    pf_assert(x == -2.5 && y == 1.5);
    PF_SWAP(p, q);
    pf_assert(strcmp(p, "second") == 0 && strcmp(q, "first") == 0);

    /* Swapping with oneself changes nothing. */
    PF_SWAP(a, a);
    pf_assert(a == 2);
    return 0;
}
#endif

int test_macro_pointers(int seed, int rep) {
    char buf[64];
    int *ip = (int *)buf;

    pf_assert(PF_OFFSET(buf, 0) == (void *)buf);
    pf_assert(PF_OFFSET(buf, 10) == (void *)(buf + 10));
    pf_assert(PF_OFFSET(ip, 4) == (void *)(buf + 4)); /* always in bytes */
    pf_assert(PF_PTRDIFF(buf + 10, buf) == 10);
    pf_assert(PF_PTRDIFF(buf, buf + 10) == -10);
    pf_assert(PF_PTRDIFF(ip + 2, ip) == 2 * (long)sizeof(int));
    pf_assert(PF_PTRDIFF(buf, buf) == 0);
    return 0;
}

int test_macro_alignment(int seed, int rep) {
    pf_assert(PF_IS_ALIGNED(0, 8));
    pf_assert(PF_IS_ALIGNED(8, 8));
    pf_assert(PF_IS_ALIGNED(64, 16));
    pf_assert(!PF_IS_ALIGNED(1, 8));
    pf_assert(!PF_IS_ALIGNED(12, 8));
    pf_assert(PF_IS_ALIGNED(7, 1));

    pf_assert(PF_ALIGN_UP(0, 8) == 0);
    pf_assert(PF_ALIGN_UP(1, 8) == 8);
    pf_assert(PF_ALIGN_UP(8, 8) == 8);
    pf_assert(PF_ALIGN_UP(9, 8) == 16);
    pf_assert(PF_ALIGN_DOWN(0, 8) == 0);
    pf_assert(PF_ALIGN_DOWN(7, 8) == 0);
    pf_assert(PF_ALIGN_DOWN(8, 8) == 8);
    pf_assert(PF_ALIGN_DOWN(15, 8) == 8);
    pf_assert(PF_ALIGN_PAD(0, 8) == 0);
    pf_assert(PF_ALIGN_PAD(1, 8) == 7);
    pf_assert(PF_ALIGN_PAD(8, 8) == 0);
    pf_assert(PF_ALIGN_CEIL(5, 4) == 8);
    pf_assert(PF_ALIGN_FLOOR(5, 4) == 4);

    /* Aligned results stay aligned. */
    for (unsigned x = 0; x < 300; x++) {
        pf_assert(PF_IS_ALIGNED(PF_ALIGN_UP(x, 16u), 16));
        pf_assert(PF_IS_ALIGNED(PF_ALIGN_DOWN(x, 16u), 16));
        pf_assert(PF_ALIGN_UP(x, 16u) - x == PF_ALIGN_PAD(x, 16u));
    }
    return 0;
}

int test_macro_flags(int seed, int rep) {
    unsigned f = 0;

    pf_assert(PF_FLAG_SET(f, 0x4) == 0x4);
    f = 0x5;
    pf_assert(PF_FLAG_SET(f, 0x2) == 0x7);
    pf_assert(PF_FLAG_CLEAR(f, 0x1) == 0x4);
    pf_assert(PF_FLAG_CLEAR(f, 0x2) == 0x5);
    pf_assert(PF_FLAG_TOGGLE(f, 0x3) == 0x6);
    pf_assert(PF_FLAG_TOGGLE(f, 0x0) == 0x5);
    pf_assert(PF_FLAG_TEST(f, 0x4));
    pf_assert(PF_FLAG_TEST(f, 0x5) == 0x5);
    pf_assert(!PF_FLAG_TEST(f, 0x2));

    /* The macros keep the width of the value: no int promotion surprises. */
    {
        unsigned char c = 0xFF;
        unsigned long long big = 0xF0F0F0F0F0F0F0F0ull;

        pf_assert(PF_FLAG_CLEAR(c, 0x0F) == 0xF0);
        pf_assert(PF_FLAG_CLEAR(big, 0xF000000000000000ull)
                  == 0x00F0F0F0F0F0F0F0ull);
        pf_assert(PF_FLAG_TOGGLE(big, 0xFFFFFFFFFFFFFFFFull)
                  == 0x0F0F0F0F0F0F0F0Full);
    }
    return 0;
}

int test_macro_bits(int seed, int rep) {
    pf_assert(PF_BIT(unsigned, 0) == 1u);
    pf_assert(PF_BIT(unsigned, 5) == 32u);
    pf_assert(PF_BIT(unsigned long long, 40) == 1ull << 40);
    pf_assert(PF_BIT(unsigned char, 7) == 0x80);

    pf_assert(PF_MASK(unsigned, 0) == 0u);
    pf_assert(PF_MASK(unsigned, 1) == 1u);
    pf_assert(PF_MASK(unsigned, 8) == 0xFFu);
    pf_assert(PF_MASK(unsigned, 31) == 0x7FFFFFFFu);
    pf_assert(PF_MASK(unsigned long long, 40) == 0xFFFFFFFFFFull);

    {
        unsigned x = 0;
        unsigned long long y = 0;

        pf_assert(PF_BIT_SET(x, 3) == 8u);
        pf_assert(PF_BIT_SET(y, 63) == 1ull << 63);
        x = 0xFF;
        pf_assert(PF_BIT_CLEAR(x, 0) == 0xFEu);
        pf_assert(PF_BIT_CLEAR(x, 7) == 0x7Fu);
        pf_assert(PF_BIT_CLEAR(x, 8) == 0xFFu);
        pf_assert(PF_BIT_TOGGLE(x, 0) == 0xFEu);
        pf_assert(PF_BIT_TOGGLE(x, 8) == 0x1FFu);
        pf_assert(PF_BIT_TEST(x, 3));
        pf_assert(!PF_BIT_TEST(x, 8));

        y = 1ull << 40;
        pf_assert(PF_BIT_TEST(y, 40));
        pf_assert(!PF_BIT_TEST(y, 39));
        pf_assert(PF_BIT_CLEAR(y, 40) == 0);
    }
    return 0;
}

int test_macro_container_of(int seed, int rep) {
    struct outer {
        char tag;
        int inner;
        double other;
    } o;
    const struct outer co = { 0 };

    pf_assert(pf_container_of(&o.inner, struct outer, inner) == &o);
    pf_assert(pf_container_of(&o.other, struct outer, other) == &o);
    pf_assert(pf_container_of(&o.tag, struct outer, tag) == &o);
    pf_assert(pf_container_of(&co.inner, struct outer, inner) == &co);
    return 0;
}

int test_macro_countof_more(int seed, int rep) {
    int matrix[3][4];
    struct s {
        int a;
    } list[7];
    char text[] = "hello";
    static const double table[] = { 1, 2, 3, 4, 5 };

    pf_assert(PF_COUNTOF(matrix) == 3);
    pf_assert(PF_COUNTOF(matrix[0]) == 4);
    pf_assert(PF_COUNTOF(list) == 7);
    pf_assert(PF_COUNTOF(text) == 6); /* with the terminator */
    pf_assert(PF_COUNTOF(table) == 5);

    /* A constant expression. */
    {
        char array[PF_COUNTOF(table)];
        pf_assert(sizeof array == 5);
    }
    return 0;
}

enum guarded {
    GUARDED_A,
    GUARDED_B,
    GUARDED_C,
    guarded_sentinel,
};

/* Fails to compile when the enum gets a new member. It needs a real
   `static_assert`, which only C11 and later have. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
PF_ENUM_GUARD(guarded, 3)
#endif

int test_macro_enum_guard(int seed, int rep) {
    pf_assert(guarded_sentinel == 3);
    return 0;
}

int test_macro_hints(int seed, int rep) {
    volatile int v = seed;

    /* Hints do not change the value. */
    pf_assert(PF_EXPECT(v == v, 1));
    pf_assert(!PF_EXPECT(v != v, 0));
    pf_assert(PF_EXPECT(v != v + 1, 1) == 1);
    pf_assert(PF_EXPECT(5, 1) == 1); /* normalised to 0 or 1 */
    pf_assert(PF_EXPECT(0, 1) == 0);
    pf_assert(PF_EXPECT_P(v == v, 1, 0.9));

    if (seed == INT_MIN && rep == INT_MIN)
        PF_UNREACHABLE;

    {
        char buf[64] __attribute__((aligned(16)));
        pf_assert(PF_ASSUME_ALIGNED(buf, 16) == (void *)buf);
    }
    return 0;
}

pf_test_t suite_macro[] = {
    { test_macro_math, "/macro/math" },
    { test_macro_countof, "/macro/countof" },
    { test_macro_math_types, "/macro/math_types" },
#ifdef PF_HAS_STMT_EXPR
    { test_macro_single_evaluation, "/macro/single_evaluation" },
#endif
    { test_macro_clamp, "/macro/clamp" },
#if defined(__GNUC__) || defined(__clang__)
    { test_macro_swap, "/macro/swap" },
#endif
    { test_macro_pointers, "/macro/pointers" },
    { test_macro_alignment, "/macro/alignment" },
    { test_macro_flags, "/macro/flags" },
    { test_macro_bits, "/macro/bits" },
    { test_macro_container_of, "/macro/container_of" },
    { test_macro_countof_more, "/macro/countof_more" },
    { test_macro_enum_guard, "/macro/enum_guard" },
    { test_macro_hints, "/macro/hints" },
    { 0 },
};
