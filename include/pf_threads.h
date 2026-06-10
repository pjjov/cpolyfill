/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a C11 <threads.h> wrapper for Windows and POSIX
    threads. Additionaly, it can be compiled on systems without threads.

    Alongside C11 objects, this file also provides cross-platform
    reader-writer locks, spin locks and barrier primitives.

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
**/

#ifndef POLYFILL_THREADS
#define POLYFILL_THREADS

#ifndef PF_API
    #define PF_API static inline
#endif

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
    #define PF_THREADS_STD
#endif

#if defined(_WIN32)
    #define PF_THREADS_WIN32
#elif !defined(PF_NO_PTHREAD)
    #define PF_THREADS_POSIX
#else
    #define PF_THREADS_NONE
#endif

#ifndef PF_THREADS_STD
    #define ONCE_FLAG_INIT ((once_flag) { 0 })
    #define TSS_DTOR_ITERATIONS 4
    #define PF__NO_RETURN

enum pf_mtx_type {
    mtx_plain = 0,
    mtx_recursive = 1,
    mtx_timed = 2,
};

enum pf_thrd_error {
    thrd_error = -1,
    thrd_success = 0,
    thrd_busy,
    thrd_nomem,
    thrd_timedout,
};

typedef void (*tss_dtor_t)(void *);
typedef int (*thrd_start_t)(void *);

#endif

#ifdef PF_THREADS_POSIX

    #define PF__PT(name, ...)                                          \
        (pthread_##name(__VA_ARGS__)) != 0 ? thrd_error : thrd_success

    #define PF__PT2(name, obj, ...)                                    \
        (obj) != NULL ? (PF__PT(name, obj __VA_OPT__(, ) __VA_ARGS__)) \
                      : thrd_error

#endif

#ifdef PF_THREADS_STD
    #include <threads.h>
#elif defined(PF_THREADS_POSIX)
    #include <errno.h>
    #include <pthread.h>
    #include <stdint.h>
    #include <stdlib.h>

typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_t thrd_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;

PF_API int mtx_init(mtx_t *mtx, int type) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (type & mtx_recursive)
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    int fail = PF__PT(mutex_init, mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    return fail;
}

struct pf__thrd_wrapper {
    void *arg;
    thrd_start_t func;
};

PF_API void *pf__thrd_wrapper_fn(void *arg) {
    struct pf__thrd_wrapper *w = arg;

    int result = w->func(w->arg);

    free(arg);
    return (void *)(intptr_t)result;
}

PF_API int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    struct pf__thrd_wrapper *w = malloc(sizeof(*w));
    w->arg = arg;
    w->func = func;
    return PF__PT(create, thr, NULL, pf__thrd_wrapper_fn, w);
}

PF_API int thrd_join(thrd_t thr, int *res) {
    void *tmp;
    int fail = pthread_join(thr, &tmp);
    if (res && !fail)
        *res = (int)(intptr_t)tmp;
    return fail ? thrd_error : thrd_success;
}

PF_API int thrd_sleep(
    const struct timespec *duration, struct timespec *remaining
) {
    if (nanosleep(duration, remaining))
        return errno == EINTR ? -1 : -2;
    return 0;
}

PF_API int mtx_timedlock(
    mtx_t *restrict mtx, const struct timespec *restrict ts
) {
    #if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L)  \
        && defined(_POSIX_THREADS) && (_POSIX_THREADS >= 200112L)

    switch (pthread_mutex_timedlock(mtx, ts)) {
        /* clang-format off */
    case 0: return thrd_success;
    case ETIMEDOUT: return thrd_timedout;
    default: return thrd_error;
        /* clang-format on */
    }

    #else

    return thrd_error;

    #endif
}

PF_API int cnd_timedwait(
    cnd_t *restrict cond,
    mtx_t *restrict mtx,
    const struct timespec *restrict ts
) {
    switch (pthread_cond_timedwait(cond, mtx, ts)) {
        /* clang-format off */
    case 0: return thrd_success;
    case ETIMEDOUT: return thrd_timedout;
    default: return thrd_error;
        /* clang-format on */
    }
}

/* clang-format off */

PF_API thrd_t thrd_current(void) { return pthread_self(); }
PF_API int thrd_detach(thrd_t thr) { return PF__PT(detach, thr); }
PF_API void thrd_yield(void) { sched_yield(); }
PF_API int thrd_equal(thrd_t thr0, thrd_t thr1) { return PF__PT(equal, thr0, thr1); }
PF_API PF__NO_RETURN void thrd_exit(int res) { pthread_exit((void *)(intptr_t)res); }
PF_API void call_once(once_flag *flag, void (*func)(void)) { pthread_once(flag, func); }
PF_API int cnd_init(cnd_t *cond) { return PF__PT(cond_init, cond, NULL); }
PF_API void cnd_destroy(cnd_t *cond) { PF__PT(cond_destroy, cond); }
PF_API int cnd_wait(cnd_t *cond, mtx_t *mtx) { return PF__PT(cond_wait, cond, mtx); }
PF_API int cnd_signal(cnd_t *cond) { return PF__PT(cond_signal, cond); }
PF_API int cnd_broadcast(cnd_t *cond) { return PF__PT(cond_broadcast, cond); }
PF_API void mtx_destroy(mtx_t *mtx) { pthread_mutex_destroy(mtx); }
PF_API int mtx_lock(mtx_t *mtx) { return PF__PT(mutex_lock, mtx); }
PF_API int mtx_unlock(mtx_t *mtx) { return PF__PT(mutex_unlock, mtx); }
PF_API int mtx_trylock(mtx_t *mtx) { return PF__PT(mutex_trylock, mtx); }
PF_API int tss_create(tss_t *key, tss_dtor_t dtor) { return PF__PT(key_create, key, dtor); }
PF_API int tss_set(tss_t key, void *val) { return PF__PT(setspecific, key, val); }
PF_API void tss_delete(tss_t key) { pthread_key_delete(key); }
PF_API void *tss_get(tss_t key) { return pthread_getspecific(key); }

    /* clang-format on */

#else

    #ifndef __STDC_NO_THREADS__
        #define __STDC_NO_THREADS__ 202311L
    #endif

    #include <stdlib.h>
    #include <time.h>

typedef char cnd_t;
typedef char thrd_t;
typedef char tss_t;
typedef char mtx_t;
typedef char once_flag;

/* clang-format off */

PF_API void call_once(once_flag* flag, void (*func)(void)) {}
PF_API int cnd_broadcast(cnd_t* cond) { return thrd_error; }
PF_API void cnd_destroy(cnd_t* cond) {}
PF_API int cnd_init(cnd_t* cond) { return thrd_error; }
PF_API int cnd_signal(cnd_t* cond) { return thrd_error; }
PF_API int cnd_timedwait(cnd_t* restrict cond, mtx_t* restrict mtx, const struct timespec* restrict ts) { return thrd_error; }
PF_API int cnd_wait(cnd_t* cond, mtx_t* mtx) { return thrd_error; }
PF_API void mtx_destroy(mtx_t* mtx) {}
PF_API int mtx_init(mtx_t* mtx, int type) { return thrd_error; }
PF_API int mtx_lock(mtx_t* mtx) { return thrd_error; }
PF_API int mtx_timedlock(mtx_t* restrict mtx, const struct timespec* restrict ts) { return thrd_error; }
PF_API int mtx_trylock(mtx_t* mtx) { return thrd_error; }
PF_API int mtx_unlock(mtx_t* mtx) { return thrd_error; }
PF_API int thrd_create(thrd_t* thr, thrd_start_t func, void* arg) { return thrd_error; }
PF_API thrd_t thrd_current(void) { return 0; }
PF_API int thrd_detach(thrd_t thr) { return thrd_error; }
PF_API int thrd_equal(thrd_t thr0, thrd_t thr1) { return thrd_error; }
PF_API PF__NO_RETURN void thrd_exit(int res) { exit(res); }
PF_API int thrd_join(thrd_t thr, int* res) { return thrd_error; }
PF_API int thrd_sleep(const struct timespec* duration, struct timespec* remaining) { return thrd_error; }
PF_API void thrd_yield(void) {}
PF_API int tss_create(tss_t* key, tss_dtor_t dtor) { return thrd_error; }
PF_API void tss_delete(tss_t key) {}
PF_API void* tss_get(tss_t key) { return NULL; }
PF_API int tss_set(tss_t key, void* val) { return thrd_error; }

    /* clang-format on */

#endif

#if defined(PF_THREADS_NONE) || defined(PF_NO_THREADS_EXTENSIONS)

typedef char pf_barrier_t;
typedef char pf_rwlock_t;
typedef char pf_spinlock_t;

/* clang-format off */

PF_API int pf_rwlock_init(pf_rwlock_t *lock) { return thrd_error; }
PF_API void pf_rwlock_free(pf_rwlock_t *lock) { }
PF_API int pf_rwlock_try_rd(pf_rwlock_t *lock) { return thrd_error; }
PF_API int pf_rwlock_try_wr(pf_rwlock_t *lock) { return thrd_error; }
PF_API int pf_rwlock_exit_rd(pf_rwlock_t *lock) { return thrd_error; }
PF_API int pf_rwlock_exit_wr(pf_rwlock_t *lock) { return thrd_error; }
PF_API int pf_rwlock_wait_rd(pf_rwlock_t *lock, const struct timespec *ts) { return thrd_error; }
PF_API int pf_rwlock_wait_wr(pf_rwlock_t *lock, const struct timespec *ts) { return thrd_error; }
PF_API int pf_barrier_init(pf_barrier_t *b, int count) { return thrd_error; }
PF_API int pf_barrier_wait(pf_barrier_t *b) { return thrd_error; }
PF_API void pf_barrier_free(pf_barrier_t *b) { }
PF_API int pf_spin_init(pf_spinlock_t *spin) { return thrd_error; }
PF_API int pf_spin_lock(pf_spinlock_t *spin) { return thrd_error; }
PF_API int pf_spin_trylock(pf_spinlock_t *spin) { return thrd_error; }
PF_API int pf_spin_unlock(pf_spinlock_t *spin) { return thrd_error; }
PF_API void pf_spin_free(pf_spinlock_t *spin) { }

    /* clang-format on */

#elif defined(PF_THREADS_POSIX)
    #include <errno.h>

    #ifdef PF_THREADS_STD
        #include <pthread.h>
    #endif

typedef pthread_barrier_t pf_barrier_t;
typedef pthread_rwlock_t pf_rwlock_t;
typedef pthread_spinlock_t pf_spinlock_t;

PF_API int pf_rwlock_wait_rd(pf_rwlock_t *lock, const struct timespec *ts) {
    return ts ? pthread_rwlock_timedrdlock(lock, ts)
              : pthread_rwlock_rdlock(lock);
}

PF_API int pf_rwlock_wait_wr(pf_rwlock_t *lock, const struct timespec *ts) {
    return ts ? pthread_rwlock_timedwrlock(lock, ts)
              : pthread_rwlock_wrlock(lock);
}

PF_API int pf_barrier_init(pf_barrier_t *b, int count) {
    if (!b || count <= 0)
        return thrd_error;
    int rc = pthread_barrier_init(b, NULL, (unsigned)count);
    return (rc == 0) ? thrd_success : thrd_error;
}

PF_API int pf_barrier_wait(pf_barrier_t *b) {
    if (!b)
        return thrd_error;
    int rc = pthread_barrier_wait(b);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD)
        return thrd_success;
    if (rc == 0)
        return thrd_busy;
    return thrd_error;
}

PF_API int pf_spin_trylock(pf_spinlock_t *spin) {
    if (!spin)
        return thrd_error;
    int rc = pthread_spin_trylock(spin);
    if (rc == 0)
        return 0;
    if (rc == EBUSY)
        return 1;
    return thrd_error;
}

/* clang-format off */

PF_API int pf_rwlock_init(pf_rwlock_t *lock) { return PF__PT2(rwlock_init, lock, NULL); }
PF_API void pf_rwlock_free(pf_rwlock_t *lock) { if (lock) pthread_rwlock_destroy(lock); }
PF_API int pf_rwlock_try_rd(pf_rwlock_t *lock) { return PF__PT2(rwlock_tryrdlock, lock); }
PF_API int pf_rwlock_try_wr(pf_rwlock_t *lock) { return PF__PT2(rwlock_trywrlock, lock); }
PF_API int pf_rwlock_exit_rd(pf_rwlock_t *lock) { return PF__PT2(rwlock_unlock, lock); }
PF_API int pf_rwlock_exit_wr(pf_rwlock_t *lock) { return PF__PT2(rwlock_unlock, lock); }
PF_API void pf_barrier_free(pf_barrier_t *b) { if (b) pthread_barrier_destroy(b); }
PF_API int pf_spin_init(pf_spinlock_t *spin) { return PF__PT2(spin_init, spin, PTHREAD_PROCESS_PRIVATE); }
PF_API int pf_spin_lock(pf_spinlock_t *spin) { return PF__PT2(spin_lock, spin); }
PF_API int pf_spin_unlock(pf_spinlock_t *spin) { return PF__PT2(spin_unlock, spin); }
PF_API void pf_spin_free(pf_spinlock_t *spin) { if (spin) pthread_spin_destroy(spin); }

    /* clang-format on */

#elif defined(PF_THREADS_WIN32)
    #ifdef PF_THREADS_STD
        #ifndef WIN32_LEAN_AND_MEAN
            #define WIN32_LEAN_AND_MEAN
        #endif
        #include <windows.h>
    #endif

typedef SRWLOCK pf_rwlock_t;

typedef struct {
    volatile LONG locked; /* 0 = free, 1 = held */
} pf_spinlock_t;

PF_API int pf_rwlock_init(pf_rwlock_t *lock) {
    if (!lock)
        return thrd_error;
    InitializeSRWLock(lock);
    return thrd_success;
}

PF_API void pf_rwlock_free(pf_rwlock_t *lock) {
    (void)lock; /* SRWLOCK has no destroy function. */
}

PF_API int pf_rwlock_try_rd(pf_rwlock_t *lock) {
    if (!lock)
        return thrd_error;
    if (!TryAcquireSRWLockShared(lock))
        return thrd_busy;
    return thrd_success;
}

PF_API int pf_rwlock_try_wr(pf_rwlock_t *lock) {
    if (!lock)
        return thrd_error;
    if (!TryAcquireSRWLockExclusive(lock))
        return thrd_error;
    return thrd_success;
}

PF_API int pf_rwlock_wait_rd(pf_rwlock_t *lock, const struct timespec *ts) {
    if (!lock)
        return thrd_error;
    if (ts)
        return thrd_error;
    AcquireSRWLockShared(lock);
    return thrd_success;
}

PF_API int pf_rwlock_wait_wr(pf_rwlock_t *lock, const struct timespec *ts) {
    if (!lock)
        return thrd_error;
    if (ts)
        return thrd_error;
    AcquireSRWLockExclusive(lock);
    return thrd_success;
}

PF_API int pf_rwlock_exit_rd(pf_rwlock_t *lock) {
    if (!lock)
        return thrd_error;
    ReleaseSRWLockShared(lock);
    return thrd_success;
}

PF_API int pf_rwlock_exit_wr(pf_rwlock_t *lock) {
    if (!lock)
        return thrd_error;
    ReleaseSRWLockExclusive(lock);
    return thrd_success;
}

PF_API int pf_spin_init(pf_spinlock_t *spin) {
    if (!spin)
        return thrd_error;
    spin->locked = 0;
    return thrd_success;
}

PF_API int pf_spin_lock(pf_spinlock_t *spin) {
    if (!spin)
        return thrd_error;
    while (InterlockedExchange(&spin->locked, 1L) != 0) {
    #if defined(_M_IX86) || defined(_M_X64)
        _mm_pause(); /* Emit a PAUSE hint to reduce bus traffic on x86/x64. */
    #else
        YieldProcessor(); /* Falls back to a no-op compiler barrier on ARM. */
    #endif
    }
    return thrd_success;
}

PF_API int pf_spin_trylock(pf_spinlock_t *spin) {
    if (!spin)
        return thrd_error;
    int r = InterlockedExchange(&spin->locked, 1L);
    return r == 0 ? thrd_success : thrd_busy;
}

PF_API int pf_spin_unlock(pf_spinlock_t *spin) {
    if (!spin)
        return thrd_error;
    InterlockedExchange(&spin->locked, 0L);
    return thrd_success;
}

PF_API void pf_spin_free(pf_spinlock_t *spin) {
    (void)spin; /* nothing to release */
}

    #if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0602

typedef SYNCHRONIZATION_BARRIER pf_barrier_t;

PF_API int pf_barrier_init(pf_barrier_t *b, int count) {
    if (!b || count <= 0)
        return thrd_error;
    int ret = InitializeSynchronizationBarrier(b, count, -1);
    return ret ? thrd_success : thrd_error;
}

PF_API int pf_barrier_wait(pf_barrier_t *b) {
    if (!b)
        return thrd_error;
    int ret = EnterSynchronizationBarrier(
        b, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY
    );
    return ret ? thrd_success : thrd_busy;
}

PF_API void pf_barrier_free(pf_barrier_t *b) {
    if (b)
        DeleteSynchronizationBarrier(b);
}

    #else

typedef struct {
    CRITICAL_SECTION cs;
    HANDLE event; /* manual-reset, initially non-signalled */
    int total; /* thread count supplied at init          */
    volatile int waiting; /* threads currently waiting              */
    volatile LONG generation; /* flipped each cycle to avoid ABA        */
} pf_barrier_t;

PF_API int pf_barrier_init(pf_barrier_t *b, int count) {
    if (!b || count <= 0)
        return thrd_error;

    b->total = count;
    b->waiting = 0;
    b->generation = 0;

    InitializeCriticalSectionAndSpinCount(&b->cs, 1500);
    b->event = CreateEventW(NULL, TRUE, FALSE, NULL); /* manual-reset */
    return (b->event != NULL) ? 0 : thrd_error;
}

/*
 * The manual-reset event lets all waiting threads wake together.
 * The generation counter ensures a late thread from the previous cycle
 * does not see the reset event of the current cycle.
 */
PF_API int pf_barrier_wait(pf_barrier_t *b) {
    if (!b)
        return thrd_error;

    EnterCriticalSection(&b->cs);

    LONG my_gen = b->generation;
    b->waiting++;

    if (b->waiting == b->total) {
        b->waiting = 0;
        InterlockedIncrement(&b->generation);
        SetEvent(b->event); /* wake all waiting threads  */
        LeaveCriticalSection(&b->cs);
        ResetEvent(b->event);
        return thrd_success;
    } else {
        LeaveCriticalSection(&b->cs);
        /* Spin-wait on the event; re-check generation to guard against
         * spurious wakes or a fast reuse of the barrier. */
        while (b->generation == my_gen) {
            DWORD rc = WaitForSingleObject(b->event, INFINITE);
            if (rc != WAIT_OBJECT_0)
                return thrd_error;
        }

        return thrd_busy;
    }
}

PF_API void pf_barrier_free(pf_barrier_t *b) {
    if (!b)
        return;

    if (b->event) {
        CloseHandle(b->event);
        b->event = NULL;
    }

    DeleteCriticalSection(&b->cs);
}

    #endif

#endif

#endif
