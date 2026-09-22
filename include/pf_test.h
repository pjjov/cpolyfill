/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a minimal unit testing framework for C,
    capable of running forked tests and tracking execution time.
    The library is barebones on purpose, expecting you to bring
    your own utilities, like custom assertions with `pf_assert.h`.

    Tests are grouped into suites. A suite has a name, an iteration
    count and a NULL-terminated (name == NULL) list of tests. Every
    test of a suite is run `count` times. The current repetition
    index is passed to the test function, alongside a seed for
    randomization, which can be supplied by the user.

    A test is marked as TODO if its callback is NULL or if the count
    of its suite is zero. A test passes by returning zero.

    Everything is driven by a single function, `pf_test_main`, which
    is meant to be called straight from `main`:
    ```c
    static int test_add(int seed, int i) {
        (void)seed; (void)i;
        return 1 + 1 == 2 ? 0 : 1;
    }

    static const pf_test_t math_tests[] = {
        {test_add, "add"},
        {NULL, "div"}, // TODO
        {0}
    };

    static const pf_suite_t suites[] = {
        {"math", 100, math_tests},
        {0}
    };

    int main(int argc, char **argv) {
        return pf_test_main(argc, argv, suites);
    }
    ```

    The API reference:
    ```c
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

    int pf_test_main(int argc, char **argv, const pf_suite_t *suites);
    ```

    Command line usage: `prog [options] [suite[:test]...]`
    Without any suite arguments (or with `all`) everything is run.
    - -h, --help     print usage and exit
    - -l, --list     list suites and tests without running them
    - --tap          use the Test Anything Protocol for output
    - -q, --quiet    silent mode, only the exit code reports the result
    - --color        force colored output (default: only for terminals,
                     honors the NO_COLOR environment variable)
    - --no-color     disable colored output
    - --fork         run each test in a child process (default on POSIX)
    - --no-fork      run tests in the current process
    - --seed N       seed passed to tests (default: current time)
    - -o, --output F write the report to file F instead of stdout

    Exit code: 0 if all tests passed, 1 if any failed, 2 on usage errors.

    Notes:
    - Without forking a crashing test takes the whole runner down.
    - Suite names must not contain ':' (used as suite:test separator).
    - Requires C99. On POSIX, compile with `-std=gnu99` (or newer), or
      define `_POSIX_C_SOURCE` >= 200112L, so that `fork` and friends
      are visible. Timing uses `timespec_get` (C11) if available and
      falls back to `clock` otherwise.

    You can define following configuration macros:
    - PF_TEST_NO_COLOR: never emit colors
    - PF_TEST_NO_FORK: never fork (`--fork` becomes an error)

    Last-updated: September 2026
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
**/

#ifndef POLYFILL_TEST
#define POLYFILL_TEST

#ifndef PF_API
    #define PF_API static inline
#endif

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
    #define PF__TEST_POSIX
    #include <unistd.h>
#else
    #ifndef PF_TEST_NO_FORK
        #define PF_TEST_NO_FORK
    #endif
#endif

#ifndef PF_TEST_NO_FORK
    #include <sys/wait.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*pf_test_fn)(int seed, int i);

typedef struct pf_test_t {
    pf_test_fn fn;    /* test function, NULL marks the test as TODO */
    const char *name; /* name of the test, NULL terminates the list */
} pf_test_t;

typedef struct pf_suite_t {
    const char *name;       /* name of the suite, NULL terminates the list */
    int count;              /* number of repetitions of each test, 0 = TODO */
    const pf_test_t *tests; /* list of tests, terminated by a NULL name */
} pf_suite_t;

/* ------------------------------------------------------------------ */
/* Internals                                                          */
/* ------------------------------------------------------------------ */

typedef struct pf__config {
    FILE *out; /* NULL in silent mode */
    int tap;
    int color;
    int fork;
    int seed;
    const char **specs; /* selected suites, "suite" or "suite:test" */
    int nspecs;
} pf__config;

enum { PF__OK, PF__FAIL, PF__TODO };

PF_API void pf__say(const pf__config *cfg, const char *fmt, ...) {
    va_list ap;
    if (!cfg->out)
        return;
    va_start(ap, fmt);
    vfprintf(cfg->out, fmt, ap);
    va_end(ap);
}

PF_API const char *pf__label(const pf__config *cfg, int kind) {
    static const char *const plain[] = { " OK ", "FAIL", "TODO" };
    static const char *const color[] = { "\x1b[32m OK \x1b[0m",
                                         "\x1b[31mFAIL\x1b[0m",
                                         "\x1b[33mTODO\x1b[0m" };
    return (cfg->color ? color : plain)[kind];
}

/* Wall clock time in seconds. `clock` only measures the CPU time of the
   calling process, which is meaningless when tests run in children. */
PF_API double pf__now(void) {
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* Result codes: 0 = success, >0 = signal number, -1 = failed or exited
   abnormally, -2 = could not fork. */
PF_API const char *pf__reason(int result) {
    switch (result) {
    case -2:
        return "could not fork the test process";
    case -1:
        return "test failed (non-zero exit status)";
    case SIGTERM:
        return "termination signal";
    case SIGSEGV:
        return "segmentation fault";
    case SIGINT:
        return "interrupted";
    case SIGILL:
        return "illegal instruction";
    case SIGABRT:
        return "aborted";
    case SIGFPE:
        return "arithmetic exception";
#ifdef SIGBUS
    case SIGBUS:
        return "bus error";
#endif
#ifdef SIGKILL
    case SIGKILL:
        return "killed";
#endif
#ifdef SIGPIPE
    case SIGPIPE:
        return "broken pipe";
#endif
#ifdef SIGALRM
    case SIGALRM:
        return "alarm clock";
#endif
    default:
        return "unknown signal";
    }
}

PF_API void pf__describe(char *buf, size_t size, int result, int at) {
    if (result > 0) {
        snprintf(
            buf,
            size,
            "signal %d: %s at iteration %d",
            result,
            pf__reason(result),
            at
        );
    } else {
        snprintf(buf, size, "%s at iteration %d", pf__reason(result), at);
    }
}

/* Runs a single iteration of a test. */
PF_API int pf__exec(const pf__config *cfg, const pf_test_t *test, int i) {
#ifndef PF_TEST_NO_FORK
    if (cfg->fork) {
        int status = 0;
        pid_t pid, done;

        /* flush every stream, or the child would duplicate buffered output */
        fflush(NULL);
        pid = fork();

        if (pid < 0)
            return -2;

        if (pid == 0) {
            /* normalize the code, so 256 does not look like success */
            int r = test->fn(cfg->seed, i);
            fflush(NULL);
            _exit(r ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        do {
            done = waitpid(pid, &status, 0);
        } while (done < 0 && errno == EINTR);

        if (done != pid)
            return -1;
        if (WIFSIGNALED(status))
            return WTERMSIG(status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
            return 0;
        return -1;
    }
#endif
    return test->fn(cfg->seed, i) ? -1 : 0;
}

/* Runs all iterations, stops at the first failure. */
PF_API int pf__run_test(
    const pf__config *cfg,
    const pf_suite_t *suite,
    const pf_test_t *test,
    int *at
) {
    for (int i = 0; i < suite->count; i++) {
        int r = pf__exec(cfg, test, i);
        if (r) {
            *at = i;
            return r;
        }
    }
    return 0;
}

PF_API int pf__match(const char *spec, const char *suite, const char *test) {
    const char *colon = strchr(spec, ':');
    size_t n = colon ? (size_t)(colon - spec) : strlen(spec);
    if (strlen(suite) != n || strncmp(spec, suite, n) != 0)
        return 0;
    return !colon || (test && strcmp(colon + 1, test) == 0);
}

PF_API int pf__selected(
    const pf__config *cfg, const pf_suite_t *suite, const pf_test_t *test
) {
    int i;
    if (cfg->nspecs == 0)
        return 1;
    for (i = 0; i < cfg->nspecs; i++)
        if (strcmp(cfg->specs[i], "all") == 0
            || pf__match(cfg->specs[i], suite->name, test->name))
            return 1;
    return 0;
}

PF_API int pf__spec_known(const char *spec, const pf_suite_t *suites) {
    int s, t;
    if (strcmp(spec, "all") == 0)
        return 1;
    for (s = 0; suites[s].name; s++) {
        if (pf__match(spec, suites[s].name, NULL))
            return 1;
        for (t = 0; suites[s].tests && suites[s].tests[t].name; t++)
            if (pf__match(spec, suites[s].name, suites[s].tests[t].name))
                return 1;
    }
    return 0;
}

PF_API int pf__run(const pf__config *cfg, const pf_suite_t *suites) {
    int s, t, total = 0, n = 0, pass = 0, fail = 0, todo = 0;

    if (cfg->tap) {
        for (s = 0; suites[s].name; s++)
            for (t = 0; suites[s].tests && suites[s].tests[t].name; t++)
                total += pf__selected(cfg, &suites[s], &suites[s].tests[t]);
        pf__say(
            cfg,
            "TAP version 14\n1..%d\n# seed: %#x\n",
            total,
            (unsigned)cfg->seed
        );
    }

    for (s = 0; suites[s].name; s++) {
        const pf_suite_t *su = &suites[s];
        int header = 0;

        for (t = 0; su->tests && su->tests[t].name; t++) {
            const pf_test_t *te = &su->tests[t];
            int result, at = 0;
            double start, elapsed;
            char msg[96];

            if (!pf__selected(cfg, su, te))
                continue;
            n++;

            if (!cfg->tap && !header) {
                pf__say(cfg, "%s (x%d)\n", su->name, su->count);
                header = 1;
            }

            if (!te->fn || su->count <= 0) {
                todo++;
                if (cfg->tap)
                    pf__say(
                        cfg, "not ok %d - %s/%s # TODO\n", n, su->name, te->name
                    );
                else
                    pf__say(
                        cfg,
                        "  %-24s [ %s ]\n",
                        te->name,
                        pf__label(cfg, PF__TODO)
                    );
                continue;
            }

            start = pf__now();
            result = pf__run_test(cfg, su, te, &at);
            elapsed = pf__now() - start;

            if (result)
                fail++;
            else
                pass++;
            if (result)
                pf__describe(msg, sizeof msg, result, at);

            if (cfg->tap) {
                pf__say(
                    cfg,
                    "%s %d - %s/%s\n",
                    result ? "not ok" : "ok",
                    n,
                    su->name,
                    te->name
                );
                if (result)
                    pf__say(cfg, "# %s\n", msg);
            } else {
                pf__say(
                    cfg,
                    "  %-24s [ %s ] [ %.6fs ]",
                    te->name,
                    pf__label(cfg, result ? PF__FAIL : PF__OK),
                    elapsed
                );
                if (result)
                    pf__say(cfg, " %s", msg);
                pf__say(cfg, "\n");
            }
        }
    }

    if (!cfg->tap) {
        int ran = pass + fail;
        pf__say(
            cfg,
            "%d of %d (%d%%) tests passed, %d skipped. (seed: %#x)\n",
            pass,
            ran,
            ran ? pass * 100 / ran : 100,
            todo,
            (unsigned)cfg->seed
        );
    }
    return fail;
}

PF_API void pf__list(
    const pf__config *cfg, const pf_suite_t *suites, FILE *to
) {
    int s, t;
    (void)cfg;
    for (s = 0; suites[s].name; s++) {
        fprintf(to, "%s (x%d)\n", suites[s].name, suites[s].count);
        for (t = 0; suites[s].tests && suites[s].tests[t].name; t++)
            fprintf(
                to,
                "  %s%s\n",
                suites[s].tests[t].name,
                suites[s].tests[t].fn ? "" : " (TODO)"
            );
    }
}

PF_API void pf__usage(const char *prog, const pf_suite_t *suites, FILE *to) {
    pf__config none;
    memset(&none, 0, sizeof none);
    fprintf(
        to,
        "Usage: %s [options] [suite[:test]...]\n"
        "\n"
        "Options:\n"
        "  -h, --help          show this help\n"
        "  -l, --list          list suites and tests\n"
        "      --tap           use the Test Anything Protocol\n"
        "  -q, --quiet         silent, report only by exit code\n"
        "      --color         force colored output\n"
        "      --no-color      disable colored output\n"
        "      --fork          run tests in child processes\n"
        "      --no-fork       run tests in this process\n"
        "      --seed N        seed passed to tests (default: time)\n"
        "  -o, --output FILE   write the report to FILE\n"
        "\n"
        "Suites and tests:\n",
        prog
    );
    pf__list(&none, suites, to);
}

/* Matches `--name value` and `--name=value`. Returns 1 on match, 0 when
   `a` is a different option, -1 when the value is missing. */
PF_API int pf__valopt(
    const char *a,
    const char *name,
    int argc,
    char **argv,
    int *i,
    const char **val
) {
    size_t n = strlen(name);
    if (strncmp(a, name, n) != 0)
        return 0;
    if (a[n] == '=') {
        *val = a + n + 1;
        return 1;
    }
    if (a[n] != '\0')
        return 0;
    if (*i + 1 >= argc)
        return -1;
    *val = argv[++*i];
    return 1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/* Runs the suites as configured by the command line. Returns 0 if all
   selected tests passed, 1 if any failed and 2 on usage errors. */
PF_API int pf_test_main(int argc, char **argv, const pf_suite_t *suites) {
    pf__config cfg;
    const char *prog = (argc > 0 && argv && argv[0]) ? argv[0] : "test";
    const char *outpath = NULL, *err = NULL, *erra = "";
    int i, rc = 2, quiet = 0, list = 0, color = -1, forking = -1;
    FILE *file = NULL;

    if (!suites)
        return 0;

    memset(&cfg, 0, sizeof cfg);
    cfg.specs = (const char **)malloc(
        sizeof *cfg.specs * (argc > 0 ? argc : 1)
    );
    if (!cfg.specs) {
        fprintf(stderr, "%s: out of memory\n", prog);
        return 2;
    }

    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *val = NULL;
        int m;

        if (a[0] != '-' || a[1] == '\0') {
            cfg.specs[cfg.nspecs++] = a;
        } else if (strcmp(a, "--") == 0) {
            for (i++; i < argc; i++)
                cfg.specs[cfg.nspecs++] = argv[i];
            break;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            pf__usage(prog, suites, stdout);
            rc = 0;
            goto done;
        } else if (!strcmp(a, "-l") || !strcmp(a, "--list")) {
            list = 1;
        } else if (!strcmp(a, "--tap")) {
            cfg.tap = 1;
        } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
            quiet = 1;
        } else if (!strcmp(a, "--color")) {
            color = 1;
        } else if (!strcmp(a, "--no-color")) {
            color = 0;
        } else if (!strcmp(a, "--fork")) {
            forking = 1;
        } else if (!strcmp(a, "--no-fork")) {
            forking = 0;
        } else if ((m = pf__valopt(a, "--seed", argc, argv, &i, &val)) != 0) {
            char *end;
            long l;
            if (m < 0 || !*val) {
                err = "missing value for";
                erra = "--seed";
                goto fail;
            }
            l = strtol(val, &end, 0);
            if (*end) {
                err = "invalid seed";
                erra = val;
                goto fail;
            }
            cfg.seed = (int)l;
        } else if (
            (m = pf__valopt(a, "--output", argc, argv, &i, &val)) != 0
            || (m = pf__valopt(a, "-o", argc, argv, &i, &val)) != 0
        ) {
            if (m < 0 || !*val) {
                err = "missing value for";
                erra = "--output";
                goto fail;
            }
            outpath = val;
        } else {
            err = "unknown option";
            erra = a;
            goto fail;
        }
    }

    for (i = 0; i < cfg.nspecs; i++) {
        if (!pf__spec_known(cfg.specs[i], suites)) {
            err = "unknown suite or test";
            erra = cfg.specs[i];
            goto fail;
        }
    }

    if (cfg.seed == 0)
        cfg.seed = (int)time(NULL);

#ifndef PF_TEST_NO_FORK
    cfg.fork = forking < 0 ? 1 : forking;
#else
    if (forking == 1) {
        err = "forking is not supported in this build:";
        erra = "--fork";
        goto fail;
    }
    cfg.fork = 0;
#endif

    if (outpath) {
        file = fopen(outpath, "w");
        if (!file) {
            err = "cannot open output file";
            erra = outpath;
            goto fail;
        }
    }
    cfg.out = quiet && !list ? NULL : (file ? file : stdout);

#ifndef PF_TEST_NO_COLOR
    if (color < 0) {
        color = 0;
    #ifdef PF__TEST_POSIX
        color = !file && !getenv("NO_COLOR") && isatty(STDOUT_FILENO);
    #endif
    }
    cfg.color = color && !cfg.tap;
#else
    (void)color;
    cfg.color = 0;
#endif

    if (list) {
        pf__list(&cfg, suites, cfg.out ? cfg.out : stdout);
        rc = 0;
    } else {
        rc = pf__run(&cfg, suites) ? 1 : 0;
    }
    goto done;

fail:
    fprintf(stderr, "%s: %s '%s'\n", prog, err, erra);
    pf__usage(prog, suites, stderr);
    rc = 2;

done:
    if (file)
        fclose(file);
    free((void *)cfg.specs);
    return rc;
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_TEST */