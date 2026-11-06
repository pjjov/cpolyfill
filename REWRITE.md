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

- Naming: functions `pf_snake_case`, types `pf_name_t`, macros and
  constants `PF_UPPER`, internals `pf__` / `PF__`. Struct fields are
  snake_case. Object lifetime is `pf_<x>_init` / `pf_<x>_free`.
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
| `attribute.h` | `pf_attribute.h` | H | + `PF_PRINTF(fmt, args)`, `PF_NONNULL`, `PF_PURE`, `PF_CONST`, `PF_HOT`, `PF_COLD`, `PF_ALWAYS_INLINE`, `PF_NOINLINE`, `PF_PACKED`, `PF_WEAK`, `PF_CONSTRUCTOR`, `PF_DESTRUCTOR` (MSVC via `.CRT$XCU`), `PF_MALLOC_LIKE`, `PF_RETURNS_NONNULL`, `PF_CLEANUP(fn)` |
| `stdc.h` | `pf_types.h` (keywords) | H | `typeof`, `typeof_unqual`, `alignof`, `alignas` (fix passing a type), one-argument `static_assert`, `bool`, `thread_local`, `noreturn`, `unreachable()`, `nullptr`, `max_align_t`, `char8_t` |
| `types.h` | `pf_types.h` (checks) | H | `pf_is_type`, `pf_check_type`, `pf_is_const`, + `pf_is_array`, `pf_is_pointer`, `pf_same_type` |
| `macro.h` | `pf_macro.h` | H | single `container_of` / `ALIGN_*` (v1 duplicates them in two headers); + `PF_STRINGIFY`, `PF_CONCAT`, `PF_UNIQUE(name)`, `PF_NARGS(...)`, `PF_FOREACH`, `PF_LIKELY`, `PF_UNLIKELY`, `PF_ASSUME` |
| `assert.h` | `pf_assert.h` | H+L | runtime failure handler; + `PF_DEBUG_ASSERT` |
| `debug.h` | (new) | L | `pf_debugbreak()`, `pf_debugger_attached()`, `pf_backtrace()` |
| `bitwise.h` | `pf_bitwise.h` | H | keep implementation tiers; + C23 `<stdbit.h>` names when missing |
| `overflow.h` | `pf_overflow.h` | H | + C23 `ckd_add` / `ckd_sub` / `ckd_mul`, using type-generic builtins where available |
| `endian.h` | `pf_endian.h` | H | + `htobe16` / `le32toh` / ... on Windows and macOS; unaligned `pf_load_le32` / `pf_store_be64` helpers |
| `ctype.h` | `pf_ctype.h` | H | mostly unchanged; UTF-8 helpers move to `unicode.h` |
| `unicode.h` | (new) | L | UTF-8 <-> UTF-16/32, validation, decode/encode, `mbrtoc16` / `c16rtomb` (missing on macOS) |
| `string.h` | `pf_string.h` | L | + `strndup`, `strlcpy`, `strlcat`, `strcasecmp`, `strncasecmp`, `strtok_r`, `strsep`, `memmem`, `memrchr`, `stpcpy`, `memccpy`, `memset_explicit`, `asprintf`, `vasprintf`, `pf_qsort_r` |
| `stdlib.h` | (new) | L | `pf_aligned_alloc` / `pf_aligned_free`, `reallocarray`, `setenv`, `unsetenv`, `getline`, `getdelim`, `realpath` |
| `errno.h` | `pf_errno.h` | L | step 1: `PF_E*` codes, `pf_error_from_errno` / `pf_error_to_errno`; step 4: public Win32/WSA mapping, `pf_strerror` |
| `version.h` | (new) | L | done in step 1 |
| `time.h` | (new) | L | `clock_gettime` (monotonic, realtime) on Windows, `pf_time_ns()`, `nanosleep`, `timespec_get`, `gmtime_r`, `localtime_r`, `timegm` |
| `threads.h` | `pf_threads.h` | L | C11 API; thread name, id, affinity, priority; Windows tss destructors via `FlsAlloc` (drops `pf_tss_cleanup`); rwlock, spinlock, barrier stay |
| `atomic.h` | `pf_atomic.h` | H+L | fallback uses a global striped lock table (like libatomic) instead of a mutex per object, so no `atomic_init` requirement and `_Atomic(T)` keeps the size of `T` |
| `semaphore.h` | `pf_semaphore.h` | L | + working unnamed semaphores on macOS |
| `fs.h` | `pf_filesystem.h` | L | unified `pf_stat` (64-bit size, ns times), `pf_mkdirs`, `pf_remove_all`, replacing `rename` on Windows, `mkstemp`, `mkdtemp`, file locks, symlinks, path join / normalize / basename / dirname; known directories stay |
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
- [ ] 2. Language layer: `attribute`, `stdc`, `types`, `macro`,
      `assert`, `debug`
- [ ] 3. Integers: `bitwise`, `overflow`, `endian`
- [ ] 4. Text: `ctype`, `unicode`, `string`, `stdlib`, `errno`
- [ ] 5. Concurrency: `time`, `threads`, `atomic`, `semaphore`
- [ ] 6. Files: `fs`, `dirent`, `dlfcn`, `mman`, `io`
- [ ] 7. OS: `process`, `signal`, `random`, `sysinfo`, `cpuid`, `term`
- [ ] 8. Network: `socket`, `pipe`
- [ ] 9. Apps: `cli`, `argparse`, `daemon`, `test`, `exception`,
      `array`, `typeid`

## Current state and notes

What exists after step 1:

- `include/cpolyfill/`: `platform.h`, `config.h.in` (+ `meson.build`
  listing `public_headers`; add new headers there), `version.h`,
  `errno.h`.
- `src/`: `internal.h` (include first in every source), `allocator.h`
  (vendored), `alloc.c`, `errno.c`, `utf16.c`, `version.c`,
  `win32/error.c` (Win32/WSA -> `PF_E*`, internal for now).
- `test/`: `runner.h` is v1's `pf_test.h`, kept as a temporary runner
  until the `test` module is ported (step 9). `check.h` has `CHECK`,
  `CHECK_INT`, `CHECK_STR`, `CHECK_MEM`, `SKIP`, `KNOWN_BUG`,
  `test_rand`. A new suite is a `test/<name>.c` defining
  `pf_test_t suite_<name>[]`, listed in `test/meson.build` and
  `test/main.c`. Tests link the library's objects directly, so they can
  include `../src/internal.h` and test internals.
- Test variants: default (gnu11), gnu99, gnu23, plus
  `check_headers.py` (each public header alone in c99..gnu23 with
  `-Wall -Wextra -Wundef -Werror`, `-pedantic-errors` for strict
  standards, C++11/17/20, and a two-TU link against the library).
- Verified on Linux with GCC 16 (shared and static) and Clang 23 with
  ASan/UBSan. `nm -D` shows only the public `pf_*` functions exported.

Caveats:

- **Windows code is untested.** This machine has no MinGW or Windows SDK,
  so `src/win32/` is not even compiled. Needs a Windows or MinGW build
  (or CI) before relying on it.
- The UTF-16 conversion is strict: unpaired surrogates in Windows file
  names are rejected with `PF_EILSEQ` (no WTF-8). Revisit in step 6 if
  needed.
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

## Open questions

- Minimum platform versions (proposed Windows 10, macOS 11,
  glibc 2.17).
- Should `allocator.h` become a public, installed header (e.g. if the
  allocator library is merged into cpolyfill)? For now consumers get it
  from the allocator library itself.
