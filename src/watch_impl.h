/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    File watching: the engine (watch.c) and its backends (posix/watch.c:
    inotify, kqueue; win32/watch.c: ReadDirectoryChangesW; watch.c:
    scanning).

    The engine keeps a snapshot of every watched directory: its entries
    with their file numbers, sizes and time stamps. A backend only says
    which directories may have changed (pf__wdir.dirty); the engine scans
    those again and turns the differences into events. Renames are found
    by file number. So every backend reports the same way, and the
    engine's logic is tested on every system with the scanning backend.
*/

#ifndef CPOLYFILL_WATCH_IMPL_H
#define CPOLYFILL_WATCH_IMPL_H

#include <cpolyfill/watch.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct pf__wentry {
    char *name;
    size_t len;
    uint64_t dev;
    uint64_t ino;
    uint64_t size;
    int64_t mtime;
    int64_t ctime;
    uint32_t perm;
    bool dir;
    bool reg;    /* a regular file */
    intptr_t bh; /* the backend's handle (kqueue: a descriptor), -1 */
} pf__wentry;

typedef struct pf__wdir {
    int node;  /* unique, for the backends */
    int watch; /* the watch's id */
    char *rel; /* relative to the watch's root ("" for the root),
                     with the system's separator */
    size_t rel_len;
    pf__wentry *ents; /* sorted by name */
    size_t n;
    size_t cap;
    intptr_t bh; /* the backend's handle (inotify: a watch descriptor;
                     kqueue: a descriptor; Windows: its state), -1 */
    bool dirty;
    bool dead;
} pf__wdir;

typedef struct pf__watch {
    int id;
    char *root; /* the directory scanned (a file's directory) */
    size_t root_len;
    char *filter; /* a watched file's name, NULL for a directory */
    size_t filter_len;
    bool recursive;
    bool gone;
    bool overflow; /* to report */
} pf__watch;

typedef struct pf__wevent {
    int id;
    int kind;
    char *path;
    size_t path_len;
    char *old;
    size_t old_len;
    bool dir;
} pf__wevent;

typedef struct pf__watcher pf__watcher;

typedef struct pf__wbackend {
    int (*init)(pf__watcher *w);
    void (*free)(pf__watcher *w);
    /* Starts watching a directory, before it is scanned. */
    int (*dir_add)(pf__watcher *w, pf__watch *wt, pf__wdir *d);
    void (*dir_remove)(pf__watcher *w, pf__wdir *d);
    /* Files, for backends that watch each (may be NULL). */
    void (*file_add)(pf__watcher *w, pf__watch *wt, pf__wdir *d, pf__wentry *e);
    void (*file_remove)(pf__watcher *w, pf__wentry *e);
    /* Waits up to `timeout_ns` (negative: forever) for the system to
       report something, and marks the directories it names dirty (or
       calls pf__watch_overflow). 0 or a PF_E* code. */
    int (*collect)(pf__watcher *w, int64_t timeout_ns);
    /* A descriptor or HANDLE for the poller, -1 if there is none. */
    intptr_t (*handle)(pf__watcher *w);
} pf__wbackend;

struct pf__watcher {
    struct allocator_t *alloc;
    const pf__wbackend *be;
    void *bstate;
    int64_t interval; /* the scanning backend's */
    int64_t next_scan;
    pf__watch **watches;
    size_t nwatches;
    size_t wcap;
    int next_id;
    pf__wdir **dirs;
    size_t ndirs;
    size_t dcap;
    int next_node;
    /* Events to give, from `qhead`. */
    pf__wevent *queue;
    size_t qhead;
    size_t qn;
    size_t qcap;
    /* Events given by the last read, freed by the next. */
    pf__wevent *given;
    size_t ngiven;
    size_t gcap;
};

/* For the backends. */
pf__watch *pf__watch_find(pf__watcher *w, int id);
pf__wdir *pf__watch_dir_by_node(pf__watcher *w, int node);
pf__wdir *pf__watch_dir_by_rel(
    pf__watcher *w, int watch, const char *rel, size_t len
);
/* Marks every directory with backend handle `bh` dirty. */
void pf__watch_dirty_bh(pf__watcher *w, intptr_t bh);
/* Events were lost for `wt` (NULL: every watch): scan it all again and
   report PF_WATCH_OVERFLOW. */
void pf__watch_overflow(pf__watcher *w, pf__watch *wt);
/* The full path of a directory or of one of its entries (`name` may be
   NULL), in new memory from the watcher's allocator; release with
   pf__watch_path_free. */
char *pf__watch_path(
    pf__watcher *w,
    const pf__watch *wt,
    const pf__wdir *d,
    const char *name,
    size_t *len
);
void pf__watch_path_free(pf__watcher *w, char *path, size_t len);

enum { PF__WATCH_DEFAULT, PF__WATCH_SCAN };

/* The backends. NULL where there is none. */
extern const pf__wbackend *const pf__watch_native;
extern const pf__wbackend *const pf__watch_scan;

/* pf_watcher_init with a chosen backend, and the scanning interval. */
int pf__watcher_init_with(
    pf_watcher_t *w, struct allocator_t *alloc, int backend, int64_t interval_ns
);

#endif /* CPOLYFILL_WATCH_IMPL_H */
