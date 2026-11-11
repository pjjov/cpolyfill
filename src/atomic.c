/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/atomic.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#elif PF_CONFIG_THREADS
    #include <sched.h>
#endif

#if PF_HAS_THREADS && (PF_OS_LINUX || PF_OS_ANDROID)
    #include <linux/futex.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    #define WAIT_FUTEX 1
#elif PF_HAS_THREADS && PF_OS_APPLE
    #define WAIT_ULOCK 1
    /* Private, but stable since macOS 10.12 and used by libc++. */
    #define UL_COMPARE_AND_WAIT 1
    #define ULF_WAKE_ALL 0x00000100
    #define ULF_NO_ERRNO 0x01000000
extern int __ulock_wait(
    uint32_t op, void *addr, uint64_t value, uint32_t timeout_us
);
extern int __ulock_wake(uint32_t op, void *addr, uint64_t wake_value);
#elif PF_HAS_THREADS && PF_OS_FREEBSD
    #include <sys/types.h>
    #include <sys/umtx.h>
    #define WAIT_UMTX 1
#elif PF_HAS_THREADS && PF_OS_OPENBSD
    #include <sys/futex.h>
    #define WAIT_OPENBSD 1
#elif PF_HAS_THREADS && PF_OS_WINDOWS
    #define WAIT_WINDOWS 1
#endif
#ifndef WAIT_FUTEX
    #define WAIT_FUTEX 0
#endif
#ifndef WAIT_ULOCK
    #define WAIT_ULOCK 0
#endif
#ifndef WAIT_UMTX
    #define WAIT_UMTX 0
#endif
#ifndef WAIT_OPENBSD
    #define WAIT_OPENBSD 0
#endif
#ifndef WAIT_WINDOWS
    #define WAIT_WINDOWS 0
#endif

/* ---- Primitives: compare-exchange on 1, 2, 4 and 8 bytes --------------- */

/* Values are handled as the unsigned integer of the object's size, widened
   to 64 bits. */
static uint64_t get_n(const void *p, size_t size) {
    switch (size) {
    case 1: { uint8_t v; memcpy(&v, p, 1); return v; }
    case 2: { uint16_t v; memcpy(&v, p, 2); return v; }
    case 4: { uint32_t v; memcpy(&v, p, 4); return v; }
    default: { uint64_t v; memcpy(&v, p, 8); return v; }
    }
}

static void put_n(void *p, size_t size, uint64_t v) {
    switch (size) {
    case 1: { uint8_t x = (uint8_t)v; memcpy(p, &x, 1); break; }
    case 2: { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); break; }
    case 4: { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); break; }
    default: memcpy(p, &v, 8); break;
    }
}

#if PF_CC_GNU

    #define CAS_N(type, p, e, d)                                           \
        do {                                                               \
            type pf__e = (type)*(e);                                       \
            bool pf__ok = __atomic_compare_exchange_n(                     \
                (volatile type *)(p), &pf__e, (type)(d), 0,                \
                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST                         \
            );                                                             \
            *(e) = pf__e;                                                  \
            return pf__ok;                                                 \
        } while (0)

static bool native(size_t size) {
    switch (size) {
    case 1: return __atomic_always_lock_free(1, 0);
    case 2: return __atomic_always_lock_free(2, 0);
    case 4: return __atomic_always_lock_free(4, 0);
    case 8: return __atomic_always_lock_free(8, 0);
    default: return false;
    }
}

static bool raw_cas(volatile void *p, size_t size, uint64_t *e, uint64_t d) {
    switch (size) {
    case 1: CAS_N(uint8_t, p, e, d);
    case 2: CAS_N(uint16_t, p, e, d);
    case 4: CAS_N(uint32_t, p, e, d);
    default: CAS_N(uint64_t, p, e, d);
    }
}

static uint64_t raw_load(const volatile void *p, size_t size) {
    switch (size) {
    case 1: return __atomic_load_n((const volatile uint8_t *)p, __ATOMIC_SEQ_CST);
    case 2: return __atomic_load_n((const volatile uint16_t *)p, __ATOMIC_SEQ_CST);
    case 4: return __atomic_load_n((const volatile uint32_t *)p, __ATOMIC_SEQ_CST);
    default: return __atomic_load_n((const volatile uint64_t *)p, __ATOMIC_SEQ_CST);
    }
}

PF_API void pf__atomic_fence(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

#elif PF__ATOMIC_MSVC

static bool native(size_t size) {
    return pf__atomic_msvc_is_lock_free(size);
}

static bool raw_cas(volatile void *p, size_t size, uint64_t *e, uint64_t d) {
    uint64_t seen = pf__atomic_msvc_cas_raw(p, size, *e, d);
    bool ok = seen == *e;
    *e = seen;
    return ok;
}

static uint64_t raw_load(const volatile void *p, size_t size) {
    return pf__atomic_msvc_cas_raw((volatile void *)p, size, 0, 0);
}

PF_API void pf__atomic_fence(void) {
    pf__atomic_msvc_fence();
}

#else /* C11 <stdatomic.h> */

    #include <stdatomic.h>

    #define CAS_N(type, p, e, d)                                           \
        do {                                                               \
            type pf__e = (type)*(e);                                       \
            bool pf__ok = atomic_compare_exchange_strong(                  \
                (volatile _Atomic(type) *)(p), &pf__e, (type)(d)           \
            );                                                             \
            *(e) = pf__e;                                                  \
            return pf__ok;                                                 \
        } while (0)

static bool native(size_t size) {
    static _Atomic(uint8_t) a8;
    static _Atomic(uint16_t) a16;
    static _Atomic(uint32_t) a32;
    static _Atomic(uint64_t) a64;
    switch (size) {
    case 1: return atomic_is_lock_free(&a8);
    case 2: return atomic_is_lock_free(&a16);
    case 4: return atomic_is_lock_free(&a32);
    case 8: return atomic_is_lock_free(&a64);
    default: return false;
    }
}

static bool raw_cas(volatile void *p, size_t size, uint64_t *e, uint64_t d) {
    switch (size) {
    case 1: CAS_N(uint8_t, p, e, d);
    case 2: CAS_N(uint16_t, p, e, d);
    case 4: CAS_N(uint32_t, p, e, d);
    default: CAS_N(uint64_t, p, e, d);
    }
}

static uint64_t raw_load(const volatile void *p, size_t size) {
    switch (size) {
    case 1: return atomic_load((volatile _Atomic(uint8_t) *)p);
    case 2: return atomic_load((volatile _Atomic(uint16_t) *)p);
    case 4: return atomic_load((volatile _Atomic(uint32_t) *)p);
    default: return atomic_load((volatile _Atomic(uint64_t) *)p);
    }
}

PF_API void pf__atomic_fence(void) {
    atomic_thread_fence(memory_order_seq_cst);
}

#endif

/* ---- Spin locks -------------------------------------------------------- */

void pf__cpu_relax(void) {
#if PF_CC_GNU && (PF_ARCH_X86 || PF_ARCH_X86_64)
    __builtin_ia32_pause();
#elif PF_CC_GNU && (PF_ARCH_ARM64 || PF_ARCH_ARM)
    __asm__ __volatile__("yield");
#elif PF_CC_MSVC && (defined(_M_IX86) || defined(_M_X64))
    _mm_pause();
#elif PF_CC_MSVC && (defined(_M_ARM64) || defined(_M_ARM))
    __yield();
#endif
}

void pf__thread_yield(void) {
#if PF_OS_WINDOWS
    SwitchToThread();
#elif PF_CONFIG_THREADS
    sched_yield();
#endif
}

bool pf__spin_trylock(volatile uint32_t *lock) {
    uint64_t e = 0;
    return raw_load(lock, 4) == 0 && raw_cas(lock, 4, &e, 1);
}

void pf__spin_lock(volatile uint32_t *lock) {
    unsigned spins = 0;
    while (!pf__spin_trylock(lock)) {
        /* Spin on reads for a while, then let the holder run. */
        if (++spins < 64)
            pf__cpu_relax();
        else
            pf__thread_yield();
    }
}

void pf__spin_unlock(volatile uint32_t *lock) {
    uint64_t e = 1;
    (void)raw_cas(lock, 4, &e, 0);
}

/* ---- Lock table -------------------------------------------------------- */

/* Objects that are not lock-free are guarded by one of these spin locks,
   picked by address, each on its own cache line. */
#define LOCK_COUNT 64

static struct {
    volatile uint32_t word;
    char pad[64 - sizeof(uint32_t)];
} locks[LOCK_COUNT];

static volatile uint32_t *lock_for(const volatile void *obj) {
    uintptr_t a = (uintptr_t)obj;
    return &locks[(a >> 4 ^ a >> 10) % LOCK_COUNT].word;
}

void *pf__atomic_load_locked(const volatile void *obj, size_t size, void *ret) {
    volatile uint32_t *l = lock_for(obj);
    pf__spin_lock(l);
    memcpy(ret, (const void *)obj, size);
    pf__spin_unlock(l);
    return ret;
}

void pf__atomic_store_locked(volatile void *obj, size_t size, const void *val) {
    volatile uint32_t *l = lock_for(obj);
    pf__spin_lock(l);
    memcpy((void *)obj, val, size);
    pf__spin_unlock(l);
}

void *pf__atomic_exchange_locked(
    volatile void *obj, size_t size, const void *val, void *ret
) {
    volatile uint32_t *l = lock_for(obj);
    pf__spin_lock(l);
    /* `val` and `ret` may be the same buffer. */
    if (val == ret) {
        unsigned char tmp[64];
        size_t done;
        for (done = 0; done < size; done += sizeof(tmp)) {
            size_t n = size - done < sizeof(tmp) ? size - done : sizeof(tmp);
            memcpy(tmp, (char *)obj + done, n);
            memcpy((char *)obj + done, (const char *)val + done, n);
            memcpy((char *)ret + done, tmp, n);
        }
    } else {
        memcpy(ret, (const void *)obj, size);
        memcpy((void *)obj, val, size);
    }
    pf__spin_unlock(l);
    return ret;
}

bool pf__atomic_cas_locked(
    volatile void *obj, size_t size, void *expected, const void *desired
) {
    volatile uint32_t *l = lock_for(obj);
    bool ok;
    pf__spin_lock(l);
    ok = memcmp((const void *)obj, expected, size) == 0;
    if (ok)
        memcpy((void *)obj, desired, size);
    else
        memcpy(expected, (const void *)obj, size);
    pf__spin_unlock(l);
    return ok;
}

static uint64_t apply(int op, uint64_t old, uint64_t arg) {
    switch (op) {
    case PF__ATOMIC_OP_ADD: return old + arg;
    case PF__ATOMIC_OP_SUB: return old - arg;
    case PF__ATOMIC_OP_OR: return old | arg;
    case PF__ATOMIC_OP_XOR: return old ^ arg;
    default: return old & arg;
    }
}

void *pf__atomic_fetch_locked(
    volatile void *obj, size_t size, int op, unsigned long long arg, void *ret
) {
    volatile uint32_t *l = lock_for(obj);
    uint64_t old;
    pf__spin_lock(l);
    old = get_n((const void *)obj, size);
    put_n((void *)obj, size, apply(op, old, arg));
    pf__spin_unlock(l);
    put_n(ret, size, old);
    return ret;
}

/* ---- Backend of <cpolyfill/atomic.h> ----------------------------------- */

PF_API bool pf__atomic_is_lock_free(size_t size) {
    return native(size);
}

PF_API void *pf__atomic_load(const volatile void *obj, size_t size, void *ret) {
    if (!native(size))
        return pf__atomic_load_locked(obj, size, ret);
    put_n(ret, size, raw_load(obj, size));
    return ret;
}

PF_API void *pf__atomic_exchange(
    volatile void *obj, size_t size, const void *val, void *ret
) {
    uint64_t v, old;
    if (!native(size))
        return pf__atomic_exchange_locked(obj, size, val, ret);
    v = get_n(val, size);
    old = raw_load(obj, size);
    while (!raw_cas(obj, size, &old, v)) {
    }
    put_n(ret, size, old);
    return ret;
}

PF_API void pf__atomic_store(volatile void *obj, size_t size, const void *val) {
    uint64_t ignored;
    if (!native(size))
        pf__atomic_store_locked(obj, size, val);
    else
        (void)pf__atomic_exchange(obj, size, val, &ignored);
}

PF_API bool pf__atomic_cas(
    volatile void *obj, size_t size, void *expected, const void *desired
) {
    uint64_t e;
    if (!native(size))
        return pf__atomic_cas_locked(obj, size, expected, desired);
    e = get_n(expected, size);
    if (raw_cas(obj, size, &e, get_n(desired, size)))
        return true;
    put_n(expected, size, e);
    return false;
}

PF_API void *pf__atomic_fetch(
    volatile void *obj, size_t size, int op, unsigned long long arg, void *ret
) {
    uint64_t old;
    if (!native(size))
        return pf__atomic_fetch_locked(obj, size, op, arg, ret);
    old = raw_load(obj, size);
    while (!raw_cas(obj, size, &old, apply(op, old, arg))) {
    }
    put_n(ret, size, old);
    return ret;
}

/* ---- Waiting ----------------------------------------------------------- */

static int valid_timeout(const struct timespec *t) {
    return !t || (t->tv_sec >= 0 && t->tv_nsec >= 0 && t->tv_nsec < 1000000000L);
}

static uint32_t load32(const volatile void *addr) {
    return (uint32_t)raw_load(addr, 4);
}

/* The fallback: a table of condition variables picked by address. A waker
   changes the value before taking the bucket's mutex, and a waiter checks
   it under that mutex, so no wakeup is lost. */
#if PF_HAS_THREADS

    #define WAIT_BUCKETS 64

static struct {
    pf_mtx_t mtx;
    pf_cnd_t cnd;
} buckets[WAIT_BUCKETS];

static pf_once_flag buckets_once = PF_ONCE_FLAG_INIT;

static void init_buckets(void) {
    size_t i;
    for (i = 0; i < WAIT_BUCKETS; i++) {
        pf_mtx_init(&buckets[i].mtx, pf_mtx_plain);
        pf_cnd_init(&buckets[i].cnd);
    }
}

static size_t bucket_of(const volatile void *addr) {
    uintptr_t a = (uintptr_t)addr;
    return (a >> 2 ^ a >> 9) % WAIT_BUCKETS;
}

#endif

int pf__atomic_wait_fallback(
    const volatile void *addr, uint32_t expected, const struct timespec *timeout
) {
    if (!addr || !valid_timeout(timeout))
        return PF_EINVAL;
    if (load32(addr) != expected)
        return 0;
#if !PF_HAS_THREADS
    /* Only a signal handler could change the value. */
    if (!timeout)
        return PF_EDEADLK;
    pf_nanosleep(timeout, NULL);
    return load32(addr) == expected ? PF_ETIMEDOUT : 0;
#else
    struct timespec deadline;
    size_t b;
    int rc = pf_thrd_success;

    if (timeout && pf_deadline_ns(&deadline, pf_timespec_to_ns(*timeout)))
        return PF_EINVAL;
    pf_call_once(&buckets_once, init_buckets);
    b = bucket_of(addr);
    pf_mtx_lock(&buckets[b].mtx);
    if (load32(addr) == expected) {
        rc = timeout ? pf_cnd_timedwait(&buckets[b].cnd, &buckets[b].mtx, &deadline)
                     : pf_cnd_wait(&buckets[b].cnd, &buckets[b].mtx);
    }
    pf_mtx_unlock(&buckets[b].mtx);
    return rc == pf_thrd_timedout ? PF_ETIMEDOUT : 0;
#endif
}

void pf__atomic_wake_fallback(const volatile void *addr, bool all) {
#if PF_HAS_THREADS
    size_t b = bucket_of(addr);
    (void)all; /* waiters of other addresses share the bucket */
    pf_call_once(&buckets_once, init_buckets);
    pf_mtx_lock(&buckets[b].mtx);
    pf_cnd_broadcast(&buckets[b].cnd);
    pf_mtx_unlock(&buckets[b].mtx);
#else
    (void)addr, (void)all;
#endif
}

PF_API int pf_atomic_wait(
    const volatile void *addr, uint32_t expected, const struct timespec *timeout
) {
#if WAIT_FUTEX
    long rc;
    if (!addr || !valid_timeout(timeout))
        return PF_EINVAL;
    if (load32(addr) != expected)
        return 0;
    #if defined(SYS_futex_time64)
    /* 32-bit systems with a 64-bit time_t need the time64 call. */
    if (sizeof(time_t) > sizeof(long))
        rc = syscall(SYS_futex_time64, (void *)addr, FUTEX_WAIT_PRIVATE,
                     expected, timeout, NULL, 0);
    else
    #endif
        rc = syscall(SYS_futex, (void *)addr, FUTEX_WAIT_PRIVATE, expected,
                     timeout, NULL, 0);
    if (rc == 0 || errno == EAGAIN || errno == EINTR)
        return 0;
    return errno == ETIMEDOUT ? PF_ETIMEDOUT : pf_error_from_errno(errno);
#elif WAIT_ULOCK
    uint32_t us = 0;
    int rc;
    if (!addr || !valid_timeout(timeout))
        return PF_EINVAL;
    if (load32(addr) != expected)
        return 0;
    if (timeout) {
        uint64_t t = (uint64_t)timeout->tv_sec * 1000000u
                   + ((uint64_t)timeout->tv_nsec + 999u) / 1000u;
        if (t == 0)
            return PF_ETIMEDOUT;
        us = t > UINT32_MAX ? UINT32_MAX : (uint32_t)t; /* 0 is forever */
    }
    rc = __ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, (void *)addr,
                      expected, us);
    if (rc >= 0 || rc == -EINTR || rc == -EFAULT)
        return 0;
    return rc == -ETIMEDOUT ? PF_ETIMEDOUT : pf_error_from_errno(-rc);
#elif WAIT_UMTX
    struct _umtx_time t;
    int rc;
    if (!addr || !valid_timeout(timeout))
        return PF_EINVAL;
    if (load32(addr) != expected)
        return 0;
    if (timeout) {
        memset(&t, 0, sizeof(t));
        t._timeout = *timeout;
        t._flags = 0; /* relative */
        t._clockid = CLOCK_MONOTONIC;
        rc = _umtx_op((void *)addr, UMTX_OP_WAIT_UINT_PRIVATE, expected,
                      (void *)sizeof(t), &t);
    } else {
        rc = _umtx_op((void *)addr, UMTX_OP_WAIT_UINT_PRIVATE, expected,
                      NULL, NULL);
    }
    if (rc == 0 || errno == EINTR)
        return 0;
    return errno == ETIMEDOUT ? PF_ETIMEDOUT : pf_error_from_errno(errno);
#elif WAIT_OPENBSD
    int rc;
    if (!addr || !valid_timeout(timeout))
        return PF_EINVAL;
    if (load32(addr) != expected)
        return 0;
    rc = futex((volatile uint32_t *)addr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
               (int)expected, timeout, NULL);
    if (rc == 0 || errno == EAGAIN || errno == EINTR)
        return 0;
    return errno == ETIMEDOUT ? PF_ETIMEDOUT : pf_error_from_errno(errno);
#elif WAIT_WINDOWS
    DWORD ms = INFINITE;
    if (!addr || !valid_timeout(timeout))
        return PF_EINVAL;
    if (load32(addr) != expected)
        return 0;
    if (timeout) {
        uint64_t t = (uint64_t)timeout->tv_sec * 1000u
                   + ((uint64_t)timeout->tv_nsec + 999999u) / 1000000u;
        ms = t >= INFINITE ? INFINITE - 1 : (DWORD)t;
    }
    if (WaitOnAddress((volatile VOID *)addr, &expected, sizeof(expected), ms))
        return 0;
    return GetLastError() == ERROR_TIMEOUT ? PF_ETIMEDOUT : 0;
#else
    return pf__atomic_wait_fallback(addr, expected, timeout);
#endif
}

static void wake(const volatile void *addr, bool all) {
    if (!addr)
        return;
#if WAIT_FUTEX
    syscall(SYS_futex, (void *)addr, FUTEX_WAKE_PRIVATE, all ? INT_MAX : 1,
            NULL, NULL, 0);
#elif WAIT_ULOCK
    __ulock_wake(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO | (all ? ULF_WAKE_ALL : 0),
                 (void *)addr, 0);
#elif WAIT_UMTX
    _umtx_op((void *)addr, UMTX_OP_WAKE_PRIVATE, all ? INT_MAX : 1, NULL, NULL);
#elif WAIT_OPENBSD
    futex((volatile uint32_t *)addr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
          all ? INT_MAX : 1, NULL, NULL);
#elif WAIT_WINDOWS
    if (all)
        WakeByAddressAll((PVOID)addr);
    else
        WakeByAddressSingle((PVOID)addr);
#else
    pf__atomic_wake_fallback(addr, all);
#endif
}

PF_API void pf_atomic_wake_one(const volatile void *addr) {
    wake(addr, false);
}

PF_API void pf_atomic_wake_all(const volatile void *addr) {
    wake(addr, true);
}
