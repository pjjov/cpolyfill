/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/assert.h>

#include <limits.h>
#include <setjmp.h>
#include <stdint.h>

#include "check.h"

#ifndef _WIN32
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

/* ---- A handler that records the failure and jumps back ---------------- */

static struct {
    jmp_buf env;
    int calls;
    char expr[256];
    char file[256];
    int line;
    char func[64];
    char message[256];
    int has_message;
} failure;

static void copy(char *dst, size_t size, const char *src) {
    snprintf(dst, size, "%s", src ? src : "");
}

static void recording_handler(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    failure.calls++;
    copy(failure.expr, sizeof(failure.expr), expr);
    copy(failure.file, sizeof(failure.file), file);
    failure.line = line;
    copy(failure.func, sizeof(failure.func), func);
    copy(failure.message, sizeof(failure.message), message);
    failure.has_message = message != NULL;
    longjmp(failure.env, 1);
}

/* Runs `stmt` with the recording handler and checks whether it failed. */
#define EXPECT(want_failure, stmt)                        \
    do {                                                  \
        int failed_;                                      \
        pf_assert_handler_fn prev_;                       \
        memset(&failure, 0, sizeof(failure));             \
        prev_ = pf_set_assert_handler(recording_handler); \
        if (setjmp(failure.env) == 0) {                   \
            stmt;                                         \
            failed_ = 0;                                  \
        } else {                                          \
            failed_ = 1;                                  \
        }                                                 \
        pf_set_assert_handler(prev_);                     \
        PF_ASSERT_ALWAYS_MSG(                             \
            failed_ == (want_failure),                    \
            "%s `%s'",                                    \
            failed_ ? "failed:" : "passed:",              \
            #stmt                                         \
        );                                                \
    } while (0)
#define EXPECT_FAIL(stmt) EXPECT(1, stmt)
#define EXPECT_PASS(stmt) EXPECT(0, stmt)

static int test_pass(int seed, int i) {
    int n = 5;
    const char *s = "abc";
    (void)seed, (void)i;

    PF_ASSERT(n == 5);
    PF_ASSERT_MSG(n == 5, "n is %d", n);
    PF_ASSERT_EQ(n, 5);
    PF_ASSERT_NE(n, 4);
    PF_ASSERT_LT(n, 6);
    PF_ASSERT_LE(n, 5);
    PF_ASSERT_GT(n, 4);
    PF_ASSERT_GE(n, 5);
    PF_ASSERT_STR_EQ(s, "abc");
    PF_ASSERT_STR_NE(s, "abd");
    PF_ASSERT_STR_EQ(NULL, NULL);
    PF_ASSERT_STR_NE(s, NULL);
    PF_ASSERT_MEM_EQ(s, "abd", 2);
    PF_ASSERT_MEM_NE(s, "abd", 3);
    PF_ASSERT_MEM_EQ(NULL, NULL, 0);
    PF_ASSERT_NULL(NULL);
    PF_ASSERT_NOT_NULL(s);

    PF_ASSERT_ALWAYS(n == 5);
    PF_ASSERT_ALWAYS_EQ(n, 5);
    PF_ASSERT(sizeof(int[2]) == 2 * sizeof(int)); /* commas */

    PF_ASSERT_ALWAYS(PF_CHECK(n == 5));
    PF_ASSERT_ALWAYS(PF_CHECK_EQ(n, 5));
    PF_ASSERT_ALWAYS(PF_CHECK_STR_EQ(s, "abc"));
    PF_ASSERT_ALWAYS(PF_CHECK_NOT_NULL(s));
    PF_ASSERT_ALWAYS(PF_CHECK_MSG(n > 0, "positive"));
    return 0;
}

static int test_handler(int seed, int i) {
    int line;
    (void)seed, (void)i;

    EXPECT_PASS(PF_ASSERT(1 + 1 == 2));
    PF_ASSERT_ALWAYS_EQ(failure.calls, 0);

    line = __LINE__ + 1;
    EXPECT_FAIL(PF_ASSERT(1 + 1 == 3));
    PF_ASSERT_ALWAYS_EQ(failure.calls, 1);
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "1 + 1 == 3");
    PF_ASSERT_ALWAYS(strstr(failure.file, "assert.c") != NULL);
    PF_ASSERT_ALWAYS_EQ(failure.line, line);
    PF_ASSERT_ALWAYS_STR_EQ(failure.func, "test_handler");
    PF_ASSERT_ALWAYS(!failure.has_message);

    /* The previous handler is returned, NULL restores the default. */
    {
        pf_assert_handler_fn runner = pf_set_assert_handler(NULL);
        pf_assert_handler_fn def = pf_set_assert_handler(recording_handler);
        PF_ASSERT_ALWAYS(def != NULL);
        PF_ASSERT_ALWAYS(def != recording_handler);
        PF_ASSERT_ALWAYS(pf_set_assert_handler(NULL) == recording_handler);
        PF_ASSERT_ALWAYS(pf_set_assert_handler(NULL) == def);
        /* Back to the test runner's. */
        pf_set_assert_handler(runner);
    }
    return 0;
}

#define TEST_LIMIT 10

static int test_unexpanded(int seed, int i) {
    int n = 11;
    (void)seed, (void)i;

    /* Macros in the expression are shown as written. */
    EXPECT_FAIL(PF_ASSERT(n < TEST_LIMIT));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "n < TEST_LIMIT");
    EXPECT_FAIL(PF_ASSERT_LE(n, TEST_LIMIT));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "n <= TEST_LIMIT");
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "11 > 10");
    return 0;
}

static int test_message(int seed, int i) {
    int n = -3;
    (void)seed, (void)i;

    EXPECT_FAIL(PF_ASSERT_MSG(n > 0, "n is %d", n));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "n > 0");
    PF_ASSERT_ALWAYS(failure.has_message);
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "n is -3");

    EXPECT_FAIL(PF_ASSERT_ALWAYS_MSG(0, "no arguments"));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "no arguments");

    /* A long message is truncated, not overflowed. */
    EXPECT_FAIL(PF_ASSERT_MSG(0, "%2000d", 1));
    PF_ASSERT_ALWAYS_EQ(strlen(failure.message), sizeof(failure.message) - 1);
    return 0;
}

static int test_compare(int seed, int i) {
    int n = 3;
    unsigned u = 4;
    long long min = LLONG_MIN;
    unsigned long long max = ULLONG_MAX;
    (void)seed, (void)i;

    EXPECT_FAIL(PF_ASSERT_EQ(n, u));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "n == u");
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "3 != 4");

    EXPECT_FAIL(PF_ASSERT_NE(n, 3));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "3 == 3");
    EXPECT_FAIL(PF_ASSERT_LT(u, n));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "4 >= 3");
    EXPECT_FAIL(PF_ASSERT_GT(n, u));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "3 <= 4");
    EXPECT_FAIL(PF_ASSERT_GE(n, u));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "3 < 4");

    /* Values, not C conversions: -1 < 0u holds and -1 != UINT_MAX. */
    EXPECT_PASS(PF_ASSERT_LT(-1, 0u));
    EXPECT_FAIL(PF_ASSERT_EQ(-1, UINT_MAX));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "-1 != 4294967295");
    EXPECT_PASS(PF_ASSERT_LT(min, max));
    EXPECT_FAIL(PF_ASSERT_GT(min, -1));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "-9223372036854775808 <= -1");
    EXPECT_FAIL(PF_ASSERT_LT(max, 0));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "18446744073709551615 >= 0");
    EXPECT_PASS(PF_ASSERT_EQ((unsigned char)200, 200));
    EXPECT_PASS(PF_ASSERT_EQ((signed char)-5, -5LL));
    EXPECT_PASS(PF_ASSERT_LT(-5, -2));
    EXPECT_PASS(PF_ASSERT_GT((int8_t)-1, INT64_MIN));
    return 0;
}

static int test_strings_memory(int seed, int i) {
    const char *s = "abc";
    const unsigned char a[4] = { 1, 2, 3, 4 }, b[4] = { 1, 2, 9, 4 };
    int x = 0;
    (void)seed, (void)i;

    EXPECT_FAIL(PF_ASSERT_STR_EQ(s, "abd"));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "s == \"abd\"");
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "\"abc\" != \"abd\"");
    EXPECT_FAIL(PF_ASSERT_STR_EQ(s, NULL));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "\"abc\" != NULL");
    EXPECT_FAIL(PF_ASSERT_STR_NE(s, "abc"));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "\"abc\" == \"abc\"");

    EXPECT_FAIL(PF_ASSERT_MEM_EQ(a, b, 4));
    PF_ASSERT_ALWAYS_STR_EQ(
        failure.message, "bytes differ at offset 2: 0x03 != 0x09"
    );
    EXPECT_PASS(PF_ASSERT_MEM_EQ(a, b, 2));
    EXPECT_FAIL(PF_ASSERT_MEM_NE(a, b, 2));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "all 2 bytes are equal");

    EXPECT_FAIL(PF_ASSERT_NULL(&x));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "&x == NULL");
    PF_ASSERT_ALWAYS(strncmp(failure.message, "it is ", 6) == 0);
    EXPECT_FAIL(PF_ASSERT_NOT_NULL(NULL));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "NULL != NULL");
    PF_ASSERT_ALWAYS(!failure.has_message);
    return 0;
}

static int test_evaluate_once(int seed, int i) {
    volatile int n = 0, m = 10;
    const char *strs[2] = { "a", "a" };
    const char **sp = strs;
    (void)seed, (void)i;

    PF_ASSERT_EQ(n++, 0);
    PF_ASSERT_LT(n++, m--);
    PF_ASSERT_ALWAYS_EQ(n, 2);
    PF_ASSERT_ALWAYS_EQ(m, 9);
    PF_ASSERT_STR_EQ(*sp++, "a");
    PF_ASSERT_ALWAYS(sp == strs + 1);
    PF_ASSERT_ALWAYS(PF_CHECK(n++ == 2));
    PF_ASSERT_ALWAYS_EQ(n, 3);

    /* Once, also when failing. */
    EXPECT_FAIL(PF_ASSERT_EQ(n++, 0));
    PF_ASSERT_ALWAYS_EQ(n, 4);
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "3 != 0");
    return 0;
}

static int test_check_fails(int seed, int i) {
    (void)seed, (void)i;
    /* Without NDEBUG a failed PF_CHECK is an assertion failure. */
    EXPECT_FAIL(PF_CHECK(1 > 2));
    PF_ASSERT_ALWAYS_STR_EQ(failure.expr, "1 > 2");
    EXPECT_FAIL(PF_CHECK_EQ(1, 2));
    EXPECT_FAIL(PF_CHECK_MSG(1 > 2, "%s", "why"));
    PF_ASSERT_ALWAYS_STR_EQ(failure.message, "why");
    return 0;
}

/* ---- With NDEBUG ------------------------------------------------------ */

#define NDEBUG
#include <cpolyfill/assert.h>

static int test_ndebug(int seed, int i) {
    volatile int n = 0;
    (void)seed, (void)i;

    /* PF_ASSERT is gone, arguments are not evaluated. */
    EXPECT_PASS(PF_ASSERT(n++ == 5));
    EXPECT_PASS(PF_ASSERT_EQ(n++, 5));
    EXPECT_PASS(PF_ASSERT_STR_EQ("a", "b"));
    EXPECT_PASS(PF_ASSERT_MSG(0, "x"));
    PF_ASSERT_ALWAYS_EQ(n, 0);

    /* PF_CHECK gives the result without failing. */
    EXPECT_PASS(PF_ASSERT_ALWAYS(!PF_CHECK(n++ == 5)));
    PF_ASSERT_ALWAYS_EQ(n, 1);
    EXPECT_PASS(PF_ASSERT_ALWAYS(!PF_CHECK_EQ(n, 5)));
    EXPECT_PASS(PF_ASSERT_ALWAYS(PF_CHECK_EQ(n, 1)));
    EXPECT_PASS(PF_ASSERT_ALWAYS(!PF_CHECK_STR_EQ("a", "b")));
    EXPECT_PASS(PF_ASSERT_ALWAYS(!PF_CHECK_NULL(&n)));
    EXPECT_PASS(PF_ASSERT_ALWAYS(!PF_CHECK_MSG(0, "x")));
    PF_ASSERT_ALWAYS_EQ(failure.calls, 0);

    /* PF_ASSERT_ALWAYS still fails. */
    EXPECT_FAIL(PF_ASSERT_ALWAYS(n == 5));
    return 0;
}

#undef NDEBUG
#include <cpolyfill/assert.h>

static int test_ndebug_restored(int seed, int i) {
    (void)seed, (void)i;
    EXPECT_FAIL(PF_ASSERT(0));
    return 0;
}

/* ---- The default handler ---------------------------------------------- */

static int test_default_output(int seed, int i) {
#ifdef _WIN32
    (void)seed, (void)i;
    return SKIP("needs fork");
#else
    int fds[2], status, line = 0;
    char out[512], expected[512];
    const char *colon;
    ssize_t n, total = 0;
    pid_t pid;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pipe(fds), 0);
    fflush(stderr);
    pid = fork();
    PF_ASSERT_ALWAYS_GE(pid, 0);
    if (pid == 0) {
        int a = 1, b = 2;
        signal(SIGABRT, SIG_DFL);
        pf_set_assert_handler(NULL); /* not the test runner's */
        dup2(fds[1], 2);
        close(fds[0]);
        line = __LINE__ + 1;
        PF_ASSERT_EQ(a, b);
        _exit(0);
    }
    line = __LINE__ - 3;
    close(fds[1]);
    while ((n = read(fds[0], out + total, sizeof(out) - 1 - (size_t)total)) > 0)
        total += n;
    out[total] = '\0';
    close(fds[0]);
    PF_ASSERT_ALWAYS_EQ(waitpid(pid, &status, 0), pid);
    PF_ASSERT_ALWAYS(WIFSIGNALED(status));
    PF_ASSERT_ALWAYS_EQ(WTERMSIG(status), SIGABRT);

    /* "prog: file:line: func: Assertion `a == b' failed: 1 != 2" */
    snprintf(
        expected,
        sizeof(expected),
        "%s:%d: test_default_output: Assertion `a == b' failed: 1 != 2\n",
        __FILE__,
        line
    );
    PF_ASSERT_ALWAYS(strncmp(out, "cpolyfill-test-", 15) == 0);
    colon = strstr(out, ": ");
    PF_ASSERT_ALWAYS_NOT_NULL(colon);
    PF_ASSERT_ALWAYS_STR_EQ(colon + 2, expected);
    return 0;
#endif
}

pf_test_t suite_assert[] = {
    { test_pass, "pass" },
    { test_handler, "handler" },
    { test_unexpanded, "unexpanded" },
    { test_message, "message" },
    { test_compare, "compare" },
    { test_strings_memory, "strings_memory" },
    { test_evaluate_once, "evaluate_once" },
    { test_check_fails, "check_fails" },
    { test_ndebug, "ndebug" },
    { test_ndebug_restored, "ndebug_restored" },
    { test_default_output, "default_output" },
    { 0 },
};
