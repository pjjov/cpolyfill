/** cpolyfill - Filling the gaps between C standards and compilers

    POSIX semaphores on every platform, unnamed ones working on macOS too.

        pf_sem_t                PF_SEM_FAILED, PF_SEM_VALUE_MAX
        PF_O_CREAT, PF_O_EXCL   flags for pf_sem_open

        int pf_sem_init(pf_sem_t *sem, int pshared, unsigned value)
        int pf_sem_destroy(pf_sem_t *sem)
        int pf_sem_wait(pf_sem_t *sem)
        int pf_sem_trywait(pf_sem_t *sem)
        int pf_sem_timedwait(pf_sem_t *sem, const struct timespec *deadline)
        int pf_sem_clockwait(pf_sem_t *sem, pf_clockid_t clock,
                             const struct timespec *deadline)
        int pf_sem_post(pf_sem_t *sem)
        int pf_sem_getvalue(pf_sem_t *sem, int *value)
                POSIX: 0, or -1 with errno (EAGAIN from trywait, ETIMEDOUT,
                EINVAL, EOVERFLOW, ...). `deadline` is absolute: UTC for
                timedwait, PF_CLOCK_REALTIME or PF_CLOCK_MONOTONIC for
                clockwait (glibc's sem_clockwait), which lets a wait ignore
                changes to the wall clock where the system can.

        pf_sem_t *pf_sem_open(const char *name, int oflag, ...)
                POSIX sem_open: with PF_O_CREAT, two more arguments, the
                permissions (unsigned) and the initial value (unsigned).
                Returns PF_SEM_FAILED with errno on failure. `name` is
                "/something" without other slashes.
        int pf_sem_close(pf_sem_t *sem)
        int pf_sem_unlink(const char *name)

    On Linux and the BSDs these are the system's semaphores. On macOS and
    Windows, unnamed semaphores are the library's own (`pshared` must be 0,
    else ENOTSUP), and named ones wrap the system's: on macOS their timed
    waits poll and getvalue fails with ENOSYS; on Windows a named semaphore
    disappears with its last handle, getvalue fails with ENOSYS and
    pf_sem_unlink does nothing. Without threads, waits that would block
    forever fail with EDEADLK and named semaphores with ENOSYS.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_SEMAPHORE, where the
    system's semaphores are not used: `sem_t`, `SEM_FAILED`,
    `SEM_VALUE_MAX` and the `sem_*` functions (and on Windows `O_CREAT`,
    `O_EXCL`).

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

#ifndef CPOLYFILL_SEMAPHORE_H
#define CPOLYFILL_SEMAPHORE_H

#include <cpolyfill/io.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#define PF_SEM_VALUE_MAX 2147483647

/* The library's own semaphore (also built where it is not used, for the
   tests). */
struct pf__sem_local {
    pf_mtx_t mtx;
    pf_cnd_t cnd;
    unsigned value;
};

#if PF__THREADS_POSIX && !PF_OS_APPLE
    #define PF__SEM_NATIVE 1
    #include <semaphore.h>
typedef sem_t pf_sem_t;
    #define PF_SEM_FAILED ((pf_sem_t *)SEM_FAILED)
#else
    #define PF__SEM_NATIVE 0
typedef struct pf_sem_t {
    void *pf__named; /* the system's named semaphore, NULL if unnamed */
    struct pf__sem_local pf__local;
} pf_sem_t;
    #define PF_SEM_FAILED ((pf_sem_t *)0)
#endif

PF_BEGIN_DECLS

PF_API int pf_sem_init(pf_sem_t *sem, int pshared, unsigned value);
PF_API int pf_sem_destroy(pf_sem_t *sem);
PF_API int pf_sem_wait(pf_sem_t *sem);
PF_API int pf_sem_trywait(pf_sem_t *sem);
PF_API int pf_sem_timedwait(pf_sem_t *sem, const struct timespec *deadline);
PF_API int pf_sem_clockwait(
    pf_sem_t *sem, pf_clockid_t clock, const struct timespec *deadline
);
PF_API int pf_sem_post(pf_sem_t *sem);
PF_API int pf_sem_getvalue(pf_sem_t *sem, int *value);
PF_API pf_sem_t *pf_sem_open(const char *name, int oflag, ...);
PF_API int pf_sem_close(pf_sem_t *sem);
PF_API int pf_sem_unlink(const char *name);

PF_END_DECLS

#endif /* CPOLYFILL_SEMAPHORE_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__SEMAPHORE_STD_NAMES)                                    \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_SEMAPHORE))   \
        || defined(PF_STD_NAMES_SEMAPHORE))
    #define PF__SEMAPHORE_STD_NAMES

    #if !PF__SEM_NATIVE
        #if PF_OS_APPLE
            /* The system's <semaphore.h> declares the names. */
            #include <semaphore.h>
            #undef SEM_FAILED
            #undef SEM_VALUE_MAX
            #define sem_t pf_sem_t
        #else
typedef pf_sem_t sem_t;
        #endif
        #define SEM_FAILED PF_SEM_FAILED
        #define SEM_VALUE_MAX PF_SEM_VALUE_MAX
        #if PF_OS_WINDOWS
            #ifndef O_CREAT
                #define O_CREAT PF_O_CREAT
            #endif
            #ifndef O_EXCL
                #define O_EXCL PF_O_EXCL
            #endif
        #endif
        #define sem_init pf_sem_init
        #define sem_destroy pf_sem_destroy
        #define sem_wait pf_sem_wait
        #define sem_trywait pf_sem_trywait
        #define sem_timedwait pf_sem_timedwait
        #define sem_clockwait pf_sem_clockwait
        #define sem_post pf_sem_post
        #define sem_getvalue pf_sem_getvalue
        #define sem_open pf_sem_open
        #define sem_close pf_sem_close
        #define sem_unlink pf_sem_unlink
    #endif
#endif
