/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The poller's POSIX backends: epoll (Linux), kqueue (macOS, the BSDs)
    and poll() (everywhere, and in the tests).
*/

#include "../internal.h"

#include "../poller_impl.h"

#include <cpolyfill/pipe.h>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#if PF_OS_LINUX || PF_OS_ANDROID
    #define PF__HAVE_EPOLL 1
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
#else
    #define PF__HAVE_EPOLL 0
#endif

#if PF_OS_APPLE || PF_OS_BSD
    #define PF__HAVE_KQUEUE 1
    #include <fcntl.h>
    #include <sys/event.h>
    #include <sys/time.h>
    #include <sys/types.h>
#else
    #define PF__HAVE_KQUEUE 0
#endif

/* Registrations that report PF_POLL_FIRED when their descriptor is
   readable. */
static bool pf__fired_kind(int kind) {
    return kind == PF__PK_PROCESS || kind == PF__PK_SHUTDOWN;
}

/* The descriptor to wait on, or a PF_E* code. */
static int pf__reg_fd(const pf__preg *r) {
    if (r->kind == PF__PK_HANDLE)
        return PF_ENOSYS;
    if (r->h < 0)
        return r->kind == PF__PK_PROCESS ? PF_ENOSYS : PF_EBADF;
    return (int)r->h;
}

/* Reads everything from a non-blocking descriptor. */
static void pf__drain(int fd) {
    char buf[64];
    while (read(fd, buf, sizeof(buf)) > 0)
        ;
}

/* ---- poll() ------------------------------------------------------------ */

typedef struct pf__pollbe {
    struct pollfd *fds; /* fds[0]: the wake pipe */
    int *ids;
    size_t n;
    size_t cap;     /* of `fds` */
    size_t ids_cap; /* of `ids` */
    int wake[2];
} pf__pollbe;

static short pf__poll_mask(const pf__preg *r) {
    short m = 0;
    if (!r->armed)
        return 0;
    if (pf__fired_kind(r->kind))
        return POLLIN;
    if (r->events & PF_POLL_IN)
        m |= POLLIN;
    if (r->events & PF_POLL_OUT)
        m |= POLLOUT;
    return m;
}

static int pf__poll_init(pf__poller *p) {
    pf__pollbe *b = (pf__pollbe *)zallocate(p->alloc, sizeof(*b));
    int rc;
    if (!b)
        return PF_ENOMEM;
    rc = pf_pipe(b->wake, PF_PIPE_NONBLOCK);
    if (rc) {
        deallocate(p->alloc, b, sizeof(*b));
        return rc;
    }
    b->cap = b->ids_cap = 16;
    b->fds = (struct pollfd *)allocate(p->alloc, b->cap * sizeof(*b->fds));
    b->ids = (int *)allocate(p->alloc, b->ids_cap * sizeof(int));
    if (!b->fds || !b->ids) {
        deallocate(p->alloc, b->fds, b->cap * sizeof(*b->fds));
        deallocate(p->alloc, b->ids, b->cap * sizeof(int));
        close(b->wake[0]);
        close(b->wake[1]);
        deallocate(p->alloc, b, sizeof(*b));
        return PF_ENOMEM;
    }
    b->fds[0].fd = b->wake[0];
    b->fds[0].events = POLLIN;
    b->ids[0] = 0;
    b->n = 1;
    p->bstate = b;
    return PF_OK;
}

static void pf__poll_free(pf__poller *p) {
    pf__pollbe *b = (pf__pollbe *)p->bstate;
    close(b->wake[0]);
    close(b->wake[1]);
    deallocate(p->alloc, b->fds, b->cap * sizeof(*b->fds));
    deallocate(p->alloc, b->ids, b->ids_cap * sizeof(int));
    deallocate(p->alloc, b, sizeof(*b));
}

static int pf__poll_add(pf__poller *p, pf__preg *r) {
    pf__pollbe *b = (pf__pollbe *)p->bstate;
    int fd = pf__reg_fd(r);
    if (fd < 0)
        return fd;
    if (b->n == b->ids_cap) {
        int *ni = (int *)reallocate(
            p->alloc, b->ids, b->ids_cap * sizeof(int),
            b->ids_cap * 2 * sizeof(int)
        );
        if (!ni)
            return PF_ENOMEM;
        b->ids = ni;
        b->ids_cap *= 2;
    }
    if (b->n == b->cap) {
        struct pollfd *nf = (struct pollfd *)reallocate(
            p->alloc, b->fds, b->cap * sizeof(*nf), b->cap * 2 * sizeof(*nf)
        );
        if (!nf)
            return PF_ENOMEM;
        b->fds = nf;
        b->cap *= 2;
    }
    r->bidx = b->n;
    b->fds[b->n].fd = fd;
    b->fds[b->n].events = pf__poll_mask(r);
    b->fds[b->n].revents = 0;
    b->ids[b->n] = r->id;
    b->n++;
    return PF_OK;
}

static int pf__poll_update(pf__poller *p, pf__preg *r) {
    pf__pollbe *b = (pf__pollbe *)p->bstate;
    /* A negative descriptor is skipped by poll(), errors included. */
    b->fds[r->bidx].fd = r->armed ? (int)r->h : -1;
    b->fds[r->bidx].events = pf__poll_mask(r);
    return PF_OK;
}

static void pf__poll_remove(pf__poller *p, pf__preg *r) {
    pf__pollbe *b = (pf__pollbe *)p->bstate;
    size_t i = r->bidx, last = b->n - 1;
    if (i != last) {
        pf__preg *moved = pf__poller_find(p, b->ids[last]);
        b->fds[i] = b->fds[last];
        b->ids[i] = b->ids[last];
        if (moved)
            moved->bidx = i;
    }
    b->n--;
}

static unsigned pf__poll_events(const pf__preg *r, short re) {
    unsigned ev = 0;
    if (pf__fired_kind(r->kind))
        return re ? PF_POLL_FIRED : 0;
    if ((re & POLLIN) && (r->events & PF_POLL_IN))
        ev |= PF_POLL_IN;
    if ((re & POLLOUT) && (r->events & PF_POLL_OUT))
        ev |= PF_POLL_OUT;
    if (re & (POLLERR | POLLNVAL))
        ev |= PF_POLL_ERR;
    if (re & POLLHUP)
        ev |= PF_POLL_HUP;
    return ev;
}

static int pf__poll_wait(pf__poller *p, int64_t timeout_ns) {
    pf__pollbe *b = (pf__pollbe *)p->bstate;
    size_t i;
    int n = poll(b->fds, (nfds_t)b->n, pf__poller_ms(timeout_ns));
    if (n < 0)
        return errno == EINTR ? PF_OK : pf_error_from_errno(errno);
    for (i = 0; i < b->n && n > 0 && pf__poller_room(p) > 0; i++) {
        short re = b->fds[i].revents;
        pf__preg *r;
        if (!re)
            continue;
        n--;
        if (b->ids[i] == 0) {
            pf__drain(b->wake[0]);
            pf__poller_emit(p, 0, PF_POLL_FIRED);
            continue;
        }
        r = pf__poller_find(p, b->ids[i]);
        if (r)
            pf__poller_emit(p, r->id, pf__poll_events(r, re));
    }
    return PF_OK;
}

static int pf__pipe_wake(int fd) {
    ssize_t rc;
    do
        rc = write(fd, "!", 1);
    while (rc < 0 && errno == EINTR);
    if (rc < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return pf_error_from_errno(errno);
    return PF_OK;
}

static int pf__poll_wake(pf__poller *p) {
    return pf__pipe_wake(((pf__pollbe *)p->bstate)->wake[1]);
}

static const pf__pbackend pf__poll_backend = {
    pf__poll_init,   pf__poll_free, pf__poll_add,  pf__poll_update,
    pf__poll_remove, pf__poll_wait, pf__poll_wake,
};

const pf__pbackend *const pf__poller_poll = &pf__poll_backend;

/* ---- epoll ------------------------------------------------------------- */

#if PF__HAVE_EPOLL

typedef struct pf__epollbe {
    int ep;
    int wake; /* eventfd */
} pf__epollbe;

static bool pf__epoll_oneshot(const pf__preg *r) {
    return (r->events & PF_POLL_ONESHOT) || r->kind == PF__PK_PROCESS;
}

static int pf__epoll_ctl(pf__poller *p, int op, pf__preg *r) {
    pf__epollbe *b = (pf__epollbe *)p->bstate;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    if (pf__fired_kind(r->kind)) {
        ev.events = EPOLLIN;
    } else {
        if (r->events & PF_POLL_IN)
            ev.events |= EPOLLIN | EPOLLRDHUP;
        if (r->events & PF_POLL_OUT)
            ev.events |= EPOLLOUT;
    }
    if (pf__epoll_oneshot(r))
        ev.events |= EPOLLONESHOT;
    ev.data.u64 = (uint64_t)(unsigned)r->id;
    if (epoll_ctl(b->ep, op, (int)r->h, &ev) != 0)
        return pf_error_from_errno(errno);
    return PF_OK;
}

static int pf__epoll_init(pf__poller *p) {
    pf__epollbe *b = (pf__epollbe *)allocate(p->alloc, sizeof(*b));
    struct epoll_event ev;
    if (!b)
        return PF_ENOMEM;
    b->ep = epoll_create1(EPOLL_CLOEXEC);
    b->wake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.u64 = 0;
    if (b->ep < 0 || b->wake < 0
        || epoll_ctl(b->ep, EPOLL_CTL_ADD, b->wake, &ev) != 0) {
        int err = errno;
        if (b->ep >= 0)
            close(b->ep);
        if (b->wake >= 0)
            close(b->wake);
        deallocate(p->alloc, b, sizeof(*b));
        return pf_error_from_errno(err);
    }
    p->bstate = b;
    return PF_OK;
}

static void pf__epoll_free(pf__poller *p) {
    pf__epollbe *b = (pf__epollbe *)p->bstate;
    close(b->ep);
    close(b->wake);
    deallocate(p->alloc, b, sizeof(*b));
}

static int pf__epoll_add(pf__poller *p, pf__preg *r) {
    int fd = pf__reg_fd(r);
    if (fd < 0)
        return fd;
    return pf__epoll_ctl(p, EPOLL_CTL_ADD, r);
}

static int pf__epoll_update(pf__poller *p, pf__preg *r) {
    /* A one-shot that fired is disabled by the kernel already. */
    if (!r->armed)
        return PF_OK;
    return pf__epoll_ctl(p, EPOLL_CTL_MOD, r);
}

static void pf__epoll_remove(pf__poller *p, pf__preg *r) {
    pf__epollbe *b = (pf__epollbe *)p->bstate;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    epoll_ctl(b->ep, EPOLL_CTL_DEL, (int)r->h, &ev);
}

static int pf__epoll_wait(pf__poller *p, int64_t timeout_ns) {
    pf__epollbe *b = (pf__epollbe *)p->bstate;
    struct epoll_event evs[64];
    size_t room = pf__poller_room(p);
    int i, n;

    n = epoll_wait(
        b->ep, evs, (int)(room < 64 ? room : 64), pf__poller_ms(timeout_ns)
    );
    if (n < 0)
        return errno == EINTR ? PF_OK : pf_error_from_errno(errno);
    for (i = 0; i < n; i++) {
        uint32_t e = evs[i].events;
        int id = (int)evs[i].data.u64;
        pf__preg *r;
        unsigned ev = 0;
        if (id == 0) {
            uint64_t v;
            if (read(b->wake, &v, sizeof(v)) < 0) {
                /* Nothing to do: the counter is reset either way. */
            }
            pf__poller_emit(p, 0, PF_POLL_FIRED);
            continue;
        }
        r = pf__poller_find(p, id);
        if (!r)
            continue;
        if (pf__fired_kind(r->kind)) {
            ev = PF_POLL_FIRED;
        } else {
            if ((e & EPOLLIN) && (r->events & PF_POLL_IN))
                ev |= PF_POLL_IN;
            if ((e & EPOLLOUT) && (r->events & PF_POLL_OUT))
                ev |= PF_POLL_OUT;
            if (e & EPOLLERR)
                ev |= PF_POLL_ERR;
            if (e & (EPOLLHUP | EPOLLRDHUP))
                ev |= PF_POLL_HUP;
        }
        pf__poller_emit(p, id, ev);
    }
    return PF_OK;
}

static int pf__epoll_wake(pf__poller *p) {
    pf__epollbe *b = (pf__epollbe *)p->bstate;
    uint64_t one = 1;
    ssize_t rc;
    do
        rc = write(b->wake, &one, sizeof(one));
    while (rc < 0 && errno == EINTR);
    /* EAGAIN: the counter is full, a wake is pending anyway. */
    if (rc < 0 && errno != EAGAIN)
        return pf_error_from_errno(errno);
    return PF_OK;
}

static const pf__pbackend pf__epoll_backend = {
    pf__epoll_init,   pf__epoll_free, pf__epoll_add,  pf__epoll_update,
    pf__epoll_remove, pf__epoll_wait, pf__epoll_wake,
};

const pf__pbackend *const pf__poller_native = &pf__epoll_backend;

/* ---- kqueue ------------------------------------------------------------ */

#elif PF__HAVE_KQUEUE

    /* NetBSD before 10 has an integer udata. */
    #if PF_OS_NETBSD && defined(__NetBSD_Version__) \
        && __NetBSD_Version__ < 1000000000
        #define PF__UDATA(id) ((intptr_t)(id))
        #define PF__UDATA_ID(u) ((int)(u))
    #else
        #define PF__UDATA(id) ((void *)(intptr_t)(id))
        #define PF__UDATA_ID(u) ((int)(intptr_t)(u))
    #endif

    /* The filters added for a registration, in its `bidx`. */
    #define PF__KQ_READ 0x1u
    #define PF__KQ_WRITE 0x2u

typedef struct pf__kqbe {
    int kq;
    int wake[2]; /* a self-pipe where there is no EVFILT_USER */
} pf__kqbe;

static int pf__kq_change(
    pf__kqbe *b, uintptr_t ident, int filter, unsigned flags,
    unsigned fflags, int id
) {
    struct kevent kev;
    EV_SET(&kev, ident, filter, flags, fflags, 0, PF__UDATA(id));
    if (kevent(b->kq, &kev, 1, NULL, 0, NULL) != 0)
        return pf_error_from_errno(errno);
    return PF_OK;
}

static int pf__kq_init(pf__poller *p) {
    pf__kqbe *b = (pf__kqbe *)allocate(p->alloc, sizeof(*b));
    int rc = PF_OK;
    if (!b)
        return PF_ENOMEM;
    b->wake[0] = b->wake[1] = -1;
    b->kq = kqueue();
    if (b->kq < 0) {
        rc = pf_error_from_errno(errno);
    } else {
        fcntl(b->kq, F_SETFD, FD_CLOEXEC);
    #ifdef EVFILT_USER
        rc = pf__kq_change(b, 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0);
    #else
        rc = pf_pipe(b->wake, PF_PIPE_NONBLOCK);
        if (!rc)
            rc = pf__kq_change(
                b, (uintptr_t)b->wake[0], EVFILT_READ, EV_ADD, 0, 0
            );
    #endif
    }
    if (rc) {
        if (b->kq >= 0)
            close(b->kq);
        if (b->wake[0] >= 0) {
            close(b->wake[0]);
            close(b->wake[1]);
        }
        deallocate(p->alloc, b, sizeof(*b));
        return rc;
    }
    p->bstate = b;
    return PF_OK;
}

static void pf__kq_free(pf__poller *p) {
    pf__kqbe *b = (pf__kqbe *)p->bstate;
    close(b->kq);
    if (b->wake[0] >= 0) {
        close(b->wake[0]);
        close(b->wake[1]);
    }
    deallocate(p->alloc, b, sizeof(*b));
}

/* Brings the read and write filters of `r` in line with what it asks. */
static int pf__kq_apply(pf__poller *p, pf__preg *r) {
    pf__kqbe *b = (pf__kqbe *)p->bstate;
    bool fired = pf__fired_kind(r->kind) || r->kind == PF__PK_PIPE_SERVER
              || r->kind == PF__PK_WATCHER;
    bool want_r = r->armed && (fired || (r->events & PF_POLL_IN));
    bool want_w = r->armed && !fired && (r->events & PF_POLL_OUT);
    unsigned once = 0;
    int rc;

    #ifdef EV_DISPATCH
    if (r->events & PF_POLL_ONESHOT)
        once = EV_DISPATCH;
    #endif
    if (want_r || (r->bidx & PF__KQ_READ)) {
        rc = pf__kq_change(
            b, (uintptr_t)r->h, EVFILT_READ,
            EV_ADD | (want_r ? EV_ENABLE | once : EV_DISABLE), 0, r->id
        );
        if (rc)
            return rc;
        r->bidx |= PF__KQ_READ;
    }
    if (want_w || (r->bidx & PF__KQ_WRITE)) {
        rc = pf__kq_change(
            b, (uintptr_t)r->h, EVFILT_WRITE,
            EV_ADD | (want_w ? EV_ENABLE | once : EV_DISABLE), 0, r->id
        );
        if (rc)
            return rc;
        r->bidx |= PF__KQ_WRITE;
    }
    return PF_OK;
}

static int pf__kq_add(pf__poller *p, pf__preg *r) {
    pf__kqbe *b = (pf__kqbe *)p->bstate;
    int rc;

    r->bidx = 0;
    if (r->kind == PF__PK_HANDLE)
        return PF_ENOSYS;
    if (r->kind == PF__PK_PROCESS) {
        pf_process_t *proc = (pf_process_t *)r->obj;
        rc = pf__kq_change(
            b, (uintptr_t)proc->pid, EVFILT_PROC, EV_ADD | EV_ONESHOT,
            NOTE_EXIT, r->id
        );
        if (rc == PF_ESRCH) {
            /* Ended (a zombie) before it could be watched. */
            r->ready_now = true;
            p->nready++;
            return PF_OK;
        }
        return rc;
    }
    if (r->h < 0)
        return PF_EBADF;
    rc = pf__kq_apply(p, r);
    if (rc) {
        pf__kq_change(b, (uintptr_t)r->h, EVFILT_READ, EV_DELETE, 0, 0);
        pf__kq_change(b, (uintptr_t)r->h, EVFILT_WRITE, EV_DELETE, 0, 0);
    }
    return rc;
}

static int pf__kq_update(pf__poller *p, pf__preg *r) {
    if (r->kind == PF__PK_PROCESS)
        return PF_OK; /* EV_ONESHOT: gone after it fired */
    return pf__kq_apply(p, r);
}

static void pf__kq_remove(pf__poller *p, pf__preg *r) {
    pf__kqbe *b = (pf__kqbe *)p->bstate;
    if (r->kind == PF__PK_PROCESS) {
        pf_process_t *proc = (pf_process_t *)r->obj;
        if (r->armed)
            pf__kq_change(
                b, (uintptr_t)proc->pid, EVFILT_PROC, EV_DELETE, 0, 0
            );
        return;
    }
    if (r->bidx & PF__KQ_READ)
        pf__kq_change(b, (uintptr_t)r->h, EVFILT_READ, EV_DELETE, 0, 0);
    if (r->bidx & PF__KQ_WRITE)
        pf__kq_change(b, (uintptr_t)r->h, EVFILT_WRITE, EV_DELETE, 0, 0);
}

static int pf__kq_wait(pf__poller *p, int64_t timeout_ns) {
    pf__kqbe *b = (pf__kqbe *)p->bstate;
    struct kevent evs[64];
    struct timespec ts, *tsp = NULL;
    size_t room = pf__poller_room(p);
    int i, n;

    if (timeout_ns >= 0) {
        ts.tv_sec = (time_t)(timeout_ns / 1000000000);
        ts.tv_nsec = (long)(timeout_ns % 1000000000);
        tsp = &ts;
    }
    n = kevent(b->kq, NULL, 0, evs, (int)(room < 64 ? room : 64), tsp);
    if (n < 0)
        return errno == EINTR ? PF_OK : pf_error_from_errno(errno);
    for (i = 0; i < n; i++) {
        const struct kevent *k = &evs[i];
        int id = PF__UDATA_ID(k->udata);
        pf__preg *r;
        unsigned ev = 0;
        if (id == 0) {
            if (b->wake[0] >= 0)
                pf__drain(b->wake[0]);
            pf__poller_emit(p, 0, PF_POLL_FIRED);
            continue;
        }
        r = pf__poller_find(p, id);
        if (!r)
            continue;
        if (k->flags & EV_ERROR) {
            ev = PF_POLL_ERR;
        } else if (k->filter == EVFILT_PROC || pf__fired_kind(r->kind)) {
            ev = PF_POLL_FIRED;
        } else if (k->filter == EVFILT_READ) {
            ev = PF_POLL_IN;
            if (k->flags & EV_EOF)
                ev |= PF_POLL_HUP | (k->fflags ? PF_POLL_ERR : 0);
        } else if (k->filter == EVFILT_WRITE) {
            ev = PF_POLL_OUT;
            if (k->flags & EV_EOF)
                ev |= PF_POLL_HUP | (k->fflags ? PF_POLL_ERR : 0);
        }
        pf__poller_emit(p, id, ev);
    }
    return PF_OK;
}

static int pf__kq_wake(pf__poller *p) {
    pf__kqbe *b = (pf__kqbe *)p->bstate;
    #ifdef EVFILT_USER
    if (b->wake[0] < 0)
        return pf__kq_change(b, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0);
    #endif
    return pf__pipe_wake(b->wake[1]);
}

static const pf__pbackend pf__kq_backend = {
    pf__kq_init,   pf__kq_free, pf__kq_add,  pf__kq_update,
    pf__kq_remove, pf__kq_wait, pf__kq_wake,
};

const pf__pbackend *const pf__poller_native = &pf__kq_backend;

#else

const pf__pbackend *const pf__poller_native = NULL;

#endif
