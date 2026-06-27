/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides Win32 implementation of POSIX <semaphore.h> interface.

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

#ifndef PF_SEMAPHORE_H
#define PF_SEMAPHORE_H

#ifndef PF_API
    #define PF_API static inline
#endif

#if defined(_WIN32) || defined(_WIN64)
    #define PF_SEMAPHORE_WIN32
#elif !defined(PF_SEMAPHORE_NO_POSIX)
    #define PF_SEMAPHORE_POSIX
#else
    #define PF_SEMAPHORE_NONE
#endif

#ifdef PF_SEMAPHORE_POSIX
    #include <semaphore.h>
#elif defined(PF_SEMAPHORE_WIN32)

    #define WIN32_LEAN_AND_MEAN
    #include <errno.h>
    #include <limits.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <windows.h>

    #ifdef __cplusplus
extern "C" {
    #endif

    #ifndef O_CREAT
        #define O_CREAT 0x0200
    #endif
    #ifndef O_EXCL
        #define O_EXCL 0x0800
    #endif

    #include <stdarg.h>

    /** Sentinel returned by sem_open() on failure (mirrors SEM_FAILED). */
    #define SEM_FAILED ((sem_t *)0)

    /** Maximum value a semaphore count may reach (Win32 limit). */
    #ifndef SEM_VALUE_MAX
        #define SEM_VALUE_MAX INT_MAX
    #endif

enum pf_sem_error {
    PF_SEM_OK = 0,
    PF_SEM_ENOENT = 2,
    PF_SEM_EINTR = 4,
    PF_SEM_EAGAIN = 11,
    PF_SEM_ENOMEM = 12,
    PF_SEM_EACCESS = 13,
    PF_SEM_EEXIST = 17,
    PF_SEM_EINVAL = 22,
    PF_SEM_EMFILE = 24,
    PF_SEM_ENOSYS = 38,
    PF_SEM_EOVERFLOW = 75,
    PF_SEM_ETIMEDOUT = 110,
};

/*
 * For unnamed semaphores the handle is owned by this struct.
 * For named semaphores the handle is shared; sem_close releases our
 * reference without destroying the kernel object.
 */
typedef struct {
    HANDLE handle; /* Win32 semaphore object */
    int named; /* non-zero when opened via sem_open() */
} sem_t;

PF_API int pf__sem_error(int err) {
    errno = err;
    return -1;
}

PF_API sem_t *pf__sem_error_ptr(int err) {
    errno = err;
    return SEM_FAILED;
}

PF_API int pf__sem_error_map(DWORD err) {
    switch (err) {
        /* clang-format off */
    case ERROR_INVALID_HANDLE:      errno = PF_SEM_EINVAL; break;
    case ERROR_ACCESS_DENIED:       errno = PF_SEM_EACCES; break;
    case ERROR_ALREADY_EXISTS:      errno = PF_SEM_EEXIST; break;
    case ERROR_FILE_NOT_FOUND:      errno = PF_SEM_ENOENT; break;
    case ERROR_TOO_MANY_OPEN_FILES: errno = PF_SEM_EMFILE; break;
    case ERROR_NOT_ENOUGH_MEMORY:   errno = PF_SEM_ENOMEM; break;
    case ERROR_INVALID_PARAMETER:   errno = PF_SEM_EINVAL; break;
    default: errno = PF_SEM_EINVAL; break;
        /* clang-format on */
    }
    return -1;
}

/*
 * Convert a POSIX name ("/mysem") to a Win32 kernel-object name.
 *
 * Win32 named objects live in the "Global\" or "Local\" namespace.
 * We prefix with "Local\" so that sandboxed processes can still use
 * named semaphores without requiring elevated privileges.
 *
 * The caller must free() the returned string.
 */
PF_API char *pf__sem_make_win32_name(const char *posix_name) {
    /* Skip any leading slashes required by POSIX */
    while (*posix_name == '/')
        ++posix_name;

    const char prefix[] = "Local\\posix_sem_";
    size_t total = sizeof(prefix) + strlen(posix_name); /* includes NUL */
    char *buf = (char *)malloc(total);

    if (!buf) {
        errno = PF_SEM_ENOMEM;
        return NULL;
    }

    strcpy(buf, prefix);
    strcat(buf, posix_name);
    return buf;
}

PF_API int sem_init(sem_t *sem, int pshared, unsigned int value) {
    if (!sem)
        return pf__sem_error(PF_SEM_EINVAL);
    if (pshared)
        return pf__sem_error(PF_SEM_ENOSYS);
    if ((long)value > (long)SEM_VALUE_MAX)
        return pf__sem_error(PF_SEM_EINVAL);

    sem->handle = CreateSemaphoreA(
        NULL, /* default security */
        (LONG)value, /* initial count */
        (LONG)SEM_VALUE_MAX, /* maximum count */
        NULL /* unnamed */
    );

    if (!sem->handle)
        return pf__sem_error_map(GetLastError());

    sem->named = 0;
    return PF_SEM_OK;
}

PF_API int sem_destroy(sem_t *sem) {
    if (!sem || !sem->handle || sem->named)
        return pf__sem_error(PF_SEM_EINVAL);
    if (!CloseHandle(sem->handle))
        return pf__sem_error_map(GetLastError());
    sem->handle = NULL;
    return PF_SEM_OK;
}

PF_API sem_t *sem_open(const char *name, int oflag, ...) {
    if (!name || !*name)
        return pf__sem_error_ptr(PF_SEM_EINVAL);

    char *win32_name;
    sem_t *sem;

    if (!(win32_name = pf__sem_make_win32_name(name)))
        return SEM_FAILED;

    if (!(sem = malloc(sizeof(sem_t)))) {
        free(win32_name);
        return pf__sem_error_ptr(PF_SEM_ENOMEM);
    }

    HANDLE h = NULL;

    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        (void)va_arg(ap, unsigned int); /* mode – ignored on Windows */
        unsigned int value = va_arg(ap, unsigned int);
        va_end(ap);

        if ((long)value > (long)SEM_VALUE_MAX) {
            free(win32_name);
            free(sem);
            return pf__sem_error_ptr(PF_SEM_EINVAL);
        }

        h = CreateSemaphoreA(
            NULL, (LONG)value, (LONG)SEM_VALUE_MAX, win32_name
        );

        if (h && (oflag & O_EXCL) && GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(h);
            free(win32_name);
            free(sem);
            return pf__sem_error_ptr(PF_SEM_EEXIST);
        }
    } else {
        /* Open existing semaphore */
        h = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, win32_name);
    }

    free(win32_name);

    if (!h) {
        DWORD err = GetLastError();
        free(sem);
        pf__sem_error_map(err);
        return SEM_FAILED;
    }

    sem->handle = h;
    sem->named = 1;
    return sem;
}

PF_API int sem_close(sem_t *sem) {
    if (!sem || !sem->handle || !sem->named)
        return pf__sem_error(PF_SEM_EINVAL);
    if (!CloseHandle(sem->handle))
        return pf__sem_error_map(GetLastError());
    sem->handle = NULL;
    free(sem);
    return PF_SEM_OK;
}

PF_API int sem_unlink(const char *name) {
    if (!name || !*name)
        return pf__sem_error(PF_SEM_EINVAL);

    char *win32_name;

    if (!(win32_name = pf__sem_make_win32_name(name)))
        return -1;

    /*
     * Verify the semaphore actually exists; we cannot truly "delete" a
     * Win32 named object while handles are open (the OS does it for us),
     * but we should return ENOENT if it was never created.
     */
    HANDLE h = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, win32_name);
    free(win32_name);

    if (!h) {
        DWORD err = GetLastError();
        int e = (err == ERROR_FILE_NOT_FOUND) ? PF_SEM_ENOENT : PF_SEM_EACCES;
        return pf__sem_error(e);
    }

    CloseHandle(h);
    return PF_SEM_OK;
}

static inline int sem_wait(sem_t *sem) {
    if (!sem || !sem->handle)
        return pf__sem_error(PF_SEM_EINVAL);

    DWORD rc = WaitForSingleObjectEx(sem->handle, INFINITE, FALSE);
    switch (rc) {
    case WAIT_OBJECT_0:
        return PF_SEM_OK;
    case WAIT_ABANDONED:
        /* Another thread terminated while holding the semaphore. */
        return pf__sem_error(PF_SEM_EINVAL);
    case WAIT_IO_COMPLETION:
        /* Alertable wait interrupted */
        return pf__sem_error(PF_SEM_EINTR);
    default:
        return pf__sem_error_map(GetLastError());
    }
}

PF_API int sem_trywait(sem_t *sem) {
    if (!sem || !sem->handle)
        return pf__sem_error(PF_SEM_EINVAL);

    DWORD rc = WaitForSingleObject(sem->handle, 0);
    switch (rc) {
    case WAIT_OBJECT_0:
        return PF_SEM_OK;
    case WAIT_TIMEOUT:
        return pf__sem_error(PF_SEM_EAGAIN);
    default:
        return pf__sem_error_map(GetLastError());
    }
}

PF_API DWORD pf__sem_timeout(const struct timespec *abs_timeout) {
    /* Compute remaining milliseconds from now until abs_timeout. */
    FILETIME ft_now_ft;
    GetSystemTimeAsFileTime(&ft_now_ft);

    /* FILETIME epoch: 1601-01-01; UNIX epoch: 1970-01-01 */
    const uint64_t EPOCH_DELTA_100NS = UINT64_C(116444736000000000);

    uint64_t now_100ns = ((uint64_t)ft_now_ft.dwHighDateTime << 32)
        | ft_now_ft.dwLowDateTime;
    /* Convert to UNIX time in 100-ns units */
    now_100ns -= EPOCH_DELTA_100NS;

    /* abs_timeout in 100-ns units since UNIX epoch */
    uint64_t deadline_100ns = (uint64_t)abs_timeout->tv_sec * 10000000ULL
        + (uint64_t)(abs_timeout->tv_nsec / 100);

    DWORD timeout_ms;
    if (deadline_100ns <= now_100ns) {
        timeout_ms = 0; /* already expired — still do a trywait */
    } else {
        uint64_t delta_100ns = deadline_100ns - now_100ns;
        uint64_t delta_ms = delta_100ns / 10000ULL;
        /* Clamp to just below INFINITE (0xFFFFFFFE) */
        timeout_ms = (delta_ms >= (uint64_t)INFINITE) ? (INFINITE - 1)
                                                      : (DWORD)delta_ms;
    }

    return timeout_ms;
}

PF_API int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    if (!sem || !sem->handle || !abs_timeout)
        return pf__sem_error(PF_SEM_EINVAL);
    if (abs_timeout->tv_nsec < 0 || abs_timeout->tv_nsec >= 1000000000L)
        return pf__sem_error(PF_SEM_EINVAL);

    DWORD timeout_ms = pf__sem_timeout(abs_timeout);

    DWORD rc = WaitForSingleObject(sem->handle, timeout_ms);
    switch (rc) {
    case WAIT_OBJECT_0:
        return PF_SEM_OK;
    case WAIT_TIMEOUT:
        return pf__sem_error(PF_SEM_ETIMEDOUT);
    default:
        return pf__sem_error_map(GetLastError());
    }
}

PF_API int sem_post(sem_t *sem) {
    if (!sem || !sem->handle)
        return pf__sem_error(EINVAL);

    if (!ReleaseSemaphore(sem->handle, 1, NULL)) {
        DWORD err = GetLastError();
        return pf__sem_error(
            err == ERROR_TOO_MANY_POSTS ? PF_SEM_EOVERFLOW : PF_SEM_EINVAL
        );
    }
    return PF_SEM_OK;
}

/**
 * Implementation note: Win32 has no direct "query count" API.  We perform
 * a zero-timeout wait to obtain the previous count from ReleaseSemaphore,
 * then immediately restore the count.  This involves two atomic Win32
 * operations and is therefore not strictly atomic with respect to other
 * threads, but it is the best achievable without a custom kernel driver.
 *
 * If the count is 0, *sval is set to 0 (or a negative number representing
 * the number of waiters — POSIX allows but does not require the latter;
 * we return 0 for simplicity because Win32 gives us no waiter count).
 */
PF_API int sem_getvalue(sem_t *sem, int *sval) {
    if (!sem || !sem->handle || !sval)
        return pf__sem_error(PF_SEM_EINVAL);

    /* Try to decrement with zero timeout to get the previous count. */
    LONG prev_count = 0;
    DWORD rc = WaitForSingleObject(sem->handle, 0);

    if (rc == WAIT_OBJECT_0) {
        /* We acquired the semaphore; release it back to get the count. */
        if (!ReleaseSemaphore(sem->handle, 1, &prev_count))
            return pf__sem_error_map(GetLastError());
        *sval = (int)(prev_count + 1);
    } else if (rc == WAIT_TIMEOUT) {
        /* Count was 0 (or we lost a race; best effort). */
        *sval = 0;
    } else {
        return pf__sem_error_map(GetLastError());
    }

    return PF_SEM_OK;
}

    #ifdef __cplusplus
} /* extern "C" */
    #endif

#else

    #ifdef __cplusplus
extern "C" {
    #endif

typedef char sem_t;

    #ifndef O_CREAT
        #define O_CREAT 0x0200
    #endif
    #ifndef O_EXCL
        #define O_EXCL 0x0800
    #endif

    #include <stdarg.h>

    #define SEM_FAILED ((sem_t *)0)

    #ifndef SEM_VALUE_MAX
        #define SEM_VALUE_MAX 0
    #endif

int sem_close(sem_t *) { return -1; }
int sem_destroy(sem_t *) { return -1; }
int sem_getvalue(sem_t *restrict, int *restrict) { return -1; }
int sem_init(sem_t *, int, unsigned) { return -1; }
sem_t *sem_open(const char *, int, ...) { return NULL; }
int sem_post(sem_t *) { return -1; }
int sem_trywait(sem_t *) { return -1; }
int sem_unlink(const char *) { return -1; }
int sem_wait(sem_t *) { return -1; }
int sem_timedwait(sem_t *restrict, const struct timespec *restrict) {
    return -1;
}

    #ifdef __cplusplus
} /* extern "C" */
    #endif

#endif

#endif
