/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a collection of common operating system functions based
    on the POSIX interface. This currently includes sleep and process id tools.

    Last-updated: September 2026
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

#ifndef POLYFILL_SYSTEM
#define POLYFILL_SYSTEM

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #define PF_SYS_WIN32
#elif !defined(PF_NO_POSIX)
    #define PF_SYS_POSIX
#else
    #define PF_SYS_NONE
#endif

#ifdef PF_SYS_POSIX
    #include <unistd.h>
#elif defined(PF_SYS_WIN32)

    #include <windows.h>

PF_API int getpid(void) { return (int)GetCurrentProcessId(); }

PF_API unsigned int sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
    return 0;
}

PF_API int usleep(unsigned int usec) {
    if (usec == 0)
        return 0;

    if (usec < 1000) {
        /* For very short delays, use a high-resolution waitable timer
           or spin, since Sleep(0) just yields the CPU. */
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);

        double target = (double)usec / 1000000.0;
        do {
            QueryPerformanceCounter(&now);
        } while (((double)(now.QuadPart - start.QuadPart) / freq.QuadPart)
                 < target);
    } else {
        Sleep(usec / 1000);
    }

    return 0;
}

#else

PF_API int getpid(void) { return -1; }
PF_API unsigned int sleep(unsigned int seconds) { return 0; }
PF_API int usleep(unsigned int usec) { return -1; }

#endif

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_SYSTEM */
