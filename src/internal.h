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

/* Pass as a length to convert up to the terminating NUL. */
#define PF__NTS ((size_t)-1)

/* Convert `len` code units of `src` (or up to its NUL with PF__NTS). The
   output is always NUL-terminated. Returns the number of code units of the
   converted string without the NUL, or a negative PF_E* code:
     PF_EILSEQ   the input is not valid UTF-8 / UTF-16 (overlong forms,
                 surrogates encoded in UTF-8, unpaired surrogates, ...)
     PF_ERANGE   `cap` is too small; nothing useful was written.
   With `dst == NULL` only the length is computed. */
ptrdiff_t pf__utf8_to_utf16(
    const char *src, size_t len, uint16_t *dst, size_t cap
);
ptrdiff_t pf__utf16_to_utf8(
    const uint16_t *src, size_t len, char *dst, size_t cap
);

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

/* ---- Windows errors --------------------------------------------------- */

#if PF_OS_WINDOWS
int pf__error_from_win32(unsigned long err);
int pf__error_from_wsa(int err);
int pf__error_last_win32(void); /* pf__error_from_win32(GetLastError()) */
#endif

#endif /* CPOLYFILL_INTERNAL_H */
