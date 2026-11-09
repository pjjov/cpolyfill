/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/attribute.h>
#include <cpolyfill/debug.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set by meson. */
#ifndef PF__HAVE_EXECINFO
    #define PF__HAVE_EXECINFO 0
#endif
#ifndef PF__HAVE_UNWIND
    #define PF__HAVE_UNWIND 0
#endif
#ifndef PF__HAVE_DLADDR
    #define PF__HAVE_DLADDR 0
#endif

#if PF_OS_WINDOWS
    #include <windows.h>
#elif PF_OS_APPLE || PF_OS_BSD
    #include <sys/param.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #if PF_OS_FREEBSD || PF_OS_DRAGONFLY
        #include <sys/user.h>
    #endif
    #include <unistd.h>
#endif

#if !PF_OS_WINDOWS
    #if PF__HAVE_EXECINFO
        #include <execinfo.h>
    #elif PF__HAVE_UNWIND
        #include <unwind.h>
    #endif
    #if PF__HAVE_DLADDR
        #include <dlfcn.h>
    #endif
#endif

/* ---- Debugger --------------------------------------------------------- */

PF_API bool pf_debugger_attached(void) {
#if PF_OS_WINDOWS
    return IsDebuggerPresent() != 0;
#elif PF_OS_LINUX
    /* "TracerPid:\t<pid>", 0 when not traced. */
    char buf[4096];
    size_t n;
    const char *p;
    FILE *f = fopen("/proc/self/status", "r");
    if (!f)
        return false;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    p = strstr(buf, "TracerPid:");
    if (!p)
        return false;
    p += strlen("TracerPid:");
    while (*p == ' ' || *p == '\t')
        p++;
    return *p >= '1' && *p <= '9';
#elif PF_OS_APPLE || PF_OS_FREEBSD || PF_OS_DRAGONFLY
    struct kinfo_proc info;
    size_t size = sizeof(info);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid() };
    memset(&info, 0, sizeof(info));
    if (sysctl(mib, 4, &info, &size, NULL, 0) != 0)
        return false;
    #if PF_OS_APPLE
    return (info.kp_proc.p_flag & P_TRACED) != 0;
    #elif PF_OS_FREEBSD
    return (info.ki_flag & P_TRACED) != 0;
    #else
    return (info.kp_flags & P_TRACED) != 0;
    #endif
#elif PF_OS_NETBSD
    struct kinfo_proc2 info;
    size_t size = sizeof(info);
    int mib[6] = { CTL_KERN,      KERN_PROC2,        KERN_PROC_PID,
                   (int)getpid(), (int)sizeof(info), 1 };
    if (sysctl(mib, 6, &info, &size, NULL, 0) != 0)
        return false;
    return (info.p_flag & P_TRACED) != 0;
#elif PF_OS_OPENBSD
    struct kinfo_proc info;
    size_t size = sizeof(info);
    int mib[6] = { CTL_KERN,      KERN_PROC,         KERN_PROC_PID,
                   (int)getpid(), (int)sizeof(info), 1 };
    if (sysctl(mib, 6, &info, &size, NULL, 0) != 0)
        return false;
    return (info.p_psflags & PS_TRACED) != 0;
#else
    return false;
#endif
}

/* ---- Backtrace -------------------------------------------------------- */

#if !PF_OS_WINDOWS && !PF__HAVE_EXECINFO && PF__HAVE_UNWIND
struct pf__unwind {
    void **frames;
    int max;
    int skip;
    int n;
};

static _Unwind_Reason_Code pf__unwind_frame(
    struct _Unwind_Context *ctx, void *arg
) {
    struct pf__unwind *u = (struct pf__unwind *)arg;
    uintptr_t ip = (uintptr_t)_Unwind_GetIP(ctx);

    if (!ip)
        return _URC_END_OF_STACK;
    if (u->skip > 0) {
        u->skip--;
        return _URC_NO_REASON;
    }
    if (u->n >= u->max)
        return _URC_END_OF_STACK;
    u->frames[u->n++] = (void *)ip;
    return _URC_NO_REASON;
}
#endif

/* Up to `max` frames, without the `skip` innermost ones and this
   function. */
PF_NOINLINE static int pf__backtrace(void **frames, int max, int skip) {
    if (!frames || max <= 0)
        return 0;
    skip++;
#if PF_OS_WINDOWS
    return (int)CaptureStackBackTrace((DWORD)skip, (DWORD)max, frames, NULL);
#elif PF__HAVE_EXECINFO
    {
        void *small[128];
        void **all = small;
        int want = max + skip, n;
        if (want > (int)(sizeof(small) / sizeof(small[0]))) {
            all = (void **)malloc(sizeof(void *) * (size_t)want);
            if (!all)
                return PF_ENOMEM;
        }
        n = backtrace(all, want);
        n = n > skip ? n - skip : 0;
        memcpy(frames, all + skip, sizeof(void *) * (size_t)n);
        if (all != small)
            free(all);
        return n;
    }
#elif PF__HAVE_UNWIND
    {
        struct pf__unwind u;
        u.frames = frames;
        u.max = max;
        u.skip = skip;
        u.n = 0;
        _Unwind_Backtrace(pf__unwind_frame, &u);
        return u.n;
    }
#else
    (void)skip;
    return PF_ENOSYS;
#endif
}

PF_NOINLINE PF_API int pf_backtrace(void **frames, int max) {
    return pf__backtrace(frames, max, 1);
}

/* One frame: "#n 0xaddr <symbol>". */
static void pf__print_frame(FILE *out, int i, void *addr) {
#if PF_OS_WINDOWS
    HMODULE module = NULL;
    wchar_t wide[MAX_PATH];
    char name[MAX_PATH * 3];
    const char *base = name;
    char *p;
    DWORD n;

    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)addr,
            &module
        )
        && (n = GetModuleFileNameW(module, wide, MAX_PATH)) > 0 && n < MAX_PATH
        && pf__utf16_to_utf8((const uint16_t *)wide, n, name, sizeof(name))
            >= 0) {
        for (p = name; *p; p++)
            if (*p == '\\' || *p == '/')
                base = p + 1;
        fprintf(
            out,
            "#%-2d %p %s+0x%llx\n",
            i,
            addr,
            base,
            (unsigned long long)((uintptr_t)addr - (uintptr_t)module)
        );
        return;
    }
#elif PF__HAVE_DLADDR
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_fname) {
        const char *base = strrchr(info.dli_fname, '/');
        base = base ? base + 1 : info.dli_fname;
        if (info.dli_sname)
            fprintf(
                out,
                "#%-2d %p %s+0x%llx (%s)\n",
                i,
                addr,
                info.dli_sname,
                (unsigned long long)((uintptr_t)addr
                                     - (uintptr_t)info.dli_saddr),
                base
            );
        else
            fprintf(
                out,
                "#%-2d %p %s+0x%llx\n",
                i,
                addr,
                base,
                (unsigned long long)((uintptr_t)addr
                                     - (uintptr_t)info.dli_fbase)
            );
        return;
    }
#endif
    fprintf(out, "#%-2d %p\n", i, addr);
}

PF_NOINLINE PF_API void pf_backtrace_print(FILE *out) {
    void *frames[64];
    int n = pf__backtrace(frames, 64, 1), i;

    if (!out)
        out = stderr;
    if (n <= 0) {
        fprintf(out, "(no backtrace available)\n");
        fflush(out);
        return;
    }
    for (i = 0; i < n; i++)
        pf__print_frame(out, i, frames[i]);
    fflush(out);
}
