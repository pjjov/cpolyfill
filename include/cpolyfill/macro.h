/** cpolyfill - Filling the gaps between C standards and compilers

    Utility macros.

    Arithmetic. With GCC and Clang each argument is evaluated once (a
    statement expression); elsewhere arguments may be evaluated twice:
        PF_MIN(x, y), PF_MAX(x, y)
        PF_ABS(x)
        PF_DIFF(x, y)           |x - y|, without overflowing unsigned types.
        PF_CLAMP(x, lo, hi)     `x` limited to [lo, hi].
        PF_SWAP(x, y)           A statement: swaps two lvalues of the same
                                type. Evaluates them more than once.

    Pointers and alignment. `align` must be a power of two, and the
    arguments may be evaluated more than once. For pointers, convert them
    to `uintptr_t` first:
        PF_OFFSET(ptr, bytes)   `ptr` advanced by `bytes`, as `void *`.
        PF_PTRDIFF(a, b)        `a - b` in bytes, as `ptrdiff_t`.
        PF_IS_ALIGNED(x, align)
        PF_ALIGN_PAD(x, align)  Bytes from `x` to the next aligned value.
        PF_ALIGN_UP(x, align), PF_ALIGN_DOWN(x, align)
        PF_CONTAINER_OF(ptr, type, member)
                                The `type *` whose `member` `ptr` points to.
                                `ptr` must have the type of `&member`, so for
                                an array member it is a pointer to the array.

    Flags and bits. These return the new value, they do not assign it:
        PF_FLAG_SET(x, flags), PF_FLAG_CLEAR(x, flags),
        PF_FLAG_TOGGLE(x, flags)
        PF_FLAG_TEST(x, flags)  1 if any of `flags` is set in `x`, else 0.
        PF_BIT(type, n)         Bit `n` as `type`.
        PF_MASK(type, n)        The low `n` bits set, `n` from 0 to the width
                                of `type`, which must be unsigned.
        PF_BIT_SET(x, n), PF_BIT_CLEAR(x, n), PF_BIT_TOGGLE(x, n)
        PF_BIT_TEST(x, n)       1 if bit `n` of `x` is set, else 0.

    Arrays and enums:
        PF_COUNTOF(array)       The number of elements. Fails to compile when
                                given a pointer (GCC, Clang and C++).
        PF_ENUM_GUARD(name, count)
                                A static assertion that `name##_sentinel`, the
                                last enumerator, is `count`. Put it next to
                                code that handles every enumerator, so adding
                                one breaks the build there.

    Optimization hints:
        PF_EXPECT(expr, value)  `!!(expr)`, which is expected to be `value`
                                (0 or 1).
        PF_EXPECT_P(expr, value, probability)
                                The same, with a probability (0.0 to 1.0).
        PF_LIKELY(expr), PF_UNLIKELY(expr)
        PF_ASSUME(expr)         A statement, `PF_ASSUME(n > 0);`: the
                                compiler may assume `expr` is true. Undefined
                                behaviour when it is not. `expr` must not have
                                side effects; it may or may not be evaluated.
        PF_ASSUME_ALIGNED(ptr, align[, offset])
                                `ptr` as `void *`, which the compiler may
                                assume is aligned.

    Preprocessor:
        PF_STRINGIFY(...)       The arguments as a string, after expansion.
        PF_CONCAT(a, b)         `a` and `b` pasted, after expansion.
        PF_UNIQUE(name)         `name` with a suffix that makes it unique
                                (__COUNTER__, or __LINE__ where missing).
        PF_NARGS(...)           The number of arguments, up to 64. An empty
                                list is 0 in C23 and C++20, 1 before.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025-2026 Предраг Јовановић

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef CPOLYFILL_MACRO_H
#define CPOLYFILL_MACRO_H

#include <cpolyfill/platform.h>
#include <cpolyfill/stdc.h>
#include <cpolyfill/types.h>

#include <limits.h>
#include <stddef.h>

/* ---- Preprocessor ----------------------------------------------------- */

#define PF__STRINGIFY(...) #__VA_ARGS__
#define PF_STRINGIFY(...) PF__STRINGIFY(__VA_ARGS__)

#define PF__CONCAT(a, b) a##b
#define PF_CONCAT(a, b) PF__CONCAT(a, b)

#ifdef __COUNTER__
    #define PF_UNIQUE(name) PF_CONCAT(name, __COUNTER__)
#else
    #define PF_UNIQUE(name) PF_CONCAT(name, __LINE__)
#endif

/* MSVC's traditional preprocessor passes __VA_ARGS__ on as one argument
   unless it is rescanned. */
#define PF__EXPAND(x) x

#define PF__NARGS_N( \
    _1,              \
    _2,              \
    _3,              \
    _4,              \
    _5,              \
    _6,              \
    _7,              \
    _8,              \
    _9,              \
    _10,             \
    _11,             \
    _12,             \
    _13,             \
    _14,             \
    _15,             \
    _16,             \
    _17,             \
    _18,             \
    _19,             \
    _20,             \
    _21,             \
    _22,             \
    _23,             \
    _24,             \
    _25,             \
    _26,             \
    _27,             \
    _28,             \
    _29,             \
    _30,             \
    _31,             \
    _32,             \
    _33,             \
    _34,             \
    _35,             \
    _36,             \
    _37,             \
    _38,             \
    _39,             \
    _40,             \
    _41,             \
    _42,             \
    _43,             \
    _44,             \
    _45,             \
    _46,             \
    _47,             \
    _48,             \
    _49,             \
    _50,             \
    _51,             \
    _52,             \
    _53,             \
    _54,             \
    _55,             \
    _56,             \
    _57,             \
    _58,             \
    _59,             \
    _60,             \
    _61,             \
    _62,             \
    _63,             \
    _64,             \
    n,               \
    ...              \
)                    \
    n
#define PF__NARGS_LIST                                                      \
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, \
        46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
        29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, \
        12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define PF__NARGS(...) PF__EXPAND(PF__NARGS_N(__VA_ARGS__))

#if (PF_STD_C23 || PF_CPP >= 202002L)                     \
    && !(defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL)
    #define PF_NARGS(...) PF__NARGS(__VA_ARGS__ __VA_OPT__(, ) PF__NARGS_LIST)
#else
    #define PF_NARGS(...) PF__NARGS(__VA_ARGS__, PF__NARGS_LIST)
#endif

/* ---- Arithmetic ------------------------------------------------------- */

#if PF_CC_GNU || PF_CC_CLANG
    #define PF__STMT_EXPR 1
#else
    #define PF__STMT_EXPR 0
#endif

#if PF__STMT_EXPR
    /* Unique names keep nested uses from shadowing each other. */
    #define PF__MIN(x, y, a, b)    \
        (__extension__({           \
            __typeof__(x) a = (x); \
            __typeof__(y) b = (y); \
            a < b ? a : b;         \
        }))
    #define PF__MAX(x, y, a, b)    \
        (__extension__({           \
            __typeof__(x) a = (x); \
            __typeof__(y) b = (y); \
            a > b ? a : b;         \
        }))
    #define PF__ABS(x, a)          \
        (__extension__({           \
            __typeof__(x) a = (x); \
            a > 0 ? a : -a;        \
        }))
    #define PF__DIFF(x, y, a, b)   \
        (__extension__({           \
            __typeof__(x) a = (x); \
            __typeof__(y) b = (y); \
            a > b ? a - b : b - a; \
        }))

    #define PF_MIN(x, y) PF__MIN(x, y, PF_UNIQUE(pf__a), PF_UNIQUE(pf__b))
    #define PF_MAX(x, y) PF__MAX(x, y, PF_UNIQUE(pf__a), PF_UNIQUE(pf__b))
    #define PF_ABS(x) PF__ABS(x, PF_UNIQUE(pf__a))
    #define PF_DIFF(x, y) PF__DIFF(x, y, PF_UNIQUE(pf__a), PF_UNIQUE(pf__b))
#else
    #define PF_MIN(x, y) ((x) < (y) ? (x) : (y))
    #define PF_MAX(x, y) ((x) > (y) ? (x) : (y))
    #define PF_ABS(x) ((x) > 0 ? (x) : -(x))
    #define PF_DIFF(x, y) ((x) > (y) ? (x) - (y) : (y) - (x))
#endif

#define PF_CLAMP(x, lo, hi) PF_MAX(PF_MIN(x, hi), lo)

#if PF_HAS_TYPEOF
    #define PF__SWAP(x, y, t)     \
        do {                      \
            PF_TYPEOF(x) t = (x); \
            (x) = (y);            \
            (y) = t;              \
        } while (0)
    #define PF_SWAP(x, y) PF__SWAP(x, y, PF_UNIQUE(pf__t))
#else
    #define PF_SWAP(x, y)                                        \
        do {                                                     \
            unsigned char *pf__x = (unsigned char *)&(x);        \
            unsigned char *pf__y = (unsigned char *)&(y);        \
            size_t pf__i;                                        \
            PF_STATIC_ASSERT(sizeof(x) == sizeof(y), "PF_SWAP"); \
            for (pf__i = 0; pf__i < sizeof(x); pf__i++) {        \
                unsigned char pf__t = pf__x[pf__i];              \
                pf__x[pf__i] = pf__y[pf__i];                     \
                pf__y[pf__i] = pf__t;                            \
            }                                                    \
        } while (0)
#endif

/* ---- Pointers and alignment ------------------------------------------- */

#define PF_OFFSET(ptr, bytes) ((void *)((char *)(ptr) + (bytes)))
#define PF_PTRDIFF(a, b) ((const char *)(a) - (const char *)(b))

#define PF_IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)
/* No mask is widened, so the type of `align` does not matter. */
#define PF_ALIGN_PAD(x, align) ((0 - (x)) & ((align) - 1))
#define PF_ALIGN_DOWN(x, align) ((x) - ((x) & ((align) - 1)))
#define PF_ALIGN_UP(x, align) ((x) + PF_ALIGN_PAD(x, align))

/* The conditional makes the compiler compare the type of `ptr` with the
   type of the member, without evaluating the second operand. */
#define PF_CONTAINER_OF(ptr, type, member)                       \
    ((type *)(void *)((char *)(1 ? (ptr) : &((type *)0)->member) \
                      - offsetof(type, member)))

/* ---- Flags and bits --------------------------------------------------- */

#define PF_FLAG_SET(x, flags) ((x) | (flags))
#define PF_FLAG_TOGGLE(x, flags) ((x) ^ (flags))
#define PF_FLAG_TEST(x, flags) (((x) & (flags)) != 0)
#if PF_HAS_TYPEOF
    /* The complement must have the width of `x`: ~0x1u is only 32 bits. */
    #define PF_FLAG_CLEAR(x, flags) ((x) & ~(PF_TYPEOF(x))(flags))
#else
    #define PF_FLAG_CLEAR(x, flags) ((x) ^ ((x) & (flags)))
#endif

#define PF_BIT(type, n) ((type)((type)1 << (n)))
#define PF_MASK(type, n) \
    ((n) ? (type)((type) ~(type)0 >> (sizeof(type) * CHAR_BIT - (n))) : (type)0)

#if PF_HAS_TYPEOF
    #define PF__ONE_LIKE(x) ((PF_TYPEOF(x))1)
#else
    #define PF__ONE_LIKE(x) 1ULL
#endif
#define PF_BIT_SET(x, n) ((x) | (PF__ONE_LIKE(x) << (n)))
#define PF_BIT_CLEAR(x, n) ((x) & ~(PF__ONE_LIKE(x) << (n)))
#define PF_BIT_TOGGLE(x, n) ((x) ^ (PF__ONE_LIKE(x) << (n)))
#define PF_BIT_TEST(x, n) ((((x) >> (n)) & 1) != 0)

/* ---- Arrays and enums ------------------------------------------------- */

#ifdef __cplusplus
extern "C++" {
template <class T, size_t N> char (&pf__countof(T (&)[N]))[N];
}
    #define PF_COUNTOF(array) sizeof(pf__countof(array))
#elif PF_HAS_TYPE_CHECKS
    /* A bit-field of negative width fails to compile. */
    #define PF_COUNTOF(array)                                                \
        (sizeof(array) / sizeof((array)[0])                                  \
         + 0 * sizeof(struct {                                               \
               int pf__countof_needs_an_array : PF_IS_ARRAY(array) ? 1 : -1; \
           }))
#else
    /* Division by zero (a warning) for most pointers. */
    #define PF_COUNTOF(array)                                \
        ((sizeof(array) / sizeof(0 [array]))                 \
         / ((size_t)(!(sizeof(array) % sizeof(0 [array])))))
#endif

#define PF_ENUM_GUARD(name, count)                             \
    PF_STATIC_ASSERT(                                          \
        name##_sentinel == (count),                            \
        "enum " #name " changed: update the code guarded here" \
    )

/* ---- Optimization hints ----------------------------------------------- */

#if PF_HAS_BUILTIN(__builtin_expect) || PF_CC_GNU
    #define PF_EXPECT(expr, value) __builtin_expect(!!(expr), (value))
#else
    #define PF_EXPECT(expr, value) (!!(expr))
#endif

#if PF_HAS_BUILTIN(__builtin_expect_with_probability)
    #define PF_EXPECT_P(expr, value, probability)                           \
        __builtin_expect_with_probability(!!(expr), (value), (probability))
#else
    #define PF_EXPECT_P(expr, value, probability) PF_EXPECT(expr, value)
#endif

#define PF_LIKELY(expr) PF_EXPECT(expr, 1)
#define PF_UNLIKELY(expr) PF_EXPECT(expr, 0)

#if PF_HAS_BUILTIN(__builtin_assume)
    #define PF_ASSUME(expr) __builtin_assume(expr)
#elif PF_CPP >= 202302L
    #define PF_ASSUME(expr) [[assume(expr)]]
#elif (PF_CC_GNU || PF_CC_CLANG) && PF_HAS_ATTRIBUTE(assume)
    #define PF_ASSUME(expr) __attribute__((__assume__(expr)))
#elif PF_CC_MSVC
    #define PF_ASSUME(expr) __assume(expr)
#elif PF_CC_GNU
    #define PF_ASSUME(expr)              \
        do {                             \
            if (!(expr))                 \
                __builtin_unreachable(); \
        } while (0)
#else
    #define PF_ASSUME(expr) ((void)0)
#endif

#if PF_HAS_BUILTIN(__builtin_assume_aligned) || PF_GCC_PREREQ(4, 7)
    #define PF_ASSUME_ALIGNED(ptr, ...)              \
        __builtin_assume_aligned((ptr), __VA_ARGS__)
#else
    #define PF_ASSUME_ALIGNED(ptr, ...) ((void *)(ptr))
#endif

#endif /* CPOLYFILL_MACRO_H */
