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
    /* 64-bit off_t on 32-bit systems (pf_off_t is always 64 bits). */
    #ifndef _FILE_OFFSET_BITS
        #define _FILE_OFFSET_BITS 64
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

/* WTF-8 (paths on Windows): UTF-8 that also carries unpaired surrogates,
   so every Windows file name converts to bytes and back. The conversions
   behave like pf_utf8_to_utf16 / pf_utf16_to_utf8 with flags 0 otherwise. */
#define PF__UNICODE_WTF8 0x80000000u
ptrdiff_t pf__wtf8_to_utf16(
    const char *src, size_t len, uint16_t *dst, size_t cap
);
ptrdiff_t pf__utf16_to_wtf8(
    const uint16_t *src, size_t len, char *dst, size_t cap
);
int pf__wstr_from_wtf8(pf__wstr *out, const char *src);

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

/* ---- Threads and atomics (threads.c, atomic.c) ------------------------ */

#include <stdbool.h>

/* Spin locks on a 32-bit word, 0 when free. */
bool pf__spin_trylock(volatile uint32_t *lock);
void pf__spin_lock(volatile uint32_t *lock);
void pf__spin_unlock(volatile uint32_t *lock);
void pf__cpu_relax(void);
void pf__thread_yield(void);

/* The lock-table paths of the atomic backend, for any size. */
void *pf__atomic_load_locked(const volatile void *obj, size_t size, void *ret);
void pf__atomic_store_locked(volatile void *obj, size_t size, const void *val);
void *pf__atomic_exchange_locked(
    volatile void *obj, size_t size, const void *val, void *ret
);
bool pf__atomic_cas_locked(
    volatile void *obj, size_t size, void *expected, const void *desired
);
void *pf__atomic_fetch_locked(
    volatile void *obj, size_t size, int op, unsigned long long arg, void *ret
);

/* pf_atomic_wait / wake on a table of condition variables, used where the
   system has no futex-like call. */
struct timespec;
int pf__atomic_wait_fallback(
    const volatile void *addr, uint32_t expected, const struct timespec *timeout
);
void pf__atomic_wake_fallback(const volatile void *addr, bool all);

/* The library's own semaphore (semaphore.c), used on macOS and Windows,
   and tested everywhere. -1 with errno, like the sem_* functions. */
struct pf__sem_local;
int pf__sem_local_init(struct pf__sem_local *s, unsigned value);
int pf__sem_local_destroy(struct pf__sem_local *s);
int pf__sem_local_wait(struct pf__sem_local *s, const struct timespec *deadline);
int pf__sem_local_trywait(struct pf__sem_local *s);
int pf__sem_local_post(struct pf__sem_local *s);
int pf__sem_local_getvalue(struct pf__sem_local *s, int *value);

/* The length of the longest prefix of `s` of at most `max` bytes that ends
   at a character boundary. */
size_t pf__utf8_truncate(const char *s, size_t max);
/* Copies a thread name into `buf`: 0, or PF_ERANGE if truncated. */
int pf__copy_name(char *buf, size_t size, const char *name);

/* ---- Files (fs, dirent, io, dlfcn, mman) ----------------------------- */

/* Sets errno to the errno value of the PF_E* `code` and returns -1, for
   the stand-ins that follow the POSIX convention. */
int pf__errno_fail(int code);

/* Copies `len` bytes of `s` and a NUL into `buf`, for the functions
   returning a length (fs.c): `len`, or PF_ERANGE if `size` is too small;
   with `buf == NULL` just `len`. */
ptrdiff_t pf__copy_out(char *buf, size_t size, const char *s, size_t len);

/* mkstemp (`dir` false: returns the descriptor) and mkdtemp (returns 0)
   on pf_open / pf_mkdir, creating with `mode` (less the umask): -1 with
   errno on failure (fs.c). */
int pf__mktemp(char *tmpl, bool dir, unsigned mode);

#if PF_OS_WINDOWS
/* A path for the W APIs (win32/path.c): WTF-8 to UTF-16, '/' to '\', and
   the \\?\ prefix (on the absolute path) when it is too long for the
   classic limit. 0 or a PF_E* code. */
int pf__wpath(pf__wstr *out, const char *path);
/* The UTF-16 path `w` (`len` units or PF_NTS) as WTF-8 in `buf`, with a
   \\?\ or \\?\UNC\ prefix removed: the length, or PF_ERANGE (with
   `buf == NULL` only the length). */
ptrdiff_t pf__path_from_wide(
    const uint16_t *w, size_t len, char *buf, size_t size
);
/* errno from GetLastError(), returns -1. */
int pf__errno_win32(void);
#endif

/* Milliseconds until the UTC `deadline` (rounded up, at most INT_MAX), 0
   if it passed, -1 without one (socket.c). */
int pf__deadline_ms(const struct timespec *deadline);

/* timegm on every platform, without the system's (time.c). */
#include <time.h>
time_t pf__timegm_portable(struct tm *tm);

#if !PF_OS_WINDOWS
/* Makes `p` track a child made with fork, so pf_process_wait (with a
   deadline), pf_process_kill and pf_process_free work on it
   (posix/process.c, used by test.c). */
struct pf_process_t;
int pf__process_adopt(struct pf_process_t *p, long pid);
#endif

#endif /* CPOLYFILL_INTERNAL_H */
