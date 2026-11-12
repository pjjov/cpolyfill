/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/stdlib.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <malloc.h>
    #include <wchar.h>
    #include <windows.h>
#endif

#ifndef PF__HAVE_QSORT_R_BSD
    #define PF__HAVE_QSORT_R_BSD 0
#endif
#ifndef PF__HAVE_GETC_UNLOCKED
    #define PF__HAVE_GETC_UNLOCKED 0
#endif

/* ---- Memory ----------------------------------------------------------- */

PF_API void *pf_aligned_alloc(size_t align, size_t size) {
    void *p;

    if (!align || (align & (align - 1))) {
        errno = EINVAL;
        return NULL;
    }
    if (align < sizeof(void *))
        align = sizeof(void *);
    if (size == 0)
        size = 1; /* a unique pointer, not NULL */
#if PF_OS_WINDOWS
    p = _aligned_malloc(size, align);
    if (!p)
        errno = ENOMEM;
#else
    {
        int rc = posix_memalign(&p, align, size);
        if (rc) {
            errno = rc;
            return NULL;
        }
    }
#endif
    return p;
}

PF_API void pf_aligned_free(void *p) {
#if PF_OS_WINDOWS
    _aligned_free(p);
#else
    free(p);
#endif
}

PF_API void *pf_reallocarray(void *p, size_t n, size_t size) {
    size_t total;

    if (size && n > SIZE_MAX / size) {
        errno = ENOMEM;
        return NULL;
    }
    total = n * size;
    /* realloc(p, 0) is implementation-defined (undefined in C23). */
    p = realloc(p, total ? total : 1);
    if (!p)
        errno = ENOMEM;
    return p;
}

/* ---- getline ---------------------------------------------------------- */

#if PF_OS_WINDOWS
    #define PF__LOCK_FILE(f) _lock_file(f)
    #define PF__UNLOCK_FILE(f) _unlock_file(f)
    #define PF__GETC(f) _getc_nolock(f)
#elif PF__HAVE_GETC_UNLOCKED
    #define PF__LOCK_FILE(f) flockfile(f)
    #define PF__UNLOCK_FILE(f) funlockfile(f)
    #define PF__GETC(f) getc_unlocked(f)
#else
    #define PF__LOCK_FILE(f) ((void)0)
    #define PF__UNLOCK_FILE(f) ((void)0)
    #define PF__GETC(f) getc(f)
#endif

/* The library's getdelim, used where the system has none (and by the
   tests everywhere). */
pf_ssize_t pf__getdelim_portable(char **line, size_t *cap, int delim, FILE *f) {
    size_t len = 0;
    int c, failed = 0;

    if (!line || !cap || !f) {
        errno = EINVAL;
        return -1;
    }
    if (!*line)
        *cap = 0;

    PF__LOCK_FILE(f);
    for (;;) {
        c = PF__GETC(f);
        if (c == EOF)
            break;
        /* room for this byte and the NUL */
        if (*cap - len < 2 || *cap < 2) {
            size_t want = *cap < 64 ? 128 : *cap * 2;
            char *p;
            if (want <= *cap || want > (size_t)PTRDIFF_MAX) {
                errno = EOVERFLOW;
                failed = 1;
                break;
            }
            p = (char *)realloc(*line, want);
            if (!p) {
                errno = ENOMEM;
                failed = 1;
                break;
            }
            *line = p;
            *cap = want;
        }
        (*line)[len++] = (char)c;
        if (c == (unsigned char)delim)
            break;
    }
    PF__UNLOCK_FILE(f);

    if (failed || len == 0)
        return -1;
    (*line)[len] = '\0';
    return (pf_ssize_t)len;
}

PF_API pf_ssize_t pf_getdelim(char **line, size_t *cap, int delim, FILE *f) {
#if PF_CONFIG_HAVE_GETDELIM
    return getdelim(line, cap, delim, f);
#else
    return pf__getdelim_portable(line, cap, delim, f);
#endif
}

PF_API pf_ssize_t pf_getline(char **line, size_t *cap, FILE *f) {
    return pf_getdelim(line, cap, '\n', f);
}

/* ---- Environment ------------------------------------------------------ */

static int pf__env_name_ok(const char *name) {
    return name && *name && !strchr(name, '=');
}

PF_API ptrdiff_t pf_getenv(const char *name, char *buf, size_t size) {
    if (!pf__env_name_ok(name))
        return PF_EINVAL;
#if PF_OS_WINDOWS
    {
        pf__wstr w;
        const wchar_t *value;
        ptrdiff_t n;
        int rc = pf__wstr_from_utf8(&w, name);

        if (rc) {
            pf__wstr_free(&w);
            return rc;
        }
        value = _wgetenv((const wchar_t *)w.ptr);
        pf__wstr_free(&w);
        if (!value)
            return PF_ENOENT;
        n = pf_wcs_to_utf8(value, PF_NTS, buf, buf ? size : 0, 0);
        return n;
    }
#else
    {
        const char *value = getenv(name);
        size_t len;

        if (!value)
            return PF_ENOENT;
        len = strlen(value);
        if (buf) {
            if (size <= len)
                return PF_ERANGE;
            memcpy(buf, value, len + 1);
        }
        return (ptrdiff_t)len;
    }
#endif
}

#if PF_OS_WINDOWS
/* _wputenv_s for UTF-8 strings. An empty value removes the variable. */
static int pf__putenv_utf8(const char *name, const char *value) {
    pf__wstr wname, wvalue;
    int rc = pf__wstr_from_utf8(&wname, name);

    if (rc == 0)
        rc = pf__wstr_from_utf8(&wvalue, value);
    else
        pf__wstr_from_utf8(&wvalue, "");
    if (rc == 0
        && _wputenv_s((const wchar_t *)wname.ptr, (const wchar_t *)wvalue.ptr)
            != 0)
        rc = PF_ENOMEM;
    pf__wstr_free(&wname);
    pf__wstr_free(&wvalue);
    if (rc) {
        errno = rc == PF_ENOMEM ? ENOMEM : EINVAL;
        return -1;
    }
    return 0;
}
#endif

PF_API int pf_setenv(const char *name, const char *value, int overwrite) {
    if (!pf__env_name_ok(name) || !value) {
        errno = EINVAL;
        return -1;
    }
#if PF_OS_WINDOWS
    if (!overwrite && pf_getenv(name, NULL, 0) != PF_ENOENT)
        return 0;
    return pf__putenv_utf8(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}

PF_API int pf_unsetenv(const char *name) {
    if (!pf__env_name_ok(name)) {
        errno = EINVAL;
        return -1;
    }
#if PF_OS_WINDOWS
    return pf__putenv_utf8(name, "");
#else
    return unsetenv(name);
#endif
}

#if PF_OS_APPLE
    #include <crt_externs.h>
    #define PF__ENVIRON (*_NSGetEnviron())
#elif !PF_OS_WINDOWS
extern char **environ;
    #define PF__ENVIRON environ
#endif

/* Calls `fn` on each "NAME=VALUE" entry of `block` (entries separated by
   NULs, `n` bytes), splitting it in place. */
static int pf__env_each(
    char *block,
    size_t n,
    int (*fn)(const char *name, const char *value, void *user),
    void *user
) {
    char *p = block, *end = block + n;
    while (p < end) {
        size_t len = strlen(p);
        /* Windows keeps per-drive directories as "=C:=C:\..." */
        char *eq = strchr(p + (*p == '='), '=');
        if (eq && eq != p) {
            int rc;
            *eq = '\0';
            rc = fn(p, eq + 1, user);
            if (rc)
                return rc;
        }
        p += len + 1;
    }
    return 0;
}

PF_API int pf_env_foreach(
    int (*fn)(const char *name, const char *value, void *user), void *user
) {
    char *block;
    size_t n = 0;
    int rc;

    if (!fn)
        return PF_EINVAL;
    /* A copy, so `fn` may change the environment. */
#if PF_OS_WINDOWS
    {
        wchar_t *env = GetEnvironmentStringsW(), *w;
        size_t units;
        ptrdiff_t len;
        if (!env)
            return pf_error_last_win32();
        for (w = env; *w; w += wcslen(w) + 1)
            ;
        units = (size_t)(w - env);
        len = pf_utf16_to_utf8(
            (const uint16_t *)env, units, NULL, 0, PF_UNICODE_REPLACE
        );
        block = len >= 0 ? (char *)malloc((size_t)len + 1) : NULL;
        if (block)
            len = pf_utf16_to_utf8(
                (const uint16_t *)env, units, block, (size_t)len + 1,
                PF_UNICODE_REPLACE
            );
        FreeEnvironmentStringsW(env);
        if (!block)
            return len < 0 ? (int)len : PF_ENOMEM;
        if (len < 0) {
            free(block);
            return (int)len;
        }
        n = (size_t)len;
    }
#else
    {
        char **e = PF__ENVIRON;
        size_t k;
        for (k = 0; e && e[k]; k++)
            n += strlen(e[k]) + 1;
        block = (char *)malloc(n + 1);
        if (!block)
            return PF_ENOMEM;
        n = 0;
        for (k = 0; e && e[k]; k++) {
            size_t len = strlen(e[k]) + 1;
            memcpy(block + n, e[k], len);
            n += len;
        }
    }
#endif
    rc = pf__env_each(block, n, fn, user);
    free(block);
    return rc;
}

/* ---- qsort_r ---------------------------------------------------------- */

static void pf__swap(unsigned char *a, unsigned char *b, size_t size) {
    while (size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

/* Heapsort: no recursion and no allocation, O(n log n) in every case.
   Used where the system has no usable qsort_r (and by the tests). */
void pf__qsort_r_portable(
    void *base,
    size_t n,
    size_t size,
    int (*cmp)(const void *, const void *, void *),
    void *arg
) {
    unsigned char *a = (unsigned char *)base;
    size_t start, end, root, child;

    if (n < 2 || size == 0)
        return;

    /* build a max-heap, then move the maximum to the end, n - 1 times */
    start = n / 2;
    end = n;
    while (end > 1) {
        if (start > 0) {
            start--;
        } else {
            end--;
            pf__swap(a, a + end * size, size);
        }
        root = start;
        while ((child = 2 * root + 1) < end) {
            if (child + 1 < end
                && cmp(a + child * size, a + (child + 1) * size, arg) < 0)
                child++;
            if (cmp(a + root * size, a + child * size, arg) >= 0)
                break;
            pf__swap(a + root * size, a + child * size, size);
            root = child;
        }
    }
}

#if !PF_CONFIG_HAVE_QSORT_R && (PF__HAVE_QSORT_R_BSD || PF_OS_WINDOWS)
struct pf__qsort_thunk {
    int (*cmp)(const void *, const void *, void *);
    void *arg;
};

static int pf__qsort_tramp(void *thunk, const void *a, const void *b) {
    struct pf__qsort_thunk *t = (struct pf__qsort_thunk *)thunk;
    return t->cmp(a, b, t->arg);
}
#endif

PF_API void pf_qsort_r(
    void *base,
    size_t n,
    size_t size,
    int (*cmp)(const void *a, const void *b, void *arg),
    void *arg
) {
#if PF_CONFIG_HAVE_QSORT_R
    qsort_r(base, n, size, cmp, arg);
#elif PF__HAVE_QSORT_R_BSD
    struct pf__qsort_thunk t;
    t.cmp = cmp;
    t.arg = arg;
    qsort_r(base, n, size, &t, pf__qsort_tramp);
#elif PF_OS_WINDOWS
    struct pf__qsort_thunk t;
    t.cmp = cmp;
    t.arg = arg;
    qsort_s(base, n, size, pf__qsort_tramp, &t);
#else
    pf__qsort_r_portable(base, n, size, cmp, arg);
#endif
}
