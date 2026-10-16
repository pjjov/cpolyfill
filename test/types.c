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

#include "../include/pf_types.h"
#include "check.h"
#include <stddef.h>
#include <stdint.h>

struct box {
    char tag;
    double value;
    int count;
    char name[7];
};

static int test_types_alignof(int seed, int rep) {
    CHECK(alignof(char) == 1);
    CHECK(alignof(short) >= sizeof(short) / 2);
    CHECK(alignof(int) >= 1 && alignof(int) <= sizeof(int));
    CHECK(alignof(double) >= 4);
    CHECK(alignof(struct box) >= alignof(double) || alignof(struct box) >= 4);
    CHECK((alignof(int) & (alignof(int) - 1)) == 0); /* power of two */
    CHECK(alignof(max_align_t) >= alignof(long long));
    CHECK(alignof(max_align_t) >= alignof(void *));
    return 0;
}

static int test_types_max_align(int seed, int rep) {
    CHECK(sizeof(pf_max_align_t) >= sizeof(long long));
    CHECK(alignof(pf_max_align_t) >= alignof(long long));
    CHECK(alignof(pf_max_align_t) >= alignof(double));
    return 0;
}

#ifndef PF_NO_ALIGNAS
static int test_types_alignas(int seed, int rep) {
    alignas(16) char a[32];
    alignas(64) int b;

    CHECK(((uintptr_t)a & 15) == 0);
    CHECK(((uintptr_t)&b & 63) == 0);
    return 0;
}
#endif

#ifndef PF_NO_TYPEOF
static int test_types_typeof(int seed, int rep) {
    int i = 3;
    const int c = 4;
    typeof(i) j = 5;
    typeof(c * 2.0) d = 1.5;
    typeof(&i) p = &i;
    typeof(struct box) b;

    CHECK(sizeof(j) == sizeof(int));
    CHECK(sizeof(d) == sizeof(double));
    CHECK(*p == 3);
    CHECK(sizeof(b) == sizeof(struct box));

    j += 1;
    CHECK(j == 6);
    return 0;
}
#endif

#ifndef PF_NO_TYPEOF_UNQUAL
static int test_types_typeof_unqual(int seed, int rep) {
    const int c = 4;
    typeof_unqual(c) v = c;

    v++; /* does not compile if `v` is const */
    CHECK(v == 5);
    return 0;
}
#endif

static int test_types_offsetof(int seed, int rep) {
    CHECK(offsetof(struct box, tag) == 0);
    CHECK(offsetof(struct box, value) >= 1);
    CHECK(offsetof(struct box, count) > offsetof(struct box, value));
    CHECK(offsetof(struct box, name) > offsetof(struct box, count));
    CHECK(offsetof(struct box, name) + 7 <= sizeof(struct box));

    {
        struct box b;
        CHECK(
            (char *)&b.count - (char *)&b
            == (ptrdiff_t)offsetof(struct box, count)
        );
    }
    return 0;
}

#ifndef PF_NO_CONTAINEROF
static int test_types_container_of(int seed, int rep) {
    struct box b;
    const struct box cb = { 0 };

    CHECK(pf_container_of(&b.tag, struct box, tag) == &b);
    CHECK(pf_container_of(&b.value, struct box, value) == &b);
    CHECK(pf_container_of(&b.count, struct box, count) == &b);
    CHECK(pf_container_of(&b.name, struct box, name) == &b);
    CHECK(pf_container_of(&cb.count, struct box, count) == &cb);
    return 0;
}
#endif

#ifndef PF_NO_IS_CONST
static int test_types_is_const(int seed, int rep) {
    int a = 0;
    const int b = 0;
    const char *s = "";

    CHECK(!pf_is_const(a));
    CHECK(pf_is_const(b));
    CHECK(!pf_is_const(int));
    CHECK(pf_is_const(const int));
    CHECK(!pf_is_const(s)); /* the pointer itself is not const */
    CHECK(pf_is_const(*s));
    (void)a;
    return 0;
}

static int test_types_if_const(int seed, int rep) {
    const int c = 0;
    int n = 0;

    CHECK(pf_if_const(c, 10, 20) == 10);
    CHECK(pf_if_const(n, 10, 20) == 20);
    CHECK(pf_if_not_const(c, 10, 20) == 20);
    CHECK(pf_if_not_const(n, 10, 20) == 10);
    (void)n;
    return 0;
}
#endif

#ifndef PF_NO_IS_TYPE
static int test_types_is_type(int seed, int rep) {
    int i = 0;
    unsigned u = 0;
    char *p = NULL;
    double d = 0;

    CHECK(pf_is_type(int, i));
    CHECK(!pf_is_type(int, u));
    CHECK(pf_is_type(unsigned, u));
    CHECK(pf_is_type(char *, p));
    CHECK(!pf_is_type(void *, p));
    CHECK(pf_is_type(double, d));
    CHECK(!pf_is_type(float, d));
    CHECK(pf_is_type(int, 5));
    CHECK(pf_is_type(double, 5.0));
    (void)i, (void)u, (void)p, (void)d;
    return 0;
}

#endif

#ifndef PF_NO_IS_TYPE
static int test_types_if_type(int seed, int rep) {
    int i = 0;

    CHECK(pf_if_type(int, i, 10, 20) == 10);
    CHECK(pf_if_type(long, i, 10, 20) == 20);
    CHECK(pf_if_not_type(int, i, 10, 20) == 20);
    CHECK(pf_if_not_type(long, i, 10, 20) == 10);
    (void)i;
    return 0;
}
#endif

#ifndef PF_NO_CHECK_TYPE
static int test_types_check_type(int seed, int rep) {
    int i = 42;
    char *p = "text";

    /* The macro is the identity for matching types. */
    CHECK(pf_check_type(int, i) == 42);
    CHECK(pf_check_type(int, 7) == 7);
    CHECK(pf_check_type(char *, p) == p);
    return 0;
}
#endif

static int test_types_align_math(int seed, int rep) {
    CHECK(PF_ALIGN_UP(0, 8) == 0);
    CHECK(PF_ALIGN_UP(1, 8) == 8);
    CHECK(PF_ALIGN_UP(7, 8) == 8);
    CHECK(PF_ALIGN_UP(8, 8) == 8);
    CHECK(PF_ALIGN_UP(9, 8) == 16);
    CHECK(PF_ALIGN_UP(13, 1) == 13);
    CHECK(PF_ALIGN_UP(13, 2) == 14);
    CHECK(PF_ALIGN_UP(100, 64) == 128);

    CHECK(PF_ALIGN_DOWN(0, 8) == 0);
    CHECK(PF_ALIGN_DOWN(1, 8) == 0);
    CHECK(PF_ALIGN_DOWN(7, 8) == 0);
    CHECK(PF_ALIGN_DOWN(8, 8) == 8);
    CHECK(PF_ALIGN_DOWN(15, 8) == 8);
    CHECK(PF_ALIGN_DOWN(100, 64) == 64);

    CHECK(PF_ALIGN_PAD(0, 8) == 0);
    CHECK(PF_ALIGN_PAD(1, 8) == 7);
    CHECK(PF_ALIGN_PAD(7, 8) == 1);
    CHECK(PF_ALIGN_PAD(8, 8) == 0);
    CHECK(PF_ALIGN_PAD(13, 8) == 3);

    CHECK(PF_ALIGN_CEIL(9, 4) == PF_ALIGN_UP(9, 4));
    CHECK(PF_ALIGN_FLOOR(9, 4) == PF_ALIGN_DOWN(9, 4));
    return 0;
}

/* The result keeps the type of the first operand, whatever its width. */
static int test_types_align_math_wide(int seed, int rep) {
    unsigned long long big = 0x100000001ull;
    size_t s = 4097;
    uint8_t small = 200;

    CHECK(PF_ALIGN_UP(big, 0x1000) == 0x100001000ull);
    CHECK(PF_ALIGN_DOWN(big, 0x1000) == 0x100000000ull);
    CHECK(PF_ALIGN_UP(s, 4096) == 8192);
    CHECK(PF_ALIGN_DOWN(s, 4096) == 4096);
    CHECK(PF_ALIGN_UP(small, 16) == 208);
    CHECK(PF_ALIGN_DOWN(small, 16) == 192);
    return 0;
}

static int test_types_align_math_random(int seed, int rep) {
    unsigned state = (unsigned)seed + (unsigned)rep * 7919u;
    int i;

    for (i = 0; i < 1000; i++) {
        unsigned x, a;
        state = state * 1664525u + 1013904223u;
        x = (state >> 8) & 0xFFFFu;
        state = state * 1664525u + 1013904223u;
        a = 1u << ((state >> 8) % 12);

        CHECK(PF_ALIGN_UP(x, a) >= x);
        CHECK(PF_ALIGN_UP(x, a) - x < a);
        CHECK(PF_ALIGN_UP(x, a) % a == 0);
        CHECK(PF_ALIGN_DOWN(x, a) <= x);
        CHECK(x - PF_ALIGN_DOWN(x, a) < a);
        CHECK(PF_ALIGN_DOWN(x, a) % a == 0);
        CHECK(x + PF_ALIGN_PAD(x, a) == PF_ALIGN_UP(x, a));
    }
    return 0;
}

pf_test_t suite_types[] = {
    { test_types_alignof, "/types/alignof" },
    { test_types_max_align, "/types/max_align" },
#ifndef PF_NO_ALIGNAS
    { test_types_alignas, "/types/alignas" },
#endif
#ifndef PF_NO_TYPEOF
    { test_types_typeof, "/types/typeof" },
#endif
#ifndef PF_NO_TYPEOF_UNQUAL
    { test_types_typeof_unqual, "/types/typeof_unqual" },
#endif
    { test_types_offsetof, "/types/offsetof" },
#ifndef PF_NO_CONTAINEROF
    { test_types_container_of, "/types/container_of" },
#endif
#ifndef PF_NO_IS_CONST
    { test_types_is_const, "/types/is_const" },
    { test_types_if_const, "/types/if_const" },
#endif
#ifndef PF_NO_IS_TYPE
    { test_types_is_type, "/types/is_type" },
    { test_types_if_type, "/types/if_type" },
#endif
#ifndef PF_NO_CHECK_TYPE
    { test_types_check_type, "/types/check_type" },
#endif
    { test_types_align_math, "/types/align_math" },
    { test_types_align_math_wide, "/types/align_math_wide" },
    { test_types_align_math_random, "/types/align_math_random" },
    { 0 },
};
