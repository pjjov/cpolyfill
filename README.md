<!--
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
-->

# cpolyfill

A C library that fills the gaps between C standards, compilers and
operating systems.

It does three things:

- It gives newer C features (`typeof`, `static_assert`, attributes, atomics,
  threads, checked arithmetic and more) to older standards and to every
  compiler.
- It provides one API for things that differ between operating systems
  (files, processes, sockets, pipes, shared libraries, terminals, time), so
  user code needs no `#ifdef _WIN32`.
- It provides common utilities that many projects write themselves: UTF-8
  handling, argument parsing, a unit-test runner, exceptions, daemons.

```c
#include <cpolyfill/errno.h>
#include <cpolyfill/io.h>
#include <cpolyfill/threads.h>
#include <stdio.h>

static int count_lines(void *arg)
{
    void *data;
    size_t size, lines = 0;
    int err = pf_read_file((const char *)arg, &data, &size, NULL);
    if (err < 0) {
        fprintf(stderr, "%s: %s\n", (const char *)arg, pf_strerror(err));
        return -1;
    }
    for (size_t i = 0; i < size; i++)
        lines += ((const char *)data)[i] == '\n';
    pf_read_file_free(data, size, NULL);
    return (int)lines;
}

int main(int argc, char **argv)
{
    pf_thrd_t thread;
    int lines;
    if (argc < 2 ||
        pf_thrd_create(&thread, count_lines, argv[1]) != pf_thrd_success)
        return 1;
    pf_thrd_join(thread, &lines);
    printf("%d\n", lines);
    return lines < 0;
}
```

This program builds as C99 with GCC, Clang or MSVC, and runs the same on
Linux, macOS, the BSDs and Windows (where the path is UTF-8, too).

## How it works

The library has two layers:

- **Language features** (attributes, keywords, bit and overflow operations,
  byte order, type checks, utility macros) are header-only. They are
  detected for the compiler and standard of the code that includes them.
- **Platform features** (threads, files, processes, sockets and the rest)
  are compiled into the library. What the system provides is detected once,
  when the library is built, and recorded in the installed
  `<cpolyfill/config.h>`. Code built as C99 and code built as C23 see the
  same types.

Public headers are C99 and can be used from C++. The library itself is C11.

## Building

cpolyfill builds with [Meson](https://mesonbuild.com) 1.3 or newer:

```sh
meson setup build
meson compile -C build
meson install -C build
```

This installs the library, the headers in `<prefix>/include/cpolyfill/`
and a pkg-config file, `cpolyfill.pc`. Use `-Ddefault_library=static` for
a static library.

### Using it from another project

From Meson, as an installed library or a subproject:

```meson
cpolyfill_dep = dependency('cpolyfill')
```

From anything else, with pkg-config:

```sh
cc main.c $(pkg-config --cflags --libs cpolyfill)
```

The system libraries cpolyfill needs (pthreads, `dl`, `ws2_32`, `bcrypt`
and so on) come with the dependency, so you do not list them yourself.

### Options

| Option | Default | Description |
| ------ | ------- | ----------- |
| `threads` | `auto` | Thread support. Without it, creating a thread fails at run time. |
| `test` | `auto` | The unit-test runner, `<cpolyfill/test.h>`. The library's own tests need it. |
| `argparse` | `auto` | Command line parsing, `<cpolyfill/argparse.h>`. |
| `cli` | `auto` | Command line interfaces, `<cpolyfill/cli.h>`. |
| `daemon` | `auto` | Background services, `<cpolyfill/daemon.h>`. |
| `poller` | `auto` | Waiting for sockets, pipes, timers and processes, `<cpolyfill/poller.h>`. |
| `watch` | `auto` | File and directory change notifications, `<cpolyfill/watch.h>`. Needs `poller`. |
| `tests` | `true` | Build the tests (ignored when cpolyfill is a subproject). |
| `known_bugs` | `false` | Run the tests of known defects instead of reporting them as TODO. |

## Conventions

- **Names.** Functions are `pf_snake_case`, types `pf_name_t`, macros
  `PF_UPPER_CASE`. Names starting with `pf__` or `PF__` are internal.
- **Errors.** `pf_*` functions return `int`: `0` on success, a negative
  `PF_E*` code on failure (`pf_strerror` describes it). Functions that stand
  in for a standard function keep that function's convention, such as `-1`
  and `errno`.
- **Strings and paths** are UTF-8 on every platform. On Windows the
  library converts them to UTF-16 for the wide API, and supports long paths.
- **Memory.** Functions that allocate take an optional
  `allocator_t *`; `NULL` means the library's default. Functions
  that stand in for standard ones (`pf_strdup`, `pf_asprintf`,
  `pf_getline`, ...) return memory to release with `free`.
- **Detection macros** in `<cpolyfill/platform.h>` (`PF_OS_*`, `PF_CC_*`,
  `PF_ARCH_*`, `PF_STD_*`, ...) are always defined, to `0` or `1`, so they
  work in `#if` and with `-Wundef`.

### Standard names

The library only exports prefixed names (`pf_strdup`, `pf_thrd_create`,
`pf_dlopen`). To use the standard names instead, define a macro before
including the headers:

| Macro | Effect |
| ----- | ------ |
| `PF_STD_NAMES` | Standard names from every header. |
| `PF_STD_NAMES_<HEADER>` | Standard names from one header, such as `PF_STD_NAMES_STRING`. |
| `PF_NO_STD_NAMES_<HEADER>` | With `PF_STD_NAMES`, leave out one header. |

A standard name is defined only where the system lacks it or where its
behaviour differs, so code written against POSIX or C11 builds unchanged
where those are missing. This includes keywords: `typeof`, `alignas`,
`static_assert`, `thread_local` and the others are always available as
`PF_TYPEOF`, `PF_ALIGNAS`, `PF_STATIC_ASSERT`, `PF_THREAD_LOCAL`, ..., and
as their own names only with the opt-in.

## Headers

Each header is documented in a comment at its top.

### Language and compiler

| Header | Description |
| ------ | ----------- |
| [platform.h](include/cpolyfill/platform.h) | Operating system, compiler, architecture and language standard detection. |
| [stdc.h](include/cpolyfill/stdc.h) | Keywords of newer C standards (`typeof`, `static_assert`, `alignas`, `nullptr`, ...) in every standard and in C++. |
| [attribute.h](include/cpolyfill/attribute.h) | Function, variable and type attributes in the best form the compiler understands. |
| [types.h](include/cpolyfill/types.h) | Compile-time type checks. |
| [macro.h](include/cpolyfill/macro.h) | Utility macros: arithmetic, preprocessor helpers, `PF_CONTAINER_OF`, ... |
| [assert.h](include/cpolyfill/assert.h) | Assertions with a replaceable failure handler. |
| [debug.h](include/cpolyfill/debug.h) | Breakpoints, debugger detection and backtraces. |
| [bitwise.h](include/cpolyfill/bitwise.h) | Bit operations (`clz`, `popcount`, rotations, powers of two, ...) using builtins where available. |
| [overflow.h](include/cpolyfill/overflow.h) | Integer arithmetic with overflow detection, and saturated arithmetic. |
| [endian.h](include/cpolyfill/endian.h) | Byte swapping, byte order conversion, unaligned loads and stores. |
| [atomic.h](include/cpolyfill/atomic.h) | C11 atomics for every compiler, and waiting on an atomic value. |
| [cpuid.h](include/cpolyfill/cpuid.h) | CPU features available at run time and at compile time. |
| [typeid.h](include/cpolyfill/typeid.h) | Numeric identifiers for the standard C types. |

### C library

| Header | Description |
| ------ | ----------- |
| [ctype.h](include/cpolyfill/ctype.h) | Character classification that ignores the locale. |
| [string.h](include/cpolyfill/string.h) | String functions missing from some C libraries (`strdup`, `strlcpy`, `memmem`, `asprintf`, ...). |
| [stdlib.h](include/cpolyfill/stdlib.h) | General utilities missing from some C libraries (`getline`, `setenv`, `qsort_r`, ...). |
| [unicode.h](include/cpolyfill/unicode.h) | UTF-8, UTF-16 and UTF-32 decoding, encoding, validation and conversion, and `<uchar.h>`. |
| [errno.h](include/cpolyfill/errno.h) | Portable error codes and their descriptions. |
| [time.h](include/cpolyfill/time.h) | Clocks, sleeping and calendar time. |
| [version.h](include/cpolyfill/version.h) | The version of the headers and of the linked library. |

### Operating system

| Header | Description |
| ------ | ----------- |
| [threads.h](include/cpolyfill/threads.h) | C11 threads, plus thread names, affinity, priority, reader-writer locks, spin locks and barriers. |
| [semaphore.h](include/cpolyfill/semaphore.h) | POSIX semaphores, unnamed ones working on macOS too. |
| [fs.h](include/cpolyfill/fs.h) | Files, directories and paths. |
| [dirent.h](include/cpolyfill/dirent.h) | Reading directories. |
| [io.h](include/cpolyfill/io.h) | File descriptors, stdio files, whole files and byte streams. |
| [mman.h](include/cpolyfill/mman.h) | Memory mappings. |
| [dlfcn.h](include/cpolyfill/dlfcn.h) | Loading shared libraries at run time. |
| [process.h](include/cpolyfill/process.h) | Starting programs, waiting for them, and process information. |
| [signal.h](include/cpolyfill/signal.h) | Shutting down cleanly on Ctrl+C, SIGTERM, a closed console window, ... |
| [pipe.h](include/cpolyfill/pipe.h) | Anonymous pipes, and named pipes between processes. |
| [socket.h](include/cpolyfill/socket.h) | BSD sockets with one behaviour on every platform, Unix domain sockets on Windows too. |
| [random.h](include/cpolyfill/random.h) | The system's secure random generator, and a fast one for everything else. |
| [sysinfo.h](include/cpolyfill/sysinfo.h) | Processors, memory, the operating system and the host name. |
| [term.h](include/cpolyfill/term.h) | Terminal detection, size, raw input and colour support. |
| [poller.h](include/cpolyfill/poller.h) | An event loop's core: waiting for sockets, pipes, timers, processes and shutdown requests at once (epoll, kqueue, IOCP) (option `poller`). |
| [watch.h](include/cpolyfill/watch.h) | Notifications of changes to files and directory trees (inotify, kqueue, ReadDirectoryChangesW) (option `watch`). |

### Utilities

| Header | Description |
| ------ | ----------- |
| [exception.h](include/cpolyfill/exception.h) | Exceptions on `setjmp` / `longjmp`, shareable between libraries. |
| [argparse.h](include/cpolyfill/argparse.h) | Command line parsing (option `argparse`). |
| [cli.h](include/cpolyfill/cli.h) | Styled output, help text, prompts, spinners and progress bars (option `cli`). |
| [daemon.h](include/cpolyfill/daemon.h) | Background services: detaching, single-instance lock, PID file, systemd notifications (option `daemon`). |
| [test.h](include/cpolyfill/test.h) | A unit-test runner with process isolation, timeouts and TAP output (option `test`). |

## Testing

```sh
meson setup build
meson test -C build
```

The suites in [test/](test) are built several times, because what the
headers do depends on the standard of the code that includes them:

| Tests | What they check |
| ----- | --------------- |
| `cpolyfill/<suite>` | The project's default standard (`gnu11`). |
| `cpolyfill-gnu99/<suite>` | The oldest supported standard. |
| `cpolyfill-gnu23/<suite>` | The newest standard, where the compiler has it. |
| `cpolyfill/cxx11`, `cpolyfill/cxx20` | The headers used from C++. |
| `cpolyfill/headers` | Every header compiles on its own in each standard and in C++, and links when included in two translation units. |
| `cpolyfill/errors` | Misuse of the compile-time checks does not compile. |

The test program can also be run directly:

```sh
build/test/cpolyfill-test-default --list               # suites and tests
build/test/cpolyfill-test-default atomic               # one suite
build/test/cpolyfill-test-default atomic:wait --no-isolate  # one test
```

Tests that need something the system does not have (sockets in a sandbox,
for example) are skipped, not failed. Tests of known, unfixed defects are
registered with `KNOWN_BUG(...)` and reported as TODO; configure with
`-Dknown_bugs=true` to run them.

Sanitizer builds are useful too:

```sh
CC=clang meson setup build-asan -Db_sanitize=address,undefined -Db_lundef=false
meson test -C build-asan
```

## License

[Apache License 2.0](LICENSE).
