/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    File watching: the engine that keeps the snapshots and turns their
    differences into events, and the scanning backend. See watch_impl.h.
*/

#include "internal.h"

#include "poller_impl.h"
#include "watch_impl.h"

#include <cpolyfill/dirent.h>
#include <cpolyfill/fs.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static pf__watcher *pf__impl(pf_watcher_t *w) {
    return w ? (pf__watcher *)w->pf__impl : NULL;
}

/* ---- Helpers ----------------------------------------------------------- */

static char *pf__wdup(pf__watcher *w, const char *s, size_t len) {
    char *d = (char *)allocate(w->alloc, len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = 0;
    }
    return d;
}

static void pf__wstr_drop(pf__watcher *w, char *s, size_t len) {
    if (s)
        deallocate(w->alloc, s, len + 1);
}

/* Makes room for one more element in `*arr`. */
static int pf__grow(
    pf__watcher *w, void **arr, size_t n, size_t *cap, size_t elem
) {
    size_t ncap;
    void *na;
    if (n < *cap)
        return PF_OK;
    ncap = *cap ? *cap * 2 : 8;
    na = reallocate(w->alloc, *arr, *cap * elem, ncap * elem);
    if (!na)
        return PF_ENOMEM;
    *arr = na;
    *cap = ncap;
    return PF_OK;
}

/* `a` and `b` joined with the separator ("" + b is b). */
static char *pf__wjoin(
    pf__watcher *w, const char *a, size_t al, const char *b, size_t bl,
    size_t *len
) {
    bool sep = al > 0 && bl > 0 && !pf_path_is_sep((unsigned char)a[al - 1]);
    size_t n = al + (sep ? 1 : 0) + bl;
    char *s = (char *)allocate(w->alloc, n + 1);
    if (!s)
        return NULL;
    memcpy(s, a, al);
    if (sep)
        s[al] = PF_PATH_SEP;
    memcpy(s + al + (sep ? 1 : 0), b, bl);
    s[n] = 0;
    *len = n;
    return s;
}

char *pf__watch_path(
    pf__watcher *w, const pf__watch *wt, const pf__wdir *d,
    const char *name, size_t *len
) {
    size_t dl;
    char *dp = pf__wjoin(w, wt->root, wt->root_len, d->rel, d->rel_len, &dl);
    char *full;
    if (!dp || !name) {
        *len = dl;
        return dp;
    }
    full = pf__wjoin(w, dp, dl, name, strlen(name), len);
    pf__wstr_drop(w, dp, dl);
    return full;
}

void pf__watch_path_free(pf__watcher *w, char *path, size_t len) {
    pf__wstr_drop(w, path, len);
}

pf__watch *pf__watch_find(pf__watcher *w, int id) {
    size_t i;
    for (i = 0; i < w->nwatches; i++)
        if (w->watches[i]->id == id)
            return w->watches[i];
    return NULL;
}

pf__wdir *pf__watch_dir_by_node(pf__watcher *w, int node) {
    size_t i;
    for (i = 0; i < w->ndirs; i++)
        if (!w->dirs[i]->dead && w->dirs[i]->node == node)
            return w->dirs[i];
    return NULL;
}

pf__wdir *pf__watch_dir_by_rel(
    pf__watcher *w, int watch, const char *rel, size_t len
) {
    size_t i;
    for (i = 0; i < w->ndirs; i++) {
        pf__wdir *d = w->dirs[i];
        if (!d->dead && d->watch == watch && d->rel_len == len
            && memcmp(d->rel, rel, len) == 0)
            return d;
    }
    return NULL;
}

void pf__watch_dirty_bh(pf__watcher *w, intptr_t bh) {
    size_t i;
    for (i = 0; i < w->ndirs; i++)
        if (!w->dirs[i]->dead && w->dirs[i]->bh == bh)
            w->dirs[i]->dirty = true;
}

void pf__watch_overflow(pf__watcher *w, pf__watch *wt) {
    size_t i;
    for (i = 0; i < w->nwatches; i++)
        if (!wt || w->watches[i] == wt)
            w->watches[i]->overflow = true;
    for (i = 0; i < w->ndirs; i++)
        if (!wt || w->dirs[i]->watch == wt->id)
            w->dirs[i]->dirty = true;
}

/* ---- Events ------------------------------------------------------------ */

static void pf__event_drop(pf__watcher *w, pf__wevent *e) {
    pf__wstr_drop(w, e->path, e->path_len);
    pf__wstr_drop(w, e->old, e->old_len);
}

static int pf__queue(
    pf__watcher *w, int id, int kind, const char *path, size_t len,
    const char *old, size_t old_len, bool dir
) {
    pf__wevent *e;
    int rc = pf__grow(w, (void **)&w->queue, w->qn, &w->qcap, sizeof(*e));
    if (rc)
        return rc;
    e = &w->queue[w->qn];
    memset(e, 0, sizeof(*e));
    e->id = id;
    e->kind = kind;
    e->dir = dir;
    e->path = pf__wdup(w, path, len);
    e->path_len = len;
    if (old) {
        e->old = pf__wdup(w, old, old_len);
        e->old_len = old_len;
    }
    if (!e->path || (old && !e->old)) {
        pf__event_drop(w, e);
        return PF_ENOMEM;
    }
    w->qn++;
    return PF_OK;
}

static void pf__given_free(pf__watcher *w) {
    size_t i;
    for (i = 0; i < w->ngiven; i++)
        pf__event_drop(w, &w->given[i]);
    w->ngiven = 0;
}

/* ---- Snapshots --------------------------------------------------------- */

static void pf__entries_free(pf__watcher *w, pf__wentry *e, size_t n, size_t cap) {
    size_t i;
    for (i = 0; i < n; i++)
        pf__wstr_drop(w, e[i].name, e[i].len);
    deallocate(w->alloc, e, cap * sizeof(*e));
}

static int pf__entry_cmp(const void *a, const void *b) {
    return strcmp(((const pf__wentry *)a)->name, ((const pf__wentry *)b)->name);
}

/* Reads directory `d` into a new sorted list: 0, or a PF_E* code if it
   cannot be read. */
static int pf__scan(
    pf__watcher *w, pf__watch *wt, pf__wdir *d, pf__wentry **out,
    size_t *nout, size_t *cout
) {
    pf__wentry *ents = NULL;
    size_t n = 0, cap = 0, dl, bufsize;
    char *dpath = pf__watch_path(w, wt, d, NULL, &dl);
    char *buf;
    pf_dir_t *dir;
    struct pf_dirent *de;
    int rc = PF_OK;
    bool filtered = wt->filter && d->rel_len == 0;

    if (!dpath)
        return PF_ENOMEM;
    dir = pf_opendir(dpath);
    if (!dir) {
        rc = pf_error_from_errno(errno);
        pf__wstr_drop(w, dpath, dl);
        return rc;
    }
    bufsize = dl + 2 + PF_NAME_MAX + 1;
    buf = (char *)allocate(w->alloc, bufsize);
    if (!buf) {
        pf_closedir(dir);
        pf__wstr_drop(w, dpath, dl);
        return PF_ENOMEM;
    }
    while ((de = pf_readdir(dir)) != NULL) {
        const char *name = de->d_name;
        size_t len = strlen(name), k = dl;
        pf_stat_t st;
        pf__wentry *e;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (filtered
            && (len != wt->filter_len || memcmp(name, wt->filter, len) != 0))
            continue;
        memcpy(buf, dpath, dl);
        if (dl > 0 && !pf_path_is_sep((unsigned char)buf[dl - 1]))
            buf[k++] = PF_PATH_SEP;
        memcpy(buf + k, name, len + 1);
        if (pf_lstat(buf, &st) != 0)
            continue; /* gone meanwhile */
        rc = pf__grow(w, (void **)&ents, n, &cap, sizeof(*ents));
        if (rc)
            break;
        e = &ents[n];
        e->name = pf__wdup(w, name, len);
        if (!e->name) {
            rc = PF_ENOMEM;
            break;
        }
        n++;
        e->len = len;
        e->dev = st.dev;
        e->ino = st.ino;
        e->size = st.size;
        e->mtime = pf_timespec_to_ns(st.mtime);
        e->ctime = pf_timespec_to_ns(st.ctime);
        e->perm = st.perm;
        e->dir = st.type == PF_FILE_DIR;
        e->reg = st.type == PF_FILE_REGULAR;
        e->bh = -1;
    }
    pf_closedir(dir);
    deallocate(w->alloc, buf, bufsize);
    pf__wstr_drop(w, dpath, dl);
    if (rc) {
        pf__entries_free(w, ents, n, cap);
        return rc;
    }
    if (n > 1)
        qsort(ents, n, sizeof(*ents), pf__entry_cmp);
    *out = ents;
    *nout = n;
    *cout = cap;
    return PF_OK;
}

/* Stops watching a directory and forgets its entries. */
static void pf__dir_unwatch(pf__watcher *w, pf__wdir *d) {
    size_t i;
    if (d->dead)
        return;
    if (w->be->file_remove)
        for (i = 0; i < d->n; i++)
            if (d->ents[i].bh != -1) {
                w->be->file_remove(w, &d->ents[i]);
                d->ents[i].bh = -1;
            }
    if (d->bh != -1)
        w->be->dir_remove(w, d);
    d->bh = -1;
    d->dead = true;
}

/* Frees the directories that died. */
static void pf__dirs_compact(pf__watcher *w) {
    size_t i, j = 0;
    for (i = 0; i < w->ndirs; i++) {
        pf__wdir *d = w->dirs[i];
        if (d->dead) {
            pf__entries_free(w, d->ents, d->n, d->cap);
            pf__wstr_drop(w, d->rel, d->rel_len);
            deallocate(w->alloc, d, sizeof(*d));
        } else {
            w->dirs[j++] = d;
        }
    }
    w->ndirs = j;
}

/* Whether `rel` is `prefix` or below it ("" is above everything). */
static bool pf__below(
    const char *rel, size_t rl, const char *prefix, size_t pl
) {
    if (pl == 0)
        return true;
    if (rl < pl || memcmp(rel, prefix, pl) != 0)
        return false;
    return rl == pl || rel[pl] == PF_PATH_SEP;
}

/* Stops watching the directory `rel` of `wt` and everything below. */
static void pf__tree_remove(
    pf__watcher *w, pf__watch *wt, const char *rel, size_t len
) {
    size_t i;
    for (i = 0; i < w->ndirs; i++) {
        pf__wdir *d = w->dirs[i];
        if (!d->dead && d->watch == wt->id
            && pf__below(d->rel, d->rel_len, rel, len))
            pf__dir_unwatch(w, d);
    }
}

/* Starts watching the directory `rel` of `wt` (and below it, for a
   recursive watch), reporting what is in it as created if `report`. A
   directory that cannot be read is skipped. 0 or PF_ENOMEM (or, for the
   root, why it cannot be read). */
static int pf__tree_add(
    pf__watcher *w, pf__watch *wt, const char *rel, size_t len, bool report
) {
    pf__wdir *d;
    size_t i;
    int rc;

    if (pf__watch_dir_by_rel(w, wt->id, rel, len))
        return PF_OK;
    rc = pf__grow(w, (void **)&w->dirs, w->ndirs, &w->dcap, sizeof(d));
    if (rc)
        return rc;
    d = (pf__wdir *)zallocate(w->alloc, sizeof(*d));
    if (!d)
        return PF_ENOMEM;
    d->rel = pf__wdup(w, rel, len);
    if (!d->rel) {
        deallocate(w->alloc, d, sizeof(*d));
        return PF_ENOMEM;
    }
    d->rel_len = len;
    d->watch = wt->id;
    d->bh = -1;
    if (++w->next_node <= 0)
        w->next_node = 1;
    d->node = w->next_node;
    w->dirs[w->ndirs++] = d;
    /* Watch first, then scan: a change in between is reported by the
       system, and found by the next scan. Without the system's help
       (too many watches), the directory is still scanned when its
       parent changes. */
    w->be->dir_add(w, wt, d);
    rc = pf__scan(w, wt, d, &d->ents, &d->n, &d->cap);
    if (rc) {
        pf__dir_unwatch(w, d);
        return rc == PF_ENOMEM || len == 0 ? rc : PF_OK;
    }
    for (i = 0; i < d->n; i++) {
        pf__wentry *e = &d->ents[i];
        size_t pl;
        char *path;
        if (e->reg && w->be->file_add)
            w->be->file_add(w, wt, d, e);
        if (!report && !(e->dir && wt->recursive))
            continue;
        path = pf__wjoin(w, rel, len, e->name, e->len, &pl);
        if (!path)
            return PF_ENOMEM;
        if (report)
            rc = pf__queue(w, wt->id, PF_WATCH_CREATED, path, pl, NULL, 0, e->dir);
        if (!rc && e->dir && wt->recursive)
            rc = pf__tree_add(w, wt, path, pl, report);
        pf__wstr_drop(w, path, pl);
        if (rc)
            return rc;
        /* `d->ents` is stable, but `w->dirs` may have moved. */
    }
    return PF_OK;
}

/* ---- Finding the changes ----------------------------------------------- */

typedef struct pf__witem {
    int watch;
    char *path;
    size_t len;
    uint64_t dev;
    uint64_t ino;
    bool dir;
    int pair;  /* a rename: the index in the other list, -1 */
    bool done; /* reported with the same path's other item */
} pf__witem;

typedef struct pf__wbatch {
    pf__witem *gone;
    size_t ng, cg;
    pf__witem *news;
    size_t nn, cn;
    pf__witem *mods;
    size_t nm, cm;
} pf__wbatch;

static int pf__item(
    pf__watcher *w, pf__witem **arr, size_t *n, size_t *cap, int watch,
    const pf__wdir *d, const pf__wentry *e
) {
    pf__witem *it;
    int rc = pf__grow(w, (void **)arr, *n, cap, sizeof(**arr));
    if (rc)
        return rc;
    it = &(*arr)[*n];
    it->path = pf__wjoin(w, d->rel, d->rel_len, e->name, e->len, &it->len);
    if (!it->path)
        return PF_ENOMEM;
    it->watch = watch;
    it->dev = e->dev;
    it->ino = e->ino;
    it->dir = e->dir;
    it->pair = -1;
    it->done = false;
    (*n)++;
    return PF_OK;
}

static void pf__batch_free(pf__watcher *w, pf__wbatch *b) {
    size_t i;
    for (i = 0; i < b->ng; i++)
        pf__wstr_drop(w, b->gone[i].path, b->gone[i].len);
    for (i = 0; i < b->nn; i++)
        pf__wstr_drop(w, b->news[i].path, b->news[i].len);
    for (i = 0; i < b->nm; i++)
        pf__wstr_drop(w, b->mods[i].path, b->mods[i].len);
    deallocate(w->alloc, b->gone, b->cg * sizeof(pf__witem));
    deallocate(w->alloc, b->news, b->cn * sizeof(pf__witem));
    deallocate(w->alloc, b->mods, b->cm * sizeof(pf__witem));
}

static bool pf__entry_changed(const pf__wentry *a, const pf__wentry *b) {
    return a->size != b->size || a->mtime != b->mtime || a->ctime != b->ctime
        || a->perm != b->perm;
}

/* Scans `d` again and records the differences in `b`. */
static int pf__rescan(pf__watcher *w, pf__watch *wt, pf__wdir *d, pf__wbatch *b) {
    pf__wentry *ne;
    size_t nn, nc, i = 0, j = 0;
    int rc = pf__scan(w, wt, d, &ne, &nn, &nc);
    if (rc)
        return rc;
    while (rc == PF_OK && (i < d->n || j < nn)) {
        pf__wentry *o = i < d->n ? &d->ents[i] : NULL;
        pf__wentry *e = j < nn ? &ne[j] : NULL;
        int c = !o ? 1 : !e ? -1 : strcmp(o->name, e->name);
        if (c < 0 || (c == 0
                      && (o->ino != e->ino || o->dev != e->dev
                          || o->dir != e->dir))) {
            rc = pf__item(w, &b->gone, &b->ng, &b->cg, wt->id, d, o);
            if (o->bh != -1) {
                w->be->file_remove(w, o);
                o->bh = -1;
            }
        }
        if (c > 0 || (c == 0
                      && (o->ino != e->ino || o->dev != e->dev
                          || o->dir != e->dir))) {
            if (!rc)
                rc = pf__item(w, &b->news, &b->nn, &b->cn, wt->id, d, e);
            if (e->reg && w->be->file_add)
                w->be->file_add(w, wt, d, e);
        } else if (c == 0) {
            e->bh = o->bh;
            o->bh = -1;
            if (!e->dir && pf__entry_changed(o, e))
                rc = pf__item(w, &b->mods, &b->nm, &b->cm, wt->id, d, e);
        }
        if (c <= 0)
            i++;
        if (c >= 0)
            j++;
    }
    /* On failure the new list is kept anyway: it is what is there. */
    pf__entries_free(w, d->ents, d->n, d->cap);
    d->ents = ne;
    d->n = nn;
    d->cap = nc;
    return rc;
}

static void pf__gone(pf__watcher *w, pf__watch *wt) {
    pf__tree_remove(w, wt, "", 0);
    wt->gone = true;
    pf__queue(w, wt->id, PF_WATCH_GONE, "", 0, NULL, 0, wt->filter == NULL);
}

/* Turns the differences into events, and follows directories that came
   and went in recursive watches. */
static int pf__report(pf__watcher *w, pf__wbatch *b) {
    size_t i, k;
    int rc = PF_OK;

    /* Renames: an entry that left and one that came with its number. */
    for (i = 0; i < b->ng; i++) {
        pf__witem *g = &b->gone[i];
        pf__watch *wt = pf__watch_find(w, g->watch);
        if (!wt || wt->filter || g->ino == 0)
            continue;
        for (k = 0; k < b->nn; k++) {
            pf__witem *n = &b->news[k];
            if (n->pair < 0 && n->watch == g->watch && n->ino == g->ino
                && n->dev == g->dev && n->dir == g->dir
                && (n->len != g->len || memcmp(n->path, g->path, g->len))) {
                g->pair = (int)k;
                n->pair = (int)i;
                break;
            }
        }
    }

    /* What left: removed, or replaced (the same path came back). */
    for (i = 0; i < b->ng && !rc; i++) {
        pf__witem *g = &b->gone[i];
        pf__watch *wt = pf__watch_find(w, g->watch);
        pf__witem *same = NULL;
        if (!wt || g->pair >= 0)
            continue;
        for (k = 0; k < b->nn; k++) {
            pf__witem *n = &b->news[k];
            if (n->pair < 0 && !n->done && n->watch == g->watch
                && n->len == g->len && memcmp(n->path, g->path, g->len) == 0) {
                same = n;
                break;
            }
        }
        if (same) {
            same->done = true;
            rc = pf__queue(
                w, wt->id, PF_WATCH_MODIFIED, g->path, g->len, NULL, 0,
                same->dir
            );
        } else {
            rc = pf__queue(
                w, wt->id, PF_WATCH_REMOVED, g->path, g->len, NULL, 0, g->dir
            );
        }
        if (g->dir && wt->recursive)
            pf__tree_remove(w, wt, g->path, g->len);
    }

    /* Renames, and the old trees of renamed directories. */
    for (i = 0; i < b->nn && !rc; i++) {
        pf__witem *n = &b->news[i];
        pf__witem *g;
        pf__watch *wt = pf__watch_find(w, n->watch);
        if (!wt || n->pair < 0)
            continue;
        g = &b->gone[n->pair];
        rc = pf__queue(
            w, wt->id, PF_WATCH_RENAMED, n->path, n->len, g->path, g->len,
            n->dir
        );
        if (g->dir && wt->recursive)
            pf__tree_remove(w, wt, g->path, g->len);
    }

    /* What came, and the new trees. */
    for (i = 0; i < b->nn && !rc; i++) {
        pf__witem *n = &b->news[i];
        pf__watch *wt = pf__watch_find(w, n->watch);
        bool report = n->pair < 0;
        if (!wt)
            continue;
        if (report && !n->done)
            rc = pf__queue(
                w, wt->id, PF_WATCH_CREATED, n->path, n->len, NULL, 0, n->dir
            );
        if (!rc && n->dir && wt->recursive)
            rc = pf__tree_add(w, wt, n->path, n->len, report);
    }

    for (i = 0; i < b->nm && !rc; i++) {
        pf__witem *m = &b->mods[i];
        rc = pf__queue(
            w, m->watch, PF_WATCH_MODIFIED, m->path, m->len, NULL, 0, false
        );
    }
    return rc;
}

/* Scans the dirty directories and queues what changed. */
static int pf__process(pf__watcher *w) {
    int round, rc = PF_OK;

    for (round = 0; round < 8 && !rc; round++) {
        pf__wbatch b;
        bool any = false;
        size_t i;

        for (i = 0; i < w->nwatches && !rc; i++) {
            pf__watch *wt = w->watches[i];
            if (wt->overflow && !wt->gone)
                rc = pf__queue(
                    w, wt->id, PF_WATCH_OVERFLOW, "", 0, NULL, 0, false
                );
            wt->overflow = false;
        }
        memset(&b, 0, sizeof(b));
        for (i = 0; i < w->ndirs && !rc; i++) {
            pf__wdir *d = w->dirs[i];
            pf__watch *wt;
            int src;
            if (d->dead || !d->dirty)
                continue;
            d->dirty = false;
            any = true;
            wt = pf__watch_find(w, d->watch);
            if (!wt || wt->gone)
                continue;
            src = pf__rescan(w, wt, d, &b);
            if (src == PF_ENOMEM) {
                rc = src;
            } else if (src && d->rel_len == 0) {
                if (src == PF_ENOENT || src == PF_ENOTDIR)
                    pf__gone(w, wt);
            } else if (src) {
                /* Gone meanwhile: its parent says so. */
                const char *rel = d->rel;
                size_t pl = d->rel_len;
                pf__wdir *parent;
                while (pl > 0 && rel[pl - 1] != PF_PATH_SEP)
                    pl--;
                parent = pf__watch_dir_by_rel(
                    w, wt->id, rel, pl > 0 ? pl - 1 : 0
                );
                if (parent)
                    parent->dirty = true;
            }
        }
        if (!rc && any)
            rc = pf__report(w, &b);
        pf__batch_free(w, &b);
        if (!any)
            break;
        for (i = 0; i < w->ndirs; i++)
            if (!w->dirs[i]->dead && w->dirs[i]->dirty)
                break;
        if (i == w->ndirs)
            break;
    }
    pf__dirs_compact(w);
    return rc;
}

/* ---- The scanning backend ---------------------------------------------- */

static int pf__scan_init(pf__watcher *w) {
    (void)w;
    return PF_OK;
}

static void pf__scan_free(pf__watcher *w) { (void)w; }

static int pf__scan_dir_add(pf__watcher *w, pf__watch *wt, pf__wdir *d) {
    (void)w, (void)wt, (void)d;
    return PF_OK;
}

static void pf__scan_dir_remove(pf__watcher *w, pf__wdir *d) {
    (void)w, (void)d;
}

static int pf__scan_collect(pf__watcher *w, int64_t timeout_ns) {
    int64_t now = (int64_t)pf_time_ns();
    size_t i;

    if (w->next_scan == 0)
        w->next_scan = now + w->interval;
    if (now < w->next_scan) {
        int64_t wait = w->next_scan - now;
        if (timeout_ns >= 0 && timeout_ns < wait) {
            if (timeout_ns > 0)
                pf_sleep_ns((uint64_t)timeout_ns);
            return PF_OK;
        }
        pf_sleep_ns((uint64_t)wait);
    }
    for (i = 0; i < w->ndirs; i++)
        w->dirs[i]->dirty = true;
    w->next_scan = (int64_t)pf_time_ns() + w->interval;
    return PF_OK;
}

static intptr_t pf__scan_handle(pf__watcher *w) {
    (void)w;
    return -1;
}

static const pf__wbackend pf__scan_backend = {
    pf__scan_init,    pf__scan_free,       pf__scan_dir_add,
    pf__scan_dir_remove, NULL,             NULL,
    pf__scan_collect, pf__scan_handle,
};

const pf__wbackend *const pf__watch_scan = &pf__scan_backend;

/* ---- API ---------------------------------------------------------------- */

int pf__watcher_init_with(
    pf_watcher_t *ww, struct allocator_t *alloc, int backend,
    int64_t interval_ns
) {
    pf__watcher *w;
    const pf__wbackend *be;
    int rc;

    if (!ww)
        return PF_EINVAL;
    ww->pf__impl = NULL;
    be = backend == PF__WATCH_SCAN || !pf__watch_native ? pf__watch_scan
                                                        : pf__watch_native;
    w = (pf__watcher *)zallocate(alloc, sizeof(*w));
    if (!w)
        return PF_ENOMEM;
    w->alloc = alloc;
    w->be = be;
    w->interval = interval_ns > 0 ? interval_ns : 500000000;
    rc = be->init(w);
    if (rc) {
        deallocate(alloc, w, sizeof(*w));
        return rc;
    }
    ww->pf__impl = w;
    return PF_OK;
}

PF_API int pf_watcher_init(pf_watcher_t *w, struct allocator_t *alloc) {
    return pf__watcher_init_with(w, alloc, PF__WATCH_DEFAULT, 0);
}

static void pf__watch_drop(pf__watcher *w, pf__watch *wt) {
    size_t i, j = 0;
    pf__tree_remove(w, wt, "", 0);
    pf__dirs_compact(w);
    /* Its events that were not read. */
    for (i = w->qhead; i < w->qn; i++) {
        if (w->queue[i].id == wt->id)
            pf__event_drop(w, &w->queue[i]);
        else
            w->queue[w->qhead + j++] = w->queue[i];
    }
    w->qn = w->qhead + j;
    for (i = 0; i < w->nwatches; i++)
        if (w->watches[i] == wt) {
            w->watches[i] = w->watches[--w->nwatches];
            break;
        }
    pf__wstr_drop(w, wt->root, wt->root_len);
    pf__wstr_drop(w, wt->filter, wt->filter_len);
    deallocate(w->alloc, wt, sizeof(*wt));
}

PF_API int pf_watcher_free(pf_watcher_t *ww) {
    pf__watcher *w = pf__impl(ww);
    size_t i;
    if (!w)
        return PF_OK;
    while (w->nwatches > 0)
        pf__watch_drop(w, w->watches[w->nwatches - 1]);
    w->be->free(w);
    pf__given_free(w);
    for (i = w->qhead; i < w->qn; i++)
        pf__event_drop(w, &w->queue[i]);
    deallocate(w->alloc, w->queue, w->qcap * sizeof(pf__wevent));
    deallocate(w->alloc, w->given, w->gcap * sizeof(pf__wevent));
    deallocate(w->alloc, w->watches, w->wcap * sizeof(pf__watch *));
    deallocate(w->alloc, w->dirs, w->dcap * sizeof(pf__wdir *));
    deallocate(w->alloc, w, sizeof(*w));
    ww->pf__impl = NULL;
    return PF_OK;
}

PF_API int pf_watcher_add(pf_watcher_t *ww, const char *path, unsigned flags) {
    pf__watcher *w = pf__impl(ww);
    pf__watch *wt;
    pf_stat_t st;
    size_t len;
    int rc;

    if (!w || !path || !*path || (flags & ~PF_WATCH_RECURSIVE))
        return PF_EINVAL;
    rc = pf_stat(path, &st);
    if (rc)
        return rc;
    if (st.type != PF_FILE_DIR && (flags & PF_WATCH_RECURSIVE))
        return PF_ENOTDIR;
    rc = pf__grow(
        w, (void **)&w->watches, w->nwatches, &w->wcap, sizeof(pf__watch *)
    );
    if (rc)
        return rc;
    wt = (pf__watch *)zallocate(w->alloc, sizeof(*wt));
    if (!wt)
        return PF_ENOMEM;
    if (st.type == PF_FILE_DIR) {
        len = strlen(path);
        while (len > 1 && pf_path_is_sep((unsigned char)path[len - 1]))
            len--;
        wt->root = pf__wdup(w, path, len);
        wt->root_len = len;
        wt->recursive = (flags & PF_WATCH_RECURSIVE) != 0;
    } else {
        size_t bl;
        const char *dir = pf_path_dirname(path, &len);
        const char *base = pf_path_basename(path, &bl);
        wt->root = pf__wdup(w, dir, len);
        wt->root_len = len;
        wt->filter = pf__wdup(w, base, bl);
        wt->filter_len = bl;
        if (!wt->filter) {
            pf__wstr_drop(w, wt->root, wt->root_len);
            wt->root = NULL;
        }
    }
    if (!wt->root) {
        deallocate(w->alloc, wt, sizeof(*wt));
        return PF_ENOMEM;
    }
    if (++w->next_id <= 0)
        w->next_id = 1;
    wt->id = w->next_id;
    w->watches[w->nwatches++] = wt;
    rc = pf__tree_add(w, wt, "", 0, false);
    if (rc) {
        pf__watch_drop(w, wt);
        return rc;
    }
    return wt->id;
}

PF_API int pf_watcher_remove(pf_watcher_t *ww, int id) {
    pf__watcher *w = pf__impl(ww);
    pf__watch *wt;
    if (!w)
        return PF_EINVAL;
    wt = pf__watch_find(w, id);
    if (!wt)
        return PF_ENOENT;
    pf__watch_drop(w, wt);
    return PF_OK;
}

/* Moves up to `n` queued events into the caller's array. */
static int pf__deliver(pf__watcher *w, pf_watch_event_t *ev, size_t n) {
    size_t k = 0;
    while (k < n && w->qhead < w->qn) {
        pf__wevent *e;
        if (pf__grow(
                w, (void **)&w->given, w->ngiven, &w->gcap, sizeof(*e)
            ))
            break;
        e = &w->given[w->ngiven++];
        *e = w->queue[w->qhead++];
        ev[k].id = e->id;
        ev[k].kind = e->kind;
        ev[k].path = e->path;
        ev[k].old_path = e->old;
        ev[k].dir = e->dir;
        k++;
    }
    if (w->qhead == w->qn)
        w->qhead = w->qn = 0;
    return k > 0 || n == 0 ? (int)k : PF_ENOMEM;
}

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

static int pf__read(
    pf_watcher_t *ww, pf_watch_event_t *ev, size_t n,
    const struct timespec *deadline, int64_t end
) {
    pf__watcher *w = pf__impl(ww);

    if (!w || !ev || n == 0)
        return PF_EINVAL;
    pf__given_free(w);
    for (;;) {
        int64_t left;
        int rc;
        if (w->qhead < w->qn)
            return pf__deliver(w, ev, n > 0x7FFFFFFF ? 0x7FFFFFFF : n);
        left = pf__left(deadline, end);
        rc = w->be->collect(w, left);
        if (rc < 0)
            return rc;
        rc = pf__process(w);
        if (rc < 0)
            return rc;
        if (w->qhead < w->qn)
            continue;
        if (left == 0)
            return 0;
    }
}

PF_API int pf_watcher_read(
    pf_watcher_t *w,
    pf_watch_event_t *ev,
    size_t n,
    const struct timespec *deadline
) {
    return pf__read(w, ev, n, deadline, -1);
}

PF_API int pf_watcher_read_ns(
    pf_watcher_t *w, pf_watch_event_t *ev, size_t n, int64_t timeout_ns
) {
    int64_t end = -1;
    if (timeout_ns >= 0) {
        end = (int64_t)pf_time_ns() + timeout_ns;
        if (end < 0)
            end = INT64_MAX;
    }
    return pf__read(w, ev, n, NULL, end);
}

PF_API int pf_poller_add_watcher(
    pf_poller_t *p, pf_watcher_t *ww, void *data
) {
    pf__watcher *w = pf__impl(ww);
    intptr_t h;
    if (!w)
        return PF_EINVAL;
    h = w->be->handle(w);
    if (h == -1)
        return PF_ENOSYS;
    return pf__poller_add(p, PF__PK_WATCHER, h, ww, PF_POLL_IN, data);
}
