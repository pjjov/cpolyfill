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

#include "../include/pf_daemon.h"
#include "util.h"
#include <errno.h>
#include <limits.h>

#ifndef _WIN32
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/wait.h>
#endif

static int test_daemon_options_init(int seed, int rep) {
    pf_daemon_options_t o;

    memset(&o, 0xFF, sizeof o);
    pf_daemon_options_t_init(&o);

    CHECK(o.workingDirectory == NULL);
    CHECK(o.lockPath == NULL);
    CHECK(o.pidPath == NULL);
    CHECK(o.daemonize == 0);
    CHECK(o.closeFds == PF_DAEMON_DEFAULT_CLOSE_FDS);
    CHECK(o.redirectStdio == PF_DAEMON_DEFAULT_REDIRECT_STDIO);
    CHECK(o.changeUmask == 0);
    CHECK(o.umaskValue == 022);
    CHECK(o.preserveFds == NULL && o.preserveFdCount == 0);
    CHECK(o.returnParent == 1);
    CHECK(o.lockTimeoutMs == 0);

    pf_daemon_options_t_init(NULL); /* must not crash */
    return 0;
}

static int test_daemon_init_and_error(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_error_info_t info;

    memset(&d, 0xFF, sizeof d);
    pf_daemon_init(&d);
    CHECK(d.started == 0 && d.ownsLock == 0 && d.pidFileCreated == 0);
    CHECK(d.lockPath[0] == '\0' && d.pidPath[0] == '\0');
#ifndef _WIN32
    CHECK(d.lockFd == -1);
#else
    CHECK(d.mutex == NULL);
#endif
    CHECK(!pf_daemon_is_running(&d));

    CHECK(pf_daemon_last_error(&d, &info) == PF_DAEMON_OK);
    CHECK(info.code == PF_DAEMON_OK && info.osError == 0);

    pf_daemon_set_error(&d, PF_DAEMON_EIO, 1234);
    CHECK(pf_daemon_last_error(&d, &info) == PF_DAEMON_OK);
    CHECK(info.code == PF_DAEMON_EIO);
    CHECK(info.osError == 1234);

    CHECK(pf_daemon_last_error(NULL, &info) == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_last_error(&d, NULL) == PF_DAEMON_EINVAL);
    pf_daemon_set_error(NULL, 1, 1);
    pf_daemon_init(NULL);
    CHECK(!pf_daemon_is_running(NULL));
    return 0;
}

static int test_daemon_error_codes(int seed, int rep) {
    /* Distinct, so callers can tell them apart. */
    int codes[] = {
        PF_DAEMON_OK,        PF_DAEMON_PARENT,    PF_DAEMON_EINVAL,
        PF_DAEMON_EFORK,     PF_DAEMON_ESETSID,   PF_DAEMON_ECHDIR,
        PF_DAEMON_EIO,       PF_DAEMON_ELOCKED,   PF_DAEMON_EPERM,
        PF_DAEMON_ENOMEM,    PF_DAEMON_ETIMEDOUT, PF_DAEMON_ESIGNAL,
        PF_DAEMON_ESTDIO,    PF_DAEMON_EUMASK,    PF_DAEMON_EPIDFILE,
        PF_DAEMON_ELOCKFILE, PF_DAEMON_EUNKNOWN
    };
    size_t i, j, n = sizeof codes / sizeof codes[0];

    CHECK(PF_DAEMON_OK == 0);
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            CHECK(codes[i] != codes[j]);
    return 0;
}

static int test_daemon_start_invalid(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;

    pf_daemon_options_t_init(&o);
    CHECK(pf_daemon_start(NULL, &o) == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_start(&d, NULL) == PF_DAEMON_EINVAL);
    pf_daemon_stop(NULL);
    return 0;
}

/* Starting without lock, pid file or daemonization does nothing but
   record that it ran. */
static int test_daemon_start_minimal(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;

    pf_daemon_options_t_init(&o);
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);
    CHECK(d.started == 1);
    CHECK(!pf_daemon_is_running(&d)); /* it holds no lock */

    pf_daemon_stop(&d);
    CHECK(d.started == 0);
    pf_daemon_stop(&d); /* stopping twice is fine */
    return 0;
}

static int test_daemon_lock(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char lock[TEST_PATH_MAX];

    test_path(lock, sizeof lock, "daemon-lock");
    pf_daemon_options_t_init(&o);
    o.lockPath = lock;

    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);
    CHECK(pf_daemon_is_running(&d));
    CHECK(d.ownsLock == 1);
#ifndef _WIN32
    CHECK(d.lockFd >= 0);
    CHECK(access(lock, F_OK) == 0);
#endif

    pf_daemon_stop(&d);
    CHECK(!pf_daemon_is_running(&d));
    CHECK(d.started == 0);
#ifndef _WIN32
    CHECK(d.lockFd == -1);
    /* The lock file stays, to avoid races between instances. */
    CHECK(access(lock, F_OK) == 0);
    unlink(lock);
#endif
    return 0;
}

static int test_daemon_pid_file(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char pid[TEST_PATH_MAX], text[64], want[64];

    test_path(pid, sizeof pid, "daemon-pid");
    pf_daemon_options_t_init(&o);
    o.pidPath = pid;

    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);
    CHECK(d.pidFileCreated == 1);

    CHECK(test_read(pid, text, sizeof text) > 0);
    snprintf(want, sizeof want, "%ld\n", test_pid());
    CHECK_STR(text, want);

    pf_daemon_stop(&d);
    CHECK(d.pidFileCreated == 0);
    {
        FILE *f = fopen(pid, "rb");
        CHECK(f == NULL); /* removed */
    }
    return 0;
}

static int test_daemon_pid_file_error(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char dir[TEST_PATH_MAX], pid[TEST_PATH_MAX], lock[TEST_PATH_MAX];
    pf_daemon_error_info_t info;

    test_path(dir, sizeof dir, "daemon-missing");
    test_join(pid, sizeof pid, dir, "pid");
    test_path(lock, sizeof lock, "daemon-lock-pidfail");

    pf_daemon_options_t_init(&o);
    o.pidPath = pid;
    o.lockPath = lock;

    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_EPIDFILE);
    CHECK(pf_daemon_last_error(&d, &info) == PF_DAEMON_OK);
    CHECK(info.code == PF_DAEMON_EPIDFILE);
    CHECK(info.osError != 0);

    /* A failed start does not keep the singleton lock. */
    CHECK(!pf_daemon_is_running(&d));
    CHECK(d.started == 0);
#ifndef _WIN32
    unlink(lock);
#endif
    return 0;
}

#ifndef _WIN32

/* Runs `body` in a child process and returns its exit code. */
typedef int (*child_fn)(void *);

static int in_child(child_fn body, void *arg) {
    int status = 0;
    pid_t pid;

    fflush(NULL);
    pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0)
        _exit(body(arg));
    if (waitpid(pid, &status, 0) != pid)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -2;
}

struct start_args {
    const char *lock;
    uint32_t timeout_ms;
    int expect;
    double min_seconds;
};

static int child_start(void *p) {
    struct start_args *a = p;
    pf_daemon_t d;
    pf_daemon_options_t o;
    double start = test_now();
    int rc;

    pf_daemon_options_t_init(&o);
    o.lockPath = a->lock;
    o.lockTimeoutMs = a->timeout_ms;
    rc = pf_daemon_start(&d, &o);
    if (rc != a->expect)
        return 10 + rc;
    if (test_now() - start < a->min_seconds)
        return 5;
    pf_daemon_stop(&d);
    return 0;
}

static int test_daemon_singleton(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char lock[TEST_PATH_MAX];
    struct start_args a;

    test_path(lock, sizeof lock, "daemon-single");
    pf_daemon_options_t_init(&o);
    o.lockPath = lock;
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);

    /* Another process cannot take the lock: at once... */
    a.lock = lock;
    a.timeout_ms = 0;
    a.expect = PF_DAEMON_ELOCKED;
    a.min_seconds = 0;
    CHECK(in_child(child_start, &a) == 0);

    /* ...or after waiting for the timeout. */
    a.timeout_ms = 120;
    a.expect = PF_DAEMON_ETIMEDOUT;
    a.min_seconds = 0.08;
    CHECK(in_child(child_start, &a) == 0);

    /* Once released, it can. */
    pf_daemon_stop(&d);
    a.timeout_ms = 0;
    a.expect = PF_DAEMON_OK;
    a.min_seconds = 0;
    CHECK(in_child(child_start, &a) == 0);

    unlink(lock);
    return 0;
}

/* A lock that is released while another process waits for it. */
static int test_daemon_lock_handover(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char lock[TEST_PATH_MAX];
    struct start_args a;
    pid_t pid;
    int status = 0;

    test_path(lock, sizeof lock, "daemon-handover");
    pf_daemon_options_t_init(&o);
    o.lockPath = lock;
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);

    a.lock = lock;
    a.timeout_ms = 5000;
    a.expect = PF_DAEMON_OK;
    a.min_seconds = 0.05;

    fflush(NULL);
    pid = fork();
    CHECK(pid >= 0);
    if (pid == 0)
        _exit(child_start(&a));

    test_sleep_ms(100);
    pf_daemon_stop(&d);

    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    unlink(lock);
    return 0;
}

static int test_daemon_lock_errors(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char dir[TEST_PATH_MAX], lock[TEST_PATH_MAX];
    static char longpath[5000];

    pf_daemon_options_t_init(&o);

    /* The directory is missing. */
    test_path(dir, sizeof dir, "daemon-nodir");
    test_join(lock, sizeof lock, dir, "lock");
    o.lockPath = lock;
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_ELOCKFILE);
    CHECK(!pf_daemon_is_running(&d));

    /* The path does not fit the state. */
    memset(longpath, 'a', sizeof longpath - 1);
    longpath[0] = '/';
    o.lockPath = longpath;
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_ELOCKFILE);

    /* An empty path is no path. */
    o.lockPath = "";
    CHECK(pf_daemon_build_lockPath(&d, "") == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_build_lockPath(&d, NULL) == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_build_lockPath(NULL, "x") == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_build_lockPath(&d, "/some/path") == PF_DAEMON_OK);
    CHECK_STR(d.lockPath, "/some/path");
    return 0;
}

static int test_daemon_default_lock_path(int seed, int rep) {
    pf_daemon_t d;

    pf_daemon_init(&d);

    CHECK(test_setenv("XDG_RUNTIME_DIR", "/run/pf-test") == 0);
    CHECK(pf_daemon_default_lockPath(&d, "my.lock") == PF_DAEMON_OK);
    CHECK_STR(d.lockPath, "/run/pf-test/my.lock");

    CHECK(test_setenv("XDG_RUNTIME_DIR", NULL) == 0);
    CHECK(pf_daemon_default_lockPath(&d, "my.lock") == PF_DAEMON_OK);
    CHECK_STR(d.lockPath, "/tmp/my.lock");

    CHECK(test_setenv("XDG_RUNTIME_DIR", "") == 0);
    CHECK(pf_daemon_default_lockPath(&d, "other") == PF_DAEMON_OK);
    CHECK_STR(d.lockPath, "/tmp/other");

    CHECK(pf_daemon_default_lockPath(&d, "") == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_default_lockPath(&d, NULL) == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_default_lockPath(NULL, "x") == PF_DAEMON_EINVAL);
    return 0;
}

static int test_daemon_working_directory(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char dir[TEST_PATH_MAX], original[TEST_PATH_MAX], now[TEST_PATH_MAX];
    pf_daemon_error_info_t info;

    CHECK(getcwd(original, sizeof original) != NULL);
    test_path(dir, sizeof dir, "daemon-cwd");
    CHECK(test_mkdir(dir) == 0);

    pf_daemon_options_t_init(&o);
    o.workingDirectory = dir;
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);
    CHECK(getcwd(now, sizeof now) != NULL);
    CHECK(strcmp(now, original) != 0);
    CHECK(strstr(now, strrchr(dir, '/') + 1) != NULL);
    pf_daemon_stop(&d);

    CHECK(chdir(original) == 0);
    CHECK(test_rmdir(dir) == 0);

    /* The directory is gone now. */
    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_ECHDIR);
    CHECK(pf_daemon_last_error(&d, &info) == PF_DAEMON_OK);
    CHECK(info.code == PF_DAEMON_ECHDIR);
    CHECK(info.osError == (uint32_t)ENOENT);
    return 0;
}

static int test_daemon_write_pid_file(int seed, int rep) {
    pf_daemon_t d;
    char pid[TEST_PATH_MAX], text[32], want[32];
    static char longpath[5000];

    pf_daemon_init(&d);
    test_path(pid, sizeof pid, "daemon-pid-direct");

    CHECK(pf_daemon_write_pid_file(&d, pid) == PF_DAEMON_OK);
    CHECK(d.pidFileCreated == 1);
    CHECK_STR(d.pidPath, pid);
    CHECK(test_read(pid, text, sizeof text) > 0);
    snprintf(want, sizeof want, "%ld\n", (long)getpid());
    CHECK_STR(text, want);

    /* Overwrites an old file. */
    CHECK(test_write(pid, "stale content that is longer\n", 29) == 0);
    CHECK(pf_daemon_write_pid_file(&d, pid) == PF_DAEMON_OK);
    CHECK(test_read(pid, text, sizeof text) > 0);
    CHECK_STR(text, want);
    unlink(pid);

    CHECK(pf_daemon_write_pid_file(&d, "") == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_write_pid_file(&d, NULL) == PF_DAEMON_EINVAL);
    CHECK(pf_daemon_write_pid_file(NULL, pid) == PF_DAEMON_EINVAL);

    memset(longpath, 'b', sizeof longpath - 1);
    longpath[0] = '/';
    CHECK(pf_daemon_write_pid_file(&d, longpath) == PF_DAEMON_EPIDFILE);
    return 0;
}

static int test_daemon_preserved_fds(int seed, int rep) {
    pf_daemon_options_t o;
    int keep[] = { 7, 9, 12 };

    pf_daemon_options_t_init(&o);
    CHECK(!pf_daemon_is_preserved_fd(7, &o));
    CHECK(!pf_daemon_is_preserved_fd(7, NULL));

    o.preserveFds = keep;
    o.preserveFdCount = 3;
    CHECK(pf_daemon_is_preserved_fd(7, &o));
    CHECK(pf_daemon_is_preserved_fd(9, &o));
    CHECK(pf_daemon_is_preserved_fd(12, &o));
    CHECK(!pf_daemon_is_preserved_fd(8, &o));
    CHECK(!pf_daemon_is_preserved_fd(0, &o));

    o.preserveFdCount = 1; /* only the first one counts */
    CHECK(pf_daemon_is_preserved_fd(7, &o));
    CHECK(!pf_daemon_is_preserved_fd(9, &o));
    return 0;
}

struct fd_probe {
    int keep;
    int other;
};

static int child_close_fds(void *p) {
    struct fd_probe *f = p;
    pf_daemon_options_t o;
    int keep[1];

    pf_daemon_options_t_init(&o);
    keep[0] = f->keep;
    o.preserveFds = keep;
    o.preserveFdCount = 1;

    if (pf_daemon_closeFds(&o) != PF_DAEMON_OK)
        return 1;
    if (fcntl(f->keep, F_GETFD) < 0)
        return 2; /* preserved descriptor was closed */
    if (fcntl(f->other, F_GETFD) >= 0)
        return 3; /* the other one is still open */
    if (fcntl(0, F_GETFD) >= 0 || fcntl(2, F_GETFD) >= 0)
        return 4; /* standard streams are closed too */
    return 0;
}

static int test_daemon_close_fds(int seed, int rep) {
    struct fd_probe f;

    f.keep = open("/dev/null", O_RDONLY);
    f.other = open("/dev/null", O_RDONLY);
    CHECK(f.keep >= 0 && f.other >= 0);
    CHECK(in_child(child_close_fds, &f) == 0);
    close(f.keep);
    close(f.other);
    return 0;
}

static int child_redirect(void *p) {
    pf_daemon_t d;
    char buf[4];
    ssize_t n;
    (void)p;

    pf_daemon_init(&d);
    if (pf_daemon_redirectStdio(&d, NULL) != PF_DAEMON_OK)
        return 1;
    /* Standard input reads as empty, output is swallowed. */
    n = read(STDIN_FILENO, buf, sizeof buf);
    if (n != 0)
        return 2;
    if (write(STDOUT_FILENO, "x", 1) != 1 || write(STDERR_FILENO, "x", 1) != 1)
        return 3;
    return 0;
}

static int test_daemon_redirect_stdio(int seed, int rep) {
    CHECK(in_child(child_redirect, NULL) == 0);
    return 0;
}

static volatile sig_atomic_t signal_seen;
static void on_signal(int sig) {
    (void)sig;
    signal_seen = 1;
}

static int child_ignore_signals(void *p) {
    pf_daemon_t d;
    struct sigaction sa;
    (void)p;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);

    pf_daemon_init(&d);
    if (pf_daemon_ignore_signals(&d) != PF_DAEMON_OK)
        return 1;
    raise(SIGHUP);
    if (signal_seen)
        return 2; /* the handler must have been replaced by "ignore" */
    return 0;
}

static int test_daemon_ignore_signals(int seed, int rep) {
    CHECK(in_child(child_ignore_signals, NULL) == 0);
    return 0;
}

/* The full double fork. The original process and the intermediate one
   return PF_DAEMON_PARENT; only the daemon gets PF_DAEMON_OK. */
static int test_daemon_daemonize(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char dir[TEST_PATH_MAX], pidfile[TEST_PATH_MAX], marker[TEST_PATH_MAX];
    char text[128];
    pid_t original = getpid();
    int rc, i;

    test_path(dir, sizeof dir, "daemon-fork");
    CHECK(test_mkdir(dir) == 0);
    test_join(pidfile, sizeof pidfile, dir, "daemon.pid");
    test_join(marker, sizeof marker, dir, "marker");

    pf_daemon_options_t_init(&o);
    o.daemonize = 1;
    o.closeFds = 0;
    o.redirectStdio = 0;
    o.workingDirectory = dir;
    o.pidPath = pidfile;

    fflush(NULL);
    rc = pf_daemon_start(&d, &o);

    if (rc == PF_DAEMON_OK) {
        /* The daemon: leave a note and disappear. */
        char note[128];
        int n = snprintf(
            note,
            sizeof note,
            "%ld %ld %ld %ld\n",
            (long)getpid(),
            (long)getppid(),
            (long)getsid(0),
            (long)getpgrp()
        );
        test_write(marker, note, (size_t)n);
        pf_daemon_stop(&d);
        _exit(0);
    }

    if (rc != PF_DAEMON_PARENT)
        _exit(2);

    /* The intermediate process has nothing left to do, and must leave
       quietly, as the documentation tells callers to do. */
    if (getpid() != original)
        _exit(0);

    for (i = 0; i < 400; i++) {
        FILE *f = fopen(marker, "rb");
        if (f) {
            fclose(f);
            test_sleep_ms(20); /* let it finish writing */
            break;
        }
        test_sleep_ms(20);
    }

    {
        long dpid = 0, dppid = 0, dsid = 0, dpgid = 0;
        long me = (long)getpid();

        CHECK(test_read(marker, text, sizeof text) > 0);
        CHECK(
            sscanf(text, "%ld %ld %ld %ld", &dpid, &dppid, &dsid, &dpgid) == 4
        );

        CHECK(dpid != me);
        CHECK(dsid != (long)getsid(0)); /* a session of its own */
        CHECK(dpgid != (long)getpgrp());
        CHECK(dppid != me); /* orphaned, adopted by init or a subreaper */
    }

    test_unlink(marker);
    test_unlink(pidfile);
    test_rmdir(dir);
    return 0;
}

#endif /* !_WIN32 */

#ifdef _WIN32
static int test_daemon_named_mutex(int seed, int rep) {
    pf_daemon_t d;
    pf_daemon_options_t o;
    char name[64];

    snprintf(name, sizeof name, "pf-test-daemon-%ld", test_pid());
    pf_daemon_options_t_init(&o);
    o.lockPath = name;

    CHECK(pf_daemon_start(&d, &o) == PF_DAEMON_OK);
    CHECK(pf_daemon_is_running(&d));
    CHECK(d.mutex != NULL);
    pf_daemon_stop(&d);
    CHECK(!pf_daemon_is_running(&d));
    CHECK(d.mutex == NULL);
    return 0;
}
#endif

pf_test_t suite_daemon[] = {
    { test_daemon_options_init, "/daemon/options_init" },
    { test_daemon_init_and_error, "/daemon/init_and_error" },
    { test_daemon_error_codes, "/daemon/error_codes" },
    { test_daemon_start_invalid, "/daemon/start_invalid" },
    { test_daemon_start_minimal, "/daemon/start_minimal" },
    { test_daemon_lock, "/daemon/lock" },
    { test_daemon_pid_file, "/daemon/pid_file" },
    { test_daemon_pid_file_error, "/daemon/pid_file_error" },
#ifndef _WIN32
    { test_daemon_singleton, "/daemon/singleton" },
    { test_daemon_lock_handover, "/daemon/lock_handover" },
    { test_daemon_lock_errors, "/daemon/lock_errors" },
    { test_daemon_default_lock_path, "/daemon/default_lock_path" },
    { test_daemon_working_directory, "/daemon/working_directory" },
    { test_daemon_write_pid_file, "/daemon/write_pid_file" },
    { test_daemon_preserved_fds, "/daemon/preserved_fds" },
    { test_daemon_close_fds, "/daemon/close_fds" },
    { test_daemon_redirect_stdio, "/daemon/redirect_stdio" },
    { test_daemon_ignore_signals, "/daemon/ignore_signals" },
    { test_daemon_daemonize, "/daemon/daemonize" },
#else
    { test_daemon_named_mutex, "/daemon/named_mutex" },
#endif
    { 0 },
};
