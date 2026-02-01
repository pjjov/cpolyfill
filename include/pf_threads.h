/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a C11 <threads.h> wrapper for Windows and POSIX
    threads. Additionaly, it can be compiled on systems without threads.

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
#elif defined(_WIN32)
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

    #define PF__PT(name, ...)                                          \
        (pthread_##name(__VA_ARGS__)) != 0 ? thrd_error : thrd_success

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

#endif
