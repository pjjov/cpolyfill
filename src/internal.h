/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Internal helpers shared by the library sources. Not installed.
    Every source file includes this header before anything else, so the
    feature macros below take effect for the system headers.
*/

#ifndef CPOLYFILL_INTERNAL_H
#define CPOLYFILL_INTERNAL_H

/* ---- System header configuration -------------------------------------- */

#if defined(_WIN32) && !defined(__CYGWIN__)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    /* Use the wide (UTF-16) API by default, never the ANSI code page. */
    #ifndef UNICODE
        #define UNICODE
    #endif
    #ifndef _UNICODE
        #define _UNICODE
    #endif
    /* Windows 10. Tentative, see "Open questions" in REWRITE.md. */
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif
#else
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifndef PF__BUILDING
    #define PF__BUILDING
#endif

#include <cpolyfill/errno.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/unicode.h>

#include <stddef.h>
#include <stdint.h>

/* ---- Allocation ------------------------------------------------------- */

/* The allocator used when an `allocator_t *` argument is NULL. Follows the
   `allocator_fn` contract: an allocation with `align` (zalign & ~1) above
   the default alignment must be reallocated with the same alignment. */
void *pf__default_allocate(void *ptr, size_t old, size_t size, size_t zalign);

#define allocator_default(m_ptr, m_old, m_size, m_zalign)    \
    pf__default_allocate((m_ptr), (m_old), (m_size), (m_zalign))

#include "allocator.h"

/* ---- UTF-8 <-> UTF-16 ------------------------------------------------- */

/* Decodes the UTF-8 sequence at `s[0..n)` (n > 0): returns the bytes it
   spans and sets `*cp`, or -1 with the length of the maximal invalid
   subpart. */
size_t pf__utf8_next(const unsigned char *s, size_t n, long *cp);
/* Encodes a valid code point, returns the number of bytes (1-4). */
size_t pf__utf8_put(uint32_t cp, unsigned char *out);

/* A UTF-16 copy of a UTF-8 string, on the stack for short strings (most
   paths) and on the heap for long ones:

       pf__wstr w;
       int rc = pf__wstr_from_utf8(&w, path);
       if (rc) return rc;
       CreateFileW((const wchar_t *)w.ptr, ...);
       pf__wstr_free(&w);
*/
#define PF__WSTR_SMALL 261

typedef struct pf__wstr {
    uint16_t *ptr;
    size_t heap_size; /* bytes allocated on the heap, 0 if `ptr == small` */
    uint16_t small[PF__WSTR_SMALL];
} pf__wstr;

int pf__wstr_from_utf8(pf__wstr *out, const char *src);
void pf__wstr_free(pf__wstr *str);

/* ---- uchar fallbacks (uchar.c) ---------------------------------------- */

/* The state of the UTF-8 / UTF-16 layer, kept inside the caller's
   mbstate_t. All zero is the initial state. */
enum {
    PF__MBEXT_NONE = 0,
    PF__MBEXT_C16_OUT,  /* mbrtoc16: low surrogate `value` still to give */
    PF__MBEXT_C16_IN,   /* c16rtomb: high surrogate `value` received */
    PF__MBEXT_C8_OUT,   /* mbrtoc8: byte `count` of `value` still to give */
    PF__MBEXT_UTF8_IN   /* partial UTF-8 input: `seen` bytes of `lead`,
                           `count` to go, bits so far in `value` */
};

typedef struct pf__mbext {
    uint32_t value;
    unsigned char kind;
    unsigned char count;
    unsigned char lead;
    unsigned char seen;
} pf__mbext;

typedef struct pf__mbconv {
    mbstate_t *inner; /* the mbrtowc / wcrtomb state (wc backend) */
    pf__mbext ext;
    int utf8;         /* 1: assume UTF-8, 0: through mbrtowc / wcrtomb */
} pf__mbconv;

size_t pf__mbrtoc8_with(
    pf__mbconv *cv, unsigned char *pc8, const char *s, size_t n
);
size_t pf__mbrtoc16_with(
    pf__mbconv *cv, uint16_t *pc16, const char *s, size_t n
);
size_t pf__mbrtoc32_with(
    pf__mbconv *cv, uint32_t *pc32, const char *s, size_t n
);
size_t pf__c8rtomb_with(pf__mbconv *cv, char *s, unsigned char c8);
size_t pf__c16rtomb_with(pf__mbconv *cv, char *s, uint16_t c16);
size_t pf__c32rtomb_with(pf__mbconv *cv, char *s, uint32_t c32);

/* ---- Portable fallbacks, also run by the tests ------------------------ */

#include <cpolyfill/stdlib.h>

pf_ssize_t pf__getdelim_portable(
    char **line, size_t *cap, int delim, FILE *f
);
void pf__qsort_r_portable(
    void *base,
    size_t n,
    size_t size,
    int (*cmp)(const void *, const void *, void *),
    void *arg
);

/* timegm on every platform, without the system's (time.c). */
#include <time.h>
time_t pf__timegm_portable(struct tm *tm);

#endif /* CPOLYFILL_INTERNAL_H */
