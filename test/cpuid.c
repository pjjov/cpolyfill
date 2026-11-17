/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/cpuid.h> */

#include "../src/internal.h"

#include <cpolyfill/cpuid.h>

#include <stdio.h>
#include <string.h>

#include "check.h"

/* Every feature with its compile-time macro (0 where there is none). */
static const struct {
    pf_cpu_feature f;
    int target;
    const char *name;
} features[] = {
    { PF_CPU_SSE3, PF_TARGET_SSE3, "SSE3" },
    { PF_CPU_PCLMULQDQ, PF_TARGET_PCLMULQDQ, "PCLMULQDQ" },
    { PF_CPU_MONITOR, 0, "MONITOR" },
    { PF_CPU_VMX, 0, "VMX" },
    { PF_CPU_SMX, 0, "SMX" },
    { PF_CPU_SSSE3, PF_TARGET_SSSE3, "SSSE3" },
    { PF_CPU_FMA, PF_TARGET_FMA, "FMA" },
    { PF_CPU_CMPXCHG16B, PF_TARGET_CMPXCHG16B, "CMPXCHG16B" },
    { PF_CPU_PCID, 0, "PCID" },
    { PF_CPU_SSE4_1, PF_TARGET_SSE4_1, "SSE4_1" },
    { PF_CPU_SSE4_2, PF_TARGET_SSE4_2, "SSE4_2" },
    { PF_CPU_X2APIC, 0, "X2APIC" },
    { PF_CPU_MOVBE, PF_TARGET_MOVBE, "MOVBE" },
    { PF_CPU_POPCNT, PF_TARGET_POPCNT, "POPCNT" },
    { PF_CPU_AES, PF_TARGET_AES, "AES" },
    { PF_CPU_XSAVE, PF_TARGET_XSAVE, "XSAVE" },
    { PF_CPU_OSXSAVE, 0, "OSXSAVE" },
    { PF_CPU_AVX, PF_TARGET_AVX, "AVX" },
    { PF_CPU_F16C, PF_TARGET_F16C, "F16C" },
    { PF_CPU_RDRND, PF_TARGET_RDRND, "RDRND" },
    { PF_CPU_HYPERVISOR, 0, "HYPERVISOR" },
    { PF_CPU_FPU, 0, "FPU" },
    { PF_CPU_TSC, 0, "TSC" },
    { PF_CPU_MSR, 0, "MSR" },
    { PF_CPU_PAE, 0, "PAE" },
    { PF_CPU_CX8, PF_TARGET_CX8, "CX8" },
    { PF_CPU_APIC, 0, "APIC" },
    { PF_CPU_SEP, 0, "SEP" },
    { PF_CPU_MTRR, 0, "MTRR" },
    { PF_CPU_PGE, 0, "PGE" },
    { PF_CPU_MCA, 0, "MCA" },
    { PF_CPU_CMOV, PF_TARGET_CMOV, "CMOV" },
    { PF_CPU_PAT, 0, "PAT" },
    { PF_CPU_PSE_36, 0, "PSE_36" },
    { PF_CPU_CLFSH, 0, "CLFSH" },
    { PF_CPU_MMX, PF_TARGET_MMX, "MMX" },
    { PF_CPU_FXSR, PF_TARGET_FXSR, "FXSR" },
    { PF_CPU_SSE, PF_TARGET_SSE, "SSE" },
    { PF_CPU_SSE2, PF_TARGET_SSE2, "SSE2" },
    { PF_CPU_HTT, 0, "HTT" },
    { PF_CPU_FSGSBASE, PF_TARGET_FSGSBASE, "FSGSBASE" },
    { PF_CPU_SGX, PF_TARGET_SGX, "SGX" },
    { PF_CPU_BMI1, PF_TARGET_BMI1, "BMI1" },
    { PF_CPU_HLE, PF_TARGET_HLE, "HLE" },
    { PF_CPU_AVX2, PF_TARGET_AVX2, "AVX2" },
    { PF_CPU_SMEP, 0, "SMEP" },
    { PF_CPU_BMI2, PF_TARGET_BMI2, "BMI2" },
    { PF_CPU_ERMS, 0, "ERMS" },
    { PF_CPU_INVPCID, 0, "INVPCID" },
    { PF_CPU_RTM, PF_TARGET_RTM, "RTM" },
    { PF_CPU_MPX, 0, "MPX" },
    { PF_CPU_AVX512F, PF_TARGET_AVX512F, "AVX512F" },
    { PF_CPU_AVX512DQ, PF_TARGET_AVX512DQ, "AVX512DQ" },
    { PF_CPU_RDSEED, PF_TARGET_RDSEED, "RDSEED" },
    { PF_CPU_ADX, PF_TARGET_ADX, "ADX" },
    { PF_CPU_SMAP, 0, "SMAP" },
    { PF_CPU_AVX512IFMA, PF_TARGET_AVX512IFMA, "AVX512IFMA" },
    { PF_CPU_CLFLUSHOPT, PF_TARGET_CLFLUSHOPT, "CLFLUSHOPT" },
    { PF_CPU_CLWB, PF_TARGET_CLWB, "CLWB" },
    { PF_CPU_INTEL_PT, 0, "INTEL_PT" },
    { PF_CPU_AVX512PF, PF_TARGET_AVX512PF, "AVX512PF" },
    { PF_CPU_AVX512ER, PF_TARGET_AVX512ER, "AVX512ER" },
    { PF_CPU_AVX512CD, PF_TARGET_AVX512CD, "AVX512CD" },
    { PF_CPU_SHA, PF_TARGET_SHA, "SHA" },
    { PF_CPU_AVX512BW, PF_TARGET_AVX512BW, "AVX512BW" },
    { PF_CPU_AVX512VL, PF_TARGET_AVX512VL, "AVX512VL" },
    { PF_CPU_PREFETCHWT1, PF_TARGET_PREFETCHWT1, "PREFETCHWT1" },
    { PF_CPU_AVX512VBMI, PF_TARGET_AVX512VBMI, "AVX512VBMI" },
    { PF_CPU_UMIP, 0, "UMIP" },
    { PF_CPU_PKU, PF_TARGET_PKU, "PKU" },
    { PF_CPU_OSPKE, 0, "OSPKE" },
    { PF_CPU_WAITPKG, PF_TARGET_WAITPKG, "WAITPKG" },
    { PF_CPU_AVX512_VBMI2, PF_TARGET_AVX512_VBMI2, "AVX512_VBMI2" },
    { PF_CPU_CET_SS, PF_TARGET_CET_SS, "CET_SS" },
    { PF_CPU_GFNI, PF_TARGET_GFNI, "GFNI" },
    { PF_CPU_VAES, PF_TARGET_VAES, "VAES" },
    { PF_CPU_VPCLMULQDQ, PF_TARGET_VPCLMULQDQ, "VPCLMULQDQ" },
    { PF_CPU_AVX512_VNNI, PF_TARGET_AVX512_VNNI, "AVX512_VNNI" },
    { PF_CPU_AVX512_BITALG, PF_TARGET_AVX512_BITALG, "AVX512_BITALG" },
    { PF_CPU_AVX512VPOPCNTDQ, PF_TARGET_AVX512VPOPCNTDQ, "AVX512VPOPCNTDQ" },
    { PF_CPU_LA57, 0, "LA57" },
    { PF_CPU_RDPID, PF_TARGET_RDPID, "RDPID" },
    { PF_CPU_KL, PF_TARGET_KL, "KL" },
    { PF_CPU_CLDEMOTE, PF_TARGET_CLDEMOTE, "CLDEMOTE" },
    { PF_CPU_MOVDIRI, PF_TARGET_MOVDIRI, "MOVDIRI" },
    { PF_CPU_MOVDIR64B, PF_TARGET_MOVDIR64B, "MOVDIR64B" },
    { PF_CPU_ENQCMD, PF_TARGET_ENQCMD, "ENQCMD" },
    { PF_CPU_AVX512_4VNNIW, PF_TARGET_AVX512_4VNNIW, "AVX512_4VNNIW" },
    { PF_CPU_AVX512_4FMAPS, PF_TARGET_AVX512_4FMAPS, "AVX512_4FMAPS" },
    { PF_CPU_FSRM, 0, "FSRM" },
    { PF_CPU_UINTR, PF_TARGET_UINTR, "UINTR" },
    { PF_CPU_AVX512_VP2INTERSECT,
      PF_TARGET_AVX512_VP2INTERSECT,
      "AVX512_VP2INTERSECT" },
    { PF_CPU_MD_CLEAR, 0, "MD_CLEAR" },
    { PF_CPU_SERIALIZE, PF_TARGET_SERIALIZE, "SERIALIZE" },
    { PF_CPU_HYBRID, 0, "HYBRID" },
    { PF_CPU_TSXLDTRK, PF_TARGET_TSXLDTRK, "TSXLDTRK" },
    { PF_CPU_PCONFIG, PF_TARGET_PCONFIG, "PCONFIG" },
    { PF_CPU_CET_IBT, 0, "CET_IBT" },
    { PF_CPU_AMX_BF16, PF_TARGET_AMX_BF16, "AMX_BF16" },
    { PF_CPU_AVX512_FP16, PF_TARGET_AVX512_FP16, "AVX512_FP16" },
    { PF_CPU_AMX_TILE, PF_TARGET_AMX_TILE, "AMX_TILE" },
    { PF_CPU_AMX_INT8, PF_TARGET_AMX_INT8, "AMX_INT8" },
    { PF_CPU_SHA512, PF_TARGET_SHA512, "SHA512" },
    { PF_CPU_SM3, PF_TARGET_SM3, "SM3" },
    { PF_CPU_SM4, PF_TARGET_SM4, "SM4" },
    { PF_CPU_AVX_VNNI, PF_TARGET_AVX_VNNI, "AVX_VNNI" },
    { PF_CPU_AVX512_BF16, PF_TARGET_AVX512_BF16, "AVX512_BF16" },
    { PF_CPU_CMPCCXADD, PF_TARGET_CMPCCXADD, "CMPCCXADD" },
    { PF_CPU_FSRS, 0, "FSRS" },
    { PF_CPU_AMX_FP16, PF_TARGET_AMX_FP16, "AMX_FP16" },
    { PF_CPU_AVX_IFMA, PF_TARGET_AVX_IFMA, "AVX_IFMA" },
    { PF_CPU_AVX_VNNI_INT8, PF_TARGET_AVX_VNNI_INT8, "AVX_VNNI_INT8" },
    { PF_CPU_AVX_NE_CONVERT, PF_TARGET_AVX_NE_CONVERT, "AVX_NE_CONVERT" },
    { PF_CPU_AMX_COMPLEX, PF_TARGET_AMX_COMPLEX, "AMX_COMPLEX" },
    { PF_CPU_AVX_VNNI_INT16, PF_TARGET_AVX_VNNI_INT16, "AVX_VNNI_INT16" },
    { PF_CPU_PREFETCHI, PF_TARGET_PREFETCHI, "PREFETCHI" },
    { PF_CPU_AVX10, 0, "AVX10" },
    { PF_CPU_APX_F, PF_TARGET_APX_F, "APX_F" },
    { PF_CPU_LAHF_LM, PF_TARGET_LAHF_LM, "LAHF_LM" },
    { PF_CPU_LZCNT, PF_TARGET_LZCNT, "LZCNT" },
    { PF_CPU_SSE4A, PF_TARGET_SSE4A, "SSE4A" },
    { PF_CPU_PREFETCHW, PF_TARGET_PREFETCHW, "PREFETCHW" },
    { PF_CPU_XOP, PF_TARGET_XOP, "XOP" },
    { PF_CPU_FMA4, PF_TARGET_FMA4, "FMA4" },
    { PF_CPU_TBM, PF_TARGET_TBM, "TBM" },
    { PF_CPU_SYSCALL, 0, "SYSCALL" },
    { PF_CPU_NX, 0, "NX" },
    { PF_CPU_PDPE1GB, 0, "PDPE1GB" },
    { PF_CPU_RDTSCP, 0, "RDTSCP" },
    { PF_CPU_LM, PF_TARGET_LM, "LM" },
    { PF_CPU_XSAVEOPT, PF_TARGET_XSAVEOPT, "XSAVEOPT" },
    { PF_CPU_XSAVEC, PF_TARGET_XSAVEC, "XSAVEC" },
    { PF_CPU_XSAVES, PF_TARGET_XSAVES, "XSAVES" },
    { PF_CPU_ARM_NEON, PF_TARGET_ARM_NEON, "ARM_NEON" },
    { PF_CPU_ARM_AES, PF_TARGET_ARM_AES, "ARM_AES" },
    { PF_CPU_ARM_PMULL, PF_TARGET_ARM_PMULL, "ARM_PMULL" },
    { PF_CPU_ARM_SHA1, PF_TARGET_ARM_SHA1, "ARM_SHA1" },
    { PF_CPU_ARM_SHA2, PF_TARGET_ARM_SHA2, "ARM_SHA2" },
    { PF_CPU_ARM_SHA3, PF_TARGET_ARM_SHA3, "ARM_SHA3" },
    { PF_CPU_ARM_SHA512, PF_TARGET_ARM_SHA512, "ARM_SHA512" },
    { PF_CPU_ARM_CRC32, PF_TARGET_ARM_CRC32, "ARM_CRC32" },
    { PF_CPU_ARM_LSE, PF_TARGET_ARM_LSE, "ARM_LSE" },
    { PF_CPU_ARM_FP16, PF_TARGET_ARM_FP16, "ARM_FP16" },
    { PF_CPU_ARM_RDM, PF_TARGET_ARM_RDM, "ARM_RDM" },
    { PF_CPU_ARM_DOTPROD, PF_TARGET_ARM_DOTPROD, "ARM_DOTPROD" },
    { PF_CPU_ARM_I8MM, PF_TARGET_ARM_I8MM, "ARM_I8MM" },
    { PF_CPU_ARM_BF16, PF_TARGET_ARM_BF16, "ARM_BF16" },
    { PF_CPU_ARM_SVE, PF_TARGET_ARM_SVE, "ARM_SVE" },
    { PF_CPU_ARM_SVE2, PF_TARGET_ARM_SVE2, "ARM_SVE2" },
    { PF_CPU_ARM_SME, PF_TARGET_ARM_SME, "ARM_SME" },
};

#define COUNT (sizeof(features) / sizeof(features[0]))

static int test_target(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    /* What the compiler may use, the processor running the tests has. */
    for (k = 0; k < COUNT; k++)
        if (features[k].target)
            PF_ASSERT_ALWAYS_MSG(
                pf_cpu_has(features[k].f), "PF_TARGET_%s", features[k].name
            );
#if PF_ARCH_X86_64
    PF_ASSERT_ALWAYS(PF_TARGET_SSE2 && PF_TARGET_LM && !PF_TARGET_ARM_NEON);
#elif PF_ARCH_ARM64
    PF_ASSERT_ALWAYS(PF_TARGET_ARM_NEON && !PF_TARGET_SSE2);
#endif
    return 0;
}

static int test_raw(int seed, int i) {
    uint32_t regs[4];
    char vendor[13];
    (void)seed, (void)i;

    pf_cpu_vendor(vendor);
    PF_ASSERT_ALWAYS_EQ(vendor[12], '\0');
    PF_ASSERT_ALWAYS_EQ(pf_cpuid_reg(0, 0, 4), 0);
    PF_ASSERT_ALWAYS_EQ(pf_cpuid_reg(0, 0, -1), 0);
    PF_ASSERT_ALWAYS(!pf_cpu_has((pf_cpu_feature)-1));
    PF_ASSERT_ALWAYS(!pf_cpu_has((pf_cpu_feature)100000));
#if PF__CPU_IS_X86 && (PF__CPUID_GNU || PF__CPUID_MSVC)
    PF_ASSERT_ALWAYS_EQ(strlen(vendor), 12);
    PF_ASSERT_ALWAYS(pf_cpuid_max_leaf(0) >= 1);
    PF_ASSERT_ALWAYS_EQ(pf_cpuid_reg(0, 0, 0), pf_cpuid_max_leaf(0));
    PF_ASSERT_ALWAYS_EQ(pf_cpuid_reg(pf_cpuid_max_leaf(0) + 1, 0, 0), 0);
    pf_cpuid(1, regs);
    PF_ASSERT_ALWAYS_EQ(pf_cpuid_reg(1, 0, 3), regs[3]);
    PF_ASSERT_ALWAYS(pf_cpu_has(PF_CPU_SSE));
    PF_ASSERT_ALWAYS(!pf_cpu_has(PF_CPU_ARM_NEON));
    if (pf_cpu_has(PF_CPU_OSXSAVE))
        PF_ASSERT_ALWAYS(pf_xgetbv(0) & 1u); /* x87 state, always on */
    if (pf_cpu_has(PF_CPU_AVX2) || pf_cpu_has(PF_CPU_AVX512F))
        PF_ASSERT_ALWAYS(pf_cpu_has(PF_CPU_AVX));
#else
    pf_cpuid(0, regs);
    PF_ASSERT_ALWAYS_EQ(regs[0] | regs[1] | regs[2] | regs[3], 0);
    PF_ASSERT_ALWAYS_EQ(pf_xgetbv(0), 0);
    PF_ASSERT_ALWAYS_EQ(vendor[0], '\0');
    PF_ASSERT_ALWAYS(!pf_cpu_has(PF_CPU_SSE2));
#endif
#if PF_ARCH_ARM64
    PF_ASSERT_ALWAYS(pf_cpu_has(PF_CPU_ARM_NEON));
#endif
    return 0;
}

/* Compared with the flags Linux reports, which also leave out what the
   kernel does not enable (the AVX-512 state, for one). */
static int test_linux(int seed, int i) {
#if PF_OS_LINUX && (PF__CPU_IS_X86 || PF_ARCH_ARM64)
    static const struct {
        const char *flag;
        pf_cpu_feature f;
    } map[] = {
    #if PF__CPU_IS_X86
        { "sse2", PF_CPU_SSE2 },         { "ssse3", PF_CPU_SSSE3 },
        { "sse4_1", PF_CPU_SSE4_1 },     { "sse4_2", PF_CPU_SSE4_2 },
        { "avx", PF_CPU_AVX },           { "avx2", PF_CPU_AVX2 },
        { "fma", PF_CPU_FMA },           { "bmi1", PF_CPU_BMI1 },
        { "bmi2", PF_CPU_BMI2 },         { "aes", PF_CPU_AES },
        { "popcnt", PF_CPU_POPCNT },     { "avx512f", PF_CPU_AVX512F },
        { "avx512bw", PF_CPU_AVX512BW }, { "sha_ni", PF_CPU_SHA },
        { "abm", PF_CPU_LZCNT },         { "hypervisor", PF_CPU_HYPERVISOR },
        { "adx", PF_CPU_ADX },           { "pclmulqdq", PF_CPU_PCLMULQDQ },
    #else
        { "asimd", PF_CPU_ARM_NEON },      { "aes", PF_CPU_ARM_AES },
        { "pmull", PF_CPU_ARM_PMULL },     { "sha2", PF_CPU_ARM_SHA2 },
        { "crc32", PF_CPU_ARM_CRC32 },     { "atomics", PF_CPU_ARM_LSE },
        { "asimddp", PF_CPU_ARM_DOTPROD }, { "sve", PF_CPU_ARM_SVE },
    #endif
    };
    static char line[8192];
    char *flags = NULL;
    FILE *f = fopen("/proc/cpuinfo", "r");
    size_t k;
    (void)seed, (void)i;

    if (!f)
        return SKIP("no /proc/cpuinfo");
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "flags", 5) == 0
            || strncmp(line, "Features", 8) == 0) {
            flags = strchr(line, ':');
            break;
        }
    fclose(f);
    if (!flags)
        return SKIP("no flags in /proc/cpuinfo");
    flags[strcspn(flags, "\n")] = ' ';

    for (k = 0; k < sizeof(map) / sizeof(map[0]); k++) {
        char word[32];
        bool listed;
        snprintf(word, sizeof(word), " %s ", map[k].flag);
        listed = strstr(flags, word) != NULL;
        PF_ASSERT_ALWAYS_MSG(
            pf_cpu_has(map[k].f) == listed,
            "%s: cpuinfo %d",
            map[k].flag,
            (int)listed
        );
    }
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("Linux only");
#endif
}

pf_test_t suite_cpuid[] = {
    { test_target, "target" },
    { test_raw, "raw" },
    { test_linux, "linux" },
    { 0 },
};
