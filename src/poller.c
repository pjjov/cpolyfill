/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The poller's core: registrations, timers and the wait loop. The
    backends are in posix/poller.c and win32/poller.c.
*/

#include "internal.h"

#include "poller_impl.h"

#include <cpolyfill/signal.h>
#include <cpolyfill/time.h>

#include <string.h>

/* Ids: the slot (plus one) in the low bits, a generation above. */
#define PF__SLOT_BITS 20
#define PF__SLOT_MAX ((1u << PF__SLOT_BITS) - 1u)
#define PF__GEN_MAX 2047u

static pf__poller *pf__impl(pf_poller_t *p) {
    return p ? (pf__poller *)p->pf__impl : NULL;
}

pf__preg *pf__poller_find(pf__poller *p, int id) {
    size_t slot;
    if (id <= 0)
        return NULL;
    slot = ((unsigned)id & PF__SLOT_MAX) - 1u;
    if (slot >= p->nregs || p->regs[slot].id != id)
        return NULL;
    return &p->regs[slot];
}

void pf__poller_emit(pf__poller *p, int id, unsigned events) {
    pf__preg *r;
    pf_poll_event_t *e;

    if (id == 0) {
        if (p->wake_out != SIZE_MAX)
            return;
        if (p->count >= p->max)
            return;
        e = &p->out[p->count];
        e->id = 0;
        e->events = PF_POLL_FIRED;
        e->data = NULL;
        p->wake_out = p->count++;
        return;
    }
    r = pf__poller_find(p, id);
    if (!r)
        return;
    if (r->batch == p->batch) {
        p->out[r->out].events |= events;
        return;
    }
    if (!r->armed || !events || p->count >= p->max)
        return;
    e = &p->out[p->count];
    e->id = id;
    e->events = events;
    e->data = r->data;
    r->batch = p->batch;
    r->out = p->count++;
    if ((r->events & PF_POLL_ONESHOT) || r->kind == PF__PK_PROCESS) {
        r->armed = false;
        if (r->in_backend)
            p->be->update(p, r);
    }
}

/* ---- Timers ------------------------------------------------------------ */

static bool pf__heap_less(pf__poller *p, size_t a, size_t b) {
    return p->regs[p->heap[a]].due < p->regs[p->heap[b]].due;
}

static void pf__heap_swap(pf__poller *p, size_t a, size_t b) {
    size_t t = p->heap[a];
    p->heap[a] = p->heap[b];
    p->heap[b] = t;
    p->regs[p->heap[a]].heap = a;
    p->regs[p->heap[b]].heap = b;
}

static void pf__heap_up(pf__poller *p, size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (!pf__heap_less(p, i, parent))
            break;
        pf__heap_swap(p, i, parent);
        i = parent;
    }
}

static void pf__heap_down(pf__poller *p, size_t i) {
    for (;;) {
        size_t l = 2 * i + 1, r = l + 1, m = i;
        if (l < p->nheap && pf__heap_less(p, l, m))
            m = l;
        if (r < p->nheap && pf__heap_less(p, r, m))
            m = r;
        if (m == i)
            break;
        pf__heap_swap(p, i, m);
        i = m;
    }
}

static int pf__heap_push(pf__poller *p, size_t slot) {
    if (p->nheap == p->heap_cap) {
        size_t ncap = p->heap_cap ? p->heap_cap * 2 : 16;
        size_t *nh = (size_t *)reallocate(
            p->alloc, p->heap, p->heap_cap * sizeof(size_t),
            ncap * sizeof(size_t)
        );
        if (!nh)
            return PF_ENOMEM;
        p->heap = nh;
        p->heap_cap = ncap;
    }
    p->heap[p->nheap] = slot;
    p->regs[slot].heap = p->nheap;
    pf__heap_up(p, p->nheap++);
    return PF_OK;
}

static void pf__heap_remove(pf__poller *p, size_t slot) {
    size_t i = p->regs[slot].heap;
    if (i == SIZE_MAX)
        return;
    p->regs[slot].heap = SIZE_MAX;
    if (i != --p->nheap) {
        p->heap[i] = p->heap[p->nheap];
        p->regs[p->heap[i]].heap = i;
        pf__heap_down(p, i);
        pf__heap_up(p, i);
    }
}

/* Emits the timers due at `now`, while there is room. */
static void pf__fire_timers(pf__poller *p, int64_t now) {
    while (p->nheap > 0 && p->count < p->max) {
        size_t slot = p->heap[0];
        pf__preg *r = &p->regs[slot];
        if (r->due > now)
            break;
        pf__poller_emit(p, r->id, PF_POLL_FIRED);
        if (r->period > 0) {
            int64_t late = now - r->due;
            r->due += (late / r->period + 1) * r->period;
            pf__heap_down(p, 0);
        } else {
            pf__heap_remove(p, slot);
            r->armed = false;
        }
    }
}

/* ---- Registrations ----------------------------------------------------- */

static int pf__reg_new(pf__poller *p, pf__preg **out) {
    pf__preg *r;
    size_t slot;

    if (p->free_head >= 0) {
        slot = (size_t)p->free_head;
        p->free_head = p->regs[slot].next_free;
    } else {
        if (p->nregs >= PF__SLOT_MAX)
            return PF_ENOMEM;
        if (p->nregs == p->cap) {
            size_t ncap = p->cap ? p->cap * 2 : 16;
            pf__preg *nr = (pf__preg *)reallocate(
                p->alloc, p->regs, p->cap * sizeof(pf__preg),
                ncap * sizeof(pf__preg)
            );
            if (!nr)
                return PF_ENOMEM;
            p->regs = nr;
            p->cap = ncap;
        }
        slot = p->nregs++;
        memset(&p->regs[slot], 0, sizeof(pf__preg));
    }
    r = &p->regs[slot];
    {
        unsigned gen = r->gen % PF__GEN_MAX + 1u;
        memset(r, 0, sizeof(*r));
        r->gen = gen;
        r->id = (int)((gen << PF__SLOT_BITS) | (unsigned)(slot + 1));
    }
    r->heap = SIZE_MAX;
    r->bidx = SIZE_MAX;
    r->armed = true;
    r->next_free = -1;
    *out = r;
    return PF_OK;
}

static void pf__reg_release(pf__poller *p, pf__preg *r) {
    size_t slot = (size_t)(r - p->regs);
    if (r->ready_now)
        p->nready--;
    r->id = 0;
    r->kind = PF__PK_FREE;
    r->next_free = p->free_head;
    p->free_head = (int)slot;
}

int pf__poller_add(
    pf_poller_t *pp,
    int kind,
    intptr_t h,
    void *obj,
    unsigned events,
    void *data
) {
    pf__poller *p = pf__impl(pp);
    pf__preg *r;
    int rc, id;

    if (!p)
        return PF_EINVAL;
    rc = pf__reg_new(p, &r);
    if (rc)
        return rc;
    r->kind = (unsigned char)kind;
    r->h = h;
    r->obj = obj;
    r->events = events;
    r->data = data;
    id = r->id;
    if (kind == PF__PK_PROCESS && ((pf_process_t *)obj)->pf__done) {
        /* Ended already: reported by the next wait. */
        r->ready_now = true;
        p->nready++;
        return id;
    }
    rc = p->be->add(p, r);
    r = pf__poller_find(p, id);
    if (rc) {
        pf__reg_release(p, r);
        return rc;
    }
    r->in_backend = true;
    return id;
}

static unsigned pf__check_events(unsigned events) {
    return events & ~(PF_POLL_IN | PF_POLL_OUT | PF_POLL_ONESHOT);
}

PF_API int pf_poller_add_socket(
    pf_poller_t *p, pf_socket_t s, unsigned events, void *data
) {
    if (s == PF_SOCKET_INVALID || pf__check_events(events))
        return PF_EINVAL;
    return pf__poller_add(p, PF__PK_SOCKET, (intptr_t)s, NULL, events, data);
}

PF_API int pf_poller_add_fd(
    pf_poller_t *p, int fd, unsigned events, void *data
) {
#if PF_OS_WINDOWS
    (void)p, (void)fd, (void)events, (void)data;
    return PF_ENOSYS;
#else
    if (fd < 0 || pf__check_events(events))
        return PF_EINVAL;
    return pf__poller_add(p, PF__PK_FD, fd, NULL, events, data);
#endif
}

PF_API int pf_poller_add_pipe(
    pf_poller_t *p, pf_pipe_t *pipe, unsigned events, void *data
) {
    if (!pipe || pipe->pf__h == -1 || pf__check_events(events))
        return PF_EINVAL;
    return pf__poller_add(p, PF__PK_PIPE, pipe->pf__h, pipe, events, data);
}

PF_API int pf_poller_add_pipe_server(
    pf_poller_t *p, pf_pipe_server_t *srv, void *data
) {
    intptr_t h;
    int rc;
    if (!srv)
        return PF_EINVAL;
    rc = pf__pipe_server_poll_handle(srv, &h);
    if (rc)
        return rc;
    return pf__poller_add(
        p, PF__PK_PIPE_SERVER, h, srv, PF_POLL_IN, data
    );
}

PF_API int pf_poller_add_timer(
    pf_poller_t *pp, int64_t delay_ns, int64_t period_ns, void *data
) {
    pf__poller *p = pf__impl(pp);
    pf__preg *r;
    int rc, id;

    if (!p || delay_ns < 0 || period_ns < 0)
        return PF_EINVAL;
    rc = pf__reg_new(p, &r);
    if (rc)
        return rc;
    r->kind = PF__PK_TIMER;
    r->data = data;
    id = r->id;
    rc = pf_poller_set_timer(pp, id, delay_ns, period_ns);
    if (rc) {
        pf__reg_release(p, pf__poller_find(p, id));
        return rc;
    }
    return id;
}

PF_API int pf_poller_set_timer(
    pf_poller_t *pp, int id, int64_t delay_ns, int64_t period_ns
) {
    pf__poller *p = pf__impl(pp);
    pf__preg *r;
    size_t slot;

    if (!p || period_ns < 0)
        return PF_EINVAL;
    r = pf__poller_find(p, id);
    if (!r)
        return PF_ENOENT;
    if (r->kind != PF__PK_TIMER)
        return PF_EINVAL;
    slot = (size_t)(r - p->regs);
    pf__heap_remove(p, slot);
    r->period = period_ns;
    r->armed = delay_ns >= 0;
    if (!r->armed)
        return PF_OK;
    r->due = (int64_t)pf_time_ns() + delay_ns;
    return pf__heap_push(p, slot);
}

PF_API int pf_poller_add_process(
    pf_poller_t *p, pf_process_t *proc, void *data
) {
    if (!proc)
        return PF_EINVAL;
    /* The backend decides whether it can wait without a pidfd. */
    return pf__poller_add(p, PF__PK_PROCESS, proc->pf__handle, proc, 0, data);
}

PF_API int pf_poller_add_shutdown(pf_poller_t *p, void *data) {
#if PF_OS_WINDOWS
    void *h = pf_shutdown_handle();
    if (!h)
        return PF_ENOMEM;
    return pf__poller_add(p, PF__PK_SHUTDOWN, (intptr_t)h, NULL, 0, data);
#else
    int fd = pf_shutdown_fd();
    if (fd < 0)
        return PF_EMFILE;
    return pf__poller_add(p, PF__PK_SHUTDOWN, fd, NULL, 0, data);
#endif
}

PF_API int pf_poller_add_handle(pf_poller_t *p, void *handle, void *data) {
#if PF_OS_WINDOWS
    if (!handle)
        return PF_EINVAL;
    return pf__poller_add(p, PF__PK_HANDLE, (intptr_t)handle, NULL, 0, data);
#else
    (void)p, (void)handle, (void)data;
    return PF_ENOSYS;
#endif
}

PF_API int pf_poller_modify(pf_poller_t *pp, int id, unsigned events) {
    pf__poller *p = pf__impl(pp);
    pf__preg *r;
    unsigned old;
    bool was_armed;
    int rc;

    if (!p || pf__check_events(events))
        return PF_EINVAL;
    r = pf__poller_find(p, id);
    if (!r)
        return PF_ENOENT;
    if (r->kind != PF__PK_SOCKET && r->kind != PF__PK_FD
        && r->kind != PF__PK_PIPE)
        return PF_EINVAL;
    old = r->events;
    was_armed = r->armed;
    r->events = events;
    r->armed = true;
    rc = p->be->update(p, r);
    if (rc) {
        r->events = old;
        r->armed = was_armed;
    }
    return rc;
}

PF_API int pf_poller_remove(pf_poller_t *pp, int id) {
    pf__poller *p = pf__impl(pp);
    pf__preg *r;

    if (!p)
        return PF_EINVAL;
    r = pf__poller_find(p, id);
    if (!r)
        return PF_ENOENT;
    if (r->kind == PF__PK_TIMER)
        pf__heap_remove(p, (size_t)(r - p->regs));
    else if (r->in_backend)
        p->be->remove(p, r);
    pf__reg_release(p, r);
    return PF_OK;
}

/* ---- Life ------------------------------------------------------------- */

int pf__poller_init_with(
    pf_poller_t *pp, struct allocator_t *alloc, int backend
) {
    pf__poller *p;
    const pf__pbackend *be;
    int rc;

    if (!pp)
        return PF_EINVAL;
    pp->pf__impl = NULL;
    be = backend == PF__POLLER_POLL ? pf__poller_poll : pf__poller_native;
    if (!be)
        be = pf__poller_poll;
    if (!be)
        return PF_ENOSYS;
    p = (pf__poller *)zallocate(alloc, sizeof(*p));
    if (!p)
        return PF_ENOMEM;
    p->alloc = alloc;
    p->be = be;
    p->free_head = -1;
    p->wake_out = SIZE_MAX;
    rc = be->init(p);
    if (rc) {
        deallocate(alloc, p, sizeof(*p));
        return rc;
    }
    pp->pf__impl = p;
    return PF_OK;
}

PF_API int pf_poller_init(pf_poller_t *p, struct allocator_t *alloc) {
    return pf__poller_init_with(p, alloc, PF__POLLER_DEFAULT);
}

PF_API int pf_poller_free(pf_poller_t *pp) {
    pf__poller *p = pf__impl(pp);
    struct allocator_t *alloc;
    size_t i;

    if (!p)
        return PF_OK;
    for (i = 0; i < p->nregs; i++)
        if (p->regs[i].id)
            pf_poller_remove(pp, p->regs[i].id);
    p->be->free(p);
    alloc = p->alloc;
    deallocate(alloc, p->regs, p->cap * sizeof(pf__preg));
    deallocate(alloc, p->heap, p->heap_cap * sizeof(size_t));
    deallocate(alloc, p, sizeof(*p));
    pp->pf__impl = NULL;
    return PF_OK;
}

/* ---- Waiting ----------------------------------------------------------- */

/* Emits the registrations that are ready without the backend. */
static void pf__emit_ready(pf__poller *p) {
    size_t i;
    if (!p->nready)
        return;
    for (i = 0; i < p->nregs && p->count < p->max; i++) {
        pf__preg *r = &p->regs[i];
        if (r->id && r->ready_now) {
            r->ready_now = false;
            p->nready--;
            pf__poller_emit(p, r->id, PF_POLL_FIRED);
        }
    }
}

/* Nanoseconds left: until the UTC `deadline`, or the monotonic `end`
   (-1: none). Negative means forever. */
static int64_t pf__left(const struct timespec *deadline, int64_t end) {
    if (deadline) {
        struct timespec now;
        int64_t left;
        if (pf_clock_gettime(PF_CLOCK_REALTIME, &now) != 0)
            return 0;
        left = pf_timespec_to_ns(pf_timespec_sub(*deadline, now));
        return left > 0 ? left : 0;
    }
    if (end < 0)
        return -1;
    {
        int64_t now = (int64_t)pf_time_ns();
        return end > now ? end - now : 0;
    }
}

static int pf__wait(
    pf_poller_t *pp,
    pf_poll_event_t *ev,
    size_t n,
    const struct timespec *deadline,
    int64_t end
) {
    pf__poller *p = pf__impl(pp);

    if (!p || !ev || n == 0)
        return PF_EINVAL;
    p->out = ev;
    p->max = n > 0x7FFFFFFF ? 0x7FFFFFFF : n;
    p->count = 0;
    p->wake_out = SIZE_MAX;
    if (++p->batch == 0)
        p->batch = 1;
    for (;;) {
        int64_t left, wait;
        int rc;

        pf__emit_ready(p);
        pf__fire_timers(p, (int64_t)pf_time_ns());
        left = pf__left(deadline, end);
        wait = p->count ? 0 : left;
        if (wait != 0 && p->nheap > 0) {
            int64_t next = p->regs[p->heap[0]].due - (int64_t)pf_time_ns();
            if (next < 0)
                next = 0;
            if (wait < 0 || next < wait)
                wait = next;
        }
        if (p->count < p->max) {
            rc = p->be->wait(p, wait);
            if (rc < 0)
                return rc;
        }
        pf__fire_timers(p, (int64_t)pf_time_ns());
        if (p->count)
            break;
        if (left == 0)
            break;
    }
    p->out = NULL;
    return (int)p->count;
}

PF_API int pf_poller_wait(
    pf_poller_t *p,
    pf_poll_event_t *ev,
    size_t n,
    const struct timespec *deadline
) {
    return pf__wait(p, ev, n, deadline, -1);
}

PF_API int pf_poller_wait_ns(
    pf_poller_t *p, pf_poll_event_t *ev, size_t n, int64_t timeout_ns
) {
    int64_t end = -1;
    if (timeout_ns >= 0) {
        end = (int64_t)pf_time_ns() + timeout_ns;
        if (end < 0)
            end = INT64_MAX;
    }
    return pf__wait(p, ev, n, NULL, end);
}

PF_API int pf_poller_wake(pf_poller_t *pp) {
    pf__poller *p = pf__impl(pp);
    if (!p)
        return PF_EINVAL;
    return p->be->wake(p);
}
