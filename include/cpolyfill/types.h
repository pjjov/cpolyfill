/** cpolyfill - Filling the gaps between C standards and compilers

    Compile-time type checks. All are integer constant expressions (0 or
    1) that do not evaluate their arguments, so they can be used in
    PF_STATIC_ASSERT and array sizes.

        PF_IS_TYPE(type, expr)  `expr` has type `type`, ignoring top-level
                                qualifiers (`const int` is an `int`).
        PF_SAME_TYPE(a, b)      Both expressions have the same type, ignoring
                                top-level qualifiers.
        PF_IS_CONST(expr)       `expr` is a const lvalue (`*p` with
                                `const char *p` is, `p` is not).
        PF_IS_ARRAY(expr)       `expr` is an array (not a pointer).
        PF_IS_POINTER(expr)     `expr` is a pointer (not an array).

    Arrays do not decay to pointers in these checks: a `char[4]` is not a
    `char *`. The result for function designators is unspecified.

        PF_CHECK_TYPE(type, expr)
                                `expr`, unchanged, if it has type `type`;
                                otherwise compilation fails.
        PF_IF_TYPE(type, expr, a, b)
                                `a` if `expr` has type `type`, else `b`.
        PF_IF_CONST(expr, a, b) `a` if `expr` is a const lvalue, else `b`.

    In C with GCC and Clang, PF_IF_TYPE and PF_IF_CONST use
    `__builtin_choose_expr`, so `a` and `b` may have different types. In
    C++ (and without type checks) they are `cond ? a : b`, so `a` and `b`
    need a common type. Either way only one of them is evaluated.

        PF_HAS_TYPE_CHECKS      1 when the checks work: GCC and Clang, and
                                C++11. Otherwise they are constants that
                                assume correct use (PF_IS_TYPE, PF_SAME_TYPE
                                and PF_IS_ARRAY are 1, the others 0) and
                                PF_CHECK_TYPE does not check.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_TYPES_H
#define CPOLYFILL_TYPES_H

#include <cpolyfill/platform.h>
#include <cpolyfill/stdc.h>

#include <stddef.h>

#if PF_CPP >= 201103L

    #define PF_HAS_TYPE_CHECKS 1

extern "C++" {
template <class A, class B> struct pf__same {
    static const bool value = false;
};
template <class A> struct pf__same<A, A> {
    static const bool value = true;
};
template <class T> struct pf__is_const {
    static const bool value = false;
};
template <class T> struct pf__is_const<const T> {
    static const bool value = true;
};
template <class T> struct pf__is_array {
    static const bool value = false;
};
template <class T, size_t N> struct pf__is_array<T[N]> {
    static const bool value = true;
};
template <class T> struct pf__is_array<T[]> {
    static const bool value = true;
};
template <class T> struct pf__is_pointer {
    static const bool value = false;
};
template <class T> struct pf__is_pointer<T *> {
    static const bool value = true;
};
template <class T, class U> constexpr U &&pf__check_type(U &&x) {
    static_assert(
        pf__same<T, typename pf__unqual<typename pf__noref<U>::type>::type>::
            value,
        "PF_CHECK_TYPE: the expression does not have the expected type"
    );
    return static_cast<U &&>(x);
}
}

    /* The declared type for names, without references for other lvalues,
       and without top-level qualifiers. */
    #define PF__DECLTYPE(expr) typename pf__noref<decltype(expr)>::type
    #define PF__BARE(expr) typename pf__unqual<PF__DECLTYPE(expr)>::type

    #define PF_IS_TYPE(type, expr) (pf__same<type, PF__BARE(expr)>::value)
    #define PF_SAME_TYPE(a, b) (pf__same<PF__BARE(a), PF__BARE(b)>::value)
    #define PF_IS_CONST(expr) (pf__is_const<PF__DECLTYPE(expr)>::value)
    #define PF_IS_ARRAY(expr) (pf__is_array<PF__DECLTYPE(expr)>::value)
    #define PF_IS_POINTER(expr) (pf__is_pointer<PF__BARE(expr)>::value)

    #define PF_CHECK_TYPE(type, expr) (pf__check_type<type>(expr))
    #define PF_IF_TYPE(type, expr, a, b) (PF_IS_TYPE(type, expr) ? (a) : (b))
    #define PF_IF_CONST(expr, a, b) (PF_IS_CONST(expr) ? (a) : (b))

#elif (PF_CC_GNU || PF_CC_CLANG) && !defined(__cplusplus)

    #define PF_HAS_TYPE_CHECKS 1

    #define PF_IS_TYPE(type, expr)                           \
        __builtin_types_compatible_p(type, __typeof__(expr))
    #define PF_SAME_TYPE(a, b)                                     \
        __builtin_types_compatible_p(__typeof__(a), __typeof__(b))
    /* Qualifiers of the pointed-to type are compared, so adding const to
       a type that has it already gives the same type. */
    #define PF_IS_CONST(expr)                            \
        __builtin_types_compatible_p(                    \
            const __typeof__(expr) *, __typeof__(expr) * \
        )

    /* `0 ? e : e` is `e` after lvalue conversion: an array decays to a
       pointer, a pointer stays the same (small integers are promoted, but
       they are excluded by the pointer class check). */
    #define PF__DECAYS(expr)                                  \
        (!__builtin_types_compatible_p(                       \
            __typeof__(expr), __typeof__(0 ? (expr) : (expr)) \
        ))
    #define PF__POINTER_CLASS(expr) (__builtin_classify_type(expr) == 5)
    #define PF_IS_ARRAY(expr) (PF__POINTER_CLASS(expr) && PF__DECAYS(expr))
    #define PF_IS_POINTER(expr) (PF__POINTER_CLASS(expr) && !PF__DECAYS(expr))

    #if PF_HAS_ATTRIBUTE(error)
extern void pf__check_type_failed(void) __attribute__((
    error("PF_CHECK_TYPE: the expression does not have the expected type")
));
        #define PF_CHECK_TYPE(type, expr)                               \
            __builtin_choose_expr(                                      \
                PF_IS_TYPE(type, expr), (expr), pf__check_type_failed() \
            )
    #else
        #define PF_CHECK_TYPE(type, expr)                           \
            (__extension__({                                        \
                __extension__ _Static_assert(                       \
                    PF_IS_TYPE(type, expr),                         \
                    "PF_CHECK_TYPE: `" #expr "' is not `" #type "'" \
                );                                                  \
                (expr);                                             \
            }))
    #endif

    #define PF_IF_TYPE(type, expr, a, b)                    \
        __builtin_choose_expr(PF_IS_TYPE(type, expr), a, b)
    #define PF_IF_CONST(expr, a, b)                    \
        __builtin_choose_expr(PF_IS_CONST(expr), a, b)

#else

    #define PF_HAS_TYPE_CHECKS 0

    /* `sizeof` keeps the arguments compiled (and checked for errors)
       without evaluating them. */
    #define PF_IS_TYPE(type, expr) (0 * sizeof(type) * sizeof(expr) + 1)
    #define PF_SAME_TYPE(a, b) (0 * sizeof(a) * sizeof(b) + 1)
    #define PF_IS_CONST(expr) (0 * sizeof(expr))
    #define PF_IS_ARRAY(expr) (0 * sizeof(expr) + 1)
    #define PF_IS_POINTER(expr) (0 * sizeof(expr))

    #define PF_CHECK_TYPE(type, expr) (expr)
    #define PF_IF_TYPE(type, expr, a, b) (PF_IS_TYPE(type, expr) ? (a) : (b))
    #define PF_IF_CONST(expr, a, b) (PF_IS_CONST(expr) ? (a) : (b))

#endif

#endif /* CPOLYFILL_TYPES_H */
