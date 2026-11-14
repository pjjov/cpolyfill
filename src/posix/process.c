/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/process.h> on POSIX: fork and execve, waitpid with pidfd
    (Linux) or kqueue (macOS, BSDs) for timeouts, poll for the pipes.
*/

#include "../internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/pipe.h>
#include <cpolyfill/process.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if PF_CONFIG_THREADS
    #include <pthread.h>
#endif
#if PF_OS_LINUX || PF_OS_ANDROID
    #include <sys/syscall.h>
#endif
#if PF_OS_APPLE || PF_OS_BSD
    #include <sys/event.h>
    #include <sys/time.h>
    #define PF__HAVE_KQUEUE 1
#else
    #define PF__HAVE_KQUEUE 0
#endif

#if PF_OS_APPLE
    #include <crt_externs.h>
    #define PF__ENVIRON (*_NSGetEnviron())
#else
extern char **environ;
    #define PF__ENVIRON environ
#endif

PF_API pf_pid_t pf_getpid(void) { return getpid(); }

PF_API pf_pid_t pf_getppid(void) { return getppid(); }

PF_API FILE *pf_popen(const char *command, const char *mode) {
    return popen(command, mode);
}

PF_API int pf_pclose(FILE *f) { return pclose(f); }

/* ---- Spawning ---------------------------------------------------------- */

/* A file descriptor flagged close-on-exec, at 3 or above (so it cannot be
   mistaken for a standard stream in the child): the new descriptor, or -1
   with errno. Closes `fd`. */
static int pf__fd_high(int fd) {
    int high;
    if (fd < 0)
        return -1;
#ifdef F_DUPFD_CLOEXEC
    high = fcntl(fd, F_DUPFD_CLOEXEC, 3);
#else
    high = fcntl(fd, F_DUPFD, 3);
    if (high >= 0)
        fcntl(high, F_SETFD, FD_CLOEXEC);
#endif
    {
        int err = errno;
        close(fd);
        errno = err;
    }
    return high;
}

/* A pipe, both ends close-on-exec and at 3 or above: 0 or a PF_E* code. */
static int pf__pipe(int p[2]) {
    int raw[2];
    int rc = pf_pipe(raw, 0);
    if (rc)
        return rc;
    p[0] = pf__fd_high(raw[0]);
    p[1] = pf__fd_high(raw[1]);
    if (p[0] < 0 || p[1] < 0) {
        int err = errno;
        if (p[0] >= 0)
            close(p[0]);
        if (p[1] >= 0)
            close(p[1]);
        return pf_error_from_errno(err);
    }
    return PF_OK;
}

static bool pf__is_exec(const char *path, int *err) {
    struct stat st;
    if (stat(path, &st) != 0) {
        *err = *err ? *err : PF_ENOENT;
        return false;
    }
    if (!S_ISREG(st.st_mode) || access(path, X_OK) != 0) {
        *err = PF_EACCES;
        return false;
    }
    return true;
}

/* The path to run for `name`: a copy in new memory (malloc), or NULL with
   `*rc` set. Relative paths are made absolute, since the child may change
   directory before it runs the program. */
static char *pf__resolve(const char *name, bool search, int *rc) {
    char *path;

    *rc = PF_OK;
    if (!name || !*name) {
        *rc = PF_EINVAL;
        return NULL;
    }
    if (!search || strchr(name, '/')) {
        if (name[0] == '/')
            path = strdup(name);
        else {
            char cwd[PF_PATH_MAX];
            size_t n;
            if (!getcwd(cwd, sizeof(cwd))) {
                *rc = pf_error_from_errno(errno);
                return NULL;
            }
            n = strlen(cwd) + strlen(name) + 2;
            path = (char *)malloc(n);
            if (path)
                snprintf(path, n, "%s/%s", cwd, name);
        }
        if (!path)
            *rc = PF_ENOMEM;
        return path;
    }

    {
        const char *list = getenv("PATH"), *p;
        size_t nlen = strlen(name);
        int err = 0;

        if (!list)
            list = "/usr/local/bin:/usr/bin:/bin";
        for (p = list;; p++) {
            const char *end = strchr(p, ':');
            size_t dlen = end ? (size_t)(end - p) : strlen(p);
            char *cand = (char *)malloc(dlen + nlen + 3);
            if (!cand) {
                *rc = PF_ENOMEM;
                return NULL;
            }
            if (dlen == 0) /* an empty entry is the current directory */
                memcpy(cand, ".", 2);
            else {
                memcpy(cand, p, dlen);
                cand[dlen] = '\0';
            }
            strcat(cand, "/");
            strcat(cand, name);
            if (pf__is_exec(cand, &err)) {
                if (cand[0] != '/') {
                    char *abs = pf__resolve(cand, false, rc);
                    free(cand);
                    return abs;
                }
                return cand;
            }
            free(cand);
            if (!end)
                break;
            p = end;
        }
        *rc = err ? err : PF_ENOENT;
        return NULL;
    }
}

static void pf__process_clear(pf_process_t *p) {
    memset(p, 0, sizeof(*p));
    p->pid = -1;
    p->pf__handle = -1;
}

/* Records `pid` in `p`, with a pidfd for timed waits where there is one. */
static void pf__process_track(pf_process_t *p, pid_t pid) {
    p->pid = pid;
#if (PF_OS_LINUX || PF_OS_ANDROID) && defined(SYS_pidfd_open)
    {
        long fd = syscall(SYS_pidfd_open, pid, 0);
        if (fd >= 0) {
            int high = pf__fd_high((int)fd);
            p->pf__handle = high >= 0 ? high : -1;
        }
    }
#endif
}

int pf__process_adopt(struct pf_process_t *p, long pid) {
    if (!p || pid <= 0)
        return PF_EINVAL;
    pf__process_clear(p);
    pf__process_track(p, (pid_t)pid);
    return PF_OK;
}

/* What the child runs, prepared by the parent: everything after fork is
   async-signal-safe. */
struct pf__child {
    const char *path;
    char *const *argv;
    char *const *envp;
    const char *cwd;
    int src[3]; /* descriptor for stdin, stdout, stderr; -1 inherits */
    bool err_to_out;
    bool new_group;
    int report; /* write end of the error pipe */
};

static void pf__child_fail(int report) {
    int err = errno;
    ssize_t rc;
    do
        rc = write(report, &err, sizeof(err));
    while (rc < 0 && errno == EINTR);
    _exit(127);
}

static void pf__child(const struct pf__child *c) {
    struct sigaction sa;
    sigset_t none;
    int src[3], k;

    /* A clean signal state: nothing blocked, SIGPIPE back to default
       (handlers are reset by exec, ignored signals are not). */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, NULL);

    if (c->new_group && setpgid(0, 0) != 0)
        pf__child_fail(c->report);
    if (c->cwd && chdir(c->cwd) != 0)
        pf__child_fail(c->report);

    /* Sources among 0..2 that go elsewhere move out of the way first. */
    for (k = 0; k < 3; k++) {
        src[k] = c->src[k];
        if (src[k] >= 0 && src[k] < 3 && src[k] != k) {
#ifdef F_DUPFD_CLOEXEC
            src[k] = fcntl(src[k], F_DUPFD_CLOEXEC, 3);
#else
            src[k] = fcntl(src[k], F_DUPFD, 3);
#endif
            if (src[k] < 0)
                pf__child_fail(c->report);
        }
    }
    for (k = 0; k < 3; k++) {
        if (src[k] < 0)
            continue;
        if (src[k] == k) {
            int fl = fcntl(k, F_GETFD);
            if (fl < 0 || fcntl(k, F_SETFD, fl & ~FD_CLOEXEC) != 0)
                pf__child_fail(c->report);
        } else if (dup2(src[k], k) < 0) {
            pf__child_fail(c->report);
        }
    }
    if (c->err_to_out && dup2(1, 2) < 0)
        pf__child_fail(c->report);

    execve(c->path, c->argv, c->envp);
    pf__child_fail(c->report);
}

PF_API int pf_spawn(
    pf_process_t *p, const char *const argv[], const pf_spawn_opts_t *opts
) {
    static const pf_spawn_opts_t none;
    struct pf__child c;
    int pipes[3][2] = { { -1, -1 }, { -1, -1 }, { -1, -1 } };
    int devnull = -1, report[2] = { -1, -1 };
    char *path;
    pid_t pid;
    int rc = PF_OK, k, err = 0;
    ssize_t got;

    if (!p)
        return PF_EINVAL;
    pf__process_clear(p);
    if (!argv || !argv[0])
        return PF_EINVAL;
    if (!opts)
        opts = &none;
    if (opts->flags
        & ~(PF_SPAWN_SEARCH_PATH | PF_SPAWN_STDERR_TO_STDOUT
            | PF_SPAWN_NEW_GROUP | PF_SPAWN_HIDE_WINDOW))
        return PF_EINVAL;
    for (k = 0; k < 3; k++)
        if (opts->stdio[k].mode < PF_STDIO_INHERIT
            || opts->stdio[k].mode > PF_STDIO_FD
            || (opts->stdio[k].mode == PF_STDIO_FD && opts->stdio[k].fd < 0))
            return PF_EINVAL;

    path = pf__resolve(argv[0], (opts->flags & PF_SPAWN_SEARCH_PATH) != 0, &rc);
    if (!path)
        return rc;
    if (!(opts->flags & PF_SPAWN_SEARCH_PATH) && !pf__is_exec(path, &err)) {
        free(path);
        return err;
    }

    memset(&c, 0, sizeof(c));
    c.path = path;
    c.argv = (char *const *)argv;
    c.envp = opts->env ? (char *const *)opts->env : PF__ENVIRON;
    c.cwd = opts->cwd;
    c.err_to_out = (opts->flags & PF_SPAWN_STDERR_TO_STDOUT) != 0;
    c.new_group = (opts->flags & PF_SPAWN_NEW_GROUP) != 0;

    for (k = 0; k < 3 && rc == PF_OK; k++) {
        c.src[k] = -1;
        switch (opts->stdio[k].mode) {
        case PF_STDIO_PIPE:
            rc = pf__pipe(pipes[k]);
            /* the child reads stdin, writes the others */
            c.src[k] = pipes[k][k == 0 ? 0 : 1];
            break;
        case PF_STDIO_NULL:
            if (devnull < 0) {
                devnull = pf__fd_high(open("/dev/null", O_RDWR | PF_O_CLOEXEC));
                if (devnull < 0)
                    rc = pf_error_from_errno(errno);
            }
            c.src[k] = devnull;
            break;
        case PF_STDIO_FD: c.src[k] = opts->stdio[k].fd; break;
        default: break;
        }
    }
    if (rc == PF_OK)
        rc = pf__pipe(report);
    if (rc != PF_OK)
        goto out;
    c.report = report[1];

    pid = fork();
    if (pid < 0) {
        rc = pf_error_from_errno(errno);
        goto out;
    }
    if (pid == 0)
        pf__child(&c); /* does not return */

    close(report[1]);
    report[1] = -1;
    do
        got = read(report[0], &err, sizeof(err));
    while (got < 0 && errno == EINTR);
    if (got == (ssize_t)sizeof(err)) {
        /* exec failed: reap the child and report why */
        int st;
        while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
            ;
        rc = pf_error_from_errno(err);
        goto out;
    }

    pf__process_track(p, pid);
    /* The caller's ends of the pipes. */
    for (k = 0; k < 3; k++) {
        int mine;
        pf_stream_t *s = k == 0 ? &p->in : k == 1 ? &p->out : &p->err;
        if (pipes[k][0] < 0)
            continue;
        mine = pipes[k][k == 0 ? 1 : 0];
        pipes[k][k == 0 ? 1 : 0] = -1;
        pf_stream_from_fd(s, mine, true);
    }

out:
    for (k = 0; k < 3; k++) {
        if (pipes[k][0] >= 0)
            close(pipes[k][0]);
        if (pipes[k][1] >= 0)
            close(pipes[k][1]);
    }
    if (devnull >= 0)
        close(devnull);
    if (report[0] >= 0)
        close(report[0]);
    if (report[1] >= 0)
        close(report[1]);
    free(path);
    return rc;
}

/* ---- Waiting ----------------------------------------------------------- */

static void pf__process_status(pf_process_t *p, int st) {
    p->pf__done = true;
    p->pf__status.exited = WIFEXITED(st) != 0;
    p->pf__status.code = WIFEXITED(st) ? WEXITSTATUS(st) : 0;
    p->pf__status.signal = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
}

/* waitpid, blocking or not: 0 when it ended, PF_EAGAIN, or an error. */
static int pf__reap(pf_process_t *p, bool block) {
    int st;
    pid_t rc;

    if (p->pf__done)
        return PF_OK;
    if (p->pid <= 0)
        return PF_EINVAL;
    do
        rc = waitpid(p->pid, &st, block ? 0 : WNOHANG);
    while (rc < 0 && errno == EINTR);
    if (rc < 0)
        return pf_error_from_errno(errno);
    if (rc == 0)
        return PF_EAGAIN;
    pf__process_status(p, st);
    return PF_OK;
}

static int pf__give_status(pf_process_t *p, pf_exit_status_t *status) {
    if (status)
        *status = p->pf__status;
    return PF_OK;
}

PF_API int pf_process_try_wait(pf_process_t *p, pf_exit_status_t *status) {
    int rc;
    if (!p)
        return PF_EINVAL;
    rc = pf__reap(p, false);
    return rc == PF_OK ? pf__give_status(p, status) : rc;
}

/* Milliseconds until `deadline` (rounded up, at most INT_MAX), 0 if it
   passed. */
static int pf__ms_left(const struct timespec *deadline) {
    struct timespec now, left;
    if (pf_clock_gettime(PF_CLOCK_REALTIME, &now) != 0)
        return 0;
    left = pf_timespec_sub(*deadline, now);
    if (left.tv_sec < 0)
        return 0;
    if (left.tv_sec >= INT_MAX / 1000 - 1)
        return INT_MAX;
    return (int)(left.tv_sec * 1000 + (left.tv_nsec + 999999) / 1000000);
}

/* Blocks up to `ms` for the process to end, or less: spurious returns are
   fine, the caller checks again. */
static void pf__wait_some(pf_process_t *p, int ms, int *backoff) {
    if (p->pf__handle >= 0) {
        struct pollfd pfd;
        pfd.fd = (int)p->pf__handle;
        pfd.events = POLLIN;
        pfd.revents = 0;
        (void)poll(&pfd, 1, ms);
        return;
    }
#if PF__HAVE_KQUEUE
    {
        int kq = kqueue();
        if (kq >= 0) {
            struct kevent ev;
            struct timespec ts;
            int rc;
            ts.tv_sec = ms / 1000;
            ts.tv_nsec = (long)(ms % 1000) * 1000000;
            EV_SET(&ev, p->pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT,
                   0, NULL);
            rc = kevent(kq, &ev, 1, &ev, 1, &ts);
            close(kq);
            if (rc >= 0 || errno != ESRCH)
                return;
        }
    }
#endif
    /* Polling, from 1 ms up to 50 ms between checks. */
    if (*backoff < ms)
        ms = *backoff;
    pf_sleep_ns((uint64_t)ms * 1000000u);
    if (*backoff < 50)
        *backoff *= 2;
}

PF_API int pf_process_wait(
    pf_process_t *p, const struct timespec *deadline, pf_exit_status_t *status
) {
    int rc, backoff = 1;

    if (!p)
        return PF_EINVAL;
    if (!deadline) {
        rc = pf__reap(p, true);
        return rc == PF_OK ? pf__give_status(p, status) : rc;
    }
    for (;;) {
        int ms;
        rc = pf__reap(p, false);
        if (rc == PF_OK)
            return pf__give_status(p, status);
        if (rc != PF_EAGAIN)
            return rc;
        ms = pf__ms_left(deadline);
        if (ms == 0)
            return PF_ETIMEDOUT;
        pf__wait_some(p, ms, &backoff);
    }
}

PF_API int pf_process_kill(pf_process_t *p, bool force) {
    if (!p || p->pid <= 0)
        return PF_EINVAL;
    if (p->pf__done)
        return PF_OK;
    /* The pid cannot be reused before it is waited for. */
    if (kill(p->pid, force ? SIGKILL : SIGTERM) != 0)
        return errno == ESRCH ? PF_OK : pf_error_from_errno(errno);
    return PF_OK;
}

PF_API int pf_process_free(pf_process_t *p) {
    if (!p)
        return PF_EINVAL;
    if (p->in.vt)
        pf_stream_close(&p->in);
    if (p->out.vt)
        pf_stream_close(&p->out);
    if (p->err.vt)
        pf_stream_close(&p->err);
    if (p->pf__handle >= 0)
        close((int)p->pf__handle);
    p->pf__handle = -1;
    return PF_OK;
}

/* ---- Communicating ------------------------------------------------------ */

/* The descriptor under a pipe stream of the process, -1 if closed. */
static int pf__stream_fd(const pf_stream_t *s) {
    return s->vt ? (int)(intptr_t)s->ctx : -1;
}

/* write() that reports a closed pipe as PF_EPIPE without raising SIGPIPE
   in this process. */
static pf_ssize_t pf__write_nosig(int fd, const void *buf, size_t n) {
    sigset_t pipe_set, old, pending;
    bool was_pending;
    ssize_t rc;
    int err;

    sigemptyset(&pipe_set);
    sigaddset(&pipe_set, SIGPIPE);
#if PF_CONFIG_THREADS
    pthread_sigmask(SIG_BLOCK, &pipe_set, &old);
#else
    sigprocmask(SIG_BLOCK, &pipe_set, &old);
#endif
    was_pending = sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE);
    rc = write(fd, buf, n);
    err = errno;
    if (rc < 0 && err == EPIPE && !was_pending) {
        /* Take the SIGPIPE this write raised before unblocking. */
        int sig;
        sigpending(&pending);
        if (sigismember(&pending, SIGPIPE))
            sigwait(&pipe_set, &sig);
    }
#if PF_CONFIG_THREADS
    pthread_sigmask(SIG_SETMASK, &old, NULL);
#else
    sigprocmask(SIG_SETMASK, &old, NULL);
#endif
    if (rc < 0)
        return pf_error_from_errno(err);
    return (pf_ssize_t)rc;
}

#define PF__COMM_BUF 65536

PF_API int pf_process_communicate(
    pf_process_t *p, pf_stream_t *in, pf_stream_t *out, pf_stream_t *err
) {
    unsigned char *inbuf = NULL, *rbuf = NULL;
    size_t inpos = 0, inlen = 0;
    bool in_end = in == NULL;
    int first_error = PF_OK;

    if (!p)
        return PF_EINVAL;
    if (pf__stream_fd(&p->in) >= 0) {
        if (in_end)
            pf_stream_close(&p->in);
        else {
            int fd = pf__stream_fd(&p->in), fl = fcntl(fd, F_GETFL);
            if (fl >= 0)
                fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        }
    }
    inbuf = (unsigned char *)malloc(PF__COMM_BUF);
    rbuf = (unsigned char *)malloc(PF__COMM_BUF);
    if (!inbuf || !rbuf) {
        free(inbuf);
        free(rbuf);
        return PF_ENOMEM;
    }

    for (;;) {
        struct pollfd fds[3];
        pf_stream_t *pipe_of[3];
        nfds_t n = 0, k;
        int infd = pf__stream_fd(&p->in);

        /* More input to send? */
        if (infd >= 0 && inpos == inlen && !in_end) {
            pf_ssize_t got = pf_stream_read(in, inbuf, PF__COMM_BUF);
            if (got < 0) {
                if (first_error == PF_OK)
                    first_error = (int)got;
                in_end = true;
            } else if (got == 0) {
                in_end = true;
            } else {
                inpos = 0;
                inlen = (size_t)got;
            }
        }
        if (infd >= 0 && inpos == inlen && in_end) {
            pf_stream_close(&p->in);
            infd = -1;
        }

        if (infd >= 0) {
            fds[n].fd = infd;
            fds[n].events = POLLOUT;
            pipe_of[n++] = &p->in;
        }
        if (pf__stream_fd(&p->out) >= 0) {
            fds[n].fd = pf__stream_fd(&p->out);
            fds[n].events = POLLIN;
            pipe_of[n++] = &p->out;
        }
        if (pf__stream_fd(&p->err) >= 0) {
            fds[n].fd = pf__stream_fd(&p->err);
            fds[n].events = POLLIN;
            pipe_of[n++] = &p->err;
        }
        if (n == 0)
            break;
        for (k = 0; k < n; k++)
            fds[k].revents = 0;
        if (poll(fds, n, -1) < 0) {
            if (errno == EINTR)
                continue;
            if (first_error == PF_OK)
                first_error = pf_error_from_errno(errno);
            break;
        }

        for (k = 0; k < n; k++) {
            if (!fds[k].revents)
                continue;
            if (pipe_of[k] == &p->in) {
                pf_ssize_t put = pf__write_nosig(
                    fds[k].fd, inbuf + inpos, inlen - inpos
                );
                if (put > 0)
                    inpos += (size_t)put;
                else if (put != PF_EAGAIN && put != PF_EINTR) {
                    /* The process closed its stdin (PF_EPIPE): the rest
                       of the input is dropped. */
                    if (put != PF_EPIPE && first_error == PF_OK)
                        first_error = (int)put;
                    pf_stream_close(&p->in);
                    in_end = true;
                    inpos = inlen = 0;
                }
            } else {
                pf_stream_t *dst = pipe_of[k] == &p->out ? out : err;
                ssize_t got = read(fds[k].fd, rbuf, PF__COMM_BUF);
                if (got < 0 && (errno == EINTR || errno == EAGAIN))
                    continue;
                if (got <= 0) {
                    if (got < 0 && first_error == PF_OK)
                        first_error = pf_error_from_errno(errno);
                    pf_stream_close(pipe_of[k]);
                    continue;
                }
                if (dst && first_error == PF_OK) {
                    int rc = pf_stream_write_all(dst, rbuf, (size_t)got);
                    if (rc != PF_OK)
                        first_error = rc;
                }
            }
        }
    }
    free(inbuf);
    free(rbuf);
    return first_error;
}
