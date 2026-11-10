<!--
SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
SPDX-License-Identifier: Apache-2.0
-->

# cpolyfill v2 rewrite

This is the working plan for rewriting cpolyfill from a header-only
library (v1, `include/pf_*.h`) into a linkable meson library (v2,
`include/cpolyfill/*.h` + `src/`). Read it at the start of every session
that works on the rewrite, and keep it up to date as you go: tick off
steps, record decisions, and note anything the next session must know.

## Resuming in a new session

1. `git switch v2`, read this whole file.
2. Look at [Steps](#steps) for the first unchecked step, and at
   [Current state and notes](#current-state-and-notes).
3. If the step has an approved spec under
   [Approved step specs](#approved-step-specs), implement it. Otherwise
   read the v1 sources of its modules and propose an API first.
4. Build and test: `meson setup build && meson test -C build`. Before
   committing, also run a Clang build with
   `-Db_sanitize=address,undefined -Db_lundef=false`, and a
   `-Ddefault_library=static` build.
5. When a step is done: tick it, rewrite "Current state and notes",
   move its spec into a short summary, log decisions, commit.

## How to work on the rewrite

- Work happens on the `v2` branch. `main` stays on v1 (1.7.x).
- Work proceeds one step at a time (see [Steps](#steps)). For each
  module: propose its API and changes to the maintainer, wait for
  approval, then implement it together with its tests.
- The v1 source of a module is the reference for porting. Once the v1
  files are removed from `v2`, read them with
  `git show main:include/pf_<name>.h` and `git show main:test/<name>.c`.
- Record every decision the maintainer makes in the
  [Decision log](#decision-log), and open questions in
  [Open questions](#open-questions).
- Commits: conventional-commit subject line only (`feat(threads): ...`,
  `build: ...`, `docs: ...`). No commit body and no co-author trailer.
  Commit after each coherent piece of work.

## Goals

1. **Linkable library.** Meson `library()` instead of header-only.
   Header-only caused per-translation-unit copies of global state and
   build problems in larger modules.
2. **Portability by inclusion.** Including a header should make existing
   code more portable, and supply features many compilers have but the C
   standard lacks.
3. **No platform or compiler macro checks in user code.** Expose
   OS-specific features behind one API, and provide reliable detection
   macros for the cases where users still need to branch.
4. **Headers in a subdirectory.** `#include <cpolyfill/threads.h>`
   instead of `#include <pf_threads.h>`.

## Architecture

### Two layers

| Layer | Contents | Form |
|---|---|---|
| Language | keywords, attributes, builtins, bit/overflow ops, endian, ctype, type checks, utility macros | Header-only `static inline` and macros. Detected against the **consumer's** compiler and standard, because they cannot be compiled ahead of time. |
| Platform | threads, files, processes, sockets, time, dlfcn, ... | Declarations in headers, implementation in `src/`. Detected **once, at library build time**, and recorded in the generated `cpolyfill/config.h`. |

Headers must trust `config.h` for anything affecting ABI (type layouts
such as `mtx_t`), so a consumer building with `-std=c99` and one with
`-std=c23` see the same types as the library.

### Layout

```
include/cpolyfill/platform.h     OS/compiler/arch/standard detection, PF_API, PF_BEGIN_DECLS
include/cpolyfill/config.h.in    template, filled in by meson and installed
include/cpolyfill/*.h            public headers
src/*.c                          implementation, src/win32/ and src/posix/ where a split helps
src/internal.h                   utf8<->utf16, win32/wsa error mapping, allocator access
test/                            test suites, built on cpolyfill/test.h
```

Inside the project, always include public headers as
`<cpolyfill/name.h>`, never `"name.h"`: names like `string.h`, `time.h`
and `errno.h` would otherwise shadow the system headers.

### platform.h

Included by every other header. All macros are always defined to `0` or
`1` (never left undefined) so `#if PF_OS_LINUX && PF_ARCH_ARM64` works
and `-Wundef` stays clean.

- OS: `PF_OS_WINDOWS`, `PF_OS_LINUX`, `PF_OS_ANDROID`, `PF_OS_MACOS`,
  `PF_OS_IOS`, `PF_OS_FREEBSD`, `PF_OS_NETBSD`, `PF_OS_OPENBSD`,
  `PF_OS_BSD`, `PF_OS_POSIX`, `PF_OS_EMSCRIPTEN`
- Compiler: `PF_CC_GCC`, `PF_CC_CLANG`, `PF_CC_APPLE_CLANG`,
  `PF_CC_MSVC`, `PF_CC_CLANG_CL`, `PF_CC_MINGW`, `PF_CC_GNU`, plus
  `PF_GCC_VERSION` / `PF_CLANG_VERSION` / `PF_MSVC_VERSION` and
  `PF_GCC_PREREQ(maj, min)` / `PF_CLANG_PREREQ(maj, min)` /
  `PF_MSVC_PREREQ(ver)` (one macro per compiler, because their version
  numbers are not comparable)
- Architecture: `PF_ARCH_X86`, `PF_ARCH_X86_64`, `PF_ARCH_ARM`,
  `PF_ARCH_ARM64`, `PF_ARCH_RISCV`, `PF_ARCH_WASM`, `PF_ARCH_BITS`
- Standard: `PF_STD_C99`, `PF_STD_C11`, `PF_STD_C17`, `PF_STD_C23`
  (cumulative), `PF_CPP`
- Detection: `PF_HAS_BUILTIN(x)`, `PF_HAS_ATTRIBUTE(x)`,
  `PF_HAS_C_ATTRIBUTE(x)`, `PF_HAS_FEATURE(x)`, `PF_HAS_EXTENSION(x)`,
  safe on every compiler. There is no `PF_HAS_INCLUDE`: `__has_include`
  cannot be wrapped portably, test `defined(__has_include)` first.
- Linkage: `PF_API` (dllexport/dllimport or visibility), `PF_INLINE`,
  `PF_BEGIN_DECLS` / `PF_END_DECLS`

### Standard-name polyfills (opt-in)

The library always exports prefixed functions (`pf_mkdir`, `pf_dlopen`,
`pf_sem_wait`, ...) that behave the same on every platform. The standard
names (`mkdir`, `dlopen`, `sem_wait`, ...) are mapped to them with
`#define` **only when the user opts in** by defining `PF_STD_NAMES`
before including, and only on platforms where the standard name is
missing or behaves differently. The library never exports a symbol with
a standard name, to avoid clashing with the system's own (MinGW already
declares a one-argument `mkdir`, for example).

Opt-in macros: `PF_STD_NAMES` (every header), `PF_STD_NAMES_<MODULE>`
(one header, e.g. `PF_STD_NAMES_THREADS`), and `PF_NO_STD_NAMES_<MODULE>`
to exclude one header from `PF_STD_NAMES`. Each header checks them with
`#if (defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_X)) || defined(PF_STD_NAMES_X)`
(a helper macro is not possible, `defined` produced by macro expansion is
not portable).

### Conventions

- Naming: functions `pf_snake_case`, types `pf_name_t`, internals
  `pf__` / `PF__`. Struct fields are snake_case. Object lifetime is
  `pf_<x>_init` / `pf_<x>_free`.
- **All macros are upper case** `PF_*`, function-like ones included
  (`PF_CONTAINER_OF`, `PF_IS_TYPE`, not v1's `pf_container_of`). The only
  lower-case macros are standard names mapped under the opt-in.
- **Language keywords count as standard names.** `typeof`, `alignas`,
  `alignof`, `bool`, `static_assert`, `thread_local`, `noreturn`,
  `unreachable`, `nullptr`, ... are always available as `PF_TYPEOF`,
  `PF_ALIGNAS`, `PF_STATIC_ASSERT`, `PF_THREAD_LOCAL`,
  `PF_UNREACHABLE()`, `PF_NULLPTR`, ... and the keyword names themselves
  are only defined under the standard-name opt-in (v1 defined them
  unconditionally).
- No `pf_bool`. C99 is the baseline, so public APIs use `bool` from
  `<stdbool.h>` (or `int` where a tri-state/error is returned).
- Errors: new `pf_*` functions return `int`: `0` on success, a negative
  `PF_E*` code on failure. Functions standing in for a standard function
  keep that standard's convention (e.g. `-1` and `errno`).
  `cpolyfill/errno.h` maps Win32 and WSA errors to `PF_E*`.
- Strings and paths are UTF-8 on every platform. On Windows the library
  converts to UTF-16 and calls the `W` APIs, with long-path support.
- Memory: the maintainer's `allocator_t` interface (from their separate
  header-only library, which may be merged into cpolyfill later).
  - A vendored copy lives in `src/allocator.h` and is used internally;
    it is **not installed**. Keep it identical to upstream.
  - Public headers never include it. They forward-declare
    `struct allocator_t;` and take `struct allocator_t *` (C99 forbids
    repeating a `typedef`, so no `allocator_t` typedef in public headers).
  - A NULL allocator means the library default (`pf__default_allocate` in
    `src/alloc.c`). Contract for the default: an allocation with extended
    alignment must be reallocated with the same alignment. On Windows every
    default allocation uses `_aligned_*`, because `allocator_fn` frees
    without the alignment.
  - Functions polyfilling standard functions that return `free()`-able
    memory (`strdup`, `asprintf`, `getline`, ...) use plain `malloc`, not
    the allocator.
  - Compile-time hooks such as v1's `PF_MALLOC` or `PF_ASSERT_FAIL` become
    runtime setters, since the library is precompiled.
- C++: every header is usable from C++ (`PF_BEGIN_DECLS`).

### Build

- `library('cpolyfill', ...)`, honouring `default_library`, hidden
  symbol visibility by default, `soversion: 2`. The library's own sources
  are C11 (`c_std=gnu11,c11`); public headers stay C99.
- `PF_API`: `dllexport` while building (`PF__BUILDING`, set by
  `src/internal.h`), `dllimport` for consumers only when
  `PF_CONFIG_SHARED` (default_library=shared). With `both` or `static`,
  consumers get no `dllimport`, which is fine for functions; exported
  *data* would need care. A static library on Windows still carries
  `dllexport`, so a consumer DLL linking it statically re-exports
  cpolyfill's functions.
- `declare_dependency` carries the system libraries (`threads`, `dl`,
  `ws2_32`, `bcrypt`, `ole32`, `shell32`, `advapi32`, ...) so consumers
  don't list them. `meson.override_dependency('cpolyfill', ...)`.
- pkg-config name stays `cpolyfill`, with `Libs.private` for static
  linking.
- Modules with heavier code (`cli`, `argparse`, `daemon`, `test`) are
  meson `feature` options.
- Test variants: default standard, `gnu99`, `c23`, atomic fallback, and
  a header self-check (each header compiles alone in C99, C11, C23 and
  C++).

### Platform support

- C99 is the minimum standard for consumers. MSVC is supported.
- Proposed minimums, **not confirmed yet**: Windows 10, macOS 11,
  glibc 2.17.

## Module map

H = header-only (language layer), L = compiled (platform layer).

| v2 header | From v1 | Layer | Planned changes and additions |
|---|---|---|---|
| `platform.h` | (new) | H | see [platform.h](#platformh) |
| `attribute.h` | `pf_attribute.h` | H | done in step 2 |
| `stdc.h` | `pf_types.h` (keywords) | H | done in step 2 |
| `types.h` | `pf_types.h` (checks) | H | done in step 2 |
| `macro.h` | `pf_macro.h` | H | done in step 2 |
| `assert.h` | `pf_assert.h` | H+L | done in step 2 |
| `debug.h` | (new) | L | done in step 2 |
| `bitwise.h` | `pf_bitwise.h` | H | done in step 3 |
| `overflow.h` | `pf_overflow.h` | H | done in step 3 |
| `endian.h` | `pf_endian.h` | H | done in step 3 |
| `ctype.h` | `pf_ctype.h` | H | done in step 4 |
| `unicode.h` | (new) | H+L | done in step 4 |
| `string.h` | `pf_string.h` | L | done in step 4 |
| `stdlib.h` | (new) | L | done in step 4 |
| `errno.h` | `pf_errno.h` | L | done in steps 1 and 4 |
| `version.h` | (new) | L | done in step 1 |
| `time.h` | (new) | L | `clock_gettime` (monotonic, realtime) on Windows, `pf_time_ns()`, `nanosleep`, `timespec_get`, `gmtime_r`, `localtime_r`, `timegm` |
| `threads.h` | `pf_threads.h` | L | C11 API; thread name, id, affinity, priority; Windows tss destructors via `FlsAlloc` (drops `pf_tss_cleanup`); rwlock, spinlock, barrier stay |
| `atomic.h` | `pf_atomic.h` | H+L | fallback uses a global striped lock table (like libatomic) instead of a mutex per object, so no `atomic_init` requirement and `_Atomic(T)` keeps the size of `T` |
| `semaphore.h` | `pf_semaphore.h` | L | + working unnamed semaphores on macOS |
| `fs.h` | `pf_filesystem.h` | L | unified `pf_stat` (64-bit size, ns times), `pf_mkdirs`, `pf_remove_all`, replacing `rename` on Windows, `realpath` (moved from `stdlib.h`), `mkstemp`, `mkdtemp`, file locks, symlinks, path join / normalize / basename / dirname; known directories stay |
| `dirent.h` | `pf_dirent.h` | L | UTF-8, `d_type` on Windows |
| `dlfcn.h` | `pf_dlsym.h` | L | renamed after the POSIX header; + `PF_DL_EXT` |
| `mman.h` | (new) | L | `mmap`, `munmap`, `mprotect` on Windows, page size, file mapping |
| `io.h` | `pf_io.h` | L | stream vtable stays, opaque state instead of a fixed blob; + memory and fd streams, `fmemopen`, `open_memstream` |
| `process.h` | `pf_process.h` | L | + `pf_spawn` with stdio pipes, wait with timeout, kill, `pf_exe_path`, environment, hostname |
| `signal.h` | (new) | L | portable shutdown handling (SIGINT/SIGTERM and `SetConsoleCtrlHandler`) |
| `random.h` | (new) | L | `pf_getrandom` (getrandom / arc4random_buf / BCryptGenRandom) |
| `sysinfo.h` | `pf_cpuinfo.h` (part) | L | logical and physical CPU count on all architectures, page size, cache line size, total memory, OS name and version |
| `cpuid.h` | `pf_cpuinfo.h` | H | x86 CPUID / xgetbv; + ARM feature detection |
| `term.h` | `pf_cli.h` (part) | L | isatty, terminal size, raw mode, Windows VT and UTF-8 console setup |
| `socket.h` | `pf_socket.h` | L | + `socketpair`, AF_UNIX on Windows |
| `pipe.h` | `pf_pipe.h` | L | port |
| `cli.h` | `pf_cli.h` | L | port onto `term.h`, naming cleanup |
| `argparse.h` | `pf_argparse.h` | L | port, naming cleanup |
| `daemon.h` | `pf_daemon.h` | L | port, snake_case options, `PF_E*` errors |
| `test.h` | `pf_test.h` | L | port |
| `exception.h` | `pf_exception.h` | L | port |
| `array.h` | `pf_array.h` | H (+L) | port, runtime allocator |
| `typeid.h` | `pf_typeid.h` | L | port |

Later, not part of v2.0: event loop (epoll / kqueue / IOCP), file
watching (inotify / kqueue / ReadDirectoryChangesW).

## Steps

Mark a step done only when its modules are implemented, tested and
committed.

- [x] 1. Skeleton: meson library, `config.h` generation, `platform.h`,
      `version.h`, `errno.h` (codes), internal helpers (utf8<->utf16,
      error mapping, allocator), test harness, header self-check, removal
      of v1 files
- [x] 2. Language layer: `attribute`, `stdc`, `types`, `macro`,
      `assert`, `debug`
- [x] 3. Integers: `bitwise`, `overflow`, `endian`
- [x] 4. Text: `ctype`, `unicode`, `string`, `stdlib`, `errno`
- [ ] 5. Concurrency: `time`, `threads`, `atomic`, `semaphore`
- [ ] 6. Files: `fs`, `dirent`, `dlfcn`, `mman`, `io`
- [ ] 7. OS: `process`, `signal`, `random`, `sysinfo`, `cpuid`, `term`
- [ ] 8. Network: `socket`, `pipe`
- [ ] 9. Apps: `cli`, `argparse`, `daemon`, `test`, `exception`,
      `array`, `typeid`

## Approved step specs

Specs approved by the maintainer but not implemented yet. Once a step is
done, replace its spec with a few lines in "Current state and notes".

None pending. Step 5 (`time`, `threads`, `atomic`, `semaphore`) needs
an API proposal first.

## Current state and notes

What exists after step 4:

- `include/cpolyfill/`: `platform.h`, `config.h.in` (+ `meson.build`
  listing `public_headers`; add new headers there), `version.h`,
  `errno.h`, the language layer: `attribute.h`, `stdc.h`, `types.h`,
  `macro.h`, `assert.h`, `debug.h`, the integer headers `bitwise.h`,
  `overflow.h`, `endian.h` (header-only), and the text headers
  `ctype.h`, `unicode.h`, `string.h`, `stdlib.h`. Each header documents
  its API in its top comment.
- `src/`: `internal.h` (include first in every source), `allocator.h`
  (vendored), `alloc.c`, `assert.c`, `debug.c`, `errno.c`, `stdlib.c`,
  `string.c`, `uchar.c`, `unicode.c`, `version.c`, `win32/error.c`
  (Win32/WSA -> `PF_E*`, public since step 4).
  `debug.c` uses execinfo, else `_Unwind_Backtrace`, plus `dladdr`;
  meson detects them and passes `PF__HAVE_EXECINFO` / `PF__HAVE_UNWIND`
  / `PF__HAVE_DLADDR` as library arguments (not in `config.h`).
- `test/`: `runner.h` is v1's `pf_test.h`, kept as a temporary runner
  until the `test` module is ported (step 9). Tests check with
  `PF_ASSERT_ALWAYS*` (the private `CHECK*` macros are gone); `check.h`
  keeps `SKIP`, `KNOWN_BUG` and `test_rand`. A new suite is a
  `test/<name>.c` defining `pf_test_t suite_<name>[]`, listed in
  `test/meson.build` and `test/main.c`. Tests link the library's objects
  directly, so they can include `../src/internal.h` and test internals.
- Test variants: default (gnu11), gnu99, gnu23, plus
  - `check_headers.py`: each public header alone in c99..gnu23 with
    `-Wall -Wextra -Wundef -Werror`, `-pedantic-errors` for strict
    standards, C++11/17/20, again with `PF_STD_NAMES` followed by the
    system headers defining the same names, and a two-TU link.
  - `check_errors.py` + `errors.c`: misuse of the compile-time checks
    (`PF_STATIC_ASSERT`, `PF_COUNTOF` on a pointer, `PF_CHECK_TYPE`,
    `PF_ENUM_GUARD`, `PF_CONTAINER_OF`) must fail to compile, in C99,
    C11, C23 and C++. Add a case there for every new static check.
  - `cxx.cpp`: the macros used from C++11 and C++20, where several have
    template implementations.
- Verified on Linux with GCC 16 (shared and static) and Clang 23 with
  ASan/UBSan. `nm -D` shows only `pf_*` functions exported (including the
  internal `pf__assert_fail_*` the assert macros call). The
  `_Unwind_Backtrace` path (musl) and the no-backtrace fallback were run
  by hand with the meson detection overridden.

Integer headers (step 3):

- Byte order is `PF_ENDIAN_LITTLE` / `PF_ENDIAN_BIG` in `platform.h`,
  with `PF_CONFIG_BIG_ENDIAN` in `config.h` as the last fallback.
- Internal switches for the tests: `PF_BITWISE_PORTABLE` (public),
  `PF__OVF_PORTABLE` (overflow without builtins or `<stdckdint.h>`) and
  `PF__FORCE_STD_NAMES_POLYFILL` (the bitwise, overflow and endian
  standard names ignore the system headers). `bitwise_portable.c` and
  `overflow_portable.c` build the dispatchers of `bitwise_ops.h` /
  `overflow_ops.h` without builtins; the suites compare both against a
  bit-by-bit (bitwise) or `__int128` (overflow) reference. Without
  `__int128` the overflow suite only checks that the two agree.
- `std_polyfill.c` tests the forced polyfills; it `#undef`s the glibc
  `htobe16`... first, since `<stdlib.h>` pulls in `<endian.h>`.
- The GNU C99 generic `PF_CLZ` ends its `__builtin_choose_expr` chain in
  `pf__bitwise_argument_must_be_unsigned(void)`, so a wrong type is a
  "too many arguments" error naming it. C11 uses `_Generic`, C++
  overloads (`pf__g_*`, ambiguous for signed types).
- `overflow.h` keeps its type generators internal (`PF__OVF_SIGNED` /
  `PF__OVF_UNSIGNED`); v1's `PF_IMPL_OVERFLOW` is not public.
- MSVC paths (`_BitScan*`, `__popcnt`, `_rotl*`, `_byteswap_*`) are not
  compiled here, like the rest of the Windows code.

Text headers (step 4):

- `config.h` records what the C library has: `PF_CONFIG_HAVE_<FN>` for
  every function with a standard-name mapping (plus `STRINGS_H`,
  `UCHAR_H`; `QSORT_R` means the POSIX argument order). Meson checks
  them with `cc.links()` taking the function's address, not
  `has_function`, which accepts GCC builtins (`__builtin_strndup`). MSVC's
  deprecated `strdup` / `memccpy` count as missing. Facts only the
  library needs are `lib_args`: `PF__HAVE_QSORT_R_BSD`,
  `PF__HAVE_GETC_UNLOCKED`, `PF__HAVE_EXPLICIT_BZERO`,
  `PF__HAVE_MEMSET_S`, `PF__STRERROR_R_GNU`. Add new mappings the same
  way: a `libc_functions` entry in `include/cpolyfill/meson.build` and a
  line in `config.h.in`.
- The simple string functions always use the library's own code (what
  the tests check); `memmem`, `memset_explicit`, `getdelim`, `qsort_r`,
  `strerror_r` and the uchar functions call the system's where present.
  The fallbacks `pf__getdelim_portable` and `pf__qsort_r_portable`
  (heapsort) are tested on every platform.
- `uchar.c`: without the system functions, a `wc` backend (mbrtowc /
  wcrtomb, needs 32-bit `wchar_t` and an mbstate_t of at least 16 bytes,
  its UTF-16 / UTF-8 layer state in the last 8 bytes: macOS, BSDs) or a
  `utf8` backend (assumes UTF-8, whole mbstate_t is ours). Both are run by
  the `uchar` suite through `pf__*_with`. `pf_c32rtomb` rejects
  non-scalar values even where glibc's `c32rtomb` encodes them.
- The unicode standard names are C only (`char16_t` / `char32_t` are C++
  keywords). `char8_t` is only typedef'd in C23.
- `pf_reallocarray(p, 0, n)` returns a minimal block, not NULL
  (`realloc(p, 0)` is undefined in C23). `pf_aligned_alloc(a, 0)` too.
- The internal `pf__utf8_to_utf16` / `pf__utf16_to_utf8` and `PF__NTS`
  are gone: use the public functions with `PF_NTS` and flags 0.
- Checked by hand: a build with every `PF_CONFIG_HAVE_*` forced to 0
  passes the suites (the forced XSI `strerror_r` branch cannot run on
  glibc, which only declares the GNU variant with `_GNU_SOURCE`).

Language layer notes, for later steps:

- Attribute placement: `PF_*` attribute macros go first in a
  declaration, before `PF_API`, `static` and the type. GCC rejects
  `[[x]]` after `__attribute__` in C23, so GNU attributes use the
  `[[gnu::x]]` form in C23 and C++ (order-free among themselves), and
  `PF_NORETURN` is `__attribute__((noreturn))` rather than `_Noreturn` on
  GNU compilers (GCC warns about `_Noreturn static`). Exceptions:
  `PF_REPRODUCIBLE` / `PF_UNSEQUENCED` go after the parameter list,
  `PF_FALLTHROUGH;` is a statement.
- In C99, GCC and Clang accept `_Static_assert`, `_Alignas` and
  `_Alignof` behind `__extension__`, which is what `stdc.h` uses (an
  `__extension__` declaration must start the declaration, hence
  `PF_ALIGNAS` first). Other C99 compilers get a negative-size extern
  array for `PF_STATIC_ASSERT`.
- Type checks: GNU C uses `__builtin_types_compatible_p` and
  `__builtin_classify_type`; arrays vs pointers are told apart by
  comparing a type with that of `0 ? e : e` (lvalue conversion).
  `PF_CHECK_TYPE` is `__builtin_choose_expr` with a call to a function
  declared with the `error` attribute, so it keeps lvalues. C++ uses
  small templates (`pf__same`, `pf__noref`, ...) in `extern "C++"`
  blocks. MSVC C gets constants (`PF_HAS_TYPE_CHECKS` 0); a `_Generic`
  tier for MSVC C11 could be added later.
- Assertions: the integer comparisons compare values (C++20
  `std::cmp_*` style): operands go through `unsigned long long` with a
  signedness flag computed without evaluating them,
  `(0 ? (x) : 0) - 1 < (0 ? (x) : 0)`. Every public assert macro
  stringizes its own arguments (once passed on, they are expanded). The
  NDEBUG-dependent macros sit outside the include guard, like
  `<assert.h>`.

Caveats:

- **Windows code is untested.** This machine has no MinGW or Windows SDK,
  so `src/win32/` and the `PF_OS_WINDOWS` branches of `assert.c` and
  `debug.c` are not even compiled, nor are the MSVC branches of the
  headers (`__declspec` attributes, `.CRT$XCU` constructors,
  `__pragma(pack)`, `__alignof`, `__debugbreak`). Needs a Windows or
  MinGW build (or CI) before relying on it.
- The BSD and macOS branches of `pf_debugger_attached` (sysctl) are not
  compiled here either, nor the BSD `qsort_r` trampoline, the Windows
  environment functions (`_wgetenv` / `_wputenv_s`; on Windows an empty
  value removes a variable) and `qsort_s`.
- The UTF-16 conversion is strict: unpaired surrogates in Windows file
  names are rejected with `PF_EILSEQ` (no WTF-8). Revisit in step 6 if
  needed.
- `README.md` still describes v1. Rewrite it once the steps are done.
- `PF_E*` keeps only the portable POSIX/C errno set; v1's Linux-only
  codes (STREAMS, Xenix, `EL2HLT`, ...) were dropped. Codes are the
  negated Linux values, `PF_EUNKNOWN` is -4095.

## Decision log

- 2026-11-06: Two-layer split (header-only language layer, compiled
  platform layer) approved.
- 2026-11-06: Standard-name polyfills are explicit opt-in.
- 2026-11-06: New APIs return `0` or a negative `PF_E*` code.
- 2026-11-06: C99 stays the minimum supported standard.
- 2026-11-06: Dependency and pkg-config name stays `cpolyfill`, even
  though it breaks v1 consumers.
- 2026-11-06: Rewrite happens on the `v2` branch.
- 2026-11-06: All v1 modules are kept.
- 2026-11-06: Commits use a subject line only, no body, no co-author
  trailer.
- 2026-11-06: Standard names opt-in is both global (`PF_STD_NAMES`) and
  per module (`PF_STD_NAMES_<MODULE>`).
- 2026-11-06: Use the maintainer's `allocator_t` (vendored in
  `src/allocator.h`), with forward declarations in public headers,
  instead of a new `pf_allocator_t`.
- 2026-11-06: `PF_E*` codes moved into step 1.
- 2026-11-06: The library is compiled as C11; headers support C99.
- 2026-11-06: Step 2 spec approved. All macros upper case; language
  keywords count as standard names (opt-in).
- 2026-11-06: The default assert handler prints the conventional
  (glibc-style) one-line message and aborts, no backtrace.
- 2026-11-06: `pf_bool` is dropped; use `bool`.
- 2026-11-09: Step 2 implemented. Departures from the approved spec,
  approved by the maintainer on 2026-11-10:
  - `PF_PACKED` became `PF_PACKED_BEGIN` / `PF_PACKED_END` (`#pragma
    pack`): the `packed` attribute cannot be placed at the start of a
    declaration, and MSVC has no attribute for it at all, so a
    single-macro form would silently not pack there.
  - `PF_CONSTRUCTOR(name)` / `PF_DESTRUCTOR(name)` define `static void
    name(void)` (`PF_CONSTRUCTOR(init) { ... }`), since the MSVC
    `.CRT$XCU` form needs a separate pointer variable.
  - Added `PF_ASSERT_STR_NE` and `PF_ASSERT_MEM_NE` (v1 had them), and
    `PF_ASSERT_ALWAYS_*` / `PF_CHECK_*` for every comparison form.
  - The assert handler has no recursion guard: it could not tell a
    handler failing an assertion from one that left with `longjmp`.
  - `PF_EXPECT(expr, v)` yields `!!(expr)`, also in the fallback.
  - `PF_NARGS()` with no arguments is 0 only with `__VA_OPT__` (C23,
    C++20), 1 before.
  - `PF_ALWAYS_INLINE` is empty on MSVC (`__forceinline` together with
    `__inline` is untested).
- 2026-11-10: Step 3 spec approved. Byte-order macros live in
  `platform.h`; the v1 overflow names (`pf_overflow_sadd`, ...) stay.
- 2026-11-10: Step 3 implemented as specified. The C23 `<stdbit.h>` tier
  is only used in C23 (where `stdc_*` are guaranteed type-generic).
- 2026-11-10: Step 4 spec approved. `ctype` predicates return `bool`;
  standard names follow what the library found at build time; `realpath`
  moves to step 6, `qsort_r` to `stdlib.h`, v1's MSVC locale mappings are
  dropped; UTF conversions take a `flags` argument.
- 2026-11-10: Step 4 implemented as specified, plus: the unicode standard
  names are C only; `pf_c32rtomb` rejects values above U+10FFFF on every
  platform; `pf_strerror_r` returns EINVAL for a zero `size`.

## Open questions

- Minimum platform versions (proposed Windows 10, macOS 11,
  glibc 2.17).
- Should `allocator.h` become a public, installed header (e.g. if the
  allocator library is merged into cpolyfill)? For now consumers get it
  from the allocator library itself.
