/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "../src/internal.h"

#include <cpolyfill/daemon.h>
#include <cpolyfill/errno.h>
#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/process.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if PF_OS_POSIX
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif
#if PF_OS_LINUX
    #include <sys/socket.h>
    #include <sys/un.h>
#endif

#include "check.h"
#include "files.h"

static long read_long(const char *path) {
    return strtol(test_read(path), NULL, 10);
}

static int test_zero_options(int seed, int i) {
    pf_daemon_opts_t o;
    pf_daemon_t d;
    (void)seed, (void)i;

    memset(&o, 0, sizeof(o));
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_OK);
    PF_ASSERT_ALWAYS(pf_daemon_is_running(&d));
    pf_daemon_stop(&d);
    PF_ASSERT_ALWAYS(!pf_daemon_is_running(&d));
    pf_daemon_stop(&d); /* twice is fine */
    pf_daemon_stop(NULL);
    PF_ASSERT_ALWAYS(!pf_daemon_is_running(NULL));

    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(NULL, &o), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, NULL), PF_EINVAL);
    o.keep_fd_count = 1;
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_EINVAL);
    PF_ASSERT_ALWAYS(!pf_daemon_is_running(&d));
    pf_daemon_stop(&d);
    return 0;
}

static int test_lock(int seed, int i) {
    char dir[PF_PATH_MAX];
    pf_daemon_opts_t o;
    pf_daemon_t a, b;
    uint64_t start;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    memset(&o, 0, sizeof(o));
    o.lock_path = test_path(0, dir, "x.lock");
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&a, &o), PF_OK);

    /* A second instance, even in this process. */
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&b, &o), PF_EBUSY);
    PF_ASSERT_ALWAYS(!pf_daemon_is_running(&b));
    o.lock_timeout_ms = 60;
    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&b, &o), PF_ETIMEDOUT);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 50000000u);

    /* Released by stop; the lock file stays. */
    pf_daemon_stop(&a);
    PF_ASSERT_ALWAYS(pf_exists(o.lock_path));
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&b, &o), PF_OK);
    pf_daemon_stop(&b);

    /* No such directory. */
    o.lock_path = test_path(1, dir, "missing/x.lock");
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&b, &o), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_pid_file(int seed, int i) {
    char dir[PF_PATH_MAX];
    const char *pid, *lock;
    pf_daemon_opts_t o;
    pf_daemon_t d;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    pid = test_path(0, dir, "x.pid");
    lock = test_path(1, dir, "x.lock");
    memset(&o, 0, sizeof(o));
    o.pid_path = pid;
    o.lock_path = lock;
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_OK);
    PF_ASSERT_ALWAYS_EQ(read_long(pid), (long)pf_getpid());
    pf_daemon_stop(&d);
    PF_ASSERT_ALWAYS(!pf_exists(pid));

    /* Replaced by another instance meanwhile: left alone. */
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_OK);
    test_write(pid, "1\n");
    pf_daemon_stop(&d);
    PF_ASSERT_ALWAYS(pf_exists(pid));
    PF_ASSERT_ALWAYS_EQ(read_long(pid), 1);

    /* A failure to write it releases the lock. */
    o.pid_path = test_path(2, dir, "missing/x.pid");
    PF_ASSERT_ALWAYS(pf_daemon_start(&d, &o) < 0);
    PF_ASSERT_ALWAYS(!pf_daemon_is_running(&d));
    o.pid_path = NULL;
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_OK);
    pf_daemon_stop(&d);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_cwd(int seed, int i) {
    char dir[PF_PATH_MAX], before[PF_PATH_MAX], now[PF_PATH_MAX];
    char real[PF_PATH_MAX];
    pf_daemon_opts_t o;
    pf_daemon_t d;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    PF_ASSERT_ALWAYS_NOT_NULL(pf_getcwd(before, sizeof(before)));
    memset(&o, 0, sizeof(o));
    o.cwd = dir;
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_OK);
    PF_ASSERT_ALWAYS_NOT_NULL(pf_getcwd(now, sizeof(now)));
    PF_ASSERT_ALWAYS_NOT_NULL(pf_realpath(dir, real));
    PF_ASSERT_ALWAYS_STR_EQ(now, real);
    pf_daemon_stop(&d);
    PF_ASSERT_ALWAYS_EQ(pf_chdir(before), 0);

    o.cwd = test_path(0, dir, "missing");
    PF_ASSERT_ALWAYS_EQ(pf_daemon_start(&d, &o), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_default_lock_path(int seed, int i) {
    char dir[PF_PATH_MAX], buf[PF_PATH_MAX];
    ptrdiff_t n;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
#if PF_OS_POSIX && !PF_OS_APPLE
    pf_setenv("XDG_RUNTIME_DIR", dir, 1);
    n = pf_daemon_default_lock_path("app.lock", buf, sizeof(buf));
    PF_ASSERT_ALWAYS_GT(n, 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, test_path(0, dir, "app.lock"));
#else
    n = pf_daemon_default_lock_path("app.lock", buf, sizeof(buf));
    PF_ASSERT_ALWAYS_GT(n, 0);
    PF_ASSERT_ALWAYS(strstr(buf, "app.lock") != NULL);
#endif
    PF_ASSERT_ALWAYS_EQ((size_t)n, strlen(buf));
    PF_ASSERT_ALWAYS_EQ(
        pf_daemon_default_lock_path("app.lock", buf, 4), PF_ERANGE
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_daemon_default_lock_path("", buf, sizeof(buf)), PF_EINVAL
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_daemon_default_lock_path(NULL, buf, sizeof(buf)), PF_EINVAL
    );
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_notify(int seed, int i) {
    (void)seed, (void)i;

    pf_unsetenv("NOTIFY_SOCKET");
    PF_ASSERT_ALWAYS_EQ(pf_daemon_notify("READY=1"), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_daemon_notify_ready(), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_daemon_notify(NULL), PF_EINVAL);
#if PF_OS_LINUX
    {
        static const char *const names[] = { NULL, "@cpolyfill-test-notify" };
        char dir[PF_PATH_MAX], name[128], got[64];
        struct sockaddr_un addr;
        socklen_t len;
        int k;

        test_tmpdir(dir, sizeof(dir));
        for (k = 0; k < 2; k++) {
            int s = socket(AF_UNIX, SOCK_DGRAM, 0);
            ssize_t n;

            PF_ASSERT_ALWAYS_GE(s, 0);
            if (names[k])
                snprintf(
                    name, sizeof(name), "%s-%ld", names[k], (long)getpid()
                );
            else
                snprintf(name, sizeof(name), "%s", test_path(0, dir, "notify"));
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            memcpy(addr.sun_path, name, strlen(name));
            if (name[0] == '@')
                addr.sun_path[0] = '\0';
            len = (socklen_t)(offsetof(struct sockaddr_un, sun_path)
                              + strlen(name) + (name[0] == '@' ? 0 : 1));
            PF_ASSERT_ALWAYS_EQ(bind(s, (struct sockaddr *)&addr, len), 0);
            pf_setenv("NOTIFY_SOCKET", name, 1);

            PF_ASSERT_ALWAYS_EQ(pf_daemon_notify_ready(), PF_OK);
            n = recv(s, got, sizeof(got) - 1, 0);
            PF_ASSERT_ALWAYS_EQ(n, 7);
            got[n] = '\0';
            PF_ASSERT_ALWAYS_STR_EQ(got, "READY=1");
            PF_ASSERT_ALWAYS_EQ(pf_daemon_notify_stopping(), PF_OK);
            n = recv(s, got, sizeof(got) - 1, 0);
            got[n > 0 ? n : 0] = '\0';
            PF_ASSERT_ALWAYS_STR_EQ(got, "STOPPING=1");
            close(s);
        }
        /* Nobody listening. */
        pf_setenv("NOTIFY_SOCKET", test_path(1, dir, "none"), 1);
        PF_ASSERT_ALWAYS(pf_daemon_notify_ready() < 0);
        pf_unsetenv("NOTIFY_SOCKET");
        PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    }
#endif
    return 0;
}

static int test_ignore_signals(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_daemon_ignore_signals(), PF_OK);
#if PF_OS_POSIX
    {
        struct sigaction sa;
        PF_ASSERT_ALWAYS_EQ(sigaction(SIGHUP, NULL, &sa), 0);
        PF_ASSERT_ALWAYS(sa.sa_handler == SIG_IGN);
        signal(SIGHUP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
    }
#endif
    return 0;
}

#if PF_OS_POSIX
/* Runs in a child of the test: daemonizes with the files in `dir`. The
   original process writes "parent <rc>" to `who`, the daemon "daemon"
   plus what it saw, then waits for `dir`/stop. */
static void detach_child(const char *dir, bool second) {
    char who[PF_PATH_MAX + 16], stop[PF_PATH_MAX + 16],
        kept_path[PF_PATH_MAX + 16];
    pf_daemon_opts_t o;
    pf_daemon_t d;
    FILE *log;
    int rc, keep, extra, k;
    pid_t old_session = getsid(0);

    snprintf(who, sizeof(who), "%s/%s", dir, second ? "who2" : "who");
    snprintf(stop, sizeof(stop), "%s/stop", dir);
    snprintf(kept_path, sizeof(kept_path), "%s/kept", dir);
    log = fopen(who, "a");
    if (!log)
        _exit(2);
    /* Buffered, not flushed: must appear once. */
    fputs("x\n", log);

    keep = open(kept_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    extra = open("/dev/null", O_RDONLY);
    memset(&o, 0, sizeof(o));
    o.flags = PF_DAEMON_DETACH | PF_DAEMON_SET_UMASK;
    o.umask = 077;
    o.cwd = "/";
    o.lock_path = test_path(0, dir, "d.lock");
    o.pid_path = test_path(1, dir, "d.pid");
    o.keep_fds = &keep;
    o.keep_fd_count = 1;
    rc = pf_daemon_start(&d, &o);
    if (rc != PF_OK) {
        /* The original process (or a daemon that failed). */
        fprintf(log, "parent %d\n", rc);
        fclose(log);
        _exit(0);
    }
    /* The daemon: its log descriptor was closed, so write through `keep`. */
    {
        char line[256];
        int n = snprintf(
            line,
            sizeof(line),
            "daemon new_session=%d leader=%d extra=%d stdin=%d umask=%o\n",
            getsid(0) != old_session,
            getsid(0) == getpid(),
            fcntl(extra, F_GETFD) != -1,
            isatty(0),
            (unsigned)umask(0)
        );
        if (write(keep, line, (size_t)n) != n)
            _exit(3);
    }
    for (k = 0; k < 500 && access(stop, F_OK) != 0; k++)
        pf_sleep_ns(10000000u);
    pf_daemon_stop(&d);
    _exit(0);
}

static int run_child(const char *dir, bool second) {
    int st;
    pid_t pid = fork();
    PF_ASSERT_ALWAYS_GE(pid, 0);
    if (pid == 0)
        detach_child(dir, second);
    PF_ASSERT_ALWAYS_EQ(waitpid(pid, &st, 0), pid);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
#endif

static int test_detach(int seed, int i) {
#if PF_OS_POSIX && !PF_OS_EMSCRIPTEN
    char dir[PF_PATH_MAX], stop[PF_PATH_MAX + 16];
    const char *pid_path;
    char expect[64];
    long pid;
    int k;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    pid_path = test_path(1, dir, "d.pid");
    PF_ASSERT_ALWAYS_EQ(run_child(dir, false), 0);
    /* The original process returned PF_DAEMON_PARENT, once: the middle
       process did not return, and buffered output was not duplicated. */
    snprintf(expect, sizeof(expect), "x\nparent %d\n", PF_DAEMON_PARENT);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(test_path(2, dir, "who")), expect);
    /* The daemon was up when the parent returned. */
    pid = read_long(pid_path);
    PF_ASSERT_ALWAYS_GT(pid, 0);
    PF_ASSERT_ALWAYS_NE(pid, (long)getpid());
    PF_ASSERT_ALWAYS_EQ(kill((pid_t)pid, 0), 0);
    for (k = 0; k < 200 && test_read(test_path(2, dir, "kept"))[0] == '\0'; k++)
        pf_sleep_ns(10000000u);
    PF_ASSERT_ALWAYS_STR_EQ(
        test_read(test_path(2, dir, "kept")),
        "daemon new_session=1 leader=0 extra=0 stdin=0 umask=77\n"
    );

    /* A second one: its parent gets the daemon's error. */
    PF_ASSERT_ALWAYS_EQ(run_child(dir, true), 0);
    snprintf(expect, sizeof(expect), "x\nparent %d\n", PF_EBUSY);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(test_path(2, dir, "who2")), expect);
    PF_ASSERT_ALWAYS_EQ(read_long(pid_path), pid); /* untouched */

    /* Stop the first; it removes its PID file. */
    snprintf(stop, sizeof(stop), "%s/stop", dir);
    test_write(stop, "");
    for (k = 0; k < 500 && pf_exists(pid_path); k++)
        pf_sleep_ns(10000000u);
    PF_ASSERT_ALWAYS(!pf_exists(pid_path));
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("detaching is POSIX only");
#endif
}

pf_test_t suite_daemon[] = {
    { test_zero_options, "zero_options" },
    { test_lock, "lock" },
    { test_pid_file, "pid_file" },
    { test_cwd, "cwd" },
    { test_default_lock_path, "default_lock_path" },
    { test_notify, "notify" },
    { test_ignore_signals, "ignore_signals" },
    { test_detach, "detach" },
    { 0 },
};
