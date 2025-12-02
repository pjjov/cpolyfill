<!--
SPDX-FileCopyrightText: 2025 Predrag Jovanović
SPDX-License-Identifier: Apache-2.0

Copyright 2025 Predrag Jovanović

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

## List of polyfills

| Header | Description |
| ------ | ----------- |
| [pf_assert.h](./include/pf_assert.h)     | Provides the `assert` macro and much more for better debugging and testing. |
| [pf_bitwise.h](./include/pf_bitwise.h)   | Provides cross platform implementations of `clz`, `popcnt`, `round_pow2` and others. |
| [pf_cpuinfo.h](./include/pf_cpuinfo.h)   | Gives runtime information on CPU features like the number of cores, SIMD capabilities and more. |
| [pf_endian.h](./include/pf_endian.h)     | Endianness detection and reordering of bytes. |
| [pf_macro.h](./include/pf_macro.h)       | Contains ubiquitous macro functions and compiler builtins. |
| [pf_overflow.h](./include/pf_overflow.h) | Overflow checks and saturated arithmetic. |
| [pf_test.h](./include/pf_test.h)         | Minimal unit testing framework which can be extended with other polyfills. |
| [pf_typeid.h](./include/pf_typeid.h)     | Runtime type information and conversion. |
| [pf_types.h](./include/pf_types.h)       | Alignment and type safety macros. (`typeof`, `container_of`, `ALIGN_UP`). |

<!--
| [pf_atomic.h](./include/pf_atomic.h)     | Cross platform implementation of `<stdatomic.h>` |
| [pf_thread.h](./include/pf_thread.h)     | C11 and pthreads interface implementations. |
| [pf_dirent.h](./include/pf_dirent.h)     | Implements `<dirent.h>` on Windows platforms for searching directories. |
| [pf_errno.h](./include/pf_errno.h)       | Error codes from Unix and Windows. |
| [pf_utf8.h](./include/pf_utf8.h)         | UTF8 validating and other utilities. |
-->
