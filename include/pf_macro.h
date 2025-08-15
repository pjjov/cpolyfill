/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a collection of common utilities and macros, and
    hardens them with compiler builtins to ensure as much safety as possible.

        PF_ABS(x)
            Returns the absolute value of parameter `x`.
        PF_MAX(x, y)
            Returns the argument with the largest value.
        PF_MIN(x, y)
            Returns the argument with the smallest value.
        PF_COUNTOF(array)
            Returns the number of elements inside `array`.
        PF_HAS_STMT_EXPR
            This macro is defined if the compiler supports GCC's extension
            for statement expressions. Used for ensuring type safety.

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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
#else
    #define PF_MAX(x, y) ((x) > (y) ? (x) : (y))
    #define PF_MIN(x, y) ((x) < (y) ? (x) : (y))
    #define PF_ABS(x) ((x) > (0) ? (x) : -(x))
#endif

#define PF_COUNTOF(array)                                \
    ((sizeof(array) / sizeof(0 [array]))                 \
     / ((size_t)(!(sizeof(array) % sizeof(0 [array])))))

#endif
