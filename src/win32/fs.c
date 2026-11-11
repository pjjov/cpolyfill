/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/fs.h> on Windows.
*/

#include "../internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/string.h>

#include <errno.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <winioctl.h>
/* The FOLDERID_* GUIDs are defined here rather than taken from uuid.lib,
   which MinGW and MSVC provide differently. */
#include <initguid.h>
#include <knownfolders.h>
#include <sddl.h>
#include <shlobj.h>

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    #define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

/* The reparse data of symbolic links and junctions (ntifs.h, which the SDK
   only has in the WDK). */
typedef struct pf__reparse {
    ULONG tag;
    USHORT data_length;
    USHORT reserved;
    union {
        struct {
            USHORT sub_offset, sub_length, print_offset, print_length;
            ULONG flags;
            WCHAR path[1];
        } symlink;
        struct {
            USHORT sub_offset, sub_length, print_offset, print_length;
            WCHAR path[1];
        } mount;
    } u;
} pf__reparse;

#define PF__REPARSE_SIZE (16 * 1024) /* MAXIMUM_REPARSE_DATA_BUFFER_SIZE */

/* Converts `path` into `w`, or returns -1 with errno from the function it
   is used in. */
#define PF__WPATH(w, path)                       \
    do {                                         \
        int pf__rc = pf__wpath(&(w), (path));    \
        if (pf__rc)                              \
            return pf__errno_fail(pf__rc);       \
    } while (0)

static bool pf__is_name_surrogate(ULONG tag) {
    return tag == IO_REPARSE_TAG_SYMLINK || tag == IO_REPARSE_TAG_MOUNT_POINT;
}

static HANDLE pf__open_meta(const uint16_t *w, bool follow) {
    return CreateFileW(
        (LPCWSTR)w,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | (follow ? 0 : FILE_FLAG_OPEN_REPARSE_POINT),
        NULL
    );
}

/* ---- Stand-ins -------------------------------------------------------- */

PF_API int pf_mkdir(const char *path, pf_mode_t mode) {
    pf__wstr w;
    BOOL ok;

    (void)mode;
    PF__WPATH(w, path);
    ok = CreateDirectoryW((LPCWSTR)w.ptr, NULL);
    pf__wstr_free(&w);
    return ok ? 0 : pf__errno_win32();
}

PF_API int pf_rmdir(const char *path) {
    pf__wstr w;
    BOOL ok;

    PF__WPATH(w, path);
    ok = RemoveDirectoryW((LPCWSTR)w.ptr);
    pf__wstr_free(&w);
    return ok ? 0 : pf__errno_win32();
}

PF_API int pf_unlink(const char *path) {
    pf__wstr w;
    DWORD attr;
    BOOL ok;

    PF__WPATH(w, path);
    attr = GetFileAttributesW((LPCWSTR)w.ptr);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        pf__wstr_free(&w);
        return pf__errno_win32();
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        /* A symbolic link to a directory, or a junction, is removed as a
           directory; a real directory is an error, as on Linux. */
        if (!(attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
            pf__wstr_free(&w);
            return pf__errno_fail(PF_EISDIR);
        }
        ok = RemoveDirectoryW((LPCWSTR)w.ptr);
    } else {
        ok = DeleteFileW((LPCWSTR)w.ptr);
        if (!ok && GetLastError() == ERROR_ACCESS_DENIED
            && (attr & FILE_ATTRIBUTE_READONLY)) {
            /* POSIX removes read-only files: only the directory counts. */
            SetFileAttributesW((LPCWSTR)w.ptr, attr & ~FILE_ATTRIBUTE_READONLY);
            ok = DeleteFileW((LPCWSTR)w.ptr);
            if (!ok) {
                DWORD err = GetLastError();
                SetFileAttributesW((LPCWSTR)w.ptr, attr);
                SetLastError(err);
            }
        }
    }
    if (!ok) {
        pf__errno_win32();
        pf__wstr_free(&w);
        return -1;
    }
    pf__wstr_free(&w);
    return 0;
}

PF_API int pf_rename(const char *from, const char *to) {
    pf__wstr wf, wt;
    int rc;
    BOOL ok;

    PF__WPATH(wf, from);
    if ((rc = pf__wpath(&wt, to)) != 0) {
        pf__wstr_free(&wf);
        return pf__errno_fail(rc);
    }
    ok = MoveFileExW(
        (LPCWSTR)wf.ptr, (LPCWSTR)wt.ptr, MOVEFILE_REPLACE_EXISTING
    );
    if (!ok)
        pf__errno_win32();
    pf__wstr_free(&wt);
    pf__wstr_free(&wf);
    return ok ? 0 : -1;
}

PF_API int pf_chdir(const char *path) {
    pf__wstr w;
    BOOL ok;

    PF__WPATH(w, path);
    ok = SetCurrentDirectoryW((LPCWSTR)w.ptr);
    pf__wstr_free(&w);
    return ok ? 0 : pf__errno_win32();
}

/* A wide path into `buf` (or new memory from malloc, at least `size`
   bytes, if NULL): the stand-ins' convention for getcwd / realpath. */
static char *pf__wide_out(const uint16_t *w, size_t len, char *buf, size_t size) {
    ptrdiff_t n = pf__path_from_wide(w, len, NULL, 0);

    if (n < 0) {
        pf__errno_fail((int)n);
        return NULL;
    }
    if (!buf) {
        if (size < (size_t)n + 1)
            size = (size_t)n + 1;
        buf = (char *)malloc(size);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
        pf__path_from_wide(w, len, buf, size);
        return buf;
    }
    if (size < (size_t)n + 1) {
        errno = ERANGE;
        return NULL;
    }
    pf__path_from_wide(w, len, buf, size);
    return buf;
}

PF_API char *pf_getcwd(char *buf, size_t size) {
    DWORD n = GetCurrentDirectoryW(0, NULL), got;
    uint16_t *w;
    char *res;

    if (buf && size == 0) {
        errno = EINVAL;
        return NULL;
    }
    if (n == 0) {
        pf__errno_win32();
        return NULL;
    }
    w = (uint16_t *)allocate(NULL, (size_t)n * 2);
    if (!w) {
        errno = ENOMEM;
        return NULL;
    }
    got = GetCurrentDirectoryW(n, (LPWSTR)w);
    if (got == 0 || got >= n) {
        deallocate(NULL, w, (size_t)n * 2);
        if (got == 0)
            pf__errno_win32();
        else
            errno = EAGAIN; /* changed meanwhile */
        return NULL;
    }
    if (!buf && size && size < (size_t)pf__path_from_wide(w, got, NULL, 0) + 1) {
        deallocate(NULL, w, (size_t)n * 2);
        errno = ERANGE;
        return NULL;
    }
    res = pf__wide_out(w, got, buf, size);
    deallocate(NULL, w, (size_t)n * 2);
    return res;
}

static bool pf__is_program(const char *path) {
    size_t len;
    const char *ext = pf_path_extension(path, &len);
    return len == 4
        && (pf_strncasecmp(ext, ".exe", 4) == 0
            || pf_strncasecmp(ext, ".com", 4) == 0
            || pf_strncasecmp(ext, ".bat", 4) == 0
            || pf_strncasecmp(ext, ".cmd", 4) == 0);
}

PF_API int pf_access(const char *path, int mode) {
    pf__wstr w;
    DWORD attr;

    PF__WPATH(w, path);
    attr = GetFileAttributesW((LPCWSTR)w.ptr);
    pf__wstr_free(&w);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return pf__errno_win32();
    if ((mode & PF_W_OK) && (attr & FILE_ATTRIBUTE_READONLY)
        && !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return pf__errno_fail(PF_EACCES);
    if ((mode & PF_X_OK) && !(attr & FILE_ATTRIBUTE_DIRECTORY)
        && !pf__is_program(path))
        return pf__errno_fail(PF_EACCES);
    return 0;
}

PF_API char *pf_realpath(const char *path, char *resolved) {
    pf__wstr w;
    HANDLE h;
    DWORD n;
    uint16_t small[MAX_PATH + 1], *full = small;
    size_t cap = MAX_PATH + 1;
    char *res;
    int rc;

    if ((rc = pf__wpath(&w, path)) != 0) {
        pf__errno_fail(rc);
        return NULL;
    }
    h = pf__open_meta(w.ptr, true);
    pf__wstr_free(&w);
    if (h == INVALID_HANDLE_VALUE) {
        pf__errno_win32();
        return NULL;
    }
    for (;;) {
        n = GetFinalPathNameByHandleW(
            h, (LPWSTR)full, (DWORD)cap, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS
        );
        if (n == 0 || n < cap)
            break;
        if (full != small)
            deallocate(NULL, full, cap * 2);
        cap = (size_t)n + 1;
        full = (uint16_t *)allocate(NULL, cap * 2);
        if (!full) {
            CloseHandle(h);
            errno = ENOMEM;
            return NULL;
        }
    }
    if (n == 0)
        pf__errno_win32();
    CloseHandle(h);
    res = NULL;
    if (n) {
        res = pf__wide_out(full, n, resolved, resolved ? PF_PATH_MAX : 0);
        if (!res && resolved && errno == ERANGE)
            errno = ENAMETOOLONG;
    }
    if (full != small)
        deallocate(NULL, full, cap * 2);
    return res;
}

PF_API int pf_mkstemp(char *tmpl) { return pf__mktemp(tmpl, false); }

PF_API char *pf_mkdtemp(char *tmpl) {
    return pf__mktemp(tmpl, true) == 0 ? tmpl : NULL;
}

PF_API int pf_symlink(const char *target, const char *linkpath) {
    pf__wstr wt, wl, wd;
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE, attr;
    size_t k, dlen, cap;
    const char *dir;
    char *probe;
    BOOL ok;
    int rc;

    if (!target || !linkpath)
        return pf__errno_fail(PF_EINVAL);
    if (!*target)
        return pf__errno_fail(PF_ENOENT);

    /* Whether the target is a directory, seen from the link's directory:
       Windows has two kinds of symbolic links. */
    cap = strlen(linkpath) + strlen(target) + 2;
    probe = (char *)malloc(cap);
    if (!probe)
        return pf__errno_fail(PF_ENOMEM);
    dir = pf_path_dirname(linkpath, &dlen);
    if (pf_path_root_len(target) > 0 || dir != linkpath) {
        strcpy(probe, target); /* absolute, or the link is in "." */
    } else {
        memcpy(probe, linkpath, dlen);
        probe[dlen] = 0;
        pf_path_join(probe, cap, probe, target);
    }
    if (pf__wpath(&wd, probe) == 0) {
        attr = GetFileAttributesW((LPCWSTR)wd.ptr);
        if (attr != INVALID_FILE_ATTRIBUTES
            && (attr & FILE_ATTRIBUTE_DIRECTORY))
            flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
        pf__wstr_free(&wd);
    }
    free(probe);

    /* The target is stored as given (a relative one stays relative), with
       Windows separators. */
    if ((rc = pf__wstr_from_wtf8(&wt, target)) != 0)
        return pf__errno_fail(rc);
    for (k = 0; wt.ptr[k]; k++)
        if (wt.ptr[k] == '/')
            wt.ptr[k] = '\\';
    if ((rc = pf__wpath(&wl, linkpath)) != 0) {
        pf__wstr_free(&wt);
        return pf__errno_fail(rc);
    }
    ok = CreateSymbolicLinkW((LPCWSTR)wl.ptr, (LPCWSTR)wt.ptr, flags);
    if (!ok && GetLastError() == ERROR_INVALID_PARAMETER) {
        /* Windows before 10 1703 rejects the unprivileged flag. */
        flags &= ~(DWORD)SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
        ok = CreateSymbolicLinkW((LPCWSTR)wl.ptr, (LPCWSTR)wt.ptr, flags);
    }
    if (!ok)
        pf__errno_win32();
    pf__wstr_free(&wl);
    pf__wstr_free(&wt);
    return ok ? 0 : -1;
}

PF_API pf_ssize_t pf_readlink(const char *path, char *buf, size_t size) {
    pf__wstr w;
    pf__reparse *rp;
    const WCHAR *name;
    size_t len, tsize;
    ptrdiff_t n;
    char *tmp;
    HANDLE h;
    DWORD got;
    BOOL ok;

    PF__WPATH(w, path);
    h = pf__open_meta(w.ptr, false);
    pf__wstr_free(&w);
    if (h == INVALID_HANDLE_VALUE)
        return pf__errno_win32();
    rp = (pf__reparse *)allocate(NULL, PF__REPARSE_SIZE);
    if (!rp) {
        CloseHandle(h);
        return pf__errno_fail(PF_ENOMEM);
    }
    ok = DeviceIoControl(
        h, FSCTL_GET_REPARSE_POINT, NULL, 0, rp, PF__REPARSE_SIZE, &got, NULL
    );
    CloseHandle(h);
    if (!ok) {
        DWORD err = GetLastError();
        deallocate(NULL, rp, PF__REPARSE_SIZE);
        if (err == ERROR_NOT_A_REPARSE_POINT)
            return pf__errno_fail(PF_EINVAL);
        return pf__errno_fail(pf_error_from_win32(err));
    }

    if (rp->tag == IO_REPARSE_TAG_SYMLINK) {
        /* The print name is the target as given (relative or C:\...). */
        name = rp->u.symlink.path + rp->u.symlink.print_offset / 2;
        len = rp->u.symlink.print_length / 2;
        if (len == 0) {
            name = rp->u.symlink.path + rp->u.symlink.sub_offset / 2;
            len = rp->u.symlink.sub_length / 2;
        }
    } else if (rp->tag == IO_REPARSE_TAG_MOUNT_POINT) {
        name = rp->u.mount.path + rp->u.mount.sub_offset / 2;
        len = rp->u.mount.sub_length / 2;
    } else {
        deallocate(NULL, rp, PF__REPARSE_SIZE);
        return pf__errno_fail(PF_EINVAL);
    }
    /* \??\C:\x is the NT form of C:\x */
    if (len >= 4 && name[0] == '\\' && name[1] == '?' && name[2] == '?'
        && name[3] == '\\') {
        name += 4;
        len -= 4;
    }

    n = pf__utf16_to_wtf8((const uint16_t *)name, len, NULL, 0);
    if (n < 0) {
        deallocate(NULL, rp, PF__REPARSE_SIZE);
        return pf__errno_fail((int)n);
    }
    tsize = (size_t)n + 1;
    tmp = (char *)allocate(NULL, tsize);
    if (!tmp) {
        deallocate(NULL, rp, PF__REPARSE_SIZE);
        return pf__errno_fail(PF_ENOMEM);
    }
    pf__utf16_to_wtf8((const uint16_t *)name, len, tmp, tsize);
    deallocate(NULL, rp, PF__REPARSE_SIZE);
    if ((size_t)n > size)
        n = (ptrdiff_t)size;
    memcpy(buf, tmp, (size_t)n);
    deallocate(NULL, tmp, tsize);
    return n;
}

PF_API int pf_link(const char *existing, const char *linkpath) {
    pf__wstr we, wl;
    int rc;
    BOOL ok;

    PF__WPATH(we, existing);
    if ((rc = pf__wpath(&wl, linkpath)) != 0) {
        pf__wstr_free(&we);
        return pf__errno_fail(rc);
    }
    ok = CreateHardLinkW((LPCWSTR)wl.ptr, (LPCWSTR)we.ptr, NULL);
    if (!ok)
        pf__errno_win32();
    pf__wstr_free(&wl);
    pf__wstr_free(&we);
    return ok ? 0 : -1;
}

PF_API int pf_truncate(const char *path, pf_off_t size) {
    FILE_END_OF_FILE_INFO eof;
    pf__wstr w;
    HANDLE h;
    BOOL ok;

    if (size < 0)
        return pf__errno_fail(PF_EINVAL);
    PF__WPATH(w, path);
    h = CreateFileW(
        (LPCWSTR)w.ptr,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    pf__wstr_free(&w);
    if (h == INVALID_HANDLE_VALUE)
        return pf__errno_win32();
    eof.EndOfFile.QuadPart = size;
    ok = SetFileInformationByHandle(h, FileEndOfFileInfo, &eof, sizeof(eof));
    if (!ok)
        pf__errno_win32();
    CloseHandle(h);
    return ok ? 0 : -1;
}

PF_API int pf_chmod(const char *path, pf_mode_t mode) {
    pf__wstr w;
    DWORD attr;
    BOOL ok;

    PF__WPATH(w, path);
    attr = GetFileAttributesW((LPCWSTR)w.ptr);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        pf__wstr_free(&w);
        return pf__errno_win32();
    }
    if (mode & 0200)
        attr &= ~(DWORD)FILE_ATTRIBUTE_READONLY;
    else
        attr |= FILE_ATTRIBUTE_READONLY;
    ok = SetFileAttributesW((LPCWSTR)w.ptr, attr);
    pf__wstr_free(&w);
    return ok ? 0 : pf__errno_win32();
}

/* ---- Status ----------------------------------------------------------- */

/* 100 ns intervals since 1601 -> struct timespec since 1970. */
static struct timespec pf__ts_from_filetime(int64_t t) {
    struct timespec ts;
    int64_t unix100 = t - INT64_C(116444736000000000);
    int64_t sec = unix100 / 10000000, rem = unix100 % 10000000;

    if (rem < 0) {
        rem += 10000000;
        sec--;
    }
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)(rem * 100);
    return ts;
}

static int64_t pf__ft64(FILETIME ft) {
    return (int64_t)(((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime);
}

static uint32_t pf__perm(DWORD attr, const char *path) {
    uint32_t perm = (attr & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666;
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) || (path && pf__is_program(path)))
        perm |= 0111;
    return perm;
}

static int pf__stat_handle(HANDLE h, const char *path, pf_stat_t *st) {
    BY_HANDLE_FILE_INFORMATION bi;
    FILE_BASIC_INFO basic;
    FILE_ATTRIBUTE_TAG_INFO tag;
    DWORD type = GetFileType(h);

    memset(st, 0, sizeof(*st));
    if (type == FILE_TYPE_CHAR || type == FILE_TYPE_PIPE) {
        st->type = type == FILE_TYPE_CHAR ? PF_FILE_CHAR : PF_FILE_FIFO;
        st->perm = 0666;
        st->nlink = 1;
        return PF_OK;
    }
    if (!GetFileInformationByHandle(h, &bi))
        return pf_error_last_win32();

    if ((bi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        && GetFileInformationByHandleEx(h, FileAttributeTagInfo, &tag, sizeof(tag))
        && pf__is_name_surrogate(tag.ReparseTag))
        st->type = PF_FILE_SYMLINK; /* only when opened without following */
    else if (bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->type = PF_FILE_DIR;
    else
        st->type = PF_FILE_REGULAR;

    st->perm = pf__perm(bi.dwFileAttributes, path);
    st->size = ((uint64_t)bi.nFileSizeHigh << 32) | bi.nFileSizeLow;
    st->dev = bi.dwVolumeSerialNumber;
    st->ino = ((uint64_t)bi.nFileIndexHigh << 32) | bi.nFileIndexLow;
    st->nlink = bi.nNumberOfLinks;
    st->atime = pf__ts_from_filetime(pf__ft64(bi.ftLastAccessTime));
    st->mtime = pf__ts_from_filetime(pf__ft64(bi.ftLastWriteTime));
    st->btime = pf__ts_from_filetime(pf__ft64(bi.ftCreationTime));
    if (GetFileInformationByHandleEx(h, FileBasicInfo, &basic, sizeof(basic)))
        st->ctime = pf__ts_from_filetime(basic.ChangeTime.QuadPart);
    else
        st->ctime = st->mtime;
    return PF_OK;
}

static int pf__stat_path(const char *path, pf_stat_t *st, bool follow) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    pf__wstr w;
    HANDLE h;
    int rc;

    if (!path || !st)
        return PF_EINVAL;
    if ((rc = pf__wpath(&w, path)) != 0)
        return rc;
    h = pf__open_meta(w.ptr, follow);
    if (h != INVALID_HANDLE_VALUE) {
        pf__wstr_free(&w);
        rc = pf__stat_handle(h, path, st);
        CloseHandle(h);
        return rc;
    }
    rc = pf_error_last_win32();
    if (rc == PF_ENOENT || rc == PF_ENOTDIR
        || !GetFileAttributesExW((LPCWSTR)w.ptr, GetFileExInfoStandard, &data)) {
        pf__wstr_free(&w);
        return rc;
    }
    /* Files that cannot be opened at all (pagefile.sys): what the
       directory knows. */
    pf__wstr_free(&w);
    memset(st, 0, sizeof(*st));
    st->type = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                 ? PF_FILE_DIR
                 : PF_FILE_REGULAR;
    st->perm = pf__perm(data.dwFileAttributes, path);
    st->size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    st->nlink = 1;
    st->atime = pf__ts_from_filetime(pf__ft64(data.ftLastAccessTime));
    st->mtime = pf__ts_from_filetime(pf__ft64(data.ftLastWriteTime));
    st->ctime = st->mtime;
    st->btime = pf__ts_from_filetime(pf__ft64(data.ftCreationTime));
    return PF_OK;
}

PF_API int pf_stat(const char *path, pf_stat_t *st) {
    return pf__stat_path(path, st, true);
}

PF_API int pf_lstat(const char *path, pf_stat_t *st) {
    return pf__stat_path(path, st, false);
}

PF_API int pf_fstat(int fd, pf_stat_t *st) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);

    if (!st)
        return PF_EINVAL;
    if (h == INVALID_HANDLE_VALUE)
        return PF_EBADF;
    return pf__stat_handle(h, NULL, st);
}

/* ---- Copying and locking ---------------------------------------------- */

PF_API int pf_copy_file(const char *from, const char *to, unsigned flags) {
    pf__wstr wf, wt;
    DWORD attr;
    int rc;
    BOOL ok;

    if (!from || !to)
        return PF_EINVAL;
    if ((rc = pf__wpath(&wf, from)) != 0)
        return rc;
    attr = GetFileAttributesW((LPCWSTR)wf.ptr);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        pf__wstr_free(&wf);
        return pf_error_last_win32();
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        pf__wstr_free(&wf);
        return PF_EISDIR;
    }
    if ((rc = pf__wpath(&wt, to)) != 0) {
        pf__wstr_free(&wf);
        return rc;
    }
    /* CopyFileExW copies the attributes (read-only) and lets ReFS clone
       the data. */
    ok = CopyFileExW(
        (LPCWSTR)wf.ptr,
        (LPCWSTR)wt.ptr,
        NULL,
        NULL,
        NULL,
        (flags & PF_COPY_OVERWRITE) ? 0 : COPY_FILE_FAIL_IF_EXISTS
    );
    rc = ok ? PF_OK : pf_error_last_win32();
    pf__wstr_free(&wt);
    pf__wstr_free(&wf);
    return rc;
}

PF_API int pf_lock_file(int fd, int op) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    OVERLAPPED ov;
    DWORD flags = 0;

    if (h == INVALID_HANDLE_VALUE)
        return PF_EBADF;
    memset(&ov, 0, sizeof(ov));
    switch (op & ~PF_LOCK_NB) {
    case PF_LOCK_UN:
        if (!UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &ov)) {
            if (GetLastError() == ERROR_NOT_LOCKED)
                return PF_OK;
            return pf_error_last_win32();
        }
        return PF_OK;
    case PF_LOCK_EX:
        flags = LOCKFILE_EXCLUSIVE_LOCK;
        break;
    case PF_LOCK_SH:
        break;
    default:
        return PF_EINVAL;
    }
    if (op & PF_LOCK_NB)
        flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!LockFileEx(h, flags, 0, MAXDWORD, MAXDWORD, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_LOCK_VIOLATION || err == ERROR_IO_PENDING)
            return PF_EWOULDBLOCK;
        return pf_error_from_win32(err);
    }
    return PF_OK;
}

/* ---- Known directories ------------------------------------------------ */

/* A wide directory, without a separator at the end. */
static ptrdiff_t pf__dir_out(const wchar_t *w, char *buf, size_t size) {
    size_t len = wcslen(w);
    while (len > 3 && (w[len - 1] == L'\\' || w[len - 1] == L'/'))
        len--;
    return pf__path_from_wide((const uint16_t *)w, len, buf, size);
}

static ptrdiff_t pf__known_folder(REFKNOWNFOLDERID id, char *buf, size_t size) {
    PWSTR w = NULL;
    ptrdiff_t n;
    HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, NULL, &w);

    if (FAILED(hr) || !w) {
        CoTaskMemFree(w);
        return PF_ENOENT;
    }
    n = pf__dir_out(w, buf, size);
    CoTaskMemFree(w);
    return n;
}

static ptrdiff_t pf__temp_dir(char *buf, size_t size) {
    wchar_t w[MAX_PATH + 2];
    DWORD n = GetTempPathW(MAX_PATH + 2, w);

    if (n == 0 || n > MAX_PATH + 1)
        return PF_ENOENT;
    return pf__dir_out(w, buf, size);
}

PF_API ptrdiff_t pf_known_dir(pf_known_dir_t kind, char *buf, size_t size) {
    switch (kind) {
    case PF_DIR_HOME:
        return pf__known_folder(&FOLDERID_Profile, buf, size);
    case PF_DIR_TEMP:
    case PF_DIR_RUNTIME:
        return pf__temp_dir(buf, size);
    case PF_DIR_DATA:
    case PF_DIR_STATE:
    case PF_DIR_CACHE:
        return pf__known_folder(&FOLDERID_LocalAppData, buf, size);
    case PF_DIR_CONFIG:
        return pf__known_folder(&FOLDERID_RoamingAppData, buf, size);
    case PF_DIR_DESKTOP:
        return pf__known_folder(&FOLDERID_Desktop, buf, size);
    case PF_DIR_DOCUMENTS:
        return pf__known_folder(&FOLDERID_Documents, buf, size);
    case PF_DIR_DOWNLOAD:
        return pf__known_folder(&FOLDERID_Downloads, buf, size);
    case PF_DIR_MUSIC:
        return pf__known_folder(&FOLDERID_Music, buf, size);
    case PF_DIR_PICTURES:
        return pf__known_folder(&FOLDERID_Pictures, buf, size);
    case PF_DIR_PUBLICSHARE:
        return pf__known_folder(&FOLDERID_Public, buf, size);
    case PF_DIR_TEMPLATES:
        return pf__known_folder(&FOLDERID_Templates, buf, size);
    case PF_DIR_VIDEOS:
        return pf__known_folder(&FOLDERID_Videos, buf, size);
    case PF_DIR_PROJECTS: {
        /* No known folder: Documents\Projects, where Visual Studio and
           others put theirs. */
        char docs[PF_PATH_MAX];
        ptrdiff_t n = pf__known_folder(&FOLDERID_Documents, docs, sizeof(docs));
        if (n < 0)
            return n == PF_ERANGE ? PF_ENAMETOOLONG : n;
        return pf_path_join(buf, size, docs, "Projects");
    }
    }
    return PF_EINVAL;
}

/* The profile directory of a local or domain account, from the registry
   (ProfileList, keyed by the account's SID). */
PF_API ptrdiff_t pf_home_dir_of(const char *user, char *buf, size_t size) {
    static const wchar_t list[] =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\";
    BYTE sid[SECURITY_MAX_SID_SIZE];
    DWORD sid_size = sizeof(sid), domain_size = 256, bytes;
    wchar_t domain[256], key[256], path[MAX_PATH * 2];
    LPWSTR sid_str = NULL;
    SID_NAME_USE use;
    pf__wstr w;
    LSTATUS st;
    int rc;

    if (!user || !*user)
        return pf_known_dir(PF_DIR_HOME, buf, size);
    if ((rc = pf__wstr_from_utf8(&w, user)) != 0)
        return rc;
    if (!LookupAccountNameW(
            NULL, (LPCWSTR)w.ptr, sid, &sid_size, domain, &domain_size, &use
        )) {
        pf__wstr_free(&w);
        return PF_ENOENT;
    }
    pf__wstr_free(&w);
    if (!ConvertSidToStringSidW(sid, &sid_str))
        return pf_error_last_win32();
    if (wcslen(sid_str) + sizeof(list) / sizeof(list[0]) > 256) {
        LocalFree(sid_str);
        return PF_ENOENT;
    }
    wcscpy(key, list);
    wcscat(key, sid_str);
    LocalFree(sid_str);

    /* RRF_RT_REG_SZ also takes REG_EXPAND_SZ, expanded (the profile
       path is "%SystemDrive%\\Users\\name"). */
    bytes = sizeof(path);
    st = RegGetValueW(
        HKEY_LOCAL_MACHINE, key, L"ProfileImagePath", RRF_RT_REG_SZ, NULL,
        path, &bytes
    );
    if (st != ERROR_SUCCESS)
        return PF_ENOENT;
    return pf__dir_out(path, buf, size);
}
