/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    pf_dir_walk, on the pf_opendir backends (posix/dirent.c,
    win32/dirent.c).
*/

#include "internal.h"

#include <cpolyfill/dirent.h>
#include <cpolyfill/fs.h>

#include <errno.h>
#include <string.h>

typedef struct pf__walk {
    char *buf;
    size_t cap;
    int max_depth;
    unsigned flags;
    pf_walk_fn *fn;
    void *user;
} pf__walk;

/* The directories being walked, for PF_WALK_FOLLOW. */
typedef struct pf__walk_frame {
    const struct pf__walk_frame *up;
    uint64_t dev, ino;
} pf__walk_frame;

static int pf__walk_reserve(pf__walk *w, size_t need) {
    size_t cap;
    char *p;

    if (need <= w->cap)
        return PF_OK;
    cap = w->cap ? w->cap : 256;
    while (cap < need)
        cap *= 2;
    p = (char *)reallocate(NULL, w->buf, w->cap, cap);
    if (!p)
        return PF_ENOMEM;
    w->buf = p;
    w->cap = cap;
    return PF_OK;
}

static bool pf__walk_loops(const pf__walk_frame *f, const pf_stat_t *st) {
    for (; f; f = f->up)
        if (f->dev == st->dev && f->ino == st->ino)
            return true;
    return false;
}

static int pf__walk_dir(
    pf__walk *w, size_t len, int depth, const pf__walk_frame *up
) {
    pf_dir_t *dir = pf_opendir(w->buf);
    int rc = 0;

    if (!dir)
        return pf_error_from_errno(errno);

    for (;;) {
        struct pf_dirent *ent;
        pf__walk_frame frame;
        size_t nlen, end;
        bool descend;

        errno = 0;
        ent = pf_readdir(dir);
        if (!ent) {
            rc = errno ? pf_error_from_errno(errno) : 0;
            break;
        }
        if (ent->d_name[0] == '.'
            && (!ent->d_name[1]
                || (ent->d_name[1] == '.' && !ent->d_name[2])))
            continue;

        nlen = strlen(ent->d_name);
        end = len + 1 + nlen;
        if ((rc = pf__walk_reserve(w, end + 1)) != 0)
            break;
        if (len && !pf_path_is_sep((unsigned char)w->buf[len - 1]))
            w->buf[len] = PF_PATH_SEP;
        else
            end--; /* the walked path already ends with a separator */
        memcpy(w->buf + end - nlen, ent->d_name, nlen + 1);

        frame.up = up;
        frame.dev = frame.ino = 0;
        descend = ent->d_type == PF_DT_DIR;
        if (w->flags & PF_WALK_FOLLOW) {
            pf_stat_t st;
            if ((descend || ent->d_type == PF_DT_LNK)
                && pf_stat(w->buf, &st) == 0 && st.type == PF_FILE_DIR) {
                descend = !pf__walk_loops(up, &st);
                if (descend)
                    ent->d_type = PF_DT_DIR;
                frame.dev = st.dev;
                frame.ino = st.ino;
            }
        }
        if (w->max_depth > 0 && depth >= w->max_depth)
            descend = false;

        if (!(w->flags & PF_WALK_POSTORDER)) {
            rc = w->fn(w->buf, ent, depth, w->user);
            if (rc == PF_WALK_SKIP)
                descend = false;
            else if (rc)
                break;
            rc = 0;
        }
        if (descend) {
            /* `ent` belongs to `dir`, which the recursion does not touch. */
            rc = pf__walk_dir(w, end, depth + 1, &frame);
            if (rc)
                break;
            w->buf[end] = 0;
        }
        if (w->flags & PF_WALK_POSTORDER) {
            rc = w->fn(w->buf, ent, depth, w->user);
            if (rc && rc != PF_WALK_SKIP)
                break;
            rc = 0;
        }
        w->buf[len] = 0;
    }

    pf_closedir(dir);
    return rc;
}

PF_API int pf_dir_walk(
    const char *path, int max_depth, unsigned flags, pf_walk_fn *fn, void *user
) {
    pf__walk w;
    pf__walk_frame root;
    size_t len;
    int rc;

    if (!path || !fn)
        return PF_EINVAL;
    memset(&w, 0, sizeof(w));
    w.max_depth = max_depth;
    w.flags = flags;
    w.fn = fn;
    w.user = user;

    len = strlen(path);
    if ((rc = pf__walk_reserve(&w, len + 1)) != 0)
        return rc;
    memcpy(w.buf, path, len + 1);

    root.up = NULL;
    root.dev = root.ino = 0;
    if (flags & PF_WALK_FOLLOW) {
        pf_stat_t st;
        if (pf_stat(path, &st) == 0) {
            root.dev = st.dev;
            root.ino = st.ino;
        }
    }
    rc = pf__walk_dir(&w, len, 1, &root);
    deallocate(NULL, w.buf, w.cap);
    return rc;
}
