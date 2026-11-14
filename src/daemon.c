/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/daemon.h>. Detaching: the original process keeps the read
    end of a close-on-exec pipe; the daemon writes its start result there
    (an int), the middle process an error of its own before exiting.
*/

#include "internal.h"

#include <cpolyfill/daemon.h>
#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/pipe.h>
#include <cpolyfill/process.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !PF_OS_WINDOWS
    #include <dirent.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #if PF_OS_LINUX || PF_OS_ANDROID
        #include <sys/socket.h>
        #include <sys/syscall.h>
        #include <sys/un.h>
    #endif
#endif

static int pf__daemon_errno(void) { return pf_error_from_errno(errno); }

/* ---- Lock and PID file ------------------------------------------------- */

static int pf__daemon_lock(pf_daemon_t *d, const char *path, uint32_t ms) {
    uint64_t deadline = pf_time_ns() + (uint64_t)ms * 1000000u;
    int fd, rc;

    fd = pf_open(path, PF_O_RDWR | PF_O_CREAT | PF_O_CLOEXEC, 0640);
    if (fd < 0)
        return pf__daemon_errno();
    for (;;) {
        rc = pf_lock_file(fd, PF_LOCK_EX | PF_LOCK_NB);
        if (rc == PF_OK)
            break;
        if (rc != PF_EWOULDBLOCK && rc != PF_EAGAIN) {
            pf_close(fd);
            return rc;
        }
        if (ms == 0) {
            pf_close(fd);
            return PF_EBUSY;
        }
        if (ms != UINT32_MAX && pf_time_ns() >= deadline) {
            pf_close(fd);
            return PF_ETIMEDOUT;
        }
        pf_sleep_ns(10000000u);
    }
    d->pf__lock_fd = fd;
    return PF_OK;
}

static int pf__daemon_pid_file(pf_daemon_t *d, const char *path) {
    char buf[32];
    size_t n = strlen(path);
    int len, rc;

    d->pf__pid = (long)pf_getpid();
    len = snprintf(buf, sizeof(buf), "%ld\n", d->pf__pid);
    rc = pf_write_file(path, buf, (size_t)len, PF_WRITE_ATOMIC);
    if (rc != PF_OK)
        return rc;
    d->pf__pid_path = (char *)allocate(NULL, n + 1);
    if (!d->pf__pid_path) {
        pf_unlink(path);
        return PF_ENOMEM;
    }
    memcpy(d->pf__pid_path, path, n + 1);
    return PF_OK;
}

/* Removes the PID file if it still holds our id (a newer instance may have
   replaced it). */
static void pf__daemon_remove_pid_file(pf_daemon_t *d) {
    void *data = NULL;
    size_t size = 0;

    if (!d->pf__pid_path)
        return;
    if (pf_read_file(d->pf__pid_path, &data, &size, NULL) == PF_OK) {
        if (strtol((const char *)data, NULL, 10) == d->pf__pid)
            pf_unlink(d->pf__pid_path);
        pf_read_file_free(data, size, NULL);
    }
    deallocate(NULL, d->pf__pid_path, strlen(d->pf__pid_path) + 1);
    d->pf__pid_path = NULL;
}

/* ---- Detaching (POSIX) ------------------------------------------------- */

#if !PF_OS_WINDOWS

static bool pf__daemon_kept(int fd, const int *keep, size_t n, int report) {
    size_t k;
    if (fd <= 2 || fd == report)
        return true;
    for (k = 0; k < n; k++)
        if (keep[k] == fd)
            return true;
    return false;
}

/* Closes every descriptor from `lo` up that is not kept, without walking
   up to RLIMIT_NOFILE (which may be a million or more). */
static void pf__daemon_close_fds(const int *keep, size_t n, int report) {
    int lo = 3, hi = report, fd;
    size_t k;
    DIR *dir;

    /* Below the highest kept descriptor one by one: few. */
    for (k = 0; k < n; k++)
        if (keep[k] > hi)
            hi = keep[k];
    for (fd = lo; fd <= hi; fd++)
        if (!pf__daemon_kept(fd, keep, n, report))
            close(fd);
    lo = hi + 1 > 3 ? hi + 1 : 3;

    #if (PF_OS_LINUX || PF_OS_ANDROID) && defined(SYS_close_range)
    if (syscall(SYS_close_range, (unsigned)lo, ~0u, 0u) == 0)
        return;
    #elif PF_OS_FREEBSD || PF_OS_OPENBSD || PF_OS_NETBSD
    closefrom(lo);
    return;
    #endif

    /* The open descriptors, from the system's list of them. */
    dir = opendir("/proc/self/fd");
    if (!dir)
        dir = opendir("/dev/fd");
    if (dir) {
        int list[256], count, self = dirfd(dir);
        bool more = true;
        while (more) {
            struct dirent *e;
            count = 0;
            more = false;
            rewinddir(dir);
            while ((e = readdir(dir)) != NULL) {
                long v;
                char *end;
                if (e->d_name[0] < '0' || e->d_name[0] > '9')
                    continue;
                v = strtol(e->d_name, &end, 10);
                if (*end || v < lo || v == self || v > INT_MAX)
                    continue;
                if (count == (int)(sizeof(list) / sizeof(list[0]))) {
                    more = true;
                    break;
                }
                list[count++] = (int)v;
            }
            for (k = 0; k < (size_t)count; k++)
                close(list[k]);
        }
        closedir(dir);
        return;
    }
    /* Last resort: a bounded loop. */
    {
        long max = sysconf(_SC_OPEN_MAX);
        if (max < 0 || max > 65536)
            max = 65536;
        for (fd = lo; fd < (int)max; fd++)
            close(fd);
    }
}

static int pf__daemon_null_stdio(void) {
    int fd = open("/dev/null", O_RDWR), k;
    if (fd < 0)
        return pf__daemon_errno();
    for (k = 0; k <= 2; k++) {
        if (fd != k && dup2(fd, k) < 0) {
            int rc = pf__daemon_errno();
            if (fd > 2)
                close(fd);
            return rc;
        }
    }
    if (fd > 2)
        close(fd);
    return PF_OK;
}

static void pf__daemon_report(int fd, int rc) {
    ssize_t n;
    if (fd < 0)
        return;
    do
        n = write(fd, &rc, sizeof(rc));
    while (n < 0 && errno == EINTR);
    close(fd);
}

/* Forks twice. In the original process: PF_DAEMON_PARENT or the daemon's
   error. In the daemon: 0, with `*report` the pipe to answer through. */
static int pf__daemon_detach(int *report) {
    int fds[2], rc, got_rc = 0;
    pid_t pid;
    ssize_t n;

    rc = pf_pipe(fds, 0);
    if (rc != PF_OK)
        return rc;
    fflush(NULL); /* or the children would write it out again */
    pid = fork();
    if (pid < 0) {
        rc = pf__daemon_errno();
        close(fds[0]);
        close(fds[1]);
        return rc;
    }
    if (pid > 0) {
        int st;
        close(fds[1]);
        do
            n = read(fds[0], &got_rc, sizeof(got_rc));
        while (n < 0 && errno == EINTR);
        close(fds[0]);
        while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
            ;
        if (n != (ssize_t)sizeof(got_rc))
            return PF_EIO; /* the daemon ended before reporting */
        return got_rc == PF_OK ? PF_DAEMON_PARENT : got_rc;
    }

    /* The middle process: a new session, then the daemon. */
    close(fds[0]);
    if (setsid() < 0) {
        pf__daemon_report(fds[1], pf__daemon_errno());
        _exit(1);
    }
    pid = fork();
    if (pid < 0) {
        pf__daemon_report(fds[1], pf__daemon_errno());
        _exit(1);
    }
    if (pid > 0)
        _exit(0);
    *report = fds[1];
    return PF_OK;
}

#endif

/* ---- Start and stop ---------------------------------------------------- */

PF_API int pf_daemon_start(pf_daemon_t *d, const pf_daemon_opts_t *o) {
    int rc = PF_OK, report = -1;

    if (!d)
        return PF_EINVAL;
    memset(d, 0, sizeof(*d));
    d->pf__lock_fd = -1;
    if (!o || (o->keep_fd_count && !o->keep_fds))
        return PF_EINVAL;

#if !PF_OS_WINDOWS
    if (o->flags & PF_DAEMON_DETACH) {
        rc = pf__daemon_detach(&report);
        if (rc != PF_OK)
            return rc; /* the original process */
    }
    if (o->flags & PF_DAEMON_SET_UMASK)
        umask((mode_t)o->umask);
#endif
    if (o->cwd && pf_chdir(o->cwd) != 0)
        rc = pf__daemon_errno();
#if !PF_OS_WINDOWS
    if (rc == PF_OK && (o->flags & PF_DAEMON_DETACH)) {
        if (!(o->flags & PF_DAEMON_KEEP_FDS))
            pf__daemon_close_fds(o->keep_fds, o->keep_fd_count, report);
        if (!(o->flags & PF_DAEMON_KEEP_STDIO))
            rc = pf__daemon_null_stdio();
    }
#endif
    if (rc == PF_OK && o->lock_path)
        rc = pf__daemon_lock(d, o->lock_path, o->lock_timeout_ms);
    if (rc == PF_OK && o->pid_path)
        rc = pf__daemon_pid_file(d, o->pid_path);
    if (rc == PF_OK)
        d->pf__started = true;
    else
        pf_daemon_stop(d);
#if !PF_OS_WINDOWS
    pf__daemon_report(report, rc);
#endif
    return rc;
}

PF_API void pf_daemon_stop(pf_daemon_t *d) {
    if (!d)
        return;
    pf__daemon_remove_pid_file(d);
    /* The lock file stays: removing it would let a second instance lock a
       new file while the first still holds the old one. */
    if (d->pf__lock_fd >= 0) {
        pf_close(d->pf__lock_fd);
        d->pf__lock_fd = -1;
    }
    d->pf__started = false;
}

PF_API bool pf_daemon_is_running(const pf_daemon_t *d) {
    return d && d->pf__started;
}

PF_API int pf_daemon_ignore_signals(void) {
#if PF_OS_WINDOWS
    return PF_OK;
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) != 0 || sigaction(SIGCHLD, &sa, NULL) != 0)
        return pf__daemon_errno();
    return PF_OK;
#endif
}

/* ---- systemd ----------------------------------------------------------- */

PF_API int pf_daemon_notify(const char *state) {
#if PF_OS_LINUX || PF_OS_ANDROID
    struct sockaddr_un addr;
    const char *path;
    size_t len;
    socklen_t addr_len;
    ssize_t sent;
    int fd, rc = PF_OK;

    if (!state)
        return PF_EINVAL;
    path = getenv("NOTIFY_SOCKET");
    if (!path || !*path)
        return PF_OK;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    len = strlen(path);
    if (len >= sizeof(addr.sun_path))
        return PF_ENAMETOOLONG;
    memcpy(addr.sun_path, path, len);
    if (path[0] == '@')
        addr.sun_path[0] = '\0'; /* the abstract namespace */
    addr_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + len
                           + (path[0] == '@' ? 0 : 1));
    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return pf__daemon_errno();
    do
        sent = sendto(
            fd,
            state,
            strlen(state),
            MSG_NOSIGNAL,
            (struct sockaddr *)&addr,
            addr_len
        );
    while (sent < 0 && errno == EINTR);
    if (sent < 0)
        rc = pf__daemon_errno();
    close(fd);
    return rc;
#else
    return state ? PF_OK : PF_EINVAL;
#endif
}

PF_API int pf_daemon_notify_ready(void) { return pf_daemon_notify("READY=1"); }

PF_API int pf_daemon_notify_stopping(void) {
    return pf_daemon_notify("STOPPING=1");
}

PF_API ptrdiff_t
pf_daemon_default_lock_path(const char *name, char *buf, size_t size) {
    char dir[PF_PATH_MAX];
    ptrdiff_t n;

    if (!name || !*name)
        return PF_EINVAL;
    n = pf_known_dir(PF_DIR_RUNTIME, dir, sizeof(dir));
    if (n < 0)
        n = pf_known_dir(PF_DIR_TEMP, dir, sizeof(dir));
    if (n < 0)
        return n;
    return pf_path_join(buf, size, dir, name);
}
