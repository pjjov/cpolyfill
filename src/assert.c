/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/assert.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#elif PF_OS_LINUX || PF_OS_CYGWIN
    #include <errno.h> /* program_invocation_short_name */
#endif

#if !defined(__STDC_NO_ATOMICS__)
    #include <stdatomic.h>
#endif

/* ---- Program name ----------------------------------------------------- */

/* The base name of the program, as glibc's assert prints it, or "". */
static const char *pf__progname(char *buf, size_t size) {
#if PF_OS_WINDOWS
    wchar_t wide[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, wide, MAX_PATH);
    const char *base = buf;
    char *p;
    if (n == 0 || n >= MAX_PATH
        || pf__utf16_to_utf8((const uint16_t *)wide, n, buf, size) < 0)
        return "";
    for (p = buf; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    return base;
#elif (PF_OS_LINUX || PF_OS_CYGWIN) && defined(_GNU_SOURCE)
    (void)buf, (void)size;
    return program_invocation_short_name;
#elif PF_OS_APPLE || PF_OS_BSD
    const char *name = getprogname();
    (void)buf, (void)size;
    return name ? name : "";
#else
    (void)buf, (void)size;
    return "";
#endif
}

/* ---- Handler ---------------------------------------------------------- */

static void pf__assert_default(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    char buf[512];
    const char *prog = pf__progname(buf, sizeof(buf));

    fprintf(
        stderr,
        "%s%s%s:%d: %s%sAssertion `%s' failed%s%s\n",
        prog,
        *prog ? ": " : "",
        file,
        line,
        func ? func : "",
        func ? ": " : "",
        expr,
        message ? ": " : ".",
        message ? message : ""
    );
    fflush(stderr);
}

#if !defined(__STDC_NO_ATOMICS__)
static _Atomic(pf_assert_handler_fn) pf__assert_handler = NULL;

static pf_assert_handler_fn pf__handler_swap(pf_assert_handler_fn fn) {
    return atomic_exchange(&pf__assert_handler, fn);
}

static pf_assert_handler_fn pf__handler_get(void) {
    return atomic_load(&pf__assert_handler);
}
#elif PF_OS_WINDOWS
static void *volatile pf__assert_handler = NULL;

static pf_assert_handler_fn pf__handler_swap(pf_assert_handler_fn fn) {
    return (pf_assert_handler_fn)InterlockedExchangePointer(
        (void *volatile *)&pf__assert_handler, (void *)fn
    );
}

static pf_assert_handler_fn pf__handler_get(void) {
    return (pf_assert_handler_fn)InterlockedCompareExchangePointer(
        &pf__assert_handler, NULL, NULL
    );
}
#else
static volatile pf_assert_handler_fn pf__assert_handler = NULL;

static pf_assert_handler_fn pf__handler_swap(pf_assert_handler_fn fn) {
    pf_assert_handler_fn old = pf__assert_handler;
    pf__assert_handler = fn;
    return old;
}

static pf_assert_handler_fn pf__handler_get(void) { return pf__assert_handler; }
#endif

PF_API pf_assert_handler_fn pf_set_assert_handler(pf_assert_handler_fn fn) {
    pf_assert_handler_fn old = pf__handler_swap(fn);
    return old ? old : pf__assert_default;
}

/* No guard against a handler failing an assertion: it could not tell that
   apart from a handler that left with longjmp before. */
PF_NORETURN static void pf__assert_report(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    pf_assert_handler_fn fn = pf__handler_get();

    (fn ? fn : pf__assert_default)(expr, file, line, func, message);
    abort();
}

/* ---- Failures --------------------------------------------------------- */

/* Formats into `buf`, ending with "..." when truncated. */
static const char *pf__vformat(
    char *buf, size_t size, const char *fmt, va_list ap
) {
    int n = vsnprintf(buf, size, fmt, ap);
    if (n < 0)
        return fmt;
    if ((size_t)n >= size)
        memcpy(buf + size - 4, "...", 4);
    return buf;
}

PF_PRINTF(3, 4)
static const char *pf__format(char *buf, size_t size, const char *fmt, ...) {
    const char *s;
    va_list ap;
    va_start(ap, fmt);
    s = pf__vformat(buf, size, fmt, ap);
    va_end(ap);
    return s;
}

PF_API void pf_assert_fail(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
) {
    char buf[1024];
    const char *message = NULL;

    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        message = pf__vformat(buf, sizeof(buf), fmt, ap);
        va_end(ap);
    }
    pf__assert_report(expr, file, line, func, message);
}

/* The relation that does hold, for each failed one. */
static const char *pf__negated(int op) {
    switch (op) {
    case PF__ASSERT_EQ:
        return "!=";
    case PF__ASSERT_NE:
        return "==";
    case PF__ASSERT_LT:
        return ">=";
    case PF__ASSERT_LE:
        return ">";
    case PF__ASSERT_GT:
        return "<=";
    default:
        return "<";
    }
}

static void pf__int_str(
    char *buf, size_t size, bool sig, unsigned long long v
) {
    if (sig && v > (unsigned long long)LLONG_MAX)
        snprintf(buf, size, "-%llu", 0 - v);
    else
        snprintf(buf, size, "%llu", v);
}

PF_API void pf__assert_fail_int(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    bool a_signed,
    unsigned long long a,
    bool b_signed,
    unsigned long long b
) {
    char sa[32], sb[32], buf[96];

    pf__int_str(sa, sizeof(sa), a_signed, a);
    pf__int_str(sb, sizeof(sb), b_signed, b);
    pf__assert_report(
        expr,
        file,
        line,
        func,
        pf__format(buf, sizeof(buf), "%s %s %s", sa, pf__negated(op), sb)
    );
}

/* A string quoted, or "NULL". */
static void pf__quote(char *buf, size_t size, const char *s) {
    if (!s)
        pf__format(buf, size, "NULL");
    else
        pf__format(buf, size, "\"%s\"", s);
}

PF_API void pf__assert_fail_str(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const char *a,
    const char *b
) {
    char qa[400], qb[400], buf[820];

    pf__quote(qa, sizeof(qa), a);
    pf__quote(qb, sizeof(qb), b);
    pf__assert_report(
        expr,
        file,
        line,
        func,
        pf__format(buf, sizeof(buf), "%s %s %s", qa, pf__negated(op), qb)
    );
}

PF_API void pf__assert_fail_mem(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const void *a,
    const void *b,
    size_t size
) {
    char buf[128];
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t i = 0;

    if (op == PF__ASSERT_NE) {
        pf__format(buf, sizeof(buf), "all %zu bytes are equal", size);
    } else {
        while (i < size && pa[i] == pb[i])
            i++;
        pf__format(
            buf,
            sizeof(buf),
            "bytes differ at offset %zu: 0x%02x != 0x%02x",
            i,
            (unsigned)pa[i],
            (unsigned)pb[i]
        );
    }
    pf__assert_report(expr, file, line, func, buf);
}

PF_API void pf__assert_fail_ptr(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const volatile void *ptr
) {
    char buf[64];

    if (op == PF__ASSERT_NULL)
        pf__format(buf, sizeof(buf), "it is %p", (void *)(uintptr_t)ptr);
    pf__assert_report(
        expr, file, line, func, op == PF__ASSERT_NULL ? buf : NULL
    );
}
