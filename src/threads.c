/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The parts of <cpolyfill/threads.h> built on the C11 primitives
    (reader-writer locks, barriers, spin locks), and the backend for builds
    without threads. The POSIX and Win32 backends are in posix/threads.c and
    win32/threads.c.
*/

#include "internal.h"

#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* The PF_E* code of a pf_thrd_* result. */
static int error_of(int rc) {
    switch (rc) {
    case pf_thrd_success: return 0;
    case pf_thrd_busy: return PF_EBUSY;
    case pf_thrd_nomem: return PF_ENOMEM;
    case pf_thrd_timedout: return PF_ETIMEDOUT;
    default: return PF_HAS_THREADS ? PF_EINVAL : PF_EDEADLK;
    }
}

/* ---- Reader-writer locks ----------------------------------------------- */

PF_API int pf_rwlock_init(pf_rwlock_t *l) {
    if (!l)
        return PF_EINVAL;
    memset(l, 0, sizeof(*l));
    if (pf_mtx_init(&l->pf__mtx, pf_mtx_plain) != pf_thrd_success)
        return PF_ENOMEM;
    if (pf_cnd_init(&l->pf__readers) != pf_thrd_success) {
        pf_mtx_destroy(&l->pf__mtx);
        return PF_ENOMEM;
    }
    if (pf_cnd_init(&l->pf__writers) != pf_thrd_success) {
        pf_cnd_destroy(&l->pf__readers);
        pf_mtx_destroy(&l->pf__mtx);
        return PF_ENOMEM;
    }
    return 0;
}

PF_API void pf_rwlock_free(pf_rwlock_t *l) {
    if (!l)
        return;
    pf_cnd_destroy(&l->pf__writers);
    pf_cnd_destroy(&l->pf__readers);
    pf_mtx_destroy(&l->pf__mtx);
}

static int wait_on(pf_cnd_t *c, pf_mtx_t *m, const struct timespec *deadline) {
    return deadline ? pf_cnd_timedwait(c, m, deadline) : pf_cnd_wait(c, m);
}

static int rdlock(pf_rwlock_t *l, const struct timespec *deadline) {
    int rc = pf_thrd_success;
    if (pf_mtx_lock(&l->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    /* Writers first, so a stream of readers cannot starve them. */
    while ((l->pf__writer || l->pf__waiting) && rc == pf_thrd_success)
        rc = wait_on(&l->pf__readers, &l->pf__mtx, deadline);
    if (rc == pf_thrd_success)
        l->pf__active++;
    pf_mtx_unlock(&l->pf__mtx);
    return error_of(rc);
}

static int wrlock(pf_rwlock_t *l, const struct timespec *deadline) {
    int rc = pf_thrd_success;
    if (pf_mtx_lock(&l->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    l->pf__waiting++;
    while ((l->pf__writer || l->pf__active) && rc == pf_thrd_success)
        rc = wait_on(&l->pf__writers, &l->pf__mtx, deadline);
    l->pf__waiting--;
    if (rc == pf_thrd_success)
        l->pf__writer = 1;
    else if (!l->pf__waiting && !l->pf__writer)
        pf_cnd_broadcast(&l->pf__readers); /* they waited for us */
    pf_mtx_unlock(&l->pf__mtx);
    return error_of(rc);
}

PF_API int pf_rwlock_rdlock(pf_rwlock_t *l) {
    return l ? rdlock(l, NULL) : PF_EINVAL;
}

PF_API int pf_rwlock_wrlock(pf_rwlock_t *l) {
    return l ? wrlock(l, NULL) : PF_EINVAL;
}

PF_API int pf_rwlock_timedrdlock(
    pf_rwlock_t *l, const struct timespec *deadline
) {
    return l && deadline ? rdlock(l, deadline) : PF_EINVAL;
}

PF_API int pf_rwlock_timedwrlock(
    pf_rwlock_t *l, const struct timespec *deadline
) {
    return l && deadline ? wrlock(l, deadline) : PF_EINVAL;
}

PF_API int pf_rwlock_tryrdlock(pf_rwlock_t *l) {
    int rc = 0;
    if (!l || pf_mtx_lock(&l->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    if (l->pf__writer || l->pf__waiting)
        rc = PF_EBUSY;
    else
        l->pf__active++;
    pf_mtx_unlock(&l->pf__mtx);
    return rc;
}

PF_API int pf_rwlock_trywrlock(pf_rwlock_t *l) {
    int rc = 0;
    if (!l || pf_mtx_lock(&l->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    if (l->pf__writer || l->pf__active)
        rc = PF_EBUSY;
    else
        l->pf__writer = 1;
    pf_mtx_unlock(&l->pf__mtx);
    return rc;
}

PF_API int pf_rwlock_rdunlock(pf_rwlock_t *l) {
    int rc = 0;
    if (!l || pf_mtx_lock(&l->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    if (!l->pf__active)
        rc = PF_EPERM;
    else if (--l->pf__active == 0 && l->pf__waiting)
        pf_cnd_signal(&l->pf__writers);
    pf_mtx_unlock(&l->pf__mtx);
    return rc;
}

PF_API int pf_rwlock_wrunlock(pf_rwlock_t *l) {
    int rc = 0;
    if (!l || pf_mtx_lock(&l->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    if (!l->pf__writer) {
        rc = PF_EPERM;
    } else {
        l->pf__writer = 0;
        if (l->pf__waiting)
            pf_cnd_signal(&l->pf__writers);
        else
            pf_cnd_broadcast(&l->pf__readers);
    }
    pf_mtx_unlock(&l->pf__mtx);
    return rc;
}

/* ---- Barriers ---------------------------------------------------------- */

PF_API int pf_barrier_init(pf_barrier_t *b, unsigned count) {
    if (!b || count == 0)
        return PF_EINVAL;
    memset(b, 0, sizeof(*b));
    if (pf_mtx_init(&b->pf__mtx, pf_mtx_plain) != pf_thrd_success)
        return PF_ENOMEM;
    if (pf_cnd_init(&b->pf__cnd) != pf_thrd_success) {
        pf_mtx_destroy(&b->pf__mtx);
        return PF_ENOMEM;
    }
    b->pf__count = count;
    return 0;
}

PF_API void pf_barrier_free(pf_barrier_t *b) {
    if (!b)
        return;
    pf_cnd_destroy(&b->pf__cnd);
    pf_mtx_destroy(&b->pf__mtx);
}

PF_API int pf_barrier_wait(pf_barrier_t *b) {
    unsigned generation;
    int rc = pf_thrd_success;

    if (!b || pf_mtx_lock(&b->pf__mtx) != pf_thrd_success)
        return PF_EINVAL;
    generation = b->pf__generation;
    if (++b->pf__waiting == b->pf__count) {
        b->pf__waiting = 0;
        b->pf__generation++;
        pf_cnd_broadcast(&b->pf__cnd);
        pf_mtx_unlock(&b->pf__mtx);
        return PF_BARRIER_SERIAL;
    }
    while (generation == b->pf__generation && rc == pf_thrd_success)
        rc = pf_cnd_wait(&b->pf__cnd, &b->pf__mtx);
    if (rc != pf_thrd_success)
        b->pf__waiting--;
    pf_mtx_unlock(&b->pf__mtx);
    return error_of(rc);
}

/* ---- Spin locks -------------------------------------------------------- */

PF_API int pf_spin_init(pf_spin_t *s) {
    if (!s)
        return PF_EINVAL;
    s->pf__lock = 0;
    return 0;
}

PF_API void pf_spin_free(pf_spin_t *s) {
    (void)s;
}

PF_API int pf_spin_lock(pf_spin_t *s) {
    if (!s)
        return PF_EINVAL;
#if !PF_HAS_THREADS
    if (s->pf__lock)
        return PF_EDEADLK; /* nobody else could release it */
#endif
    pf__spin_lock(&s->pf__lock);
    return 0;
}

PF_API int pf_spin_trylock(pf_spin_t *s) {
    if (!s)
        return PF_EINVAL;
    return pf__spin_trylock(&s->pf__lock) ? 0 : PF_EBUSY;
}

PF_API int pf_spin_unlock(pf_spin_t *s) {
    if (!s)
        return PF_EINVAL;
    pf__spin_unlock(&s->pf__lock);
    return 0;
}

/* ---- Backend without threads ------------------------------------------- */

#if !PF_HAS_THREADS

    #include <limits.h>

/* The only thread. */
    #define MAIN_THREAD 1ul

PF_API int pf_thrd_create(pf_thrd_t *thr, pf_thrd_start_t func, void *arg) {
    (void)thr, (void)func, (void)arg;
    return pf_thrd_error;
}

PF_API int pf_thrd_create_ex(
    pf_thrd_t *thr, pf_thrd_start_t func, void *arg,
    const pf_thrd_attr_t *attr
) {
    (void)thr, (void)func, (void)arg, (void)attr;
    return PF_ENOSYS;
}

PF_API int pf_thrd_equal(pf_thrd_t a, pf_thrd_t b) {
    return a == b;
}

PF_API pf_thrd_t pf_thrd_current(void) {
    return MAIN_THREAD;
}

PF_API int pf_thrd_sleep(
    const struct timespec *duration, struct timespec *remaining
) {
    if (pf_nanosleep(duration, remaining) == 0)
        return 0;
    return errno == EINTR ? -1 : -2;
}

PF_API void pf_thrd_yield(void) {
}

PF_NORETURN PF_API void pf_thrd_exit(int res) {
    exit(res);
}

PF_API int pf_thrd_detach(pf_thrd_t thr) {
    (void)thr;
    return pf_thrd_error;
}

PF_API int pf_thrd_join(pf_thrd_t thr, int *res) {
    (void)thr, (void)res;
    return pf_thrd_error;
}

/* Sleeps until the deadline. */
static void sleep_until(const struct timespec *deadline) {
    struct timespec now;
    while (pf_timespec_get(&now, PF_TIME_UTC)
           && pf_timespec_cmp(now, *deadline) < 0) {
        struct timespec left = pf_timespec_sub(*deadline, now);
        pf_nanosleep(&left, NULL);
    }
}

PF_API int pf_mtx_init(pf_mtx_t *mtx, int type) {
    if (!mtx || (type & ~(pf_mtx_timed | pf_mtx_recursive)))
        return pf_thrd_error;
    mtx->pf__type = type;
    mtx->pf__count = 0;
    return pf_thrd_success;
}

PF_API int pf_mtx_lock(pf_mtx_t *mtx) {
    if (mtx->pf__count && !(mtx->pf__type & pf_mtx_recursive))
        return pf_thrd_error; /* would never be released */
    mtx->pf__count++;
    return pf_thrd_success;
}

PF_API int pf_mtx_timedlock(pf_mtx_t *mtx, const struct timespec *deadline) {
    if (mtx->pf__count && !(mtx->pf__type & pf_mtx_recursive)) {
        sleep_until(deadline);
        return pf_thrd_timedout;
    }
    mtx->pf__count++;
    return pf_thrd_success;
}

PF_API int pf_mtx_trylock(pf_mtx_t *mtx) {
    if (mtx->pf__count && !(mtx->pf__type & pf_mtx_recursive))
        return pf_thrd_busy;
    mtx->pf__count++;
    return pf_thrd_success;
}

PF_API int pf_mtx_unlock(pf_mtx_t *mtx) {
    if (!mtx->pf__count)
        return pf_thrd_error;
    mtx->pf__count--;
    return pf_thrd_success;
}

PF_API void pf_mtx_destroy(pf_mtx_t *mtx) {
    (void)mtx;
}

PF_API void pf_call_once(pf_once_flag *flag, void (*func)(void)) {
    if (!flag->pf__done) {
        flag->pf__done = 1;
        func();
    }
}

PF_API int pf_cnd_init(pf_cnd_t *cnd) {
    cnd->pf__unused = 0;
    return pf_thrd_success;
}

PF_API int pf_cnd_signal(pf_cnd_t *cnd) {
    (void)cnd;
    return pf_thrd_success;
}

PF_API int pf_cnd_broadcast(pf_cnd_t *cnd) {
    (void)cnd;
    return pf_thrd_success;
}

PF_API int pf_cnd_wait(pf_cnd_t *cnd, pf_mtx_t *mtx) {
    (void)cnd, (void)mtx;
    return pf_thrd_error; /* nothing could signal it */
}

PF_API int pf_cnd_timedwait(
    pf_cnd_t *cnd, pf_mtx_t *mtx, const struct timespec *deadline
) {
    (void)cnd, (void)mtx;
    sleep_until(deadline);
    return pf_thrd_timedout;
}

PF_API void pf_cnd_destroy(pf_cnd_t *cnd) {
    (void)cnd;
}

    #define TSS_MAX 128

static struct {
    int used;
    pf_tss_dtor_t dtor;
    void *value;
} tss_slots[TSS_MAX];

PF_API int pf_tss_create(pf_tss_t *key, pf_tss_dtor_t dtor) {
    unsigned i;
    for (i = 0; i < TSS_MAX; i++) {
        if (!tss_slots[i].used) {
            tss_slots[i].used = 1;
            tss_slots[i].dtor = dtor;
            tss_slots[i].value = NULL;
            *key = i;
            return pf_thrd_success;
        }
    }
    return pf_thrd_error;
}

PF_API void pf_tss_delete(pf_tss_t key) {
    if (key < TSS_MAX)
        tss_slots[key].used = 0;
}

PF_API void *pf_tss_get(pf_tss_t key) {
    return key < TSS_MAX && tss_slots[key].used ? tss_slots[key].value : NULL;
}

PF_API int pf_tss_set(pf_tss_t key, void *value) {
    if (key >= TSS_MAX || !tss_slots[key].used)
        return pf_thrd_error;
    tss_slots[key].value = value;
    return pf_thrd_success;
}

static char thread_name[64];

PF_API int pf_thrd_set_name(const char *name) {
    size_t n;
    if (!name)
        return PF_EINVAL;
    n = pf__utf8_truncate(name, sizeof(thread_name) - 1);
    memcpy(thread_name, name, n);
    thread_name[n] = 0;
    return 0;
}

PF_API int pf_thrd_get_name(char *buf, size_t size) {
    return pf__copy_name(buf, size, thread_name);
}

PF_API uint64_t pf_thrd_id(void) {
    return MAIN_THREAD;
}

PF_API int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus) {
    (void)mask, (void)ncpus;
    return PF_ENOSYS;
}

PF_API int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus) {
    (void)mask, (void)ncpus;
    return PF_ENOSYS;
}

PF_API int pf_thrd_set_priority(int priority) {
    (void)priority;
    return PF_ENOSYS;
}

#endif /* !PF_HAS_THREADS */

/* ---- Helpers for the backends ------------------------------------------ */

size_t pf__utf8_truncate(const char *s, size_t max) {
    size_t n = strlen(s);
    if (n <= max)
        return n;
    /* Back up over continuation bytes to a character boundary. */
    n = max;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80)
        n--;
    return n;
}

int pf__copy_name(char *buf, size_t size, const char *name) {
    size_t n;
    if (!buf || size == 0)
        return PF_EINVAL;
    n = strlen(name);
    if (n < size) {
        memcpy(buf, name, n + 1);
        return 0;
    }
    n = pf__utf8_truncate(name, size - 1);
    memcpy(buf, name, n);
    buf[n] = 0;
    return PF_ERANGE;
}
