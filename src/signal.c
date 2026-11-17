/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/signal.h>: signal handlers and a self-pipe on POSIX, a
    console control handler and an event on Windows.
*/

#include "internal.h"

#include <cpolyfill/atomic.h>
#include <cpolyfill/signal.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <poll.h>
    #include <unistd.h>
#endif

/* The signal that asked first, 0 if none; the flags of pf_shutdown_init. */
static PF_ATOMIC(int) pf__shutdown_sig = 0;
static PF_ATOMIC(int) pf__shutdown_flags = 0;
static pf_once_flag pf__shutdown_once = PF_ONCE_FLAG_INIT;

/* Milliseconds until the UTC `deadline` (rounded up), -1 without one. */
static long long pf__ms_until(const struct timespec *deadline) {
    struct timespec now, left;
    long long ms;
    if (!deadline)
        return -1;
    if (pf_clock_gettime(PF_CLOCK_REALTIME, &now) != 0)
        return 0;
    left = pf_timespec_sub(*deadline, now);
    if (left.tv_sec < 0)
        return 0;
    if (left.tv_sec > (time_t)(INT_MAX / 1000 - 1))
        return INT_MAX;
    ms = (long long)left.tv_sec * 1000 + (left.tv_nsec + 999999) / 1000000;
    return ms;
}

PF_API bool pf_shutdown_requested(void) {
    return PF_ATOMIC_LOAD(&pf__shutdown_sig) != 0;
}

PF_API int pf_shutdown_signal(void) {
    return PF_ATOMIC_LOAD(&pf__shutdown_sig);
}

/* Records `sig` unless a request is pending: true if it was the first. */
static bool pf__shutdown_record(int sig) {
    int none = 0;
    return PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&pf__shutdown_sig, &none, sig);
}

#if PF_OS_WINDOWS

static HANDLE pf__shutdown_event; /* manual reset: set while pending */
static HANDLE pf__shutdown_done_event;
static PF_ATOMIC(int) pf__shutdown_installed = 0;

static void pf__shutdown_create(void) {
    pf__shutdown_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    pf__shutdown_done_event = CreateEventW(NULL, TRUE, FALSE, NULL);
}

static bool pf__shutdown_ready(void) {
    pf_call_once(&pf__shutdown_once, pf__shutdown_create);
    return pf__shutdown_event != NULL && pf__shutdown_done_event != NULL;
}

static BOOL WINAPI pf__shutdown_ctrl(DWORD type) {
    int sig;
    bool ending = false;

    switch (type) {
    case CTRL_C_EVENT:
        sig = PF_SIGINT;
        break;
    case CTRL_BREAK_EVENT:
        sig = PF_SIGBREAK;
        break;
    case CTRL_CLOSE_EVENT:
        sig = PF_SIGHUP;
        ending = true;
        break;
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        sig = PF_SIGTERM;
        ending = true;
        break;
    default:
        return FALSE;
    }
    if (!pf__shutdown_record(sig) && !ending
        && (PF_ATOMIC_LOAD(&pf__shutdown_flags) & PF_SHUTDOWN_FORCE_SECOND))
        return FALSE; /* the default handler ends the process */
    SetEvent(pf__shutdown_event);
    if (ending) {
        /* Returning lets Windows end the process: wait for the program. */
        WaitForSingleObject(pf__shutdown_done_event, INFINITE);
    }
    return TRUE;
}

PF_API int pf_shutdown_init(unsigned flags) {
    int expected = 0;

    if (flags & ~(PF_SHUTDOWN_HUP | PF_SHUTDOWN_FORCE_SECOND))
        return PF_EINVAL;
    if (!pf__shutdown_ready())
        return pf_error_last_win32();
    PF_ATOMIC_STORE(&pf__shutdown_flags, (int)flags);
    if (PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&pf__shutdown_installed, &expected, 1)
        && !SetConsoleCtrlHandler(pf__shutdown_ctrl, TRUE)) {
        PF_ATOMIC_STORE(&pf__shutdown_installed, 0);
        return pf_error_last_win32();
    }
    return PF_OK;
}

PF_API int pf_shutdown_wait(const struct timespec *deadline) {
    if (!pf__shutdown_ready())
        return pf_error_last_win32();
    for (;;) {
        long long ms;
        DWORD rc;
        if (pf_shutdown_requested())
            return PF_OK;
        ms = pf__ms_until(deadline);
        if (ms == 0)
            return PF_ETIMEDOUT;
        rc = WaitForSingleObject(
            pf__shutdown_event, ms < 0 ? INFINITE : (DWORD)ms
        );
        if (rc == WAIT_FAILED)
            return pf_error_last_win32();
    }
}

PF_API int pf_shutdown_request(int sig) {
    if (sig == 0)
        return PF_EINVAL;
    if (!pf__shutdown_ready())
        return pf_error_last_win32();
    pf__shutdown_record(sig);
    SetEvent(pf__shutdown_event);
    return PF_OK;
}

PF_API void pf_shutdown_reset(void) {
    if (!pf__shutdown_ready())
        return;
    PF_ATOMIC_STORE(&pf__shutdown_sig, 0);
    ResetEvent(pf__shutdown_event);
    ResetEvent(pf__shutdown_done_event);
    /* A request between the two stores keeps the event set. */
    if (pf_shutdown_requested())
        SetEvent(pf__shutdown_event);
}

PF_API int pf_shutdown_fd(void) { return -1; }

PF_API void *pf_shutdown_handle(void) {
    return pf__shutdown_ready() ? (void *)pf__shutdown_event : NULL;
}

PF_API void pf_shutdown_done(void) {
    if (pf__shutdown_ready())
        SetEvent(pf__shutdown_done_event);
}

#else /* POSIX */

/* The self-pipe: a byte in it while a request is pending. */
static int pf__shutdown_pipe[2] = { -1, -1 };

static bool pf__set_fd_flags(int fd) {
    int fl = fcntl(fd, F_GETFL), fdl = fcntl(fd, F_GETFD);
    return fl >= 0 && fdl >= 0 && fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0
        && fcntl(fd, F_SETFD, fdl | FD_CLOEXEC) == 0;
}

static void pf__shutdown_create(void) {
    int p[2];
    if (pipe(p) != 0)
        return;
    if (!pf__set_fd_flags(p[0]) || !pf__set_fd_flags(p[1])) {
        close(p[0]);
        close(p[1]);
        return;
    }
    pf__shutdown_pipe[0] = p[0];
    pf__shutdown_pipe[1] = p[1];
}

static bool pf__shutdown_ready(void) {
    pf_call_once(&pf__shutdown_once, pf__shutdown_create);
    return pf__shutdown_pipe[0] >= 0;
}

/* Async-signal-safe: one byte, ignoring a full pipe. */
static void pf__shutdown_poke(void) {
    int saved = errno;
    ssize_t rc;
    do
        rc = write(pf__shutdown_pipe[1], "!", 1);
    while (rc < 0 && errno == EINTR);
    errno = saved;
}

static void pf__shutdown_handler(int sig) {
    if (!pf__shutdown_record(sig)) {
        if (PF_ATOMIC_LOAD(&pf__shutdown_flags) & PF_SHUTDOWN_FORCE_SECOND) {
            /* The default action, delivered when the handler returns
               (the signal is blocked until then). */
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sigaction(sig, &sa, NULL);
            raise(sig);
        }
        return;
    }
    pf__shutdown_poke();
}

static int pf__shutdown_install(int sig, bool on) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on ? pf__shutdown_handler : SIG_DFL;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    /* One handler at a time. */
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGHUP);
    return sigaction(sig, &sa, NULL) == 0 ? PF_OK : pf_error_from_errno(errno);
}

PF_API int pf_shutdown_init(unsigned flags) {
    int rc;
    unsigned old;

    if (flags & ~(PF_SHUTDOWN_HUP | PF_SHUTDOWN_FORCE_SECOND))
        return PF_EINVAL;
    if (!pf__shutdown_ready())
        return PF_EMFILE;
    old = (unsigned)PF_ATOMIC_EXCHANGE(&pf__shutdown_flags, (int)flags);
    if ((rc = pf__shutdown_install(SIGINT, true)) != PF_OK
        || (rc = pf__shutdown_install(SIGTERM, true)) != PF_OK)
        return rc;
    if (flags & PF_SHUTDOWN_HUP)
        return pf__shutdown_install(SIGHUP, true);
    if (old & PF_SHUTDOWN_HUP)
        return pf__shutdown_install(SIGHUP, false);
    return PF_OK;
}

PF_API int pf_shutdown_wait(const struct timespec *deadline) {
    if (!pf__shutdown_ready())
        return PF_EMFILE;
    for (;;) {
        struct pollfd p;
        long long ms;
        if (pf_shutdown_requested())
            return PF_OK;
        ms = pf__ms_until(deadline);
        if (ms == 0)
            return PF_ETIMEDOUT;
        p.fd = pf__shutdown_pipe[0];
        p.events = POLLIN;
        p.revents = 0;
        if (poll(&p, 1, ms < 0 ? -1 : (int)ms) < 0 && errno != EINTR)
            return pf_error_from_errno(errno);
    }
}

PF_API int pf_shutdown_request(int sig) {
    if (sig == 0)
        return PF_EINVAL;
    if (!pf__shutdown_ready())
        return PF_EMFILE;
    if (pf__shutdown_record(sig))
        pf__shutdown_poke();
    return PF_OK;
}

PF_API void pf_shutdown_reset(void) {
    char buf[64];
    if (!pf__shutdown_ready())
        return;
    PF_ATOMIC_STORE(&pf__shutdown_sig, 0);
    while (read(pf__shutdown_pipe[0], buf, sizeof(buf)) > 0)
        ;
    /* A request between the store and the drain lost its byte. */
    if (pf_shutdown_requested())
        pf__shutdown_poke();
}

PF_API int pf_shutdown_fd(void) {
    return pf__shutdown_ready() ? pf__shutdown_pipe[0] : -1;
}

PF_API void *pf_shutdown_handle(void) { return NULL; }

PF_API void pf_shutdown_done(void) { }

#endif

PF_API const char *pf_signal_name(int sig) {
    static const struct {
        int sig;
        const char *name;
    } names[] = {
#ifdef SIGHUP
        { SIGHUP, "SIGHUP" },
#endif
        { SIGINT, "SIGINT" },
#ifdef SIGQUIT
        { SIGQUIT, "SIGQUIT" },
#endif
        { SIGILL, "SIGILL" },
#ifdef SIGTRAP
        { SIGTRAP, "SIGTRAP" },
#endif
        { SIGABRT, "SIGABRT" },
#ifdef SIGBUS
        { SIGBUS, "SIGBUS" },
#endif
        { SIGFPE, "SIGFPE" },
#ifdef SIGKILL
        { SIGKILL, "SIGKILL" },
#endif
#ifdef SIGUSR1
        { SIGUSR1, "SIGUSR1" },
#endif
        { SIGSEGV, "SIGSEGV" },
#ifdef SIGUSR2
        { SIGUSR2, "SIGUSR2" },
#endif
#ifdef SIGPIPE
        { SIGPIPE, "SIGPIPE" },
#endif
#ifdef SIGALRM
        { SIGALRM, "SIGALRM" },
#endif
        { SIGTERM, "SIGTERM" },
#ifdef SIGCHLD
        { SIGCHLD, "SIGCHLD" },
#endif
#ifdef SIGCONT
        { SIGCONT, "SIGCONT" },
#endif
#ifdef SIGSTOP
        { SIGSTOP, "SIGSTOP" },
#endif
#ifdef SIGTSTP
        { SIGTSTP, "SIGTSTP" },
#endif
#ifdef SIGTTIN
        { SIGTTIN, "SIGTTIN" },
#endif
#ifdef SIGTTOU
        { SIGTTOU, "SIGTTOU" },
#endif
#ifdef SIGURG
        { SIGURG, "SIGURG" },
#endif
#ifdef SIGXCPU
        { SIGXCPU, "SIGXCPU" },
#endif
#ifdef SIGXFSZ
        { SIGXFSZ, "SIGXFSZ" },
#endif
#ifdef SIGVTALRM
        { SIGVTALRM, "SIGVTALRM" },
#endif
#ifdef SIGPROF
        { SIGPROF, "SIGPROF" },
#endif
#ifdef SIGWINCH
        { SIGWINCH, "SIGWINCH" },
#endif
#ifdef SIGIO
        { SIGIO, "SIGIO" },
#endif
#ifdef SIGSYS
        { SIGSYS, "SIGSYS" },
#endif
#ifdef SIGBREAK
        { SIGBREAK, "SIGBREAK" },
#endif
    };
    size_t k;
    for (k = 0; k < sizeof(names) / sizeof(names[0]); k++)
        if (names[k].sig == sig)
            return names[k].name;
    return NULL;
}
