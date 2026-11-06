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
| `attribute.h` | `pf_attribute.h` | H | see [step 2 spec](#step-2-language-layer) |
| `stdc.h` | `pf_types.h` (keywords) | H | see [step 2 spec](#step-2-language-layer) |
| `types.h` | `pf_types.h` (checks) | H | see [step 2 spec](#step-2-language-layer) |
| `macro.h` | `pf_macro.h` | H | see [step 2 spec](#step-2-language-layer) |
| `assert.h` | `pf_assert.h` | H+L | see [step 2 spec](#step-2-language-layer) |
| `debug.h` | (new) | L | see [step 2 spec](#step-2-language-layer) |
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

## Approved step specs

Specs approved by the maintainer but not implemented yet. Once a step is
done, replace its spec with a few lines in "Current state and notes".

### Step 2: language layer

Approved 2026-11-06. Header-only except the assert handler and `debug`.
Follow the naming rules in [Conventions](#conventions) (upper-case
macros, keywords only under `PF_STD_NAMES_STDC`).

**`attribute.h`**
- Keep v1's set: `PF_DEPRECATED(reason)`, `PF_FALLTHROUGH`,
  `PF_NORETURN`, `PF_NODISCARD(reason)`, `PF_UNUSED`,
  `PF_REPRODUCIBLE`, `PF_UNSEQUENCED`.
- Add: `PF_PRINTF(fmt_index, args_index)`, `PF_NONNULL(...)`,
  `PF_RETURNS_NONNULL`, `PF_PURE`, `PF_CONST`, `PF_MALLOC_LIKE`,
  `PF_HOT`, `PF_COLD`, `PF_ALWAYS_INLINE`, `PF_NOINLINE`, `PF_PACKED`,
  `PF_WEAK`, `PF_CONSTRUCTOR`, `PF_DESTRUCTOR` (MSVC through
  `.CRT$XCU` sections), `PF_CLEANUP(fn)`.
- Use the C23 `[[x]]` form only when compiling as C23 (`PF_STD_C23`, or
  C++ with `__has_cpp_attribute`): GCC reports `__has_c_attribute` in
  older modes where `[[ ]]` is not accepted. Otherwise `__attribute__`
  or `__declspec`, otherwise nothing.
- Every macro is written at the start of a declaration, which is valid
  for both forms. Document placement in the header.

**`stdc.h`** (keyword polyfills)
- `PF_TYPEOF`, `PF_TYPEOF_UNQUAL`, `PF_ALIGNOF`, `PF_ALIGNAS`
  (accepting a type or an expression, fixing v1's TODO),
  `PF_STATIC_ASSERT(expr[, msg])`, `PF_THREAD_LOCAL`, `PF_NORETURN`
  (keyword form; the attribute is in `attribute.h`, avoid a clash),
  `PF_UNREACHABLE()`, `PF_NULLPTR`, `pf_max_align_t`, `pf_char8_t`.
- `PF_STATIC_ASSERT` must be a real compile-time check on C99 (the
  negative-array-size trick with a unique name). v1 fell back to a
  runtime assert.
- Availability macros (`PF_HAS_TYPEOF`, ...) where a fallback is not
  possible.
- With `PF_STD_NAMES` / `PF_STD_NAMES_STDC`: define `typeof`,
  `typeof_unqual`, `alignof`, `alignas`, `static_assert`,
  `thread_local`, `unreachable`, `nullptr`, `bool`/`true`/`false` (via
  `<stdbool.h>`), only where the language lacks them.

**`types.h`** (type checks)
- `PF_IS_TYPE(type, expr)`, `PF_SAME_TYPE(a, b)`, `PF_IS_CONST(expr)`,
  `PF_IS_ARRAY(expr)`, `PF_IS_POINTER(expr)`,
  `PF_CHECK_TYPE(type, expr)` (compile error on mismatch, yields
  `expr`), `PF_IF_TYPE(type, expr, a, b)`, `PF_IF_CONST(expr, a, b)`.
- Prefer `_Generic` / `__builtin_types_compatible_p` /
  `__builtin_choose_expr`. Where the compiler can't check, fall back to
  a fixed constant and set `PF_HAS_TYPE_CHECKS` to 0 so callers know.
- Reference: v1 `pf_types.h` (`git show main:include/pf_types.h`).

**`macro.h`**
- v1's set, upper case, defined once (v1 defined `container_of` and
  `ALIGN_*` in two headers, differently): `PF_MIN`, `PF_MAX`, `PF_ABS`,
  `PF_DIFF`, `PF_CLAMP`, `PF_SWAP`, `PF_OFFSET`, `PF_PTRDIFF`,
  `PF_IS_ALIGNED`, `PF_ALIGN_PAD`, `PF_ALIGN_UP`, `PF_ALIGN_DOWN`,
  `PF_FLAG_*`, `PF_BIT*`, `PF_MASK`, `PF_COUNTOF`, `PF_CONTAINER_OF`,
  `PF_ENUM_GUARD`, `PF_EXPECT`, `PF_EXPECT_P`, `PF_ASSUME_ALIGNED`.
- Add: `PF_STRINGIFY`, `PF_CONCAT`, `PF_UNIQUE(name)`, `PF_NARGS(...)`
  (up to 64), `PF_LIKELY`, `PF_UNLIKELY`, `PF_ASSUME(expr)`.
- `PF_COUNTOF` fails to compile when given a pointer (GCC/Clang).
- Single-evaluation forms (statement expressions) on GNU compilers,
  plain forms elsewhere, as in v1. v1's `PF_EXPECT` fallback expanded
  to nothing, which drops the expression: fall back to `(expr)`.

**`assert.h`**
- `PF_ASSERT(expr)`, `PF_ASSERT_MSG(expr, fmt, ...)`: compiled out with
  `NDEBUG`. `PF_ASSERT_ALWAYS(expr)`, `PF_ASSERT_ALWAYS_MSG(...)`: never
  compiled out.
- `PF_CHECK(expr)` (+ `_MSG`): asserts in debug builds, a plain boolean
  condition with `NDEBUG` (v1's `pf_check`), for parameter validation.
- Comparison variants that print both values on failure:
  `PF_ASSERT_EQ` / `_NE` / `_LT` / `_LE` / `_GT` / `_GE` (integers),
  `PF_ASSERT_STR_EQ`, `PF_ASSERT_MEM_EQ`, `PF_ASSERT_NULL`,
  `PF_ASSERT_NOT_NULL`, and the same for `PF_CHECK_*`.
- Messages show the expression unexpanded, as in v1.
- Failure handler in the library: `pf_assert_handler_fn`,
  `pf_set_assert_handler(fn)` returning the previous one,
  `pf_assert_fail(...)` noreturn. The default handler prints what users
  expect from `assert`: the glibc format
  `prog: file:line: func: Assertion `expr' failed.` on stderr (with the
  formatted message appended for `_MSG` variants), flushes, and calls
  `abort()`. No backtrace by default; a custom handler can call
  `pf_backtrace_print()`.
- Under the opt-in, `assert` maps to `PF_ASSERT` and `static_assert` to
  `PF_STATIC_ASSERT`. Fix v1 bugs: `#define assert pf_assert;` had a
  stray semicolon, the memcmp/strcmp variants ended in `;`, and the
  `static_assert` fallback was a runtime check.
- Once done, replace `test/check.h`'s private `CHECK*` macros with
  `PF_ASSERT_ALWAYS*`.

**`debug.h`**
- `pf_debugbreak()`: inline, `__builtin_debugtrap` / `__debugbreak` /
  `raise(SIGTRAP)`.
- In the library: `bool pf_debugger_attached(void)` (Linux
  `/proc/self/status` TracerPid, macOS `sysctl` P_TRACED, BSD
  `sysctl`, Windows `IsDebuggerPresent`), `int pf_backtrace(void **frames,
  int max)`, `void pf_backtrace_print(FILE *out)` (execinfo where
  available, else `_Unwind_Backtrace`; Windows
  `CaptureStackBackTrace`; symbol names best effort).

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

## Open questions

- Minimum platform versions (proposed Windows 10, macOS 11,
  glibc 2.17).
- Should `allocator.h` become a public, installed header (e.g. if the
  allocator library is merged into cpolyfill)? For now consumers get it
  from the allocator library itself.
