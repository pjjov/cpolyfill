/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/cpuid.h>: the cached CPUID words and XCR0 on x86, the
    hardware capabilities the system reports on ARM.
*/

#include "internal.h"

#include <cpolyfill/attribute.h>
#include <cpolyfill/cpuid.h>
#include <cpolyfill/threads.h>

#include <string.h>

#if PF_OS_APPLE
    #include <sys/sysctl.h>
    #include <sys/types.h>
#endif
#if PF__CPU_IS_ARM
    #if PF_OS_LINUX || PF_OS_ANDROID || PF_OS_FREEBSD
        #include <sys/auxv.h>
    #elif PF_OS_WINDOWS
        #include <windows.h>
    #endif
#endif

#define PF__CPU_WORDS 10

static struct {
    uint32_t max_basic;
    uint32_t max_ext;
    uint32_t words[PF__CPU_WORDS];
    uint64_t xcr0;
    uint64_t arm; /* bit i: feature PF__CPU_ARM(i) */
} pf__cpu;

static pf_once_flag pf__cpu_once = PF_ONCE_FLAG_INIT;

#if PF_OS_APPLE
/* A boolean sysctl, false if it does not exist. */
PF_UNUSED static bool pf__sysctl_bool(const char *name) {
    int64_t v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
        return false;
    /* 4- or 8-byte integers; both little-endian here */
    return len == 4 ? (int32_t)v != 0 : v != 0;
}
#endif

#if PF__CPU_IS_ARM

enum {
    PF__ARM_NEON = PF_CPU_ARM_NEON - 0x400,
    PF__ARM_AES = PF_CPU_ARM_AES - 0x400,
    PF__ARM_PMULL = PF_CPU_ARM_PMULL - 0x400,
    PF__ARM_SHA1 = PF_CPU_ARM_SHA1 - 0x400,
    PF__ARM_SHA2 = PF_CPU_ARM_SHA2 - 0x400,
    PF__ARM_SHA3 = PF_CPU_ARM_SHA3 - 0x400,
    PF__ARM_SHA512 = PF_CPU_ARM_SHA512 - 0x400,
    PF__ARM_CRC32 = PF_CPU_ARM_CRC32 - 0x400,
    PF__ARM_LSE = PF_CPU_ARM_LSE - 0x400,
    PF__ARM_FP16 = PF_CPU_ARM_FP16 - 0x400,
    PF__ARM_RDM = PF_CPU_ARM_RDM - 0x400,
    PF__ARM_DOTPROD = PF_CPU_ARM_DOTPROD - 0x400,
    PF__ARM_I8MM = PF_CPU_ARM_I8MM - 0x400,
    PF__ARM_BF16 = PF_CPU_ARM_BF16 - 0x400,
    PF__ARM_SVE = PF_CPU_ARM_SVE - 0x400,
    PF__ARM_SVE2 = PF_CPU_ARM_SVE2 - 0x400,
    PF__ARM_SME = PF_CPU_ARM_SME - 0x400
};

    #define PF__BIT(i) ((uint64_t)1 << (i))

    #if PF_OS_LINUX || PF_OS_ANDROID || PF_OS_FREEBSD
static unsigned long pf__auxval(unsigned long type) {
        #if PF_OS_FREEBSD
    unsigned long v = 0;
    if (elf_aux_info((int)type, &v, sizeof(v)) != 0)
        return 0;
    return v;
        #else
    return getauxval(type);
        #endif
}
    #endif

static uint64_t pf__arm_detect(void) {
    uint64_t f = 0;
    #if (PF_OS_LINUX || PF_OS_ANDROID || PF_OS_FREEBSD) && PF_ARCH_ARM64
    /* HWCAP_* and HWCAP2_* of the arm64 kernel ABI (AT_HWCAP 16,
       AT_HWCAP2 26). */
    static const struct {
        unsigned char word, bit, feature;
    } map[] = {
        { 0, 1, PF__ARM_NEON },     { 0, 3, PF__ARM_AES },
        { 0, 4, PF__ARM_PMULL },    { 0, 5, PF__ARM_SHA1 },
        { 0, 6, PF__ARM_SHA2 },     { 0, 7, PF__ARM_CRC32 },
        { 0, 8, PF__ARM_LSE },      { 0, 10, PF__ARM_FP16 },
        { 0, 12, PF__ARM_RDM },     { 0, 17, PF__ARM_SHA3 },
        { 0, 20, PF__ARM_DOTPROD }, { 0, 21, PF__ARM_SHA512 },
        { 0, 22, PF__ARM_SVE },     { 1, 1, PF__ARM_SVE2 },
        { 1, 13, PF__ARM_I8MM },    { 1, 14, PF__ARM_BF16 },
        { 1, 23, PF__ARM_SME },
    };
    unsigned long hw[2];
    size_t i;
    hw[0] = pf__auxval(16);
    hw[1] = pf__auxval(26);
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (hw[map[i].word] >> map[i].bit & 1u)
            f |= PF__BIT(map[i].feature);
    #elif (PF_OS_LINUX || PF_OS_ANDROID || PF_OS_FREEBSD) && PF_ARCH_ARM
    /* HWCAP_NEON, and HWCAP2_AES .. HWCAP2_CRC32 of the arm kernel ABI. */
    unsigned long hw = pf__auxval(16), hw2 = pf__auxval(26);
    if (hw >> 12 & 1u)
        f |= PF__BIT(PF__ARM_NEON);
    if (hw2 & 1u)
        f |= PF__BIT(PF__ARM_AES);
    if (hw2 >> 1 & 1u)
        f |= PF__BIT(PF__ARM_PMULL);
    if (hw2 >> 2 & 1u)
        f |= PF__BIT(PF__ARM_SHA1);
    if (hw2 >> 3 & 1u)
        f |= PF__BIT(PF__ARM_SHA2);
    if (hw2 >> 4 & 1u)
        f |= PF__BIT(PF__ARM_CRC32);
    #elif PF_OS_APPLE
    static const struct {
        const char *name;
        int feature;
    } map[] = {
        { "hw.optional.neon", PF__ARM_NEON },
        { "hw.optional.arm.FEAT_AES", PF__ARM_AES },
        { "hw.optional.arm.FEAT_PMULL", PF__ARM_PMULL },
        { "hw.optional.arm.FEAT_SHA1", PF__ARM_SHA1 },
        { "hw.optional.arm.FEAT_SHA256", PF__ARM_SHA2 },
        { "hw.optional.arm.FEAT_SHA3", PF__ARM_SHA3 },
        { "hw.optional.arm.FEAT_SHA512", PF__ARM_SHA512 },
        { "hw.optional.armv8_crc32", PF__ARM_CRC32 },
        { "hw.optional.arm.FEAT_LSE", PF__ARM_LSE },
        { "hw.optional.arm.FEAT_FP16", PF__ARM_FP16 },
        { "hw.optional.arm.FEAT_RDM", PF__ARM_RDM },
        { "hw.optional.arm.FEAT_DotProd", PF__ARM_DOTPROD },
        { "hw.optional.arm.FEAT_I8MM", PF__ARM_I8MM },
        { "hw.optional.arm.FEAT_BF16", PF__ARM_BF16 },
        { "hw.optional.arm.FEAT_SME", PF__ARM_SME },
    };
    size_t i;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (pf__sysctl_bool(map[i].name))
            f |= PF__BIT(map[i].feature);
    #elif PF_OS_WINDOWS
        #ifndef PF_ARM_NEON_INSTRUCTIONS_AVAILABLE
            #define PF_ARM_NEON_INSTRUCTIONS_AVAILABLE 19
        #endif
        #ifndef PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
            #define PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE 30
        #endif
        #ifndef PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE
            #define PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE 31
        #endif
        #ifndef PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE
            #define PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE 34
        #endif
        #ifndef PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE
            #define PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE 43
        #endif
    if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_NEON);
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_AES) | PF__BIT(PF__ARM_PMULL)
            | PF__BIT(PF__ARM_SHA1) | PF__BIT(PF__ARM_SHA2);
    if (IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_CRC32);
    if (IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_LSE);
    if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_DOTPROD);
        #ifdef PF_ARM_SVE_INSTRUCTIONS_AVAILABLE
    if (IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_SVE);
        #endif
        #ifdef PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE
    if (IsProcessorFeaturePresent(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE))
        f |= PF__BIT(PF__ARM_SVE2);
        #endif
    #endif
    /* Advanced SIMD is part of the 64-bit architecture. */
    #if PF_ARCH_ARM64
    f |= PF__BIT(PF__ARM_NEON);
    #endif
    return f;
}

#endif /* PF__CPU_IS_ARM */

static void pf__cpu_init(void) {
#if PF__CPU_IS_X86
    static const struct {
        uint32_t leaf, sub;
        unsigned char reg;
    } src[PF__CPU_WORDS] = {
        { 1, 0, 2 },           { 1, 0, 3 },           { 7, 0, 1 },
        { 7, 0, 2 },           { 7, 0, 3 },           { 7, 1, 0 },
        { 0x80000001u, 0, 2 }, { 0x80000001u, 0, 3 }, { 7, 1, 3 },
        { 0xD, 1, 0 },
    };
    int i;

    pf__cpu.max_basic = pf_cpuid_max_leaf(0);
    pf__cpu.max_ext = pf_cpuid_max_leaf(0x80000000u);
    for (i = 0; i < PF__CPU_WORDS; i++) {
        uint32_t leaf = src[i].leaf, regs[4];
        uint32_t max = leaf >= 0x80000000u ? pf__cpu.max_ext
                                           : pf__cpu.max_basic;
        if (max == 0 || leaf > max)
            continue;
        /* Leaf 7 lists its sub-leaves in 7.0 EAX. */
        if (leaf == 7 && src[i].sub > 0) {
            pf_cpuidex(7, 0, regs);
            if (regs[0] < src[i].sub)
                continue;
        }
        pf_cpuidex(leaf, src[i].sub, regs);
        pf__cpu.words[i] = regs[src[i].reg];
    }
    if (pf__cpu.words[0] >> 27 & 1u) /* OSXSAVE */
        pf__cpu.xcr0 = pf_xgetbv(0);
#elif PF__CPU_IS_ARM
    pf__cpu.arm = pf__arm_detect();
#endif
}

static void pf__cpu_ready(void) { pf_call_once(&pf__cpu_once, pf__cpu_init); }

PF_API uint32_t pf_cpuid_reg(uint32_t leaf, uint32_t sub, int reg) {
    uint32_t regs[4], max;

    if (reg < 0 || reg > 3)
        return 0;
    pf__cpu_ready();
    max = leaf >= 0x80000000u ? pf__cpu.max_ext : pf__cpu.max_basic;
    if (max == 0 || leaf > max)
        return 0;
    pf_cpuidex(leaf, sub, regs);
    return regs[reg];
}

PF_API void pf_cpu_vendor(char vendor[13]) {
    if (!vendor)
        return;
    memset(vendor, 0, 13);
#if PF__CPU_IS_X86
    pf__cpu_ready();
    if (pf__cpu.max_basic > 0 || pf_cpuid_max_leaf(0) > 0) {
        uint32_t regs[4];
        pf_cpuid(0, regs);
        memcpy(vendor + 0, &regs[1], 4);
        memcpy(vendor + 4, &regs[3], 4);
        memcpy(vendor + 8, &regs[2], 4);
    }
#endif
}

#if PF__CPU_IS_X86

/* The XCR0 bits a feature's registers need: SSE + AVX (0x6), plus the
   AVX-512 opmask and upper registers (0xE0), AMX tiles (0x60000), APX
   (0x80000). */
static uint64_t pf__cpu_state(pf_cpu_feature f) {
    switch (f) {
    case PF_CPU_AVX:
    case PF_CPU_AVX2:
    case PF_CPU_FMA:
    case PF_CPU_F16C:
    case PF_CPU_FMA4:
    case PF_CPU_XOP:
    case PF_CPU_VAES:
    case PF_CPU_VPCLMULQDQ:
    case PF_CPU_AVX_VNNI:
    case PF_CPU_AVX_IFMA:
    case PF_CPU_AVX_VNNI_INT8:
    case PF_CPU_AVX_NE_CONVERT:
    case PF_CPU_AVX_VNNI_INT16:
    case PF_CPU_SHA512:
    case PF_CPU_SM3:
    case PF_CPU_SM4:
        return 0x6;
    case PF_CPU_AVX512F:
    case PF_CPU_AVX512DQ:
    case PF_CPU_AVX512IFMA:
    case PF_CPU_AVX512PF:
    case PF_CPU_AVX512ER:
    case PF_CPU_AVX512CD:
    case PF_CPU_AVX512BW:
    case PF_CPU_AVX512VL:
    case PF_CPU_AVX512VBMI:
    case PF_CPU_AVX512_VBMI2:
    case PF_CPU_AVX512_VNNI:
    case PF_CPU_AVX512_BITALG:
    case PF_CPU_AVX512VPOPCNTDQ:
    case PF_CPU_AVX512_4VNNIW:
    case PF_CPU_AVX512_4FMAPS:
    case PF_CPU_AVX512_VP2INTERSECT:
    case PF_CPU_AVX512_FP16:
    case PF_CPU_AVX512_BF16:
    case PF_CPU_AVX10:
        return 0xE6;
    case PF_CPU_AMX_BF16:
    case PF_CPU_AMX_TILE:
    case PF_CPU_AMX_INT8:
    case PF_CPU_AMX_FP16:
    case PF_CPU_AMX_COMPLEX:
        return 0x60000;
    case PF_CPU_APX_F:
        return 0x80000;
    default:
        return 0;
    }
}

#endif

PF_API bool pf_cpu_has(pf_cpu_feature f) {
    int v = (int)f;

    if (v < 0)
        return false;
    pf__cpu_ready();
#if PF__CPU_IS_X86
    {
        uint64_t need;
        if (v / 32 >= PF__CPU_WORDS)
            return false;
        if (!(pf__cpu.words[v / 32] >> (v % 32) & 1u))
            return false;
        need = pf__cpu_state(f);
        if ((pf__cpu.xcr0 & need) == need)
            return true;
    #if PF_OS_MACOS
        /* macOS turns the AVX-512 state on at the first use, so XCR0
           does not show it yet. */
        if (need == 0xE6 && (pf__cpu.xcr0 & 0x6) == 0x6)
            return pf__sysctl_bool("hw.optional.avx512f");
    #endif
        return false;
    }
#elif PF__CPU_IS_ARM
    v -= 0x400;
    return v >= 0 && v < 64 && (pf__cpu.arm >> v & 1u);
#else
    return false;
#endif
}
