/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/signal.h> */

#include "../src/internal.h"

#include <cpolyfill/signal.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <signal.h>
#include <string.h>
#if PF_OS_POSIX
    #include <poll.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include "check.h"

/* Whether the shutdown descriptor is readable now (true off POSIX). */
static bool fd_ready(void) {
#if PF_OS_POSIX
    struct pollfd p;
    p.fd = pf_shutdown_fd();
    p.events = POLLIN;
    p.revents = 0;
    PF_ASSERT_ALWAYS_GE(p.fd, 0);
    return poll(&p, 1, 0) == 1 && (p.revents & POLLIN);
#else
    return pf_shutdown_requested();
#endif
}

#if PF_OS_POSIX
static void restore_default(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
}
#endif

static int test_names(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_STR_EQ(pf_signal_name(SIGINT), "SIGINT");
    PF_ASSERT_ALWAYS_STR_EQ(pf_signal_name(SIGSEGV), "SIGSEGV");
    PF_ASSERT_ALWAYS_STR_EQ(pf_signal_name(SIGTERM), "SIGTERM");
    PF_ASSERT_ALWAYS(pf_signal_name(0) == NULL);
    PF_ASSERT_ALWAYS(pf_signal_name(9999) == NULL);
    PF_ASSERT_ALWAYS_EQ(PF_SIGINT, SIGINT);
    PF_ASSERT_ALWAYS_EQ(PF_SIGTERM, SIGTERM);
#if PF_OS_POSIX
    PF_ASSERT_ALWAYS_EQ(PF_SIGHUP, SIGHUP);
    PF_ASSERT_ALWAYS_STR_EQ(pf_signal_name(SIGKILL), "SIGKILL");
#endif
    return 0;
}

static int test_request(int seed, int i) {
    struct timespec deadline;
    uint64_t start;
    (void)seed, (void)i;

    pf_shutdown_reset();
    PF_ASSERT_ALWAYS(!pf_shutdown_requested());
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), 0);
    PF_ASSERT_ALWAYS(!fd_ready());
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS(pf_shutdown_handle() != NULL);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_fd(), -1);
#else
    PF_ASSERT_ALWAYS(pf_shutdown_handle() == NULL);
#endif

    PF_ASSERT_ALWAYS_EQ(pf_shutdown_request(0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_request(PF_SIGTERM), PF_OK);
    PF_ASSERT_ALWAYS(pf_shutdown_requested());
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), PF_SIGTERM);
    /* the first request wins */
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_request(PF_SIGINT), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), PF_SIGTERM);
    PF_ASSERT_ALWAYS(fd_ready());
    PF_ASSERT_ALWAYS(fd_ready()); /* stays readable */
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_wait(NULL), PF_OK);
    pf_shutdown_done();

    pf_shutdown_reset();
    PF_ASSERT_ALWAYS(!pf_shutdown_requested());
    PF_ASSERT_ALWAYS(!fd_ready());
    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 50000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_wait(&deadline), PF_ETIMEDOUT);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 45000000u);
    /* a deadline in the past */
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_wait(&deadline), PF_ETIMEDOUT);
    return 0;
}

static int request_later(void *arg) {
    (void)arg;
    pf_sleep_ns(30000000);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_request(PF_SIGINT), PF_OK);
    return 0;
}

static int test_thread(int seed, int i) {
#if PF_CONFIG_THREADS
    struct timespec deadline;
    pf_thrd_t t;
    (void)seed, (void)i;

    pf_shutdown_reset();
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t, request_later, NULL), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&deadline, 10000000000), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_wait(&deadline), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), PF_SIGINT);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t, NULL), pf_thrd_success);
    pf_shutdown_reset();
    return 0;
#else
    (void)seed, (void)i;
    (void)request_later;
    return SKIP("no threads");
#endif
}

static int test_signals(int seed, int i) {
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_shutdown_init(0x100), PF_EINVAL);
#if PF_OS_POSIX
    pf_shutdown_reset();
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_init(0), PF_OK);
    PF_ASSERT_ALWAYS_EQ(raise(SIGINT), 0);
    PF_ASSERT_ALWAYS(pf_shutdown_requested());
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), SIGINT);
    PF_ASSERT_ALWAYS(fd_ready());
    /* without FORCE_SECOND, a second signal is ignored */
    PF_ASSERT_ALWAYS_EQ(raise(SIGTERM), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), SIGINT);

    pf_shutdown_reset();
    PF_ASSERT_ALWAYS_EQ(kill(getpid(), SIGTERM), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_wait(NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), SIGTERM);

    pf_shutdown_reset();
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_init(PF_SHUTDOWN_HUP), PF_OK);
    PF_ASSERT_ALWAYS_EQ(raise(SIGHUP), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_signal(), SIGHUP);
    pf_shutdown_reset();

    /* HUP handling turned off again: SIGHUP has its default action */
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_init(0), PF_OK);
    {
        struct sigaction sa;
        PF_ASSERT_ALWAYS_EQ(sigaction(SIGHUP, NULL, &sa), 0);
        PF_ASSERT_ALWAYS(sa.sa_handler == SIG_DFL);
        PF_ASSERT_ALWAYS_EQ(sigaction(SIGINT, NULL, &sa), 0);
        PF_ASSERT_ALWAYS(sa.sa_flags & SA_RESTART);
    }
    restore_default();
    return 0;
#else
    return SKIP("POSIX signals");
#endif
}

static int test_force_second(int seed, int i) {
#if PF_OS_POSIX
    int status = 0;
    pid_t pid;
    (void)seed, (void)i;

    fflush(NULL);
    pid = fork();
    PF_ASSERT_ALWAYS_GE(pid, 0);
    if (pid == 0) {
        if (pf_shutdown_init(PF_SHUTDOWN_FORCE_SECOND) != 0)
            _exit(2);
        raise(SIGTERM);
        if (!pf_shutdown_requested())
            _exit(3);
        raise(SIGTERM); /* ends the process */
        _exit(4);
    }
    PF_ASSERT_ALWAYS_EQ(waitpid(pid, &status, 0), pid);
    PF_ASSERT_ALWAYS_MSG(
        WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM, "status %#x",
        status
    );
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("POSIX signals");
#endif
}

pf_test_t suite_signal[] = {
    { test_names, "names" },
    { test_request, "request" },
    { test_thread, "thread" },
    { test_signals, "signals" },
    { test_force_second, "force_second" },
    { 0 },
};
