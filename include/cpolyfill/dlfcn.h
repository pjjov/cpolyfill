/** cpolyfill - Filling the gaps between C standards and compilers

    Loading shared libraries at run time, with the POSIX <dlfcn.h> API on
    every platform.

        void *pf_dlopen(const char *path, int flags)
                Loads a library (UTF-8 path; on Windows '/' works too), or
                with `path == NULL` returns a handle to the program itself.
                NULL on failure, see pf_dlerror. `flags`: PF_RTLD_LAZY or
                PF_RTLD_NOW, with PF_RTLD_GLOBAL or PF_RTLD_LOCAL; Windows
                ignores them (it binds immediately, every symbol is local
                to its module).
        void *pf_dlsym(void *handle, const char *name)
                The address of a symbol (function or variable), or NULL.
                With PF_RTLD_DEFAULT, the program and every loaded library
                are searched (on Windows: the modules in load order).
        int pf_dlclose(void *handle)
                0, or non-zero on failure.
        char *pf_dlerror(void)
                A description of the last failure of the functions above
                in this thread, or NULL if none happened since the last
                call (each failure is reported once).
        int pf_dladdr(const void *addr, pf_dl_info_t *info)
                The library holding `addr`: non-zero on success, filling
                dli_fname    the library's path
                dli_fbase    where it is loaded
                dli_sname    the nearest exported symbol at or before
                             `addr`, or NULL (always on Windows)
                dli_saddr    its address, or NULL
                The strings stay valid until the library is unloaded (on
                Windows: until the next call in the same thread).

        PF_DL_EXT               The usual file name extension of shared
                                libraries: ".so", ".dylib" (macOS) or
                                ".dll" (Windows).

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_DLFCN, only where the
    library found no <dlfcn.h> (Windows): `dlopen`, `dlsym`, `dlclose`,
    `dlerror`, `dladdr`, `Dl_info`, `RTLD_LAZY`, `RTLD_NOW`,
    `RTLD_GLOBAL`, `RTLD_LOCAL`, `RTLD_DEFAULT`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_DLFCN_H
#define CPOLYFILL_DLFCN_H

#include <cpolyfill/config.h>
#include <cpolyfill/platform.h>

#define PF_RTLD_LAZY 0x1
#define PF_RTLD_NOW 0x2
#define PF_RTLD_LOCAL 0x0
#define PF_RTLD_GLOBAL 0x100
#define PF_RTLD_DEFAULT ((void *)0)

#if PF_OS_WINDOWS
    #define PF_DL_EXT ".dll"
#elif PF_OS_APPLE
    #define PF_DL_EXT ".dylib"
#else
    #define PF_DL_EXT ".so"
#endif

typedef struct pf_dl_info_t {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} pf_dl_info_t;

PF_BEGIN_DECLS

PF_API void *pf_dlopen(const char *path, int flags);
PF_API void *pf_dlsym(void *handle, const char *name);
PF_API int pf_dlclose(void *handle);
PF_API char *pf_dlerror(void);
PF_API int pf_dladdr(const void *addr, pf_dl_info_t *info);

PF_END_DECLS

#endif /* CPOLYFILL_DLFCN_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__DLFCN_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_DLFCN)) \
        || defined(PF_STD_NAMES_DLFCN))
#define PF__DLFCN_STD_NAMES

#if !PF_CONFIG_HAVE_DLFCN_H
    #define RTLD_LAZY PF_RTLD_LAZY
    #define RTLD_NOW PF_RTLD_NOW
    #define RTLD_LOCAL PF_RTLD_LOCAL
    #define RTLD_GLOBAL PF_RTLD_GLOBAL
    #define RTLD_DEFAULT PF_RTLD_DEFAULT
typedef pf_dl_info_t Dl_info;
    #define dlopen pf_dlopen
    #define dlsym pf_dlsym
    #define dlclose pf_dlclose
    #define dlerror pf_dlerror
    #define dladdr pf_dladdr
#endif
#endif
