/** cpolyfill - Filling the gaps between C standards and compilers

    Debugging helpers.

        void pf_debugbreak(void);
                                Stops in the debugger, as a breakpoint
                                would. Without a debugger the process is
                                usually terminated (SIGTRAP on POSIX).
        bool pf_debugger_attached(void);
                                Whether a debugger (or another tracer, such
                                as strace) is attached to the process.
        int pf_backtrace(void **frames, int max);
                                Stores up to `max` return addresses of the
                                calling thread, the caller of pf_backtrace
                                first, and returns how many were stored, or
                                PF_ENOSYS where not supported.
        void pf_backtrace_print(FILE *out);
                                Prints the backtrace of the calling thread
                                to `out` (stderr if NULL), one frame per
                                line, with symbol names where available.
                                Symbol names are best effort: on Linux, only
                                functions exported from their module (link
                                with `-rdynamic` to export all); on Windows
                                the module and offset only.

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

#ifndef CPOLYFILL_DEBUG_H
#define CPOLYFILL_DEBUG_H

#include <cpolyfill/platform.h>

#include <stdio.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#if PF_HAS_BUILTIN(__builtin_debugtrap)
    #define PF__DEBUGBREAK() __builtin_debugtrap()
#elif PF_OS_WINDOWS && (PF_CC_MSVC || PF_CC_MINGW)
    #include <intrin.h>
    #define PF__DEBUGBREAK() __debugbreak()
#elif PF_CC_GNU && (PF_ARCH_X86 || PF_ARCH_X86_64)
    #define PF__DEBUGBREAK() __asm__ __volatile__("int3")
#elif PF_CC_GNU && PF_ARCH_ARM64
    #define PF__DEBUGBREAK() __asm__ __volatile__("brk #0xf000")
#elif PF_OS_POSIX
    #include <signal.h>
    #define PF__DEBUGBREAK() raise(SIGTRAP)
#else
    #include <stdlib.h>
    #define PF__DEBUGBREAK() abort()
#endif

PF_BEGIN_DECLS

PF_INLINE void pf_debugbreak(void) { PF__DEBUGBREAK(); }

PF_API bool pf_debugger_attached(void);

PF_API int pf_backtrace(void **frames, int max);

PF_API void pf_backtrace_print(FILE *out);

PF_END_DECLS

#endif /* CPOLYFILL_DEBUG_H */
