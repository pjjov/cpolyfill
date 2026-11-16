/** cpolyfill - Filling the gaps between C standards and compilers

    Assertions with a replaceable failure handler.

    Three families, which differ in what NDEBUG does:
        PF_ASSERT*              Removed with NDEBUG (arguments not
                                evaluated), like `assert`.
        PF_ASSERT_ALWAYS*       Never removed.
        PF_CHECK*               Expressions giving `bool`. They fail like
                                PF_ASSERT without NDEBUG; with NDEBUG they
                                only give the result, for validation that
                                must also be handled in release builds:
                                    if (!PF_CHECK(len <= cap))
                                        return PF_EINVAL;

    Each family has these forms, shown for PF_ASSERT:
        PF_ASSERT(expr)
        PF_ASSERT_MSG(expr, fmt, ...)
                                With a printf-style message.
        PF_ASSERT_EQ(a, b), PF_ASSERT_NE(a, b), PF_ASSERT_LT(a, b),
        PF_ASSERT_LE(a, b), PF_ASSERT_GT(a, b), PF_ASSERT_GE(a, b)
                                Integers (up to 64 bits), printed on failure.
                                They compare values, not C types: -1 is less
                                than 0u, unlike in `-1 < 0u`.
        PF_ASSERT_STR_EQ(a, b), PF_ASSERT_STR_NE(a, b)
                                Strings; NULL equals only NULL.
        PF_ASSERT_MEM_EQ(a, b, size), PF_ASSERT_MEM_NE(a, b, size)
        PF_ASSERT_NULL(ptr), PF_ASSERT_NOT_NULL(ptr)

    Every argument is evaluated once. Like <assert.h>, NDEBUG is checked
    every time this header is included.

    Failure output, by the default handler, is that of glibc's `assert`:
        prog: file.c:12: main: Assertion `n > 0' failed.
        prog: file.c:13: main: Assertion `a == b' failed: 3 != 4
    on stderr, followed by `abort()`.

        typedef void (*pf_assert_handler_fn)(const char *expr,
            const char *file, int line, const char *func,
            const char *message);
        pf_assert_handler_fn pf_set_assert_handler(pf_assert_handler_fn fn);
                                Installs `fn` for all threads (NULL restores
                                the default) and returns the previous handler.
                                `message` is NULL when there is none. When the
                                handler returns, `abort()` is called; it may
                                also exit or `longjmp` instead. The handler
                                must not fail an assertion itself.
        void pf_assert_fail(const char *expr, const char *file, int line,
            const char *func, const char *fmt, ...);
                                What the macros call: runs the handler and
                                aborts. `fmt` may be NULL.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_ASSERT:
        `assert` is PF_ASSERT (replacing the one of <assert.h>; include this
        header after it), and `static_assert` is PF_STATIC_ASSERT where the
        language lacks it.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_ASSERT_H
    #define CPOLYFILL_ASSERT_H

    #include <cpolyfill/attribute.h>
    #include <cpolyfill/platform.h>
    #include <cpolyfill/stdc.h>

    #include <limits.h>
    #include <stddef.h>
    #include <string.h>
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif

PF_BEGIN_DECLS

typedef void (*pf_assert_handler_fn)(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
);

PF_API pf_assert_handler_fn pf_set_assert_handler(pf_assert_handler_fn fn);

PF_NORETURN PF_COLD PF_PRINTF(5, 6) PF_API void pf_assert_fail(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
);

/* ---- Internals used by the macros ------------------------------------- */

enum {
    PF__ASSERT_EQ,
    PF__ASSERT_NE,
    PF__ASSERT_LT,
    PF__ASSERT_LE,
    PF__ASSERT_GT,
    PF__ASSERT_GE,
    PF__ASSERT_NULL,
    PF__ASSERT_NOT_NULL
};

PF_NORETURN PF_COLD PF_API void pf__assert_fail_int(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    bool a_signed,
    unsigned long long a,
    bool b_signed,
    unsigned long long b
);
PF_NORETURN PF_COLD PF_API void pf__assert_fail_str(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const char *a,
    const char *b
);
PF_NORETURN PF_COLD PF_API void pf__assert_fail_mem(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const void *a,
    const void *b,
    size_t size
);
PF_NORETURN PF_COLD PF_API void pf__assert_fail_ptr(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const volatile void *ptr
);

    /* Whether the type of `x` (after promotion) is signed, without evaluating
   `x`: `0 ? x : 0` has that type and the value 0. */
    #define PF__ASSERT_SIGNED(x) ((0 ? (x) : 0) - 1 < (0 ? (x) : 0))

/* Compares the mathematical values: a negative value is below any value
   of an unsigned type. Converting to unsigned long long is well defined
   and keeps the order of negative values. */
PF_INLINE bool pf__assert_int(
    bool fail,
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
    const unsigned long long sign = 1ULL << (sizeof(a) * CHAR_BIT - 1);
    bool a_neg = a_signed && (a & sign), b_neg = b_signed && (b & sign);
    int cmp = a_neg != b_neg ? (a_neg ? -1 : 1) : (a < b ? -1 : a > b);
    bool ok;
    switch (op) {
    case PF__ASSERT_EQ:
        ok = cmp == 0;
        break;
    case PF__ASSERT_NE:
        ok = cmp != 0;
        break;
    case PF__ASSERT_LT:
        ok = cmp < 0;
        break;
    case PF__ASSERT_LE:
        ok = cmp <= 0;
        break;
    case PF__ASSERT_GT:
        ok = cmp > 0;
        break;
    default:
        ok = cmp >= 0;
        break;
    }
    if (!ok && fail)
        pf__assert_fail_int(
            expr, file, line, func, op, a_signed, a, b_signed, b
        );
    return ok;
}

PF_INLINE bool pf__assert_str(
    bool fail,
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const char *a,
    const char *b
) {
    bool eq = a == b || (a && b && strcmp(a, b) == 0);
    bool ok = op == PF__ASSERT_EQ ? eq : !eq;
    if (!ok && fail)
        pf__assert_fail_str(expr, file, line, func, op, a, b);
    return ok;
}

PF_INLINE bool pf__assert_mem(
    bool fail,
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const void *a,
    const void *b,
    size_t size
) {
    bool eq = size == 0 || memcmp(a, b, size) == 0;
    bool ok = op == PF__ASSERT_EQ ? eq : !eq;
    if (!ok && fail)
        pf__assert_fail_mem(expr, file, line, func, op, a, b, size);
    return ok;
}

PF_INLINE bool pf__assert_ptr(
    bool fail,
    const char *expr,
    const char *file,
    int line,
    const char *func,
    int op,
    const volatile void *ptr
) {
    bool ok = op == PF__ASSERT_NULL ? ptr == NULL : ptr != NULL;
    if (!ok && fail)
        pf__assert_fail_ptr(expr, file, line, func, op, ptr);
    return ok;
}

PF_INLINE bool pf__assert_false(void) { return false; }

PF_INLINE bool pf__assert_true(
    bool fail,
    bool ok,
    const char *expr,
    const char *file,
    int line,
    const char *func
) {
    if (!ok && fail)
        pf_assert_fail(expr, file, line, func, NULL);
    return ok;
}

PF_END_DECLS

    /* `fail` is whether a failure aborts (or only gives false), `str` the
   expression as written. Each public macro stringizes its own arguments:
   once they are passed on to another macro, they are expanded. */
    #define PF__ASSERT_LOC(str) str, __FILE__, __LINE__, __func__

    #define PF__ASSERT_IF(fail, str, ...)                           \
        pf__assert_true(fail, !!(__VA_ARGS__), PF__ASSERT_LOC(str))
    /* Ends in a call, not `false`, so using it as a statement does not warn
   about a comma operand without effect. */
    #define PF__ASSERT_IF_MSG(fail, str, expr, ...)                          \
        ((expr) ? true                                                       \
                : ((fail) ? pf_assert_fail(PF__ASSERT_LOC(str), __VA_ARGS__) \
                          : (void)0,                                         \
                   pf__assert_false()))
    #define PF__ASSERT_INT(fail, op, str, a, b) \
        pf__assert_int(                         \
            fail,                               \
            PF__ASSERT_LOC(str),                \
            op,                                 \
            PF__ASSERT_SIGNED(a),               \
            (unsigned long long)(a),            \
            PF__ASSERT_SIGNED(b),               \
            (unsigned long long)(b)             \
        )
    #define PF__ASSERT_STR(fail, op, str, a, b)                 \
        pf__assert_str(fail, PF__ASSERT_LOC(str), op, (a), (b))
    #define PF__ASSERT_MEM(fail, op, str, a, b, size)                   \
        pf__assert_mem(fail, PF__ASSERT_LOC(str), op, (a), (b), (size))
    #define PF__ASSERT_PTR(fail, op, str, ptr)               \
        pf__assert_ptr(fail, PF__ASSERT_LOC(str), op, (ptr))

/* ---- PF_ASSERT_ALWAYS ------------------------------------------------- */

    #define PF_ASSERT_ALWAYS(...)                           \
        ((void)PF__ASSERT_IF(1, #__VA_ARGS__, __VA_ARGS__))
    #define PF_ASSERT_ALWAYS_MSG(expr, ...) \
        ((expr) ? (void)0 : pf_assert_fail(PF__ASSERT_LOC(#expr), __VA_ARGS__))
    #define PF_ASSERT_ALWAYS_EQ(a, b)                                \
        ((void)PF__ASSERT_INT(1, PF__ASSERT_EQ, #a " == " #b, a, b))
    #define PF_ASSERT_ALWAYS_NE(a, b)                                \
        ((void)PF__ASSERT_INT(1, PF__ASSERT_NE, #a " != " #b, a, b))
    #define PF_ASSERT_ALWAYS_LT(a, b)                               \
        ((void)PF__ASSERT_INT(1, PF__ASSERT_LT, #a " < " #b, a, b))
    #define PF_ASSERT_ALWAYS_LE(a, b)                                \
        ((void)PF__ASSERT_INT(1, PF__ASSERT_LE, #a " <= " #b, a, b))
    #define PF_ASSERT_ALWAYS_GT(a, b)                               \
        ((void)PF__ASSERT_INT(1, PF__ASSERT_GT, #a " > " #b, a, b))
    #define PF_ASSERT_ALWAYS_GE(a, b)                                \
        ((void)PF__ASSERT_INT(1, PF__ASSERT_GE, #a " >= " #b, a, b))
    #define PF_ASSERT_ALWAYS_STR_EQ(a, b)                            \
        ((void)PF__ASSERT_STR(1, PF__ASSERT_EQ, #a " == " #b, a, b))
    #define PF_ASSERT_ALWAYS_STR_NE(a, b)                            \
        ((void)PF__ASSERT_STR(1, PF__ASSERT_NE, #a " != " #b, a, b))
    #define PF_ASSERT_ALWAYS_MEM_EQ(a, b, size)                            \
        ((void)PF__ASSERT_MEM(1, PF__ASSERT_EQ, #a " == " #b, a, b, size))
    #define PF_ASSERT_ALWAYS_MEM_NE(a, b, size)                            \
        ((void)PF__ASSERT_MEM(1, PF__ASSERT_NE, #a " != " #b, a, b, size))
    #define PF_ASSERT_ALWAYS_NULL(ptr)                                   \
        ((void)PF__ASSERT_PTR(1, PF__ASSERT_NULL, #ptr " == NULL", ptr))
    #define PF_ASSERT_ALWAYS_NOT_NULL(ptr)                                   \
        ((void)PF__ASSERT_PTR(1, PF__ASSERT_NOT_NULL, #ptr " != NULL", ptr))

/* ---- Standard names --------------------------------------------------- */

    #if (defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_ASSERT)) \
        || defined(PF_STD_NAMES_ASSERT)
        #define PF__ASSERT_STD_NAMES 1
        #if !PF_STD_C23 && !defined(__cplusplus) && !defined(static_assert)
            #define static_assert(...) PF_STATIC_ASSERT(__VA_ARGS__)
        #endif
    #else
        #define PF__ASSERT_STD_NAMES 0
    #endif

#endif /* CPOLYFILL_ASSERT_H */

/* ---- Depending on NDEBUG, every time the header is included ----------- */

#undef PF__ASSERT_DEBUG
#undef PF_ASSERT
#undef PF_ASSERT_MSG
#undef PF_ASSERT_EQ
#undef PF_ASSERT_NE
#undef PF_ASSERT_LT
#undef PF_ASSERT_LE
#undef PF_ASSERT_GT
#undef PF_ASSERT_GE
#undef PF_ASSERT_STR_EQ
#undef PF_ASSERT_STR_NE
#undef PF_ASSERT_MEM_EQ
#undef PF_ASSERT_MEM_NE
#undef PF_ASSERT_NULL
#undef PF_ASSERT_NOT_NULL

#ifdef NDEBUG
    #define PF__ASSERT_DEBUG 0
    #define PF_ASSERT(...) ((void)0)
    #define PF_ASSERT_MSG(expr, ...) ((void)0)
    #define PF_ASSERT_EQ(a, b) ((void)0)
    #define PF_ASSERT_NE(a, b) ((void)0)
    #define PF_ASSERT_LT(a, b) ((void)0)
    #define PF_ASSERT_LE(a, b) ((void)0)
    #define PF_ASSERT_GT(a, b) ((void)0)
    #define PF_ASSERT_GE(a, b) ((void)0)
    #define PF_ASSERT_STR_EQ(a, b) ((void)0)
    #define PF_ASSERT_STR_NE(a, b) ((void)0)
    #define PF_ASSERT_MEM_EQ(a, b, size) ((void)0)
    #define PF_ASSERT_MEM_NE(a, b, size) ((void)0)
    #define PF_ASSERT_NULL(ptr) ((void)0)
    #define PF_ASSERT_NOT_NULL(ptr) ((void)0)
#else
    #define PF__ASSERT_DEBUG 1
    #define PF_ASSERT PF_ASSERT_ALWAYS
    #define PF_ASSERT_MSG PF_ASSERT_ALWAYS_MSG
    #define PF_ASSERT_EQ PF_ASSERT_ALWAYS_EQ
    #define PF_ASSERT_NE PF_ASSERT_ALWAYS_NE
    #define PF_ASSERT_LT PF_ASSERT_ALWAYS_LT
    #define PF_ASSERT_LE PF_ASSERT_ALWAYS_LE
    #define PF_ASSERT_GT PF_ASSERT_ALWAYS_GT
    #define PF_ASSERT_GE PF_ASSERT_ALWAYS_GE
    #define PF_ASSERT_STR_EQ PF_ASSERT_ALWAYS_STR_EQ
    #define PF_ASSERT_STR_NE PF_ASSERT_ALWAYS_STR_NE
    #define PF_ASSERT_MEM_EQ PF_ASSERT_ALWAYS_MEM_EQ
    #define PF_ASSERT_MEM_NE PF_ASSERT_ALWAYS_MEM_NE
    #define PF_ASSERT_NULL PF_ASSERT_ALWAYS_NULL
    #define PF_ASSERT_NOT_NULL PF_ASSERT_ALWAYS_NOT_NULL
#endif

/* PF_CHECK aborts on failure without NDEBUG, and only gives the result with
   it. PF__ASSERT_DEBUG is expanded when PF_CHECK is used, so the macros
   need no redefinition. */
#ifndef PF_CHECK
    #define PF_CHECK(...)                                          \
        PF__ASSERT_IF(PF__ASSERT_DEBUG, #__VA_ARGS__, __VA_ARGS__)
    #define PF_CHECK_MSG(expr, ...)                                   \
        PF__ASSERT_IF_MSG(PF__ASSERT_DEBUG, #expr, expr, __VA_ARGS__)
    #define PF_CHECK_EQ(a, b)                                               \
        PF__ASSERT_INT(PF__ASSERT_DEBUG, PF__ASSERT_EQ, #a " == " #b, a, b)
    #define PF_CHECK_NE(a, b)                                               \
        PF__ASSERT_INT(PF__ASSERT_DEBUG, PF__ASSERT_NE, #a " != " #b, a, b)
    #define PF_CHECK_LT(a, b)                                              \
        PF__ASSERT_INT(PF__ASSERT_DEBUG, PF__ASSERT_LT, #a " < " #b, a, b)
    #define PF_CHECK_LE(a, b)                                               \
        PF__ASSERT_INT(PF__ASSERT_DEBUG, PF__ASSERT_LE, #a " <= " #b, a, b)
    #define PF_CHECK_GT(a, b)                                              \
        PF__ASSERT_INT(PF__ASSERT_DEBUG, PF__ASSERT_GT, #a " > " #b, a, b)
    #define PF_CHECK_GE(a, b)                                               \
        PF__ASSERT_INT(PF__ASSERT_DEBUG, PF__ASSERT_GE, #a " >= " #b, a, b)
    #define PF_CHECK_STR_EQ(a, b)                                           \
        PF__ASSERT_STR(PF__ASSERT_DEBUG, PF__ASSERT_EQ, #a " == " #b, a, b)
    #define PF_CHECK_STR_NE(a, b)                                           \
        PF__ASSERT_STR(PF__ASSERT_DEBUG, PF__ASSERT_NE, #a " != " #b, a, b)
    #define PF_CHECK_MEM_EQ(a, b, size)                               \
        PF__ASSERT_MEM(                                               \
            PF__ASSERT_DEBUG, PF__ASSERT_EQ, #a " == " #b, a, b, size \
        )
    #define PF_CHECK_MEM_NE(a, b, size)                               \
        PF__ASSERT_MEM(                                               \
            PF__ASSERT_DEBUG, PF__ASSERT_NE, #a " != " #b, a, b, size \
        )
    #define PF_CHECK_NULL(ptr) \
        PF__ASSERT_PTR(PF__ASSERT_DEBUG, PF__ASSERT_NULL, #ptr " == NULL", ptr)
    #define PF_CHECK_NOT_NULL(ptr)                                      \
        PF__ASSERT_PTR(                                                 \
            PF__ASSERT_DEBUG, PF__ASSERT_NOT_NULL, #ptr " != NULL", ptr \
        )
#endif

#if PF__ASSERT_STD_NAMES
    #undef assert
    #define assert(...) PF_ASSERT(__VA_ARGS__)
#endif
