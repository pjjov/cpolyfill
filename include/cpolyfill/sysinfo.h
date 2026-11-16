/** cpolyfill - Filling the gaps between C standards and compilers

    The machine and the operating system: processors, memory, the system's
    name and version, the host name.

    Processors:

        int pf_cpu_count(void)
                Logical processors online in the system (at least 1).
        int pf_cpu_count_physical(void)
                Processor cores, each counted once however many hardware
                threads it runs; pf_cpu_count where the system does not
                tell.
        int pf_cpu_count_available(void)
                Logical processors this process may run on: its affinity
                mask, and on Linux the cgroup (v2) CPU quota, rounded up
                (at least 1). The number of threads worth starting.
        ptrdiff_t pf_cpu_name(char *buf, size_t size)
                The processor's model name ("AMD Ryzen 7 7840U ..."), from
                CPUID on x86, else from the system: its length, PF_ENOENT
                if unknown, or PF_ERANGE.
        size_t pf_cache_line_size(void)
                The size of a level 1 data cache line, PF_CACHE_LINE_SIZE
                if unknown.
        PF_CACHE_LINE_SIZE
                A constant for aligning data (to avoid false sharing): 128
                on Apple's arm64 and on 64-bit POWER, 256 on s390x, else
                64.

    Memory:

        uint64_t pf_memory_total(void)
                Physical memory in bytes, 0 if unknown.
        uint64_t pf_memory_available(void)
                Memory that can be given to programs without swapping
                (free plus reclaimable caches), 0 if unknown.

    The system:

        pf_os_info_t            name      "Linux", "Android", "macOS",
                                          "iOS", "Windows", "FreeBSD", ...
                                version   of the kernel (Linux, BSDs) or
                                          the system ("10.0.26100" on
                                          Windows, "15.1" on macOS)
                                pretty    for people: the distribution's
                                          PRETTY_NAME on Linux ("Arch
                                          Linux"), the product name on
                                          Windows ("Windows 11 Pro")
                                machine   the system's architecture:
                                          "x86_64", "x86", "arm64", "arm",
                                          or as the system names it
                                All NUL-terminated (truncated if long).
        int pf_os_info(pf_os_info_t *info)
                0 or a PF_E* code.
        ptrdiff_t pf_gethostname(char *buf, size_t size)
                The computer's host name: its length or PF_ERANGE (with
                `buf == NULL`, the length only).

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_SYSINFO_H
#define CPOLYFILL_SYSINFO_H

#include <cpolyfill/platform.h>

#include <stddef.h>
#include <stdint.h>

#if (PF_OS_APPLE && PF_ARCH_ARM64) || PF_ARCH_PPC64
    #define PF_CACHE_LINE_SIZE 128
#elif PF_ARCH_S390X
    #define PF_CACHE_LINE_SIZE 256
#else
    #define PF_CACHE_LINE_SIZE 64
#endif

typedef struct pf_os_info_t {
    char name[32];
    char version[64];
    char pretty[128];
    char machine[32];
} pf_os_info_t;

PF_BEGIN_DECLS

PF_API int pf_cpu_count(void);
PF_API int pf_cpu_count_physical(void);
PF_API int pf_cpu_count_available(void);
PF_API ptrdiff_t pf_cpu_name(char *buf, size_t size);
PF_API size_t pf_cache_line_size(void);
PF_API uint64_t pf_memory_total(void);
PF_API uint64_t pf_memory_available(void);
PF_API int pf_os_info(pf_os_info_t *info);
PF_API ptrdiff_t pf_gethostname(char *buf, size_t size);

PF_END_DECLS

#endif /* CPOLYFILL_SYSINFO_H */
