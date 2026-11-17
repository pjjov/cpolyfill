/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/sysinfo.h>: sysconf, /proc and /sys on Linux, sysctl on
    macOS and the BSDs, the Win32 API on Windows.
*/

#include "internal.h"

#include <cpolyfill/attribute.h>
#include <cpolyfill/cpuid.h>
#include <cpolyfill/sysinfo.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/utsname.h>
    #include <unistd.h>
    #if PF_OS_LINUX || PF_OS_ANDROID
        #include <sched.h>
    #endif
    #if PF_OS_APPLE || PF_OS_BSD
        #include <sys/sysctl.h>
        #include <sys/types.h>
    #endif
    #if PF_OS_APPLE
        #include <mach/mach.h>
    #endif
    #if PF_OS_FREEBSD
        #include <sys/cpuset.h>
        #include <sys/param.h>
    #endif
#endif

/* Copies `len` bytes of `s` into the fixed field `dst`, truncated at a
   character boundary. */
static void pf__field(char *dst, size_t cap, const char *s, size_t len) {
    if (len >= cap)
        len = pf__utf8_truncate(s, cap - 1);
    memcpy(dst, s, len);
    dst[len] = '\0';
}

/* Removes spaces at both ends, in place; returns `s`. */
static char *pf__trim(char *s) {
    size_t n;
    while (*s == ' ' || *s == '\t')
        s++;
    n = strlen(s);
    while (n > 0
           && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
               || s[n - 1] == '\r'))
        s[--n] = '\0';
    return s;
}

#if !PF_OS_WINDOWS

/* Reads a small file (of /proc or /sys) into `buf`, NUL-terminated: the
   length, or -1. */
PF_UNUSED static ptrdiff_t pf__read_small(
    const char *path, char *buf, size_t size
) {
    size_t len = 0;
    int fd;

    #ifdef O_CLOEXEC
    fd = open(path, O_RDONLY | O_CLOEXEC);
    #else
    fd = open(path, O_RDONLY);
    #endif
    if (fd < 0)
        return -1;
    while (len + 1 < size) {
        ssize_t got = read(fd, buf + len, size - 1 - len);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;
        len += (size_t)got;
    }
    close(fd);
    buf[len] = '\0';
    return (ptrdiff_t)len;
}

#endif

#if PF_OS_APPLE || PF_OS_BSD

static bool pf__sysctl_u64(const char *name, uint64_t *out) {
    union {
        uint32_t u32;
        uint64_t u64;
    } v;
    size_t len = sizeof(v);
    memset(&v, 0, sizeof(v));
    if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
        return false;
    if (len == sizeof(uint32_t))
        *out = v.u32;
    else if (len == sizeof(uint64_t))
        *out = v.u64;
    else
        return false;
    return true;
}

PF_UNUSED static bool pf__sysctl_str(const char *name, char *buf, size_t size) {
    size_t len = size;
    if (sysctlbyname(name, buf, &len, NULL, 0) != 0 || len == 0)
        return false;
    buf[len < size ? len : size - 1] = '\0';
    return true;
}

#endif

/* ---- Processors -------------------------------------------------------- */

PF_API int pf_cpu_count(void) {
    long n = 0;
#if PF_OS_WINDOWS
    n = (long)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#elif PF_OS_APPLE
    uint64_t v;
    if (pf__sysctl_u64("hw.logicalcpu", &v))
        n = (long)v;
#elif defined(_SC_NPROCESSORS_ONLN)
    n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    return n > 0 ? (n > INT_MAX ? INT_MAX : (int)n) : 1;
}

#if PF_OS_LINUX || PF_OS_ANDROID

/* Calls `fn` for every CPU of a kernel CPU list ("0-3,8,10-11"). */
static void pf__cpulist_each(
    const char *s, void (*fn)(long cpu, void *user), void *user
) {
    while (*s) {
        char *end;
        long lo = strtol(s, &end, 10), hi = lo;
        if (end == s)
            break;
        s = end;
        if (*s == '-') {
            hi = strtol(s + 1, &end, 10);
            s = end;
        }
        for (; lo <= hi; lo++)
            fn(lo, user);
        if (*s != ',')
            break;
        s++;
    }
}

/* Counts a CPU if it is the first of its core's hardware threads. */
static void pf__count_core(long cpu, void *user) {
    char path[96], buf[256];
    snprintf(
        path,
        sizeof(path),
        "/sys/devices/system/cpu/cpu%ld/topology/thread_siblings_list",
        cpu
    );
    if (pf__read_small(path, buf, sizeof(buf)) > 0) {
        if (strtol(buf, NULL, 10) == cpu)
            ++*(int *)user;
    }
}

#endif

PF_API int pf_cpu_count_physical(void) {
    int n = 0;
#if PF_OS_WINDOWS
    DWORD len = 0;
    char *buf, *p;
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0
        && (buf = (char *)malloc(len)) != NULL) {
        if (GetLogicalProcessorInformationEx(
                RelationProcessorCore,
                (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf,
                &len
            ))
            for (p = buf; p < buf + len;) {
                PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
                    e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)p;
                if (e->Relationship == RelationProcessorCore)
                    n++;
                p += e->Size;
            }
        free(buf);
    }
#elif PF_OS_LINUX || PF_OS_ANDROID
    char online[1024];
    if (pf__read_small("/sys/devices/system/cpu/online", online, sizeof(online))
        > 0)
        pf__cpulist_each(online, pf__count_core, &n);
#elif PF_OS_APPLE
    uint64_t v;
    if (pf__sysctl_u64("hw.physicalcpu", &v))
        n = (int)v;
#elif PF_OS_FREEBSD
    uint64_t v;
    if (pf__sysctl_u64("kern.smp.cores", &v))
        n = (int)v;
#endif
    return n > 0 ? n : pf_cpu_count();
}

#if PF_OS_LINUX || PF_OS_ANDROID

/* The CPU quota of the cgroup (v2) the process is in, or of its parents
   (the lowest), as a number of CPUs rounded up; 0 if there is none. */
static int pf__cgroup_cpus(void) {
    char buf[4096], path[4200], *rel, *nl;
    int best = 0;

    if (pf__read_small("/proc/self/cgroup", buf, sizeof(buf)) <= 0)
        return 0;
    /* The v2 line is "0::/path". */
    rel = strstr(buf, "0::/");
    if (!rel || (rel != buf && rel[-1] != '\n'))
        return 0;
    rel += 3;
    nl = strchr(rel, '\n');
    if (nl)
        *nl = '\0';

    for (;;) {
        char max[128];
        char *slash;
        snprintf(
            path,
            sizeof(path),
            "/sys/fs/cgroup%s/cpu.max",
            strcmp(rel, "/") == 0 ? "" : rel
        );
        if (pf__read_small(path, max, sizeof(max)) > 0
            && strncmp(max, "max", 3) != 0) {
            char *end;
            long long quota = strtoll(max, &end, 10), period;
            period = strtoll(end, NULL, 10);
            if (quota > 0 && period > 0) {
                long long cpus = (quota + period - 1) / period;
                if (cpus > INT_MAX)
                    cpus = INT_MAX;
                if (best == 0 || cpus < best)
                    best = (int)cpus;
            }
        }
        if (strcmp(rel, "/") == 0)
            break;
        slash = strrchr(rel, '/');
        if (!slash)
            break;
        if (slash == rel)
            rel[1] = '\0';
        else
            *slash = '\0';
    }
    return best;
}

#endif

PF_API int pf_cpu_count_available(void) {
    int n = 0;
#if PF_OS_WINDOWS
    DWORD_PTR proc, sys;
    if (GetActiveProcessorGroupCount() == 1
        && GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys)) {
        for (; proc; proc &= proc - 1)
            n++;
    }
#elif PF_OS_LINUX || PF_OS_ANDROID
    {
        int cpus = 1024, quota;
        for (;;) {
            cpu_set_t *set = CPU_ALLOC((size_t)cpus);
            size_t size = CPU_ALLOC_SIZE((size_t)cpus);
            int rc;
            if (!set)
                break;
            rc = sched_getaffinity(0, size, set);
            if (rc == 0)
                n = CPU_COUNT_S(size, set);
            CPU_FREE(set);
            if (rc == 0 || errno != EINVAL || cpus >= (1 << 20))
                break;
            cpus *= 2;
        }
        quota = pf__cgroup_cpus();
        if (quota > 0 && (n == 0 || quota < n))
            n = quota;
    }
#elif PF_OS_FREEBSD
    cpuset_t set;
    CPU_ZERO(&set);
    if (cpuset_getaffinity(
            CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(set), &set
        )
        == 0)
        n = CPU_COUNT(&set);
#endif
    return n > 0 ? n : pf_cpu_count();
}

PF_API ptrdiff_t pf_cpu_name(char *buf, size_t size) {
    char name[256];
    char *s;

    name[0] = '\0';
#if PF__CPU_IS_X86
    if (pf_cpuid_max_leaf(0x80000000u) >= 0x80000004u) {
        uint32_t regs[4];
        uint32_t i;
        for (i = 0; i < 3; i++) {
            pf_cpuid(0x80000002u + i, regs);
            memcpy(name + 16 * i, regs, sizeof(regs));
        }
        name[48] = '\0';
    }
#endif
    if (!*pf__trim(name)) {
#if PF_OS_WINDOWS
        wchar_t w[128];
        DWORD wsize = sizeof(w);
        if (RegGetValueW(
                HKEY_LOCAL_MACHINE,
                L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                L"ProcessorNameString",
                RRF_RT_REG_SZ,
                NULL,
                w,
                &wsize
            )
            == ERROR_SUCCESS) {
            ptrdiff_t n = pf_utf16_to_utf8(
                (const uint16_t *)w,
                PF_NTS,
                name,
                sizeof(name),
                PF_UNICODE_REPLACE
            );
            if (n < 0)
                name[0] = '\0';
        }
#elif PF_OS_APPLE
        if (!pf__sysctl_str("machdep.cpu.brand_string", name, sizeof(name)))
            name[0] = '\0';
#elif PF_OS_BSD
        if (!pf__sysctl_str("hw.model", name, sizeof(name)))
            name[0] = '\0';
#elif PF_OS_LINUX || PF_OS_ANDROID
        static const char *const keys[] = {
            "model name", "Model", "cpu model", "Processor", "cpu"
        };
        FILE *f = fopen("/proc/cpuinfo", "r");
        char line[512];
        size_t k, best = sizeof(keys) / sizeof(keys[0]);
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                char *colon = strchr(line, ':');
                if (!colon)
                    continue;
                *colon = '\0';
                for (k = 0; k < best; k++)
                    if (strcmp(pf__trim(line), keys[k]) == 0) {
                        char *v = pf__trim(colon + 1);
                        if (*v) {
                            pf__field(name, sizeof(name), v, strlen(v));
                            best = k;
                        }
                        break;
                    }
            }
            fclose(f);
        }
#endif
    }
    s = pf__trim(name);
    if (!*s)
        return PF_ENOENT;
    return pf__copy_out(buf, size, s, strlen(s));
}

PF_API size_t pf_cache_line_size(void) {
    long n = 0;
#if PF_OS_WINDOWS
    DWORD len = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *info;
    GetLogicalProcessorInformation(NULL, &len);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0
        && (info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(len))
            != NULL) {
        if (GetLogicalProcessorInformation(info, &len)) {
            DWORD k;
            for (k = 0; k < len / sizeof(*info); k++)
                if (info[k].Relationship == RelationCache
                    && info[k].Cache.Level == 1
                    && (info[k].Cache.Type == CacheData
                        || info[k].Cache.Type == CacheUnified)) {
                    n = info[k].Cache.LineSize;
                    break;
                }
        }
        free(info);
    }
#elif PF_OS_APPLE
    uint64_t v;
    if (pf__sysctl_u64("hw.cachelinesize", &v))
        n = (long)v;
#else
    #if defined(_SC_LEVEL1_DCACHE_LINESIZE)
    n = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    #endif
    #if PF_OS_LINUX || PF_OS_ANDROID
    if (n <= 0) {
        char buf[32];
        if (pf__read_small(
                "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size",
                buf,
                sizeof(buf)
            )
            > 0)
            n = strtol(buf, NULL, 10);
    }
    #endif
#endif
#if PF__CPU_IS_X86
    /* The CLFLUSH line size, in units of 8 bytes. */
    if (n <= 0 && pf_cpu_has(PF_CPU_CLFSH))
        n = (long)((pf_cpuid_reg(1, 0, 1) >> 8 & 0xFFu) * 8);
#endif
    return n > 0 ? (size_t)n : PF_CACHE_LINE_SIZE;
}

/* ---- Memory ------------------------------------------------------------ */

#if PF_OS_LINUX || PF_OS_ANDROID
/* A "Key:   1234 kB" line of /proc/meminfo, in bytes; 0 if missing. */
static uint64_t pf__meminfo(const char *buf, const char *key) {
    size_t klen = strlen(key);
    const char *p = buf;
    while ((p = strstr(p, key)) != NULL) {
        if ((p == buf || p[-1] == '\n') && p[klen] == ':')
            return (uint64_t)strtoull(p + klen + 1, NULL, 10) * 1024u;
        p += klen;
    }
    return 0;
}
#endif

#if !PF_OS_WINDOWS && !PF_OS_APPLE
/* sysconf pages times the page size, 0 if unknown. */
PF_UNUSED static uint64_t pf__sysconf_pages(int name) {
    long pages = sysconf(name), page = sysconf(_SC_PAGESIZE);
    return pages > 0 && page > 0 ? (uint64_t)pages * (uint64_t)page : 0;
}
#endif

PF_API uint64_t pf_memory_total(void) {
#if PF_OS_WINDOWS
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    return GlobalMemoryStatusEx(&ms) ? ms.ullTotalPhys : 0;
#elif PF_OS_APPLE
    uint64_t v;
    return pf__sysctl_u64("hw.memsize", &v) ? v : 0;
#else
    uint64_t v = 0;
    #if PF_OS_LINUX || PF_OS_ANDROID
    char buf[4096];
    if (pf__read_small("/proc/meminfo", buf, sizeof(buf)) > 0)
        v = pf__meminfo(buf, "MemTotal");
    #elif PF_OS_FREEBSD || PF_OS_DRAGONFLY
    if (!pf__sysctl_u64("hw.physmem", &v))
        v = 0;
    #endif
    #if defined(_SC_PHYS_PAGES)
    if (v == 0)
        v = pf__sysconf_pages(_SC_PHYS_PAGES);
    #endif
    return v;
#endif
}

PF_API uint64_t pf_memory_available(void) {
#if PF_OS_WINDOWS
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    return GlobalMemoryStatusEx(&ms) ? ms.ullAvailPhys : 0;
#elif PF_OS_APPLE
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t page = 0;
    if (host_page_size(mach_host_self(), &page) != KERN_SUCCESS
        || host_statistics64(
               mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count
           ) != KERN_SUCCESS)
        return 0;
    return ((uint64_t)vm.free_count + vm.inactive_count + vm.purgeable_count)
        * page;
#else
    uint64_t v = 0;
    #if PF_OS_LINUX || PF_OS_ANDROID
    char buf[4096];
    if (pf__read_small("/proc/meminfo", buf, sizeof(buf)) > 0) {
        v = pf__meminfo(buf, "MemAvailable");
        if (v == 0) /* before Linux 3.14 */
            v = pf__meminfo(buf, "MemFree") + pf__meminfo(buf, "Cached");
    }
    #elif PF_OS_FREEBSD || PF_OS_DRAGONFLY
    uint64_t free_pages, inactive, page;
    if (pf__sysctl_u64("vm.stats.vm.v_free_count", &free_pages)
        && pf__sysctl_u64("vm.stats.vm.v_inactive_count", &inactive)
        && pf__sysctl_u64("hw.pagesize", &page))
        v = (free_pages + inactive) * page;
    #endif
    #if defined(_SC_AVPHYS_PAGES)
    if (v == 0)
        v = pf__sysconf_pages(_SC_AVPHYS_PAGES);
    #endif
    return v;
#endif
}

/* ---- The system -------------------------------------------------------- */

/* The library's name for an architecture. */
PF_UNUSED static const char *pf__machine_name(const char *m) {
    static const char *const map[][2] = {
        { "x86_64", "x86_64" }, { "amd64", "x86_64" }, { "x64", "x86_64" },
        { "i386", "x86" },      { "i486", "x86" },     { "i586", "x86" },
        { "i686", "x86" },      { "i86pc", "x86" },    { "x86", "x86" },
        { "aarch64", "arm64" }, { "arm64", "arm64" },
    };
    size_t k;
    for (k = 0; k < sizeof(map) / sizeof(map[0]); k++)
        if (strcmp(m, map[k][0]) == 0)
            return map[k][1];
    if (strncmp(m, "armv", 4) == 0 || strcmp(m, "arm") == 0)
        return "arm";
    return m;
}

#define PF__SET(field, s)                                       \
    pf__field(info->field, sizeof(info->field), (s), strlen(s))

#if PF_OS_LINUX
/* PRETTY_NAME of os-release, unquoted, into `out`; false if missing. */
static bool pf__os_release(char *out, size_t size) {
    static const char *const files[] = { "/etc/os-release",
                                         "/usr/lib/os-release" };
    char buf[4096];
    size_t k;

    for (k = 0; k < 2; k++) {
        char *p, *end, *o, *start;
        if (pf__read_small(files[k], buf, sizeof(buf)) <= 0)
            continue;
        for (p = buf; p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL)
            if (strncmp(p, "PRETTY_NAME=", 12) == 0)
                break;
        if (!p)
            continue;
        p += 12;
        start = p;
        end = strchr(p, '\n');
        if (end)
            *end = '\0';
        /* Shell-style: quotes removed, backslash escapes taken literally. */
        for (o = start; *p; p++) {
            if (*p == '"' || *p == '\'')
                continue;
            if (*p == '\\' && p[1])
                p++;
            *o++ = *p;
        }
        *o = '\0';
        if (*start) {
            pf__field(out, size, start, strlen(start));
            return true;
        }
    }
    return false;
}
#endif

PF_API int pf_os_info(pf_os_info_t *info) {
    if (!info)
        return PF_EINVAL;
    memset(info, 0, sizeof(*info));

#if PF_OS_WINDOWS
    {
        typedef LONG(WINAPI * rtl_get_version_fn)(OSVERSIONINFOW *);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        rtl_get_version_fn get = NULL;
        OSVERSIONINFOW v;
        wchar_t w[128];
        DWORD wsize = sizeof(w);
        char product[128] = "Windows";
        USHORT proc = 0, native = 0;
        typedef BOOL(WINAPI * wow64_fn)(HANDLE, USHORT *, USHORT *);
        wow64_fn wow64 = NULL;
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        FARPROC fp;

        PF__SET(name, "Windows");
        memset(&v, 0, sizeof(v));
        v.dwOSVersionInfoSize = sizeof(v);
        if (ntdll && (fp = GetProcAddress(ntdll, "RtlGetVersion")) != NULL) {
            memcpy(&get, &fp, sizeof(get));
            if (get(&v) == 0)
                snprintf(
                    info->version,
                    sizeof(info->version),
                    "%lu.%lu.%lu",
                    (unsigned long)v.dwMajorVersion,
                    (unsigned long)v.dwMinorVersion,
                    (unsigned long)v.dwBuildNumber
                );
        }
        if (RegGetValueW(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                L"ProductName",
                RRF_RT_REG_SZ,
                NULL,
                w,
                &wsize
            )
            == ERROR_SUCCESS) {
            if (pf_utf16_to_utf8(
                    (const uint16_t *)w,
                    PF_NTS,
                    product,
                    sizeof(product),
                    PF_UNICODE_REPLACE
                )
                < 0)
                strcpy(product, "Windows");
        }
        /* Windows 11 still calls itself "Windows 10" there. */
        if (v.dwBuildNumber >= 22000 && strncmp(product, "Windows 10", 10) == 0)
            product[9] = '1';
        PF__SET(pretty, product);

        if (k32 && (fp = GetProcAddress(k32, "IsWow64Process2")) != NULL) {
            memcpy(&wow64, &fp, sizeof(wow64));
            if (!wow64(GetCurrentProcess(), &proc, &native))
                native = 0;
        }
        if (native == 0) {
            SYSTEM_INFO si;
            GetNativeSystemInfo(&si);
            switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64:
                native = 0x8664;
                break;
            case PROCESSOR_ARCHITECTURE_INTEL:
                native = 0x14c;
                break;
            case 12 /* PROCESSOR_ARCHITECTURE_ARM64 */:
                native = 0xAA64;
                break;
            case PROCESSOR_ARCHITECTURE_ARM:
                native = 0x1c4;
                break;
            default:
                break;
            }
        }
        PF__SET(
            machine,
            native == 0x8664       ? "x86_64"
                : native == 0x14c  ? "x86"
                : native == 0xAA64 ? "arm64"
                : native == 0x1c4  ? "arm"
                                   : "unknown"
        );
    }
    return PF_OK;
#else
    {
        struct utsname u;
        if (uname(&u) != 0)
            return pf_error_from_errno(errno);
        PF__SET(machine, pf__machine_name(u.machine));
    #if PF_OS_MACOS || PF_OS_IOS
        PF__SET(name, PF_OS_MACOS ? "macOS" : "iOS");
        if (!pf__sysctl_str(
                "kern.osproductversion", info->version, sizeof(info->version)
            ))
            PF__SET(version, u.release);
        snprintf(
            info->pretty,
            sizeof(info->pretty),
            "%s %s",
            info->name,
            info->version
        );
    #else
        PF__SET(name, PF_OS_ANDROID ? "Android" : u.sysname);
        PF__SET(version, u.release);
        #if PF_OS_LINUX
        if (!pf__os_release(info->pretty, sizeof(info->pretty)))
        #endif
            snprintf(
                info->pretty,
                sizeof(info->pretty),
                "%s %s",
                info->name,
                info->version
            );
    #endif
    }
    return PF_OK;
#endif
}

PF_API ptrdiff_t pf_gethostname(char *buf, size_t size) {
#if PF_OS_WINDOWS
    wchar_t w[256];
    DWORD n = sizeof(w) / sizeof(w[0]);
    char name[1024];
    ptrdiff_t len;
    if (!GetComputerNameExW(ComputerNameDnsHostname, w, &n))
        return pf_error_last_win32();
    len = pf_utf16_to_utf8(
        (const uint16_t *)w, n, name, sizeof(name), PF_UNICODE_REPLACE
    );
    if (len < 0)
        return len;
    return pf__copy_out(buf, size, name, (size_t)len);
#else
    char name[1025];
    if (gethostname(name, sizeof(name) - 1) != 0)
        return pf_error_from_errno(errno);
    name[sizeof(name) - 1] = '\0';
    return pf__copy_out(buf, size, name, strlen(name));
#endif
}
