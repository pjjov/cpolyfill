/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The portable part of <cpolyfill/fs.h>: path strings, and what is built
    on the backends (posix/fs.c, win32/fs.c).
*/

#include "internal.h"

#include <cpolyfill/dirent.h>
#include <cpolyfill/fs.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define PF__IS_SEP(c) pf_path_is_sep((unsigned char)(c))

ptrdiff_t pf__copy_out(char *buf, size_t size, const char *s, size_t len) {
    if (len > (size_t)PTRDIFF_MAX - 1)
        return PF_ERANGE;
    if (buf) {
        if (size <= len)
            return PF_ERANGE;
        memcpy(buf, s, len);
        buf[len] = 0;
    }
    return (ptrdiff_t)len;
}

/* ---- Path strings ----------------------------------------------------- */

#if PF_OS_WINDOWS
static bool pf__is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* "X:" without a separator: relative to the current directory of a
   drive, so it is a root that `..` cannot go past, but not absolute. */
static bool pf__drive_relative(const char *p, size_t root) {
    return root == 2 && p[1] == ':';
}

/* The end of the component starting at `i`, plus the separator after it
   if there is one. */
static size_t pf__skip_component(const char *p, size_t i) {
    while (p[i] && !PF__IS_SEP(p[i]))
        i++;
    return p[i] ? i + 1 : i;
}
#endif

PF_API size_t pf_path_root_len(const char *p) {
    if (!p)
        return 0;
#if PF_OS_WINDOWS
    if (PF__IS_SEP(p[0]) && PF__IS_SEP(p[1])) {
        size_t i = 2;

        if ((p[2] == '?' || p[2] == '.') && PF__IS_SEP(p[3])) {
            /* \\?\C:\, \\?\UNC\server\share\, \\.\device\ */
            i = 4;
            if ((p[4] | 0x20) == 'u' && (p[5] | 0x20) == 'n'
                && (p[6] | 0x20) == 'c' && PF__IS_SEP(p[7])) {
                i = 8;
            } else if (pf__is_alpha(p[4]) && p[5] == ':') {
                return PF__IS_SEP(p[6]) ? 7 : 6;
            } else {
                return pf__skip_component(p, i);
            }
        }
        /* \\server\share\ */
        if (!p[i] || PF__IS_SEP(p[i]))
            return i;
        i = pf__skip_component(p, i);
        if (!p[i])
            return i;
        return pf__skip_component(p, i);
    }
    if (pf__is_alpha(p[0]) && p[1] == ':')
        return PF__IS_SEP(p[2]) ? 3 : 2;
    return PF__IS_SEP(p[0]) ? 1 : 0;
#else
    return p[0] == '/' ? 1 : 0;
#endif
}

PF_API bool pf_path_is_absolute(const char *p) {
#if PF_OS_WINDOWS
    size_t root = pf_path_root_len(p);
    if (root == 0)
        return false;
    if (PF__IS_SEP(p[0]) && PF__IS_SEP(p[1]))
        return true;
    return root == 3;
#else
    return p && p[0] == '/';
#endif
}

PF_API ptrdiff_t pf_path_join(
    char *buf, size_t size, const char *a, const char *b
) {
    size_t la, lb, need;
    bool sep;

    if (!a || !b)
        return PF_EINVAL;
    la = strlen(a);
    lb = strlen(b);
    if (la == 0 || pf_path_root_len(b) > 0)
        return pf__copy_out(buf, size, b, lb);

    sep = !PF__IS_SEP(a[la - 1]);
#if PF_OS_WINDOWS
    if (pf__drive_relative(a, pf_path_root_len(a)) && la == 2)
        sep = false; /* "C:" + "x" is "C:x" */
#endif
    need = la + sep + lb;
    if (!buf)
        return (ptrdiff_t)need;
    if (size <= need)
        return PF_ERANGE;
    memmove(buf + la + sep, b, lb + 1); /* b may overlap buf */
    memmove(buf, a, la);
    if (sep)
        buf[la] = PF_PATH_SEP;
    return (ptrdiff_t)need;
}

PF_API ptrdiff_t pf_path_normalize(char *buf, size_t size, const char *path) {
    size_t root, base, out = 0, i, n;
    bool rooted;
    ptrdiff_t rc;
    char *tmp;

    if (!path)
        return PF_EINVAL;
    n = strlen(path);
    root = pf_path_root_len(path);
#if PF_OS_WINDOWS
    rooted = root > 0 && !pf__drive_relative(path, root);
#else
    rooted = root > 0;
#endif
    /* The result is never longer than the path plus a separator (after a
       UNC root) or ".". */
    tmp = (char *)allocate(NULL, n + 2);
    if (!tmp)
        return PF_ENOMEM;

    for (i = 0; i < root; i++)
        tmp[out++] = PF__IS_SEP(path[i]) ? PF_PATH_SEP : path[i];
#if PF_OS_WINDOWS
    if (rooted && !PF__IS_SEP(tmp[out - 1]) && path[root])
        tmp[out++] = PF_PATH_SEP; /* "\\server\share" + "x" */
#endif
    base = out;

    i = root;
    while (path[i]) {
        size_t start, len;

        while (PF__IS_SEP(path[i]))
            i++;
        start = i;
        while (path[i] && !PF__IS_SEP(path[i]))
            i++;
        len = i - start;
        if (len == 0 || (len == 1 && path[start] == '.'))
            continue;
        if (len == 2 && path[start] == '.' && path[start + 1] == '.') {
            size_t q = out;
            while (q > base && tmp[q - 1] != PF_PATH_SEP)
                q--;
            if (out > base && !(out - q == 2 && tmp[q] == '.'
                                && tmp[q + 1] == '.')) {
                out = q > base ? q - 1 : base; /* pop the last component */
                continue;
            }
            if (rooted)
                continue; /* "/.." is "/" */
        }
        if (out > base)
            tmp[out++] = PF_PATH_SEP;
        memcpy(tmp + out, path + start, len);
        out += len;
    }
    if (out == 0)
        tmp[out++] = '.';

    rc = pf__copy_out(buf, size, tmp, out);
    deallocate(NULL, tmp, n + 2);
    return rc;
}

PF_API const char *pf_path_basename(const char *path, size_t *len) {
    size_t root, end, start, dummy;

    if (!len)
        len = &dummy;
    if (!path || !*path) {
        *len = 1;
        return ".";
    }
    root = pf_path_root_len(path);
    end = strlen(path);
    while (end > root && PF__IS_SEP(path[end - 1]))
        end--;
    if (end == root) {
        *len = root;
        return path;
    }
    start = end;
    while (start > root && !PF__IS_SEP(path[start - 1]))
        start--;
    *len = end - start;
    return path + start;
}

PF_API const char *pf_path_dirname(const char *path, size_t *len) {
    size_t root, end, dummy;

    if (!len)
        len = &dummy;
    if (!path || !*path) {
        *len = 1;
        return ".";
    }
    root = pf_path_root_len(path);
    end = strlen(path);
    while (end > root && PF__IS_SEP(path[end - 1]))
        end--;
    while (end > root && !PF__IS_SEP(path[end - 1]))
        end--;
    while (end > root && PF__IS_SEP(path[end - 1]))
        end--;
    if (end == 0) {
        *len = 1;
        return ".";
    }
    *len = end;
    return path;
}

PF_API const char *pf_path_extension(const char *path, size_t *len) {
    size_t blen, i, dot = 0, dummy;
    const char *base = pf_path_basename(path, &blen);

    if (!len)
        len = &dummy;
    /* A root alone ("/", "C:\") has no extension. */
    if (path && base == path && blen > 0 && blen == pf_path_root_len(path)) {
        *len = 0;
        return base + blen;
    }
    /* Leading dots belong to the name: ".bashrc", "..". */
    i = 0;
    while (i < blen && base[i] == '.')
        i++;
    for (; i < blen; i++)
        if (base[i] == '.')
            dot = i;
    if (dot == 0) {
        *len = 0;
        return base + blen;
    }
    *len = blen - dot;
    return base + dot;
}

/* ---- Status ----------------------------------------------------------- */

PF_API bool pf_exists(const char *path) {
    pf_stat_t st;
    return pf_stat(path, &st) == 0;
}

PF_API bool pf_is_dir(const char *path) {
    pf_stat_t st;
    return pf_stat(path, &st) == 0 && st.type == PF_FILE_DIR;
}

PF_API bool pf_is_file(const char *path) {
    pf_stat_t st;
    return pf_stat(path, &st) == 0 && st.type == PF_FILE_REGULAR;
}

PF_API bool pf_is_symlink(const char *path) {
    pf_stat_t st;
    return pf_lstat(path, &st) == 0 && st.type == PF_FILE_SYMLINK;
}

PF_API int64_t pf_file_size(const char *path) {
    pf_stat_t st;
    int rc = pf_stat(path, &st);

    if (rc)
        return rc;
    if (st.size > (uint64_t)INT64_MAX)
        return PF_EOVERFLOW;
    return (int64_t)st.size;
}

/* ---- Trees ------------------------------------------------------------ */

PF_API int pf_mkdirs(const char *path, pf_mode_t mode) {
    const char *dir;
    size_t len;
    char *parent;
    int rc;

    if (!path)
        return PF_EINVAL;
    if (pf_mkdir(path, mode) == 0)
        return PF_OK;
    rc = pf_error_from_errno(errno);
    if (rc == PF_EEXIST)
        return pf_is_dir(path) ? PF_OK : PF_EEXIST;
    if (rc != PF_ENOENT)
        return rc;

    dir = pf_path_dirname(path, &len);
    if (dir != path || len == strlen(path))
        return PF_ENOENT; /* the root or "." is missing */
    parent = (char *)allocate(NULL, len + 1);
    if (!parent)
        return PF_ENOMEM;
    memcpy(parent, path, len);
    parent[len] = 0;
    /* Parents can be entered and written by the owner whatever `mode`
       is, as with `mkdir -p`. */
    rc = pf_mkdirs(parent, mode | 0300);
    deallocate(NULL, parent, len + 1);
    if (rc)
        return rc;

    if (pf_mkdir(path, mode) == 0)
        return PF_OK;
    rc = pf_error_from_errno(errno);
    if (rc == PF_EEXIST && pf_is_dir(path))
        return PF_OK;
    return rc;
}

static int pf__remove_entry(
    const char *path, const struct pf_dirent *ent, int depth, void *user
) {
    int rc;

    (void)depth, (void)user;
    if (ent->d_type == PF_DT_DIR)
        rc = pf_rmdir(path);
    else
        rc = pf_unlink(path);
    if (rc == 0 || errno == ENOENT)
        return 0;
    return pf_error_from_errno(errno);
}

PF_API int pf_remove_all(const char *path) {
    pf_stat_t st;
    int rc;

    if (!path)
        return PF_EINVAL;
    rc = pf_lstat(path, &st);
    if (rc == PF_ENOENT)
        return PF_OK;
    if (rc)
        return rc;
    if (st.type != PF_FILE_DIR) {
        if (pf_unlink(path) == 0 || errno == ENOENT)
            return PF_OK;
        return pf_error_from_errno(errno);
    }
    rc = pf_dir_walk(path, 0, PF_WALK_POSTORDER, pf__remove_entry, NULL);
    if (rc)
        return rc;
    if (pf_rmdir(path) == 0 || errno == ENOENT)
        return PF_OK;
    return pf_error_from_errno(errno);
}

/* ---- Temporary names -------------------------------------------------- */

/* mkstemp and mkdtemp where the system has none (Windows) and for the
   tests. The names need not be unpredictable: creation is exclusive
   (O_EXCL, which on POSIX also refuses symbolic links). */
int pf__mktemp(char *tmpl, bool dir, unsigned mode) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static unsigned long long counter;
    size_t len;
    char *x;
    int attempt;

    if (!tmpl)
        return pf__errno_fail(PF_EINVAL);
    len = strlen(tmpl);
    if (len < 6 || memcmp(tmpl + len - 6, "XXXXXX", 6) != 0)
        return pf__errno_fail(PF_EINVAL);
    x = tmpl + len - 6;

    for (attempt = 0; attempt < 1000; attempt++) {
        unsigned long long v = pf_time_ns() ^ (uintptr_t)tmpl
                             ^ (++counter * 0x9E3779B97F4A7C15ull);
        int k, fd;

        for (k = 0; k < 6; k++) {
            v ^= v >> 29;
            v *= 0xBF58476D1CE4E5B9ull;
            x[k] = chars[(v >> 32) % (sizeof(chars) - 1)];
        }
        if (dir) {
            if (pf_mkdir(tmpl, (pf_mode_t)mode) == 0)
                return 0;
        } else {
            fd = pf_open(
                tmpl, PF_O_RDWR | PF_O_CREAT | PF_O_EXCL | PF_O_CLOEXEC,
                (pf_mode_t)mode
            );
            if (fd >= 0)
                return fd;
        }
        if (errno != EEXIST)
            break;
    }
    memset(x, 'X', 6);
    return -1;
}
