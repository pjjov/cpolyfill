/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides portable macros for annotating functions,
    variables and types using compiler and C23 attributes.

    SPDX-FileCopyrightText: 2026 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Predrag Jovanović

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

#ifndef POLYFILL_ATTRIBUTE
#define POLYFILL_ATTRIBUTE

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define PF_STD_C23
#endif

#ifdef __attribute__
    #define pf_attribute __attribute__
#endif

#ifdef __has_attribute
    #define pf_has_attribute __has_attribute
#elif defined(__has_builtin)
    #if __has_builtin(__builtin_has_attribute)
        #define pf_has_attribute __builtin_has_attribute
    #endif
#endif

#ifndef pf_attribute
    #define pf_attribute(...)
#endif

#ifndef pf_has_attribute
    #define pf_has_attribute(...) 0
#endif

#ifndef PF_DEPRECATED
    #ifdef PF_STD_C23
        #define PF_DEPRECATED(reason) [[deprecated(reason)]]
    #elif pf_has_attribute(deprecated)
        #define PF_DEPRECATED(reason) pf_attribute((deprecated(reason)))
    #else
        #define PF_DEPRECATED(reason)
    #endif
#endif

#ifndef PF_FALLTHROUGH
    #ifdef PF_STD_C23
        #define PF_FALLTHROUGH [[fallthrough]]
    #elif pf_has_attribute(fallthrough)
        #define PF_FALLTHROUGH pf_attribute((fallthrough))
    #else
        #define PF_FALLTHROUGH
    #endif
#endif

#ifndef PF_NORETURN
    #ifdef PF_STD_C23
        #define PF_NORETURN [[noreturn]]
    #elif pf_has_attribute(noreturn)
        #define PF_NORETURN pf_attribute((noreturn))
    #else
        #define PF_NORETURN
    #endif
#endif

#ifndef PF_NODISCARD
    #ifdef PF_STD_C23
        #define PF_NODISCARD(reason) [[nodiscard(reason)]]
    #elif pf_has_attribute(warn_unused_result)
        #define PF_NODISCARD(reason) pf_attribute((warn_unused_result))
    #else
        #define PF_NODISCARD(reason)
    #endif
#endif

#ifndef PF_REPRODUCIBLE
    #ifdef PF_STD_C23
        #define PF_REPRODUCIBLE [[reproducible]]
    #elif pf_has_attribute(reproducible)
        #define PF_REPRODUCIBLE pf_attribute((reproducible))
    #else
        #define PF_REPRODUCIBLE
    #endif
#endif

#ifndef PF_UNUSED
    #ifdef PF_STD_C23
        #define PF_UNUSED [[unused]]
    #elif pf_has_attribute(unused)
        #define PF_UNUSED pf_attribute((unused))
    #else
        #define PF_UNUSED
    #endif
#endif

#ifndef PF_UNSEQUENCED
    #ifdef PF_STD_C23
        #define PF_UNSEQUENCED [[unsequenced]]
    #elif pf_has_attribute(unsequenced)
        #define PF_UNSEQUENCED pf_attribute((unsequenced))
    #else
        #define PF_UNSEQUENCED
    #endif
#endif

#endif
