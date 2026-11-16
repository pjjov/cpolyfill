/** cpolyfill - Filling the gaps between C standards and compilers

    Function, variable and type attributes.

    Each macro expands to the best form the compiler understands: the
    standard `[[attr]]` syntax in C23 and C++, `__attribute__` or
    `__declspec` before that, and nothing where the attribute is not
    supported (the `PF_HAS_*` macros below say when that loses meaning).

    Placement: write the macros at the very start of a declaration, before
    `PF_API`, storage classes (`static`, `extern`) and the type. GCC rejects
    a `[[attr]]` after `__attribute__` or `PF_API` in C23. The exceptions
    are noted below.

        PF_NODISCARD("check the error") PF_API int pf_open(const char *path);
        PF_NORETURN static void die(const char *msg);
        PF_PRINTF(1, 2) PF_COLD void log_error(const char *fmt, ...);

    Standard attributes:
        PF_DEPRECATED(reason)   Using the entity warns with `reason`.
        PF_NODISCARD(reason)    Ignoring the result warns. With GCC before
                                C23, a cast to void does not silence it.
        PF_NORETURN             The function never returns.
        PF_UNUSED               The entity may be unused (no warning).
                                Also usable before a parameter.
        PF_FALLTHROUGH          A statement: `PF_FALLTHROUGH;` before the
                                next `case` label.
        PF_REPRODUCIBLE         C23 `[[reproducible]]` and `[[unsequenced]]`.
        PF_UNSEQUENCED          They apply to function types, so they go
                                after the parameter list:
                                    int sq(int x) PF_UNSEQUENCED;
                                Nothing before C23.

    Compiler attributes (GCC, Clang, partly MSVC):
        PF_PRINTF(fmt, args)    printf-like format checking. `fmt` is the
                                1-based index of the format parameter, `args`
                                of the first variadic one (0 for a va_list).
        PF_NONNULL(...)         Pointer parameters that must not be NULL, by
                                1-based index; all of them when empty:
                                `PF_NONNULL()`. The compiler may delete NULL
                                checks of these parameters in the function.
        PF_RETURNS_NONNULL      The returned pointer is never NULL.
        PF_PURE                 No side effects; reads global memory.
        PF_CONST                No side effects; reads only its arguments.
        PF_MALLOC_LIKE          Returns new memory that aliases nothing.
        PF_HOT, PF_COLD         Often / rarely called.
        PF_ALWAYS_INLINE        Inline even without optimization. Use it
                                together with `PF_INLINE` or `static inline`.
                                Nothing on MSVC (untested combination with
                                `__inline`).
        PF_NOINLINE             Never inline.
        PF_WEAK                 A weak symbol definition. Only meaningful
                                when PF_HAS_WEAK is 1 (not Windows).
        PF_CLEANUP(fn)          On a local variable: `fn(&var)` runs when it
                                goes out of scope. Only meaningful when
                                PF_HAS_CLEANUP is 1 (GCC and Clang).

    Packed structures (GCC, Clang and MSVC):
        PF_PACKED_BEGIN         Members of the structures declared between
        PF_PACKED_END           these are packed (no padding):
                                    PF_PACKED_BEGIN
                                    struct header { char tag; int size; };
                                    PF_PACKED_END
                                Uses `#pragma pack`, which all three support,
                                unlike the `packed` attribute.

    Constructors and destructors:
        PF_CONSTRUCTOR(name) { ... }
        PF_DESTRUCTOR(name) { ... }
                                Define `static void name(void)`, run before
                                `main` (constructor) or at exit (destructor).
                                GCC and Clang use the attributes, MSVC the
                                `.CRT$XCU` initializer section (and `atexit`
                                for destructors), C++ a static object. When
                                linking statically, an object file nothing
                                else refers to is dropped with its
                                constructors.

    Availability, always 0 or 1:
        PF_HAS_WEAK, PF_HAS_CLEANUP

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_ATTRIBUTE_H
#define CPOLYFILL_ATTRIBUTE_H

#include <cpolyfill/platform.h>

/* `PF__GNU(attr)` is a GNU attribute, in the `[[gnu::attr]]` form where
   the standard syntax is available, so it can be mixed with standard
   attributes in any order. clang-cl understands it but is not PF_CC_GNU. */
#if PF_CC_GNU || PF_CC_CLANG
#define PF__HAS_GNU_ATTRIBUTES 1
#if PF_STD_C23 || PF_CPP >= 201103L
#define PF__GNU(...) [[gnu::__VA_ARGS__]]
#else
#define PF__GNU(...) __attribute__((__VA_ARGS__))
#endif
#else
#define PF__HAS_GNU_ATTRIBUTES 0
#endif

#define PF__GNU_HAS(x) (PF__HAS_GNU_ATTRIBUTES && PF_HAS_ATTRIBUTE(x))

/* ---- Standard attributes ---------------------------------------------- */

#if (PF_STD_C23 && PF_HAS_C_ATTRIBUTE(deprecated)) || PF_CPP >= 201402L
#define PF_DEPRECATED(reason) [[deprecated(reason)]]
#elif PF__GNU_HAS(deprecated)
#define PF_DEPRECATED(reason) PF__GNU(deprecated(reason))
#elif PF_CC_MSVC
#define PF_DEPRECATED(reason) __declspec(deprecated(reason))
#else
#define PF_DEPRECATED(reason)
#endif

#if (PF_STD_C23 && PF_HAS_C_ATTRIBUTE(nodiscard)) || PF_CPP >= 202002L
#define PF_NODISCARD(reason) [[nodiscard(reason)]]
#elif PF_CPP >= 201703L
#define PF_NODISCARD(reason) [[nodiscard]]
#elif PF__GNU_HAS(warn_unused_result)
#define PF_NODISCARD(reason) PF__GNU(warn_unused_result)
#else
#define PF_NODISCARD(reason)
#endif

#if (PF_STD_C23 && PF_HAS_C_ATTRIBUTE(noreturn)) || PF_CPP >= 201103L
#define PF_NORETURN [[noreturn]]
#elif PF__HAS_GNU_ATTRIBUTES
    /* Not _Noreturn: GCC warns when `static` follows a function specifier,
       and the attribute may also come first. */
#define PF_NORETURN __attribute__((__noreturn__))
#elif PF_STD_C11
#define PF_NORETURN _Noreturn
#elif PF_CC_MSVC
#define PF_NORETURN __declspec(noreturn)
#else
#define PF_NORETURN
#endif

#if (PF_STD_C23 && PF_HAS_C_ATTRIBUTE(maybe_unused)) || PF_CPP >= 201703L
#define PF_UNUSED [[maybe_unused]]
#elif PF__GNU_HAS(unused) || PF_CC_GNU
#define PF_UNUSED __attribute__((__unused__))
#else
#define PF_UNUSED
#endif

#if (PF_STD_C23 && PF_HAS_C_ATTRIBUTE(fallthrough)) || PF_CPP >= 201703L
#define PF_FALLTHROUGH [[fallthrough]]
#elif PF__GNU_HAS(fallthrough)
#define PF_FALLTHROUGH PF__GNU(fallthrough)
#else
#define PF_FALLTHROUGH ((void)0)
#endif

#if PF_STD_C23 && PF_HAS_C_ATTRIBUTE(reproducible)
#define PF_REPRODUCIBLE [[reproducible]]
#else
#define PF_REPRODUCIBLE
#endif

#if PF_STD_C23 && PF_HAS_C_ATTRIBUTE(unsequenced)
#define PF_UNSEQUENCED [[unsequenced]]
#else
#define PF_UNSEQUENCED
#endif

/* ---- Compiler attributes ---------------------------------------------- */

#if PF__GNU_HAS(format)
#if PF_CC_MINGW
/* gnu_printf or ms_printf, depending on __USE_MINGW_ANSI_STDIO. */
#include <_mingw.h>
#endif
#ifdef __MINGW_PRINTF_FORMAT
#define PF_PRINTF(fmt, args) PF__GNU(format(__MINGW_PRINTF_FORMAT, fmt, args))
#else
#define PF_PRINTF(fmt, args) PF__GNU(format(printf, fmt, args))
#endif
#else
#define PF_PRINTF(fmt, args)
#endif

#if PF__GNU_HAS(nonnull)
/* The standard syntax rejects empty parentheses, so `[[gnu::nonnull]]`
   needs __VA_OPT__. Without it, C++ accepts `__attribute__` before a
   standard attribute (C23 does not, but has __VA_OPT__). */
#if PF_STD_C23 || PF_CPP >= 202002L
#define PF_NONNULL(...) [[gnu::nonnull __VA_OPT__((__VA_ARGS__))]]
#else
#define PF_NONNULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#endif
#else
#define PF_NONNULL(...)
#endif

#if PF__GNU_HAS(returns_nonnull)
#define PF_RETURNS_NONNULL PF__GNU(returns_nonnull)
#else
#define PF_RETURNS_NONNULL
#endif

#if PF__GNU_HAS(pure)
#define PF_PURE PF__GNU(pure)
#else
#define PF_PURE
#endif

#if PF__GNU_HAS(const)
#define PF_CONST PF__GNU(const)
#else
#define PF_CONST
#endif

#if PF__GNU_HAS(malloc)
#define PF_MALLOC_LIKE PF__GNU(malloc)
#elif PF_CC_MSVC
#define PF_MALLOC_LIKE __declspec(restrict)
#else
#define PF_MALLOC_LIKE
#endif

#if PF__GNU_HAS(hot)
#define PF_HOT PF__GNU(hot)
#else
#define PF_HOT
#endif

#if PF__GNU_HAS(cold)
#define PF_COLD PF__GNU(cold)
#else
#define PF_COLD
#endif

#if PF__GNU_HAS(always_inline)
#define PF_ALWAYS_INLINE PF__GNU(always_inline)
#else
#define PF_ALWAYS_INLINE
#endif

#if PF__GNU_HAS(noinline)
#define PF_NOINLINE PF__GNU(noinline)
#elif PF_CC_MSVC
#define PF_NOINLINE __declspec(noinline)
#else
#define PF_NOINLINE
#endif

#if PF__GNU_HAS(weak) && !PF_OS_WINDOWS && !PF_OS_CYGWIN
#define PF_HAS_WEAK 1
#define PF_WEAK PF__GNU(weak)
#else
#define PF_HAS_WEAK 0
#define PF_WEAK
#endif

#if PF__GNU_HAS(cleanup)
#define PF_HAS_CLEANUP 1
#define PF_CLEANUP(fn) PF__GNU(cleanup(fn))
#else
#define PF_HAS_CLEANUP 0
#define PF_CLEANUP(fn)
#endif

/* ---- Packed structures ------------------------------------------------ */

#if PF_CC_MSVC
#define PF_PACKED_BEGIN __pragma(pack(push, 1))
#define PF_PACKED_END __pragma(pack(pop))
#else
#define PF_PACKED_BEGIN _Pragma("pack(push, 1)")
#define PF_PACKED_END _Pragma("pack(pop)")
#endif

/* ---- Constructors and destructors ------------------------------------- */

#if PF__GNU_HAS(constructor) && PF__GNU_HAS(destructor)
#define PF_CONSTRUCTOR(name)                                                   \
  static void name(void);                                                      \
  PF__GNU(constructor) static void name(void)
#define PF_DESTRUCTOR(name)                                                    \
  static void name(void);                                                      \
  PF__GNU(destructor) static void name(void)
#elif defined(__cplusplus)
#define PF_CONSTRUCTOR(name)                                                   \
  static void name(void);                                                      \
  namespace {                                                                  \
  struct name##_pf_ctor_t {                                                    \
    name##_pf_ctor_t() { name(); }                                             \
  } name##_pf_ctor;                                                            \
  }                                                                            \
  static void name(void)
#define PF_DESTRUCTOR(name)                                                    \
  static void name(void);                                                      \
  namespace {                                                                  \
  struct name##_pf_dtor_t {                                                    \
    ~name##_pf_dtor_t() { name(); }                                            \
  } name##_pf_dtor;                                                            \
  }                                                                            \
  static void name(void)
#elif PF_CC_MSVC
#include <stdlib.h>
#ifdef _M_IX86
#define PF__SYMBOL_PREFIX "_"
#else
#define PF__SYMBOL_PREFIX ""
#endif
    /* A pointer in the CRT initializer section, kept by the linker with
       /include although nothing refers to it. */
#define PF_CONSTRUCTOR(name)                                                   \
  static void __cdecl name(void);                                              \
  __pragma(section(".CRT$XCU", read)) __declspec(allocate(".CRT$XCU")) void(   \
      __cdecl *const name##_pf_ctor)(void) = name;                             \
  __pragma(comment(linker, "/include:" PF__SYMBOL_PREFIX #name                 \
                           "_pf_ctor")) static void __cdecl                    \
  name(void)
#define PF_DESTRUCTOR(name)                                                    \
  static void __cdecl name(void);                                              \
  PF_CONSTRUCTOR(name##_pf_dtor) { atexit(name); }                             \
  static void __cdecl name(void)
#endif

#endif /* CPOLYFILL_ATTRIBUTE_H */
