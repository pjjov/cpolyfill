/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The standard names mapped by PF_STD_NAMES. Built for every standard of
   the test variants, so the names are polyfilled in some and native in
   others. */

#define PF_STD_NAMES

#include <cpolyfill/assert.h>
#include <cpolyfill/bitwise.h>
#include <cpolyfill/endian.h>
#include <cpolyfill/overflow.h>
#include <cpolyfill/stdc.h>

#include <setjmp.h>
#include <stdint.h>

#include "check.h"

/* C11 and C17 have the static_assert of <assert.h>, which needs a
   message. */
static_assert(sizeof(char) == 1, "char");
#if PF_STD_C23 || !PF_STD_C11
static_assert(sizeof(int) >= 2);
#endif

#if PF_HAS_THREAD_LOCAL
static thread_local int tls_value = 3;
#endif

static int sign(int x) {
    if (x > 0)
        return 1;
    if (x < 0)
        return -1;
    if (x == 0)
        return 0;
    unreachable();
}

static int test_keywords(int seed, int i) {
    bool yes = true, no = false;
    int *p = nullptr;
    alignas(16) char buf[4];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(yes && !no);
    PF_ASSERT_ALWAYS(p == NULL);
    PF_ASSERT_ALWAYS_EQ((uintptr_t)buf % 16, 0);
    PF_ASSERT_ALWAYS_EQ(alignof(char), 1);
    PF_ASSERT_ALWAYS_EQ(sign(-4), -1);
#if PF_HAS_TYPEOF
    {
        typeof(yes) copy = yes;
        PF_ASSERT_ALWAYS(copy);
    }
#endif
#if PF_HAS_TYPEOF_UNQUAL
    {
        const int c = 1;
        typeof_unqual(c) m = c;
        m++;
        PF_ASSERT_ALWAYS_EQ(m, 2);
    }
#endif
#if PF_HAS_THREAD_LOCAL
    PF_ASSERT_ALWAYS_EQ(tls_value, 3);
#endif
    (void)buf;
    return 0;
}

static jmp_buf env;

static void jump_back(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    (void)expr, (void)file, (void)line, (void)func, (void)message;
    longjmp(env, 1);
}

static int test_assert(int seed, int i) {
    int reached;
    (void)seed, (void)i;

    /* `assert` is PF_ASSERT, so a failure reaches the handler. */
    assert(1 + 1 == 2);
    pf_set_assert_handler(jump_back);
    if (setjmp(env) == 0) {
        assert(1 + 1 == 3);
        reached = 0;
    } else {
        reached = 1;
    }
    pf_set_assert_handler(NULL);
    PF_ASSERT_ALWAYS(reached);
    return 0;
}

/* Native names where the system has them, the polyfills elsewhere. */
static int test_integers(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(stdc_count_ones_ui(0xF0u), 4);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil_ui(0), 1);
    PF_ASSERT_ALWAYS(stdc_has_single_bit_ul(64ul));
#if PF_HAS_BITWISE_GENERIC
    PF_ASSERT_ALWAYS_EQ(stdc_leading_zeros((unsigned char)1), 7);
#endif
    PF_ASSERT_ALWAYS_EQ(
        __STDC_ENDIAN_NATIVE__,
        PF_ENDIAN_LITTLE ? __STDC_ENDIAN_LITTLE__ : __STDC_ENDIAN_BIG__
    );
#if PF_HAS_CKD
    {
        unsigned char r = 0;
        PF_ASSERT_ALWAYS(ckd_add(&r, 255, 1));
        PF_ASSERT_ALWAYS(!ckd_mul(&r, 5, 50) && r == 250);
    }
#endif
    PF_ASSERT_ALWAYS_EQ(be16toh(htobe16(0x1234)), 0x1234);
    PF_ASSERT_ALWAYS_EQ(htole32(1), pf_htole32(1));
    PF_ASSERT_ALWAYS_EQ(bswap_32(0x11223344), 0x44332211);
    return 0;
}

pf_test_t suite_std_names[] = {
    { test_keywords, "keywords" },
    { test_assert, "assert" },
    { test_integers, "integers" },
    { 0 },
};
