/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/dlfcn.h>: the system's <dlfcn.h>, or LoadLibrary on
    Windows.
*/

#include "internal.h"

#include <cpolyfill/dlfcn.h>
#include <cpolyfill/stdc.h>

#include <string.h>

#if PF_OS_WINDOWS
    #include <psapi.h>
    #include <stdio.h>
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

/* A failure of the library itself (bad arguments), reported by
   pf_dlerror before the system's. */
static PF_THREAD_LOCAL const char *pf__dl_own_error;

#if PF_OS_WINDOWS

static PF_THREAD_LOCAL char pf__dl_msg[512];
static PF_THREAD_LOCAL bool pf__dl_failed;
static PF_THREAD_LOCAL char pf__dl_fname[MAX_PATH * 3 + 1];

/* Records the failure of `what` with GetLastError(). */
static void pf__dl_fail(const char *what, const char *name) {
    DWORD err = GetLastError();
    wchar_t wmsg[256];
    char msg[400];
    DWORD n = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        0,
        wmsg,
        sizeof(wmsg) / sizeof(wmsg[0]),
        NULL
    );
    /* FormatMessage ends the text with "\r\n". */
    while (n > 0
           && (wmsg[n - 1] == L'\r' || wmsg[n - 1] == L'\n'
               || wmsg[n - 1] == L' ' || wmsg[n - 1] == L'.'))
        n--;
    if (n == 0
        || pf_utf16_to_utf8(
               (const uint16_t *)wmsg, n, msg, sizeof(msg), PF_UNICODE_REPLACE
           ) < 0)
        snprintf(msg, sizeof(msg), "error %lu", (unsigned long)err);
    snprintf(
        pf__dl_msg,
        sizeof(pf__dl_msg),
        "%s%s%s: %s",
        what,
        name ? " " : "",
        name ? name : "",
        msg
    );
    pf__dl_failed = true;
}

PF_API void *pf_dlopen(const char *path, int flags) {
    pf__wstr w;
    HMODULE mod;
    DWORD old_mode;
    int rc;

    (void)flags;
    pf__dl_own_error = NULL;
    if (!path)
        return (void *)GetModuleHandleW(NULL);
    if ((rc = pf__wpath(&w, path)) != 0) {
        pf__dl_own_error = pf_strerror(rc);
        return NULL;
    }
    /* No "cannot find DLL" dialog boxes. */
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &old_mode);
    mod = LoadLibraryExW((LPCWSTR)w.ptr, NULL, 0);
    if (!mod)
        pf__dl_fail("cannot load", path);
    SetThreadErrorMode(old_mode, NULL);
    pf__wstr_free(&w);
    return (void *)mod;
}

PF_API void *pf_dlsym(void *handle, const char *name) {
    FARPROC p = NULL;

    pf__dl_own_error = NULL;
    if (!name) {
        pf__dl_own_error = "no symbol name";
        return NULL;
    }
    if (handle == PF_RTLD_DEFAULT) {
        HMODULE mods[1024];
        DWORD bytes, k, count;
        if (!EnumProcessModules(
                GetCurrentProcess(), mods, sizeof(mods), &bytes
            )) {
            pf__dl_fail("cannot list the modules for", name);
            return NULL;
        }
        count = bytes / sizeof(HMODULE);
        if (count > sizeof(mods) / sizeof(mods[0]))
            count = sizeof(mods) / sizeof(mods[0]);
        for (k = 0; k < count && !p; k++)
            p = GetProcAddress(mods[k], name);
        if (!p)
            SetLastError(ERROR_PROC_NOT_FOUND);
    } else {
        p = GetProcAddress((HMODULE)handle, name);
    }
    if (!p) {
        pf__dl_fail("cannot find symbol", name);
        return NULL;
    }
    /* A function pointer as data pointer, as POSIX dlsym does. */
    {
        void *v;
        memcpy(&v, &p, sizeof(v));
        return v;
    }
}

PF_API int pf_dlclose(void *handle) {
    pf__dl_own_error = NULL;
    if (!handle) {
        pf__dl_own_error = "invalid handle";
        return -1;
    }
    /* The program's own handle is not counted. */
    if ((HMODULE)handle == GetModuleHandleW(NULL))
        return 0;
    if (!FreeLibrary((HMODULE)handle)) {
        pf__dl_fail("cannot unload", NULL);
        return -1;
    }
    return 0;
}

PF_API char *pf_dlerror(void) {
    const char *own = pf__dl_own_error;

    if (own) {
        pf__dl_own_error = NULL;
        pf__dl_failed = false;
        return (char *)own;
    }
    if (!pf__dl_failed)
        return NULL;
    pf__dl_failed = false;
    return pf__dl_msg;
}

PF_API int pf_dladdr(const void *addr, pf_dl_info_t *info) {
    HMODULE mod;
    wchar_t wname[MAX_PATH + 1];
    DWORD n;

    if (!info)
        return 0;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)addr,
            &mod
        ))
        return 0;
    n = GetModuleFileNameW(mod, wname, MAX_PATH + 1);
    if (n == 0 || n > MAX_PATH
        || pf__path_from_wide(
               (const uint16_t *)wname, n, pf__dl_fname, sizeof(pf__dl_fname)
           ) < 0)
        pf__dl_fname[0] = 0;
    info->dli_fname = pf__dl_fname;
    info->dli_fbase = (void *)mod;
    info->dli_sname = NULL;
    info->dli_saddr = NULL;
    return 1;
}

#else /* POSIX */

PF_API void *pf_dlopen(const char *path, int flags) {
    int mode = 0;

    pf__dl_own_error = NULL;
    mode |= (flags & PF_RTLD_NOW) ? RTLD_NOW : RTLD_LAZY;
    mode |= (flags & PF_RTLD_GLOBAL) ? RTLD_GLOBAL : RTLD_LOCAL;
    return dlopen(path, mode);
}

PF_API void *pf_dlsym(void *handle, const char *name) {
    pf__dl_own_error = NULL;
    if (!name) {
        pf__dl_own_error = "no symbol name";
        return NULL;
    }
    (void)dlerror(); /* report this call's failure only */
    return dlsym(handle == PF_RTLD_DEFAULT ? RTLD_DEFAULT : handle, name);
}

PF_API int pf_dlclose(void *handle) {
    pf__dl_own_error = NULL;
    if (!handle) {
        pf__dl_own_error = "invalid handle";
        return -1;
    }
    return dlclose(handle);
}

PF_API char *pf_dlerror(void) {
    const char *own = pf__dl_own_error;

    if (own) {
        pf__dl_own_error = NULL;
        (void)dlerror();
        return (char *)own;
    }
    return dlerror();
}

PF_API int pf_dladdr(const void *addr, pf_dl_info_t *info) {
    #if PF__HAVE_DLADDR
    Dl_info di;

    if (!info || !dladdr(addr, &di))
        return 0;
    info->dli_fname = di.dli_fname;
    info->dli_fbase = di.dli_fbase;
    info->dli_sname = di.dli_sname;
    info->dli_saddr = di.dli_saddr;
    return 1;
    #else
    (void)addr, (void)info;
    return 0;
    #endif
}

#endif
