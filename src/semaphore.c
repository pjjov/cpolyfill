/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/semaphore.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#elif PF__SEM_NATIVE || (PF_OS_APPLE && PF_HAS_THREADS)
    #include <semaphore.h>
#endif

#ifndef PF__HAVE_SEM_CLOCKWAIT
    #define PF__HAVE_SEM_CLOCKWAIT 0
#endif

static int fail(int err) {
    errno = err;
    return -1;
}

/* The UTC deadline matching a deadline on `clock`. */
static int utc_deadline(
    pf_clockid_t clock, const struct timespec *deadline, struct timespec *out
) {
    struct timespec now, utc;

    if (clock == PF_CLOCK_REALTIME) {
        *out = *deadline;
        return 0;
    }
    if (clock != PF_CLOCK_MONOTONIC || pf_clock_gettime(clock, &now)
        || !pf_timespec_get(&utc, PF_TIME_UTC))
        return -1;
    *out = pf_timespec_add(utc, pf_timespec_sub(*deadline, now));
    return 0;
}

static int valid_deadline(const struct timespec *d) {
    return d && d->tv_nsec >= 0 && d->tv_nsec < 1000000000L;
}

/* ---- The library's own semaphore --------------------------------------- */

int pf__sem_local_init(struct pf__sem_local *s, unsigned value) {
    if (value > PF_SEM_VALUE_MAX)
        return fail(EINVAL);
    if (pf_mtx_init(&s->mtx, pf_mtx_plain) != pf_thrd_success)
        return fail(ENOMEM);
    if (pf_cnd_init(&s->cnd) != pf_thrd_success) {
        pf_mtx_destroy(&s->mtx);
        return fail(ENOMEM);
    }
    s->value = value;
    return 0;
}

int pf__sem_local_destroy(struct pf__sem_local *s) {
    pf_cnd_destroy(&s->cnd);
    pf_mtx_destroy(&s->mtx);
    return 0;
}

/* `deadline` is UTC, NULL to wait forever. */
int pf__sem_local_wait(
    struct pf__sem_local *s, const struct timespec *deadline
) {
    int rc = pf_thrd_success;

    if (deadline && !valid_deadline(deadline))
        return fail(EINVAL);
    pf_mtx_lock(&s->mtx);
    while (s->value == 0 && rc == pf_thrd_success) {
        rc = deadline ? pf_cnd_timedwait(&s->cnd, &s->mtx, deadline)
                      : pf_cnd_wait(&s->cnd, &s->mtx);
    }
    if (s->value > 0) {
        s->value--;
        rc = pf_thrd_success;
    }
    pf_mtx_unlock(&s->mtx);
    switch (rc) {
    case pf_thrd_success: return 0;
    case pf_thrd_timedout: return fail(ETIMEDOUT);
    default: return fail(PF_HAS_THREADS ? EINVAL : EDEADLK);
    }
}

int pf__sem_local_trywait(struct pf__sem_local *s) {
    int ok;
    pf_mtx_lock(&s->mtx);
    ok = s->value > 0;
    if (ok)
        s->value--;
    pf_mtx_unlock(&s->mtx);
    return ok ? 0 : fail(EAGAIN);
}

int pf__sem_local_post(struct pf__sem_local *s) {
    int ok;
    pf_mtx_lock(&s->mtx);
    ok = s->value < PF_SEM_VALUE_MAX;
    if (ok) {
        s->value++;
        pf_cnd_signal(&s->cnd);
    }
    pf_mtx_unlock(&s->mtx);
    return ok ? 0 : fail(EOVERFLOW);
}

int pf__sem_local_getvalue(struct pf__sem_local *s, int *value) {
    pf_mtx_lock(&s->mtx);
    *value = (int)s->value;
    pf_mtx_unlock(&s->mtx);
    return 0;
}

/* ---- The system's semaphores (Linux, BSDs) ----------------------------- */

#if PF__SEM_NATIVE

PF_API int pf_sem_init(pf_sem_t *sem, int pshared, unsigned value) {
    if (value > PF_SEM_VALUE_MAX)
        return fail(EINVAL);
    return sem_init(sem, pshared, value);
}

PF_API int pf_sem_destroy(pf_sem_t *sem) {
    return sem_destroy(sem);
}

PF_API int pf_sem_wait(pf_sem_t *sem) {
    return sem_wait(sem);
}

PF_API int pf_sem_trywait(pf_sem_t *sem) {
    return sem_trywait(sem);
}

PF_API int pf_sem_timedwait(pf_sem_t *sem, const struct timespec *deadline) {
    return sem_timedwait(sem, deadline);
}

PF_API int pf_sem_clockwait(
    pf_sem_t *sem, pf_clockid_t clock, const struct timespec *deadline
) {
    struct timespec utc;

    if (!valid_deadline(deadline))
        return fail(EINVAL);
    #if PF__HAVE_SEM_CLOCKWAIT
    if (clock == PF_CLOCK_MONOTONIC)
        return sem_clockwait(sem, CLOCK_MONOTONIC, deadline);
    #endif
    if (utc_deadline(clock, deadline, &utc))
        return fail(EINVAL);
    return sem_timedwait(sem, &utc);
}

PF_API int pf_sem_post(pf_sem_t *sem) {
    return sem_post(sem);
}

PF_API int pf_sem_getvalue(pf_sem_t *sem, int *value) {
    return sem_getvalue(sem, value);
}

PF_API pf_sem_t *pf_sem_open(const char *name, int oflag, ...) {
    unsigned mode = 0, value = 0;

    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, unsigned);
        value = va_arg(ap, unsigned);
        va_end(ap);
        if (value > PF_SEM_VALUE_MAX) {
            errno = EINVAL;
            return PF_SEM_FAILED;
        }
        return sem_open(name, oflag, (mode_t)mode, value);
    }
    return sem_open(name, oflag);
}

PF_API int pf_sem_close(pf_sem_t *sem) {
    return sem_close(sem);
}

PF_API int pf_sem_unlink(const char *name) {
    return sem_unlink(name);
}

/* ---- Our own unnamed ones, the system's named ones (macOS, Windows) ---- */

#else

/* "/name", without other slashes. */
static int valid_name(const char *name) {
    return name && name[0] == '/' && name[1] && !strchr(name + 1, '/');
}

    #if PF_OS_WINDOWS
        #define NAMED(sem) ((HANDLE)(sem)->pf__named)

/* "/name" -> "Local\name", in UTF-16. */
static int win32_name(const char *name, pf__wstr *out) {
    size_t n = strlen(name);
    char *local = (char *)malloc(n + 6);
    int rc;
    if (!local)
        return PF_ENOMEM;
    memcpy(local, "Local\\", 6);
    memcpy(local + 6, name + 1, n); /* with the NUL */
    rc = pf__wstr_from_utf8(out, local);
    free(local);
    return rc;
}

static DWORD ms_until(const struct timespec *deadline) {
    struct timespec now;
    int64_t ns;
    if (!pf_timespec_get(&now, PF_TIME_UTC))
        return 0;
    ns = pf_timespec_to_ns(pf_timespec_sub(*deadline, now));
    if (ns <= 0)
        return 0;
    if (ns / 1000000 >= INFINITE - 1)
        return INFINITE - 1;
    return (DWORD)((ns + 999999) / 1000000);
}
    #elif PF_OS_APPLE && PF_HAS_THREADS
        #define NAMED(sem) ((sem_t *)(sem)->pf__named)
    #endif

PF_API int pf_sem_init(pf_sem_t *sem, int pshared, unsigned value) {
    if (!sem)
        return fail(EINVAL);
    if (pshared)
        return fail(ENOTSUP);
    sem->pf__named = NULL;
    return pf__sem_local_init(&sem->pf__local, value);
}

PF_API int pf_sem_destroy(pf_sem_t *sem) {
    if (!sem || sem->pf__named)
        return fail(EINVAL);
    return pf__sem_local_destroy(&sem->pf__local);
}

PF_API int pf_sem_wait(pf_sem_t *sem) {
    if (!sem)
        return fail(EINVAL);
    if (!sem->pf__named)
        return pf__sem_local_wait(&sem->pf__local, NULL);
    #if PF_OS_WINDOWS
    if (WaitForSingleObject(NAMED(sem), INFINITE) != WAIT_OBJECT_0)
        return fail(EINVAL);
    return 0;
    #elif defined(NAMED)
    return sem_wait(NAMED(sem));
    #else
    return fail(EINVAL);
    #endif
}

PF_API int pf_sem_trywait(pf_sem_t *sem) {
    if (!sem)
        return fail(EINVAL);
    if (!sem->pf__named)
        return pf__sem_local_trywait(&sem->pf__local);
    #if PF_OS_WINDOWS
    switch (WaitForSingleObject(NAMED(sem), 0)) {
    case WAIT_OBJECT_0: return 0;
    case WAIT_TIMEOUT: return fail(EAGAIN);
    default: return fail(EINVAL);
    }
    #elif defined(NAMED)
    return sem_trywait(NAMED(sem));
    #else
    return fail(EINVAL);
    #endif
}

PF_API int pf_sem_timedwait(pf_sem_t *sem, const struct timespec *deadline) {
    if (!sem || !valid_deadline(deadline))
        return fail(EINVAL);
    if (!sem->pf__named)
        return pf__sem_local_wait(&sem->pf__local, deadline);
    #if PF_OS_WINDOWS
    for (;;) {
        struct timespec now;
        switch (WaitForSingleObject(NAMED(sem), ms_until(deadline))) {
        case WAIT_OBJECT_0: return 0;
        case WAIT_TIMEOUT: break;
        default: return fail(EINVAL);
        }
        /* The wait is in whole milliseconds: make sure the time is up. */
        if (pf_timespec_get(&now, PF_TIME_UTC)
            && pf_timespec_cmp(now, *deadline) >= 0)
            return fail(ETIMEDOUT);
    }
    #elif defined(NAMED)
    {
        /* macOS has no sem_timedwait: poll, from 50 us up to 1 ms. */
        long wait_ns = 50000;
        for (;;) {
            struct timespec now, nap;
            if (sem_trywait(NAMED(sem)) == 0)
                return 0;
            if (errno != EAGAIN)
                return -1;
            if (!pf_timespec_get(&now, PF_TIME_UTC))
                return fail(EINVAL);
            if (pf_timespec_cmp(now, *deadline) >= 0)
                return fail(ETIMEDOUT);
            nap = pf_timespec_sub(*deadline, now);
            if (nap.tv_sec > 0 || nap.tv_nsec > wait_ns) {
                nap.tv_sec = 0;
                nap.tv_nsec = wait_ns;
            }
            pf_nanosleep(&nap, NULL);
            if (wait_ns < 1000000)
                wait_ns *= 2;
        }
    }
    #else
    return fail(EINVAL);
    #endif
}

PF_API int pf_sem_clockwait(
    pf_sem_t *sem, pf_clockid_t clock, const struct timespec *deadline
) {
    struct timespec utc;
    if (!valid_deadline(deadline) || utc_deadline(clock, deadline, &utc))
        return fail(EINVAL);
    return pf_sem_timedwait(sem, &utc);
}

PF_API int pf_sem_post(pf_sem_t *sem) {
    if (!sem)
        return fail(EINVAL);
    if (!sem->pf__named)
        return pf__sem_local_post(&sem->pf__local);
    #if PF_OS_WINDOWS
    if (!ReleaseSemaphore(NAMED(sem), 1, NULL))
        return fail(
            GetLastError() == ERROR_TOO_MANY_POSTS ? EOVERFLOW : EINVAL
        );
    return 0;
    #elif defined(NAMED)
    return sem_post(NAMED(sem));
    #else
    return fail(EINVAL);
    #endif
}

PF_API int pf_sem_getvalue(pf_sem_t *sem, int *value) {
    if (!sem || !value)
        return fail(EINVAL);
    if (!sem->pf__named)
        return pf__sem_local_getvalue(&sem->pf__local, value);
    return fail(ENOSYS);
}

PF_API pf_sem_t *pf_sem_open(const char *name, int oflag, ...) {
    unsigned mode = 0, value = 0;
    pf_sem_t *sem;

    if (oflag & PF_O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, unsigned);
        value = va_arg(ap, unsigned);
        va_end(ap);
    }
    if (!valid_name(name) || value > PF_SEM_VALUE_MAX) {
        errno = EINVAL;
        return PF_SEM_FAILED;
    }
    #if PF_OS_WINDOWS
    {
        pf__wstr w;
        HANDLE h;
        int rc = win32_name(name, &w);
        (void)mode;
        if (rc) {
            errno = pf_error_to_errno(rc);
            return PF_SEM_FAILED;
        }
        if (oflag & PF_O_CREAT) {
            h = CreateSemaphoreW(
                NULL, (LONG)value, PF_SEM_VALUE_MAX, (LPCWSTR)w.ptr
            );
            if (h && (oflag & PF_O_EXCL)
                && GetLastError() == ERROR_ALREADY_EXISTS) {
                CloseHandle(h);
                pf__wstr_free(&w);
                errno = EEXIST;
                return PF_SEM_FAILED;
            }
        } else {
            h = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, (LPCWSTR)w.ptr);
        }
        pf__wstr_free(&w);
        if (!h) {
            errno = pf_error_to_errno(pf_error_last_win32());
            return PF_SEM_FAILED;
        }
        if (!(sem = (pf_sem_t *)calloc(1, sizeof(*sem)))) {
            CloseHandle(h);
            errno = ENOMEM;
            return PF_SEM_FAILED;
        }
        sem->pf__named = h;
        return sem;
    }
    #elif defined(NAMED)
    {
        sem_t *s = oflag & PF_O_CREAT
                     ? sem_open(name, oflag, (mode_t)mode, value)
                     : sem_open(name, oflag);
        if (s == SEM_FAILED)
            return PF_SEM_FAILED;
        if (!(sem = (pf_sem_t *)calloc(1, sizeof(*sem)))) {
            sem_close(s);
            errno = ENOMEM;
            return PF_SEM_FAILED;
        }
        sem->pf__named = s;
        return sem;
    }
    #else
    (void)mode, (void)sem;
    errno = ENOSYS;
    return PF_SEM_FAILED;
    #endif
}

PF_API int pf_sem_close(pf_sem_t *sem) {
    if (!sem || !sem->pf__named)
        return fail(EINVAL);
    #if PF_OS_WINDOWS
    CloseHandle(NAMED(sem));
    #elif defined(NAMED)
    sem_close(NAMED(sem));
    #endif
    free(sem);
    return 0;
}

PF_API int pf_sem_unlink(const char *name) {
    if (!valid_name(name))
        return fail(EINVAL);
    #if PF_OS_WINDOWS
    return 0; /* gone with the last handle */
    #elif defined(NAMED)
    return sem_unlink(name);
    #else
    return fail(ENOSYS);
    #endif
}

#endif
