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

# Polyfill for C

Polyfills fill the gaps between different C standards and compilers,
enabling you to write safe and portable code without headache.

Alongside portability, polyfills contain common utilities you may find
in many projects already, improving their safety and standardization.

## Features

If a polyfill detects a missing standard feature, it will attempt to replace it.
This means you can code with whatever features you need and polyfills will
find the best implementation for them.

Polyfills define macros with which you can test feature availability. You can
also control polyfill's behaviour by defining macros. Information on these
macros can be found at the top of the header file.

## Building

Polyfills are header-only. Just copy and include them in your project. Some
polyfills can gain additional behaviour and safety if other polyfills are
included alongside it.

## Testing

Every header has a test suite in [test/](./test). They are built with
[Meson](https://mesonbuild.com) and report with the Test Anything Protocol:

```sh
meson setup build
meson test -C build
```

The same tests are built several times, because what a header does depends on
the language standard and on which features the system lacks:

| Variant | Purpose |
| ------- | ------- |
| `cpolyfill/<suite>` | The compiler's default standard, using system features where they exist. |
| `cpolyfill-gnu99/<suite>` | `-std=gnu99`, so threads, atomics and `static_assert` come from the polyfills. |
| `cpolyfill-atomic-fallback/atomic` | Atomics implemented with mutexes (`PF_ATOMIC_NONE`). |
| `cpolyfill/headers` | Each header compiles on its own, and links when used in two translation units. |

The test program can also be run directly, which is handy on a machine where
Meson is not available (`cc test/*.c -lpthread -ldl`):

```sh
build/cpolyfill-test-default --list          # suites and tests
build/cpolyfill-test-default atomic          # one suite
build/cpolyfill-test-default atomic:/atomic/cas_int --no-fork
PF_TEST_VERBOSE=1 build/cpolyfill-test-default   # say why tests are skipped
```

Tests that need something the platform does not have (sockets in a sandbox,
unnamed semaphores on macOS, ...) are skipped, not failed.

Tests for defects that are known but not fixed yet are listed as `TODO` (there are none right now). Run
`meson configure build -Dknown_bugs=true` to run them, and watch them fail.
Each is registered with `KNOWN_BUG(...)`, so fixing a header means deleting
that wrapper. The same goes for the `KNOWN` table in
[test/check_headers.py](./test/check_headers.py).

## List of polyfills

| Header | Description |
| ------ | ----------- |
| [pf_argparse.h](./include/pf_argparse.h) | Minimal argument parsing alternative to `getopt`. |
| [pf_array.h](./include/pf_array.h)       | Simple implementation of generic growable arrays. |
| [pf_assert.h](./include/pf_assert.h)     | Provides the `assert` macro and much more for better debugging and testing. |
| [pf_atomic.h](./include/pf_atomic.h)     | C11's atomic instructions for existing compiler builtins. |
| [pf_attribute.h](./include/pf_attribute.h) | Macros for annotating functions, variables and types using compiler and C23 attributes. |
| [pf_bitwise.h](./include/pf_bitwise.h)   | Provides cross platform implementations of `clz`, `popcnt`, `round_pow2` and others. |
| [pf_cli.h](./include/pf_cli.h)           | Functions for building command line interfaces. |
| [pf_cpuinfo.h](./include/pf_cpuinfo.h)   | Gives runtime information on CPU features like the number of cores, SIMD capabilities and more. |
| [pf_ctype.h](./include/pf_ctype.h)       | Unlocalized variants of `<ctype.h>` functions and other utilities. |
| [pf_daemon.h](./include/pf_daemon.h)     | Cross-platform daemon/background-process helper functions. |
| [pf_dirent.h](./include/pf_dirent.h)     | Directory traversal using the POSIX `<dirent.h>` interface. |
| [pf_dlsym.h](./include/pf_dlsym.h)       | Shared library loading using the POSIX `<dlsym.h>` interface. |
| [pf_endian.h](./include/pf_endian.h)     | Endianness detection and reordering of bytes. |
| [pf_errno.h](./include/pf_errno.h)       | Compilation of common standard library error codes. |
| [pf_exception.h](./include/pf_exception.h) | Exception handling mechanism for C. |
| [pf_filesystem.h](./include/pf_filesystem.h) | Cross-platform directory operations. |
| [pf_io.h](./include/pf_io.h)       | Custom byte/character stream interface and `<stdio.h>` wrapper. |
| [pf_macro.h](./include/pf_macro.h)       | Contains ubiquitous macro functions and compiler builtins. |
| [pf_overflow.h](./include/pf_overflow.h) | Overflow checks and saturated arithmetic. |
| [pf_pipe.h](./include/pf_pipe.h)         | Cross-platform named pipe interface. |
| [pf_process.h](./include/pf_process.h)     | Process and operating system utilities. |
| [pf_semaphore.h](./include/pf_semaphore.h) | Cross-platform POSIX semaphores. |
| [pf_socket.h](./include/pf_socket.h)     | Cross-platform named socket interface. |
| [pf_string.h](./include/pf_string.h)     | Miscellaneous string utilities. |
| [pf_test.h](./include/pf_test.h)         | Minimal unit testing framework which can be extended with other polyfills. |
| [pf_threads.h](./include/pf_threads.h)   | C11's threading interface for existing libraries. |
| [pf_typeid.h](./include/pf_typeid.h)     | Runtime type information and conversion. |
| [pf_types.h](./include/pf_types.h)       | Alignment and type safety macros. (`typeof`, `container_of`, `ALIGN_UP`). |
