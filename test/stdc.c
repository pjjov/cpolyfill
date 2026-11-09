/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/stdc.h>

#include <stdint.h>

#include "check.h"

/* At file scope, with and without a message, twice on one line. */
PF_STATIC_ASSERT(sizeof(char) == 1);
PF_STATIC_ASSERT(sizeof(int) >= 2, "int has at least 16 bits");
PF_STATIC_ASSERT(1);
PF_STATIC_ASSERT(2 > 1);

/* An expression with commas between parentheses. */
PF_STATIC_ASSERT(sizeof(int[2]) == 2 * sizeof(int), "array");

struct aligned_member {
    char c;
    PF_ALIGNAS(16) char buf[3];
};

static int test_typeof(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_TYPEOF
    {
        int x = 3;
        PF_TYPEOF(x) y = x + 1;
        PF_TYPEOF(int) z = 5;
        PF_TYPEOF(int (*)(int, int)) fn = 0;
        PF_ASSERT_ALWAYS_EQ(y, 4);
        PF_ASSERT_ALWAYS_EQ(z, 5);
        PF_ASSERT_ALWAYS(fn == 0);
        PF_ASSERT_ALWAYS_EQ(sizeof(PF_TYPEOF((char)0)), 1);
    }
#endif
#if PF_HAS_TYPEOF_UNQUAL
    {
        const volatile int c = 7;
        PF_TYPEOF_UNQUAL(c) u = c;
        u += 1; /* not const */
        PF_ASSERT_ALWAYS_EQ(u, 8);
    }
#endif
    return 0;
}

static int test_alignment(int seed, int i) {
    PF_ALIGNAS(32) char buf32[3];
    PF_ALIGNAS(double) char buf_double[sizeof(double)];
    struct aligned_member m;
    struct {
        char c;
        double d;
    } probe;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(PF_ALIGNOF(char), 1);
    /* The ABI alignment, as a struct member gets (_Alignof, not the
       preferred __alignof__). */
    PF_ASSERT_ALWAYS_EQ(
        PF_ALIGNOF(double), (size_t)((char *)&probe.d - (char *)&probe)
    );

    PF_ASSERT_ALWAYS_EQ((uintptr_t)buf32 % 32, 0);
    PF_ASSERT_ALWAYS_EQ((uintptr_t)buf_double % PF_ALIGNOF(double), 0);
    PF_ASSERT_ALWAYS_EQ((uintptr_t)m.buf % 16, 0);
    PF_ASSERT_ALWAYS_EQ(sizeof(struct aligned_member), 32);
    (void)buf32, (void)buf_double, (void)m;
    return 0;
}

static int test_static_assert_block(int seed, int i) {
    (void)seed, (void)i;
    PF_STATIC_ASSERT(sizeof(long) >= sizeof(int));
    PF_STATIC_ASSERT(sizeof(long) >= sizeof(int), "twice in one block");
    return 0;
}

#if PF_HAS_THREAD_LOCAL
static PF_THREAD_LOCAL int tls_counter = 10;
#endif

static int test_thread_local(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_THREAD_LOCAL
    tls_counter++;
    PF_ASSERT_ALWAYS_EQ(tls_counter, 11);
    tls_counter = 10;
#endif
    return 0;
}

static int classify(int x) {
    switch (x & 1) {
    case 0:
        return 0;
    case 1:
        return 1;
    default:
        PF_UNREACHABLE();
    }
    return -1;
}

static int test_unreachable(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(classify(4), 0);
    PF_ASSERT_ALWAYS_EQ(classify(7), 1);
    return 0;
}

static int test_nullptr(int seed, int i) {
    int *p = PF_NULLPTR;
    void (*fn)(void) = PF_NULLPTR;
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS(p == NULL);
    PF_ASSERT_ALWAYS(fn == NULL);
    PF_ASSERT_ALWAYS(!PF_NULLPTR);
    return 0;
}

static int test_types(int seed, int i) {
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_GE(PF_ALIGNOF(pf_max_align_t), PF_ALIGNOF(long long));
    PF_ASSERT_ALWAYS_GE(PF_ALIGNOF(pf_max_align_t), PF_ALIGNOF(double));
    PF_ASSERT_ALWAYS_GE(PF_ALIGNOF(pf_max_align_t), PF_ALIGNOF(void *));
    PF_ASSERT_ALWAYS_EQ(
        PF_ALIGNOF(pf_max_align_t) & (PF_ALIGNOF(pf_max_align_t) - 1), 0
    );
#if PF_STD_C11
    PF_ASSERT_ALWAYS_EQ(PF_ALIGNOF(pf_max_align_t), PF_ALIGNOF(max_align_t));
#elif PF_CC_GNU && !defined(__i386__)
    /* What max_align_t has in C11 with GCC and Clang. */
    PF_ASSERT_ALWAYS_EQ(
        PF_ALIGNOF(pf_max_align_t),
        PF_ALIGNOF(long double) > PF_ALIGNOF(long long)
            ? PF_ALIGNOF(long double)
            : PF_ALIGNOF(long long)
    );
#endif

    PF_ASSERT_ALWAYS_EQ(sizeof(pf_char8_t), 1);
    PF_ASSERT_ALWAYS_GT((pf_char8_t)-1, 0);
    return 0;
}

pf_test_t suite_stdc[] = {
    { test_typeof, "typeof" },
    { test_alignment, "alignment" },
    { test_static_assert_block, "static_assert_block" },
    { test_thread_local, "thread_local" },
    { test_unreachable, "unreachable" },
    { test_nullptr, "nullptr" },
    { test_types, "types" },
    { 0 },
};
