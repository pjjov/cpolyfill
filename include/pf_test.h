/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a minimal unit testing framework for C,
    capable of running forked tests and tracking execution time.
    The library is barebones on purpose, expecting you to bring
    your own utilities, like custom assertions with `pf_assert.h`.

    For POSIX systems tests are run with `fork` if possible.
    You can define PF_TEST_NO_FORK to disable forking.

    Tests are run multiple times, depending on their `count`. If count
    is zero or test's callback is NULL, the test is marked as TODO.
    Current repetion index is passed to test functions, alongside a
    seed for randomization, which can be supplied by the user.

    The `Test Anything Protocol` is used by `pf_suite_run_tap`.

    If you want to see `pf_test.h` in action, you can take a look
    in the `test` directory of the 'cpolyfill' repository.

    The API reference:
    ```c
    typedef int (*pf_test_fn)(int seed, unsigned int i);
    typedef struct pf_test {
        pf_test_fn fn;
        const char *name;
        unsigned int count;
    } pf_test;

    int pf_test_run(const pf_test *test, int seed);
    int pf_suite_run(const pf_test *tests, int seed, FILE *out);
    void pf_suite_run_tap(const pf_test *tests, int seed, FILE *out);
    int pf_suite_run_all(const pf_test **suites, int seed, FILE *out);
    ```

    You can define following configuration macros:
    - PF_TEST_NO_COLOR
    - PF_TEST_NO_FORK

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef POLYFILL_TEST
#define POLYFILL_TEST

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef int (*pf_test_fn)(int seed, int i);
typedef struct pf_test {
    pf_test_fn fn; /* test function */
    const char *name; /* name of the test */
    int count; /* number of repetitions */
} pf_test;

#ifndef PF_TEST_NO_COLOR
    #define PF_TEST_TODO "\x1b[33mTODO\x1b[0m"
    #define PF_TEST_OK "\x1b[32m OK \x1b[0m"
    #define PF_TEST_FAIL "\x1b[31mFAIL\x1b[0m"
#else
    #define PF_TEST_TODO "TODO"
    #define PF_TEST_OK " OK "
    #define PF_TEST_FAIL "FAIL"
#endif

#ifndef __unix__
    #define PF_TEST_NO_FORK
#endif

#ifndef PF_TEST_NO_FORK
    #include <sys/wait.h>
    #include <unistd.h>
#endif

static const char *pf__test_message(int signal) {
    switch (signal) {
    case SIGTERM:
        return "Signal %d: Termination signal in %s!";
    case SIGSEGV:
        return "Signal %d: Segmentation fault occured in %s!";
    case SIGINT:
        return "Signal %d: Interrupt occured in %s!";
    case SIGILL:
        return "Signal %d: Illegal instruction in %s!";
    case SIGABRT:
        return "Signal %d: Aborted in %s!\n";
    case SIGFPE:
        return "Signal %d: Arithmetic exception in %s!";
    case -1:
        return "Signal %d: Test %s exited abnormally!";
    default:
        return "Signal %d: Unknown signal appeared in %s!";
    }
}

static int pf__test_exec(const pf_test *test, int seed, unsigned int i) {
#ifndef PF_TEST_NO_FORK
    pid_t testPid = fork();

    if (testPid == 0)
        exit(test->fn(seed, i));

    if (testPid != -1) {
        /* parent process */
        int status;
        pid_t newPid = waitpid(testPid, &status, 0);

        if (testPid == newPid && WIFEXITED(status))
            return WEXITSTATUS(status) != EXIT_SUCCESS ? -1 : 0;
        else if (WIFSIGNALED(status))
            return WTERMSIG(status);
        else if (WIFSTOPPED(status))
            return WSTOPSIG(status);

        return -1;
    }
#endif

    return test->fn(seed, i);
}

static inline int pf_test_run(const pf_test *test, int seed) {
    if (!test || !test->name || !test->fn)
        return -1;

    if (seed == 0)
        seed = time(NULL);

    int i, result = 0;
    for (i = 0; i < test->count && !result; i++)
        result = pf__test_exec(test, seed, i);

#ifdef PF_TEST_NO_FORK
    if (result)
        exit(result);
#endif
    return result;
}

static inline int pf_suite_run(const pf_test *tests, int seed, FILE *out) {
    if (!tests)
        return 0;

    if (!out)
        out = stdout;

    int i, skip = 0, fail = 0;
    for (i = 0; tests[i].name; i++) {
        if (!tests[i].fn || tests[i].count == 0) {
            fprintf(stdout, "%-24s [ %s ]\n", tests[i].name, PF_TEST_TODO);
            skip++;
            continue;
        }

        double elapsed, start = clock();
        int result = pf_test_run(&tests[i], seed);
        elapsed = ((double)clock() - start) / CLOCKS_PER_SEC;

        fprintf(
            out,
            "%-24s [ %s ] [ %.6fs ]",
            tests[i].name,
            result ? PF_TEST_FAIL : PF_TEST_OK,
            elapsed
        );

        if (result) {
            fprintf(out, pf__test_message(result), result, tests[i].name);
            fail++;
        }

        fputc('\n', out);
    }

    fprintf(
        out,
        "%u of %u (%u%%) tests passed, %u skipped. (seed: %#x)\n",
        i - fail,
        i,
        (i - fail) * 100 / i,
        skip,
        seed
    );

    return fail;
}

static inline int pf_suite_run_all(
    const pf_test **suites, int seed, FILE *out
) {
    if (!suites)
        return 0;

    int i, fail = 0;
    for (i = 0; suites[i]; i++)
        fail += pf_suite_run(suites[i], seed, out);

    fprintf(
        out ? out : stdout,
        "total: failed %d tests across %d suites!\n",
        fail,
        i
    );
    return fail;
}

static inline void pf_suite_run_tap(const pf_test *tests, int seed, FILE *out) {
    if (!tests)
        return;

    if (!out)
        out = stdout;

    int i = 0;
    fputs("TAP version 14\n", out);
    for (i = 0; tests[i].name; i++) {
        if (!tests[i].fn || tests[i].count == 0) {
            fprintf(out, "not ok %d - %s # TODO\n", i + 1, tests[i].name);
        } else {
            int result = pf_test_run(&tests[i], seed);
            const char *fmt = result ? "not ok %d - %s\n" : "ok %d - %s\n";
            fprintf(out, fmt, i + 1, tests[i].name);
        }
    }

    fprintf(out, "1..%d\n", i);
}

#endif
