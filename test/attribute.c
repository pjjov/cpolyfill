/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/attribute.h>

#include <stdarg.h>

#include "check.h"

/* Every macro in the documented position. The file is built with -Wall
   -Wextra, so a misplaced attribute shows up as a warning. */

PF_DEPRECATED("use something else") int attr_deprecated(void);
PF_NODISCARD("the sum") static int attr_nodiscard(int a, int b);
PF_NORETURN void attr_noreturn(void);
PF_PRINTF(2, 3) static int attr_printf(char *buf, const char *fmt, ...);
PF_NONNULL(1) static size_t attr_nonnull(const char *s);
PF_NONNULL() static int attr_nonnull_all(const int *a, const int *b);
PF_RETURNS_NONNULL static const char *attr_returns_nonnull(void);
PF_PURE static int attr_pure(const int *p);
PF_CONST static int attr_const(int x);
PF_MALLOC_LIKE void *attr_malloc(size_t size);
PF_HOT static int attr_hot(int x);
PF_COLD static int attr_cold(int x);
PF_NOINLINE static int attr_noinline(int x);
int attr_unsequenced(int x) PF_UNSEQUENCED;
int attr_reproducible(const int *x) PF_REPRODUCIBLE;

/* Several at once, standard and compiler attributes mixed, before
   `static`. */
PF_NODISCARD("x") PF_CONST PF_NOINLINE static int attr_many(int x);
PF_COLD PF_NODISCARD("x") PF_PURE static int attr_many2(const int *x);

PF_ALWAYS_INLINE PF_INLINE int attr_always_inline(int x) { return x + 1; }

static int attr_nodiscard(int a, int b) { return a + b; }

static int attr_printf(char *buf, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, 32, fmt, ap);
    va_end(ap);
    return n;
}

static size_t attr_nonnull(const char *s) { return strlen(s); }
static int attr_nonnull_all(const int *a, const int *b) { return *a + *b; }
static const char *attr_returns_nonnull(void) { return "x"; }
static int attr_pure(const int *p) { return *p; }
static int attr_const(int x) { return x * 2; }
static int attr_hot(int x) { return x + 1; }
static int attr_cold(int x) { return x - 1; }
static int attr_noinline(int x) { return x * 3; }
static int attr_many(int x) { return x; }
static int attr_many2(const int *x) { return *x; }

static int attr_unused_param(PF_UNUSED int x) { return 0; }

static int attr_fallthrough(int x) {
    int n = 0;
    switch (x) {
    case 0:
        n++;
        PF_FALLTHROUGH;
    case 1:
        n++;
        break;
    default:
        break;
    }
    return n;
}

static int test_functions(int seed, int i) {
    char buf[32];
    int a = 2, b = 3;
    PF_UNUSED int unused_local;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(attr_nodiscard(1, 2), 3);
    PF_ASSERT_ALWAYS_EQ(attr_printf(buf, "%d-%s", 7, "x"), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "7-x");
    PF_ASSERT_ALWAYS_EQ(attr_nonnull("abc"), 3);
    PF_ASSERT_ALWAYS_EQ(attr_nonnull_all(&a, &b), 5);
    PF_ASSERT_ALWAYS_STR_EQ(attr_returns_nonnull(), "x");
    PF_ASSERT_ALWAYS_EQ(attr_pure(&a), 2);
    PF_ASSERT_ALWAYS_EQ(attr_const(4), 8);
    PF_ASSERT_ALWAYS_EQ(attr_hot(1), 2);
    PF_ASSERT_ALWAYS_EQ(attr_cold(1), 0);
    PF_ASSERT_ALWAYS_EQ(attr_noinline(2), 6);
    PF_ASSERT_ALWAYS_EQ(attr_many(5), 5);
    PF_ASSERT_ALWAYS_EQ(attr_many2(&b), 3);
    PF_ASSERT_ALWAYS_EQ(attr_always_inline(1), 2);
    PF_ASSERT_ALWAYS_EQ(attr_unused_param(1), 0);
    PF_ASSERT_ALWAYS_EQ(attr_fallthrough(0), 2);
    PF_ASSERT_ALWAYS_EQ(attr_fallthrough(1), 1);
    PF_ASSERT_ALWAYS_EQ(attr_fallthrough(2), 0);
    return 0;
}

PF_PACKED_BEGIN
struct packed {
    char c;
    int i;
};
PF_PACKED_END

struct not_packed {
    char c;
    int i;
};

static int test_packed(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(sizeof(struct packed), 1 + sizeof(int));
    PF_ASSERT_ALWAYS_GT(sizeof(struct not_packed), 1 + sizeof(int));
    return 0;
}

static int constructed = 0;

PF_CONSTRUCTOR(attr_constructor) { constructed++; }

/* Runs at exit, after the test; it must merely not break the program. */
PF_DESTRUCTOR(attr_destructor) { constructed = -1; }

static int test_constructor(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(constructed, 1);
    return 0;
}

#if PF_HAS_CLEANUP
static int cleaned = 0;
static void attr_cleanup(int *p) { cleaned += *p; }
#endif

static int test_cleanup(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_CLEANUP
    cleaned = 0;
    {
        PF_CLEANUP(attr_cleanup) int x = 5;
        (void)x;
        PF_ASSERT_ALWAYS_EQ(cleaned, 0);
    }
    PF_ASSERT_ALWAYS_EQ(cleaned, 5);
    return 0;
#else
    return SKIP("no cleanup attribute");
#endif
}

#if PF_HAS_WEAK
PF_WEAK int attr_weak(void);
PF_WEAK int attr_weak(void) { return 42; }
#endif

static int test_weak(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_WEAK
    PF_ASSERT_ALWAYS_EQ(attr_weak(), 42);
    return 0;
#else
    return SKIP("no weak symbols");
#endif
}

pf_test_t suite_attribute[] = {
    { test_functions, "functions" },
    { test_packed, "packed" },
    { test_constructor, "constructor" },
    { test_cleanup, "cleanup" },
    { test_weak, "weak" },
    { 0 },
};
