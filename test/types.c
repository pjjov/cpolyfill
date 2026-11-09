/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/types.h>

#include "check.h"

struct point {
    int x, y;
};

static char global_array[4];
static char *global_pointer;

/* The checks are integer constant expressions. */
PF_STATIC_ASSERT(PF_IS_ARRAY(global_array));
PF_STATIC_ASSERT(PF_IS_TYPE(char[4], global_array));
#if PF_HAS_TYPE_CHECKS
PF_STATIC_ASSERT(!PF_IS_ARRAY(global_pointer));
PF_STATIC_ASSERT(PF_IS_POINTER(global_pointer));
#endif

static int test_is_type(int seed, int i) {
    int n = 0;
    const int cn = 0;
    long l = 0;
    char arr[4];
    char *p = arr;
    struct point pt = { 1, 2 };
    (void)seed, (void)i, (void)cn, (void)l, (void)pt;

    if (!PF_HAS_TYPE_CHECKS)
        return SKIP("no type checks");

    PF_ASSERT_ALWAYS(PF_IS_TYPE(int, n));
    PF_ASSERT_ALWAYS(PF_IS_TYPE(int, cn)); /* top-level const ignored */
    PF_ASSERT_ALWAYS(!PF_IS_TYPE(long, n));
    PF_ASSERT_ALWAYS(!PF_IS_TYPE(unsigned, n));
    PF_ASSERT_ALWAYS(PF_IS_TYPE(long, l));
    PF_ASSERT_ALWAYS(PF_IS_TYPE(char *, p));
    PF_ASSERT_ALWAYS(!PF_IS_TYPE(char *, arr)); /* no decay */
    PF_ASSERT_ALWAYS(PF_IS_TYPE(char[4], arr));
    PF_ASSERT_ALWAYS(PF_IS_TYPE(struct point, pt));
    PF_ASSERT_ALWAYS(PF_IS_TYPE(int, pt.x));
    PF_ASSERT_ALWAYS(PF_IS_TYPE(int, n + 1));

    PF_ASSERT_ALWAYS(PF_SAME_TYPE(n, cn));
    PF_ASSERT_ALWAYS(!PF_SAME_TYPE(n, l));
    PF_ASSERT_ALWAYS(PF_SAME_TYPE(p, &arr[0]));
    PF_ASSERT_ALWAYS(!PF_SAME_TYPE(p, arr));

    /* Not evaluated. */
    PF_ASSERT_ALWAYS(PF_IS_TYPE(int, n++));
    PF_ASSERT_ALWAYS_EQ(n, 0);
    return 0;
}

static int test_is_const(int seed, int i) {
    int n = 0;
    const int cn = 0;
    const char *cp = "x";
    char *const pc = NULL;
    const char carr[2] = { 0 };
    (void)seed, (void)i, (void)cn, (void)pc, (void)carr;

    if (!PF_HAS_TYPE_CHECKS)
        return SKIP("no type checks");

    PF_ASSERT_ALWAYS(!PF_IS_CONST(n));
    PF_ASSERT_ALWAYS(PF_IS_CONST(cn));
    PF_ASSERT_ALWAYS(!PF_IS_CONST(cp));
    PF_ASSERT_ALWAYS(PF_IS_CONST(*cp));
    PF_ASSERT_ALWAYS(PF_IS_CONST(pc));
    PF_ASSERT_ALWAYS(PF_IS_CONST(carr));
    return 0;
}

static int test_array_pointer(int seed, int i) {
    int n = 0;
    char arr[4];
    const char carr[2] = { 0 };
    int matrix[2][3];
    char *p = arr;
    const char *cp = arr;
    char *const pc = arr;
    struct point pt = { 0, 0 };
    (void)seed, (void)i, (void)n, (void)carr, (void)matrix, (void)p, (void)cp,
        (void)pc, (void)pt;

    if (!PF_HAS_TYPE_CHECKS)
        return SKIP("no type checks");

    PF_ASSERT_ALWAYS(PF_IS_ARRAY(arr));
    PF_ASSERT_ALWAYS(PF_IS_ARRAY(carr));
    PF_ASSERT_ALWAYS(PF_IS_ARRAY(matrix));
    PF_ASSERT_ALWAYS(PF_IS_ARRAY(matrix[0]));
    PF_ASSERT_ALWAYS(!PF_IS_ARRAY(p));
    PF_ASSERT_ALWAYS(!PF_IS_ARRAY(cp));
    PF_ASSERT_ALWAYS(!PF_IS_ARRAY(pc));
    PF_ASSERT_ALWAYS(!PF_IS_ARRAY(n));
    PF_ASSERT_ALWAYS(!PF_IS_ARRAY(pt));
    PF_ASSERT_ALWAYS(!PF_IS_ARRAY(&"literal"[1]));

    PF_ASSERT_ALWAYS(PF_IS_POINTER(p));
    PF_ASSERT_ALWAYS(PF_IS_POINTER(cp));
    PF_ASSERT_ALWAYS(PF_IS_POINTER(pc));
    PF_ASSERT_ALWAYS(PF_IS_POINTER(&n));
    PF_ASSERT_ALWAYS(!PF_IS_POINTER(arr));
    PF_ASSERT_ALWAYS(!PF_IS_POINTER(n));
    PF_ASSERT_ALWAYS(!PF_IS_POINTER(pt));
    return 0;
}

static int test_check_and_choose(int seed, int i) {
    int n = 3;
    const int cn = 4;
    long l = 5;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(PF_CHECK_TYPE(int, n) + 1, 4);
    PF_ASSERT_ALWAYS_EQ(PF_CHECK_TYPE(long, l), 5);

    if (!PF_HAS_TYPE_CHECKS)
        return SKIP("no type checks");

    PF_ASSERT_ALWAYS_EQ(PF_IF_TYPE(int, n, 1, 2), 1);
    PF_ASSERT_ALWAYS_EQ(PF_IF_TYPE(int, l, 1, 2), 2);
    PF_ASSERT_ALWAYS_EQ(PF_IF_CONST(cn, 1, 2), 1);
    PF_ASSERT_ALWAYS_EQ(PF_IF_CONST(n, 1, 2), 2);

    /* Only the chosen expression is evaluated. */
    PF_ASSERT_ALWAYS_EQ(PF_IF_TYPE(int, n, n++, n--), 3);
    PF_ASSERT_ALWAYS_EQ(n, 4);
    return 0;
}

pf_test_t suite_types[] = {
    { test_is_type, "is_type" },
    { test_is_const, "is_const" },
    { test_array_pointer, "array_pointer" },
    { test_check_and_choose, "check_and_choose" },
    { 0 },
};
