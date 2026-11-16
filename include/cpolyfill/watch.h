/** cpolyfill - Filling the gaps between C standards and compilers

    Watching files and directories for changes: inotify (Linux), kqueue
    (macOS, the BSDs), ReadDirectoryChangesW (Windows), or scanning every
    half second elsewhere.

    The systems report changes differently (some merge them, some lose
    them under load, some only say that a directory changed), so the
    library keeps a snapshot of what it watches and reports the
    differences it finds when the system says something changed. An
    event therefore says "this path changed, like this", not "this system
    call happened": a file written ten times between two reads may be
    reported once, a file created and removed meanwhile not at all.

        pf_watcher_t            A set of watched paths. One thread uses it.
        int pf_watcher_init(pf_watcher_t *w, struct allocator_t *alloc)
                Memory comes from `alloc` (NULL: the library's default).
                0 or a negative PF_E* code.
        int pf_watcher_free(pf_watcher_t *w)

        int pf_watcher_add(pf_watcher_t *w, const char *path,
                           unsigned flags)
                Watches a directory (the entries in it), or a file. A file
                is watched by its name in its directory, so it is still
                watched after an editor replaces it, and its creation is
                seen after it was removed. Returns the watch's id (> 0),
                or PF_ENOENT, PF_ENOTDIR (PF_WATCH_RECURSIVE on a file),
                or another PF_E* code.
                flags: PF_WATCH_RECURSIVE, the whole tree below the
                       directory; directories created later are watched
                       too, and their contents reported as created.
        int pf_watcher_remove(pf_watcher_t *w, int id)

    Events:

        pf_watch_event_t        id        the watch
                                kind      PF_WATCH_CREATED,
                                          PF_WATCH_REMOVED,
                                          PF_WATCH_MODIFIED (contents,
                                          size, time stamps or
                                          permissions; for a file that was
                                          replaced too),
                                          PF_WATCH_RENAMED,
                                          PF_WATCH_OVERFLOW (events were
                                          lost; the watch is consistent
                                          again, but rescan what you keep),
                                          PF_WATCH_GONE (the watched
                                          directory itself, or the
                                          directory of a watched file, was
                                          removed: the watch stays, quiet,
                                          until removed)
                                path      what changed, relative to the
                                          watched directory (for a watched
                                          file, its name); "" for OVERFLOW
                                          and GONE. Uses the system's
                                          separator.
                                old_path  RENAMED: the old name, else
                                          NULL. A rename the library cannot
                                          pair (out of or into the watched
                                          tree) comes as REMOVED or
                                          CREATED.
                                dir       whether `path` is a directory
                                The paths stay valid until the next read
                                or until the watcher is freed.

        int pf_watcher_read(pf_watcher_t *w, pf_watch_event_t *ev,
                            size_t n, const struct timespec *deadline)
                Waits for changes and stores up to `n` events: their
                count, 0 once the absolute UTC `deadline` (NULL: none)
                passes, or a PF_E* code. Events that did not fit are kept
                for the next read.
        int pf_watcher_read_ns(pf_watcher_t *w, pf_watch_event_t *ev,
                               size_t n, int64_t timeout_ns)
                The same with a timeout on the monotonic clock: 0 does not
                wait, a negative timeout waits forever.

        int pf_poller_add_watcher(pf_poller_t *p, pf_watcher_t *w,
                                  void *data)
                Registers the watcher with a poller (<cpolyfill/poller.h>):
                PF_POLL_IN when a change may be waiting. Then read with a
                timeout of 0, until a read returns fewer than `n` events.
                PF_ENOSYS where the watcher scans (it has nothing to wait
                on).

    Limits: inotify has a per-user limit on watched directories
    (/proc/sys/fs/inotify/max_user_watches); kqueue needs a descriptor for
    every watched directory and file, so large trees can run into the
    limit on open files. Changes are found by comparing sizes and time
    stamps, so a write that changes neither may be missed.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_WATCH_H
#define CPOLYFILL_WATCH_H

#include <cpolyfill/platform.h>
#include <cpolyfill/poller.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF_WATCH_RECURSIVE 0x1u

#define PF_WATCH_CREATED 1
#define PF_WATCH_REMOVED 2
#define PF_WATCH_MODIFIED 3
#define PF_WATCH_RENAMED 4
#define PF_WATCH_OVERFLOW 5
#define PF_WATCH_GONE 6

struct allocator_t;

typedef struct pf_watcher_t {
    /* Private. */
    void *pf__impl;
} pf_watcher_t;

typedef struct pf_watch_event_t {
    int id;
    int kind;
    const char *path;
    const char *old_path;
    bool dir;
} pf_watch_event_t;

PF_BEGIN_DECLS

PF_API int pf_watcher_init(pf_watcher_t *w, struct allocator_t *alloc);
PF_API int pf_watcher_free(pf_watcher_t *w);
PF_API int pf_watcher_add(pf_watcher_t *w, const char *path, unsigned flags);
PF_API int pf_watcher_remove(pf_watcher_t *w, int id);
PF_API int pf_watcher_read(
    pf_watcher_t *w,
    pf_watch_event_t *ev,
    size_t n,
    const struct timespec *deadline
);
PF_API int pf_watcher_read_ns(
    pf_watcher_t *w, pf_watch_event_t *ev, size_t n, int64_t timeout_ns
);
PF_API int pf_poller_add_watcher(pf_poller_t *p, pf_watcher_t *w, void *data);

PF_END_DECLS

#endif /* CPOLYFILL_WATCH_H */
