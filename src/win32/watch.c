/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    File watching on Windows: ReadDirectoryChangesW on each watch's root
    (over the whole tree for a recursive watch) tells the engine (watch.c)
    which directories changed. Every root's overlapped read signals one
    event, which is also what a poller waits on.
*/

#include "../internal.h"

#include "../watch_impl.h"

#include <string.h>
#include <windows.h>

#define PF__RDCW_SIZE 65536

#define PF__RDCW_FILTER                                            \
    (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME    \
     | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE     \
     | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION \
     | FILE_NOTIFY_CHANGE_SECURITY)

/* A root's read, in its pf__wdir.bh. */
typedef struct pf__rdcw {
    HANDLE h;
    OVERLAPPED ov;
    void *buf; /* PF__RDCW_SIZE bytes, DWORD-aligned */
    bool pending;
    bool recursive;
} pf__rdcw;

static HANDLE pf__rd_event(pf__watcher *w) { return (HANDLE)w->bstate; }

static int pf__rd_init(pf__watcher *w) {
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ev)
        return pf_error_last_win32();
    w->bstate = ev;
    return PF_OK;
}

static void pf__rd_free(pf__watcher *w) { CloseHandle(pf__rd_event(w)); }

static int pf__rd_issue(pf__watcher *w, pf__rdcw *r) {
    memset(&r->ov, 0, sizeof(r->ov));
    r->ov.hEvent = pf__rd_event(w);
    if (!ReadDirectoryChangesW(
            r->h,
            r->buf,
            PF__RDCW_SIZE,
            r->recursive,
            PF__RDCW_FILTER,
            NULL,
            &r->ov,
            NULL
        )) {
        r->pending = false;
        return pf_error_last_win32();
    }
    r->pending = true;
    return PF_OK;
}

static int pf__rd_dir_add(pf__watcher *w, pf__watch *wt, pf__wdir *d) {
    pf__rdcw *r;
    pf__wstr ws;
    size_t len;
    char *path;
    int rc;

    /* The root's read covers a recursive tree, and the other watches have
       only their root. */
    if (d->rel_len != 0)
        return PF_OK;
    path = pf__watch_path(w, wt, d, NULL, &len);
    if (!path)
        return PF_ENOMEM;
    rc = pf__wpath(&ws, path);
    pf__watch_path_free(w, path, len);
    if (rc)
        return rc;
    r = (pf__rdcw *)zallocate(w->alloc, sizeof(*r));
    if (r)
        r->buf = allocate(w->alloc, PF__RDCW_SIZE);
    if (!r || !r->buf) {
        if (r)
            deallocate(w->alloc, r, sizeof(*r));
        pf__wstr_free(&ws);
        return PF_ENOMEM;
    }
    r->h = CreateFileW(
        (const wchar_t *)ws.ptr,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    pf__wstr_free(&ws);
    if (r->h == INVALID_HANDLE_VALUE) {
        rc = pf_error_last_win32();
    } else {
        r->recursive = wt->recursive;
        rc = pf__rd_issue(w, r);
        if (rc)
            CloseHandle(r->h);
    }
    if (rc) {
        deallocate(w->alloc, r->buf, PF__RDCW_SIZE);
        deallocate(w->alloc, r, sizeof(*r));
        return rc;
    }
    d->bh = (intptr_t)r;
    return PF_OK;
}

static void pf__rd_dir_remove(pf__watcher *w, pf__wdir *d) {
    pf__rdcw *r = (pf__rdcw *)d->bh;
    if (r->pending) {
        DWORD n;
        CancelIoEx(r->h, &r->ov);
        GetOverlappedResult(r->h, &r->ov, &n, TRUE);
    }
    CloseHandle(r->h);
    deallocate(w->alloc, r->buf, PF__RDCW_SIZE);
    deallocate(w->alloc, r, sizeof(*r));
}

/* Marks the directory a reported name is in (and the name itself, if it
   is a directory) dirty. */
static void pf__rd_mark(
    pf__watcher *w, int watch, const WCHAR *name, size_t units
) {
    ptrdiff_t len = pf__utf16_to_wtf8((const uint16_t *)name, units, NULL, 0);
    char *rel;
    pf__wdir *d;
    size_t cut;

    if (len < 0)
        return;
    rel = (char *)allocate(w->alloc, (size_t)len + 1);
    if (!rel) {
        pf__watch_overflow(w, pf__watch_find(w, watch));
        return;
    }
    pf__utf16_to_wtf8((const uint16_t *)name, units, rel, (size_t)len + 1);
    cut = (size_t)len;
    while (cut > 0 && rel[cut - 1] != PF_PATH_SEP)
        cut--;
    d = pf__watch_dir_by_rel(w, watch, rel, cut > 0 ? cut - 1 : 0);
    if (d)
        d->dirty = true;
    d = pf__watch_dir_by_rel(w, watch, rel, (size_t)len);
    if (d)
        d->dirty = true;
    deallocate(w->alloc, rel, (size_t)len + 1);
}

static void pf__rd_parse(pf__watcher *w, pf__wdir *root, DWORD bytes) {
    pf__rdcw *r = (pf__rdcw *)root->bh;
    const BYTE *at = (const BYTE *)r->buf;
    const BYTE *end = at + bytes;

    while (at + offsetof(FILE_NOTIFY_INFORMATION, FileName) <= end) {
        const FILE_NOTIFY_INFORMATION *fi = (const FILE_NOTIFY_INFORMATION *)at;
        size_t units = fi->FileNameLength / sizeof(WCHAR);
        if ((const BYTE *)fi->FileName + fi->FileNameLength > end)
            break;
        pf__rd_mark(w, root->watch, fi->FileName, units);
        if (fi->NextEntryOffset == 0)
            break;
        at += fi->NextEntryOffset;
    }
}

static int pf__rd_collect(pf__watcher *w, int64_t timeout_ns) {
    HANDLE ev = pf__rd_event(w);
    int64_t ms64 = timeout_ns < 0 ? -1 : (timeout_ns + 999999) / 1000000;
    DWORD ms = ms64 < 0             ? INFINITE
        : ms64 >= (int64_t)INFINITE ? INFINITE - 1
                                    : (DWORD)ms64;
    DWORD r = WaitForSingleObject(ev, ms);
    size_t i;

    if (r == WAIT_TIMEOUT)
        return PF_OK;
    if (r != WAIT_OBJECT_0)
        return pf_error_last_win32();
    /* Reset first: a read that completes after this sets it again. */
    ResetEvent(ev);
    for (i = 0; i < w->ndirs; i++) {
        pf__wdir *d = w->dirs[i];
        pf__rdcw *rd;
        DWORD bytes = 0;
        if (d->dead || d->rel_len != 0 || d->bh == -1)
            continue;
        rd = (pf__rdcw *)d->bh;
        if (!rd->pending || !HasOverlappedIoCompleted(&rd->ov))
            continue;
        rd->pending = false;
        if (GetOverlappedResult(rd->h, &rd->ov, &bytes, FALSE)) {
            if (bytes == 0)
                pf__watch_overflow(w, pf__watch_find(w, d->watch));
            else
                pf__rd_parse(w, d, bytes);
        } else if (GetLastError() == ERROR_NOTIFY_ENUM_DIR) {
            pf__watch_overflow(w, pf__watch_find(w, d->watch));
        } else {
            /* Most likely the directory is gone: the scan says. */
            d->dirty = true;
        }
        if (pf__rd_issue(w, rd) != PF_OK)
            d->dirty = true;
    }
    return PF_OK;
}

static intptr_t pf__rd_handle(pf__watcher *w) {
    return (intptr_t)pf__rd_event(w);
}

static const pf__wbackend pf__rd_backend = {
    pf__rd_init, pf__rd_free, pf__rd_dir_add, pf__rd_dir_remove,
    NULL,        NULL,        pf__rd_collect, pf__rd_handle,
};

const pf__wbackend *const pf__watch_native = &pf__rd_backend;
