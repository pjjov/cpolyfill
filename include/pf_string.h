/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides some non-standard, but widely available string and
    string-related functions across platforms.

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

#ifndef POLYFILL_STRING
#define POLYFILL_STRING

#ifndef PF_API
    #define PF_API static inline
#endif

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if __STDC_VERSION__ >= 201112L && __STDC_LIB_EXT1__
    #define PF_HAS_STRNLEN_S
#endif

#if defined(_UCRT) || (defined(_MSC_VER) && (_MSC_VER >= 1400))
    #define PF_HAS_STRNLEN
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
    #define PF_HAS_STRNLEN
#endif

PF_API size_t pf_strnlen(const char *str, size_t max) {
    if (!str)
        return 0;

    for (size_t i = 0; i < max; i++)
        if (str[i] == '\0')
            return i;

    return max;
}

#ifndef PF_HAS_STRNLEN_S
    #ifdef PF_HAS_STRNLEN
        #define strnlen_s strnlen
    #else
        #define strnlen_s pf_strnlen
    #endif
#endif

#ifndef PF_HAS_STRNLEN
    #ifdef PF_HAS_STRNLEN_S
        #define strnlen strnlen_s
    #else
        #define strnlen pf_strnlen
    #endif
#endif

#if defined(_MSC_VER)

typedef _locale_t locale_t;

    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define wcscasecmp _wcsicmp
    #define wcsncasecmp _wcsnicmp

    #define strcasecmp_l _stricmp_l
    #define strncasecmp_l _strnicmp_l
    #define wcscasecmp_l _wcsicmp_l
    #define wcsncasecmp_l _wcsnicmp_l

    #define newlocale(category, locale_str, base) \
        _create_locale(LC_CTYPE, locale_str)
    #define freelocale(loc) _free_locale(loc)

#else
    #include <strings.h>
#endif

#endif
