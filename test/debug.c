/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/attribute.h>
#include <cpolyfill/debug.h>
#include <cpolyfill/errno.h>

#include "check.h"

#if !defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
    #include <signal.h>
    #define TEST_DEBUGBREAK 1
#else
    #define TEST_DEBUGBREAK 0
#endif

static int test_debugger_attached(int seed, int i) {
    bool first = pf_debugger_attached();
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS(pf_debugger_attached() == first);
    return 0;
}

/* A few frames that must show up in the backtrace. */
PF_NOINLINE static int capture_inner(void **frames, int max) {
    return pf_backtrace(frames, max);
}

PF_NOINLINE static int capture_outer(void **frames, int max) {
    int n = capture_inner(frames, max);
    return n + 0 * max; /* not a tail call */
}

static int test_backtrace(int seed, int i) {
    void *frames[64], *few[2];
    int n;
    (void)seed, (void)i;

    n = capture_outer(frames, 64);
    if (n == PF_ENOSYS)
        return SKIP("no backtrace on this platform");
    PF_ASSERT_ALWAYS_GE(n, 3);
    PF_ASSERT_ALWAYS_LE(n, 64);
    PF_ASSERT_ALWAYS_NOT_NULL(frames[0]);
    PF_ASSERT_ALWAYS_NOT_NULL(frames[1]);

    /* The first frame is in capture_inner, the caller of pf_backtrace. */
    PF_ASSERT_ALWAYS_EQ(capture_outer(few, 2), 2);
    PF_ASSERT_ALWAYS(few[0] == frames[0]);
    PF_ASSERT_ALWAYS(few[1] == frames[1]);

    PF_ASSERT_ALWAYS_EQ(pf_backtrace(frames, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_backtrace(NULL, 4), 0);
    return 0;
}

static int test_backtrace_print(int seed, int i) {
    FILE *f = tmpfile();
    char line[512];
    int lines = 0;
    void *probe[4];
    (void)seed, (void)i;

    if (pf_backtrace(probe, 4) == PF_ENOSYS)
        return SKIP("no backtrace on this platform");
    if (!f)
        return SKIP("no temporary file");

    pf_backtrace_print(f);
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        char expected[16];
        snprintf(expected, sizeof(expected), "#%-2d ", lines);
        PF_ASSERT_ALWAYS_MSG(
            strncmp(line, expected, strlen(expected)) == 0, "line `%s'", line
        );
        lines++;
    }
    fclose(f);
    PF_ASSERT_ALWAYS_GE(lines, 2);
    return 0;
}

#if TEST_DEBUGBREAK
static volatile sig_atomic_t trapped = 0;

static void on_trap(int sig) {
    (void)sig;
    trapped = 1;
}
#endif

static int test_debugbreak(int seed, int i) {
    (void)seed, (void)i;
#if TEST_DEBUGBREAK
    /* On x86 the trap leaves the instruction pointer after the breakpoint,
       so the program continues when the handler returns. */
    if (pf_debugger_attached())
        return SKIP("a debugger would stop here");
    signal(SIGTRAP, on_trap);
    pf_debugbreak();
    signal(SIGTRAP, SIG_DFL);
    PF_ASSERT_ALWAYS(trapped);
    return 0;
#else
    return SKIP("resuming after the trap is only tested on x86");
#endif
}

pf_test_t suite_debug[] = {
    { test_debugger_attached, "debugger_attached" },
    { test_backtrace, "backtrace" },
    { test_backtrace_print, "backtrace_print" },
    { test_debugbreak, "debugbreak" },
    { 0 },
};
