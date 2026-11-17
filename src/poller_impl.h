/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The poller's core (poller.c) and its backends: epoll, kqueue and
    poll() in posix/poller.c, an I/O completion port in win32/poller.c.

    The core keeps the registrations and the timers, and fills the
    caller's event array. A backend waits for the registrations it is
    given and reports what happened with pf__poller_emit.
*/

#ifndef CPOLYFILL_POLLER_IMPL_H
#define CPOLYFILL_POLLER_IMPL_H

#include <cpolyfill/poller.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    PF__PK_FREE,
    PF__PK_SOCKET,      /* h: the socket */
    PF__PK_FD,          /* h: the descriptor (POSIX) */
    PF__PK_PIPE,        /* h: the socket (POSIX) or HANDLE; obj: pf_pipe_t */
    PF__PK_PIPE_SERVER, /* h: the socket (POSIX); obj: pf_pipe_server_t */
    PF__PK_TIMER,       /* the core's alone */
    PF__PK_PROCESS,     /* h: pidfd or HANDLE; obj: pf_process_t */
    PF__PK_SHUTDOWN,    /* h: pf_shutdown_fd() or pf_shutdown_handle() */
    PF__PK_HANDLE,      /* h: a waitable HANDLE (Windows) */
    PF__PK_WATCHER      /* h: the watcher's descriptor or HANDLE; obj: the
                           watcher */
};

typedef struct pf__preg {
    int id;       /* 0 while the slot is free */
    unsigned gen; /* of the slot, for the next id */
    unsigned char kind;
    bool armed;      /* false: a one-shot that fired, a stopped timer, a
                        process already reported */
    bool ready_now;  /* report at the next wait without asking the
                        backend (a process that had ended already) */
    bool in_backend; /* the backend knows it (not timers, not processes
                        that had ended already) */
    unsigned events; /* PF_POLL_IN / OUT / ONESHOT asked for */
    void *data;
    intptr_t h;
    void *obj;
    void *be;    /* the backend's own state, if it needs any */
    size_t bidx; /* the backend's index (poll) */
    /* Timers. */
    int64_t due;
    int64_t period;
    size_t heap; /* index in the heap, SIZE_MAX if not scheduled */
    /* The batch being filled. */
    unsigned batch;
    size_t out;
    int next_free; /* free slots: the next one, -1 at the end */
} pf__preg;

typedef struct pf__poller pf__poller;

typedef struct pf__pbackend {
    int (*init)(pf__poller *p);
    void (*free)(pf__poller *p);
    /* A new registration (any kind but timers); 0 or a PF_E* code. */
    int (*add)(pf__poller *p, pf__preg *r);
    /* `r->events` or `r->armed` changed. */
    int (*update)(pf__poller *p, pf__preg *r);
    void (*remove)(pf__poller *p, pf__preg *r);
    /* Waits up to `timeout_ns` (negative: forever, 0: not at all) and
       emits what happened; 0 or a PF_E* code. May return early. */
    int (*wait)(pf__poller *p, int64_t timeout_ns);
    int (*wake)(pf__poller *p);
} pf__pbackend;

struct pf__poller {
    struct allocator_t *alloc;
    const pf__pbackend *be;
    void *bstate;
    pf__preg *regs;
    size_t nregs;
    size_t cap;
    int free_head;
    size_t nready; /* registrations with ready_now */
    /* The timers: slot indices, a min-heap on `due`. */
    size_t *heap;
    size_t nheap;
    size_t heap_cap;
    /* The wait in progress. */
    pf_poll_event_t *out;
    size_t count;
    size_t max;
    unsigned batch;
    size_t wake_out; /* the wake event's index, SIZE_MAX if none */
};

/* The registration of `id`, NULL if there is none. */
pf__preg *pf__poller_find(pf__poller *p, int id);

/* Reports `events` of registration `id` (0: a wake) in the wait in
   progress. Events of one registration are merged; events of disarmed
   registrations, or beyond the caller's array, are dropped. */
void pf__poller_emit(pf__poller *p, int id, unsigned events);

/* Room left in the caller's array. */
static inline size_t pf__poller_room(const pf__poller *p) {
    return p->max - p->count;
}

/* Milliseconds for a backend wait: rounded up, -1 for forever. */
static inline int pf__poller_ms(int64_t timeout_ns) {
    int64_t ms;
    if (timeout_ns < 0)
        return -1;
    ms = (timeout_ns + 999999) / 1000000;
    return ms > 0x7FFFFFFF ? 0x7FFFFFFF : (int)ms;
}

enum { PF__POLLER_DEFAULT, PF__POLLER_POLL };

/* The backends. NULL where there is none. */
extern const pf__pbackend *const pf__poller_native;
extern const pf__pbackend *const pf__poller_poll;

/* pf_poller_init with a chosen backend (the tests try them all). */
int pf__poller_init_with(
    pf_poller_t *p, struct allocator_t *alloc, int backend
);

/* Adds a registration of a kind the public functions do not offer (the
   watcher's): its id or a PF_E* code. */
int pf__poller_add(
    pf_poller_t *p, int kind, intptr_t h, void *obj, unsigned events, void *data
);

#endif /* CPOLYFILL_POLLER_IMPL_H */
