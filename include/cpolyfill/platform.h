/** cpolyfill - Filling the gaps between C standards and compilers

    Platform, compiler, architecture and language standard detection.

    Every macro below is always defined, to either 0 or 1 (or a number),
    so they can be combined in `#if` expressions and stay clean under
    `-Wundef`:

        #if PF_OS_LINUX && PF_ARCH_ARM64
        #if PF_CC_GCC && PF_GCC_PREREQ(11, 0)

    Operating system:
        PF_OS_WINDOWS       Windows (native, MSVC or MinGW). Not Cygwin.
        PF_OS_CYGWIN        Cygwin (a POSIX system on Windows).
        PF_OS_LINUX         Linux, including Android.
        PF_OS_ANDROID       Android.
        PF_OS_APPLE         Any Apple OS.
        PF_OS_MACOS         macOS.
        PF_OS_IOS           iOS, iPadOS, tvOS, watchOS, visionOS.
        PF_OS_FREEBSD       FreeBSD.
        PF_OS_NETBSD        NetBSD.
        PF_OS_OPENBSD       OpenBSD.
        PF_OS_DRAGONFLY     DragonFly BSD.
        PF_OS_BSD           Any of the BSDs above (not Apple).
        PF_OS_SOLARIS       Solaris and illumos.
        PF_OS_HAIKU         Haiku.
        PF_OS_EMSCRIPTEN    Emscripten.
        PF_OS_WASI          WebAssembly System Interface.
        PF_OS_POSIX         A POSIX-like system (everything but Windows and
                            bare WASI).

    Compiler:
        PF_CC_GCC           GCC (not Clang or other GCC-compatible compilers).
        PF_CC_CLANG         Clang, including clang-cl and Apple Clang.
        PF_CC_APPLE_CLANG   Apple Clang (its version numbers differ from
                            upstream Clang).
        PF_CC_CLANG_CL      clang-cl (Clang with the MSVC driver).
        PF_CC_MSVC          Microsoft Visual C++ (not clang-cl).
        PF_CC_MINGW         MinGW-w64 toolchain (with GCC or Clang).
        PF_CC_GNU           A compiler with GNU C extensions (GCC, Clang).

        PF_GCC_VERSION      major * 10000 + minor * 100 + patch, or 0.
        PF_CLANG_VERSION    major * 10000 + minor * 100 + patch, or 0.
        PF_MSVC_VERSION     the value of _MSC_VER (e.g. 1938), or 0.
        PF_GCC_PREREQ(major, minor)    GCC of at least that version.
        PF_CLANG_PREREQ(major, minor)  Clang of at least that version.
        PF_MSVC_PREREQ(version)        _MSC_VER of at least `version`.

    Architecture:
        PF_ARCH_X86, PF_ARCH_X86_64, PF_ARCH_ARM, PF_ARCH_ARM64,
        PF_ARCH_RISCV, PF_ARCH_RISCV64, PF_ARCH_PPC, PF_ARCH_PPC64,
        PF_ARCH_MIPS, PF_ARCH_LOONGARCH64, PF_ARCH_S390X, PF_ARCH_WASM
        PF_ARCH_BITS        Pointer width in bits (32 or 64).

        ARM64EC code (which MSVC also marks as x64) counts as PF_ARCH_ARM64.

    Language standard:
        PF_STD_VERSION      The value of __STDC_VERSION__, or 0.
        PF_STD_C99, PF_STD_C11, PF_STD_C17, PF_STD_C23
                            At least that C standard. Cumulative, so C23
                            also sets PF_STD_C11. MSVC without /std:c11
                            counts as C99, which it mostly implements.
                            All are 0 when compiling as C++.
        PF_CPP              The C++ standard (`__cplusplus`, or
                            `_MSVC_LANG` on MSVC), or 0 when compiling C.

    Feature detection, usable in `#if` on every compiler (0 if unknown):
        PF_HAS_BUILTIN(x)       __has_builtin
        PF_HAS_ATTRIBUTE(x)     __has_attribute (GNU `__attribute__`)
        PF_HAS_C_ATTRIBUTE(x)   __has_c_attribute (`[[x]]`), or
                                __has_cpp_attribute in C++
        PF_HAS_FEATURE(x)       __has_feature
        PF_HAS_EXTENSION(x)     __has_extension

    For `__has_include`, test `defined(__has_include)` first. It cannot be
    wrapped in a portable macro.

    Linkage:
        PF_API              Marks a function exported by the library.
        PF_INLINE           `static inline` for header-only functions.
        PF_BEGIN_DECLS      `extern "C" {` in C++, nothing in C.
        PF_END_DECLS        `}` in C++, nothing in C.

    Standard names:
        Headers that polyfill standard or POSIX functions always provide
        the prefixed `pf_*` versions. The standard names (`mkdir`, `dlopen`,
        ...) are mapped to them only on request, by defining before the
        include either
            PF_STD_NAMES            for every header, or
            PF_STD_NAMES_<MODULE>   for one header (e.g. PF_STD_NAMES_THREADS),
        and `PF_NO_STD_NAMES_<MODULE>` excludes one header from
        PF_STD_NAMES. Names are only mapped on platforms where the standard
        function is missing or behaves differently.

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

#ifndef CPOLYFILL_PLATFORM_H
#define CPOLYFILL_PLATFORM_H

#include <cpolyfill/config.h>

/* ---- Operating system ------------------------------------------------- */

#if defined(_WIN32) && !defined(__CYGWIN__)
    #define PF_OS_WINDOWS 1
#else
    #define PF_OS_WINDOWS 0
#endif

#ifdef __CYGWIN__
    #define PF_OS_CYGWIN 1
#else
    #define PF_OS_CYGWIN 0
#endif

#ifdef __linux__
    #define PF_OS_LINUX 1
#else
    #define PF_OS_LINUX 0
#endif

#ifdef __ANDROID__
    #define PF_OS_ANDROID 1
#else
    #define PF_OS_ANDROID 0
#endif

#if defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #define PF_OS_APPLE 1
    #if defined(TARGET_OS_OSX) && TARGET_OS_OSX
        #define PF_OS_MACOS 1
        #define PF_OS_IOS 0
    #elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        #define PF_OS_MACOS 0
        #define PF_OS_IOS 1
    #else
        #define PF_OS_MACOS 1
        #define PF_OS_IOS 0
    #endif
#else
    #define PF_OS_APPLE 0
    #define PF_OS_MACOS 0
    #define PF_OS_IOS 0
#endif

#ifdef __FreeBSD__
    #define PF_OS_FREEBSD 1
#else
    #define PF_OS_FREEBSD 0
#endif

#ifdef __NetBSD__
    #define PF_OS_NETBSD 1
#else
    #define PF_OS_NETBSD 0
#endif

#ifdef __OpenBSD__
    #define PF_OS_OPENBSD 1
#else
    #define PF_OS_OPENBSD 0
#endif

#ifdef __DragonFly__
    #define PF_OS_DRAGONFLY 1
#else
    #define PF_OS_DRAGONFLY 0
#endif

#if PF_OS_FREEBSD || PF_OS_NETBSD || PF_OS_OPENBSD || PF_OS_DRAGONFLY
    #define PF_OS_BSD 1
#else
    #define PF_OS_BSD 0
#endif

#if defined(__sun) && defined(__SVR4)
    #define PF_OS_SOLARIS 1
#else
    #define PF_OS_SOLARIS 0
#endif

#ifdef __HAIKU__
    #define PF_OS_HAIKU 1
#else
    #define PF_OS_HAIKU 0
#endif

#ifdef __EMSCRIPTEN__
    #define PF_OS_EMSCRIPTEN 1
#else
    #define PF_OS_EMSCRIPTEN 0
#endif

#ifdef __wasi__
    #define PF_OS_WASI 1
#else
    #define PF_OS_WASI 0
#endif

#if !PF_OS_WINDOWS && !PF_OS_WASI                                          \
    && (defined(__unix__) || defined(__unix) || PF_OS_APPLE || PF_OS_CYGWIN \
        || PF_OS_HAIKU || PF_OS_EMSCRIPTEN)
    #define PF_OS_POSIX 1
#else
    #define PF_OS_POSIX 0
#endif

/* ---- Compiler --------------------------------------------------------- */

#ifdef __clang__
    #define PF_CC_CLANG 1
    #define PF_CLANG_VERSION                                               \
        (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#else
    #define PF_CC_CLANG 0
    #define PF_CLANG_VERSION 0
#endif

#if defined(__clang__) && defined(__apple_build_version__)
    #define PF_CC_APPLE_CLANG 1
#else
    #define PF_CC_APPLE_CLANG 0
#endif

#if defined(__clang__) && defined(_MSC_VER)
    #define PF_CC_CLANG_CL 1
#else
    #define PF_CC_CLANG_CL 0
#endif

#if defined(_MSC_VER) && !defined(__clang__)
    #define PF_CC_MSVC 1
#else
    #define PF_CC_MSVC 0
#endif

#ifdef _MSC_VER
    #define PF_MSVC_VERSION _MSC_VER
#else
    #define PF_MSVC_VERSION 0
#endif

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) \
    && !defined(__INTEL_LLVM_COMPILER)
    #define PF_CC_GCC 1
    #define PF_GCC_VERSION                                         \
        (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #define PF_CC_GCC 0
    #define PF_GCC_VERSION 0
#endif

#ifdef __GNUC__
    #define PF_CC_GNU 1
#else
    #define PF_CC_GNU 0
#endif

#ifdef __MINGW32__
    #define PF_CC_MINGW 1
#else
    #define PF_CC_MINGW 0
#endif

#define PF_GCC_PREREQ(major, minor)                                   \
    (PF_GCC_VERSION >= (major) * 10000 + (minor) * 100)
#define PF_CLANG_PREREQ(major, minor)                                 \
    (PF_CLANG_VERSION >= (major) * 10000 + (minor) * 100)
#define PF_MSVC_PREREQ(version) (PF_MSVC_VERSION >= (version))

/* ---- Architecture ----------------------------------------------------- */

#if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
    #define PF_ARCH_ARM64 1
#else
    #define PF_ARCH_ARM64 0
#endif

#if (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) \
    && !PF_ARCH_ARM64
    #define PF_ARCH_X86_64 1
#else
    #define PF_ARCH_X86_64 0
#endif

#if defined(__i386__) || defined(_M_IX86)
    #define PF_ARCH_X86 1
#else
    #define PF_ARCH_X86 0
#endif

#if (defined(__arm__) || defined(_M_ARM)) && !PF_ARCH_ARM64
    #define PF_ARCH_ARM 1
#else
    #define PF_ARCH_ARM 0
#endif

#ifdef __riscv
    #define PF_ARCH_RISCV 1
    #if defined(__riscv_xlen) && __riscv_xlen == 64
        #define PF_ARCH_RISCV64 1
    #else
        #define PF_ARCH_RISCV64 0
    #endif
#else
    #define PF_ARCH_RISCV 0
    #define PF_ARCH_RISCV64 0
#endif

#if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
    #define PF_ARCH_PPC64 1
#else
    #define PF_ARCH_PPC64 0
#endif

#if (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) \
     || defined(_M_PPC))                                          \
    && !PF_ARCH_PPC64
    #define PF_ARCH_PPC 1
#else
    #define PF_ARCH_PPC 0
#endif

#if defined(__mips__) || defined(__mips)
    #define PF_ARCH_MIPS 1
#else
    #define PF_ARCH_MIPS 0
#endif

#if defined(__loongarch__) && defined(__loongarch64)
    #define PF_ARCH_LOONGARCH64 1
#else
    #define PF_ARCH_LOONGARCH64 0
#endif

#ifdef __s390x__
    #define PF_ARCH_S390X 1
#else
    #define PF_ARCH_S390X 0
#endif

#ifdef __wasm__
    #define PF_ARCH_WASM 1
#else
    #define PF_ARCH_WASM 0
#endif

#if defined(__SIZEOF_POINTER__)
    #define PF_ARCH_BITS (__SIZEOF_POINTER__ * 8)
#elif defined(_WIN64) || defined(__LP64__) || defined(_LP64)
    #define PF_ARCH_BITS 64
#else
    #define PF_ARCH_BITS 32
#endif

/* ---- Language standard ------------------------------------------------ */

#ifdef __cplusplus
    #if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
        #define PF_CPP _MSVC_LANG
    #else
        #define PF_CPP __cplusplus
    #endif
#else
    #define PF_CPP 0
#endif

#if defined(__STDC_VERSION__) && !defined(__cplusplus)
    #define PF_STD_VERSION __STDC_VERSION__
#else
    #define PF_STD_VERSION 0
#endif

#if !defined(__cplusplus) && (PF_STD_VERSION >= 199901L || PF_CC_MSVC)
    #define PF_STD_C99 1
#else
    #define PF_STD_C99 0
#endif

#if PF_STD_VERSION >= 201112L
    #define PF_STD_C11 1
#else
    #define PF_STD_C11 0
#endif

#if PF_STD_VERSION >= 201710L
    #define PF_STD_C17 1
#else
    #define PF_STD_C17 0
#endif

#if PF_STD_VERSION >= 202311L
    #define PF_STD_C23 1
#else
    #define PF_STD_C23 0
#endif

/* ---- Feature detection ------------------------------------------------ */

#ifdef __has_builtin
    #define PF_HAS_BUILTIN(x) __has_builtin(x)
#else
    #define PF_HAS_BUILTIN(x) 0
#endif

#ifdef __has_attribute
    #define PF_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
    #define PF_HAS_ATTRIBUTE(x) 0
#endif

#if defined(__cplusplus) && defined(__has_cpp_attribute)
    #define PF_HAS_C_ATTRIBUTE(x) __has_cpp_attribute(x)
#elif !defined(__cplusplus) && defined(__has_c_attribute)
    #define PF_HAS_C_ATTRIBUTE(x) __has_c_attribute(x)
#else
    #define PF_HAS_C_ATTRIBUTE(x) 0
#endif

#ifdef __has_feature
    #define PF_HAS_FEATURE(x) __has_feature(x)
#else
    #define PF_HAS_FEATURE(x) 0
#endif

#ifdef __has_extension
    #define PF_HAS_EXTENSION(x) __has_extension(x)
#else
    #define PF_HAS_EXTENSION(x) PF_HAS_FEATURE(x)
#endif

/* ---- Linkage ---------------------------------------------------------- */

#ifndef PF_API
    #if PF_OS_WINDOWS || PF_OS_CYGWIN
        #if defined(PF__BUILDING)
            #define PF_API __declspec(dllexport)
        #elif PF_CONFIG_SHARED
            #define PF_API __declspec(dllimport)
        #else
            #define PF_API
        #endif
    #elif PF_HAS_ATTRIBUTE(visibility) || PF_GCC_PREREQ(4, 0)
        #define PF_API __attribute__((visibility("default")))
    #else
        #define PF_API
    #endif
#endif

#ifndef PF_INLINE
    #if PF_CC_MSVC && !defined(__cplusplus)
        #define PF_INLINE static __inline
    #else
        #define PF_INLINE static inline
    #endif
#endif

#ifdef __cplusplus
    #define PF_BEGIN_DECLS extern "C" {
    #define PF_END_DECLS }
#else
    #define PF_BEGIN_DECLS
    #define PF_END_DECLS
#endif

#endif /* CPOLYFILL_PLATFORM_H */
