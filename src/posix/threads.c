/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/threads.h> on pthreads.
*/

#include "../internal.h"

#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#if PF__THREADS_POSIX

    #include <errno.h>
    #include <limits.h>
    #include <pthread.h>
    #include <sched.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>

    #if PF_OS_LINUX || PF_OS_ANDROID
        #include <sys/prctl.h>
        #include <sys/resource.h>
        #include <sys/syscall.h>
    #endif
    #if PF_OS_FREEBSD || PF_OS_DRAGONFLY || PF_OS_OPENBSD
        #include <pthread_np.h>
    #endif
    #if PF_OS_FREEBSD
        #include <sys/cpuset.h>
        #include <sys/param.h>
    #endif
    #if PF_OS_NETBSD
        #include <lwp.h>
    #endif

    #ifndef PF__HAVE_PTHREAD_MUTEX_TIMEDLOCK
        #define PF__HAVE_PTHREAD_MUTEX_TIMEDLOCK 0
    #endif
    #ifndef PF__HAVE_PTHREAD_SETNAME_NP
        #define PF__HAVE_PTHREAD_SETNAME_NP 0
    #endif
    #ifndef PF__HAVE_PTHREAD_GETNAME_NP
        #define PF__HAVE_PTHREAD_GETNAME_NP 0
    #endif

/* ---- Threads ----------------------------------------------------------- */

struct start {
    pf_thrd_start_t func;
    void *arg;
    char name[];
};

static void *trampoline(void *raw) {
    struct start s = *(struct start *)raw;
    if (((struct start *)raw)->name[0])
        (void)pf_thrd_set_name(((struct start *)raw)->name);
    free(raw);
    return (void *)(intptr_t)s.func(s.arg);
}

PF_API int pf_thrd_create_ex(
    pf_thrd_t *thr, pf_thrd_start_t func, void *arg, const pf_thrd_attr_t *attr
) {
    size_t name_len = attr && attr->name ? strlen(attr->name) : 0;
    struct start *s;
    pthread_attr_t pattr;
    int rc;

    if (!thr || !func)
        return PF_EINVAL;
    s = (struct start *)malloc(sizeof(*s) + name_len + 1);
    if (!s)
        return PF_ENOMEM;
    s->func = func;
    s->arg = arg;
    if (name_len)
        memcpy(s->name, attr->name, name_len);
    s->name[name_len] = 0;

    if ((rc = pthread_attr_init(&pattr)) != 0) {
        free(s);
        return pf_error_from_errno(rc);
    }
    if (attr && attr->stack_size) {
        size_t size = attr->stack_size;
    #ifdef PTHREAD_STACK_MIN
        if (size < (size_t)PTHREAD_STACK_MIN)
            size = (size_t)PTHREAD_STACK_MIN;
    #endif
        {
            /* Some systems want whole pages. */
            long page = sysconf(_SC_PAGESIZE);
            if (page > 0)
                size = (size + (size_t)page - 1) / (size_t)page * (size_t)page;
        }
        rc = pthread_attr_setstacksize(&pattr, size);
    }
    if (rc == 0)
        rc = pthread_create(thr, &pattr, trampoline, s);
    pthread_attr_destroy(&pattr);
    if (rc) {
        free(s);
        return pf_error_from_errno(rc);
    }
    return 0;
}

PF_API int pf_thrd_create(pf_thrd_t *thr, pf_thrd_start_t func, void *arg) {
    switch (pf_thrd_create_ex(thr, func, arg, NULL)) {
    case 0:
        return pf_thrd_success;
    case PF_ENOMEM:
        return pf_thrd_nomem;
    default:
        return pf_thrd_error;
    }
}

PF_API int pf_thrd_equal(pf_thrd_t a, pf_thrd_t b) {
    return pthread_equal(a, b) != 0;
}

PF_API pf_thrd_t pf_thrd_current(void) { return pthread_self(); }

PF_API int pf_thrd_sleep(
    const struct timespec *duration, struct timespec *remaining
) {
    if (pf_nanosleep(duration, remaining) == 0)
        return 0;
    return errno == EINTR ? -1 : -2;
}

PF_API void pf_thrd_yield(void) { sched_yield(); }

PF_NORETURN PF_API void pf_thrd_exit(int res) {
    pthread_exit((void *)(intptr_t)res);
}

PF_API int pf_thrd_detach(pf_thrd_t thr) {
    return pthread_detach(thr) ? pf_thrd_error : pf_thrd_success;
}

PF_API int pf_thrd_join(pf_thrd_t thr, int *res) {
    void *value;
    if (pthread_join(thr, &value))
        return pf_thrd_error;
    if (res)
        *res = (int)(intptr_t)value;
    return pf_thrd_success;
}

/* ---- Mutexes ----------------------------------------------------------- */

PF_API int pf_mtx_init(pf_mtx_t *mtx, int type) {
    pthread_mutexattr_t attr;
    int rc;

    if (!mtx || (type & ~(pf_mtx_timed | pf_mtx_recursive)))
        return pf_thrd_error;
    if (pthread_mutexattr_init(&attr))
        return pf_thrd_error;
    rc = pthread_mutexattr_settype(
        &attr,
        type & pf_mtx_recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL
    );
    if (rc == 0)
        rc = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    if (rc)
        return rc == ENOMEM ? pf_thrd_nomem : pf_thrd_error;
    return pf_thrd_success;
}

PF_API int pf_mtx_lock(pf_mtx_t *mtx) {
    return pthread_mutex_lock(mtx) ? pf_thrd_error : pf_thrd_success;
}

PF_API int pf_mtx_trylock(pf_mtx_t *mtx) {
    switch (pthread_mutex_trylock(mtx)) {
    case 0:
        return pf_thrd_success;
    case EBUSY:
        return pf_thrd_busy;
    default:
        return pf_thrd_error;
    }
}

PF_API int pf_mtx_timedlock(pf_mtx_t *mtx, const struct timespec *deadline) {
    #if PF__HAVE_PTHREAD_MUTEX_TIMEDLOCK
    switch (pthread_mutex_timedlock(mtx, deadline)) {
    case 0:
        return pf_thrd_success;
    case ETIMEDOUT:
        return pf_thrd_timedout;
    default:
        return pf_thrd_error;
    }
    #else
    /* No timed mutex (macOS): poll, backing off from 50 us to 1 ms. */
    long wait_ns = 50000;
    for (;;) {
        struct timespec now, nap;
        int rc = pf_mtx_trylock(mtx);
        if (rc != pf_thrd_busy)
            return rc;
        if (!pf_timespec_get(&now, PF_TIME_UTC))
            return pf_thrd_error;
        if (pf_timespec_cmp(now, *deadline) >= 0)
            return pf_thrd_timedout;
        nap = pf_timespec_sub(*deadline, now);
        if (nap.tv_sec > 0 || nap.tv_nsec > wait_ns) {
            nap.tv_sec = 0;
            nap.tv_nsec = wait_ns;
        }
        pf_nanosleep(&nap, NULL);
        if (wait_ns < 1000000)
            wait_ns *= 2;
    }
    #endif
}

PF_API int pf_mtx_unlock(pf_mtx_t *mtx) {
    return pthread_mutex_unlock(mtx) ? pf_thrd_error : pf_thrd_success;
}

PF_API void pf_mtx_destroy(pf_mtx_t *mtx) { pthread_mutex_destroy(mtx); }

PF_API void pf_call_once(pf_once_flag *flag, void (*func)(void)) {
    pthread_once(flag, func);
}

/* ---- Condition variables ----------------------------------------------- */

PF_API int pf_cnd_init(pf_cnd_t *cnd) {
    switch (pthread_cond_init(cnd, NULL)) {
    case 0:
        return pf_thrd_success;
    case ENOMEM:
        return pf_thrd_nomem;
    default:
        return pf_thrd_error;
    }
}

PF_API int pf_cnd_signal(pf_cnd_t *cnd) {
    return pthread_cond_signal(cnd) ? pf_thrd_error : pf_thrd_success;
}

PF_API int pf_cnd_broadcast(pf_cnd_t *cnd) {
    return pthread_cond_broadcast(cnd) ? pf_thrd_error : pf_thrd_success;
}

PF_API int pf_cnd_wait(pf_cnd_t *cnd, pf_mtx_t *mtx) {
    return pthread_cond_wait(cnd, mtx) ? pf_thrd_error : pf_thrd_success;
}

PF_API int pf_cnd_timedwait(
    pf_cnd_t *cnd, pf_mtx_t *mtx, const struct timespec *deadline
) {
    switch (pthread_cond_timedwait(cnd, mtx, deadline)) {
    case 0:
        return pf_thrd_success;
    case ETIMEDOUT:
        return pf_thrd_timedout;
    default:
        return pf_thrd_error;
    }
}

PF_API void pf_cnd_destroy(pf_cnd_t *cnd) { pthread_cond_destroy(cnd); }

/* ---- Thread-specific storage ------------------------------------------- */

PF_API int pf_tss_create(pf_tss_t *key, pf_tss_dtor_t dtor) {
    return pthread_key_create(key, dtor) ? pf_thrd_error : pf_thrd_success;
}

PF_API void pf_tss_delete(pf_tss_t key) { pthread_key_delete(key); }

PF_API void *pf_tss_get(pf_tss_t key) { return pthread_getspecific(key); }

PF_API int pf_tss_set(pf_tss_t key, void *value) {
    return pthread_setspecific(key, value) ? pf_thrd_error : pf_thrd_success;
}

/* ---- Names ------------------------------------------------------------- */

PF_API int pf_thrd_set_name(const char *name) {
    if (!name)
        return PF_EINVAL;
    #if PF_OS_LINUX || PF_OS_ANDROID
    {
        char buf[16]; /* TASK_COMM_LEN */
        size_t n = pf__utf8_truncate(name, sizeof(buf) - 1);
        memcpy(buf, name, n);
        buf[n] = 0;
        return prctl(PR_SET_NAME, (unsigned long)buf, 0, 0, 0)
            ? pf_error_from_errno(errno)
            : 0;
    }
    #elif PF_OS_APPLE
    {
        char buf[64]; /* MAXTHREADNAMESIZE */
        size_t n = pf__utf8_truncate(name, sizeof(buf) - 1);
        memcpy(buf, name, n);
        buf[n] = 0;
        return pf_error_from_errno(pthread_setname_np(buf));
    }
    #elif PF_OS_NETBSD
    {
        char buf[32]; /* PTHREAD_MAX_NAMELEN_NP */
        size_t n = pf__utf8_truncate(name, sizeof(buf) - 1);
        memcpy(buf, name, n);
        buf[n] = 0;
        return pf_error_from_errno(
            pthread_setname_np(pthread_self(), "%s", (void *)buf)
        );
    }
    #elif PF_OS_FREEBSD || PF_OS_DRAGONFLY || PF_OS_OPENBSD
    pthread_set_name_np(pthread_self(), name);
    return 0;
    #elif PF__HAVE_PTHREAD_SETNAME_NP
    return pf_error_from_errno(pthread_setname_np(pthread_self(), name));
    #else
    return PF_ENOTSUP;
    #endif
}

PF_API int pf_thrd_get_name(char *buf, size_t size) {
    char name[64];

    if (!buf || size == 0)
        return PF_EINVAL;
    #if PF_OS_LINUX || PF_OS_ANDROID
    if (prctl(PR_GET_NAME, (unsigned long)name, 0, 0, 0))
        return pf_error_from_errno(errno);
    name[16] = 0;
    #elif PF_OS_OPENBSD
    pthread_get_name_np(pthread_self(), name, sizeof(name));
    #elif PF__HAVE_PTHREAD_GETNAME_NP
    {
        int rc = pthread_getname_np(pthread_self(), name, sizeof(name));
        if (rc)
            return pf_error_from_errno(rc);
    }
    #else
    return PF_ENOTSUP;
    #endif
    name[sizeof(name) - 1] = 0;
    return pf__copy_name(buf, size, name);
}

PF_API uint64_t pf_thrd_id(void) {
    #if PF_OS_LINUX || PF_OS_ANDROID
    return (uint64_t)syscall(SYS_gettid);
    #elif PF_OS_APPLE
    uint64_t id = 0;
    pthread_threadid_np(NULL, &id);
    return id;
    #elif PF_OS_FREEBSD
    return (uint64_t)pthread_getthreadid_np();
    #elif PF_OS_NETBSD
    return (uint64_t)_lwp_self();
    #elif PF_OS_OPENBSD
    return (uint64_t)getthrid();
    #elif PF_OS_DRAGONFLY
    return (uint64_t)lwp_gettid();
    #else
    return (uint64_t)(uintptr_t)pthread_self();
    #endif
}

/* ---- Affinity ---------------------------------------------------------- */

static int mask_empty(const uint64_t *mask, size_t ncpus) {
    size_t i;
    for (i = 0; i < ncpus; i++)
        if (mask[i / 64] >> (i % 64) & 1)
            return 0;
    return 1;
}

    #if PF_OS_LINUX || PF_OS_ANDROID

    /* The kernel wants a set at least as large as its own CPU mask. */
        #define LINUX_MIN_CPUS 8192

PF_API int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus) {
    size_t n = ncpus > LINUX_MIN_CPUS ? ncpus : LINUX_MIN_CPUS, i;
    size_t bytes = CPU_ALLOC_SIZE(n);
    cpu_set_t *set;
    int rc = 0;

    if (!mask || ncpus == 0 || mask_empty(mask, ncpus))
        return PF_EINVAL;
    if (!(set = CPU_ALLOC(n)))
        return PF_ENOMEM;
    CPU_ZERO_S(bytes, set);
    for (i = 0; i < ncpus; i++)
        if (mask[i / 64] >> (i % 64) & 1)
            CPU_SET_S(i, bytes, set);
    if (sched_setaffinity(0, bytes, set))
        rc = pf_error_from_errno(errno);
    CPU_FREE(set);
    return rc;
}

PF_API int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus) {
    size_t n = ncpus > LINUX_MIN_CPUS ? ncpus : LINUX_MIN_CPUS, i;
    size_t bytes = CPU_ALLOC_SIZE(n);
    cpu_set_t *set;
    int rc = 0;

    if (!mask || ncpus == 0)
        return PF_EINVAL;
    if (!(set = CPU_ALLOC(n)))
        return PF_ENOMEM;
    CPU_ZERO_S(bytes, set);
    if (sched_getaffinity(0, bytes, set)) {
        rc = pf_error_from_errno(errno);
    } else {
        memset(mask, 0, (ncpus + 63) / 64 * sizeof(uint64_t));
        for (i = 0; i < ncpus; i++)
            if (CPU_ISSET_S(i, bytes, set))
                mask[i / 64] |= (uint64_t)1 << (i % 64);
    }
    CPU_FREE(set);
    return rc;
}

    #elif PF_OS_FREEBSD

PF_API int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus) {
    cpuset_t set;
    size_t i;

    if (!mask || ncpus == 0 || mask_empty(mask, ncpus))
        return PF_EINVAL;
    CPU_ZERO(&set);
    for (i = 0; i < ncpus; i++) {
        if (mask[i / 64] >> (i % 64) & 1) {
            if (i >= CPU_SETSIZE)
                return PF_EINVAL;
            CPU_SET(i, &set);
        }
    }
    if (cpuset_setaffinity(
            CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(set), &set
        ))
        return pf_error_from_errno(errno);
    return 0;
}

PF_API int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus) {
    cpuset_t set;
    size_t i;

    if (!mask || ncpus == 0)
        return PF_EINVAL;
    if (cpuset_getaffinity(
            CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(set), &set
        ))
        return pf_error_from_errno(errno);
    memset(mask, 0, (ncpus + 63) / 64 * sizeof(uint64_t));
    for (i = 0; i < ncpus && i < CPU_SETSIZE; i++)
        if (CPU_ISSET(i, &set))
            mask[i / 64] |= (uint64_t)1 << (i % 64);
    return 0;
}

    #else

PF_API int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus) {
    if (!mask || ncpus == 0 || mask_empty(mask, ncpus))
        return PF_EINVAL;
    return PF_ENOTSUP;
}

PF_API int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus) {
    if (!mask || ncpus == 0)
        return PF_EINVAL;
    return PF_ENOTSUP;
}

    #endif

/* ---- Priority ---------------------------------------------------------- */

PF_API int pf_thrd_set_priority(int priority) {
    if (priority < PF_THRD_PRIORITY_LOWEST
        || priority > PF_THRD_PRIORITY_HIGHEST)
        return PF_EINVAL;
    #if PF_OS_LINUX || PF_OS_ANDROID
    {
        /* Linux schedules threads by their own nice value. */
        static const int nice[] = { 19, 10, 0, -10, -20 };
        id_t tid = (id_t)syscall(SYS_gettid);
        if (setpriority(PRIO_PROCESS, tid, nice[priority + 2]))
            return errno == EACCES ? PF_EPERM : pf_error_from_errno(errno);
        return 0;
    }
    #else
    {
        struct sched_param param;
        int policy, lo, hi, rc;

        if ((rc = pthread_getschedparam(pthread_self(), &policy, &param)))
            return pf_error_from_errno(rc);
        lo = sched_get_priority_min(policy);
        hi = sched_get_priority_max(policy);
        if (lo < 0 || hi < 0)
            return PF_ENOTSUP;
        /* LOWEST..HIGHEST spread over the policy's range. */
        param.sched_priority = lo + (hi - lo) * (priority + 2) / 4;
        rc = pthread_setschedparam(pthread_self(), policy, &param);
        return rc ? pf_error_from_errno(rc) : 0;
    }
    #endif
}

#endif /* PF__THREADS_POSIX */
