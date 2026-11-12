/** cpolyfill - Filling the gaps between C standards and compilers

    Shutting down cleanly when asked to: Ctrl+C, SIGTERM, a closed
    terminal or console window, logging off.

    The handlers only record the request; the program notices it when it
    checks, waits, or polls a file descriptor (POSIX) or event handle
    (Windows) in its event loop.

        int pf_shutdown_init(unsigned flags)
                Installs the handlers (again with new flags if called
                again): 0 or a PF_E* code.
                POSIX: SIGINT and SIGTERM, and SIGHUP with
                PF_SHUTDOWN_HUP. Interrupted system calls are restarted
                (SA_RESTART), so blocking reads do not end early: wait on
                pf_shutdown_fd to notice the request.
                Windows: SetConsoleCtrlHandler, for Ctrl+C (PF_SIGINT),
                Ctrl+Break (PF_SIGBREAK), closing the console window
                (PF_SIGHUP, always handled), logoff and shutdown
                (PF_SIGTERM). For the last three the handler waits until
                the program calls pf_shutdown_done, then lets it end;
                Windows ends it after a few seconds anyway.
                With PF_SHUTDOWN_FORCE_SECOND, a second Ctrl+C (or
                termination signal) while a request is pending ends the
                program at once, as it would without a handler.
        bool pf_shutdown_requested(void)
        int pf_shutdown_signal(void)
                The signal that asked first (PF_SIGINT, ...), 0 if none.
        int pf_shutdown_wait(const struct timespec *deadline)
                Blocks until a request: 0, or PF_ETIMEDOUT once the
                absolute UTC `deadline` (NULL: none) passes.
        int pf_shutdown_request(int sig)
                Asks for a shutdown from the program itself (any thread),
                as if `sig` (not 0) had arrived: 0 or PF_EINVAL.
        void pf_shutdown_reset(void)
                Forgets the request, to wait for the next one.
        int pf_shutdown_fd(void)
                POSIX: a descriptor that is readable while a request is
                pending (never read from it), for poll / select / epoll;
                -1 on Windows or on failure.
        void *pf_shutdown_handle(void)
                Windows: an event HANDLE, signalled while a request is
                pending, for WaitForMultipleObjects; NULL elsewhere.
        void pf_shutdown_done(void)
                The program has cleaned up: on Windows, lets a console
                close, logoff or shutdown proceed. Nothing elsewhere.
        The functions other than pf_shutdown_init work without it (only
        pf_shutdown_request then asks).

        PF_SIGHUP, PF_SIGINT, PF_SIGTERM, PF_SIGBREAK
                1, 2, 15 and 21: the values of SIGHUP, SIGINT and SIGTERM
                on POSIX systems and of SIGINT, SIGTERM and SIGBREAK in the
                Windows CRT.
        const char *pf_signal_name(int sig)
                "SIGINT", "SIGSEGV", ... for the system's signal numbers,
                NULL for unknown ones.

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

#ifndef CPOLYFILL_SIGNAL_H
#define CPOLYFILL_SIGNAL_H

#include <cpolyfill/platform.h>
#include <cpolyfill/time.h>

#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF_SIGHUP 1
#define PF_SIGINT 2
#define PF_SIGTERM 15
#define PF_SIGBREAK 21

#define PF_SHUTDOWN_HUP 0x1u
#define PF_SHUTDOWN_FORCE_SECOND 0x2u

PF_BEGIN_DECLS

PF_API int pf_shutdown_init(unsigned flags);
PF_API bool pf_shutdown_requested(void);
PF_API int pf_shutdown_signal(void);
PF_API int pf_shutdown_wait(const struct timespec *deadline);
PF_API int pf_shutdown_request(int sig);
PF_API void pf_shutdown_reset(void);
PF_API int pf_shutdown_fd(void);
PF_API void *pf_shutdown_handle(void);
PF_API void pf_shutdown_done(void);
PF_API const char *pf_signal_name(int sig);

PF_END_DECLS

#endif /* CPOLYFILL_SIGNAL_H */
