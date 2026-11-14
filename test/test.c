/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The test runner, <cpolyfill/test.h>: runs test/sample.c (found through
   PF_TEST_SAMPLE, set by meson test) and checks its report. */

#include "../src/internal.h"

#include <cpolyfill/io.h>
#include <cpolyfill/process.h>
#include <cpolyfill/test.h>
#include <cpolyfill/time.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "files.h"

static const char *sample_path(void) { return getenv("PF_TEST_SAMPLE"); }

#define NEED_SAMPLE()                                                          \
    do {                                                                       \
        if (!sample_path())                                                    \
            return SKIP("PF_TEST_SAMPLE is not set (run through meson test)"); \
    } while (0)

/* The ways of running tests. */
static const char *const modes[] = {
#if PF_OS_POSIX
    "--isolate=fork",
#endif
    "--isolate=spawn",
    "--no-isolate",
    NULL,
};

static char output[8192];

/* Runs the sample with up to 8 arguments (NULL-terminated); returns its
   exit code, with its stdout in `output` (stderr is discarded). */
static int run(const char *a0, ...) {
    const char *argv[12];
    pf_spawn_opts_t opts;
    pf_stream_t out;
    pf_exit_status_t st;
    size_t n;
    const char *data;
    va_list ap;
    int k = 1;

    argv[0] = sample_path();
    va_start(ap, a0);
    for (argv[k] = a0; argv[k] && k < 10; argv[++k] = va_arg(ap, const char *))
        ;
    va_end(ap);
    argv[k] = NULL;

    memset(&opts, 0, sizeof(opts));
    opts.stdio[2].mode = PF_STDIO_NULL;
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(
        pf_process_run(argv, &opts, NULL, &out, NULL, &st), PF_OK
    );
    data = (const char *)pf_stream_buffer_data(&out, &n);
    PF_ASSERT_ALWAYS_LT(n, sizeof(output));
    memcpy(output, data, n + 1);
    pf_stream_close(&out);
    PF_ASSERT_ALWAYS(st.exited);
    return st.code;
}

#define HAS(text)                                                           \
    PF_ASSERT_ALWAYS_MSG(                                                   \
        strstr(output, (text)) != NULL, "no \"%s\" in:\n%s", (text), output \
    )
#define LACKS(text)                                                      \
    PF_ASSERT_ALWAYS_MSG(                                                \
        strstr(output, (text)) == NULL, "\"%s\" in:\n%s", (text), output \
    )

static int test_skip_fn(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_test_skip("x %d", 1), PF_TEST_SKIP);
    PF_ASSERT_ALWAYS_EQ(pf_test_skip(NULL), PF_TEST_SKIP);
    return 0;
}

static int test_outcomes(int seed, int i) {
    int m;
    (void)seed, (void)i;
    NEED_SAMPLE();

    for (m = 0; modes[m]; m++) {
        const char *mode = modes[m];

        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "sample:pass", NULL), 0);
        HAS("TAP version 14\n1..1\n");
        HAS("\nok 1 - sample/pass\n");

        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "sample:fail", NULL), 1);
        HAS("\nnot ok 1 - sample/fail\n# failed at iteration 0\n");

        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "sample:assert", NULL), 1);
        HAS("\nnot ok 1 - sample/assert\n# assertion failed at iteration 0\n");

        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "sample:skip", NULL), 0);
        HAS("\nok 1 - sample/skip # SKIP no network\n");

        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "sample:todo", NULL), 0);
        HAS("\nnot ok 1 - sample/todo # TODO\n");

        /* A suite with count 0 is TODO. */
        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "later:a", NULL), 0);
        HAS("\nnot ok 1 - later/a # TODO\n");

        if (strcmp(mode, "--no-isolate") == 0)
            continue;
        PF_ASSERT_ALWAYS_EQ(run("--tap", mode, "sample:crash", NULL), 1);
#if PF_OS_WINDOWS
        HAS("\n# exited with code 3 at iteration 0\n");
#else
        HAS("\n# signal 6: aborted at iteration 0\n");
#endif
    }
    return 0;
}

static int test_timeout(int seed, int i) {
    int m;
    (void)seed, (void)i;
    NEED_SAMPLE();

    for (m = 0; modes[m]; m++) {
        uint64_t start;
        if (strcmp(modes[m], "--no-isolate") == 0)
            continue;
        start = pf_time_ns();
        PF_ASSERT_ALWAYS_EQ(
            run("--tap", modes[m], "--timeout", "0.3", "sample:slow", NULL), 1
        );
        HAS("\n# timed out after 0.3s at iteration 0\n");
        PF_ASSERT_ALWAYS_LT(pf_time_ns() - start, 5000000000u);
    }
    /* Only isolated tests can be ended. */
    PF_ASSERT_ALWAYS_EQ(
        run("--no-isolate", "--timeout", "1", "sample:slow", NULL), 2
    );
    PF_ASSERT_ALWAYS_EQ(run("--timeout", "-1", "sample:pass", NULL), 2);
    return 0;
}

static int test_repeat_seed(int seed, int i) {
    int m;
    (void)seed, (void)i;
    NEED_SAMPLE();

    for (m = 0; modes[m]; m++) {
        PF_ASSERT_ALWAYS_EQ(
            run("--no-color",
                modes[m],
                "--seed",
                "42",
                "-r",
                "5",
                "sample:seed",
                NULL),
            1
        );
        HAS("seed=42 i=0\n");
        HAS("seed=42 i=1\n");
        HAS("seed=42 i=2\n");
        LACKS("i=3");
        HAS("failed at iteration 2");
        HAS("(seed: 0x2a)");
    }

    /* --repeat=N, and the suite's own count without it. */
    PF_ASSERT_ALWAYS_EQ(
        run("--no-color", "--repeat=2", "sample:seed", NULL), 0
    );
    HAS("sample (x2)\n");
    HAS("i=1");
    LACKS("i=2");
    PF_ASSERT_ALWAYS_EQ(run("--no-color", "--seed=7", "sample:seed", NULL), 0);
    HAS("seed=7 i=0\n");
    LACKS("i=1");
    PF_ASSERT_ALWAYS_EQ(run("-r", "0", "sample:seed", NULL), 2);
    PF_ASSERT_ALWAYS_EQ(run("--seed", "x", "sample:seed", NULL), 2);
    return 0;
}

static int test_select(int seed, int i) {
    (void)seed, (void)i;
    NEED_SAMPLE();

    PF_ASSERT_ALWAYS_EQ(run("--tap", "other", NULL), 0);
    HAS("\n1..2\n");
    PF_ASSERT_ALWAYS_EQ(run("--tap", "other:*", NULL), 0);
    HAS("\n1..2\n");
    PF_ASSERT_ALWAYS_EQ(run("--tap", "oth?r:b", NULL), 0);
    HAS("\n1..1\n");
    HAS("ok 1 - other/b\n");
    PF_ASSERT_ALWAYS_EQ(run("--tap", "*:a", NULL), 0);
    HAS("\n1..2\n");
    HAS("ok 1 - other/a\n");
    HAS("not ok 2 - later/a # TODO\n");
    PF_ASSERT_ALWAYS_EQ(
        run("--tap", "sample:pa*", "sample:?ail", "other:a", NULL), 1
    );
    HAS("\n1..3\n");
    PF_ASSERT_ALWAYS_EQ(
        run("--tap", "-r", "1", "--timeout", "0.2", "all", NULL), 1
    );
    HAS("\n1..12\n");
    PF_ASSERT_ALWAYS_EQ(run("--tap", "--", "other:a", NULL), 0);
    HAS("\n1..1\n");

    PF_ASSERT_ALWAYS_EQ(run("nomatch*", NULL), 2);
    PF_ASSERT_ALWAYS_EQ(run("other:c", NULL), 2);
    PF_ASSERT_ALWAYS_EQ(run("other:", NULL), 2);
    return 0;
}

static int test_report(int seed, int i) {
    char dir[PF_PATH_MAX];
    const char *file;
    (void)seed, (void)i;
    NEED_SAMPLE();

    PF_ASSERT_ALWAYS_EQ(
        run("--no-color",
            "sample:pass",
            "sample:skip",
            "sample:todo",
            "sample:fail",
            NULL),
        1
    );
    HAS("sample (x1)\n");
    HAS("  pass                     [  OK  ] [ ");
    HAS("  skip                     [ SKIP ] [ ");
    HAS("s ] no network\n");
    HAS("  todo                     [ TODO ]\n");
    HAS("  fail                     [ FAIL ] [ ");
    HAS("s ] failed at iteration 0\n");
    HAS("1 of 2 (50%) tests passed, 1 skipped, 1 to do.");
    LACKS("\x1b[");

    PF_ASSERT_ALWAYS_EQ(run("--color", "sample:pass", NULL), 0);
    HAS("\x1b[32m OK \x1b[0m");
    PF_ASSERT_ALWAYS_EQ(run("--color", "--tap", "sample:pass", NULL), 0);
    LACKS("\x1b[");

    /* Quiet: the exit code only. */
    PF_ASSERT_ALWAYS_EQ(run("-q", "sample:fail", NULL), 1);
    PF_ASSERT_ALWAYS_STR_EQ(output, "");
    PF_ASSERT_ALWAYS_EQ(run("--quiet", "sample:pass", NULL), 0);
    PF_ASSERT_ALWAYS_STR_EQ(output, "");

    /* To a file. */
    test_tmpdir(dir, sizeof(dir));
    file = test_path(0, dir, "report.tap");
    PF_ASSERT_ALWAYS_EQ(run("--tap", "-o", file, "sample:skip", NULL), 0);
    PF_ASSERT_ALWAYS_STR_EQ(output, "");
    PF_ASSERT_ALWAYS(
        strstr(test_read(file), "ok 1 - sample/skip # SKIP") != NULL
    );
    PF_ASSERT_ALWAYS_EQ(run("--output", test_path(1, dir, "x/y"), NULL), 2);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_usage(int seed, int i) {
    (void)seed, (void)i;
    NEED_SAMPLE();

    PF_ASSERT_ALWAYS_EQ(run("--help", NULL), 0);
    HAS("Usage: ");
    HAS("\nother (x2)\n");
    PF_ASSERT_ALWAYS_EQ(run("-l", NULL), 0);
    HAS("sample (x1)\n  pass\n");
    HAS("  todo (TODO)\n");
    LACKS("Usage");
    PF_ASSERT_ALWAYS_EQ(run("--bogus", NULL), 2);
    PF_ASSERT_ALWAYS_STR_EQ(output, "");
    PF_ASSERT_ALWAYS_EQ(run("--isolate=thread", NULL), 2);
    PF_ASSERT_ALWAYS_EQ(run("-o", NULL), 2);
    PF_ASSERT_ALWAYS_EQ(run("--seed", NULL), 2);
    /* The hidden option of spawned children. */
    PF_ASSERT_ALWAYS_EQ(
        run("--pf-test-child", "sample", "nope", "0", "1", "x", NULL), 2
    );
    PF_ASSERT_ALWAYS_EQ(
        run("--pf-test-child", "sample", "pass", "0", "1", "unused", NULL), 0
    );
    return 0;
}

pf_test_t suite_test[] = {
    { test_skip_fn, "skip_fn" }, { test_outcomes, "outcomes" },
    { test_timeout, "timeout" }, { test_repeat_seed, "repeat_seed" },
    { test_select, "select" },   { test_report, "report" },
    { test_usage, "usage" },     { 0 },
};
