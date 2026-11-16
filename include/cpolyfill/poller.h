/** cpolyfill - Filling the gaps between C standards and compilers

    Waiting for many things at once: sockets, pipes, timers, processes and
    shutdown requests, on epoll (Linux), kqueue (macOS, the BSDs), an I/O
    completion port (Windows) or poll() elsewhere.

    The poller reports readiness, as epoll does: an event says a socket can
    be read without blocking, and the program then calls pf_recv itself
    (on a non-blocking socket, until it gives PF_EAGAIN). There are no
    callbacks; a program's event loop is a `for` around pf_poller_wait:

        pf_poller_t p;
        pf_poll_event_t ev[64];
        pf_poller_init(&p, NULL);
        pf_poller_add_socket(&p, listener, PF_POLL_IN, &server);
        pf_poller_add_shutdown(&p, NULL);
        for (;;) {
            int i, n = pf_poller_wait(&p, ev, 64, NULL);
            for (i = 0; i < n; i++)
                ... ev[i].data, ev[i].events ...
        }

    One thread uses a poller: it waits, and it adds, changes and removes
    registrations. Only pf_poller_wake may be called from other threads.

        pf_poller_t             A poller.
        int pf_poller_init(pf_poller_t *p, struct allocator_t *alloc)
                Memory comes from `alloc` (NULL: the library's default).
                0 or a negative PF_E* code.
        int pf_poller_free(pf_poller_t *p)
                Removes every registration (the sockets, pipes and
                processes stay open) and releases the poller.

    Registrations: each returns its id (> 0), or a negative PF_E* code.
    `data` comes back in every event of the registration. Ids are not
    reused soon, so an event of a registration removed meanwhile can be
    recognised. `events` is PF_POLL_IN and / or PF_POLL_OUT, with
    PF_POLL_ONESHOT to stop after one event until pf_poller_modify
    enables the registration again.

        int pf_poller_add_socket(pf_poller_t *p, pf_socket_t s,
                                 unsigned events, void *data)
                PF_POLL_IN: data or a connection to accept, PF_POLL_OUT:
                room to send (or a non-blocking connect is done).
        int pf_poller_add_fd(pf_poller_t *p, int fd, unsigned events,
                             void *data)
                POSIX: any descriptor that poll() takes (pipes,
                terminals, eventfd, ...). PF_ENOSYS on Windows.
        int pf_poller_add_pipe(pf_poller_t *p, pf_pipe_t *pipe,
                               unsigned events, void *data)
                A named pipe connection (<cpolyfill/pipe.h>). On Windows
                PF_POLL_OUT is always reported (a pipe cannot say whether
                a write would block), so pf_pipe_write may wait for the
                reader.
        int pf_poller_add_pipe_server(pf_poller_t *p,
                                      pf_pipe_server_t *srv, void *data)
                PF_POLL_IN when pf_pipe_accept would not wait.
        int pf_poller_add_timer(pf_poller_t *p, int64_t delay_ns,
                                int64_t period_ns, void *data)
                PF_POLL_FIRED after `delay_ns` (0 or more), then every
                `period_ns` (0: once). Measured on the monotonic clock,
                to the millisecond on Windows. A timer that fell behind
                fires once, not once per period missed.
        int pf_poller_set_timer(pf_poller_t *p, int id, int64_t delay_ns,
                                int64_t period_ns)
                Starts the timer `id` again with new times; a negative
                `delay_ns` stops it.
        int pf_poller_add_process(pf_poller_t *p, pf_process_t *proc,
                                  void *data)
                PF_POLL_FIRED once, when the process has ended: collect
                its status with pf_process_try_wait. `proc` must stay
                where it is while registered. PF_ENOSYS where the system
                cannot wait for a process without a signal handler
                (Linux before 5.3, systems without kqueue).
        int pf_poller_add_shutdown(pf_poller_t *p, void *data)
                PF_POLL_FIRED while a shutdown request is pending
                (<cpolyfill/signal.h>; pf_shutdown_reset clears it).
        int pf_poller_add_handle(pf_poller_t *p, void *handle,
                                 void *data)
                Windows: PF_POLL_FIRED while a waitable HANDLE is
                signalled (an auto-reset event or a semaphore is
                consumed by the wait). PF_ENOSYS elsewhere.

        int pf_poller_modify(pf_poller_t *p, int id, unsigned events)
                New `events` for a socket, descriptor or pipe; enables a
                PF_POLL_ONESHOT registration that fired.
        int pf_poller_remove(pf_poller_t *p, int id)
                Remove a registration before closing its socket, pipe or
                descriptor. PF_ENOENT for an unknown id.

    Waiting:

        pf_poll_event_t         id      the registration, 0 for a wake
                                data    as given when added
                                events  PF_POLL_IN, PF_POLL_OUT,
                                        PF_POLL_ERR (an error is pending:
                                        pf_socket_error), PF_POLL_HUP (the
                                        other side closed: reading gives
                                        0), PF_POLL_FIRED (timers,
                                        processes, shutdown, handles,
                                        wakes)

        int pf_poller_wait(pf_poller_t *p, pf_poll_event_t *ev, size_t n,
                           const struct timespec *deadline)
                Waits for events and stores up to `n` (one per
                registration): their count, 0 once the absolute UTC
                `deadline` (NULL: none) passes, or a PF_E* code.
        int pf_poller_wait_ns(pf_poller_t *p, pf_poll_event_t *ev,
                              size_t n, int64_t timeout_ns)
                The same with a timeout on the monotonic clock, which a
                change of the system's time does not affect: 0 does not
                wait, a negative timeout waits forever.
        int pf_poller_wake(pf_poller_t *p)
                Ends the current or next wait with an event of id 0. Any
                thread may call it; wakes before that wait ends are
                reported once.

    Registrations are level-triggered: a socket with unread data is
    reported by every wait until it is read. PF_POLL_ERR and PF_POLL_HUP
    are reported whether asked for or not, where the system says.
    Readiness is a hint: a read may still give PF_EAGAIN, so use
    non-blocking sockets. Regular files are not supported (they are
    always ready).

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_POLLER_H
#define CPOLYFILL_POLLER_H

#include <cpolyfill/pipe.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/process.h>
#include <cpolyfill/socket.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>

#define PF_POLL_IN 0x1u
#define PF_POLL_OUT 0x2u
#define PF_POLL_ERR 0x4u
#define PF_POLL_HUP 0x8u
#define PF_POLL_FIRED 0x10u
#define PF_POLL_ONESHOT 0x100u

struct allocator_t;

typedef struct pf_poller_t {
    /* Private. */
    void *pf__impl;
} pf_poller_t;

typedef struct pf_poll_event_t {
    int id;
    unsigned events;
    void *data;
} pf_poll_event_t;

PF_BEGIN_DECLS

PF_API int pf_poller_init(pf_poller_t *p, struct allocator_t *alloc);
PF_API int pf_poller_free(pf_poller_t *p);

PF_API int pf_poller_add_socket(
    pf_poller_t *p, pf_socket_t s, unsigned events, void *data
);
PF_API int pf_poller_add_fd(
    pf_poller_t *p, int fd, unsigned events, void *data
);
PF_API int pf_poller_add_pipe(
    pf_poller_t *p, pf_pipe_t *pipe, unsigned events, void *data
);
PF_API int pf_poller_add_pipe_server(
    pf_poller_t *p, pf_pipe_server_t *srv, void *data
);
PF_API int pf_poller_add_timer(
    pf_poller_t *p, int64_t delay_ns, int64_t period_ns, void *data
);
PF_API int pf_poller_set_timer(
    pf_poller_t *p, int id, int64_t delay_ns, int64_t period_ns
);
PF_API int pf_poller_add_process(
    pf_poller_t *p, pf_process_t *proc, void *data
);
PF_API int pf_poller_add_shutdown(pf_poller_t *p, void *data);
PF_API int pf_poller_add_handle(pf_poller_t *p, void *handle, void *data);

PF_API int pf_poller_modify(pf_poller_t *p, int id, unsigned events);
PF_API int pf_poller_remove(pf_poller_t *p, int id);

PF_API int pf_poller_wait(
    pf_poller_t *p,
    pf_poll_event_t *ev,
    size_t n,
    const struct timespec *deadline
);
PF_API int pf_poller_wait_ns(
    pf_poller_t *p, pf_poll_event_t *ev, size_t n, int64_t timeout_ns
);
PF_API int pf_poller_wake(pf_poller_t *p);

PF_END_DECLS

#endif /* CPOLYFILL_POLLER_H */
