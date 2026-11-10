/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The library's own implementations are used everywhere (so they are what
   the tests check), except memmem and memset_explicit, which call the
   system's where it has one. */

#ifndef __STDC_WANT_LIB_EXT1__
    #define __STDC_WANT_LIB_EXT1__ 1 /* memset_s */
#endif

#include "internal.h"

#include <cpolyfill/ctype.h>
#include <cpolyfill/string.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#endif

#ifndef PF__HAVE_EXPLICIT_BZERO
    #define PF__HAVE_EXPLICIT_BZERO 0
#endif
#ifndef PF__HAVE_MEMSET_S
    #define PF__HAVE_MEMSET_S 0
#endif

static char *pf__dup(const char *s, size_t len) {
    char *p = (char *)malloc(len + 1);

    if (!p) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

PF_API char *pf_strdup(const char *s) { return pf__dup(s, strlen(s)); }

PF_API char *pf_strndup(const char *s, size_t n) {
    return pf__dup(s, pf_strnlen(s, n));
}

/* A loop rather than memchr: `max` may exceed the size of the object. */
PF_API size_t pf_strnlen(const char *s, size_t max) {
    size_t n = 0;

    if (!s)
        return 0;
    while (n < max && s[n])
        n++;
    return n;
}

PF_API size_t pf_strlcpy(char *dst, const char *src, size_t size) {
    size_t len = strlen(src);

    if (size) {
        size_t n = len < size ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}

PF_API size_t pf_strlcat(char *dst, const char *src, size_t size) {
    size_t used = pf_strnlen(dst, size);

    /* no NUL within `size`: nothing is appended (BSD behaviour) */
    if (used == size)
        return size + strlen(src);
    return used + pf_strlcpy(dst + used, src, size - used);
}

PF_API int pf_strcasecmp(const char *a, const char *b) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    int d;

    for (;; p++, q++) {
        d = pf_tolower(*p) - pf_tolower(*q);
        if (d || !*p)
            return d;
    }
}

PF_API int pf_strncasecmp(const char *a, const char *b, size_t n) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    int d;

    for (; n; n--, p++, q++) {
        d = pf_tolower(*p) - pf_tolower(*q);
        if (d || !*p)
            return d;
    }
    return 0;
}

PF_API char *pf_strtok_r(char *s, const char *delim, char **save) {
    char *end;

    if (!s)
        s = *save;
    if (!s)
        return NULL;
    s += strspn(s, delim);
    if (!*s) {
        *save = s;
        return NULL;
    }
    end = s + strcspn(s, delim);
    if (*end)
        *end++ = '\0';
    *save = end;
    return s;
}

PF_API char *pf_strsep(char **sp, const char *delim) {
    char *s = *sp, *end;

    if (!s)
        return NULL;
    end = s + strcspn(s, delim);
    if (*end) {
        *end = '\0';
        *sp = end + 1;
    } else {
        *sp = NULL;
    }
    return s;
}

PF_API char *pf_strchrnul(const char *s, int c) {
    char ch = (char)c;

    while (*s && *s != ch)
        s++;
    return (char *)s;
}

PF_API void *pf_memmem(
    const void *hay, size_t n, const void *needle, size_t m
) {
#if PF_CONFIG_HAVE_MEMMEM
    if (m == 0)
        return (void *)hay; /* older glibc and musl agree, but be sure */
    return memmem(hay, n, needle, m);
#else
    const unsigned char *h = (const unsigned char *)hay;
    const unsigned char *nd = (const unsigned char *)needle;
    const unsigned char *last;

    if (m == 0)
        return (void *)hay;
    if (m > n)
        return NULL;
    last = h + (n - m);
    while (h <= last) {
        h = (const unsigned char *)memchr(h, nd[0], (size_t)(last - h) + 1);
        if (!h)
            return NULL;
        if (memcmp(h + 1, nd + 1, m - 1) == 0)
            return (void *)h;
        h++;
    }
    return NULL;
#endif
}

PF_API void *pf_memrchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s + n;
    unsigned char ch = (unsigned char)c;

    while (n--) {
        if (*--p == ch)
            return (void *)p;
    }
    return NULL;
}

PF_API void *pf_mempcpy(void *dst, const void *src, size_t n) {
    if (n)
        memcpy(dst, src, n);
    return (char *)dst + n;
}

PF_API char *pf_stpcpy(char *dst, const char *src) {
    size_t len = strlen(src);

    memcpy(dst, src, len + 1);
    return dst + len;
}

PF_API char *pf_stpncpy(char *dst, const char *src, size_t n) {
    size_t len = pf_strnlen(src, n);

    memcpy(dst, src, len);
    memset(dst + len, 0, n - len);
    return dst + len;
}

PF_API void *pf_memccpy(void *dst, const void *src, int c, size_t n) {
    const unsigned char *s = (const unsigned char *)src;
    const unsigned char *hit;
    size_t len;

    hit = (const unsigned char *)memchr(s, (unsigned char)c, n);
    len = hit ? (size_t)(hit - s) + 1 : n;
    if (len)
        memcpy(dst, src, len);
    return hit ? (char *)dst + len : NULL;
}

#if !PF_OS_WINDOWS && !PF__HAVE_EXPLICIT_BZERO && !PF__HAVE_MEMSET_S
/* Called through a volatile pointer, so the compiler cannot know it is
   memset and drop the call. */
static void *(*volatile pf__memset_ptr)(void *, int, size_t) = memset;
#endif

PF_API void *pf_memset_explicit(void *s, int c, size_t n) {
    if (n == 0)
        return s;
#if PF_OS_WINDOWS
    if (c == 0) {
        SecureZeroMemory(s, n);
    } else {
        volatile unsigned char *p = (volatile unsigned char *)s;
        while (n--)
            *p++ = (unsigned char)c;
    }
#elif PF__HAVE_MEMSET_S
    memset_s(s, n, c, n);
#elif PF__HAVE_EXPLICIT_BZERO
    memset(s, c, n);
    if (c == 0) {
        explicit_bzero(s, n);
    } else {
        /* a compiler barrier: the memory is assumed to be read */
        __asm__ __volatile__("" : : "r"(s) : "memory");
    }
#else
    pf__memset_ptr(s, c, n);
#endif
    return s;
}

PF_API int pf_vasprintf(char **strp, const char *fmt, va_list ap) {
    va_list copy;
    char small[256];
    char *p;
    int len;

    va_copy(copy, ap);
    len = vsnprintf(small, sizeof(small), fmt, copy);
    va_end(copy);
    if (len < 0 || len == INT_MAX) {
        *strp = NULL;
        return -1;
    }

    p = (char *)malloc((size_t)len + 1);
    if (!p) {
        errno = ENOMEM;
        *strp = NULL;
        return -1;
    }
    if ((size_t)len < sizeof(small)) {
        memcpy(p, small, (size_t)len + 1);
    } else if (vsnprintf(p, (size_t)len + 1, fmt, ap) != len) {
        free(p);
        *strp = NULL;
        return -1;
    }
    *strp = p;
    return len;
}

PF_API int pf_asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = pf_vasprintf(strp, fmt, ap);
    va_end(ap);
    return len;
}
