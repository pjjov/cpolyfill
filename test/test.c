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

#include "check.h"
#include "util.h"
#include <signal.h>

#ifndef _WIN32
    #include <sys/resource.h>
#endif

#ifdef _WIN32
    #include <io.h>
    #define dup_ _dup
    #define dup2_ _dup2
    #define close_ _close
    #define NULL_DEVICE "NUL"
#else
    #include <fcntl.h>
    #define dup_ dup
    #define dup2_ dup2
    #define close_ close
    #define NULL_DEVICE "/dev/null"
#endif

/* Sanitizers install their own handler for segmentation faults, which turns
   them into a plain failing exit status instead of a signal. */
#if defined(__SANITIZE_ADDRESS__)
    #define SEGV_IS_SIGNAL 0
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define SEGV_IS_SIGNAL 0
    #endif
#endif
#ifndef SEGV_IS_SIGNAL
    #define SEGV_IS_SIGNAL 1
#endif

/* Tests of the test framework itself. The suites below are run in-process,
   with their output sent to a file. */

/* ---------------------------------------------------------------- */
/* Fixtures                                                          */
/* ---------------------------------------------------------------- */

static int calls;
static int last_seed;
static int visited[16];
static int visited_count;

static int t_pass(int seed, int i) {
    (void)seed;
    (void)i;
    calls++;
    return 0;
}

static int t_fail(int seed, int i) {
    (void)seed;
    (void)i;
    calls++;
    return 1;
}

static int t_fail_late(int seed, int i) {
    (void)seed;
    calls++;
    return i == 2;
}

static int t_record(int seed, int i) {
    last_seed = seed;
    if (visited_count < 16)
        visited[visited_count++] = i;
    return 0;
}

static int t_seed_1234(int seed, int i) {
    (void)i;
    return seed == 1234 ? 0 : 1;
}

static int t_seed_16(int seed, int i) {
    (void)i;
    return seed == 16 ? 0 : 1;
}

static int t_abort(int seed, int i) {
    (void)seed;
    (void)i;
    abort();
    return 0;
}

static int t_segv(int seed, int i) {
    (void)seed;
    (void)i;
    *(volatile int *)0 = 1;
    return 0;
}

static int t_exit_256(int seed, int i) {
    (void)seed;
    (void)i;
    return 256;
}

static int t_output(int seed, int i) {
    (void)seed;
    (void)i;
    printf("test output\n");
    return 0;
}

/* clang-format off */
static const pf_test_t alpha_tests[] = {
    { t_pass, "pass" },
    { t_pass, "other" },
    { NULL, "todo" },
    { 0 },
};

static const pf_test_t beta_tests[] = {
    { t_pass, "good" },
    { t_fail, "bad" },
    { 0 },
};

static const pf_suite_t two_suites[] = {
    { "alpha", 3, alpha_tests },
    { "beta", 2, beta_tests },
    { 0 },
};

static const pf_suite_t passing[] = {
    { "alpha", 3, alpha_tests },
    { 0 },
};
/* clang-format on */

/* ---------------------------------------------------------------- */
/* Harness                                                           */
/* ---------------------------------------------------------------- */

struct result {
    int rc;
    char out[4096];
};

static int g_hide_stderr;

static int hide_stderr(void) {
    int saved = dup_(2), null = -1;
    FILE *n;

    if (saved < 0)
        return -1;
    fflush(stderr);
    n = fopen(NULL_DEVICE, "w");
    if (n) {
        null = dup2_(fileno(n), 2);
        fclose(n);
    }
    (void)null;
    return saved;
}

static void restore_stderr(int saved) {
    fflush(stderr);
    dup2_(saved, 2);
    close_(saved);
}

/* Sends everything written to standard output to a file, until stopped. */
static int capture_start(const char *path) {
    int saved = dup_(1);
    FILE *f;

    fflush(stdout);
    f = fopen(path, "w");
    if (saved < 0 || !f) {
        if (f)
            fclose(f);
        return -1;
    }
    dup2_(fileno(f), 1);
    fclose(f);
    return saved;
}

static void capture_stop(int saved) {
    fflush(stdout);
    dup2_(saved, 1);
    close_(saved);
}

/* Runs `pf_test_main` with the arguments, report to a file. */
static void run_args(
    struct result *r, const pf_suite_t *suites, int argc, char **argv
) {
    char path[TEST_PATH_MAX];
    char *full[32];
    int i, n = 0, saved = -1;

    test_path(path, sizeof path, "report");
    full[n++] = "tester";
    full[n++] = "-o";
    full[n++] = path;
    for (i = 0; i < argc && n < 31; i++)
        full[n++] = argv[i];
    full[n] = NULL;

    if (g_hide_stderr)
        saved = hide_stderr();
    r->rc = pf_test_main(n, full, suites);
    if (saved >= 0)
        restore_stderr(saved);

    if (test_read(path, r->out, sizeof r->out) < 0)
        r->out[0] = '\0';
    test_unlink(path);
}

#define RUN(r, suites, ...)                                               \
    do {                                                                  \
        char *args_[] = { __VA_ARGS__, NULL };                            \
        run_args(                                                         \
            (r), (suites), (int)(sizeof args_ / sizeof *args_) - 1, args_ \
        );                                                                \
    } while (0)

static void reset(void) {
    calls = 0;
    last_seed = 0;
    visited_count = 0;
    g_hide_stderr = 0;
}

/* ---------------------------------------------------------------- */
/* Tests                                                             */
/* ---------------------------------------------------------------- */

static int test_framework_pass(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, passing, "--no-fork", "--no-color", "--seed", "1234");
    CHECK(r.rc == 0);
    CHECK(calls == 2 * 3); /* two tests, three repetitions each */
    CHECK(strstr(r.out, "alpha (x3)") != NULL);
    CHECK(strstr(r.out, "pass") != NULL && strstr(r.out, "other") != NULL);
    CHECK(strstr(r.out, "[  OK  ]") != NULL);
    CHECK(strstr(r.out, "FAIL") == NULL);
    CHECK(strstr(r.out, "2 of 2 (100%) tests passed, 1 skipped.") != NULL);
    CHECK(strstr(r.out, "(seed: 0x4d2)") != NULL);
    return 0;
}

static int test_framework_timing(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, passing, "--no-fork", "--no-color");
    /* Every finished test shows how long it took. */
    CHECK(strstr(r.out, "s ]") != NULL);
    CHECK(strstr(r.out, "[ 0.") != NULL);
    return 0;
}

static int test_framework_failure(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, two_suites, "--no-fork", "--no-color", "beta");
    CHECK(r.rc == 1);
    CHECK(strstr(r.out, "good") != NULL);
    CHECK(strstr(r.out, "FAIL") != NULL);
    CHECK(
        strstr(r.out, "test failed (non-zero exit status) at iteration 0")
        != NULL
    );
    CHECK(strstr(r.out, "1 of 2 (50%) tests passed, 0 skipped.") != NULL);
    /* good ran twice, bad stopped after its first failure. */
    CHECK(calls == 2 + 1);
    return 0;
}

static int test_framework_failure_iteration(int seed, int rep) {
    static const pf_test_t tests[] = { { t_fail_late, "late" }, { 0 } };
    static const pf_suite_t suites[] = { { "s", 5, tests }, { 0 } };
    struct result r;

    reset();
    RUN(&r, suites, "--no-fork", "--no-color");
    CHECK(r.rc == 1);
    CHECK(strstr(r.out, "at iteration 2") != NULL);
    CHECK(calls == 3); /* iterations 3 and 4 are not run */
    return 0;
}

static int test_framework_todo(int seed, int rep) {
    static const pf_test_t tests[] = { { t_pass, "real" }, { 0 } };
    static const pf_suite_t suites[] = {
        { "off", 0, tests }, /* a count of zero makes everything TODO */
        { 0 },
    };
    struct result r;

    reset();
    RUN(&r, suites, "--no-fork", "--no-color");
    CHECK(r.rc == 0);
    CHECK(calls == 0);
    CHECK(strstr(r.out, "TODO") != NULL);
    CHECK(strstr(r.out, "0 of 0 (100%) tests passed, 1 skipped.") != NULL);

    reset();
    RUN(&r, passing, "--no-fork", "--no-color");
    CHECK(strstr(r.out, "todo") != NULL);
    CHECK(strstr(r.out, "TODO") != NULL);
    return 0;
}

static int test_framework_repetitions(int seed, int rep) {
    static const pf_test_t tests[] = { { t_record, "record" }, { 0 } };
    static const pf_suite_t suites[] = { { "s", 4, tests }, { 0 } };
    struct result r;

    reset();
    RUN(&r, suites, "--no-fork", "--no-color", "--seed", "77");
    CHECK(r.rc == 0);
    CHECK(visited_count == 4);
    CHECK(
        visited[0] == 0 && visited[1] == 1 && visited[2] == 2 && visited[3] == 3
    );
    CHECK(last_seed == 77);
    return 0;
}

static int test_framework_selection(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, two_suites, "--no-fork", "--no-color", "alpha");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "alpha") != NULL);
    CHECK(strstr(r.out, "beta") == NULL);

    reset();
    RUN(&r, two_suites, "--no-fork", "--no-color", "beta:good");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "good") != NULL);
    CHECK(strstr(r.out, "bad") == NULL);
    CHECK(calls == 2);

    reset();
    RUN(&r, two_suites, "--no-fork", "--no-color", "alpha:other", "beta:good");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "other") != NULL);
    CHECK(strstr(r.out, "  pass ") == NULL);
    CHECK(strstr(r.out, "good") != NULL);

    reset();
    RUN(&r, two_suites, "--no-fork", "--no-color", "all");
    CHECK(r.rc == 1); /* beta:bad fails */
    CHECK(strstr(r.out, "alpha") != NULL && strstr(r.out, "beta") != NULL);

    reset();
    RUN(&r, two_suites, "--no-fork", "--no-color");
    CHECK(strstr(r.out, "alpha") != NULL && strstr(r.out, "beta") != NULL);
    return 0;
}

static int test_framework_selection_errors(int seed, int rep) {
    struct result r;

    reset();
    g_hide_stderr = 1;
    RUN(&r, two_suites, "--no-fork", "gamma");
    CHECK(r.rc == 2);
    CHECK(calls == 0);

    RUN(&r, two_suites, "--no-fork", "alpha:nothing");
    CHECK(r.rc == 2);
    RUN(&r, two_suites, "--no-fork", "alph");
    CHECK(r.rc == 2); /* whole names only */
    RUN(&r, two_suites, "--no-fork", "alpha", "gamma");
    CHECK(r.rc == 2);
    CHECK(calls == 0); /* nothing ran */
    return 0;
}

static int test_framework_list(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, two_suites, "--list");
    CHECK(r.rc == 0);
    CHECK(calls == 0);
    CHECK(strstr(r.out, "alpha (x3)") != NULL);
    CHECK(strstr(r.out, "beta (x2)") != NULL);
    CHECK(strstr(r.out, "  pass\n") != NULL);
    CHECK(strstr(r.out, "  todo (TODO)\n") != NULL);
    CHECK(strstr(r.out, "  bad\n") != NULL);
    CHECK(strstr(r.out, "tests passed") == NULL);

    RUN(&r, two_suites, "-l", "--quiet");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "alpha") != NULL); /* quiet still lists */
    return 0;
}

static int test_framework_tap(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, two_suites, "--no-fork", "--tap", "--seed", "99");
    CHECK(r.rc == 1);
    CHECK(strncmp(r.out, "TAP version 14\n1..5\n# seed: 0x63\n", 33) == 0);
    CHECK(strstr(r.out, "ok 1 - alpha/pass\n") != NULL);
    CHECK(strstr(r.out, "ok 2 - alpha/other\n") != NULL);
    CHECK(strstr(r.out, "not ok 3 - alpha/todo # TODO\n") != NULL);
    CHECK(strstr(r.out, "ok 4 - beta/good\n") != NULL);
    CHECK(strstr(r.out, "not ok 5 - beta/bad\n") != NULL);
    CHECK(
        strstr(r.out, "# test failed (non-zero exit status) at iteration 0\n")
        != NULL
    );
    /* No colors and no human readable summary in TAP. */
    CHECK(strchr(r.out, '\033') == NULL);
    CHECK(strstr(r.out, "tests passed") == NULL);
    return 0;
}

static int test_framework_tap_selection(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, two_suites, "--no-fork", "--tap", "beta:good");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "1..1\n") != NULL);
    CHECK(strstr(r.out, "ok 1 - beta/good\n") != NULL);
    return 0;
}

static int test_framework_quiet(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, passing, "--no-fork", "--quiet");
    CHECK(r.rc == 0);
    CHECK(r.out[0] == '\0');
    CHECK(calls == 6);

    RUN(&r, two_suites, "--no-fork", "-q");
    CHECK(r.rc == 1); /* still reported through the exit code */
    CHECK(r.out[0] == '\0');
    return 0;
}

static int test_framework_seed(int seed, int rep) {
    static const pf_test_t a[] = { { t_seed_1234, "dec" }, { 0 } };
    static const pf_test_t b[] = { { t_seed_16, "hex" }, { 0 } };
    static const pf_suite_t sa[] = { { "s", 1, a }, { 0 } };
    static const pf_suite_t sb[] = { { "s", 1, b }, { 0 } };
    struct result r;

    reset();
    RUN(&r, sa, "--no-fork", "--seed", "1234");
    CHECK(r.rc == 0);
    RUN(&r, sa, "--no-fork", "--seed=1234");
    CHECK(r.rc == 0);
    RUN(&r, sa, "--no-fork", "--seed", "1235");
    CHECK(r.rc == 1);

    /* Parsed with the usual C prefixes. */
    RUN(&r, sb, "--no-fork", "--seed", "0x10");
    CHECK(r.rc == 0);
    RUN(&r, sb, "--no-fork", "--seed=020");
    CHECK(r.rc == 0);

    /* Without a seed one is made up, and never zero. */
    reset();
    {
        static const pf_test_t t[] = { { t_record, "r" }, { 0 } };
        static const pf_suite_t s[] = { { "s", 1, t }, { 0 } };
        RUN(&r, s, "--no-fork");
        CHECK(r.rc == 0);
        CHECK(last_seed != 0);
        CHECK(strstr(r.out, "(seed: 0x") != NULL);
    }
    return 0;
}

static int test_framework_usage_errors(int seed, int rep) {
    struct result r;

    reset();
    g_hide_stderr = 1;
    RUN(&r, passing, "--bogus");
    CHECK(r.rc == 2);
    RUN(&r, passing, "-z");
    CHECK(r.rc == 2);
    RUN(&r, passing, "--seed");
    CHECK(r.rc == 2);
    RUN(&r, passing, "--seed", "abc");
    CHECK(r.rc == 2);
    RUN(&r, passing, "--seed=");
    CHECK(r.rc == 2);
    RUN(&r, passing, "--seed", "12x");
    CHECK(r.rc == 2);
    RUN(&r, passing, "-o");
    CHECK(r.rc == 2);
    CHECK(calls == 0);
    return 0;
}

static int test_framework_output_file_error(int seed, int rep) {
    char dir[TEST_PATH_MAX], file[TEST_PATH_MAX];
    char *argv[4];
    int rc, saved;

    test_path(dir, sizeof dir, "no-such-dir");
    test_join(file, sizeof file, dir, "report");
    argv[0] = "tester";
    argv[1] = "--output";
    argv[2] = file;
    argv[3] = NULL;

    saved = hide_stderr();
    rc = pf_test_main(3, argv, passing);
    if (saved >= 0)
        restore_stderr(saved);
    CHECK(rc == 2);
    return 0;
}

static int test_framework_help(int seed, int rep) {
    char path[TEST_PATH_MAX], text[4096];
    char *argv[3];
    int rc, saved;
    reset();

    test_path(path, sizeof path, "help");
    argv[0] = "tester";
    argv[1] = "--help";
    argv[2] = NULL;

    saved = capture_start(path);
    CHECK(saved >= 0);
    rc = pf_test_main(2, argv, two_suites);
    capture_stop(saved);
    CHECK(rc == 0);
    CHECK(calls == 0);

    CHECK(test_read(path, text, sizeof text) > 0);
    CHECK(strstr(text, "Usage: tester [options] [suite[:test]...]") != NULL);
    CHECK(strstr(text, "--tap") != NULL);
    CHECK(strstr(text, "--seed") != NULL);
    CHECK(strstr(text, "alpha (x3)") != NULL);
    CHECK(strstr(text, "  todo (TODO)") != NULL);

    argv[1] = "-h";
    saved = capture_start(path);
    CHECK(saved >= 0);
    rc = pf_test_main(2, argv, two_suites);
    capture_stop(saved);
    CHECK(rc == 0);
    test_unlink(path);
    return 0;
}

static int test_framework_double_dash(int seed, int rep) {
    struct result r;

    reset();
    /* After "--" everything is a test selection, even options. */
    RUN(&r, two_suites, "--no-fork", "--no-color", "--", "alpha");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "alpha") != NULL);
    CHECK(strstr(r.out, "beta") == NULL);

    g_hide_stderr = 1;
    RUN(&r, two_suites, "--no-fork", "--", "--tap");
    CHECK(r.rc == 2);
    return 0;
}

static int test_framework_color(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, two_suites, "--no-fork", "--color");
    CHECK(strstr(r.out, "\033[32m OK \033[0m") != NULL);
    CHECK(strstr(r.out, "\033[31mFAIL\033[0m") != NULL);
    CHECK(strstr(r.out, "\033[33mTODO\033[0m") != NULL);

    RUN(&r, two_suites, "--no-fork", "--no-color");
    CHECK(strchr(r.out, '\033') == NULL);

    /* Reports written to a file are plain by default. */
    RUN(&r, two_suites, "--no-fork");
    CHECK(strchr(r.out, '\033') == NULL);

    /* Colors have no place in TAP. */
    RUN(&r, two_suites, "--no-fork", "--tap", "--color");
    CHECK(strchr(r.out, '\033') == NULL);
    return 0;
}

static int test_framework_empty(int seed, int rep) {
    static const pf_suite_t none[] = { { 0 } };
    static const pf_suite_t empty_suite[] = {
        { "empty", 3, NULL },
        { 0 },
    };
    static const pf_test_t no_tests[] = { { 0 } };
    static const pf_suite_t empty_list[] = { { "nothing", 3, no_tests },
                                             { 0 } };
    struct result r;
    char *argv[2];

    reset();
    RUN(&r, none, "--no-fork");
    CHECK(r.rc == 0);
    CHECK(strstr(r.out, "0 of 0 (100%) tests passed, 0 skipped.") != NULL);

    RUN(&r, empty_suite, "--no-fork");
    CHECK(r.rc == 0);
    RUN(&r, empty_list, "--no-fork");
    CHECK(r.rc == 0);

    /* No suites at all is not an error. */
    argv[0] = "tester";
    argv[1] = NULL;
    CHECK(pf_test_main(1, argv, NULL) == 0);
    return 0;
}

#ifndef PF_TEST_NO_FORK

static int test_framework_fork_isolation(int seed, int rep) {
    static const pf_test_t tests[] = { { t_pass, "pass" }, { 0 } };
    static const pf_suite_t suites[] = { { "s", 3, tests }, { 0 } };
    struct result r;

    /* Forked tests count in their own process: ours is untouched. */
    reset();
    RUN(&r, suites, "--fork", "--no-color");
    CHECK(r.rc == 0);
    CHECK(calls == 0);
    CHECK(strstr(r.out, "1 of 1 (100%) tests passed") != NULL);

    reset();
    RUN(&r, suites, "--no-fork", "--no-color");
    CHECK(calls == 3);
    return 0;
}

static int test_framework_fork_crashes(int seed, int rep) {
    static const pf_test_t tests[] = {
        { t_abort, "abort" }, { t_segv, "segv" }, { t_exit_256, "wrap" },
        { t_fail, "fail" },   { t_pass, "pass" }, { 0 },
    };
    static const pf_suite_t suites[] = { { "crash", 2, tests }, { 0 } };
    struct result r;

    #ifndef _WIN32
    /* Crashing on purpose should not leave core files behind. */
    {
        struct rlimit none = { 0, 0 };
        setrlimit(RLIMIT_CORE, &none);
    }
    #endif

    reset();
    RUN(&r, suites, "--fork", "--no-color");
    CHECK(r.rc == 1);

    /* A crash fails that test, without taking the others down. */
    CHECK(strstr(r.out, "signal 6: aborted at iteration 0") != NULL);
    #if SEGV_IS_SIGNAL
    CHECK(
        strstr(r.out, "signal 11: segmentation fault at iteration 0") != NULL
    );
    #else
    CHECK(strstr(r.out, "segv") != NULL);
    #endif
    /* 256 would be 0 as an exit code; it still has to count as failure. */
    CHECK(strstr(r.out, "wrap") != NULL);
    {
        const char *wrap = strstr(r.out, "wrap");
        const char *fail = strstr(wrap, "FAIL");
        const char *nl = strchr(wrap, '\n');
        CHECK(fail != NULL && fail < nl);
    }
    CHECK(strstr(r.out, "1 of 5 (20%) tests passed") != NULL);
    return 0;
}

static int test_framework_fork_output(int seed, int rep) {
    static const pf_test_t tests[] = { { t_output, "print" }, { 0 } };
    static const pf_suite_t suites[] = { { "s", 2, tests }, { 0 } };
    char path[TEST_PATH_MAX], text[1024];
    struct result r;
    int saved, count = 0;
    const char *p;

    test_path(path, sizeof path, "fork-output");
    reset();

    saved = capture_start(path);
    CHECK(saved >= 0);
    printf("before\n"); /* sits in the buffer when the tests fork */
    RUN(&r, suites, "--fork", "--no-color");
    capture_stop(saved);
    CHECK(r.rc == 0);

    CHECK(test_read(path, text, sizeof text) > 0);
    for (p = text; (p = strstr(p, "before")) != NULL; p++)
        count++;
    CHECK(count == 1); /* not repeated by the children */
    count = 0;
    for (p = text; (p = strstr(p, "test output")) != NULL; p++)
        count++;
    CHECK(count == 2); /* once per repetition */
    test_unlink(path);
    return 0;
}

#else

static int test_framework_fork_unsupported(int seed, int rep) {
    struct result r;

    reset();
    g_hide_stderr = 1;
    RUN(&r, passing, "--fork");
    CHECK(r.rc == 2);
    return 0;
}

#endif

static int test_framework_exit_code_values(int seed, int rep) {
    struct result r;

    reset();
    RUN(&r, passing, "--no-fork");
    CHECK(r.rc == 0);
    RUN(&r, two_suites, "--no-fork");
    CHECK(r.rc == 1);
    g_hide_stderr = 1;
    RUN(&r, two_suites, "--nonsense");
    CHECK(r.rc == 2);
    return 0;
}

pf_test_t suite_test[] = {
    { test_framework_pass, "/test/pass" },
    { test_framework_timing, "/test/timing" },
    { test_framework_failure, "/test/failure" },
    { test_framework_failure_iteration, "/test/failure_iteration" },
    { test_framework_todo, "/test/todo" },
    { test_framework_repetitions, "/test/repetitions" },
    { test_framework_selection, "/test/selection" },
    { test_framework_selection_errors, "/test/selection_errors" },
    { test_framework_list, "/test/list" },
    { test_framework_tap, "/test/tap" },
    { test_framework_tap_selection, "/test/tap_selection" },
    { test_framework_quiet, "/test/quiet" },
    { test_framework_seed, "/test/seed" },
    { test_framework_usage_errors, "/test/usage_errors" },
    { test_framework_output_file_error, "/test/output_file_error" },
    { test_framework_help, "/test/help" },
    { test_framework_double_dash, "/test/double_dash" },
    { test_framework_color, "/test/color" },
    { test_framework_empty, "/test/empty" },
#ifndef PF_TEST_NO_FORK
    { test_framework_fork_isolation, "/test/fork_isolation" },
    { test_framework_fork_crashes, "/test/fork_crashes" },
    { test_framework_fork_output, "/test/fork_output" },
#else
    { test_framework_fork_unsupported, "/test/fork_unsupported" },
#endif
    { test_framework_exit_code_values, "/test/exit_codes" },
    { 0 },
};
