/** cpolyfill - Filling the gaps between C standards and compilers

    CPU features: what the processor running the program can do, and what
    the compiler was allowed to use.

    At run time:

        bool pf_cpu_has(pf_cpu_feature f)
                Whether the instructions of `f` can be used: the processor
                has them and, for those needing extra register state (AVX,
                AVX-512, AMX, APX), the operating system saves it. Read
                once and cached; always false for features of another
                architecture. On Linux AMX tiles also need a permission
                request (arch_prctl ARCH_REQ_XCOMP_PERM) before use.

        x86 features are named as in the CPUID documentation: PF_CPU_SSE2,
        PF_CPU_AVX2, PF_CPU_AVX512F, PF_CPU_BMI2, PF_CPU_AES, PF_CPU_SHA,
        PF_CPU_LZCNT, PF_CPU_HYPERVISOR, ... (the list is below). ARM ones
        are PF_CPU_ARM_NEON, _AES, _PMULL, _SHA1, _SHA2, _SHA3, _SHA512,
        _CRC32, _LSE (atomics), _FP16, _RDM, _DOTPROD, _I8MM, _BF16, _SVE,
        _SVE2 and _SME, read from getauxval (Linux), elf_aux_info
        (FreeBSD), sysctl (macOS) or IsProcessorFeaturePresent (Windows).

    At compile time, PF_TARGET_<name> (PF_TARGET_AVX2, PF_TARGET_ARM_NEON,
    ...) is 1 when the compiler may use the feature in this translation
    unit (-mavx2, -march=..., /arch:AVX2), else 0. MSVC only reports
    /arch levels; the features they imply are derived.

    Raw access (x86; elsewhere all zeros), inline:

        void pf_cpuid(uint32_t leaf, uint32_t regs[4])
        void pf_cpuidex(uint32_t leaf, uint32_t sub, uint32_t regs[4])
                EAX, EBX, ECX, EDX of CPUID, unconditionally.
        uint32_t pf_cpuid_max_leaf(uint32_t range)
                The highest leaf of `range` (0 or 0x80000000), 0 if
                unsupported.
        uint64_t pf_xgetbv(uint32_t index)
                Only when PF_CPU_OSXSAVE is set, it faults otherwise.

    In the library:

        uint32_t pf_cpuid_reg(uint32_t leaf, uint32_t sub, int reg)
                Register `reg` (0 EAX .. 3 EDX) of a leaf, 0 for a leaf the
                processor does not have.
        void pf_cpu_vendor(char vendor[13])
                "GenuineIntel", "AuthenticAMD", ... NUL-terminated; empty
                on other architectures.

    The processor's name and the number of processors are in
    <cpolyfill/sysinfo.h>.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_CPUID_H
#define CPOLYFILL_CPUID_H

#include <cpolyfill/platform.h>

#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF__CPU_IS_X86 (PF_ARCH_X86 || PF_ARCH_X86_64)
#define PF__CPU_IS_ARM (PF_ARCH_ARM || PF_ARCH_ARM64)

#if PF__CPU_IS_X86
    #if defined(_MSC_VER)
        #include <intrin.h>
        #define PF__CPUID_MSVC 1
        #define PF__CPUID_GNU 0
    #elif defined(__GNUC__)
        #include <cpuid.h>
        #define PF__CPUID_MSVC 0
        #define PF__CPUID_GNU 1
    #else
        #define PF__CPUID_MSVC 0
        #define PF__CPUID_GNU 0
    #endif
#else
    #define PF__CPUID_MSVC 0
    #define PF__CPUID_GNU 0
#endif

/* The features: a word of the library's CPUID cache and a bit (x86), or
   an index (ARM). Words: 0 leaf 1 ECX, 1 leaf 1 EDX, 2-4 leaf 7.0 EBX /
   ECX / EDX, 5 leaf 7.1 EAX, 6-7 leaf 0x80000001 ECX / EDX, 8 leaf 7.1
   EDX, 9 leaf 0xD.1 EAX. */
#define PF__CPU_X86(word, bit) ((word) * 32 + (bit))
#define PF__CPU_ARM(index) (0x400 + (index))

/* clang-format off */

typedef enum pf_cpu_feature {
    PF_CPU_SSE3                 = PF__CPU_X86(0, 0),
    PF_CPU_PCLMULQDQ            = PF__CPU_X86(0, 1),
    PF_CPU_MONITOR              = PF__CPU_X86(0, 3),
    PF_CPU_VMX                  = PF__CPU_X86(0, 5),
    PF_CPU_SMX                  = PF__CPU_X86(0, 6),
    PF_CPU_SSSE3                = PF__CPU_X86(0, 9),
    PF_CPU_FMA                  = PF__CPU_X86(0, 12),
    PF_CPU_CMPXCHG16B           = PF__CPU_X86(0, 13),
    PF_CPU_PCID                 = PF__CPU_X86(0, 17),
    PF_CPU_SSE4_1               = PF__CPU_X86(0, 19),
    PF_CPU_SSE4_2               = PF__CPU_X86(0, 20),
    PF_CPU_X2APIC               = PF__CPU_X86(0, 21),
    PF_CPU_MOVBE                = PF__CPU_X86(0, 22),
    PF_CPU_POPCNT               = PF__CPU_X86(0, 23),
    PF_CPU_AES                  = PF__CPU_X86(0, 25),
    PF_CPU_XSAVE                = PF__CPU_X86(0, 26),
    PF_CPU_OSXSAVE              = PF__CPU_X86(0, 27),
    PF_CPU_AVX                  = PF__CPU_X86(0, 28),
    PF_CPU_F16C                 = PF__CPU_X86(0, 29),
    PF_CPU_RDRND                = PF__CPU_X86(0, 30),
    PF_CPU_HYPERVISOR           = PF__CPU_X86(0, 31),
    PF_CPU_FPU                  = PF__CPU_X86(1, 0),
    PF_CPU_TSC                  = PF__CPU_X86(1, 4),
    PF_CPU_MSR                  = PF__CPU_X86(1, 5),
    PF_CPU_PAE                  = PF__CPU_X86(1, 6),
    PF_CPU_CX8                  = PF__CPU_X86(1, 8),
    PF_CPU_APIC                 = PF__CPU_X86(1, 9),
    PF_CPU_SEP                  = PF__CPU_X86(1, 11),
    PF_CPU_MTRR                 = PF__CPU_X86(1, 12),
    PF_CPU_PGE                  = PF__CPU_X86(1, 13),
    PF_CPU_MCA                  = PF__CPU_X86(1, 14),
    PF_CPU_CMOV                 = PF__CPU_X86(1, 15),
    PF_CPU_PAT                  = PF__CPU_X86(1, 16),
    PF_CPU_PSE_36               = PF__CPU_X86(1, 17),
    PF_CPU_CLFSH                = PF__CPU_X86(1, 19),
    PF_CPU_MMX                  = PF__CPU_X86(1, 23),
    PF_CPU_FXSR                 = PF__CPU_X86(1, 24),
    PF_CPU_SSE                  = PF__CPU_X86(1, 25),
    PF_CPU_SSE2                 = PF__CPU_X86(1, 26),
    PF_CPU_HTT                  = PF__CPU_X86(1, 28),
    PF_CPU_FSGSBASE             = PF__CPU_X86(2, 0),
    PF_CPU_SGX                  = PF__CPU_X86(2, 2),
    PF_CPU_BMI1                 = PF__CPU_X86(2, 3),
    PF_CPU_HLE                  = PF__CPU_X86(2, 4),
    PF_CPU_AVX2                 = PF__CPU_X86(2, 5),
    PF_CPU_SMEP                 = PF__CPU_X86(2, 7),
    PF_CPU_BMI2                 = PF__CPU_X86(2, 8),
    PF_CPU_ERMS                 = PF__CPU_X86(2, 9),
    PF_CPU_INVPCID              = PF__CPU_X86(2, 10),
    PF_CPU_RTM                  = PF__CPU_X86(2, 11),
    PF_CPU_MPX                  = PF__CPU_X86(2, 14),
    PF_CPU_AVX512F              = PF__CPU_X86(2, 16),
    PF_CPU_AVX512DQ             = PF__CPU_X86(2, 17),
    PF_CPU_RDSEED               = PF__CPU_X86(2, 18),
    PF_CPU_ADX                  = PF__CPU_X86(2, 19),
    PF_CPU_SMAP                 = PF__CPU_X86(2, 20),
    PF_CPU_AVX512IFMA           = PF__CPU_X86(2, 21),
    PF_CPU_CLFLUSHOPT           = PF__CPU_X86(2, 23),
    PF_CPU_CLWB                 = PF__CPU_X86(2, 24),
    PF_CPU_INTEL_PT             = PF__CPU_X86(2, 25),
    PF_CPU_AVX512PF             = PF__CPU_X86(2, 26),
    PF_CPU_AVX512ER             = PF__CPU_X86(2, 27),
    PF_CPU_AVX512CD             = PF__CPU_X86(2, 28),
    PF_CPU_SHA                  = PF__CPU_X86(2, 29),
    PF_CPU_AVX512BW             = PF__CPU_X86(2, 30),
    PF_CPU_AVX512VL             = PF__CPU_X86(2, 31),
    PF_CPU_PREFETCHWT1          = PF__CPU_X86(3, 0),
    PF_CPU_AVX512VBMI           = PF__CPU_X86(3, 1),
    PF_CPU_UMIP                 = PF__CPU_X86(3, 2),
    PF_CPU_PKU                  = PF__CPU_X86(3, 3),
    PF_CPU_OSPKE                = PF__CPU_X86(3, 4),
    PF_CPU_WAITPKG              = PF__CPU_X86(3, 5),
    PF_CPU_AVX512_VBMI2         = PF__CPU_X86(3, 6),
    PF_CPU_CET_SS               = PF__CPU_X86(3, 7),
    PF_CPU_GFNI                 = PF__CPU_X86(3, 8),
    PF_CPU_VAES                 = PF__CPU_X86(3, 9),
    PF_CPU_VPCLMULQDQ           = PF__CPU_X86(3, 10),
    PF_CPU_AVX512_VNNI          = PF__CPU_X86(3, 11),
    PF_CPU_AVX512_BITALG        = PF__CPU_X86(3, 12),
    PF_CPU_AVX512VPOPCNTDQ      = PF__CPU_X86(3, 14),
    PF_CPU_LA57                 = PF__CPU_X86(3, 16),
    PF_CPU_RDPID                = PF__CPU_X86(3, 22),
    PF_CPU_KL                   = PF__CPU_X86(3, 23),
    PF_CPU_CLDEMOTE             = PF__CPU_X86(3, 25),
    PF_CPU_MOVDIRI              = PF__CPU_X86(3, 27),
    PF_CPU_MOVDIR64B            = PF__CPU_X86(3, 28),
    PF_CPU_ENQCMD               = PF__CPU_X86(3, 29),
    PF_CPU_AVX512_4VNNIW        = PF__CPU_X86(4, 2),
    PF_CPU_AVX512_4FMAPS        = PF__CPU_X86(4, 3),
    PF_CPU_FSRM                 = PF__CPU_X86(4, 4),
    PF_CPU_UINTR                = PF__CPU_X86(4, 5),
    PF_CPU_AVX512_VP2INTERSECT  = PF__CPU_X86(4, 8),
    PF_CPU_MD_CLEAR             = PF__CPU_X86(4, 10),
    PF_CPU_SERIALIZE            = PF__CPU_X86(4, 14),
    PF_CPU_HYBRID               = PF__CPU_X86(4, 15),
    PF_CPU_TSXLDTRK             = PF__CPU_X86(4, 16),
    PF_CPU_PCONFIG              = PF__CPU_X86(4, 18),
    PF_CPU_CET_IBT              = PF__CPU_X86(4, 20),
    PF_CPU_AMX_BF16             = PF__CPU_X86(4, 22),
    PF_CPU_AVX512_FP16          = PF__CPU_X86(4, 23),
    PF_CPU_AMX_TILE             = PF__CPU_X86(4, 24),
    PF_CPU_AMX_INT8             = PF__CPU_X86(4, 25),
    PF_CPU_SHA512               = PF__CPU_X86(5, 0),
    PF_CPU_SM3                  = PF__CPU_X86(5, 1),
    PF_CPU_SM4                  = PF__CPU_X86(5, 2),
    PF_CPU_AVX_VNNI             = PF__CPU_X86(5, 4),
    PF_CPU_AVX512_BF16          = PF__CPU_X86(5, 5),
    PF_CPU_CMPCCXADD            = PF__CPU_X86(5, 7),
    PF_CPU_FSRS                 = PF__CPU_X86(5, 11),
    PF_CPU_AMX_FP16             = PF__CPU_X86(5, 21),
    PF_CPU_AVX_IFMA             = PF__CPU_X86(5, 23),
    PF_CPU_AVX_VNNI_INT8        = PF__CPU_X86(8, 4),
    PF_CPU_AVX_NE_CONVERT       = PF__CPU_X86(8, 5),
    PF_CPU_AMX_COMPLEX          = PF__CPU_X86(8, 8),
    PF_CPU_AVX_VNNI_INT16       = PF__CPU_X86(8, 10),
    PF_CPU_PREFETCHI            = PF__CPU_X86(8, 14),
    PF_CPU_AVX10                = PF__CPU_X86(8, 19),
    PF_CPU_APX_F                = PF__CPU_X86(8, 21),
    PF_CPU_LAHF_LM              = PF__CPU_X86(6, 0),
    PF_CPU_LZCNT                = PF__CPU_X86(6, 5),
    PF_CPU_SSE4A                = PF__CPU_X86(6, 6),
    PF_CPU_PREFETCHW            = PF__CPU_X86(6, 8),
    PF_CPU_XOP                  = PF__CPU_X86(6, 11),
    PF_CPU_FMA4                 = PF__CPU_X86(6, 16),
    PF_CPU_TBM                  = PF__CPU_X86(6, 21),
    PF_CPU_SYSCALL              = PF__CPU_X86(7, 11),
    PF_CPU_NX                   = PF__CPU_X86(7, 20),
    PF_CPU_PDPE1GB              = PF__CPU_X86(7, 26),
    PF_CPU_RDTSCP               = PF__CPU_X86(7, 27),
    PF_CPU_LM                   = PF__CPU_X86(7, 29),
    PF_CPU_XSAVEOPT             = PF__CPU_X86(9, 0),
    PF_CPU_XSAVEC               = PF__CPU_X86(9, 1),
    PF_CPU_XSAVES               = PF__CPU_X86(9, 3),
    PF_CPU_ARM_NEON             = PF__CPU_ARM(0),
    PF_CPU_ARM_AES              = PF__CPU_ARM(1),
    PF_CPU_ARM_PMULL            = PF__CPU_ARM(2),
    PF_CPU_ARM_SHA1             = PF__CPU_ARM(3),
    PF_CPU_ARM_SHA2             = PF__CPU_ARM(4),
    PF_CPU_ARM_SHA3             = PF__CPU_ARM(5),
    PF_CPU_ARM_SHA512           = PF__CPU_ARM(6),
    PF_CPU_ARM_CRC32            = PF__CPU_ARM(7),
    PF_CPU_ARM_LSE              = PF__CPU_ARM(8),
    PF_CPU_ARM_FP16             = PF__CPU_ARM(9),
    PF_CPU_ARM_RDM              = PF__CPU_ARM(10),
    PF_CPU_ARM_DOTPROD          = PF__CPU_ARM(11),
    PF_CPU_ARM_I8MM             = PF__CPU_ARM(12),
    PF_CPU_ARM_BF16             = PF__CPU_ARM(13),
    PF_CPU_ARM_SVE              = PF__CPU_ARM(14),
    PF_CPU_ARM_SVE2             = PF__CPU_ARM(15),
    PF_CPU_ARM_SME              = PF__CPU_ARM(16),
} pf_cpu_feature;

/* clang-format on */

PF_BEGIN_DECLS

PF_INLINE void pf_cpuidex(uint32_t leaf, uint32_t sub, uint32_t regs[4]) {
#if PF__CPUID_MSVC
    int r[4];
    __cpuidex(r, (int)leaf, (int)sub);
    regs[0] = (uint32_t)r[0];
    regs[1] = (uint32_t)r[1];
    regs[2] = (uint32_t)r[2];
    regs[3] = (uint32_t)r[3];
#elif PF__CPUID_GNU
    unsigned a, b, c, d;
    /* Keeps EBX intact where it is the PIC register (i386). */
    __cpuid_count(leaf, sub, a, b, c, d);
    regs[0] = a;
    regs[1] = b;
    regs[2] = c;
    regs[3] = d;
#else
    (void)leaf;
    (void)sub;
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
#endif
}

PF_INLINE void pf_cpuid(uint32_t leaf, uint32_t regs[4]) {
    pf_cpuidex(leaf, 0, regs);
}

PF_INLINE uint32_t pf_cpuid_max_leaf(uint32_t range) {
#if PF__CPUID_MSVC || PF__CPUID_GNU
    uint32_t max;
    #if PF__CPUID_GNU
    /* Also checks that CPUID exists (old i386). */
    max = (uint32_t)__get_cpuid_max(range, 0);
    #else
    uint32_t r[4];
    pf_cpuidex(range, 0, r);
    max = r[0];
    #endif
    /* Without the extended range, CPUID returns basic-leaf data. */
    if (range != 0 && max < range)
        return 0;
    return max;
#else
    (void)range;
    return 0;
#endif
}

PF_INLINE uint64_t pf_xgetbv(uint32_t index) {
#if PF__CPUID_MSVC
    return (uint64_t)_xgetbv(index);
#elif PF__CPUID_GNU
    uint32_t lo, hi;
    /* The bytes of XGETBV, for assemblers that do not know it. */
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0"
                         : "=a"(lo), "=d"(hi)
                         : "c"(index));
    return ((uint64_t)hi << 32) | lo;
#else
    (void)index;
    return 0;
#endif
}

PF_API uint32_t pf_cpuid_reg(uint32_t leaf, uint32_t sub, int reg);
PF_API void pf_cpu_vendor(char vendor[13]);
PF_API bool pf_cpu_has(pf_cpu_feature f);

PF_END_DECLS

/* ---- Compile-time features -------------------------------------------- */

/* MSVC defines only __AVX__, __AVX2__ and __AVX512*__ for /arch, and
   _M_IX86_FP for SSE on 32-bit x86. */
#if PF_CC_MSVC && defined(__AVX__)
    #define PF__MSVC_AVX
#endif
#if PF_CC_MSVC && defined(__AVX2__)
    #define PF__MSVC_AVX2
#endif
#if defined(_M_IX86_FP)
    #if _M_IX86_FP >= 1
        #define PF__MSVC_SSE
    #endif
    #if _M_IX86_FP >= 2
        #define PF__MSVC_SSE2
    #endif
#endif

/* clang-format off */

#if PF__CPU_IS_X86 && (defined(__SSE3__) || defined(PF__MSVC_AVX))
    #define PF_TARGET_SSE3 1
#else
    #define PF_TARGET_SSE3 0
#endif
#if PF__CPU_IS_X86 && defined(__PCLMUL__)
    #define PF_TARGET_PCLMULQDQ 1
#else
    #define PF_TARGET_PCLMULQDQ 0
#endif
#if PF__CPU_IS_X86 && (defined(__SSSE3__) || defined(PF__MSVC_AVX))
    #define PF_TARGET_SSSE3 1
#else
    #define PF_TARGET_SSSE3 0
#endif
#if PF__CPU_IS_X86 && (defined(__FMA__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_FMA 1
#else
    #define PF_TARGET_FMA 0
#endif
#if PF__CPU_IS_X86 && (defined(__CX16__) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16))
    #define PF_TARGET_CMPXCHG16B 1
#else
    #define PF_TARGET_CMPXCHG16B 0
#endif
#if PF__CPU_IS_X86 && (defined(__SSE4_1__) || defined(PF__MSVC_AVX))
    #define PF_TARGET_SSE4_1 1
#else
    #define PF_TARGET_SSE4_1 0
#endif
#if PF__CPU_IS_X86 && (defined(__SSE4_2__) || defined(PF__MSVC_AVX))
    #define PF_TARGET_SSE4_2 1
#else
    #define PF_TARGET_SSE4_2 0
#endif
#if PF__CPU_IS_X86 && (defined(__MOVBE__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_MOVBE 1
#else
    #define PF_TARGET_MOVBE 0
#endif
#if PF__CPU_IS_X86 && (defined(__POPCNT__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_POPCNT 1
#else
    #define PF_TARGET_POPCNT 0
#endif
#if PF__CPU_IS_X86 && defined(__AES__)
    #define PF_TARGET_AES 1
#else
    #define PF_TARGET_AES 0
#endif
#if PF__CPU_IS_X86 && defined(__XSAVE__)
    #define PF_TARGET_XSAVE 1
#else
    #define PF_TARGET_XSAVE 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX__)
    #define PF_TARGET_AVX 1
#else
    #define PF_TARGET_AVX 0
#endif
#if PF__CPU_IS_X86 && (defined(__F16C__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_F16C 1
#else
    #define PF_TARGET_F16C 0
#endif
#if PF__CPU_IS_X86 && defined(__RDRND__)
    #define PF_TARGET_RDRND 1
#else
    #define PF_TARGET_RDRND 0
#endif
#if PF__CPU_IS_X86 && (defined(__i586__) || defined(__i686__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
    #define PF_TARGET_CX8 1
#else
    #define PF_TARGET_CX8 0
#endif
#if PF__CPU_IS_X86 && (defined(__i686__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
    #define PF_TARGET_CMOV 1
#else
    #define PF_TARGET_CMOV 0
#endif
#if PF__CPU_IS_X86 && defined(__MMX__)
    #define PF_TARGET_MMX 1
#else
    #define PF_TARGET_MMX 0
#endif
#if PF__CPU_IS_X86 && (defined(__FXSR__) || defined(_M_X64) || defined(_M_AMD64))
    #define PF_TARGET_FXSR 1
#else
    #define PF_TARGET_FXSR 0
#endif
#if PF__CPU_IS_X86 && (defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || defined(PF__MSVC_SSE))
    #define PF_TARGET_SSE 1
#else
    #define PF_TARGET_SSE 0
#endif
#if PF__CPU_IS_X86 && (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(PF__MSVC_SSE2))
    #define PF_TARGET_SSE2 1
#else
    #define PF_TARGET_SSE2 0
#endif
#if PF__CPU_IS_X86 && defined(__FSGSBASE__)
    #define PF_TARGET_FSGSBASE 1
#else
    #define PF_TARGET_FSGSBASE 0
#endif
#if PF__CPU_IS_X86 && defined(__SGX__)
    #define PF_TARGET_SGX 1
#else
    #define PF_TARGET_SGX 0
#endif
#if PF__CPU_IS_X86 && (defined(__BMI__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_BMI1 1
#else
    #define PF_TARGET_BMI1 0
#endif
#if PF__CPU_IS_X86 && defined(__HLE__)
    #define PF_TARGET_HLE 1
#else
    #define PF_TARGET_HLE 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX2__)
    #define PF_TARGET_AVX2 1
#else
    #define PF_TARGET_AVX2 0
#endif
#if PF__CPU_IS_X86 && (defined(__BMI2__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_BMI2 1
#else
    #define PF_TARGET_BMI2 0
#endif
#if PF__CPU_IS_X86 && defined(__RTM__)
    #define PF_TARGET_RTM 1
#else
    #define PF_TARGET_RTM 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512F__)
    #define PF_TARGET_AVX512F 1
#else
    #define PF_TARGET_AVX512F 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512DQ__)
    #define PF_TARGET_AVX512DQ 1
#else
    #define PF_TARGET_AVX512DQ 0
#endif
#if PF__CPU_IS_X86 && defined(__RDSEED__)
    #define PF_TARGET_RDSEED 1
#else
    #define PF_TARGET_RDSEED 0
#endif
#if PF__CPU_IS_X86 && defined(__ADX__)
    #define PF_TARGET_ADX 1
#else
    #define PF_TARGET_ADX 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512IFMA__)
    #define PF_TARGET_AVX512IFMA 1
#else
    #define PF_TARGET_AVX512IFMA 0
#endif
#if PF__CPU_IS_X86 && defined(__CLFLUSHOPT__)
    #define PF_TARGET_CLFLUSHOPT 1
#else
    #define PF_TARGET_CLFLUSHOPT 0
#endif
#if PF__CPU_IS_X86 && defined(__CLWB__)
    #define PF_TARGET_CLWB 1
#else
    #define PF_TARGET_CLWB 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512PF__)
    #define PF_TARGET_AVX512PF 1
#else
    #define PF_TARGET_AVX512PF 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512ER__)
    #define PF_TARGET_AVX512ER 1
#else
    #define PF_TARGET_AVX512ER 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512CD__)
    #define PF_TARGET_AVX512CD 1
#else
    #define PF_TARGET_AVX512CD 0
#endif
#if PF__CPU_IS_X86 && defined(__SHA__)
    #define PF_TARGET_SHA 1
#else
    #define PF_TARGET_SHA 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512BW__)
    #define PF_TARGET_AVX512BW 1
#else
    #define PF_TARGET_AVX512BW 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512VL__)
    #define PF_TARGET_AVX512VL 1
#else
    #define PF_TARGET_AVX512VL 0
#endif
#if PF__CPU_IS_X86 && defined(__PREFETCHWT1__)
    #define PF_TARGET_PREFETCHWT1 1
#else
    #define PF_TARGET_PREFETCHWT1 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512VBMI__)
    #define PF_TARGET_AVX512VBMI 1
#else
    #define PF_TARGET_AVX512VBMI 0
#endif
#if PF__CPU_IS_X86 && defined(__PKU__)
    #define PF_TARGET_PKU 1
#else
    #define PF_TARGET_PKU 0
#endif
#if PF__CPU_IS_X86 && defined(__WAITPKG__)
    #define PF_TARGET_WAITPKG 1
#else
    #define PF_TARGET_WAITPKG 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512VBMI2__)
    #define PF_TARGET_AVX512_VBMI2 1
#else
    #define PF_TARGET_AVX512_VBMI2 0
#endif
#if PF__CPU_IS_X86 && defined(__SHSTK__)
    #define PF_TARGET_CET_SS 1
#else
    #define PF_TARGET_CET_SS 0
#endif
#if PF__CPU_IS_X86 && defined(__GFNI__)
    #define PF_TARGET_GFNI 1
#else
    #define PF_TARGET_GFNI 0
#endif
#if PF__CPU_IS_X86 && defined(__VAES__)
    #define PF_TARGET_VAES 1
#else
    #define PF_TARGET_VAES 0
#endif
#if PF__CPU_IS_X86 && defined(__VPCLMULQDQ__)
    #define PF_TARGET_VPCLMULQDQ 1
#else
    #define PF_TARGET_VPCLMULQDQ 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512VNNI__)
    #define PF_TARGET_AVX512_VNNI 1
#else
    #define PF_TARGET_AVX512_VNNI 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512BITALG__)
    #define PF_TARGET_AVX512_BITALG 1
#else
    #define PF_TARGET_AVX512_BITALG 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512VPOPCNTDQ__)
    #define PF_TARGET_AVX512VPOPCNTDQ 1
#else
    #define PF_TARGET_AVX512VPOPCNTDQ 0
#endif
#if PF__CPU_IS_X86 && defined(__RDPID__)
    #define PF_TARGET_RDPID 1
#else
    #define PF_TARGET_RDPID 0
#endif
#if PF__CPU_IS_X86 && defined(__KL__)
    #define PF_TARGET_KL 1
#else
    #define PF_TARGET_KL 0
#endif
#if PF__CPU_IS_X86 && defined(__CLDEMOTE__)
    #define PF_TARGET_CLDEMOTE 1
#else
    #define PF_TARGET_CLDEMOTE 0
#endif
#if PF__CPU_IS_X86 && defined(__MOVDIRI__)
    #define PF_TARGET_MOVDIRI 1
#else
    #define PF_TARGET_MOVDIRI 0
#endif
#if PF__CPU_IS_X86 && defined(__MOVDIR64B__)
    #define PF_TARGET_MOVDIR64B 1
#else
    #define PF_TARGET_MOVDIR64B 0
#endif
#if PF__CPU_IS_X86 && defined(__ENQCMD__)
    #define PF_TARGET_ENQCMD 1
#else
    #define PF_TARGET_ENQCMD 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX5124VNNIW__)
    #define PF_TARGET_AVX512_4VNNIW 1
#else
    #define PF_TARGET_AVX512_4VNNIW 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX5124FMAPS__)
    #define PF_TARGET_AVX512_4FMAPS 1
#else
    #define PF_TARGET_AVX512_4FMAPS 0
#endif
#if PF__CPU_IS_X86 && defined(__UINTR__)
    #define PF_TARGET_UINTR 1
#else
    #define PF_TARGET_UINTR 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512VP2INTERSECT__)
    #define PF_TARGET_AVX512_VP2INTERSECT 1
#else
    #define PF_TARGET_AVX512_VP2INTERSECT 0
#endif
#if PF__CPU_IS_X86 && defined(__SERIALIZE__)
    #define PF_TARGET_SERIALIZE 1
#else
    #define PF_TARGET_SERIALIZE 0
#endif
#if PF__CPU_IS_X86 && defined(__TSXLDTRK__)
    #define PF_TARGET_TSXLDTRK 1
#else
    #define PF_TARGET_TSXLDTRK 0
#endif
#if PF__CPU_IS_X86 && defined(__PCONFIG__)
    #define PF_TARGET_PCONFIG 1
#else
    #define PF_TARGET_PCONFIG 0
#endif
#if PF__CPU_IS_X86 && defined(__AMX_BF16__)
    #define PF_TARGET_AMX_BF16 1
#else
    #define PF_TARGET_AMX_BF16 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512FP16__)
    #define PF_TARGET_AVX512_FP16 1
#else
    #define PF_TARGET_AVX512_FP16 0
#endif
#if PF__CPU_IS_X86 && defined(__AMX_TILE__)
    #define PF_TARGET_AMX_TILE 1
#else
    #define PF_TARGET_AMX_TILE 0
#endif
#if PF__CPU_IS_X86 && defined(__AMX_INT8__)
    #define PF_TARGET_AMX_INT8 1
#else
    #define PF_TARGET_AMX_INT8 0
#endif
#if PF__CPU_IS_X86 && defined(__SHA512__)
    #define PF_TARGET_SHA512 1
#else
    #define PF_TARGET_SHA512 0
#endif
#if PF__CPU_IS_X86 && defined(__SM3__)
    #define PF_TARGET_SM3 1
#else
    #define PF_TARGET_SM3 0
#endif
#if PF__CPU_IS_X86 && defined(__SM4__)
    #define PF_TARGET_SM4 1
#else
    #define PF_TARGET_SM4 0
#endif
#if PF__CPU_IS_X86 && defined(__AVXVNNI__)
    #define PF_TARGET_AVX_VNNI 1
#else
    #define PF_TARGET_AVX_VNNI 0
#endif
#if PF__CPU_IS_X86 && defined(__AVX512BF16__)
    #define PF_TARGET_AVX512_BF16 1
#else
    #define PF_TARGET_AVX512_BF16 0
#endif
#if PF__CPU_IS_X86 && defined(__CMPCCXADD__)
    #define PF_TARGET_CMPCCXADD 1
#else
    #define PF_TARGET_CMPCCXADD 0
#endif
#if PF__CPU_IS_X86 && defined(__AMX_FP16__)
    #define PF_TARGET_AMX_FP16 1
#else
    #define PF_TARGET_AMX_FP16 0
#endif
#if PF__CPU_IS_X86 && defined(__AVXIFMA__)
    #define PF_TARGET_AVX_IFMA 1
#else
    #define PF_TARGET_AVX_IFMA 0
#endif
#if PF__CPU_IS_X86 && defined(__AVXVNNIINT8__)
    #define PF_TARGET_AVX_VNNI_INT8 1
#else
    #define PF_TARGET_AVX_VNNI_INT8 0
#endif
#if PF__CPU_IS_X86 && defined(__AVXNECONVERT__)
    #define PF_TARGET_AVX_NE_CONVERT 1
#else
    #define PF_TARGET_AVX_NE_CONVERT 0
#endif
#if PF__CPU_IS_X86 && defined(__AMX_COMPLEX__)
    #define PF_TARGET_AMX_COMPLEX 1
#else
    #define PF_TARGET_AMX_COMPLEX 0
#endif
#if PF__CPU_IS_X86 && defined(__AVXVNNIINT16__)
    #define PF_TARGET_AVX_VNNI_INT16 1
#else
    #define PF_TARGET_AVX_VNNI_INT16 0
#endif
#if PF__CPU_IS_X86 && defined(__PREFETCHI__)
    #define PF_TARGET_PREFETCHI 1
#else
    #define PF_TARGET_PREFETCHI 0
#endif
#if PF__CPU_IS_X86 && defined(__APX_F__)
    #define PF_TARGET_APX_F 1
#else
    #define PF_TARGET_APX_F 0
#endif
#if PF__CPU_IS_X86 && defined(__LAHF_SAHF__)
    #define PF_TARGET_LAHF_LM 1
#else
    #define PF_TARGET_LAHF_LM 0
#endif
#if PF__CPU_IS_X86 && (defined(__LZCNT__) || defined(PF__MSVC_AVX2))
    #define PF_TARGET_LZCNT 1
#else
    #define PF_TARGET_LZCNT 0
#endif
#if PF__CPU_IS_X86 && defined(__SSE4A__)
    #define PF_TARGET_SSE4A 1
#else
    #define PF_TARGET_SSE4A 0
#endif
#if PF__CPU_IS_X86 && defined(__PRFCHW__)
    #define PF_TARGET_PREFETCHW 1
#else
    #define PF_TARGET_PREFETCHW 0
#endif
#if PF__CPU_IS_X86 && defined(__XOP__)
    #define PF_TARGET_XOP 1
#else
    #define PF_TARGET_XOP 0
#endif
#if PF__CPU_IS_X86 && defined(__FMA4__)
    #define PF_TARGET_FMA4 1
#else
    #define PF_TARGET_FMA4 0
#endif
#if PF__CPU_IS_X86 && defined(__TBM__)
    #define PF_TARGET_TBM 1
#else
    #define PF_TARGET_TBM 0
#endif
#if PF__CPU_IS_X86 && (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
    #define PF_TARGET_LM 1
#else
    #define PF_TARGET_LM 0
#endif
#if PF__CPU_IS_X86 && defined(__XSAVEOPT__)
    #define PF_TARGET_XSAVEOPT 1
#else
    #define PF_TARGET_XSAVEOPT 0
#endif
#if PF__CPU_IS_X86 && defined(__XSAVEC__)
    #define PF_TARGET_XSAVEC 1
#else
    #define PF_TARGET_XSAVEC 0
#endif
#if PF__CPU_IS_X86 && defined(__XSAVES__)
    #define PF_TARGET_XSAVES 1
#else
    #define PF_TARGET_XSAVES 0
#endif
#if PF__CPU_IS_ARM && (defined(__ARM_NEON) || defined(_M_ARM64))
    #define PF_TARGET_ARM_NEON 1
#else
    #define PF_TARGET_ARM_NEON 0
#endif
#if PF__CPU_IS_ARM && (defined(__ARM_FEATURE_AES) || defined(__ARM_FEATURE_CRYPTO))
    #define PF_TARGET_ARM_AES 1
#else
    #define PF_TARGET_ARM_AES 0
#endif
#if PF__CPU_IS_ARM && (defined(__ARM_FEATURE_AES) || defined(__ARM_FEATURE_CRYPTO))
    #define PF_TARGET_ARM_PMULL 1
#else
    #define PF_TARGET_ARM_PMULL 0
#endif
#if PF__CPU_IS_ARM && (defined(__ARM_FEATURE_SHA2) || defined(__ARM_FEATURE_CRYPTO))
    #define PF_TARGET_ARM_SHA1 1
#else
    #define PF_TARGET_ARM_SHA1 0
#endif
#if PF__CPU_IS_ARM && (defined(__ARM_FEATURE_SHA2) || defined(__ARM_FEATURE_CRYPTO))
    #define PF_TARGET_ARM_SHA2 1
#else
    #define PF_TARGET_ARM_SHA2 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_SHA3)
    #define PF_TARGET_ARM_SHA3 1
#else
    #define PF_TARGET_ARM_SHA3 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_SHA512)
    #define PF_TARGET_ARM_SHA512 1
#else
    #define PF_TARGET_ARM_SHA512 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_CRC32)
    #define PF_TARGET_ARM_CRC32 1
#else
    #define PF_TARGET_ARM_CRC32 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_ATOMICS)
    #define PF_TARGET_ARM_LSE 1
#else
    #define PF_TARGET_ARM_LSE 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    #define PF_TARGET_ARM_FP16 1
#else
    #define PF_TARGET_ARM_FP16 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_QRDMX)
    #define PF_TARGET_ARM_RDM 1
#else
    #define PF_TARGET_ARM_RDM 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_DOTPROD)
    #define PF_TARGET_ARM_DOTPROD 1
#else
    #define PF_TARGET_ARM_DOTPROD 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_MATMUL_INT8)
    #define PF_TARGET_ARM_I8MM 1
#else
    #define PF_TARGET_ARM_I8MM 0
#endif
#if PF__CPU_IS_ARM && (defined(__ARM_FEATURE_BF16) || defined(__ARM_FEATURE_BF16_VECTOR_ARITHMETIC))
    #define PF_TARGET_ARM_BF16 1
#else
    #define PF_TARGET_ARM_BF16 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_SVE)
    #define PF_TARGET_ARM_SVE 1
#else
    #define PF_TARGET_ARM_SVE 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_SVE2)
    #define PF_TARGET_ARM_SVE2 1
#else
    #define PF_TARGET_ARM_SVE2 0
#endif
#if PF__CPU_IS_ARM && defined(__ARM_FEATURE_SME)
    #define PF_TARGET_ARM_SME 1
#else
    #define PF_TARGET_ARM_SME 0
#endif
/* clang-format on */

#endif /* CPOLYFILL_CPUID_H */
