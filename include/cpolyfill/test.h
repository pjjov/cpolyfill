/** cpolyfill - Filling the gaps between C standards and compilers

    A small unit-test runner: tests are grouped into suites, selected and
    configured from the command line, run in a separate process each (so a
    crash fails one test, not the run), timed, and reported as text or TAP.
    Checks inside tests are up to you, such as PF_ASSERT_ALWAYS* from
    <cpolyfill/assert.h>.

        static int test_add(int seed, int i) {
            (void)seed, (void)i;
            PF_ASSERT_ALWAYS_EQ(1 + 1, 2);
            return PF_TEST_PASS;
        }

        static int test_net(int seed, int i) {
            (void)seed, (void)i;
            return pf_test_skip("no network");
        }

        static const pf_test_t math_tests[] = {
            { test_add, "add" },
            { NULL, "div" },            // TODO
            { 0 },
        };

        static const pf_suite_t suites[] = {
            { "math", 100, math_tests },
            { 0 },
        };

        int main(int argc, char **argv) {
            return pf_test_main(argc, argv, suites);
        }

    Types:

        typedef int (*pf_test_fn)(int seed, int i)
                A test: `seed` comes from the command line (or the time),
                `i` counts the repetitions from 0. Returns PF_TEST_PASS (0),
                PF_TEST_SKIP (77, see pf_test_skip), or anything else to
                fail. A failed assertion fails it too.
        pf_test_t               fn    the test; NULL marks it TODO
                                name  NULL ends the list
        pf_suite_t              name  NULL ends the list; no ':'
                                count how often each test is run, 0 marks
                                      them all TODO
                                tests the tests, ended by a NULL name

    Functions:

        int pf_test_main(int argc, char **argv, const pf_suite_t *suites)
                Runs the suites as the command line says (below). Returns
                the exit code: 0 if no selected test failed, 1 if one did,
                2 for a usage error (or when isolated tests cannot be set
                up: no program path or temporary file). Meant to be called from main with its
                arguments unchanged: isolated tests on Windows run in a
                copy of the program started with a hidden option, which
                this function handles.
        int pf_test_skip(const char *fmt, ...)
                Records why the test is skipped (printf format) and returns
                PF_TEST_SKIP: `return pf_test_skip("no %s", what);`. The
                remaining repetitions are not run.

    Command line: `prog [options] [suite[:test]...]`, everything without
    selections (or with `all`). Selections may use the wildcards `*` and
    `?`, such as `socket:*addr*`; one that matches no test is an error.

        -h, --help              usage, with the suites and tests
        -l, --list              list the suites and tests
        --tap                   Test Anything Protocol (version 14)
        -q, --quiet             no report, only the exit code
        --color, --no-color     default: pf_term_colors (NO_COLOR, ...)
                                for a terminal, none for a file or pipe
        --isolate[=fork|spawn]  run each repetition in a child process
                                (the default): fork on POSIX, spawn (start
                                the program again) on Windows
        --no-isolate            run the tests in this process; a failed
                                assertion fails the test (the handler of
                                <cpolyfill/assert.h> jumps back), a crash
                                ends the run
        --fork, --no-fork       the same as --isolate and --no-isolate
        --timeout SECONDS       end an isolated test that runs longer
                                (decimals allowed; default 0: no limit)
        -r, --repeat N          run each test N times, whatever the suites
                                say
        --seed N                the seed passed to tests (default: time)
        -o, --output FILE       write the report to FILE

    Notes:
    - Output of a test goes where the program's goes, between the report
      lines; a test that writes to stdout breaks TAP output to stdout
      (use -o, or stderr).
    - A test returning PF_TEST_SKIP is reported as SKIP (in TAP `ok ...
      # SKIP reason`), also by `meson test`, which takes 77 to mean a skip.
    - In an isolated child, the assertion handler is replaced by one that
      prints the message and exits; a test that installs its own handler
      should restore the previous one (pf_set_assert_handler returns it),
      not NULL.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_TEST_H
#define CPOLYFILL_TEST_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>

#include <stddef.h>

#define PF_TEST_PASS 0
#define PF_TEST_FAIL 1
#define PF_TEST_SKIP 77

PF_BEGIN_DECLS

typedef int (*pf_test_fn)(int seed, int i);

typedef struct pf_test_t {
    pf_test_fn fn;
    const char *name;
} pf_test_t;

typedef struct pf_suite_t {
    const char *name;
    int count;
    const pf_test_t *tests;
} pf_suite_t;

PF_API int pf_test_main(int argc, char **argv, const pf_suite_t *suites);
PF_PRINTF(1, 2) PF_API int pf_test_skip(const char *fmt, ...);

PF_END_DECLS

#endif /* CPOLYFILL_TEST_H */
