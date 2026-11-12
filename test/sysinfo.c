/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/sysinfo.h> */

#include "../src/internal.h"

#include <cpolyfill/sysinfo.h>
#include <cpolyfill/threads.h>

#include <string.h>
#if PF_OS_POSIX
    #include <sys/utsname.h>
    #include <unistd.h>
#endif
#if PF_OS_LINUX
    #include <sched.h>
#endif

#include "check.h"

static int test_cpus(int seed, int i) {
    int logical = pf_cpu_count(), physical = pf_cpu_count_physical();
    int available = pf_cpu_count_available();
    char name[256];
    ptrdiff_t len;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_GE(logical, 1);
    PF_ASSERT_ALWAYS_GE(physical, 1);
    PF_ASSERT_ALWAYS_LE(physical, logical);
    PF_ASSERT_ALWAYS_GE(available, 1);
    PF_ASSERT_ALWAYS_LE(available, logical);
#if PF_OS_LINUX
    {
        cpu_set_t set;
        if (sched_getaffinity(0, sizeof(set), &set) == 0)
            PF_ASSERT_ALWAYS_LE(available, CPU_COUNT(&set));
    }
#endif
    {
        /* the processors we may run on: the affinity agrees */
        uint64_t mask[16] = { 0 };
        if (pf_thrd_get_affinity(mask, 1024) == 0) {
            int n = 0, k;
            for (k = 0; k < 16; k++) {
                uint64_t m = mask[k];
                for (; m; m &= m - 1)
                    n++;
            }
            PF_ASSERT_ALWAYS_LE(available, n);
        }
    }

    len = pf_cpu_name(name, sizeof(name));
#if PF_ARCH_X86 || PF_ARCH_X86_64
    PF_ASSERT_ALWAYS_GT(len, 0);
#endif
    if (len > 0) {
        PF_ASSERT_ALWAYS_EQ((size_t)len, strlen(name));
        PF_ASSERT_ALWAYS(name[0] != ' ' && name[len - 1] != ' ');
        PF_ASSERT_ALWAYS_EQ(pf_cpu_name(NULL, 0), len);
        PF_ASSERT_ALWAYS_EQ(pf_cpu_name(name, (size_t)len), PF_ERANGE);
    } else {
        PF_ASSERT_ALWAYS_EQ(len, PF_ENOENT);
    }

    {
        size_t line = pf_cache_line_size();
        PF_ASSERT_ALWAYS(line >= 16 && line <= 512);
        PF_ASSERT_ALWAYS_EQ(line & (line - 1), 0);
        PF_ASSERT_ALWAYS_EQ(PF_CACHE_LINE_SIZE & (PF_CACHE_LINE_SIZE - 1), 0);
    }
    return 0;
}

static int test_memory(int seed, int i) {
    uint64_t total = pf_memory_total(), avail = pf_memory_available();
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_GT(total, (uint64_t)16 << 20);
    PF_ASSERT_ALWAYS_GT(avail, 0);
    PF_ASSERT_ALWAYS_LE(avail, total);
    return 0;
}

static int test_os(int seed, int i) {
    pf_os_info_t os;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_os_info(NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_os_info(&os), PF_OK);
    PF_ASSERT_ALWAYS(os.name[0] && os.version[0] && os.pretty[0]);
    PF_ASSERT_ALWAYS(os.machine[0]);
#if PF_OS_LINUX && !PF_OS_ANDROID
    PF_ASSERT_ALWAYS_STR_EQ(os.name, "Linux");
#elif PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_STR_EQ(os.name, "Windows");
#elif PF_OS_MACOS
    PF_ASSERT_ALWAYS_STR_EQ(os.name, "macOS");
#endif
#if PF_ARCH_X86_64 && !PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_STR_EQ(os.machine, "x86_64");
#elif PF_ARCH_ARM64
    PF_ASSERT_ALWAYS_STR_EQ(os.machine, "arm64");
#endif
#if PF_OS_POSIX && !PF_OS_APPLE
    {
        struct utsname u;
        PF_ASSERT_ALWAYS_EQ(uname(&u), 0);
        PF_ASSERT_ALWAYS_STR_EQ(os.version, u.release);
    }
#endif
    PF_ASSERT_ALWAYS(memchr(os.pretty, '"', sizeof(os.pretty)) == NULL);
    return 0;
}

static int test_hostname(int seed, int i) {
    char name[1024], small[2];
    ptrdiff_t len;
    (void)seed, (void)i;

    len = pf_gethostname(name, sizeof(name));
    PF_ASSERT_ALWAYS_GT(len, 0);
    PF_ASSERT_ALWAYS_EQ((size_t)len, strlen(name));
    PF_ASSERT_ALWAYS_EQ(pf_gethostname(NULL, 0), len);
    if (len >= 2)
        PF_ASSERT_ALWAYS_EQ(pf_gethostname(small, sizeof(small)), PF_ERANGE);
#if PF_OS_POSIX
    {
        char sys[1024];
        PF_ASSERT_ALWAYS_EQ(gethostname(sys, sizeof(sys)), 0);
        PF_ASSERT_ALWAYS_STR_EQ(name, sys);
    }
#endif
    return 0;
}

pf_test_t suite_sysinfo[] = {
    { test_cpus, "cpus" },
    { test_memory, "memory" },
    { test_os, "os" },
    { test_hostname, "hostname" },
    { 0 },
};
