/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/poller.h>, on every backend of the platform. */

#include "../src/internal.h"

#include "../src/poller_impl.h"

#include <cpolyfill/pipe.h>
#include <cpolyfill/poller.h>
#include <cpolyfill/process.h>
#include <cpolyfill/random.h>
#include <cpolyfill/signal.h>
#include <cpolyfill/socket.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if PF_OS_POSIX
    #include <unistd.h>
#endif

#include "check.h"

#define MS 1000000LL

/* Runs `body(p)` on a fresh poller of every backend there is here. */
#define EACH_BACKEND(p, body)                                           \
    do {                                                                \
        int be_;                                                        \
        for (be_ = PF__POLLER_DEFAULT; be_ <= PF__POLLER_POLL; be_++) { \
            pf_poller_t p;                                              \
            int rc_ = pf__poller_init_with(&p, NULL, be_);              \
            if (rc_ == PF_ENOSYS)                                       \
                continue;                                               \
            PF_ASSERT_ALWAYS_EQ(rc_, 0);                                \
            body;                                                       \
            PF_ASSERT_ALWAYS_EQ(pf_poller_free(&p), 0);                 \
        }                                                               \
    } while (0)

/* The event of `id` among `ev[0..n)`, NULL if there is none. */
static const pf_poll_event_t *find(const pf_poll_event_t *ev, int n, int id) {
    int i;
    for (i = 0; i < n; i++)
        if (ev[i].id == id)
            return &ev[i];
    return NULL;
}

/* Waits up to `ms` for an event of `id`: its events, 0 if none came. */
static unsigned wait_for(pf_poller_t *p, int id, int64_t ms) {
    int64_t end = (int64_t)pf_time_ns() + ms * MS;
    for (;;) {
        pf_poll_event_t ev[16];
        int64_t left = end - (int64_t)pf_time_ns();
        int n = pf_poller_wait_ns(p, ev, 16, left > 0 ? left : 0);
        const pf_poll_event_t *e;
        PF_ASSERT_ALWAYS(n >= 0);
        e = find(ev, n, id);
        if (e)
            return e->events;
        if (left <= 0)
            return 0;
    }
}

static void socket_pair(pf_socket_t s[2]) {
    PF_ASSERT_ALWAYS_EQ(
        pf_socketpair(s, AF_UNIX, SOCK_STREAM, PF_SOCK_NONBLOCK), 0
    );
}

static void sockets_body(pf_poller_t *p) {
    pf_socket_t s[2];
    pf_poll_event_t ev[8];
    char buf[16];
    int id, n;

    socket_pair(s);
    id = pf_poller_add_socket(p, s[0], PF_POLL_IN, &s[0]);
    PF_ASSERT_ALWAYS(id > 0);

    /* Nothing to read. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 0), 0);

    /* Level-triggered: reported until read. */
    PF_ASSERT_ALWAYS_EQ(pf_send(s[1], "hello", 5, 0), 5);
    n = pf_poller_wait_ns(p, ev, 8, 1000 * MS);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(ev[0].id, id);
    PF_ASSERT_ALWAYS(ev[0].data == &s[0]);
    PF_ASSERT_ALWAYS(ev[0].events & PF_POLL_IN);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000) & PF_POLL_IN, PF_POLL_IN);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[0], buf, sizeof(buf), 0), 5);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 20 * MS), 0);

    /* IN and OUT come in one event. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_modify(p, id, PF_POLL_IN | PF_POLL_OUT), 0);
    PF_ASSERT_ALWAYS_EQ(pf_send(s[1], "x", 1, 0), 1);
    PF_ASSERT_ALWAYS_EQ(pf_sleep_ns(10 * MS), 0);
    n = pf_poller_wait_ns(p, ev, 8, 1000 * MS);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(ev[0].events & (PF_POLL_IN | PF_POLL_OUT),
                        PF_POLL_IN | PF_POLL_OUT);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[0], buf, sizeof(buf), 0), 1);

    /* OUT only: IN is not reported. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_modify(p, id, PF_POLL_OUT), 0);
    PF_ASSERT_ALWAYS_EQ(pf_send(s[1], "y", 1, 0), 1);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_OUT);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[0], buf, sizeof(buf), 0), 1);

    /* The other side closes: HUP (and IN, reading gives 0). */
    PF_ASSERT_ALWAYS_EQ(pf_poller_modify(p, id, PF_POLL_IN), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_close(s[1]), 0);
    {
        unsigned e = wait_for(p, id, 1000);
        PF_ASSERT_ALWAYS(e & (PF_POLL_IN | PF_POLL_HUP));
        PF_ASSERT_ALWAYS_EQ(pf_recv(s[0], buf, sizeof(buf), 0), 0);
    }

    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_poller_modify(p, id, PF_POLL_IN), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 0), 0);
    pf_socket_close(s[0]);
}

static int test_sockets(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, sockets_body(&p));
    return 0;
}

static void oneshot_body(pf_poller_t *p) {
    pf_socket_t s[2];
    pf_poll_event_t ev[8];
    int id;

    socket_pair(s);
    id = pf_poller_add_socket(p, s[0], PF_POLL_IN | PF_POLL_ONESHOT, NULL);
    PF_ASSERT_ALWAYS(id > 0);
    PF_ASSERT_ALWAYS_EQ(pf_send(s[1], "a", 1, 0), 1);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_IN);
    /* Disabled until modified, although the data is still there. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 20 * MS), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf_poller_modify(p, id, PF_POLL_IN | PF_POLL_ONESHOT), 0
    );
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_IN);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 20 * MS), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), 0);
    pf_socket_close(s[0]);
    pf_socket_close(s[1]);
}

static int test_oneshot(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, oneshot_body(&p));
    return 0;
}

static void timers_body(pf_poller_t *p) {
    pf_poll_event_t ev[8];
    int once, every, n, fired = 0;
    uint64_t start = pf_time_ns();
    int marker;

    once = pf_poller_add_timer(p, 30 * MS, 0, &marker);
    every = pf_poller_add_timer(p, 10 * MS, 10 * MS, NULL);
    PF_ASSERT_ALWAYS(once > 0 && every > 0 && once != every);

    /* The one-shot fires once, after its delay, with its data. */
    PF_ASSERT_ALWAYS_EQ(wait_for(p, once, 2000), PF_POLL_FIRED);
    PF_ASSERT_ALWAYS(pf_time_ns() - start >= 30 * MS);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, every), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 60 * MS), 0);

    /* Started again, then stopped. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_set_timer(p, once, 0, 0), 0);
    n = pf_poller_wait_ns(p, ev, 8, 1000 * MS);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(ev[0].id, once);
    PF_ASSERT_ALWAYS(ev[0].data == &marker);
    PF_ASSERT_ALWAYS_EQ(pf_poller_set_timer(p, once, 20 * MS, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_set_timer(p, once, -1, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 8, 50 * MS), 0);

    /* Periodic. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_set_timer(p, once, 5 * MS, 5 * MS), 0);
    while (fired < 3)
        if (wait_for(p, once, 2000) == PF_POLL_FIRED)
            fired++;
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, once), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_set_timer(p, once, 0, 0), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_poller_add_timer(p, -1, 0, NULL), PF_EINVAL);
}

static int test_timers(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, timers_body(&p));
    return 0;
}

/* Timers that come due together, and more events than room. */
static void many_body(pf_poller_t *p) {
    pf_poll_event_t ev[4];
    int ids[10], k, seen = 0, n;
    bool got[10];

    memset(got, 0, sizeof(got));
    for (k = 0; k < 10; k++) {
        ids[k] = pf_poller_add_timer(p, (int64_t)(10 - k) * MS, 0, NULL);
        PF_ASSERT_ALWAYS(ids[k] > 0);
    }
    PF_ASSERT_ALWAYS_EQ(pf_sleep_ns(20 * MS), 0);
    while (seen < 10) {
        n = pf_poller_wait_ns(p, ev, 4, 1000 * MS);
        PF_ASSERT_ALWAYS(n > 0 && n <= 4);
        for (k = 0; k < n; k++) {
            int j;
            for (j = 0; j < 10; j++)
                if (ids[j] == ev[k].id) {
                    PF_ASSERT_ALWAYS(!got[j]);
                    got[j] = true;
                    seen++;
                }
        }
    }
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 4, 0), 0);
    for (k = 0; k < 10; k++)
        PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, ids[k]), 0);
}

static int test_many(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, many_body(&p));
    return 0;
}

static int waker(void *arg) {
    pf_sleep_ns(30 * MS);
    return pf_poller_wake((pf_poller_t *)arg);
}

static void wake_body(pf_poller_t *p) {
    pf_poll_event_t ev[4];
    pf_thrd_t t;
    int n, res;

    /* Wakes before the wait are reported once. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_wake(p), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wake(p), 0);
    n = pf_poller_wait_ns(p, ev, 4, 1000 * MS);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(ev[0].id, 0);
    PF_ASSERT_ALWAYS_EQ(ev[0].events, PF_POLL_FIRED);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 4, 0), 0);

    /* From another thread, ending a wait without a deadline. */
#if PF_CONFIG_THREADS
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t, waker, p), pf_thrd_success);
    n = pf_poller_wait(p, ev, 4, NULL);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(ev[0].id, 0);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t, &res), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(res, 0);
#else
    (void)t, (void)res, (void)waker;
#endif
}

static int test_wake(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, wake_body(&p));
    return 0;
}

static void deadline_body(pf_poller_t *p) {
    pf_poll_event_t ev[4];
    struct timespec d;
    uint64_t start = pf_time_ns();

    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&d, 40 * MS), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait(p, ev, 4, &d), 0);
    PF_ASSERT_ALWAYS(pf_time_ns() - start >= 35 * MS);
    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 4, 40 * MS), 0);
    PF_ASSERT_ALWAYS(pf_time_ns() - start >= 40 * MS);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 0, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, NULL, 4, 0), PF_EINVAL);
}

static int test_deadline(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, deadline_body(&p));
    return 0;
}

static void ids_body(pf_poller_t *p) {
    int a, b;
    a = pf_poller_add_timer(p, 1000 * MS, 0, NULL);
    PF_ASSERT_ALWAYS(a > 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, a), 0);
    /* The slot is reused with another id. */
    b = pf_poller_add_timer(p, 1000 * MS, 0, NULL);
    PF_ASSERT_ALWAYS(b > 0 && b != a);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, a), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, 0), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, -5), PF_ENOENT);
    /* Timers cannot be modified like sockets. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_modify(p, b, PF_POLL_IN), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(
        pf_poller_add_socket(p, PF_SOCKET_INVALID, PF_POLL_IN, NULL),
        PF_EINVAL
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_poller_add_timer(p, 0, 0, NULL) > 0, true
    );
    /* Freed with registrations left. */
}

static int test_ids(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, ids_body(&p));
    return 0;
}

static void shutdown_body(pf_poller_t *p) {
    pf_poll_event_t ev[4];
    int id = pf_poller_add_shutdown(p, NULL);
    PF_ASSERT_ALWAYS(id > 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 4, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown_request(PF_SIGTERM), 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_FIRED);
    /* While pending, every wait reports it. */
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_FIRED);
    pf_shutdown_reset();
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(p, ev, 4, 20 * MS), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), 0);
}

static int test_shutdown(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, shutdown_body(&p));
    return 0;
}

static const char *pipe_name(void) {
    static char buf[64];
    snprintf(
        buf, sizeof(buf), "cpolyfill-poll-%lu-%08lx",
        (unsigned long)pf_getpid(), (unsigned long)pf_random_u32()
    );
    return buf;
}

static void pipes_body(pf_poller_t *p) {
    const char *name = pipe_name();
    pf_pipe_server_t srv;
    pf_pipe_t c, s;
    char buf[16];
    int sid, cid, rid;

    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, name), 0);
    sid = pf_poller_add_pipe_server(p, &srv, &srv);
    PF_ASSERT_ALWAYS(sid > 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, sid, 20), 0);

    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, name, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, sid, 2000), PF_POLL_IN);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, sid, 20), 0);

    rid = pf_poller_add_pipe(p, &s, PF_POLL_IN, NULL);
    cid = pf_poller_add_pipe(p, &c, PF_POLL_OUT, NULL);
    PF_ASSERT_ALWAYS(rid > 0 && cid > 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, rid, 20), 0);
    PF_ASSERT_ALWAYS(wait_for(p, cid, 2000) & PF_POLL_OUT);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_write(&c, "ping", 4), 4);
    PF_ASSERT_ALWAYS(wait_for(p, rid, 2000) & PF_POLL_IN);
    /* Still readable until read. */
    PF_ASSERT_ALWAYS(wait_for(p, rid, 2000) & PF_POLL_IN);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&s, buf, sizeof(buf)), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "ping", 4);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, rid, 20), 0);

    /* The client leaves: the end of the input. */
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, cid), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_close(&c), 0);
    PF_ASSERT_ALWAYS(wait_for(p, rid, 2000) & (PF_POLL_IN | PF_POLL_HUP));
    PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&s, buf, sizeof(buf)), 0);

    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, rid), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, sid), 0);
    pf_pipe_close(&s);
    pf_pipe_server_close(&srv);
}

static int test_pipes(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, pipes_body(&p));
    return 0;
}

static void fd_body(pf_poller_t *p) {
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_EQ(pf_poller_add_fd(p, 0, PF_POLL_IN, NULL), PF_ENOSYS);
#else
    int fds[2], id;
    char c;
    PF_ASSERT_ALWAYS_EQ(pf_pipe(fds, PF_PIPE_NONBLOCK), 0);
    id = pf_poller_add_fd(p, fds[0], PF_POLL_IN, NULL);
    PF_ASSERT_ALWAYS(id > 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 20), 0);
    PF_ASSERT_ALWAYS_EQ(write(fds[1], "z", 1), 1);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_IN);
    PF_ASSERT_ALWAYS_EQ(read(fds[0], &c, 1), 1);
    close(fds[1]);
    PF_ASSERT_ALWAYS(wait_for(p, id, 1000) & PF_POLL_HUP);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), 0);
    close(fds[0]);
    PF_ASSERT_ALWAYS_EQ(pf_poller_add_fd(p, -1, PF_POLL_IN, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(
        pf_poller_add_handle(p, (void *)1, NULL), PF_ENOSYS
    );
#endif
}

static int test_fd(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(p, fd_body(&p));
    return 0;
}

static void process_body(pf_poller_t *p, const char *child) {
    const char *argv[] = { child, "sleep", "50", NULL };
    pf_process_t proc;
    pf_exit_status_t st;
    int id;

    PF_ASSERT_ALWAYS_EQ(pf_spawn(&proc, argv, NULL), 0);
    id = pf_poller_add_process(p, &proc, &proc);
    if (id == PF_ENOSYS) {
        PF_ASSERT_ALWAYS_EQ(pf_process_wait(&proc, NULL, &st), 0);
        pf_process_free(&proc);
        return;
    }
    PF_ASSERT_ALWAYS(id > 0);
    PF_ASSERT_ALWAYS_EQ(pf_process_try_wait(&proc, &st), PF_EAGAIN);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 5000), PF_POLL_FIRED);
    PF_ASSERT_ALWAYS_EQ(pf_process_try_wait(&proc, &st), 0);
    PF_ASSERT_ALWAYS(st.exited);
    PF_ASSERT_ALWAYS_EQ(st.code, 0);
    /* Once only. */
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 30), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), 0);

    /* A process that was waited for already is reported at once. */
    id = pf_poller_add_process(p, &proc, NULL);
    PF_ASSERT_ALWAYS(id > 0);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 1000), PF_POLL_FIRED);
    PF_ASSERT_ALWAYS_EQ(wait_for(p, id, 30), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(p, id), 0);
    pf_process_free(&proc);
}

static int test_process(int seed, int i) {
    const char *child = getenv("PF_TEST_CHILD");
    (void)seed, (void)i;
    if (!child)
        return SKIP("PF_TEST_CHILD is not set (run through meson test)");
    EACH_BACKEND(p, process_body(&p, child));
    return 0;
}

pf_test_t suite_poller[] = {
    { test_sockets, "sockets" },
    { test_oneshot, "oneshot" },
    { test_timers, "timers" },
    { test_many, "many" },
    { test_wake, "wake" },
    { test_deadline, "deadline" },
    { test_ids, "ids" },
    { test_shutdown, "shutdown" },
    { test_pipes, "pipes" },
    { test_fd, "fd" },
    { test_process, "process" },
    { 0 },
};
