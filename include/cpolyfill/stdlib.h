/** cpolyfill - Filling the gaps between C standards and compilers

    General utilities missing from some C libraries. Strings are UTF-8 on
    every platform (on Windows the environment functions convert to and
    from UTF-16).

        pf_ssize_t              Signed size: `ssize_t`, or `ptrdiff_t` on
                                MSVC.

        void *pf_aligned_alloc(size_t align, size_t size)
                Memory aligned to `align` (a power of two; smaller values
                than `sizeof(void *)` are raised to it), or NULL with errno
                EINVAL or ENOMEM. `size` need not be a multiple of `align`.
        void pf_aligned_free(void *p)
                Releases memory from pf_aligned_alloc (on Windows `free`
                cannot). NULL is ignored.
        void *pf_reallocarray(void *p, size_t n, size_t size)
                realloc(p, n * size), or NULL with errno ENOMEM if the
                product overflows.

        pf_ssize_t pf_getline(char **line, size_t *cap, FILE *f)
        pf_ssize_t pf_getdelim(char **line, size_t *cap, int delim, FILE *f)
                Read up to and including the next newline (`delim`) into
                `*line`, which is grown with `realloc` as needed (it may
                start as NULL with `*cap` 0). Return the number of bytes
                read, or -1 at end of file or on error (errno set).

        ptrdiff_t pf_getenv(const char *name, char *buf, size_t size)
                Copies the value of an environment variable, NUL-terminated.
                Returns its length, or PF_ENOENT if it is not set,
                PF_ERANGE if `size` is too small, PF_EILSEQ if it is not
                valid UTF-16 (Windows) or PF_EINVAL. With `buf == NULL`
                only the length is returned.
        int pf_setenv(const char *name, const char *value, int overwrite)
        int pf_unsetenv(const char *name)
                POSIX setenv and unsetenv: 0, or -1 with errno EINVAL (empty
                name or one containing '=') or ENOMEM.
                None of these are safe to call while another thread changes
                the environment.

        void pf_qsort_r(void *base, size_t n, size_t size,
                        int (*cmp)(const void *a, const void *b, void *arg),
                        void *arg)
                qsort with an extra argument for `cmp`, in the POSIX (2024)
                and glibc order. Not stable.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_STDLIB, where the
    library found them missing when it was built: `reallocarray`,
    `getline`, `getdelim`, `setenv`, `unsetenv`, `qsort_r` (also where it
    has the older BSD argument order), and on MSVC `ssize_t`.
    `aligned_alloc` is not mapped: memory from it must be released with
    pf_aligned_free on Windows, not `free`.

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

#ifndef CPOLYFILL_STDLIB_H
#define CPOLYFILL_STDLIB_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if PF_OS_WINDOWS && !PF_CC_MINGW
typedef ptrdiff_t pf_ssize_t;
#else
    #include <sys/types.h>
typedef ssize_t pf_ssize_t;
#endif

PF_BEGIN_DECLS

PF_MALLOC_LIKE PF_API void *pf_aligned_alloc(size_t align, size_t size);
PF_API void pf_aligned_free(void *p);
PF_API void *pf_reallocarray(void *p, size_t n, size_t size);

PF_API pf_ssize_t pf_getline(char **line, size_t *cap, FILE *f);
PF_API pf_ssize_t pf_getdelim(char **line, size_t *cap, int delim, FILE *f);

PF_API ptrdiff_t pf_getenv(const char *name, char *buf, size_t size);
PF_API int pf_setenv(const char *name, const char *value, int overwrite);
PF_API int pf_unsetenv(const char *name);

PF_API void pf_qsort_r(
    void *base,
    size_t n,
    size_t size,
    int (*cmp)(const void *a, const void *b, void *arg),
    void *arg
);

PF_END_DECLS

#endif /* CPOLYFILL_STDLIB_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__STDLIB_STD_NAMES)                                    \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_STDLIB))   \
        || defined(PF_STD_NAMES_STDLIB))
    #define PF__STDLIB_STD_NAMES

    #ifdef PF__FORCE_STD_NAMES_POLYFILL
        #define PF__STDLIB_MAP(have) 1
    #else
        #define PF__STDLIB_MAP(have) (!(have))
    #endif

    #if PF_OS_WINDOWS && !PF_CC_MINGW && !defined(_SSIZE_T_DEFINED)
        #define _SSIZE_T_DEFINED
typedef pf_ssize_t ssize_t;
    #endif

    #if PF__STDLIB_MAP(PF_CONFIG_HAVE_REALLOCARRAY)
        #undef reallocarray
        #define reallocarray pf_reallocarray
    #endif
    #if PF__STDLIB_MAP(PF_CONFIG_HAVE_GETLINE)
        #undef getline
        #define getline pf_getline
    #endif
    #if PF__STDLIB_MAP(PF_CONFIG_HAVE_GETDELIM)
        #undef getdelim
        #define getdelim pf_getdelim
    #endif
    #if PF__STDLIB_MAP(PF_CONFIG_HAVE_SETENV)
        #undef setenv
        #define setenv pf_setenv
    #endif
    #if PF__STDLIB_MAP(PF_CONFIG_HAVE_UNSETENV)
        #undef unsetenv
        #define unsetenv pf_unsetenv
    #endif
    #if PF__STDLIB_MAP(PF_CONFIG_HAVE_QSORT_R)
        #undef qsort_r
        #define qsort_r pf_qsort_r
    #endif
#endif
