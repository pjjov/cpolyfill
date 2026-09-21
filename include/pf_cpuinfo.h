/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides detection of x86 CPU features and other information.

    Two families of feature macros are provided:

    - PF_HAS_<feature>       Runtime: does the CPU report this feature (CPUID)?
                             Note: for AVX/AVX2/AVX-512 the OS must also enable
                             the register state, see PF_CAN_USE_<feature>.
    - PF_SUPPORTS_<feature>  Compile-time: was the compiler told it may emit /
                             use this feature (e.g. -mavx2, /arch:AVX2)?
                             Defined (empty) when supported, undefined otherwise.

    Function reference:
    - PF_HAS_<feature>
    - PF_SUPPORTS_<feature>
    - PF_CAN_USE_AVX, PF_CAN_USE_AVX2, PF_CAN_USE_AVX512F
    - void     pf_cpuid(uint32_t leaf, uint32_t regs[4]);
    - void     pf_cpuidex(uint32_t leaf, uint32_t sub, uint32_t regs[4]);
    - uint64_t pf_xgetbv(uint32_t index);
    - void     pf_cpu_vendor(char vendor[13]);   (NUL-terminated)
    - void     pf_cpu_brand(char brand[48]);     (NUL-terminated)
    - int      pf_cpu_logical_cores(void);       (per package, 0 = unknown)

    Last-updated: September 2026
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
**/

#ifndef POLYFILL_CPUINFO
#define POLYFILL_CPUINFO

#ifndef PF_API
    #define PF_API static inline
#endif

#include <stdint.h>
#include <string.h>

#if defined(__i686__) || defined(__i586__) || defined(__i486__)        \
    || defined(__i386__) || defined(__i386) || defined(_M_IX86)        \
    || defined(_X86_) || defined(__THW_INTEL__) || defined(__x86_64__) \
    || defined(_M_AMD64) || defined(_M_X64)
    #define PF_CPU_X86

    #if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
        #define PF_CPU_X86_64
    #endif

    #if defined(_MSC_VER)
        #include <immintrin.h>
        #include <intrin.h>
    #else
        #if defined(__has_include)
            #if __has_include(<cpuid.h>)
                #define PF__HAVE_CPUID_H
            #endif
        #elif defined(__GNUC__)                                         \
            && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
            #define PF__HAVE_CPUID_H
        #endif

        #ifdef PF__HAVE_CPUID_H
            #include <cpuid.h>
        #endif
    #endif
#endif

#if defined(_MSC_VER)
    #define PF__BARRIER() _ReadWriteBarrier()
#elif defined(__GNUC__) || defined(__clang__)
    #define PF__BARRIER() __asm__ __volatile__("" ::: "memory")
#else
    #define PF__BARRIER() ((void)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Raw CPUID access                                                          */
/* ------------------------------------------------------------------------ */

/* Executes CPUID unconditionally. Prefer pf_cpuid_reg()/PF_HAS_* which
   validate the leaf and cache the result. All zeros on non-x86. */
PF_API void pf_cpuidex(uint32_t leaf, uint32_t sub, uint32_t regs[4]) {
#ifdef PF_CPU_X86
    #if defined(_MSC_VER)
    int r[4];
    __cpuidex(r, (int)leaf, (int)sub);
    regs[0] = (uint32_t)r[0];
    regs[1] = (uint32_t)r[1];
    regs[2] = (uint32_t)r[2];
    regs[3] = (uint32_t)r[3];
    #elif defined(PF__HAVE_CPUID_H)
    uint32_t a, b, c, d;
    /* Handles PIC/EBX on i386 and sets ECX to the sub-leaf. */
    __cpuid_count(leaf, sub, a, b, c, d);
    regs[0] = a;
    regs[1] = b;
    regs[2] = c;
    regs[3] = d;
    #else
    uint32_t a, b, c, d;
        #if defined(__i386__) || defined(__i386) || defined(_X86_)
    /* EBX may be the PIC register on i386: save/restore it manually. */
    __asm__ __volatile__("movl %%ebx, %1\n\t"
                         "cpuid\n\t"
                         "xchgl %%ebx, %1\n\t"
                         : "=a"(a), "=&r"(b), "=c"(c), "=d"(d)
                         : "0"(leaf), "2"(sub));
        #else
    __asm__ __volatile__("cpuid"
                         : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "0"(leaf), "2"(sub));
        #endif
    regs[0] = a;
    regs[1] = b;
    regs[2] = c;
    regs[3] = d;
    #endif
#else
    (void)leaf;
    (void)sub;
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
#endif
}

PF_API void pf_cpuid(uint32_t leaf, uint32_t regs[4]) {
    pf_cpuidex(leaf, 0, regs);
}

/* Highest supported leaf of a range. `range` is 0 (basic) or 0x80000000
   (extended). Returns 0 if CPUID or the range is unsupported. */
PF_API uint32_t pf_cpuid_max_leaf(uint32_t range) {
#ifdef PF_CPU_X86
    uint32_t max;
    #if defined(PF__HAVE_CPUID_H)
    max = (uint32_t)__get_cpuid_max(range, 0); /* also probes CPUID support */
    #else
    uint32_t r[4];
    pf_cpuidex(range, 0, r);
    max = r[0];
    #endif
    /* Without extended leaves CPUID returns basic-leaf data instead. */
    if (range != 0 && max < range)
        return 0;
    return max;
#else
    (void)range;
    return 0;
#endif
}

/* ------------------------------------------------------------------------ */
/* Cached, validated register access                                         */
/* ------------------------------------------------------------------------ */

/* CPUID is slow (and traps to the hypervisor in VMs), so the leaves used by
   the PF_HAS_* macros are read once. Concurrent first calls may race but
   always write identical values. */
typedef struct pf_cpu_cache_t {
    uint32_t max_basic;
    uint32_t max_ext;
    uint32_t l1[4];
    uint32_t l7[4];
} pf_cpu_cache_t;

PF_API const pf_cpu_cache_t *pf_cpu_cache(void) {
    static pf_cpu_cache_t cache;
    static volatile int ready = 0;

    if (!ready) {
        pf_cpu_cache_t t;
        memset(&t, 0, sizeof(t));
        t.max_basic = pf_cpuid_max_leaf(0);
        t.max_ext = pf_cpuid_max_leaf(0x80000000u);
        if (t.max_basic >= 1)
            pf_cpuidex(1, 0, t.l1);
        if (t.max_basic >= 7)
            pf_cpuidex(7, 0, t.l7);
        cache = t;
        PF__BARRIER();
        ready = 1;
    }
    return &cache;
}

/* Returns 0 if the leaf is not supported by the CPU (instead of the
   undefined/garbage data CPUID returns for out-of-range leaves). */
PF_API uint32_t pf_cpuid_reg(uint32_t leaf, uint32_t sub, int reg) {
    const pf_cpu_cache_t *c;
    uint32_t regs[4];

    if (reg < 0 || reg > 3)
        return 0;

    c = pf_cpu_cache();
    if (leaf == 1 && sub == 0)
        return c->l1[reg];
    if (leaf == 7 && sub == 0)
        return c->l7[reg];

    if (leaf >= 0x80000000u ? leaf > c->max_ext : leaf > c->max_basic)
        return 0;

    pf_cpuidex(leaf, sub, regs);
    return regs[reg];
}

PF_API int pf_cpuid_flag(uint32_t leaf, uint32_t sub, int reg, int shift) {
    return (int)((pf_cpuid_reg(leaf, sub, reg) >> shift) & 1u);
}

/* ------------------------------------------------------------------------ */
/* Vendor / brand / topology                                                 */
/* ------------------------------------------------------------------------ */

/* Writes the 12-character vendor id plus a NUL terminator. */
PF_API void pf_cpu_vendor(char vendor[13]) {
    if (!vendor)
        return;
    memset(vendor, 0, 13);
#ifdef PF_CPU_X86
    {
        uint32_t regs[4];
        pf_cpuid(0, regs);
        memcpy(vendor + 0, &regs[1], sizeof(uint32_t));
        memcpy(vendor + 4, &regs[3], sizeof(uint32_t));
        memcpy(vendor + 8, &regs[2], sizeof(uint32_t));
    }
#endif
}

/* Writes the NUL-terminated brand string (may have leading spaces).
   Empty string if unsupported. */
PF_API void pf_cpu_brand(char brand[48]) {
    if (!brand)
        return;
    memset(brand, 0, 48);
#ifdef PF_CPU_X86
    if (pf_cpu_cache()->max_ext >= 0x80000004u) {
        uint32_t regs[4];
        uint32_t i;
        for (i = 0; i < 3; i++) {
            pf_cpuid(0x80000002u + i, regs);
            memcpy(brand + 16 * i, regs, sizeof(regs));
        }
        brand[47] = '\0';
    }
#endif
}

PF_API int pf_cpu_vendor_is_(const char id[12]) {
    char v[13];
    pf_cpu_vendor(v);
    return memcmp(v, id, 12) == 0;
}

/* Number of logical processors in the *package* (socket) the calling thread
   runs on, as reported by CPUID. It is not system-wide: multi-socket machines
   and restricted VMs/affinity masks will differ. Returns 0 if unknown. */
PF_API int pf_cpu_logical_cores(void) {
#ifdef PF_CPU_X86
    const pf_cpu_cache_t *c = pf_cpu_cache();
    uint32_t regs[4];
    uint32_t n = 0;

    if (c->max_basic == 0)
        return 0;

    /* AMD/Hygon: threads per package from the extended leaf. */
    if ((pf_cpu_vendor_is_("AuthenticAMD") || pf_cpu_vendor_is_("HygonGenuine"))
        && c->max_ext >= 0x80000008u) {
        pf_cpuid(0x80000008u, regs);
        return (int)(regs[2] & 0xffu) + 1;
    }

    /* Intel (and others): extended topology enumeration, leaf 0x1F then 0xB.
       The largest level count is the number of logical CPUs in the package. */
    {
        uint32_t leafs[2];
        int i;
        leafs[0] = 0x1F;
        leafs[1] = 0x0B;
        for (i = 0; i < 2 && n == 0; i++) {
            uint32_t sub;
            if (c->max_basic < leafs[i])
                continue;
            for (sub = 0; sub < 8; sub++) {
                pf_cpuidex(leafs[i], sub, regs);
                if (((regs[2] >> 8) & 0xffu) == 0) /* level type 0: invalid */
                    break;
                if ((regs[1] & 0xffffu) > n)
                    n = regs[1] & 0xffffu;
            }
        }
        if (n)
            return (int)n;
    }

    /* Legacy: leaf 1 EBX[23:16] is only meaningful if HTT is set. */
    if ((c->l1[3] >> 28) & 1u) {
        n = (c->l1[1] >> 16) & 0xffu;
        if (n)
            return (int)n;
    }
    return 1;
#else
    return 0;
#endif
}

/* ------------------------------------------------------------------------ */
/* OS support for extended register state (XCR0)                             */
/* ------------------------------------------------------------------------ */

/* Only valid if CPUID.1:ECX.OSXSAVE is set, otherwise XGETBV raises #UD. */
PF_API uint64_t pf_xgetbv(uint32_t index) {
#ifdef PF_CPU_X86
    #if defined(_MSC_VER)
    return (uint64_t)_xgetbv(index);
    #else
    uint32_t lo, hi;
    /* Raw bytes: works with assemblers that don't know xgetbv. */
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0"
                         : "=a"(lo), "=d"(hi)
                         : "c"(index));
    return ((uint64_t)hi << 32) | lo;
    #endif
#else
    (void)index;
    return 0;
#endif
}

/* SSE (bit 1) and AVX/YMM (bit 2) state enabled by the OS. */
PF_API int pf_os_supports_avx(void) {
    return pf_cpuid_flag(1, 0, 2, 27) && (pf_xgetbv(0) & 0x06u) == 0x06u;
}

/* Additionally opmask (5), ZMM_Hi256 (6) and Hi16_ZMM (7) state. */
PF_API int pf_os_supports_avx512(void) {
    return pf_cpuid_flag(1, 0, 2, 27) && (pf_xgetbv(0) & 0xE6u) == 0xE6u;
}

/* ------------------------------------------------------------------------ */
/* Runtime feature flags (CPUID)                                            */
/* ------------------------------------------------------------------------ */

/* clang-format off */

#define PF_HAS_SSE3         (pf_cpuid_flag(1, 0, 2, 0))
#define PF_HAS_PCLMULQDQ    (pf_cpuid_flag(1, 0, 2, 1))
#define PF_HAS_DTES64       (pf_cpuid_flag(1, 0, 2, 2))
#define PF_HAS_MONITOR      (pf_cpuid_flag(1, 0, 2, 3))
#define PF_HAS_DS_CPL       (pf_cpuid_flag(1, 0, 2, 4))
#define PF_HAS_VMX          (pf_cpuid_flag(1, 0, 2, 5))
#define PF_HAS_SMX          (pf_cpuid_flag(1, 0, 2, 6))
#define PF_HAS_EST          (pf_cpuid_flag(1, 0, 2, 7))
#define PF_HAS_TM2          (pf_cpuid_flag(1, 0, 2, 8))
#define PF_HAS_SSSE3        (pf_cpuid_flag(1, 0, 2, 9))
#define PF_HAS_CNXT_ID      (pf_cpuid_flag(1, 0, 2, 10))
#define PF_HAS_SDBG         (pf_cpuid_flag(1, 0, 2, 11))
#define PF_HAS_FMA          (pf_cpuid_flag(1, 0, 2, 12))
#define PF_HAS_CMPXCHG16B   (pf_cpuid_flag(1, 0, 2, 13))
#define PF_HAS_XTPR         (pf_cpuid_flag(1, 0, 2, 14))
#define PF_HAS_PDCM         (pf_cpuid_flag(1, 0, 2, 15))
#define PF_HAS_PCID         (pf_cpuid_flag(1, 0, 2, 17))
#define PF_HAS_DCA          (pf_cpuid_flag(1, 0, 2, 18))
#define PF_HAS_SSE4_1       (pf_cpuid_flag(1, 0, 2, 19))
#define PF_HAS_SSE4_2       (pf_cpuid_flag(1, 0, 2, 20))
#define PF_HAS_X2APIC       (pf_cpuid_flag(1, 0, 2, 21))
#define PF_HAS_MOVBE        (pf_cpuid_flag(1, 0, 2, 22))
#define PF_HAS_POPCNT       (pf_cpuid_flag(1, 0, 2, 23))
#define PF_HAS_TSC_DEADLINE (pf_cpuid_flag(1, 0, 2, 24))
#define PF_HAS_AES          (pf_cpuid_flag(1, 0, 2, 25))
#define PF_HAS_XSAVE        (pf_cpuid_flag(1, 0, 2, 26))
#define PF_HAS_OSXSAVE      (pf_cpuid_flag(1, 0, 2, 27))
#define PF_HAS_AVX          (pf_cpuid_flag(1, 0, 2, 28))
#define PF_HAS_F16C         (pf_cpuid_flag(1, 0, 2, 29))
#define PF_HAS_RDRND        (pf_cpuid_flag(1, 0, 2, 30))

#define PF_HAS_FPU    (pf_cpuid_flag(1, 0, 3, 0))
#define PF_HAS_VME    (pf_cpuid_flag(1, 0, 3, 1))
#define PF_HAS_DE     (pf_cpuid_flag(1, 0, 3, 2))
#define PF_HAS_PSE    (pf_cpuid_flag(1, 0, 3, 3))
#define PF_HAS_TSC    (pf_cpuid_flag(1, 0, 3, 4))
#define PF_HAS_MSR    (pf_cpuid_flag(1, 0, 3, 5))
#define PF_HAS_PAE    (pf_cpuid_flag(1, 0, 3, 6))
#define PF_HAS_MCE    (pf_cpuid_flag(1, 0, 3, 7))
#define PF_HAS_CX8    (pf_cpuid_flag(1, 0, 3, 8))
#define PF_HAS_APIC   (pf_cpuid_flag(1, 0, 3, 9))
#define PF_HAS_SEP    (pf_cpuid_flag(1, 0, 3, 11))
#define PF_HAS_MTRR   (pf_cpuid_flag(1, 0, 3, 12))
#define PF_HAS_PGE    (pf_cpuid_flag(1, 0, 3, 13))
#define PF_HAS_MCA    (pf_cpuid_flag(1, 0, 3, 14))
#define PF_HAS_CMOV   (pf_cpuid_flag(1, 0, 3, 15))
#define PF_HAS_PAT    (pf_cpuid_flag(1, 0, 3, 16))
#define PF_HAS_PSE_36 (pf_cpuid_flag(1, 0, 3, 17))
#define PF_HAS_PSN    (pf_cpuid_flag(1, 0, 3, 18))
#define PF_HAS_CLFSH  (pf_cpuid_flag(1, 0, 3, 19))
#define PF_HAS_DS     (pf_cpuid_flag(1, 0, 3, 21))
#define PF_HAS_ACPI   (pf_cpuid_flag(1, 0, 3, 22))
#define PF_HAS_MMX    (pf_cpuid_flag(1, 0, 3, 23))
#define PF_HAS_FXSR   (pf_cpuid_flag(1, 0, 3, 24))
#define PF_HAS_SSE    (pf_cpuid_flag(1, 0, 3, 25))
#define PF_HAS_SSE2   (pf_cpuid_flag(1, 0, 3, 26))
#define PF_HAS_SS     (pf_cpuid_flag(1, 0, 3, 27))
#define PF_HAS_HTT    (pf_cpuid_flag(1, 0, 3, 28))
#define PF_HAS_TM     (pf_cpuid_flag(1, 0, 3, 29))
#define PF_HAS_IA64   (pf_cpuid_flag(1, 0, 3, 30))
#define PF_HAS_PBE    (pf_cpuid_flag(1, 0, 3, 31))

#define PF_HAS_FSGSBASE        (pf_cpuid_flag(7, 0, 1, 0))
#define PF_HAS_TSC_ADJ         (pf_cpuid_flag(7, 0, 1, 1))
#define PF_HAS_SGX             (pf_cpuid_flag(7, 0, 1, 2))
#define PF_HAS_BMI1            (pf_cpuid_flag(7, 0, 1, 3))
#define PF_HAS_HLE             (pf_cpuid_flag(7, 0, 1, 4))
#define PF_HAS_AVX2            (pf_cpuid_flag(7, 0, 1, 5))
#define PF_HAS_FDP_EXCPTN_ONLY (pf_cpuid_flag(7, 0, 1, 6))
#define PF_HAS_SMEP            (pf_cpuid_flag(7, 0, 1, 7))
#define PF_HAS_BMI2            (pf_cpuid_flag(7, 0, 1, 8))
#define PF_HAS_ERMS            (pf_cpuid_flag(7, 0, 1, 9))
#define PF_HAS_INVPCID         (pf_cpuid_flag(7, 0, 1, 10))
#define PF_HAS_RTM             (pf_cpuid_flag(7, 0, 1, 11))
#define PF_HAS_RDT_M           (pf_cpuid_flag(7, 0, 1, 12))
#define PF_HAS_DEPRECATE_FPU   (pf_cpuid_flag(7, 0, 1, 13))
#define PF_HAS_MPX             (pf_cpuid_flag(7, 0, 1, 14))
#define PF_HAS_RDT_A           (pf_cpuid_flag(7, 0, 1, 15))
#define PF_HAS_AVX512F         (pf_cpuid_flag(7, 0, 1, 16))
#define PF_HAS_AVX512DQ        (pf_cpuid_flag(7, 0, 1, 17))
#define PF_HAS_RDSEED          (pf_cpuid_flag(7, 0, 1, 18))
#define PF_HAS_ADX             (pf_cpuid_flag(7, 0, 1, 19))
#define PF_HAS_SMAP            (pf_cpuid_flag(7, 0, 1, 20))
#define PF_HAS_AVX512IFMA      (pf_cpuid_flag(7, 0, 1, 21))
#define PF_HAS_PCOMMIT         (pf_cpuid_flag(7, 0, 1, 22))
#define PF_HAS_CLFLUSHOPT      (pf_cpuid_flag(7, 0, 1, 23))
#define PF_HAS_CLWB            (pf_cpuid_flag(7, 0, 1, 24))
#define PF_HAS_INTEL_PT        (pf_cpuid_flag(7, 0, 1, 25))
#define PF_HAS_AVX512PF        (pf_cpuid_flag(7, 0, 1, 26))
#define PF_HAS_AVX512ER        (pf_cpuid_flag(7, 0, 1, 27))
#define PF_HAS_AVX512CD        (pf_cpuid_flag(7, 0, 1, 28))
#define PF_HAS_SHA             (pf_cpuid_flag(7, 0, 1, 29))
#define PF_HAS_AVX512BW        (pf_cpuid_flag(7, 0, 1, 30))
#define PF_HAS_AVX512VL        (pf_cpuid_flag(7, 0, 1, 31))

#define PF_HAS_PREFETCHWT1     (pf_cpuid_flag(7, 0, 2, 0))
#define PF_HAS_AVX512VBMI      (pf_cpuid_flag(7, 0, 2, 1))
#define PF_HAS_UMIP            (pf_cpuid_flag(7, 0, 2, 2))
#define PF_HAS_PKU             (pf_cpuid_flag(7, 0, 2, 3))
#define PF_HAS_OSPKE           (pf_cpuid_flag(7, 0, 2, 4))
#define PF_HAS_WAITPKG         (pf_cpuid_flag(7, 0, 2, 5))
#define PF_HAS_AVX512_VBMI2    (pf_cpuid_flag(7, 0, 2, 6))
#define PF_HAS_CET_SS          (pf_cpuid_flag(7, 0, 2, 7))
#define PF_HAS_GFNI            (pf_cpuid_flag(7, 0, 2, 8))
#define PF_HAS_VAES            (pf_cpuid_flag(7, 0, 2, 9))
#define PF_HAS_VPCLMULQDQ      (pf_cpuid_flag(7, 0, 2, 10))
#define PF_HAS_AVX512_VNNI     (pf_cpuid_flag(7, 0, 2, 11))
#define PF_HAS_AVX512_BITALG   (pf_cpuid_flag(7, 0, 2, 12))
#define PF_HAS_TME_EN          (pf_cpuid_flag(7, 0, 2, 13))
#define PF_HAS_AVX512VPOPCNTDQ (pf_cpuid_flag(7, 0, 2, 14))
#define PF_HAS_LA57            (pf_cpuid_flag(7, 0, 2, 16))
#define PF_HAS_RDPID           (pf_cpuid_flag(7, 0, 2, 22))
#define PF_HAS_KL              (pf_cpuid_flag(7, 0, 2, 23))
#define PF_HAS_BUS_LOCK_DETECT (pf_cpuid_flag(7, 0, 2, 24))
#define PF_HAS_CLDEMOTE        (pf_cpuid_flag(7, 0, 2, 25))
#define PF_HAS_MOVDIRI         (pf_cpuid_flag(7, 0, 2, 27))
#define PF_HAS_MOVDIR64B       (pf_cpuid_flag(7, 0, 2, 28))
#define PF_HAS_ENQCMD          (pf_cpuid_flag(7, 0, 2, 29))
#define PF_HAS_SGX_LC          (pf_cpuid_flag(7, 0, 2, 30))
#define PF_HAS_PKS             (pf_cpuid_flag(7, 0, 2, 31))

#define PF_HAS_SGX_KEYS               (pf_cpuid_flag(7, 0, 3, 1))
#define PF_HAS_AVX512_4VNNIW          (pf_cpuid_flag(7, 0, 3, 2))
#define PF_HAS_AVX512_4FMAPS          (pf_cpuid_flag(7, 0, 3, 3))
#define PF_HAS_FS_REP_MOV             (pf_cpuid_flag(7, 0, 3, 4))
#define PF_HAS_UINTR                  (pf_cpuid_flag(7, 0, 3, 5))
#define PF_HAS_AVX512_VP2INTERSECT    (pf_cpuid_flag(7, 0, 3, 8))
#define PF_HAS_SRBDS_CTRL             (pf_cpuid_flag(7, 0, 3, 9))
#define PF_HAS_MD_CLEAR               (pf_cpuid_flag(7, 0, 3, 10))
#define PF_HAS_RTM_ALWAYS_ABORT       (pf_cpuid_flag(7, 0, 3, 11))
#define PF_HAS_TSX_FORCE_ABORT        (pf_cpuid_flag(7, 0, 3, 13))
#define PF_HAS_SERIALIZE              (pf_cpuid_flag(7, 0, 3, 14))
#define PF_HAS_HYBRID                 (pf_cpuid_flag(7, 0, 3, 15))
#define PF_HAS_TSXLDTRK               (pf_cpuid_flag(7, 0, 3, 16))
#define PF_HAS_PCONFIG                (pf_cpuid_flag(7, 0, 3, 18))
#define PF_HAS_ARCH_LBR               (pf_cpuid_flag(7, 0, 3, 19))
#define PF_HAS_CET_IBT                (pf_cpuid_flag(7, 0, 3, 20))
#define PF_HAS_AMX_BF16               (pf_cpuid_flag(7, 0, 3, 22))
#define PF_HAS_AVX512_FP16            (pf_cpuid_flag(7, 0, 3, 23))
#define PF_HAS_AMX_TILE               (pf_cpuid_flag(7, 0, 3, 24))
#define PF_HAS_AMX_INT8               (pf_cpuid_flag(7, 0, 3, 25))
#define PF_HAS_IBRS_IBPB              (pf_cpuid_flag(7, 0, 3, 26))
#define PF_HAS_STIBP                  (pf_cpuid_flag(7, 0, 3, 27))
#define PF_HAS_L1D_FLUSH              (pf_cpuid_flag(7, 0, 3, 28))
#define PF_HAS_IA32_ARCH_CAPABILITIES (pf_cpuid_flag(7, 0, 3, 29))
#define PF_HAS_IA32_CORE_CAPABILITIES (pf_cpuid_flag(7, 0, 3, 30))
#define PF_HAS_SSBD                   (pf_cpuid_flag(7, 0, 3, 31))

/* CPU *and* OS support: safe to execute AVX / AVX2 / AVX-512F code. */
#define PF_CAN_USE_AVX     (PF_HAS_AVX && pf_os_supports_avx())
#define PF_CAN_USE_AVX2    (PF_HAS_AVX2 && pf_os_supports_avx())
#define PF_CAN_USE_AVX512F (PF_HAS_AVX512F && pf_os_supports_avx512())

/* ------------------------------------------------------------------------ */
/* Compile-time feature support (what the compiler may emit / use)           */
/* ------------------------------------------------------------------------ */

#ifdef PF_CPU_X86

/* Architectural baseline */
#if defined(PF_CPU_X86_64)
#define PF_SUPPORTS_MMX
#define PF_SUPPORTS_SSE
#define PF_SUPPORTS_SSE2
#define PF_SUPPORTS_FXSR
#define PF_SUPPORTS_CMOV
#define PF_SUPPORTS_CX8
#endif

#if defined(__i686__) || defined(__pentiumpro__)
#define PF_SUPPORTS_CMOV
#endif
#if defined(__i586__) || defined(__i686__) || defined(__pentium__)
#define PF_SUPPORTS_CX8
#endif

/* MSVC: SSE/SSE2 on 32-bit are selected via /arch and _M_IX86_FP */
#if defined(_M_IX86_FP)
    #if _M_IX86_FP >= 1
    #define PF_SUPPORTS_SSE
    #endif
    #if _M_IX86_FP >= 2
    #define PF_SUPPORTS_SSE2
    #endif
#endif

/* GCC / Clang / ICC / clang-cl (and MSVC for AVX / AVX-512 families) */
#ifdef __MMX__
#define PF_SUPPORTS_MMX
#endif
#ifdef __FXSR__
#define PF_SUPPORTS_FXSR
#endif
#ifdef __SSE__
#define PF_SUPPORTS_SSE
#endif
#ifdef __SSE2__
#define PF_SUPPORTS_SSE2
#endif
#ifdef __SSE3__
#define PF_SUPPORTS_SSE3
#endif
#ifdef __SSSE3__
#define PF_SUPPORTS_SSSE3
#endif
#ifdef __SSE4_1__
#define PF_SUPPORTS_SSE4_1
#endif
#ifdef __SSE4_2__
#define PF_SUPPORTS_SSE4_2
#endif
#ifdef __AVX__
#define PF_SUPPORTS_AVX
#endif
#ifdef __AVX2__
#define PF_SUPPORTS_AVX2
#endif
#ifdef __FMA__
#define PF_SUPPORTS_FMA
#endif
#ifdef __F16C__
#define PF_SUPPORTS_F16C
#endif
#ifdef __PCLMUL__
#define PF_SUPPORTS_PCLMULQDQ
#endif
#ifdef __AES__
#define PF_SUPPORTS_AES
#endif
#ifdef __SHA__
#define PF_SUPPORTS_SHA
#endif
#ifdef __POPCNT__
#define PF_SUPPORTS_POPCNT
#endif
#ifdef __MOVBE__
#define PF_SUPPORTS_MOVBE
#endif
#ifdef __XSAVE__
#define PF_SUPPORTS_XSAVE
#endif
#ifdef __RDRND__
#define PF_SUPPORTS_RDRND
#endif
#ifdef __RDSEED__
#define PF_SUPPORTS_RDSEED
#endif
#ifdef __ADX__
#define PF_SUPPORTS_ADX
#endif
#ifdef __BMI__
#define PF_SUPPORTS_BMI1
#endif
#ifdef __BMI2__
#define PF_SUPPORTS_BMI2
#endif
#ifdef __FSGSBASE__
#define PF_SUPPORTS_FSGSBASE
#endif
#ifdef __HLE__
#define PF_SUPPORTS_HLE
#endif
#ifdef __RTM__
#define PF_SUPPORTS_RTM
#endif
#ifdef __SGX__
#define PF_SUPPORTS_SGX
#endif
#ifdef __CLFLUSHOPT__
#define PF_SUPPORTS_CLFLUSHOPT
#endif
#ifdef __CLWB__
#define PF_SUPPORTS_CLWB
#endif
#if defined(__CX16__) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#define PF_SUPPORTS_CMPXCHG16B
#endif
#ifdef __PREFETCHWT1__
#define PF_SUPPORTS_PREFETCHWT1
#endif

/* AVX-512 family */
#ifdef __AVX512F__
#define PF_SUPPORTS_AVX512F
#endif
#ifdef __AVX512DQ__
#define PF_SUPPORTS_AVX512DQ
#endif
#ifdef __AVX512BW__
#define PF_SUPPORTS_AVX512BW
#endif
#ifdef __AVX512VL__
#define PF_SUPPORTS_AVX512VL
#endif
#ifdef __AVX512CD__
#define PF_SUPPORTS_AVX512CD
#endif
#ifdef __AVX512ER__
#define PF_SUPPORTS_AVX512ER
#endif
#ifdef __AVX512PF__
#define PF_SUPPORTS_AVX512PF
#endif
#ifdef __AVX512IFMA__
#define PF_SUPPORTS_AVX512IFMA
#endif
#ifdef __AVX512VBMI__
#define PF_SUPPORTS_AVX512VBMI
#endif
#ifdef __AVX512VBMI2__
#define PF_SUPPORTS_AVX512_VBMI2
#endif
#ifdef __AVX512VNNI__
#define PF_SUPPORTS_AVX512_VNNI
#endif
#ifdef __AVX512BITALG__
#define PF_SUPPORTS_AVX512_BITALG
#endif
#ifdef __AVX512VPOPCNTDQ__
#define PF_SUPPORTS_AVX512VPOPCNTDQ
#endif
#ifdef __AVX5124VNNIW__
#define PF_SUPPORTS_AVX512_4VNNIW
#endif
#ifdef __AVX5124FMAPS__
#define PF_SUPPORTS_AVX512_4FMAPS
#endif
#ifdef __AVX512VP2INTERSECT__
#define PF_SUPPORTS_AVX512_VP2INTERSECT
#endif
#ifdef __AVX512FP16__
#define PF_SUPPORTS_AVX512_FP16
#endif

/* Newer extensions */
#ifdef __GFNI__
#define PF_SUPPORTS_GFNI
#endif
#ifdef __VAES__
#define PF_SUPPORTS_VAES
#endif
#ifdef __VPCLMULQDQ__
#define PF_SUPPORTS_VPCLMULQDQ
#endif
#ifdef __PKU__
#define PF_SUPPORTS_PKU
#endif
#ifdef __WAITPKG__
#define PF_SUPPORTS_WAITPKG
#endif
#ifdef __SHSTK__
#define PF_SUPPORTS_CET_SS
#endif
#ifdef __RDPID__
#define PF_SUPPORTS_RDPID
#endif
#ifdef __KL__
#define PF_SUPPORTS_KL
#endif
#ifdef __CLDEMOTE__
#define PF_SUPPORTS_CLDEMOTE
#endif
#ifdef __MOVDIRI__
#define PF_SUPPORTS_MOVDIRI
#endif
#ifdef __MOVDIR64B__
#define PF_SUPPORTS_MOVDIR64B
#endif
#ifdef __ENQCMD__
#define PF_SUPPORTS_ENQCMD
#endif
#ifdef __UINTR__
#define PF_SUPPORTS_UINTR
#endif
#ifdef __SERIALIZE__
#define PF_SUPPORTS_SERIALIZE
#endif
#ifdef __TSXLDTRK__
#define PF_SUPPORTS_TSXLDTRK
#endif
#ifdef __PCONFIG__
#define PF_SUPPORTS_PCONFIG
#endif
#ifdef __AMX_TILE__
#define PF_SUPPORTS_AMX_TILE
#endif
#ifdef __AMX_INT8__
#define PF_SUPPORTS_AMX_INT8
#endif
#ifdef __AMX_BF16__
#define PF_SUPPORTS_AMX_BF16
#endif

/* Genuine MSVC has no per-feature macros: only __AVX__, __AVX2__ and
   __AVX512*__ exist. Derive what the /arch level implies. Redefining the
   same empty macro is harmless. */
#if defined(_MSC_VER) && !defined(__clang__)
    #ifdef __AVX__
    #define PF_SUPPORTS_SSE3
    #define PF_SUPPORTS_SSSE3
    #define PF_SUPPORTS_SSE4_1
    #define PF_SUPPORTS_SSE4_2
    #endif
    #ifdef __AVX2__ /* Haswell baseline */
    #define PF_SUPPORTS_FMA
    #define PF_SUPPORTS_BMI1
    #define PF_SUPPORTS_BMI2
    #define PF_SUPPORTS_F16C
    #define PF_SUPPORTS_POPCNT
    #define PF_SUPPORTS_MOVBE
    #endif
#endif

#endif /* PF_CPU_X86 */

/* clang-format on */

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_CPUINFO */
