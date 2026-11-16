/** cpolyfill - Filling the gaps between C standards and compilers

    String functions that are missing from some C libraries, with the same
    behaviour everywhere. The functions that return new memory use
    `malloc`; release it with `free`.

        char *pf_strdup(const char *s)
        char *pf_strndup(const char *s, size_t n)
                A copy of `s` (at most `n` bytes of it, plus a NUL), or NULL
                with errno ENOMEM.
        size_t pf_strnlen(const char *s, size_t max)
                strlen(s), but at most `max`. 0 for a NULL `s`.
        size_t pf_strlcpy(char *dst, const char *src, size_t size)
        size_t pf_strlcat(char *dst, const char *src, size_t size)
                BSD copy and append: `dst` is NUL-terminated if `size` > 0
                (for strlcat: if it held a NUL within `size`). Return the
                length of the string they tried to create; truncation
                happened if it is >= `size`.
        int pf_strcasecmp(const char *a, const char *b)
        int pf_strncasecmp(const char *a, const char *b, size_t n)
                Compare ignoring the case of ASCII letters (other bytes
                compare as `unsigned char`, whatever the locale).
        char *pf_strtok_r(char *s, const char *delim, char **save)
                Reentrant strtok.
        char *pf_strsep(char **sp, const char *delim)
                The token at `*sp` up to the first byte of `delim` (which
                is overwritten with a NUL), advancing `*sp` past it, or to
                NULL at the end. Empty tokens are returned.
        char *pf_strchrnul(const char *s, int c)
                strchr, but a pointer to the NUL instead of NULL.
        void *pf_memmem(const void *hay, size_t n, const void *needle,
                        size_t m)
                The first occurrence of `needle` in `hay`, or NULL. An
                empty `needle` is found at `hay`.
        void *pf_memrchr(const void *s, int c, size_t n)
                The last byte `c` in `s[0..n)`, or NULL.
        void *pf_mempcpy(void *dst, const void *src, size_t n)
                memcpy returning `dst + n`.
        char *pf_stpcpy(char *dst, const char *src)
                strcpy returning a pointer to the NUL written.
        char *pf_stpncpy(char *dst, const char *src, size_t n)
                strncpy returning a pointer to the first NUL written, or
                `dst + n` if none.
        void *pf_memccpy(void *dst, const void *src, int c, size_t n)
                Copy up to and including the first byte `c`; returns the
                byte after it in `dst`, or NULL if `c` was not found.
        void *pf_memset_explicit(void *s, int c, size_t n)
                memset that is not optimised away (C23), for wiping
                secrets.
        int pf_asprintf(char **strp, const char *fmt, ...)
        int pf_vasprintf(char **strp, const char *fmt, va_list ap)
                sprintf into new memory. Return the length, or -1 with
                `*strp` set to NULL.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_STRING: every
    function above under its name without `pf_`, but only where the library
    found it missing when it was built (or deprecated, like MSVC's
    `strdup`). Where the system has one, its declaration is used, so the
    consumer's feature macros (_GNU_SOURCE, _POSIX_C_SOURCE, ...) still
    decide whether it is visible.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_STRING_H
#define CPOLYFILL_STRING_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

PF_BEGIN_DECLS

PF_MALLOC_LIKE PF_API char *pf_strdup(const char *s);
PF_MALLOC_LIKE PF_API char *pf_strndup(const char *s, size_t n);
PF_API size_t pf_strnlen(const char *s, size_t max);
PF_API size_t pf_strlcpy(char *dst, const char *src, size_t size);
PF_API size_t pf_strlcat(char *dst, const char *src, size_t size);
PF_API int pf_strcasecmp(const char *a, const char *b);
PF_API int pf_strncasecmp(const char *a, const char *b, size_t n);
PF_API char *pf_strtok_r(char *s, const char *delim, char **save);
PF_API char *pf_strsep(char **sp, const char *delim);
PF_API char *pf_strchrnul(const char *s, int c);
PF_API void *pf_memmem(
    const void *hay, size_t n, const void *needle, size_t m
);
PF_API void *pf_memrchr(const void *s, int c, size_t n);
PF_API void *pf_mempcpy(void *dst, const void *src, size_t n);
PF_API char *pf_stpcpy(char *dst, const char *src);
PF_API char *pf_stpncpy(char *dst, const char *src, size_t n);
PF_API void *pf_memccpy(void *dst, const void *src, int c, size_t n);
PF_API void *pf_memset_explicit(void *s, int c, size_t n);
PF_PRINTF(2, 3) PF_API int pf_asprintf(char **strp, const char *fmt, ...);
PF_PRINTF(2, 0)
PF_API int pf_vasprintf(char **strp, const char *fmt, va_list ap);

PF_END_DECLS

#endif /* CPOLYFILL_STRING_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__STRING_STD_NAMES)                                    \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_STRING))   \
        || defined(PF_STD_NAMES_STRING))
    #define PF__STRING_STD_NAMES

    /* The system declarations come first, so a later include of these
       headers does not redeclare a mapped name. */
    #include <stdio.h>
    #if PF_CONFIG_HAVE_STRINGS_H
        #include <strings.h>
    #endif

    #ifdef PF__FORCE_STD_NAMES_POLYFILL
        #define PF__STR_MAP(have) 1
    #else
        #define PF__STR_MAP(have) (!(have))
    #endif

    #if PF__STR_MAP(PF_CONFIG_HAVE_STRDUP)
        #undef strdup
        #define strdup pf_strdup
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRNDUP)
        #undef strndup
        #define strndup pf_strndup
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRNLEN)
        #undef strnlen
        #define strnlen pf_strnlen
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRLCPY)
        #undef strlcpy
        #define strlcpy pf_strlcpy
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRLCAT)
        #undef strlcat
        #define strlcat pf_strlcat
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRCASECMP)
        #undef strcasecmp
        #define strcasecmp pf_strcasecmp
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRNCASECMP)
        #undef strncasecmp
        #define strncasecmp pf_strncasecmp
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRTOK_R)
        #undef strtok_r
        #define strtok_r pf_strtok_r
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRSEP)
        #undef strsep
        #define strsep pf_strsep
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STRCHRNUL)
        #undef strchrnul
        #define strchrnul pf_strchrnul
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_MEMMEM)
        #undef memmem
        #define memmem pf_memmem
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_MEMRCHR)
        #undef memrchr
        #define memrchr pf_memrchr
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_MEMPCPY)
        #undef mempcpy
        #define mempcpy pf_mempcpy
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STPCPY)
        #undef stpcpy
        #define stpcpy pf_stpcpy
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_STPNCPY)
        #undef stpncpy
        #define stpncpy pf_stpncpy
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_MEMCCPY)
        #undef memccpy
        #define memccpy pf_memccpy
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_MEMSET_EXPLICIT)
        #undef memset_explicit
        #define memset_explicit pf_memset_explicit
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_ASPRINTF)
        #undef asprintf
        #define asprintf pf_asprintf
    #endif
    #if PF__STR_MAP(PF_CONFIG_HAVE_VASPRINTF)
        #undef vasprintf
        #define vasprintf pf_vasprintf
    #endif
#endif
