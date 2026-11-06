/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/platform.h>

#include "check.h"

/* Every macro must be usable in #if (this file is built with -Wundef). */
#if PF_OS_WINDOWS + PF_OS_CYGWIN + PF_OS_LINUX + PF_OS_ANDROID + PF_OS_APPLE \
        + PF_OS_MACOS + PF_OS_IOS + PF_OS_FREEBSD + PF_OS_NETBSD             \
        + PF_OS_OPENBSD + PF_OS_DRAGONFLY + PF_OS_BSD + PF_OS_SOLARIS        \
        + PF_OS_HAIKU + PF_OS_EMSCRIPTEN + PF_OS_WASI + PF_OS_POSIX          \
    < 0
    #error "OS macros"
#endif
#if PF_CC_GCC + PF_CC_CLANG + PF_CC_APPLE_CLANG + PF_CC_CLANG_CL + PF_CC_MSVC \
        + PF_CC_MINGW + PF_CC_GNU + PF_GCC_VERSION + PF_CLANG_VERSION         \
        + PF_MSVC_VERSION + PF_GCC_PREREQ(1, 0) + PF_CLANG_PREREQ(1, 0)       \
        + PF_MSVC_PREREQ(1)                                                   \
    < 0
    #error "Compiler macros"
#endif
#if PF_ARCH_X86 + PF_ARCH_X86_64 + PF_ARCH_ARM + PF_ARCH_ARM64 + PF_ARCH_RISCV \
        + PF_ARCH_RISCV64 + PF_ARCH_PPC + PF_ARCH_PPC64 + PF_ARCH_MIPS         \
        + PF_ARCH_LOONGARCH64 + PF_ARCH_S390X + PF_ARCH_WASM + PF_ARCH_BITS    \
    < 0
    #error "Architecture macros"
#endif
#if PF_STD_VERSION + PF_STD_C99 + PF_STD_C11 + PF_STD_C17 + PF_STD_C23 \
        + PF_CPP                                                       \
    < 0
    #error "Standard macros"
#endif
#if PF_HAS_BUILTIN(__pf_none) || PF_HAS_ATTRIBUTE(__pf_none)       \
    || PF_HAS_C_ATTRIBUTE(__pf_none) || PF_HAS_FEATURE(__pf_none) \
    || PF_HAS_EXTENSION(__pf_none)
    #error "Detection of something that does not exist"
#endif

static int test_os(int seed, int i) {
    (void)seed, (void)i;

    CHECK(!(PF_OS_WINDOWS && PF_OS_POSIX));
    CHECK(!(PF_OS_WINDOWS && PF_OS_CYGWIN));
    CHECK(!PF_OS_ANDROID || PF_OS_LINUX);
    CHECK(!PF_OS_LINUX || PF_OS_POSIX);
    CHECK(!PF_OS_APPLE || PF_OS_POSIX);
    CHECK(PF_OS_APPLE == (PF_OS_MACOS || PF_OS_IOS));
    CHECK(!(PF_OS_MACOS && PF_OS_IOS));
    CHECK(PF_OS_BSD
          == (PF_OS_FREEBSD || PF_OS_NETBSD || PF_OS_OPENBSD
              || PF_OS_DRAGONFLY));
    CHECK(!(PF_OS_BSD && PF_OS_APPLE));

#ifdef _WIN32
    CHECK(PF_OS_WINDOWS || PF_OS_CYGWIN);
#endif
#ifdef __linux__
    CHECK(PF_OS_LINUX);
#endif
#if defined(__unix__) && !defined(__wasi__)
    CHECK(PF_OS_POSIX);
#endif
    return 0;
}

static int test_compiler(int seed, int i) {
    (void)seed, (void)i;

    CHECK(PF_CC_GCC + PF_CC_CLANG + PF_CC_MSVC <= 1);
    CHECK(!PF_CC_CLANG_CL || PF_CC_CLANG);
    CHECK(!PF_CC_APPLE_CLANG || PF_CC_CLANG);
    CHECK(!PF_CC_GCC || PF_CC_GNU);

    CHECK((PF_GCC_VERSION != 0) == PF_CC_GCC);
    CHECK((PF_CLANG_VERSION != 0) == PF_CC_CLANG);
    CHECK(PF_GCC_PREREQ(1, 0) == PF_CC_GCC);
    CHECK(PF_CLANG_PREREQ(1, 0) == PF_CC_CLANG);
    CHECK(!PF_GCC_PREREQ(999, 0));
    CHECK(!PF_CLANG_PREREQ(999, 0));
    CHECK(!PF_CC_MSVC || PF_MSVC_PREREQ(1900));

#if defined(__GNUC__) && !defined(__clang__)
    CHECK(PF_GCC_PREREQ(__GNUC__, __GNUC_MINOR__));
    CHECK(!PF_GCC_PREREQ(__GNUC__, __GNUC_MINOR__ + 1));
#endif
    return 0;
}

static int test_arch(int seed, int i) {
    int count = PF_ARCH_X86 + PF_ARCH_X86_64 + PF_ARCH_ARM + PF_ARCH_ARM64
              + PF_ARCH_RISCV + PF_ARCH_PPC + PF_ARCH_PPC64 + PF_ARCH_MIPS
              + PF_ARCH_LOONGARCH64 + PF_ARCH_S390X + PF_ARCH_WASM;
    (void)seed, (void)i;

    CHECK(count <= 1);
    CHECK(!PF_ARCH_RISCV64 || PF_ARCH_RISCV);
    CHECK_INT(PF_ARCH_BITS, sizeof(void *) * 8);
#if defined(__x86_64__) || defined(__aarch64__) || defined(__i386__)
    CHECK(count == 1);
#endif
    return 0;
}

static int test_standard(int seed, int i) {
    (void)seed, (void)i;

    CHECK(!PF_STD_C23 || PF_STD_C17);
    CHECK(!PF_STD_C17 || PF_STD_C11);
    CHECK(!PF_STD_C11 || PF_STD_C99);
    CHECK(PF_CPP == 0);
#ifdef __STDC_VERSION__
    CHECK_INT(PF_STD_VERSION, __STDC_VERSION__);
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    CHECK(PF_STD_C11);
#endif
    return 0;
}

static int test_detection(int seed, int i) {
    (void)seed, (void)i;

#if PF_CC_GNU && (PF_GCC_PREREQ(10, 0) || PF_CC_CLANG)
    CHECK(PF_HAS_BUILTIN(__builtin_expect));
#endif
#if PF_CC_GNU && (PF_GCC_PREREQ(5, 0) || PF_CC_CLANG)
    CHECK(PF_HAS_ATTRIBUTE(unused));
#endif
#if PF_CC_CLANG
    CHECK(PF_HAS_FEATURE(c_static_assert) || !PF_STD_C11);
#endif
    return 0;
}

pf_test_t suite_platform[] = {
    { test_os, "os" },
    { test_compiler, "compiler" },
    { test_arch, "arch" },
    { test_standard, "standard" },
    { test_detection, "detection" },
    { 0 },
};
