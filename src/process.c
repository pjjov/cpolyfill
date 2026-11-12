/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/process.h>: the parts shared by the POSIX and Windows
    backends (posix/process.c, win32/process.c).
*/

#include "internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/process.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#else
    #include <unistd.h>
    #if PF_OS_APPLE
        #include <mach-o/dyld.h>
    #endif
    #if PF_OS_FREEBSD || PF_OS_DRAGONFLY || PF_OS_NETBSD
        #include <sys/sysctl.h>
        #include <sys/types.h>
    #endif
#endif

PF_API int pf_process_run(
    const char *const argv[],
    const pf_spawn_opts_t *opts,
    pf_stream_t *in,
    pf_stream_t *out,
    pf_stream_t *err,
    pf_exit_status_t *status
) {
    pf_spawn_opts_t o;
    pf_process_t p;
    pf_stream_t *streams[3];
    int rc, k;

    streams[0] = in;
    streams[1] = out;
    streams[2] = err;
    if (opts)
        o = *opts;
    else
        memset(&o, 0, sizeof(o));
    for (k = 0; k < 3; k++)
        if (streams[k])
            o.stdio[k].mode = PF_STDIO_PIPE;

    rc = pf_spawn(&p, argv, &o);
    if (rc)
        return rc;
    rc = pf_process_communicate(&p, in, out, err);
    if (rc) {
        /* Do not leave it running unattended. */
        pf_process_kill(&p, true);
        pf_process_wait(&p, NULL, NULL);
    } else {
        rc = pf_process_wait(&p, NULL, status);
    }
    pf_process_free(&p);
    return rc;
}

PF_API ptrdiff_t pf_exe_path(char *buf, size_t size) {
#if PF_OS_WINDOWS
    DWORD cap = 260, n;
    wchar_t *w = NULL;
    ptrdiff_t rc;
    for (;;) {
        wchar_t *grown = (wchar_t *)realloc(w, cap * sizeof(wchar_t));
        if (!grown) {
            free(w);
            return PF_ENOMEM;
        }
        w = grown;
        n = GetModuleFileNameW(NULL, w, cap);
        if (n == 0) {
            free(w);
            return pf_error_last_win32();
        }
        if (n < cap)
            break;
        if (cap >= 32768 * 2) {
            free(w);
            return PF_ENAMETOOLONG;
        }
        cap *= 2;
    }
    rc = pf__path_from_wide((const uint16_t *)w, n, buf, size);
    free(w);
    return rc;
#elif PF_OS_LINUX || PF_OS_ANDROID
    char path[PF_PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path));
    if (n < 0)
        return pf_error_from_errno(errno);
    if ((size_t)n >= sizeof(path))
        return PF_ENAMETOOLONG;
    return pf__copy_out(buf, size, path, (size_t)n);
#elif PF_OS_APPLE
    char raw[PF_PATH_MAX], real[PF_PATH_MAX];
    uint32_t len = sizeof(raw);
    if (_NSGetExecutablePath(raw, &len) != 0)
        return PF_ENAMETOOLONG;
    if (!realpath(raw, real))
        return pf_error_from_errno(errno);
    return pf__copy_out(buf, size, real, strlen(real));
#elif PF_OS_FREEBSD || PF_OS_DRAGONFLY || PF_OS_NETBSD
    char path[PF_PATH_MAX];
    size_t len = sizeof(path);
    #if PF_OS_NETBSD
    int mib[4] = { CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME };
    #else
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    #endif
    if (sysctl(mib, 4, path, &len, NULL, 0) != 0)
        return pf_error_from_errno(errno);
    return pf__copy_out(buf, size, path, strlen(path));
#else
    (void)buf;
    (void)size;
    return PF_ENOSYS;
#endif
}
