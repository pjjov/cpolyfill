/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/process.h>, on the program built from child.c. */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/process.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/time.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#if PF_OS_POSIX
    #include <unistd.h>
#endif

#include "check.h"

static const char *child_path(void) { return getenv("PF_TEST_CHILD"); }

#define NEED_CHILD()                                                          \
    do {                                                                      \
        if (!child_path())                                                    \
            return SKIP("PF_TEST_CHILD is not set (run through meson test)"); \
    } while (0)

/* Runs the child with `args` (after its path), `input` on stdin; its
   stdout and stderr are collected into `out` / `err` (may be NULL). */
static int run_child(
    const char *const *args,
    const pf_spawn_opts_t *opts,
    const char *input,
    pf_stream_t *out,
    pf_stream_t *err,
    pf_exit_status_t *st
) {
    const char *argv[16];
    pf_stream_t in;
    int k, rc;

    argv[0] = child_path();
    for (k = 0; args[k]; k++)
        argv[k + 1] = args[k];
    argv[k + 1] = NULL;
    if (input)
        PF_ASSERT_ALWAYS_EQ(
            pf_stream_memory(&in, (void *)input, strlen(input), false, NULL),
            PF_OK
        );
    rc = pf_process_run(argv, opts, input ? &in : NULL, out, err, st);
    if (input)
        pf_stream_close(&in);
    return rc;
}

/* The content of a buffer stream, NUL-terminated. */
static const char *text(pf_stream_t *s, size_t *size) {
    size_t n;
    const char *d = (const char *)pf_stream_buffer_data(s, &n);
    if (size)
        *size = n;
    return d;
}

static int test_ids(int seed, int i) {
    (void)seed, (void)i;
#if PF_OS_POSIX
    PF_ASSERT_ALWAYS_EQ(pf_getpid(), getpid());
    PF_ASSERT_ALWAYS_EQ(pf_getppid(), getppid());
#else
    PF_ASSERT_ALWAYS_GT(pf_getpid(), 0);
#endif
    return 0;
}

static int test_exe_path(int seed, int i) {
    char path[PF_PATH_MAX];
    ptrdiff_t len;
    size_t blen;
    (void)seed, (void)i;

    len = pf_exe_path(path, sizeof(path));
#if PF_OS_OPENBSD
    PF_ASSERT_ALWAYS_EQ(len, PF_ENOSYS);
    return 0;
#endif
    PF_ASSERT_ALWAYS_GT(len, 0);
    PF_ASSERT_ALWAYS_EQ((size_t)len, strlen(path));
    PF_ASSERT_ALWAYS(pf_path_is_absolute(path));
    PF_ASSERT_ALWAYS(pf_is_file(path));
    PF_ASSERT_ALWAYS(
        strstr(pf_path_basename(path, &blen), "cpolyfill-test") != NULL
    );
    PF_ASSERT_ALWAYS_EQ(pf_exe_path(NULL, 0), len);
    PF_ASSERT_ALWAYS_EQ(pf_exe_path(path, 3), PF_ERANGE);
    return 0;
}

static int test_run(int seed, int i) {
    static const char *const echo[] = { "echo", NULL };
    static const char *const exit42[] = { "exit", "42", NULL };
    static const char *const err[] = { "err", "oops", NULL };
    static const char *const both[] = { "both", "ab", NULL };
    pf_stream_t out, errs;
    pf_exit_status_t st;
    (void)seed, (void)i;
    NEED_CHILD();

    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(
        run_child(echo, NULL, "hello\nworld", &out, NULL, &st), PF_OK
    );
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "hello\nworld");
    pf_stream_close(&out);

    memset(&st, 0xFF, sizeof(st));
    PF_ASSERT_ALWAYS_EQ(run_child(exit42, NULL, NULL, NULL, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(st.code, 42);
    PF_ASSERT_ALWAYS_EQ(st.signal, 0);
    /* no status wanted */
    PF_ASSERT_ALWAYS_EQ(run_child(exit42, NULL, NULL, NULL, NULL, NULL), PF_OK);

    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&errs, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(err, NULL, NULL, &out, &errs, &st), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "");
    PF_ASSERT_ALWAYS_STR_EQ(text(&errs, NULL), "oops");
    pf_stream_close(&out);
    pf_stream_close(&errs);

    {
        pf_spawn_opts_t o;
        memset(&o, 0, sizeof(o));
        o.flags = PF_SPAWN_STDERR_TO_STDOUT;
        PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
        PF_ASSERT_ALWAYS_EQ(run_child(both, &o, NULL, &out, NULL, &st), PF_OK);
        PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "abab");
        pf_stream_close(&out);
    }
    return 0;
}

/* Much more than a pipe holds, on both pipes and the input at once. */
static int test_big(int seed, int i) {
    static const char *const big[] = { "big", "3000000", NULL };
    static const char *const echo[] = { "echo", NULL };
    pf_stream_t out, errs;
    pf_exit_status_t st;
    char *input;
    size_t n, k;
    (void)seed, (void)i;
    NEED_CHILD();

    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&errs, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(big, NULL, NULL, &out, &errs, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    text(&out, &n);
    PF_ASSERT_ALWAYS_EQ(n, 3000000);
    text(&errs, &n);
    PF_ASSERT_ALWAYS_EQ(n, 3000000);
    pf_stream_close(&out);
    pf_stream_close(&errs);

    n = 4u << 20;
    input = (char *)malloc(n + 1);
    PF_ASSERT_ALWAYS(input != NULL);
    for (k = 0; k < n; k++)
        input[k] = (char)('a' + (k * 7 + k / 13) % 26);
    input[n] = '\0';
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(echo, NULL, input, &out, NULL, &st), PF_OK);
    {
        size_t got;
        const char *d = text(&out, &got);
        PF_ASSERT_ALWAYS_EQ(got, n);
        PF_ASSERT_ALWAYS(memcmp(d, input, n) == 0);
    }
    pf_stream_close(&out);
    free(input);
    return 0;
}

static int test_options(int seed, int i) {
    static const char *const env_x[] = { "env", "PF_TEST_X", NULL };
    static const char *const cwd[] = { "cwd", NULL };
    static const char *const out_hi[] = { "out", "hi there", NULL };
    static const char *const echo[] = { "echo", NULL };
    static const char *const pgid[] = { "pgid", NULL };
    const char *env[] = { "PF_TEST_X=value 1", "PF_TEST_Y=2", NULL };
    pf_spawn_opts_t o;
    pf_stream_t out;
    pf_exit_status_t st;
    char tmp[PF_PATH_MAX], real[PF_PATH_MAX];
    (void)seed, (void)i;
    NEED_CHILD();

    /* the environment */
    memset(&o, 0, sizeof(o));
    o.env = env;
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(env_x, &o, NULL, &out, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "value 1");
    pf_stream_close(&out);
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(env_x, NULL, NULL, &out, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "<unset>");
    pf_stream_close(&out);

    /* the directory (the program path is ours, not relative to it) */
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_TEMP, tmp, sizeof(tmp)) > 0);
    PF_ASSERT_ALWAYS(pf_realpath(tmp, real) != NULL);
    memset(&o, 0, sizeof(o));
    o.cwd = real;
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(cwd, &o, NULL, &out, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), real);
    pf_stream_close(&out);

    /* stdout into a file, stdin from the null device */
    {
        char path[PF_PATH_MAX];
        void *data;
        size_t size;
        int fd;
        PF_ASSERT_ALWAYS(
            pf_path_join(path, sizeof(path), tmp, "pf-proc-XXXXXX") > 0
        );
        fd = pf_mkstemp(path);
        PF_ASSERT_ALWAYS_GE(fd, 0);
        memset(&o, 0, sizeof(o));
        o.stdio[1].mode = PF_STDIO_FD;
        o.stdio[1].fd = fd;
        PF_ASSERT_ALWAYS_EQ(
            run_child(out_hi, &o, NULL, NULL, NULL, &st), PF_OK
        );
        PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
        PF_ASSERT_ALWAYS_EQ(pf_read_file(path, &data, &size, NULL), PF_OK);
        PF_ASSERT_ALWAYS_STR_EQ((const char *)data, "hi there");
        pf_read_file_free(data, size, NULL);
        PF_ASSERT_ALWAYS_EQ(pf_unlink(path), 0);

        memset(&o, 0, sizeof(o));
        o.stdio[0].mode = PF_STDIO_NULL;
        PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
        PF_ASSERT_ALWAYS_EQ(run_child(echo, &o, NULL, &out, NULL, &st), PF_OK);
        PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "");
        pf_stream_close(&out);
    }

#if PF_OS_POSIX
    memset(&o, 0, sizeof(o));
    o.flags = PF_SPAWN_NEW_GROUP;
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(pgid, &o, NULL, &out, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(text(&out, NULL), "1");
    pf_stream_close(&out);
#else
    (void)pgid;
#endif

    /* invalid options */
    memset(&o, 0, sizeof(o));
    o.flags = 0x1000;
    PF_ASSERT_ALWAYS_EQ(run_child(echo, &o, NULL, NULL, NULL, &st), PF_EINVAL);
    memset(&o, 0, sizeof(o));
    o.stdio[2].mode = PF_STDIO_FD;
    o.stdio[2].fd = -1;
    PF_ASSERT_ALWAYS_EQ(run_child(echo, &o, NULL, NULL, NULL, &st), PF_EINVAL);
    return 0;
}

/* Arguments arrive unchanged, whatever quoting Windows needs. */
static int test_args(int seed, int i) {
    static const char *const args[] = {
        "args",          "plain",       "two words",       "",
        "quote\"inside", "back\\slash", "trail\\",         "both\\\"x",
        "tab\there",     "ünïcödé €",   "\\\\server\\x y", NULL,
    };
    pf_stream_t out;
    pf_exit_status_t st;
    const char *d;
    size_t n, k, pos = 0;
    (void)seed, (void)i;
    NEED_CHILD();

    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&out, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(run_child(args, NULL, NULL, &out, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    d = text(&out, &n);
    for (k = 1; args[k]; k++) {
        PF_ASSERT_ALWAYS_LT(pos, n);
        PF_ASSERT_ALWAYS_STR_EQ(d + pos, args[k]);
        pos += strlen(args[k]) + 1;
    }
    PF_ASSERT_ALWAYS_EQ(pos, n);
    pf_stream_close(&out);
    return 0;
}

static int test_errors(int seed, int i) {
    pf_process_t p;
    const char *missing[] = { "/nonexistent-dir/pf-no-such-program", NULL };
    const char *search[] = { "pf-no-such-program-4f8a", NULL };
    const char *empty[] = { NULL };
    pf_spawn_opts_t o;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, missing, NULL), PF_ENOENT);
    memset(&o, 0, sizeof(o));
    o.flags = PF_SPAWN_SEARCH_PATH;
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, search, &o), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, empty, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, NULL, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_spawn(NULL, missing, NULL), PF_EINVAL);
#if PF_OS_POSIX
    {
        /* a directory cannot run */
        const char *dir[] = { "/", NULL };
        PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, dir, NULL), PF_EACCES);
    }
    if (child_path()) {
        /* a cwd that does not exist: the error comes from the child */
        const char *argv[] = { child_path(), "exit", "0", NULL };
        memset(&o, 0, sizeof(o));
        o.cwd = "/nonexistent-dir/pf";
        PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, &o), PF_ENOENT);
    }
#endif
    return 0;
}

static int test_search_path(int seed, int i) {
    char dir[PF_PATH_MAX], saved[8192];
    const char *name;
    ptrdiff_t had;
    size_t len;
    pf_spawn_opts_t o;
    pf_exit_status_t st;
    const char *argv[] = { NULL, "exit", "7", NULL };
    (void)seed, (void)i;
    NEED_CHILD();

    name = pf_path_basename(child_path(), &len);
    PF_ASSERT_ALWAYS(pf_path_dirname(child_path(), &len) != NULL);
    PF_ASSERT_ALWAYS_LT(len, sizeof(dir));
    memcpy(dir, child_path(), len);
    dir[len] = '\0';
    had = pf_getenv("PATH", saved, sizeof(saved));
    PF_ASSERT_ALWAYS_EQ(pf_setenv("PATH", dir, 1), 0);

    memset(&o, 0, sizeof(o));
    o.flags = PF_SPAWN_SEARCH_PATH;
#if PF_OS_WINDOWS
    {
        /* found with ".exe" added */
        static char base[256];
        size_t n = strlen(name);
        PF_ASSERT_ALWAYS(n > 4 && n < sizeof(base));
        memcpy(base, name, n - 4);
        base[n - 4] = '\0';
        argv[0] = base;
    }
#else
    argv[0] = name;
#endif
    PF_ASSERT_ALWAYS_EQ(pf_process_run(argv, &o, NULL, NULL, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(st.code, 7);

    if (had >= 0)
        PF_ASSERT_ALWAYS_EQ(pf_setenv("PATH", saved, 1), 0);
    else
        PF_ASSERT_ALWAYS_EQ(pf_unsetenv("PATH"), 0);
    return 0;
}

static int test_wait_kill(int seed, int i) {
    pf_process_t p;
    pf_exit_status_t st, again;
    struct timespec deadline;
    const char *argv[] = { NULL, "sleep", "20000", NULL };
    uint64_t start;
    (void)seed, (void)i;
    NEED_CHILD();

    argv[0] = child_path();
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, NULL), PF_OK);
    PF_ASSERT_ALWAYS_GT(p.pid, 0);
    PF_ASSERT_ALWAYS(p.in.vt == NULL && p.out.vt == NULL && p.err.vt == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_process_try_wait(&p, &st), PF_EAGAIN);

    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 100000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, &deadline, &st), PF_ETIMEDOUT);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 90000000u);

    PF_ASSERT_ALWAYS_EQ(pf_process_kill(&p, false), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 10000000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, &deadline, &st), PF_OK);
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(st.code, 1);
#else
    PF_ASSERT_ALWAYS(!st.exited);
    PF_ASSERT_ALWAYS_EQ(st.signal, SIGTERM);
#endif
    /* the same status again; killing an ended process is fine */
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, NULL, &again), PF_OK);
    PF_ASSERT_ALWAYS_EQ(again.exited, st.exited);
    PF_ASSERT_ALWAYS_EQ(again.signal, st.signal);
    PF_ASSERT_ALWAYS_EQ(pf_process_try_wait(&p, &again), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_kill(&p, true), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);

#if PF_OS_POSIX
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_kill(&p, true), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(st.signal, SIGKILL);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);

    /* without a pidfd (as on systems that have none): polling */
    argv[2] = "150";
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, NULL), PF_OK);
    if (p.pf__handle >= 0) {
        close((int)p.pf__handle);
        p.pf__handle = -1;
    }
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 20000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, &deadline, &st), PF_ETIMEDOUT);
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 10000000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, &deadline, &st), PF_OK);
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);
#endif

    /* a short one ends before the deadline */
    argv[2] = "10";
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 10000000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, &deadline, &st), PF_OK);
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);
    return 0;
}

/* Using the pipe streams directly, and a process that stops reading. */
static int test_streams(int seed, int i) {
    static char big[1 << 20];
    pf_process_t p;
    pf_spawn_opts_t o;
    pf_exit_status_t st;
    pf_stream_t in;
    const char *echo[] = { NULL, "echo", NULL };
    const char *closer[] = { NULL, "close_stdin", NULL };
    char buf[64];
    pf_ssize_t n;
    size_t got = 0;
    (void)seed, (void)i;
    NEED_CHILD();

    echo[0] = closer[0] = child_path();
    memset(&o, 0, sizeof(o));
    o.stdio[0].mode = PF_STDIO_PIPE;
    o.stdio[1].mode = PF_STDIO_PIPE;
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, echo, &o), PF_OK);
    PF_ASSERT_ALWAYS(p.in.vt != NULL && p.out.vt != NULL && p.err.vt == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_stream_write_all(&p.in, "ping", 4), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&p.in), PF_OK);
    while ((n = pf_stream_read(&p.out, buf + got, sizeof(buf) - 1 - got)) > 0)
        got += (size_t)n;
    PF_ASSERT_ALWAYS_EQ(n, 0);
    buf[got] = '\0';
    PF_ASSERT_ALWAYS_STR_EQ(buf, "ping");
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, NULL, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);

    /* the input is dropped, without an error or a SIGPIPE */
    memset(big, 'z', sizeof(big));
    PF_ASSERT_ALWAYS_EQ(
        pf_stream_memory(&in, big, sizeof(big), false, NULL), PF_OK
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_process_run(closer, NULL, &in, NULL, NULL, &st), PF_OK
    );
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    pf_stream_close(&in);
    return 0;
}

static int test_child_ids(int seed, int i) {
    pf_process_t p;
    pf_spawn_opts_t o;
    const char *argv[] = { NULL, "ids", NULL };
    char buf[64], expect[64];
    pf_ssize_t n;
    size_t got = 0;
    (void)seed, (void)i;
    NEED_CHILD();

    argv[0] = child_path();
    memset(&o, 0, sizeof(o));
    o.stdio[1].mode = PF_STDIO_PIPE;
    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, &o), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_communicate(&p, NULL, NULL, NULL), PF_OK);
    PF_ASSERT_ALWAYS(p.out.vt == NULL); /* read to the end and closed */
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, NULL, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);

    PF_ASSERT_ALWAYS_EQ(pf_spawn(&p, argv, &o), PF_OK);
    snprintf(
        expect,
        sizeof(expect),
        "%lu %lu",
        (unsigned long)p.pid,
        (unsigned long)pf_getpid()
    );
    while ((n = pf_stream_read(&p.out, buf + got, sizeof(buf) - 1 - got)) > 0)
        got += (size_t)n;
    buf[got] = '\0';
    PF_ASSERT_ALWAYS_STR_EQ(buf, expect);
    PF_ASSERT_ALWAYS_EQ(pf_process_wait(&p, NULL, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_process_free(&p), PF_OK);
    return 0;
}

static int test_popen(int seed, int i) {
    char buf[64];
    FILE *f;
    (void)seed, (void)i;

    f = pf_popen("echo popen-ok", "r");
    PF_ASSERT_ALWAYS(f != NULL);
    PF_ASSERT_ALWAYS(fgets(buf, sizeof(buf), f) != NULL);
    PF_ASSERT_ALWAYS(strncmp(buf, "popen-ok", 8) == 0);
    PF_ASSERT_ALWAYS_EQ(pf_pclose(f), 0);
    return 0;
}

pf_test_t suite_process[] = {
    { test_ids, "ids" },
    { test_exe_path, "exe_path" },
    { test_run, "run" },
    { test_big, "big" },
    { test_options, "options" },
    { test_args, "args" },
    { test_errors, "errors" },
    { test_search_path, "search_path" },
    { test_wait_kill, "wait_kill" },
    { test_streams, "streams" },
    { test_child_ids, "child_ids" },
    { test_popen, "popen" },
    { 0 },
};
