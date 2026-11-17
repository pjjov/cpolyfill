/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    File watching on POSIX: inotify (Linux) and kqueue (macOS, the BSDs)
    tell the engine (watch.c) which directories changed. Elsewhere the
    engine scans.
*/

#include "../internal.h"

#include "../watch_impl.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#if PF_OS_LINUX || PF_OS_ANDROID

    #include <sys/inotify.h>

    #define PF__IN_MASK                                                 \
        (IN_CREATE | IN_DELETE | IN_MODIFY | IN_ATTRIB | IN_MOVED_FROM  \
         | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF \
         | IN_EXCL_UNLINK | IN_ONLYDIR)

static int pf__in_fd(pf__watcher *w) { return (int)(intptr_t)w->bstate; }

static int pf__in_init(pf__watcher *w) {
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0)
        return pf_error_from_errno(errno);
    w->bstate = (void *)(intptr_t)fd;
    return PF_OK;
}

static void pf__in_free(pf__watcher *w) { close(pf__in_fd(w)); }

static int pf__in_dir_add(pf__watcher *w, pf__watch *wt, pf__wdir *d) {
    size_t len;
    char *path = pf__watch_path(w, wt, d, NULL, &len);
    int wd;
    if (!path)
        return PF_ENOMEM;
    wd = inotify_add_watch(
        pf__in_fd(w), path, PF__IN_MASK | (d->rel_len ? IN_DONT_FOLLOW : 0)
    );
    pf__watch_path_free(w, path, len);
    if (wd < 0)
        return pf_error_from_errno(errno);
    d->bh = wd;
    return PF_OK;
}

static void pf__in_dir_remove(pf__watcher *w, pf__wdir *d) {
    size_t i;
    /* The same directory in two watches has one watch descriptor. */
    for (i = 0; i < w->ndirs; i++) {
        pf__wdir *o = w->dirs[i];
        if (o != d && !o->dead && o->bh == d->bh)
            return;
    }
    inotify_rm_watch(pf__in_fd(w), (int)d->bh);
}

static int pf__in_collect(pf__watcher *w, int64_t timeout_ns) {
    union {
        struct inotify_event ev;
        char buf[16384];
    } u;
    int fd = pf__in_fd(w);

    if (timeout_ns != 0) {
        struct pollfd p;
        int ms = timeout_ns < 0 ? -1
            : timeout_ns / 1000000 >= 0x7FFFFFFF
            ? 0x7FFFFFFF
            : (int)((timeout_ns + 999999) / 1000000);
        p.fd = fd;
        p.events = POLLIN;
        p.revents = 0;
        if (poll(&p, 1, ms) < 0)
            return errno == EINTR ? PF_OK : pf_error_from_errno(errno);
    }
    for (;;) {
        ssize_t n = read(fd, u.buf, sizeof(u.buf));
        char *at = u.buf;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return PF_OK;
            return pf_error_from_errno(errno);
        }
        if (n == 0)
            return PF_OK;
        while (at < u.buf + n) {
            struct inotify_event ev;
            memcpy(&ev, at, sizeof(ev));
            if (ev.mask & IN_Q_OVERFLOW) {
                pf__watch_overflow(w, NULL);
            } else if (ev.wd >= 0) {
                pf__watch_dirty_bh(w, ev.wd);
                if (ev.mask & IN_IGNORED) {
                    /* The kernel dropped it (the directory is gone). */
                    size_t i;
                    for (i = 0; i < w->ndirs; i++)
                        if (w->dirs[i]->bh == ev.wd)
                            w->dirs[i]->bh = -1;
                }
            }
            at += sizeof(struct inotify_event) + ev.len;
        }
    }
}

static intptr_t pf__in_handle(pf__watcher *w) { return pf__in_fd(w); }

static const pf__wbackend pf__in_backend = {
    pf__in_init, pf__in_free, pf__in_dir_add, pf__in_dir_remove,
    NULL,        NULL,        pf__in_collect, pf__in_handle,
};

const pf__wbackend *const pf__watch_native = &pf__in_backend;

#elif PF_OS_APPLE || PF_OS_BSD

    #include <sys/event.h>
    #include <sys/time.h>
    #include <sys/types.h>

    #ifdef O_EVTONLY
        #define PF__OPEN_EVENTS O_EVTONLY
    #else
        #define PF__OPEN_EVENTS O_RDONLY
    #endif

    #if PF_OS_NETBSD && defined(__NetBSD_Version__) \
        && __NetBSD_Version__ < 1000000000
        #define PF__UDATA(id) ((intptr_t)(id))
        #define PF__UDATA_ID(u) ((int)(u))
    #else
        #define PF__UDATA(id) ((void *)(intptr_t)(id))
        #define PF__UDATA_ID(u) ((int)(intptr_t)(u))
    #endif

    #define PF__KQ_NOTES                                                    \
        (NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_EXTEND | NOTE_ATTRIB \
         | NOTE_LINK | NOTE_REVOKE)

static int pf__kqw_fd(pf__watcher *w) { return (int)(intptr_t)w->bstate; }

static int pf__kqw_init(pf__watcher *w) {
    int kq = kqueue();
    if (kq < 0)
        return pf_error_from_errno(errno);
    fcntl(kq, F_SETFD, FD_CLOEXEC);
    w->bstate = (void *)(intptr_t)kq;
    return PF_OK;
}

static void pf__kqw_free(pf__watcher *w) { close(pf__kqw_fd(w)); }

/* Opens `path` and watches it, reporting to directory `node`: the
   descriptor, or a PF_E* code. */
static int pf__kqw_open(pf__watcher *w, const char *path, int flags, int node) {
    struct kevent kev;
    int fd = open(path, PF__OPEN_EVENTS | O_CLOEXEC | flags);
    if (fd < 0)
        return pf_error_from_errno(errno);
    EV_SET(
        &kev,
        (uintptr_t)fd,
        EVFILT_VNODE,
        EV_ADD | EV_CLEAR,
        PF__KQ_NOTES,
        0,
        PF__UDATA(node)
    );
    if (kevent(pf__kqw_fd(w), &kev, 1, NULL, 0, NULL) != 0) {
        int rc = pf_error_from_errno(errno);
        close(fd);
        return rc;
    }
    return fd;
}

static int pf__kqw_dir_add(pf__watcher *w, pf__watch *wt, pf__wdir *d) {
    size_t len;
    char *path = pf__watch_path(w, wt, d, NULL, &len);
    int fd;
    if (!path)
        return PF_ENOMEM;
    fd = pf__kqw_open(
        w, path, O_DIRECTORY | (d->rel_len ? O_NOFOLLOW : 0), d->node
    );
    pf__watch_path_free(w, path, len);
    if (fd < 0)
        return fd;
    d->bh = fd;
    return PF_OK;
}

static void pf__kqw_dir_remove(pf__watcher *w, pf__wdir *d) {
    (void)w;
    close((int)d->bh);
}

static void pf__kqw_file_add(
    pf__watcher *w, pf__watch *wt, pf__wdir *d, pf__wentry *e
) {
    size_t len;
    char *path = pf__watch_path(w, wt, d, e->name, &len);
    int fd;
    if (!path)
        return;
    fd = pf__kqw_open(w, path, O_NOFOLLOW | O_NONBLOCK, d->node);
    pf__watch_path_free(w, path, len);
    if (fd >= 0)
        e->bh = fd;
}

static void pf__kqw_file_remove(pf__watcher *w, pf__wentry *e) {
    (void)w;
    close((int)e->bh);
}

static int pf__kqw_collect(pf__watcher *w, int64_t timeout_ns) {
    struct kevent evs[64];
    struct timespec ts, *tsp = NULL;
    int i, n;

    if (timeout_ns >= 0) {
        ts.tv_sec = (time_t)(timeout_ns / 1000000000);
        ts.tv_nsec = (long)(timeout_ns % 1000000000);
        tsp = &ts;
    }
    n = kevent(pf__kqw_fd(w), NULL, 0, evs, 64, tsp);
    if (n < 0)
        return errno == EINTR ? PF_OK : pf_error_from_errno(errno);
    for (i = 0; i < n; i++) {
        pf__wdir *d = pf__watch_dir_by_node(w, PF__UDATA_ID(evs[i].udata));
        if (d)
            d->dirty = true;
    }
    return PF_OK;
}

static intptr_t pf__kqw_handle(pf__watcher *w) { return pf__kqw_fd(w); }

static const pf__wbackend pf__kqw_backend = {
    pf__kqw_init,     pf__kqw_free,        pf__kqw_dir_add, pf__kqw_dir_remove,
    pf__kqw_file_add, pf__kqw_file_remove, pf__kqw_collect, pf__kqw_handle,
};

const pf__wbackend *const pf__watch_native = &pf__kqw_backend;

#else

const pf__wbackend *const pf__watch_native = NULL;

#endif
