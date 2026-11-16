/** cpolyfill - Filling the gaps between C standards and compilers

    C11 threads on every platform, plus thread names, ids, affinity and
    priority, reader-writer locks, spin locks and barriers.

    The C11 API, prefixed (the library's own implementation on pthreads or
    Win32, never the system's <threads.h>):

        pf_thrd_t, pf_mtx_t, pf_cnd_t, pf_tss_t, pf_once_flag
        PF_ONCE_FLAG_INIT, PF_TSS_DTOR_ITERATIONS
        pf_thrd_start_t         int (*)(void *)
        pf_tss_dtor_t           void (*)(void *)
        pf_mtx_plain, pf_mtx_timed, pf_mtx_recursive
        pf_thrd_success, pf_thrd_busy, pf_thrd_error, pf_thrd_nomem,
        pf_thrd_timedout

        pf_thrd_create, pf_thrd_current, pf_thrd_detach, pf_thrd_equal,
        pf_thrd_exit, pf_thrd_join, pf_thrd_sleep, pf_thrd_yield,
        pf_mtx_init, pf_mtx_lock, pf_mtx_timedlock, pf_mtx_trylock,
        pf_mtx_unlock, pf_mtx_destroy, pf_call_once,
        pf_cnd_init, pf_cnd_signal, pf_cnd_broadcast, pf_cnd_wait,
        pf_cnd_timedwait, pf_cnd_destroy,
        pf_tss_create, pf_tss_delete, pf_tss_get, pf_tss_set

    They return the `pf_thrd_*` codes like their C11 counterparts. Timed
    waits take an absolute PF_TIME_UTC deadline (see pf_deadline_ns in
    <cpolyfill/time.h>). Every mutex type works with condition variables.
    tss destructors run when a thread exits, on Windows too, but not for
    the main thread when the process exits. On macOS pf_mtx_timedlock
    polls (the system has no timed mutex).

    Additions, returning 0 or a negative PF_E* code:

        pf_thrd_attr_t          { size_t stack_size; const char *name; }
                                0 / NULL for the defaults
        int pf_thrd_create_ex(pf_thrd_t *thr, pf_thrd_start_t func,
                              void *arg, const pf_thrd_attr_t *attr)
                pf_thrd_create with attributes (`attr` may be NULL). The
                name is set from the new thread before `func` runs.
        int pf_thrd_set_name(const char *name)
                Names the calling thread (UTF-8) for debuggers and tools.
                Linux keeps 15 bytes, cut at a character boundary.
        int pf_thrd_get_name(char *buf, size_t size)
                The calling thread's name: PF_ERANGE if it does not fit
                (`buf` gets what fits), PF_ENOTSUP where unknown.
        uint64_t pf_thrd_id(void)
                The operating system's id of the calling thread (gettid,
                GetCurrentThreadId, ...), as shown by debuggers.
        int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus)
        int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus)
                The CPUs the calling thread may run on: bit `i % 64` of
                `mask[i / 64]` is CPU `i`, for `ncpus` CPUs. Setting an
                empty mask is PF_EINVAL. On Windows the CPUs must lie in one
                processor group of 64. Linux, FreeBSD and Windows;
                PF_ENOTSUP elsewhere (macOS has no affinity).
        int pf_thrd_set_priority(int priority)
                PF_THRD_PRIORITY_LOWEST, _LOW, _NORMAL, _HIGH or _HIGHEST
                for the calling thread. On Linux this is the thread's nice
                value, and raising it needs privileges (PF_EPERM).

        pf_rwlock_t             A writer-preferring reader-writer lock
                                (not recursive).
        int pf_rwlock_init(pf_rwlock_t *l)       void pf_rwlock_free(l)
        int pf_rwlock_rdlock(l)                  int pf_rwlock_wrlock(l)
        int pf_rwlock_tryrdlock(l)               int pf_rwlock_trywrlock(l)
                PF_EBUSY if the lock is taken.
        int pf_rwlock_timedrdlock(l, const struct timespec *deadline)
        int pf_rwlock_timedwrlock(l, const struct timespec *deadline)
                PF_ETIMEDOUT once the absolute UTC deadline passes.
        int pf_rwlock_rdunlock(l)                int pf_rwlock_wrunlock(l)

        pf_spin_t               A spin lock, for very short critical
                                sections.
        int pf_spin_init(pf_spin_t *s)           void pf_spin_free(s)
        int pf_spin_lock(s)   int pf_spin_trylock(s)   int pf_spin_unlock(s)

        pf_barrier_t
        int pf_barrier_init(pf_barrier_t *b, unsigned count)
        void pf_barrier_free(pf_barrier_t *b)
        int pf_barrier_wait(pf_barrier_t *b)
                Blocks until `count` threads wait, then releases them all:
                PF_BARRIER_SERIAL in one of them, 0 in the others.

    Without threads (PF_HAS_THREADS 0, the library was built with
    -Dthreads=disabled or for a platform without them), creating a thread
    fails with pf_thrd_error / PF_ENOSYS. Mutexes, once, tss and the other
    locks work for the single thread; waits that nothing could end fail
    with pf_thrd_error / PF_EDEADLK, and timed ones sleep until the deadline.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_THREADS, where the
    library found no <threads.h> when it was built: the C11 types,
    constants and functions above without the `pf_` prefix (and
    `ONCE_FLAG_INIT`, `TSS_DTOR_ITERATIONS`). `once_flag` and `call_once`
    are left alone where <stdlib.h> declares them (C23). `thread_local` comes from
    <cpolyfill/stdc.h>.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_THREADS_H
#define CPOLYFILL_THREADS_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>

#define PF_HAS_THREADS PF_CONFIG_THREADS

#if PF_CONFIG_THREADS && PF_OS_WINDOWS
    #define PF__THREADS_WIN32 1
    #define PF__THREADS_POSIX 0
#elif PF_CONFIG_THREADS
    #define PF__THREADS_WIN32 0
    #define PF__THREADS_POSIX 1
#else
    #define PF__THREADS_WIN32 0
    #define PF__THREADS_POSIX 0
#endif

#if PF__THREADS_POSIX
    #include <pthread.h>
#endif

enum pf_thrd_result {
    pf_thrd_success = 0,
    pf_thrd_busy = 1,
    pf_thrd_error = 2,
    pf_thrd_nomem = 3,
    pf_thrd_timedout = 4
};

enum pf_mtx_type {
    pf_mtx_plain = 0,
    pf_mtx_recursive = 1,
    pf_mtx_timed = 2
};

enum pf_thrd_priority {
    PF_THRD_PRIORITY_LOWEST = -2,
    PF_THRD_PRIORITY_LOW = -1,
    PF_THRD_PRIORITY_NORMAL = 0,
    PF_THRD_PRIORITY_HIGH = 1,
    PF_THRD_PRIORITY_HIGHEST = 2
};

#define PF_TSS_DTOR_ITERATIONS 4
#define PF_BARRIER_SERIAL 1

typedef int (*pf_thrd_start_t)(void *);
typedef void (*pf_tss_dtor_t)(void *);

#if PF__THREADS_POSIX

typedef pthread_t pf_thrd_t;
typedef pthread_mutex_t pf_mtx_t;
typedef pthread_cond_t pf_cnd_t;
typedef pthread_key_t pf_tss_t;
typedef pthread_once_t pf_once_flag;
    #define PF_ONCE_FLAG_INIT PTHREAD_ONCE_INIT

#elif PF__THREADS_WIN32

/* The Win32 objects (SRWLOCK, CONDITION_VARIABLE, INIT_ONCE) are one
   pointer each; the library checks that the sizes match. */
typedef struct pf__thrd *pf_thrd_t;
typedef struct pf_mtx_t {
    void *pf__srw;
    void *pf__cv;
    unsigned long pf__owner;
    unsigned long pf__count;
    int pf__type;
    int pf__locked;
} pf_mtx_t;
typedef struct pf_cnd_t {
    void *pf__cv;
} pf_cnd_t;
typedef unsigned long pf_tss_t;
typedef struct pf_once_flag {
    void *pf__once;
} pf_once_flag;
    #define PF_ONCE_FLAG_INIT { 0 }

#else /* no threads */

typedef unsigned long pf_thrd_t;
typedef struct pf_mtx_t {
    int pf__type;
    unsigned long pf__count;
} pf_mtx_t;
typedef struct pf_cnd_t {
    int pf__unused;
} pf_cnd_t;
typedef unsigned pf_tss_t;
typedef struct pf_once_flag {
    int pf__done;
} pf_once_flag;
    #define PF_ONCE_FLAG_INIT { 0 }

#endif

typedef struct pf_thrd_attr_t {
    size_t stack_size;
    const char *name;
} pf_thrd_attr_t;

typedef struct pf_rwlock_t {
    pf_mtx_t pf__mtx;
    pf_cnd_t pf__readers;
    pf_cnd_t pf__writers;
    unsigned pf__active;  /* readers holding the lock */
    unsigned pf__waiting; /* writers waiting */
    int pf__writer;       /* a writer holds the lock */
} pf_rwlock_t;

typedef struct pf_spin_t {
    volatile uint32_t pf__lock;
} pf_spin_t;

typedef struct pf_barrier_t {
    pf_mtx_t pf__mtx;
    pf_cnd_t pf__cnd;
    unsigned pf__count;
    unsigned pf__waiting;
    unsigned pf__generation;
} pf_barrier_t;

PF_BEGIN_DECLS

PF_API int pf_thrd_create(pf_thrd_t *thr, pf_thrd_start_t func, void *arg);
PF_API int pf_thrd_equal(pf_thrd_t a, pf_thrd_t b);
PF_API pf_thrd_t pf_thrd_current(void);
PF_API int pf_thrd_sleep(
    const struct timespec *duration, struct timespec *remaining
);
PF_API void pf_thrd_yield(void);
PF_NORETURN PF_API void pf_thrd_exit(int res);
PF_API int pf_thrd_detach(pf_thrd_t thr);
PF_API int pf_thrd_join(pf_thrd_t thr, int *res);

PF_API int pf_mtx_init(pf_mtx_t *mtx, int type);
PF_API int pf_mtx_lock(pf_mtx_t *mtx);
PF_API int pf_mtx_timedlock(pf_mtx_t *mtx, const struct timespec *deadline);
PF_API int pf_mtx_trylock(pf_mtx_t *mtx);
PF_API int pf_mtx_unlock(pf_mtx_t *mtx);
PF_API void pf_mtx_destroy(pf_mtx_t *mtx);

PF_API void pf_call_once(pf_once_flag *flag, void (*func)(void));

PF_API int pf_cnd_init(pf_cnd_t *cnd);
PF_API int pf_cnd_signal(pf_cnd_t *cnd);
PF_API int pf_cnd_broadcast(pf_cnd_t *cnd);
PF_API int pf_cnd_wait(pf_cnd_t *cnd, pf_mtx_t *mtx);
PF_API int pf_cnd_timedwait(
    pf_cnd_t *cnd, pf_mtx_t *mtx, const struct timespec *deadline
);
PF_API void pf_cnd_destroy(pf_cnd_t *cnd);

PF_API int pf_tss_create(pf_tss_t *key, pf_tss_dtor_t dtor);
PF_API void pf_tss_delete(pf_tss_t key);
PF_API void *pf_tss_get(pf_tss_t key);
PF_API int pf_tss_set(pf_tss_t key, void *value);

PF_API int pf_thrd_create_ex(
    pf_thrd_t *thr, pf_thrd_start_t func, void *arg,
    const pf_thrd_attr_t *attr
);
PF_API int pf_thrd_set_name(const char *name);
PF_API int pf_thrd_get_name(char *buf, size_t size);
PF_API uint64_t pf_thrd_id(void);
PF_API int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus);
PF_API int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus);
PF_API int pf_thrd_set_priority(int priority);

PF_API int pf_rwlock_init(pf_rwlock_t *lock);
PF_API void pf_rwlock_free(pf_rwlock_t *lock);
PF_API int pf_rwlock_rdlock(pf_rwlock_t *lock);
PF_API int pf_rwlock_wrlock(pf_rwlock_t *lock);
PF_API int pf_rwlock_tryrdlock(pf_rwlock_t *lock);
PF_API int pf_rwlock_trywrlock(pf_rwlock_t *lock);
PF_API int pf_rwlock_timedrdlock(
    pf_rwlock_t *lock, const struct timespec *deadline
);
PF_API int pf_rwlock_timedwrlock(
    pf_rwlock_t *lock, const struct timespec *deadline
);
PF_API int pf_rwlock_rdunlock(pf_rwlock_t *lock);
PF_API int pf_rwlock_wrunlock(pf_rwlock_t *lock);

PF_API int pf_spin_init(pf_spin_t *spin);
PF_API void pf_spin_free(pf_spin_t *spin);
PF_API int pf_spin_lock(pf_spin_t *spin);
PF_API int pf_spin_trylock(pf_spin_t *spin);
PF_API int pf_spin_unlock(pf_spin_t *spin);

PF_API int pf_barrier_init(pf_barrier_t *barrier, unsigned count);
PF_API void pf_barrier_free(pf_barrier_t *barrier);
PF_API int pf_barrier_wait(pf_barrier_t *barrier);

PF_END_DECLS

#endif /* CPOLYFILL_THREADS_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__THREADS_STD_NAMES)                                     \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_THREADS))    \
        || defined(PF_STD_NAMES_THREADS))
    #define PF__THREADS_STD_NAMES

    #if !PF_CONFIG_HAVE_THREADS_H || defined(PF__FORCE_STD_NAMES_POLYFILL)
typedef pf_thrd_t thrd_t;
typedef pf_mtx_t mtx_t;
typedef pf_cnd_t cnd_t;
typedef pf_tss_t tss_t;
typedef pf_thrd_start_t thrd_start_t;
typedef pf_tss_dtor_t tss_dtor_t;

        /* C23 (and glibc 2.34) have call_once in <stdlib.h>. */
        #include <stdlib.h>
        #ifndef ONCE_FLAG_INIT
typedef pf_once_flag once_flag;
            #define ONCE_FLAG_INIT PF_ONCE_FLAG_INIT
            #define call_once pf_call_once
        #endif
        #define TSS_DTOR_ITERATIONS PF_TSS_DTOR_ITERATIONS

        #define thrd_success pf_thrd_success
        #define thrd_busy pf_thrd_busy
        #define thrd_error pf_thrd_error
        #define thrd_nomem pf_thrd_nomem
        #define thrd_timedout pf_thrd_timedout
        #define mtx_plain pf_mtx_plain
        #define mtx_recursive pf_mtx_recursive
        #define mtx_timed pf_mtx_timed

        #define thrd_create pf_thrd_create
        #define thrd_equal pf_thrd_equal
        #define thrd_current pf_thrd_current
        #define thrd_sleep pf_thrd_sleep
        #define thrd_yield pf_thrd_yield
        #define thrd_exit pf_thrd_exit
        #define thrd_detach pf_thrd_detach
        #define thrd_join pf_thrd_join
        #define mtx_init pf_mtx_init
        #define mtx_lock pf_mtx_lock
        #define mtx_timedlock pf_mtx_timedlock
        #define mtx_trylock pf_mtx_trylock
        #define mtx_unlock pf_mtx_unlock
        #define mtx_destroy pf_mtx_destroy
        #define cnd_init pf_cnd_init
        #define cnd_signal pf_cnd_signal
        #define cnd_broadcast pf_cnd_broadcast
        #define cnd_wait pf_cnd_wait
        #define cnd_timedwait pf_cnd_timedwait
        #define cnd_destroy pf_cnd_destroy
        #define tss_create pf_tss_create
        #define tss_delete pf_tss_delete
        #define tss_get pf_tss_get
        #define tss_set pf_tss_set
    #endif
#endif
