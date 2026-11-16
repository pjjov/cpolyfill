/** cpolyfill - Filling the gaps between C standards and compilers

    Language keywords of newer C standards, usable in every standard and
    in C++.

        PF_TYPEOF(x)            C23 `typeof`: the type of an expression or a
                                type name. With MSVC in C++ (decltype) only
                                expressions are accepted.
        PF_TYPEOF_UNQUAL(x)     C23 `typeof_unqual`: the same without
                                qualifiers (const, volatile, ...).
        PF_ALIGNOF(type)        C11 `alignof`. GNU compilers also accept an
                                expression.
        PF_ALIGNAS(x)           C11 `alignas`: an alignment (a constant) or a
                                type to take it from. Write it at the start
                                of the declaration (`PF_ALIGNAS(16) static
                                char buf[64];`), which is required for C99
                                with GCC and Clang. MSVC in C99 mode only
                                takes a constant.
        PF_STATIC_ASSERT(expr[, message])
                                C11 `static_assert`, with the C23 form that
                                has no message (the expression is shown
                                instead). A compile-time check in every
                                standard. In C99 with compilers other than
                                GCC and Clang it is only valid at file and
                                block scope, not inside a structure.
        PF_THREAD_LOCAL         C11 `thread_local`. Write it after `static`
                                or `extern`.
        PF_NORETURN             C11 `noreturn`, from <cpolyfill/attribute.h>.
        PF_UNREACHABLE()        C23 `unreachable()`: reaching it is
                                undefined behaviour.
        PF_NULLPTR              C23 `nullptr`, `((void *)0)` before.

        pf_max_align_t          C11 `max_align_t`.
        pf_char8_t              C23 `char8_t` (`unsigned char`), and the
                                distinct `char8_t` type of C++20.

    Availability, always 0 or 1. When 0, the macro is not defined:
        PF_HAS_TYPEOF, PF_HAS_TYPEOF_UNQUAL, PF_HAS_ALIGNAS,
        PF_HAS_THREAD_LOCAL

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_STDC (C only; the
    names already exist in C++, `typeof` aside):
        `typeof`, `typeof_unqual`, `alignof`, `alignas`, `static_assert`,
        `thread_local`, `unreachable()`, `nullptr`, `bool`, `true` and
        `false`, each only where the language lacks it and the polyfill is
        available. Where the standard library has a name, it is used: C11
        and C17 get `<stdbool.h>`, `<stdalign.h>` and the `static_assert`
        of `<assert.h>`, which requires a message.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_STDC_H
#define CPOLYFILL_STDC_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C++" {
template <class T> struct pf__noref {
    typedef T type;
};
template <class T> struct pf__noref<T &> {
    typedef T type;
};
template <class T> struct pf__noref<T &&> {
    typedef T type;
};
template <class T> struct pf__unqual {
    typedef T type;
};
template <class T> struct pf__unqual<const T> {
    typedef T type;
};
template <class T> struct pf__unqual<volatile T> {
    typedef T type;
};
template <class T> struct pf__unqual<const volatile T> {
    typedef T type;
};
}
#endif

/* ---- typeof ----------------------------------------------------------- */

#if PF_STD_C23
    #define PF_HAS_TYPEOF 1
    #define PF_TYPEOF(...) typeof(__VA_ARGS__)
#elif PF_CC_GNU || PF_CC_CLANG                         \
    || (PF_MSVC_PREREQ(1939) && !defined(__cplusplus))
    #define PF_HAS_TYPEOF 1
    #define PF_TYPEOF(...) __typeof__(__VA_ARGS__)
#elif PF_CPP >= 201103L
    #define PF_HAS_TYPEOF 1
    #define PF_TYPEOF(...) typename pf__noref<decltype(__VA_ARGS__)>::type
#else
    #define PF_HAS_TYPEOF 0
#endif

#if PF_STD_C23
    #define PF_HAS_TYPEOF_UNQUAL 1
    #define PF_TYPEOF_UNQUAL(...) typeof_unqual(__VA_ARGS__)
#elif !defined(__cplusplus)                                              \
    && ((PF_CC_GCC && PF_GCC_PREREQ(14, 0))                              \
        || (PF_CC_CLANG && !PF_CC_APPLE_CLANG && PF_CLANG_PREREQ(19, 0)) \
        || (PF_CC_APPLE_CLANG && PF_CLANG_PREREQ(17, 0))                 \
        || PF_MSVC_PREREQ(1939))
    #define PF_HAS_TYPEOF_UNQUAL 1
    #define PF_TYPEOF_UNQUAL(...) __typeof_unqual__(__VA_ARGS__)
#elif defined(__cplusplus) && PF_HAS_TYPEOF
    #define PF_HAS_TYPEOF_UNQUAL 1
    #define PF_TYPEOF_UNQUAL(...)                         \
        typename pf__unqual<PF_TYPEOF(__VA_ARGS__)>::type
#else
    #define PF_HAS_TYPEOF_UNQUAL 0
#endif

/* ---- Alignment -------------------------------------------------------- */

#if PF_STD_C23 || PF_CPP >= 201103L
    #define PF_ALIGNOF(...) alignof(__VA_ARGS__)
#elif PF_STD_C11
    #define PF_ALIGNOF(...) _Alignof(__VA_ARGS__)
#elif PF_CC_GNU || PF_CC_CLANG
    /* Not __alignof__, which gives the preferred alignment (8 for a
       `long long` on x86) instead of the one _Alignof gives (4). */
    #define PF_ALIGNOF(...) (__extension__ _Alignof(__VA_ARGS__))
#elif PF_CC_MSVC
    #define PF_ALIGNOF(...) __alignof(__VA_ARGS__)
#else
    #define PF_ALIGNOF(...)        \
        offsetof(                  \
            struct {               \
                char pf__c;        \
                __VA_ARGS__ pf__t; \
            },                     \
            pf__t                  \
        )
#endif

#if PF_STD_C23 || PF_CPP >= 201103L
    #define PF_HAS_ALIGNAS 1
    #define PF_ALIGNAS(...) alignas(__VA_ARGS__)
#elif PF_STD_C11
    #define PF_HAS_ALIGNAS 1
    #define PF_ALIGNAS(...) _Alignas(__VA_ARGS__)
#elif PF_CC_GNU || PF_CC_CLANG
    #define PF_HAS_ALIGNAS 1
    #define PF_ALIGNAS(...) __extension__ _Alignas(__VA_ARGS__)
#elif PF_CC_MSVC
    #define PF_HAS_ALIGNAS 1
    #define PF_ALIGNAS(...) __declspec(align(__VA_ARGS__))
#else
    #define PF_HAS_ALIGNAS 0
#endif

/* ---- static_assert ---------------------------------------------------- */

/* The message is the second argument, or the stringized arguments. */
#define PF_STATIC_ASSERT(...) PF__STATIC_ASSERT(__VA_ARGS__, #__VA_ARGS__, )

#if PF_STD_C23 || PF_CPP >= 201103L
    #define PF__STATIC_ASSERT(expr, msg, ...) static_assert(expr, msg)
#elif PF_STD_C11
    #define PF__STATIC_ASSERT(expr, msg, ...) _Static_assert(expr, msg)
#elif PF_CC_GNU || PF_CC_CLANG
    #define PF__STATIC_ASSERT(expr, msg, ...)   \
        __extension__ _Static_assert(expr, msg)
#else
    /* An array of negative size when false. Redeclaring an extern array
       of the same size is valid, so no unique name is needed, and an
       extern array cannot be variably sized, so `expr` must be a
       constant. */
    #define PF__STATIC_ASSERT(expr, msg, ...)          \
        extern char pf__static_assert[(expr) ? 1 : -1]
#endif

/* ---- thread_local ----------------------------------------------------- */

#if PF_STD_C23 || PF_CPP >= 201103L
    #define PF_HAS_THREAD_LOCAL 1
    #define PF_THREAD_LOCAL thread_local
#elif PF_STD_C11 && !PF_CC_MSVC
    #define PF_HAS_THREAD_LOCAL 1
    #define PF_THREAD_LOCAL _Thread_local
#elif PF_CC_GNU || PF_CC_CLANG
    #define PF_HAS_THREAD_LOCAL 1
    #define PF_THREAD_LOCAL __thread
#elif PF_CC_MSVC
    #define PF_HAS_THREAD_LOCAL 1
    #define PF_THREAD_LOCAL __declspec(thread)
#else
    #define PF_HAS_THREAD_LOCAL 0
#endif

/* ---- unreachable ------------------------------------------------------ */

#if PF_CC_GNU || PF_CC_CLANG
    #define PF_UNREACHABLE() __builtin_unreachable()
#elif PF_CC_MSVC
    #define PF_UNREACHABLE() __assume(0)
#elif PF_STD_C23 && defined(unreachable)
    #define PF_UNREACHABLE() unreachable()
#else
    #define PF_UNREACHABLE() ((void)0)
#endif

/* ---- nullptr ---------------------------------------------------------- */

#if PF_STD_C23 || PF_CPP >= 201103L
    #define PF_NULLPTR nullptr
#else
    #define PF_NULLPTR ((void *)0)
#endif

/* ---- Types ------------------------------------------------------------ */

#if PF_STD_C11 || PF_CPP >= 201103L
typedef max_align_t pf_max_align_t;
#elif PF_CC_MSVC
typedef double pf_max_align_t;
#elif PF_CC_GNU || PF_CC_CLANG
/* The definition GCC and Clang use for max_align_t in C11. */
typedef struct pf_max_align_t {
    long long pf__ll __attribute__((__aligned__(__alignof__(long long))));
    long double pf__ld __attribute__((__aligned__(__alignof__(long double))));
    #if PF_CC_GCC && PF_ARCH_X86
    __extension__ __float128 pf__f128
        __attribute__((__aligned__(__alignof__(__float128))));
    #endif
} pf_max_align_t;
#else
typedef union pf_max_align_t {
    long long pf__ll;
    long double pf__ld;
    void *pf__p;
    void (*pf__fn)(void);
} pf_max_align_t;
#endif

#if defined(__cpp_char8_t)
typedef char8_t pf_char8_t;
#else
typedef unsigned char pf_char8_t;
#endif

/* ---- Standard names --------------------------------------------------- */

#if (                                                         \
    (defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_STDC)) \
    || defined(PF_STD_NAMES_STDC)                             \
) && !defined(__cplusplus)

    #if !PF_STD_C23
        #include <stdbool.h>

        #if PF_HAS_TYPEOF && !defined(typeof)
            #define typeof(...) PF_TYPEOF(__VA_ARGS__)
        #endif
        #if PF_HAS_TYPEOF_UNQUAL && !defined(typeof_unqual)
            #define typeof_unqual(...) PF_TYPEOF_UNQUAL(__VA_ARGS__)
        #endif

        #if PF_STD_C11
            /* The system's definitions, so including these headers later
               does not redefine the names differently. */
            #include <assert.h>
            #include <stdalign.h>
            #ifndef thread_local
                #define thread_local _Thread_local
            #endif
        #else
            #ifndef alignof
                #define alignof(...) PF_ALIGNOF(__VA_ARGS__)
            #endif
            #if PF_HAS_ALIGNAS && !defined(alignas)
                #define alignas(...) PF_ALIGNAS(__VA_ARGS__)
            #endif
            #if PF_HAS_THREAD_LOCAL && !defined(thread_local)
                #define thread_local PF_THREAD_LOCAL
            #endif
        #endif

        #ifndef static_assert
            #define static_assert(...) PF_STATIC_ASSERT(__VA_ARGS__)
        #endif
        #ifndef nullptr
            #define nullptr PF_NULLPTR
        #endif
    #endif

    #ifndef unreachable
        #define unreachable() PF_UNREACHABLE()
    #endif
#endif

#endif /* CPOLYFILL_STDC_H */
