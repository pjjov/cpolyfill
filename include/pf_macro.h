/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a collection of common utilities and macros, and
    hardens them with compiler builtins to ensure as much safety as possible.

        PF_ABS(x)
            Returns the absolute value of parameter `x`.
        PF_DIFF(x, y)
            Returns the absolute difference of parameters `x` and `y`.
        PF_MAX(x, y)
            Returns the argument with the largest value.
        PF_MIN(x, y)
            Returns the argument with the smallest value.
        PF_CLAMP(x, min, max)
            Returns the clamped value of x between min and max.
        PF_SWAP(x, y)
            Swaps the values of given variables `x` and `y`.
        PF_OFFSET(ptr, offset)
            Returns a new pointer with a given byte offset.
        PF_PTRDIFF(x, y)
            Returns the difference between two pointers in bytes.
        PF_ALIGN_PAD(x, align)
            Returns the padding required to align `x` to `PF_ALIGN_CEIL(x, align)`.
        PF_ALIGN_CEIL(x, align) and PF_ALIGN_UP(x, align)
            Return the value equal or larger to `x` with given alignment.
        PF_ALIGN_FLOOR(x, align) and PF_ALIGN_DOWN(x, align)
            Return the value equal or smaller to `x` with given alignment.
        PF_FLAG_SET(x, flag)
            Returns a copy of `x` with `flag` turned on.
        PF_FLAG_CLEAR(x, flag)
            Returns a copy of `x` with `flag` turned off.
        PF_FLAG_TOGGLE(x, flag)
            Returns a copy of `x` with `flag` toggled.
        PF_FLAG_TEST(x, flag)
            Checks if `flag` is turned on in `x`.
        PF_MASK(type, bit)
            Returns a constant of `type` with bits under `bit` set to 1.
        PF_BIT(type, bit)
            Returns a constant of `type` with `bit` set to 1.
        PF_BIT_SET(x, bit)
            Returns a copy of `x` with `bit` turned on.
        PF_BIT_CLEAR(x, bit)
            Returns a copy of `x` with `bit` turned off.
        PF_BIT_TOGGLE(x, bit)
            Returns a copy of `x` with `bit` toggled.
        PF_BIT_TEST(x, bit)
            Checks if `bit` is turned on in `x`.
        PF_COUNTOF(array)
            Returns the number of elements inside `array`.
        PF_EXPECT(expr, expect)
            Annotate that the value of `expr` is expected to be `expect`.
        PF_EXPECT_P(expr, expect, prop)
            Annotate that the value of `expr` is expected to be `expect`
            with given probability `p`, between 0.0 and 1.0.
        PF_UNREACHABLE
            Annotate that following piece of code cannot be reached.
        PF_ASSUME_ALIGNED(expr, align, ...)
            Annotate that the value of `expr` should have the given alignment.
        pf_container_of(ptr, type, member)
            Returns a pointer of `type` using the offset of it's member `ptr`.
        PF_HAS_STMT_EXPR
            This macro is defined if the compiler supports GCC's extension
            for statement expressions. Used for ensuring type safety.

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

#ifndef POLYFILL_MACRO
#define POLYFILL_MACRO

#include <stddef.h>

#ifndef pf_has_builtin
    #ifdef __has_builtin
        #define pf_has_builtin(x) __has_builtin(x)
    #else
        #define pf_has_builtin(x) 0
    #endif
#endif

#ifndef PF_HAS_STMT_EXPR
    #if !__STRICT_ANSI__ && __GNUC__ >= 3
        #define PF_HAS_STMT_EXPR
    #endif
#endif

#ifdef PF_HAS_STMT_EXPR
    #define PF_MAX(x, y)        \
        ({                      \
            typeof(x) _x = (x); \
            typeof(y) _y = (y); \
            _x > _y ? _x : _y;  \
        })

    #define PF_MIN(x, y)        \
        ({                      \
            typeof(x) _x = (x); \
            typeof(y) _y = (y); \
            _x < _y ? _x : _y;  \
        })

    #define PF_ABS(x)           \
        ({                      \
            typeof(x) _x = (x); \
            _x > 0 ? _x : -_x;  \
        })

    #define PF_DIFF(x, y)                \
        ({                               \
            typeof(x) _x = (x);          \
            typeof(y) _y = (y);          \
            _x > _y ? _x - _y : _y - _x; \
        })
#else
    #define PF_MAX(x, y) ((x) > (y) ? (x) : (y))
    #define PF_MIN(x, y) ((x) < (y) ? (x) : (y))
    #define PF_ABS(x) ((x) > (0) ? (x) : -(x))
    #define PF_DIFF(x, y) ((x) > (y) ? (x) - (y) : (y) - (x))
#endif

#define PF_CLAMP(x, min, max) PF_MAX(PF_MIN(x, max), min)

#define PF_SWAP(x, y)         \
    ({                        \
        typeof(x) _tmp = (x); \
        x = (y);              \
        y = _tmp;             \
    })

#define PF_OFFSET(ptr, offset) ((void *)(&((char *)(ptr))[offset]))
#define PF_PTRDIFF(x, y) (((char *)(x)) - ((char *)(y)))

#ifndef PF__ALIGN
    #define PF__ALIGN
    #define PF_ALIGN_PAD(x, a) ((~(x) + 1) & ((a) - 1))
    #define PF_ALIGN_DOWN(x, a) ((x) & ~((typeof(x))(a) - 1))
    #define PF_ALIGN_UP(x, a)                                  \
        (((x) + ((typeof(x))(a) - 1)) & ~((typeof(x))(a) - 1))

    #define PF_ALIGN_CEIL PF_ALIGN_UP
    #define PF_ALIGN_FLOOR PF_ALIGN_DOWN
#endif

#define PF_FLAG_SET(x, flag) ((x) | (flag))
#define PF_FLAG_CLEAR(x, flag) ((x) & ~((typeof(x))flag))
#define PF_FLAG_TOGGLE(x, flag) ((x) ^ ((typeof(x))flag))
#define PF_FLAG_TEST(x, flag) ((x) & (flag))

#define PF_MASK(type, bit) (((type)1 << (bit)) - 1)
#define PF_BIT(type, bit) ((type)1 << (bit))
#define PF_BIT_SET(x, bit) ((x) | ((typeof(x))1 << (bit)))
#define PF_BIT_CLEAR(x, bit) ((x) & ~((typeof(x))1 << (bit)))
#define PF_BIT_TOGGLE(x, bit) ((x) ^ ((typeof(x))1 << (bit)))
#define PF_BIT_TEST(x, bit) ((x) & ((typeof(x))1 << (bit)))

#ifndef pf_container_of
    #define pf_container_of(ptr, type, member)               \
        ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) \
                  - offsetof(type, member)))
#endif

#define PF_COUNTOF(array)                                \
    ((sizeof(array) / sizeof(0 [array]))                 \
     / ((size_t)(!(sizeof(array) % sizeof(0 [array])))))

#if pf_has_builtin(__builtin_expect)
    #define PF_EXPECT(expr, expect) (__builtin_expect((expr) != 0, (expect)))
#else
    #define PF_EXPECT(expr, expect)
#endif

#if pf_has_builtin(__builtin_expect_with_probability)
    #define PF_EXPECT_P(expr, expect, prob)                                \
        (__builtin_expect_with_probability((expr) != 0, (expect), (prob)))
#else
    #define PF_EXPECT_P(expr, expect, prob)
#endif

#if pf_has_builtin(__builtin_unreachable)
    #define PF_UNREACHABLE __builtin_unreachable()
#else
    #define PF_UNREACHABLE
#endif

#if pf_has_builtin(__builtin_assume_aligned)
    #define PF_ASSUME_ALIGNED __builtin_assume_aligned
#else
    #define PF_ASSUME_ALIGNED(expr, align, ...) (expr)
#endif

#endif
