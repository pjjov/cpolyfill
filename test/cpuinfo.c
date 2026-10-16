/*
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
*/

#include "check.h"
#include "../include/pf_cpuinfo.h"
#include <ctype.h>
#include <stdint.h>

#ifdef PF_CPU_X86
    #define ON_X86 1
#else
    #define ON_X86 0
#endif

static int printable(const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (!isprint((unsigned char)s[i]))
            return 0;
    return 1;
}

static int test_cpuinfo_vendor(int seed, int rep) {
    char v[13];

    memset(v, 'x', sizeof v);
    pf_cpu_vendor(v);
    CHECK(v[12] == '\0');
    pf_cpu_vendor(NULL); /* must not crash */

#ifdef PF_CPU_X86
    CHECK(strlen(v) == 12);
    CHECK(printable(v, 12));
    CHECK(pf_cpu_vendor_is_(v));
    CHECK(!pf_cpu_vendor_is_("NotAVendor!!"));
    CHECK(
        pf_cpu_vendor_is_("GenuineIntel") + pf_cpu_vendor_is_("AuthenticAMD")
            + pf_cpu_vendor_is_("HygonGenuine")
            + (pf_cpu_vendor_is_("GenuineIntel") == 0)
        >= 1
    );
#else
    CHECK(v[0] == '\0');
    CHECK(!pf_cpu_vendor_is_("GenuineIntel"));
#endif
    return 0;
}

static int test_cpuinfo_brand(int seed, int rep) {
    char b[48];

    memset(b, 'x', sizeof b);
    pf_cpu_brand(b);
    CHECK(memchr(b, '\0', sizeof b) != NULL);
    pf_cpu_brand(NULL);

#ifdef PF_CPU_X86
    if (pf_cpuid_max_leaf(0x80000000u) >= 0x80000004u) {
        CHECK(strlen(b) > 3);
        CHECK(printable(b, strlen(b)));
    }
#else
    CHECK(b[0] == '\0');
#endif
    return 0;
}

static int test_cpuinfo_cpuid(int seed, int rep) {
    uint32_t a[4] = { 1, 2, 3, 4 }, b[4], c[4];

    pf_cpuid(0, a);
    pf_cpuidex(0, 0, b);
    CHECK(memcmp(a, b, sizeof a) == 0);

#ifdef PF_CPU_X86
    {
        uint32_t max = pf_cpuid_max_leaf(0);
        CHECK(max >= 1);
        CHECK(a[0] == max);

        /* The vendor id sits in EBX, EDX, ECX. */
        {
            char v[13];
            pf_cpu_vendor(v);
            CHECK(memcmp(v, &a[1], 4) == 0);
            CHECK(memcmp(v + 4, &a[3], 4) == 0);
            CHECK(memcmp(v + 8, &a[2], 4) == 0);
        }
    }

    /* Repeatable. */
    pf_cpuid(1, a);
    pf_cpuid(1, b);
    CHECK(a[0] == b[0] && a[2] == b[2] && a[3] == b[3]);

    /* Sub-leaves of leaf 7: the maximum sub-leaf in EAX. */
    if (pf_cpuid_max_leaf(0) >= 7) {
        pf_cpuidex(7, 0, a);
        pf_cpuidex(7, 0, c);
        CHECK(a[1] == c[1] && a[2] == c[2]);
    }
#else
    CHECK(a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0);
    pf_cpuidex(7, 3, c);
    CHECK(c[0] == 0 && c[1] == 0 && c[2] == 0 && c[3] == 0);
    CHECK(pf_cpuid_max_leaf(0) == 0);
    CHECK(pf_cpuid_max_leaf(0x80000000u) == 0);
#endif
    return 0;
}

static int test_cpuinfo_max_leaf(int seed, int rep) {
#ifdef PF_CPU_X86
    uint32_t basic = pf_cpuid_max_leaf(0);
    uint32_t ext = pf_cpuid_max_leaf(0x80000000u);

    CHECK(basic >= 1 && basic < 0x1000);
    /* Extended leaves are absent (0) or at least start at the range. */
    CHECK(ext == 0 || ext >= 0x80000000u);
#else
    CHECK(pf_cpuid_max_leaf(0) == 0);
#endif
    return 0;
}

static int test_cpuinfo_cache(int seed, int rep) {
    const pf_cpu_cache_t *a = pf_cpu_cache(), *b = pf_cpu_cache();

    CHECK(a == b);
    CHECK(a->max_basic == pf_cpuid_max_leaf(0));
    CHECK(a->max_ext == pf_cpuid_max_leaf(0x80000000u));

#ifdef PF_CPU_X86
    {
        uint32_t regs[4];
        pf_cpuidex(1, 0, regs);
        CHECK(memcmp(a->l1, regs, sizeof regs) == 0);
        if (a->max_basic >= 7) {
            pf_cpuidex(7, 0, regs);
            CHECK(memcmp(a->l7, regs, sizeof regs) == 0);
        }
    }
#endif
    return 0;
}

static int test_cpuinfo_reg(int seed, int rep) {
    int reg;

    /* Invalid registers are zero. */
    CHECK(pf_cpuid_reg(1, 0, -1) == 0);
    CHECK(pf_cpuid_reg(1, 0, 4) == 0);

    /* Leaves the CPU does not have read as zero, not as garbage. */
    CHECK(pf_cpuid_reg(0x7FFFFFFFu, 0, 0) == 0);
    CHECK(pf_cpuid_reg(0x7FFFFFFFu, 0, 3) == 0);
    CHECK(pf_cpuid_reg(0xFFFFFFFFu, 0, 0) == 0);
    CHECK(pf_cpuid_flag(0xFFFFFFFFu, 0, 1, 5) == 0);

#ifdef PF_CPU_X86
    for (reg = 0; reg < 4; reg++) {
        uint32_t raw[4];
        pf_cpuidex(1, 0, raw);
        CHECK(pf_cpuid_reg(1, 0, reg) == raw[reg]);
    }

    /* Flags are single bits of the registers. */
    {
        int shift;
        uint32_t edx = pf_cpuid_reg(1, 0, 3);
        for (shift = 0; shift < 32; shift++)
            CHECK(pf_cpuid_flag(1, 0, 3, shift) == (int)((edx >> shift) & 1u));
    }
#else
    for (reg = 0; reg < 4; reg++)
        CHECK(pf_cpuid_reg(1, 0, reg) == 0);
#endif
    return 0;
}

/* Every x86-64 CPU has these. */
static int test_cpuinfo_baseline(int seed, int rep) {
#ifdef PF_CPU_X86_64
    CHECK(PF_HAS_FPU);
    CHECK(PF_HAS_TSC);
    CHECK(PF_HAS_CX8);
    CHECK(PF_HAS_CMOV);
    CHECK(PF_HAS_MMX);
    CHECK(PF_HAS_FXSR);
    CHECK(PF_HAS_SSE);
    CHECK(PF_HAS_SSE2);
    return 0;
#else
    return SKIP("not x86-64");
#endif
}

static int test_cpuinfo_flags_are_boolean(int seed, int rep) {
    CHECK(PF_HAS_SSE2 == 0 || PF_HAS_SSE2 == 1);
    CHECK(PF_HAS_AVX == 0 || PF_HAS_AVX == 1);
    CHECK(PF_HAS_AVX2 == 0 || PF_HAS_AVX2 == 1);
    CHECK(PF_HAS_AVX512F == 0 || PF_HAS_AVX512F == 1);
    CHECK(PF_HAS_POPCNT == 0 || PF_HAS_POPCNT == 1);
    CHECK(PF_CAN_USE_AVX == 0 || PF_CAN_USE_AVX == 1);
    CHECK(PF_CAN_USE_AVX2 == 0 || PF_CAN_USE_AVX2 == 1);
    CHECK(PF_CAN_USE_AVX512F == 0 || PF_CAN_USE_AVX512F == 1);
    return 0;
}

/* Newer instruction sets imply the older ones. */
static int test_cpuinfo_hierarchy(int seed, int rep) {
#ifdef PF_CPU_X86
    if (PF_HAS_AVX2)
        CHECK(PF_HAS_AVX);
    if (PF_HAS_AVX)
        CHECK(PF_HAS_SSE4_2 && PF_HAS_XSAVE);
    if (PF_HAS_SSE4_2)
        CHECK(PF_HAS_SSE4_1);
    if (PF_HAS_SSE4_1)
        CHECK(PF_HAS_SSSE3);
    if (PF_HAS_SSSE3)
        CHECK(PF_HAS_SSE3);
    if (PF_HAS_SSE3)
        CHECK(PF_HAS_SSE2);
    if (PF_HAS_SSE2)
        CHECK(PF_HAS_SSE);
    if (PF_HAS_AVX512F)
        CHECK(PF_HAS_AVX2);
    if (PF_HAS_AVX512VL || PF_HAS_AVX512BW || PF_HAS_AVX512DQ)
        CHECK(PF_HAS_AVX512F);
    if (PF_HAS_FMA)
        CHECK(PF_HAS_AVX);
    if (PF_HAS_F16C)
        CHECK(PF_HAS_AVX);
    return 0;
#else
    return SKIP("not x86");
#endif
}

static int test_cpuinfo_os_support(int seed, int rep) {
#ifdef PF_CPU_X86
    /* AVX needs the OS to save the registers: CPUID alone is not enough. */
    if (PF_CAN_USE_AVX)
        CHECK(PF_HAS_AVX && PF_HAS_OSXSAVE && pf_os_supports_avx());
    if (PF_CAN_USE_AVX2)
        CHECK(PF_HAS_AVX2 && PF_CAN_USE_AVX);
    if (PF_CAN_USE_AVX512F)
        CHECK(PF_HAS_AVX512F && pf_os_supports_avx512() && PF_CAN_USE_AVX2);

    if (PF_HAS_OSXSAVE) {
        uint64_t xcr0 = pf_xgetbv(0);
        CHECK((xcr0 & 1u) == 1u); /* x87 state is always enabled */
        CHECK(
            pf_os_supports_avx() == ((xcr0 & 0x06u) == 0x06u && PF_HAS_OSXSAVE)
        );
        if (pf_os_supports_avx512())
            CHECK(pf_os_supports_avx());
    }
    return 0;
#else
    CHECK(pf_xgetbv(0) == 0);
    CHECK(!pf_os_supports_avx());
    CHECK(!pf_os_supports_avx512());
    return 0;
#endif
}

static int test_cpuinfo_logical_cores(int seed, int rep) {
    int n = pf_cpu_logical_cores();

#ifdef PF_CPU_X86
    CHECK(n >= 1);
    CHECK(n <= 1024);
#else
    CHECK(n == 0);
#endif
    return 0;
}

/* What the compiler was told it may use must really be there. */
static int test_cpuinfo_supports_implies_has(int seed, int rep) {
#ifdef PF_CPU_X86
    #define CHECK_SUPPORTED(feature) CHECK(PF_HAS_##feature)
    #ifdef PF_SUPPORTS_SSE
    CHECK_SUPPORTED(SSE);
    #endif
    #ifdef PF_SUPPORTS_SSE2
    CHECK_SUPPORTED(SSE2);
    #endif
    #ifdef PF_SUPPORTS_SSE3
    CHECK_SUPPORTED(SSE3);
    #endif
    #ifdef PF_SUPPORTS_SSSE3
    CHECK_SUPPORTED(SSSE3);
    #endif
    #ifdef PF_SUPPORTS_SSE4_1
    CHECK_SUPPORTED(SSE4_1);
    #endif
    #ifdef PF_SUPPORTS_SSE4_2
    CHECK_SUPPORTED(SSE4_2);
    #endif
    #ifdef PF_SUPPORTS_AVX
    CHECK_SUPPORTED(AVX);
    #endif
    #ifdef PF_SUPPORTS_AVX2
    CHECK_SUPPORTED(AVX2);
    #endif
    #ifdef PF_SUPPORTS_FMA
    CHECK_SUPPORTED(FMA);
    #endif
    #ifdef PF_SUPPORTS_POPCNT
    CHECK_SUPPORTED(POPCNT);
    #endif
    #ifdef PF_SUPPORTS_BMI1
    CHECK_SUPPORTED(BMI1);
    #endif
    #ifdef PF_SUPPORTS_BMI2
    CHECK_SUPPORTED(BMI2);
    #endif
    #ifdef PF_SUPPORTS_AES
    CHECK_SUPPORTED(AES);
    #endif
    #ifdef PF_SUPPORTS_AVX512F
    CHECK_SUPPORTED(AVX512F);
    #endif
    #undef CHECK_SUPPORTED
    return 0;
#else
    return SKIP("not x86");
#endif
}

#if defined(PF_CPU_X86) && defined(__linux__)

static int cpuinfo_flag(const char *flags, const char *name) {
    size_t n = strlen(name);
    const char *p = flags;

    while ((p = strstr(p, name)) != NULL) {
        int before = p == flags || p[-1] == ' ';
        int after = p[n] == ' ' || p[n] == '\0' || p[n] == '\n';
        if (before && after)
            return 1;
        p += n;
    }
    return 0;
}

/* Cross-checks against what the kernel reports. */
static int test_cpuinfo_proc(int seed, int rep) {
    static char line[16384];
    FILE *f = fopen("/proc/cpuinfo", "r");
    int found = 0;

    if (!f)
        return SKIP("no /proc/cpuinfo");
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "flags", 5) == 0 && strchr(line, ':')) {
            found = 1;
            break;
        }
    }
    fclose(f);
    if (!found)
        return SKIP("no flags in /proc/cpuinfo");

    CHECK(cpuinfo_flag(line, "fpu") == PF_HAS_FPU);
    CHECK(cpuinfo_flag(line, "tsc") == PF_HAS_TSC);
    CHECK(cpuinfo_flag(line, "cx8") == PF_HAS_CX8);
    CHECK(cpuinfo_flag(line, "cmov") == PF_HAS_CMOV);
    CHECK(cpuinfo_flag(line, "mmx") == PF_HAS_MMX);
    CHECK(cpuinfo_flag(line, "sse") == PF_HAS_SSE);
    CHECK(cpuinfo_flag(line, "sse2") == PF_HAS_SSE2);
    CHECK(cpuinfo_flag(line, "pni") == PF_HAS_SSE3);
    CHECK(cpuinfo_flag(line, "ssse3") == PF_HAS_SSSE3);
    CHECK(cpuinfo_flag(line, "sse4_1") == PF_HAS_SSE4_1);
    CHECK(cpuinfo_flag(line, "sse4_2") == PF_HAS_SSE4_2);
    CHECK(cpuinfo_flag(line, "popcnt") == PF_HAS_POPCNT);
    CHECK(cpuinfo_flag(line, "aes") == PF_HAS_AES);
    CHECK(cpuinfo_flag(line, "pclmulqdq") == PF_HAS_PCLMULQDQ);
    CHECK(cpuinfo_flag(line, "movbe") == PF_HAS_MOVBE);
    CHECK(cpuinfo_flag(line, "f16c") == PF_HAS_F16C);
    CHECK(cpuinfo_flag(line, "fma") == PF_HAS_FMA);
    CHECK(cpuinfo_flag(line, "rdrand") == PF_HAS_RDRND);
    CHECK(cpuinfo_flag(line, "bmi1") == PF_HAS_BMI1);
    CHECK(cpuinfo_flag(line, "bmi2") == PF_HAS_BMI2);
    CHECK(cpuinfo_flag(line, "adx") == PF_HAS_ADX);
    CHECK(cpuinfo_flag(line, "rdseed") == PF_HAS_RDSEED);
    CHECK(cpuinfo_flag(line, "avx") == PF_CAN_USE_AVX);
    CHECK(cpuinfo_flag(line, "avx2") == PF_CAN_USE_AVX2);
    CHECK(cpuinfo_flag(line, "avx512f") == PF_CAN_USE_AVX512F);
    return 0;
}

#endif

pf_test_t suite_cpuinfo[] = {
    { test_cpuinfo_vendor, "/cpuinfo/vendor" },
    { test_cpuinfo_brand, "/cpuinfo/brand" },
    { test_cpuinfo_cpuid, "/cpuinfo/cpuid" },
    { test_cpuinfo_max_leaf, "/cpuinfo/max_leaf" },
    { test_cpuinfo_cache, "/cpuinfo/cache" },
    { test_cpuinfo_reg, "/cpuinfo/reg" },
    { test_cpuinfo_baseline, "/cpuinfo/baseline" },
    { test_cpuinfo_flags_are_boolean, "/cpuinfo/flags_are_boolean" },
    { test_cpuinfo_hierarchy, "/cpuinfo/hierarchy" },
    { test_cpuinfo_os_support, "/cpuinfo/os_support" },
    { test_cpuinfo_logical_cores, "/cpuinfo/logical_cores" },
    { test_cpuinfo_supports_implies_has, "/cpuinfo/supports_implies_has" },
#if defined(PF_CPU_X86) && defined(__linux__)
    { test_cpuinfo_proc, "/cpuinfo/proc" },
#endif
    { 0 },
};
