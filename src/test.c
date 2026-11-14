/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/test.h>: the test runner. Isolated repetitions run in a
    child made with fork (POSIX) or in a copy of the program started with
    `--pf-test-child suite test i seed report` (spawn); either way the child
    writes a skip reason to the report file, and its exit code says how the
    test ended (PF__CODE_*).
*/

#include "internal.h"

#include <cpolyfill/assert.h>
#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/process.h>
#include <cpolyfill/term.h>
#include <cpolyfill/test.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#if PF_OS_POSIX && !PF_OS_EMSCRIPTEN
    #define PF__TEST_FORK 1
#else
    #define PF__TEST_FORK 0
#endif

/* Exit codes of an isolated child. */
#define PF__CODE_PASS 0
#define PF__CODE_FAIL 1
#define PF__CODE_SKIP PF_TEST_SKIP
#define PF__CODE_ASSERT 99

#define PF__CHILD_OPTION "--pf-test-child"

enum { PF__ISO_NONE, PF__ISO_FORK, PF__ISO_SPAWN };
enum { PF__OK, PF__FAIL, PF__SKIP, PF__TODO };

typedef struct pf__tcfg {
    FILE *out; /* NULL in quiet mode */
    bool tap;
    bool color;
    int isolate;
    int seed;
    int repeat;         /* 0: the suites' counts */
    int64_t timeout_ns; /* 0: none */
    const char **specs;
    int nspecs;
    char *self;   /* the program, for spawn */
    char *report; /* the report file of isolated children */
} pf__tcfg;

/* ---- Skipping ---------------------------------------------------------- */

static char pf__skip_reason[256];

PF_API int pf_test_skip(const char *fmt, ...) {
    va_list ap;
    pf__skip_reason[0] = '\0';
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(pf__skip_reason, sizeof(pf__skip_reason), fmt, ap);
        va_end(ap);
    }
    return PF_TEST_SKIP;
}

/* ---- Assertions -------------------------------------------------------- */

static pf_assert_handler_fn pf__prev_handler;
static jmp_buf pf__jump;
static volatile bool pf__jump_armed;
static pf_thrd_t pf__runner;

/* --no-isolate: back to pf__run_here, if the assertion failed in the
   thread running the test. Elsewhere the default abort follows. */
static void pf__assert_jump(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    pf__prev_handler(expr, file, line, func, message);
    if (pf__jump_armed && pf_thrd_equal(pf_thrd_current(), pf__runner)) {
        pf__jump_armed = false;
        longjmp(pf__jump, 1);
    }
}

/* In an isolated child: from any thread, the test failed. */
static void pf__assert_exit(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    pf__prev_handler(expr, file, line, func, message);
    fflush(NULL);
    _Exit(PF__CODE_ASSERT);
}

/* ---- Running one repetition -------------------------------------------- */

/* Runs a repetition in this process: PF__OK, PF__FAIL or PF__SKIP, with
   the reason of a failure or skip in `msg`. */
static int pf__run_here(
    const pf_test_t *t, int seed, int i, char *msg, size_t size
) {
    int r;

    pf__skip_reason[0] = '\0';
    pf__runner = pf_thrd_current();
    pf__prev_handler = pf_set_assert_handler(pf__assert_jump);
    if (setjmp(pf__jump)) {
        pf_set_assert_handler(pf__prev_handler);
        snprintf(msg, size, "assertion failed");
        return PF__FAIL;
    }
    pf__jump_armed = true;
    r = t->fn(seed, i);
    pf__jump_armed = false;
    pf_set_assert_handler(pf__prev_handler);

    if (r == PF_TEST_PASS)
        return PF__OK;
    if (r == PF_TEST_SKIP) {
        snprintf(msg, size, "%s", pf__skip_reason);
        return PF__SKIP;
    }
    snprintf(msg, size, "failed");
    return PF__FAIL;
}

/* The body of an isolated child: runs the test, writes the skip reason,
   returns the exit code. */
static int pf__child_run(
    const pf_test_t *t, int seed, int i, const char *report
) {
    int r;

#if PF_OS_WINDOWS
    /* No error dialogs: a crash or abort just ends the child. */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    pf__skip_reason[0] = '\0';
    pf__prev_handler = pf_set_assert_handler(pf__assert_exit);
    r = t->fn(seed, i);
    fflush(NULL);
    if (r == PF_TEST_PASS)
        return PF__CODE_PASS;
    if (r != PF_TEST_SKIP)
        return PF__CODE_FAIL;
    if (report)
        (void)pf_write_file(
            report, pf__skip_reason, strlen(pf__skip_reason), 0
        );
    return PF__CODE_SKIP;
}

static const char *pf__signal_name(int sig) {
    switch (sig) {
    case SIGTERM:
        return "terminated";
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

/* Waits for an isolated child (killing it at the timeout) and tells how
   the test ended. */
static int pf__finish(
    const pf__tcfg *cfg, pf_process_t *p, char *msg, size_t size
) {
    pf_exit_status_t st;
    bool timed_out = false;
    int rc;

    if (cfg->timeout_ns > 0) {
        struct timespec deadline;
        rc = pf_deadline_ns(&deadline, cfg->timeout_ns);
        if (rc == PF_OK)
            rc = pf_process_wait(p, &deadline, &st);
        if (rc == PF_ETIMEDOUT) {
            timed_out = true;
            (void)pf_process_kill(p, true);
            rc = pf_process_wait(p, NULL, &st);
        }
    } else {
        rc = pf_process_wait(p, NULL, &st);
    }
    pf_process_free(p);

    if (rc != PF_OK) {
        snprintf(msg, size, "could not wait for the test: %s", pf_strerror(rc));
        return PF__FAIL;
    }
    if (timed_out) {
        snprintf(
            msg, size, "timed out after %gs", (double)cfg->timeout_ns / 1e9
        );
        return PF__FAIL;
    }
    if (!st.exited) {
        snprintf(
            msg, size, "signal %d: %s", st.signal, pf__signal_name(st.signal)
        );
        return PF__FAIL;
    }
    switch (st.code) {
    case PF__CODE_PASS:
        return PF__OK;
    case PF__CODE_SKIP: {
        void *data = NULL;
        size_t n = 0;
        msg[0] = '\0';
        if (pf_read_file(cfg->report, &data, &n, NULL) == PF_OK) {
            snprintf(msg, size, "%s", (const char *)data);
            pf_read_file_free(data, n, NULL);
        }
        return PF__SKIP;
    }
    case PF__CODE_FAIL:
        snprintf(msg, size, "failed");
        return PF__FAIL;
    case PF__CODE_ASSERT:
        snprintf(msg, size, "assertion failed");
        return PF__FAIL;
    default:
        /* Windows reports crashes as NTSTATUS codes (0xC0000005). */
        if (st.code < 0 || st.code > 255)
            snprintf(msg, size, "exited with code %#x", (unsigned)st.code);
        else
            snprintf(msg, size, "exited with code %d", st.code);
        return PF__FAIL;
    }
}

#if PF__TEST_FORK
static int pf__run_fork(
    const pf__tcfg *cfg, const pf_test_t *t, int i, char *msg, size_t size
) {
    pf_process_t p;
    pid_t pid;

    /* Or the child would write out what is buffered again. */
    fflush(NULL);
    pid = fork();
    if (pid < 0) {
        snprintf(
            msg,
            size,
            "could not start the test process: %s",
            pf_strerror(pf_error_from_errno(errno))
        );
        return PF__FAIL;
    }
    if (pid == 0)
        _exit(pf__child_run(t, cfg->seed, i, cfg->report));
    (void)pf__process_adopt(&p, (long)pid);
    return pf__finish(cfg, &p, msg, size);
}
#endif

static int pf__run_spawn(
    const pf__tcfg *cfg,
    const pf_suite_t *su,
    const pf_test_t *t,
    int i,
    char *msg,
    size_t size
) {
    char istr[16], seedstr[16];
    const char *argv[8];
    pf_process_t p;
    int rc;

    snprintf(istr, sizeof(istr), "%d", i);
    snprintf(seedstr, sizeof(seedstr), "%d", cfg->seed);
    argv[0] = cfg->self;
    argv[1] = PF__CHILD_OPTION;
    argv[2] = su->name;
    argv[3] = t->name;
    argv[4] = istr;
    argv[5] = seedstr;
    argv[6] = cfg->report;
    argv[7] = NULL;

    fflush(NULL);
    rc = pf_spawn(&p, argv, NULL);
    if (rc != PF_OK) {
        snprintf(
            msg, size, "could not start the test process: %s", pf_strerror(rc)
        );
        return PF__FAIL;
    }
    return pf__finish(cfg, &p, msg, size);
}

static int pf__run_once(
    const pf__tcfg *cfg,
    const pf_suite_t *su,
    const pf_test_t *t,
    int i,
    char *msg,
    size_t size
) {
    switch (cfg->isolate) {
#if PF__TEST_FORK
    case PF__ISO_FORK:
        return pf__run_fork(cfg, t, i, msg, size);
#endif
    case PF__ISO_SPAWN:
        return pf__run_spawn(cfg, su, t, i, msg, size);
    default:
        return pf__run_here(t, cfg->seed, i, msg, size);
    }
}

/* ---- Selection --------------------------------------------------------- */

/* Matches `s` against the first `n` characters of the pattern `p`, with
   `*` (any run) and `?` (any character). */
static bool pf__glob(const char *p, size_t n, const char *s) {
    size_t pi = 0, star = (size_t)-1;
    const char *mark = NULL;

    while (*s) {
        if (pi < n && (p[pi] == '?' || p[pi] == *s)) {
            pi++;
            s++;
        } else if (pi < n && p[pi] == '*') {
            star = pi++;
            mark = s;
        } else if (star != (size_t)-1) {
            pi = star + 1;
            s = ++mark;
        } else {
            return false;
        }
    }
    while (pi < n && p[pi] == '*')
        pi++;
    return pi == n;
}

/* `suite` alone matches every test of the suite (`test` may be NULL),
   `suite:test` one test. */
static bool pf__match(const char *spec, const char *suite, const char *test) {
    const char *colon = strchr(spec, ':');
    size_t n = colon ? (size_t)(colon - spec) : strlen(spec);

    if (strcmp(spec, "all") == 0)
        return true;
    if (!pf__glob(spec, n, suite))
        return false;
    if (!colon)
        return true;
    return test && pf__glob(colon + 1, strlen(colon + 1), test);
}

static bool pf__selected(
    const pf__tcfg *cfg, const pf_suite_t *su, const pf_test_t *t
) {
    int k;
    if (cfg->nspecs == 0)
        return true;
    for (k = 0; k < cfg->nspecs; k++)
        if (pf__match(cfg->specs[k], su->name, t->name))
            return true;
    return false;
}

static bool pf__spec_known(const char *spec, const pf_suite_t *suites) {
    int s, t;
    for (s = 0; suites[s].name; s++) {
        if (!suites[s].tests || !suites[s].tests[0].name) {
            if (!strchr(spec, ':') && pf__match(spec, suites[s].name, NULL))
                return true;
            continue;
        }
        for (t = 0; suites[s].tests[t].name; t++)
            if (pf__match(spec, suites[s].name, suites[s].tests[t].name))
                return true;
    }
    return false;
}

/* ---- Report ------------------------------------------------------------ */

PF_PRINTF(2, 3) static void pf__say(const pf__tcfg *cfg, const char *fmt, ...) {
    va_list ap;
    if (!cfg->out)
        return;
    va_start(ap, fmt);
    vfprintf(cfg->out, fmt, ap);
    va_end(ap);
    /* Before a child writes its own output. */
    fflush(cfg->out);
}

static const char *pf__label(const pf__tcfg *cfg, int kind) {
    static const char *const plain[] = { " OK ", "FAIL", "SKIP", "TODO" };
    static const char *const color[] = {
        "\x1b[32m OK \x1b[0m",
        "\x1b[31mFAIL\x1b[0m",
        "\x1b[36mSKIP\x1b[0m",
        "\x1b[33mTODO\x1b[0m",
    };
    return (cfg->color ? color : plain)[kind];
}

static int pf__count(const pf__tcfg *cfg, const pf_suite_t *su) {
    if (su->count <= 0)
        return 0;
    return cfg->repeat > 0 ? cfg->repeat : su->count;
}

static int pf__run(const pf__tcfg *cfg, const pf_suite_t *suites) {
    int s, t, n = 0, pass = 0, fail = 0, skip = 0, todo = 0;

    if (cfg->tap) {
        int total = 0;
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
        int count = pf__count(cfg, su);
        bool header = false;

        for (t = 0; su->tests && su->tests[t].name; t++) {
            const pf_test_t *te = &su->tests[t];
            int kind = PF__OK, i, at = 0;
            uint64_t start, elapsed;
            char msg[320];

            if (!pf__selected(cfg, su, te))
                continue;
            n++;
            if (!cfg->tap && !header) {
                pf__say(cfg, "%s (x%d)\n", su->name, count);
                header = true;
            }

            if (!te->fn || count <= 0) {
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

            msg[0] = '\0';
            start = pf_time_ns();
            for (i = 0; i < count && kind == PF__OK; i++) {
                kind = pf__run_once(cfg, su, te, i, msg, sizeof(msg));
                at = i;
            }
            elapsed = pf_time_ns() - start;

            if (kind == PF__OK)
                pass++;
            else if (kind == PF__SKIP)
                skip++;
            else
                fail++;

            if (cfg->tap) {
                if (kind == PF__SKIP)
                    pf__say(
                        cfg,
                        "ok %d - %s/%s # SKIP%s%s\n",
                        n,
                        su->name,
                        te->name,
                        msg[0] ? " " : "",
                        msg
                    );
                else
                    pf__say(
                        cfg,
                        "%s %d - %s/%s\n",
                        kind == PF__OK ? "ok" : "not ok",
                        n,
                        su->name,
                        te->name
                    );
                if (kind == PF__FAIL)
                    pf__say(cfg, "# %s at iteration %d\n", msg, at);
            } else {
                pf__say(
                    cfg,
                    "  %-24s [ %s ] [ %.6fs ]",
                    te->name,
                    pf__label(cfg, kind),
                    (double)elapsed / 1e9
                );
                if (kind == PF__FAIL)
                    pf__say(cfg, " %s at iteration %d", msg, at);
                else if (kind == PF__SKIP && msg[0])
                    pf__say(cfg, " %s", msg);
                pf__say(cfg, "\n");
            }
        }
    }

    if (!cfg->tap) {
        int ran = pass + fail;
        pf__say(
            cfg,
            "%d of %d (%d%%) tests passed, %d skipped, %d to do. (seed: %#x)\n",
            pass,
            ran,
            ran ? pass * 100 / ran : 100,
            skip,
            todo,
            (unsigned)cfg->seed
        );
    }
    return fail;
}

static void pf__list(const pf_suite_t *suites, FILE *to) {
    int s, t;
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

static void pf__usage(const char *prog, const pf_suite_t *suites, FILE *to) {
    fprintf(
        to,
        "Usage: %s [options] [suite[:test]...]\n"
        "\n"
        "Selections may use the wildcards * and ?.\n"
        "\n"
        "Options:\n"
        "  -h, --help              show this help\n"
        "  -l, --list              list suites and tests\n"
        "      --tap               use the Test Anything Protocol\n"
        "  -q, --quiet             silent, report only by exit code\n"
        "      --color             force colored output\n"
        "      --no-color          disable colored output\n"
        "      --isolate[=fork|spawn]\n"
        "                          run tests in child processes (default)\n"
        "      --no-isolate        run tests in this process\n"
        "      --fork, --no-fork   the same as --isolate, --no-isolate\n"
        "      --timeout SECONDS   end isolated tests that run longer\n"
        "  -r, --repeat N          run each test N times\n"
        "      --seed N            seed passed to tests (default: time)\n"
        "  -o, --output FILE       write the report to FILE\n"
        "\n"
        "Suites and tests:\n",
        prog
    );
    pf__list(suites, to);
}

/* ---- Command line ------------------------------------------------------ */

/* Matches `--name value` and `--name=value` (`-n value` for short names):
   1 on a match, 0 for another option, -1 when the value is missing. */
static int pf__valopt(
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
    if (a[n] == '=' && name[1] == '-') {
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

/* The hidden option of a spawned child: runs one repetition and returns
   its exit code. */
static int pf__child_main(int argc, char **argv, const pf_suite_t *suites) {
    int s, t;
    if (argc < 7)
        return 2;
    for (s = 0; suites[s].name; s++) {
        if (strcmp(suites[s].name, argv[2]) != 0)
            continue;
        for (t = 0; suites[s].tests && suites[s].tests[t].name; t++) {
            const pf_test_t *te = &suites[s].tests[t];
            if (strcmp(te->name, argv[3]) == 0 && te->fn)
                return pf__child_run(
                    te, (int)strtol(argv[5], NULL, 0), atoi(argv[4]), argv[6]
                );
        }
    }
    fprintf(stderr, "%s: no test %s:%s\n", argv[0], argv[2], argv[3]);
    return 2;
}

/* The program's path and the report file, for isolated tests. */
static int pf__prepare(pf__tcfg *cfg, const char **err, const char **erra) {
    static const char name[] = "/cpolyfill-test-XXXXXX";
    ptrdiff_t len;
    int fd;

    if (cfg->isolate == PF__ISO_SPAWN) {
        len = pf_exe_path(NULL, 0);
        if (len < 0 || !(cfg->self = (char *)malloc((size_t)len + 1))
            || pf_exe_path(cfg->self, (size_t)len + 1) < 0) {
            *err = "cannot find the program's path for";
            *erra = "--isolate=spawn";
            return -1;
        }
    }

    len = pf_known_dir(PF_DIR_TEMP, NULL, 0);
    if (len < 0 || !(cfg->report = (char *)malloc((size_t)len + sizeof(name)))
        || pf_known_dir(PF_DIR_TEMP, cfg->report, (size_t)len + 1) < 0) {
        *err = "cannot find the temporary directory";
        *erra = "";
        return -1;
    }
    memcpy(cfg->report + len, name, sizeof(name));
    fd = pf_mkstemp(cfg->report);
    if (fd < 0) {
        free(cfg->report);
        cfg->report = NULL;
        *err = "cannot create a file in the temporary directory";
        *erra = "";
        return -1;
    }
    pf_close(fd);
    return 0;
}

PF_API int pf_test_main(int argc, char **argv, const pf_suite_t *suites) {
    pf__tcfg cfg;
    const char *prog = (argc > 0 && argv && argv[0]) ? argv[0] : "test";
    const char *outpath = NULL, *err = NULL, *erra = "";
    int i, rc = 2, color = -1;
    bool quiet = false, list = false;
    FILE *file = NULL;

    if (!suites)
        return 0;
    if (argc > 1 && strcmp(argv[1], PF__CHILD_OPTION) == 0)
        return pf__child_main(argc, argv, suites);

    memset(&cfg, 0, sizeof(cfg));
    cfg.isolate = PF__TEST_FORK ? PF__ISO_FORK : PF__ISO_SPAWN;
    cfg.specs = (const char **)malloc(
        sizeof(*cfg.specs) * (argc > 0 ? argc : 1)
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
            list = true;
        } else if (!strcmp(a, "--tap")) {
            cfg.tap = true;
        } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
            quiet = true;
        } else if (!strcmp(a, "--color")) {
            color = 1;
        } else if (!strcmp(a, "--no-color")) {
            color = 0;
        } else if (!strcmp(a, "--isolate") || !strcmp(a, "--fork")) {
            cfg.isolate = PF__TEST_FORK ? PF__ISO_FORK : PF__ISO_SPAWN;
        } else if (!strcmp(a, "--isolate=fork")) {
            if (!PF__TEST_FORK) {
                err = "fork is not available here:";
                erra = a;
                goto fail;
            }
            cfg.isolate = PF__ISO_FORK;
        } else if (!strcmp(a, "--isolate=spawn")) {
            cfg.isolate = PF__ISO_SPAWN;
        } else if (!strcmp(a, "--no-isolate") || !strcmp(a, "--no-fork")) {
            cfg.isolate = PF__ISO_NONE;
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
            (m = pf__valopt(a, "--timeout", argc, argv, &i, &val)) != 0
        ) {
            char *end;
            double d;
            if (m < 0 || !*val) {
                err = "missing value for";
                erra = "--timeout";
                goto fail;
            }
            d = strtod(val, &end);
            if (*end || !(d >= 0) || d > 1e9) {
                err = "invalid timeout";
                erra = val;
                goto fail;
            }
            cfg.timeout_ns = (int64_t)(d * 1e9);
        } else if (
            (m = pf__valopt(a, "--repeat", argc, argv, &i, &val)) != 0
            || (m = pf__valopt(a, "-r", argc, argv, &i, &val)) != 0
        ) {
            char *end;
            long l;
            if (m < 0 || !*val) {
                err = "missing value for";
                erra = "--repeat";
                goto fail;
            }
            l = strtol(val, &end, 10);
            if (*end || l < 1 || l > 1000000000L) {
                err = "invalid repeat count";
                erra = val;
                goto fail;
            }
            cfg.repeat = (int)l;
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
    if (cfg.timeout_ns > 0 && cfg.isolate == PF__ISO_NONE) {
        err = "a timeout needs isolated tests:";
        erra = "--no-isolate";
        goto fail;
    }

    if (cfg.seed == 0)
        cfg.seed = (int)time(NULL);

    if (outpath) {
        file = pf_fopen(outpath, "w");
        if (!file) {
            err = "cannot open output file";
            erra = outpath;
            goto fail;
        }
    }
    cfg.out = quiet && !list ? NULL : (file ? file : stdout);

    if (color < 0)
        color = !file && pf_term_colors(1) > 0;
    else if (color > 0 && !file)
        (void)pf_term_enable_vt(1);
    cfg.color = color && !cfg.tap;

    if (list) {
        pf__list(suites, cfg.out ? cfg.out : stdout);
        rc = 0;
        goto done;
    }
    if (cfg.isolate != PF__ISO_NONE && pf__prepare(&cfg, &err, &erra) != 0) {
        fprintf(stderr, "%s: %s%s%s\n", prog, err, *erra ? " " : "", erra);
        rc = 2;
        goto done;
    }
    rc = pf__run(&cfg, suites) ? 1 : 0;
    goto done;

fail:
    fprintf(stderr, "%s: %s '%s'\n", prog, err, erra);
    pf__usage(prog, suites, stderr);
    rc = 2;

done:
    if (file)
        fclose(file);
    if (cfg.report) {
        (void)pf_unlink(cfg.report);
        free(cfg.report);
    }
    free(cfg.self);
    free((void *)cfg.specs);
    return rc;
}
