/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/threads.h>. Without threads (PF_HAS_THREADS 0) only the
   single-thread behaviour is checked. */

#include "../src/internal.h"

#include <cpolyfill/atomic.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <stdint.h>

#include "check.h"

#define NTHREADS 8

static const int mtx_types[] = {
    pf_mtx_plain,
    pf_mtx_timed,
    pf_mtx_plain | pf_mtx_recursive,
    pf_mtx_timed | pf_mtx_recursive,
};
#define NTYPES (int)(sizeof(mtx_types) / sizeof(mtx_types[0]))

static void deadline_ms(struct timespec *ts, long ms) {
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(ts, (int64_t)ms * 1000000), 0);
}

#if PF_HAS_THREADS

static int join_all(pf_thrd_t *t, int n) {
    int i, sum = 0;
    for (i = 0; i < n; i++) {
        int res = -1;
        PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t[i], &res), pf_thrd_success);
        sum += res;
    }
    return sum;
}

/* ---- Threads ----------------------------------------------------------- */

static pf_thrd_t main_thread;

static int report(void *arg) {
    PF_ASSERT_ALWAYS(!pf_thrd_equal(pf_thrd_current(), main_thread));
    PF_ASSERT_ALWAYS(pf_thrd_equal(pf_thrd_current(), pf_thrd_current()));
    return (int)(intptr_t)arg * 2;
}

static int exits(void *arg) {
    (void)arg;
    pf_thrd_exit(7);
}

static PF_ATOMIC(int) detached_ran;

static int detached(void *arg) {
    (void)arg;
    PF_ATOMIC_STORE(&detached_ran, 1);
    return 0;
}

static pf_thrd_t created;

static int compare_self(void *arg) {
    (void)arg;
    /* The value pf_thrd_create gave equals the thread's own. */
    while (PF_ATOMIC_LOAD(&detached_ran) != 2)
        pf_thrd_yield();
    return pf_thrd_equal(created, pf_thrd_current()) ? 5 : 0;
}

static int test_create(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int k, res;
    (void)seed, (void)i;

    main_thread = pf_thrd_current();
    PF_ASSERT_ALWAYS(pf_thrd_equal(main_thread, pf_thrd_current()));
    for (k = 0; k < NTHREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], report, (void *)(intptr_t)k), pf_thrd_success
        );
    PF_ASSERT_ALWAYS_EQ(join_all(t, NTHREADS), NTHREADS * (NTHREADS - 1));

    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t[0], exits, NULL), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t[0], &res), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(res, 7);

    PF_ATOMIC_STORE(&detached_ran, 0);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t[0], detached, NULL), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_detach(t[0]), pf_thrd_success);
    while (!PF_ATOMIC_LOAD(&detached_ran))
        pf_thrd_yield();

    PF_ASSERT_ALWAYS_EQ(
        pf_thrd_create(&created, compare_self, NULL), pf_thrd_success
    );
    PF_ATOMIC_STORE(&detached_ran, 2);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_join(created, &res), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(res, 5);
    return 0;
}

static int test_sleep(int seed, int i) {
    struct timespec d = { 0, 10000000 };
    uint64_t start = pf_time_ns();
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_thrd_sleep(&d, NULL), 0);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 10000000u);
    pf_thrd_yield();
    d.tv_nsec = -1;
    PF_ASSERT_ALWAYS_EQ(pf_thrd_sleep(&d, NULL), -2);
    return 0;
}

/* ---- Mutexes ----------------------------------------------------------- */

static pf_mtx_t counter_mtx;
static long counter;

static int count_up(void *arg) {
    int k, recursive = arg != NULL;
    for (k = 0; k < 20000; k++) {
        PF_ASSERT_ALWAYS_EQ(pf_mtx_lock(&counter_mtx), pf_thrd_success);
        if (recursive)
            PF_ASSERT_ALWAYS_EQ(pf_mtx_lock(&counter_mtx), pf_thrd_success);
        counter++;
        if (recursive)
            PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&counter_mtx), pf_thrd_success);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&counter_mtx), pf_thrd_success);
    }
    return 0;
}

static int test_mutex_counter(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int type, k;
    (void)seed, (void)i;

    for (type = 0; type < NTYPES; type++) {
        int recursive = mtx_types[type] & pf_mtx_recursive;
        PF_ASSERT_ALWAYS_EQ(
            pf_mtx_init(&counter_mtx, mtx_types[type]), pf_thrd_success
        );
        counter = 0;
        for (k = 0; k < NTHREADS; k++)
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&t[k], count_up, recursive ? &counter : NULL),
                pf_thrd_success
            );
        join_all(t, NTHREADS);
        PF_ASSERT_ALWAYS_EQ(counter, NTHREADS * 20000L);
        pf_mtx_destroy(&counter_mtx);
    }
    PF_ASSERT_ALWAYS_EQ(pf_mtx_init(&counter_mtx, 8), pf_thrd_error);
    return 0;
}

static pf_mtx_t held;

static int try_held(void *arg) {
    struct timespec d;
    uint64_t start;
    int timed = arg != NULL;

    PF_ASSERT_ALWAYS_EQ(pf_mtx_trylock(&held), pf_thrd_busy);
    if (timed) {
        deadline_ms(&d, 30);
        start = pf_time_ns();
        PF_ASSERT_ALWAYS_EQ(pf_mtx_timedlock(&held, &d), pf_thrd_timedout);
        PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 25000000u);
    }
    return 0;
}

static int lock_later(void *arg) {
    struct timespec d;
    (void)arg;
    deadline_ms(&d, 5000);
    return pf_mtx_timedlock(&held, &d) == pf_thrd_success
        && pf_mtx_unlock(&held) == pf_thrd_success;
}

static int test_mutex_contention(int seed, int i) {
    pf_thrd_t t;
    int type;
    (void)seed, (void)i;

    for (type = 0; type < NTYPES; type++) {
        int timed = mtx_types[type] & pf_mtx_timed;
        int recursive = mtx_types[type] & pf_mtx_recursive;
        struct timespec d;

        PF_ASSERT_ALWAYS_EQ(
            pf_mtx_init(&held, mtx_types[type]), pf_thrd_success
        );
        PF_ASSERT_ALWAYS_EQ(pf_mtx_lock(&held), pf_thrd_success);
        if (recursive) {
            PF_ASSERT_ALWAYS_EQ(pf_mtx_trylock(&held), pf_thrd_success);
            PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&held), pf_thrd_success);
        }
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t, try_held, timed ? &held : NULL), pf_thrd_success
        );
        join_all(&t, 1);

        /* A waiter gets the mutex once it is released. */
        if (timed) {
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&t, lock_later, NULL), pf_thrd_success
            );
            pf_sleep_ns(10000000);
        }
        PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&held), pf_thrd_success);
        if (timed)
            PF_ASSERT_ALWAYS_EQ(join_all(&t, 1), 1);

        PF_ASSERT_ALWAYS_EQ(pf_mtx_trylock(&held), pf_thrd_success);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&held), pf_thrd_success);
        if (timed) {
            deadline_ms(&d, 1000);
            PF_ASSERT_ALWAYS_EQ(pf_mtx_timedlock(&held, &d), pf_thrd_success);
            PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&held), pf_thrd_success);
        }
        pf_mtx_destroy(&held);
    }
    return 0;
}

/* ---- Condition variables ----------------------------------------------- */

static pf_mtx_t queue_mtx;
static pf_cnd_t queue_cnd;
static int queue_items, queue_done;

static int consumer(void *arg) {
    int got = 0;
    (void)arg;
    pf_mtx_lock(&queue_mtx);
    for (;;) {
        while (queue_items == 0 && !queue_done)
            PF_ASSERT_ALWAYS_EQ(
                pf_cnd_wait(&queue_cnd, &queue_mtx), pf_thrd_success
            );
        if (queue_items == 0)
            break;
        queue_items--;
        got++;
    }
    pf_mtx_unlock(&queue_mtx);
    return got;
}

static int test_condition(int seed, int i) {
    pf_thrd_t t[4];
    int type, k;
    (void)seed, (void)i;

    for (type = 0; type < NTYPES; type++) {
        struct timespec d;
        uint64_t start;

        PF_ASSERT_ALWAYS_EQ(
            pf_mtx_init(&queue_mtx, mtx_types[type]), pf_thrd_success
        );
        PF_ASSERT_ALWAYS_EQ(pf_cnd_init(&queue_cnd), pf_thrd_success);
        queue_items = queue_done = 0;
        for (k = 0; k < 4; k++)
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&t[k], consumer, NULL), pf_thrd_success
            );
        for (k = 0; k < 1000; k++) {
            pf_mtx_lock(&queue_mtx);
            queue_items++;
            pf_cnd_signal(&queue_cnd);
            pf_mtx_unlock(&queue_mtx);
        }
        pf_mtx_lock(&queue_mtx);
        queue_done = 1;
        pf_cnd_broadcast(&queue_cnd);
        pf_mtx_unlock(&queue_mtx);
        PF_ASSERT_ALWAYS_EQ(join_all(t, 4), 1000);

        /* Timing out gives the mutex back locked. */
        pf_mtx_lock(&queue_mtx);
        deadline_ms(&d, 20);
        start = pf_time_ns();
        k = pf_cnd_timedwait(&queue_cnd, &queue_mtx, &d);
        while (k == pf_thrd_success) /* spurious wakeup */
            k = pf_cnd_timedwait(&queue_cnd, &queue_mtx, &d);
        PF_ASSERT_ALWAYS_EQ(k, pf_thrd_timedout);
        PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 15000000u);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&queue_mtx), pf_thrd_success);

        pf_cnd_destroy(&queue_cnd);
        pf_mtx_destroy(&queue_mtx);
    }
    return 0;
}

/* ---- Once and tss ------------------------------------------------------ */

static pf_once_flag once = PF_ONCE_FLAG_INIT;
static PF_ATOMIC(int) once_calls;

static void once_fn(void) {
    pf_sleep_ns(1000000);
    PF_ATOMIC_FETCH_ADD(&once_calls, 1);
}

static int call_once_thread(void *arg) {
    (void)arg;
    pf_call_once(&once, once_fn);
    return PF_ATOMIC_LOAD(&once_calls);
}

static int test_once(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int k;
    (void)seed, (void)i;

    for (k = 0; k < NTHREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], call_once_thread, NULL), pf_thrd_success
        );
    PF_ASSERT_ALWAYS_EQ(join_all(t, NTHREADS), NTHREADS); /* each saw 1 */
    pf_call_once(&once, once_fn);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&once_calls), 1);
    return 0;
}

static pf_tss_t key, again_key;
static PF_ATOMIC(int) dtor_calls, again_calls;

static void dtor(void *value) {
    PF_ASSERT_ALWAYS_EQ(*(int *)value, 42);
    PF_ATOMIC_FETCH_ADD(&dtor_calls, 1);
}

/* Sets its value (a per-thread counter) again twice, so it runs on the
   next passes too. */
static void dtor_again(void *value) {
    PF_ATOMIC_FETCH_ADD(&again_calls, 1);
    if (++*(int *)value < 3)
        pf_tss_set(again_key, value);
    else
        free(value);
}

static int use_tss(void *arg) {
    static int forty_two = 42;
    (void)arg;
    PF_ASSERT_ALWAYS(pf_tss_get(key) == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_tss_set(key, &forty_two), pf_thrd_success);
    PF_ASSERT_ALWAYS(pf_tss_get(key) == &forty_two);
    PF_ASSERT_ALWAYS_EQ(
        pf_tss_set(again_key, calloc(1, sizeof(int))), pf_thrd_success
    );
    return 0;
}

static int test_tss(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int k, local = 1;
    (void)seed, (void)i;

    PF_ATOMIC_STORE(&dtor_calls, 0);
    PF_ATOMIC_STORE(&again_calls, 0);
    PF_ASSERT_ALWAYS_EQ(pf_tss_create(&key, dtor), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_tss_create(&again_key, dtor_again), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_tss_set(key, &local), pf_thrd_success);
    for (k = 0; k < NTHREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], use_tss, NULL), pf_thrd_success
        );
    join_all(t, NTHREADS);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&dtor_calls), NTHREADS);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&again_calls), 3 * NTHREADS);
    PF_ASSERT_ALWAYS(pf_tss_get(key) == &local); /* ours is untouched */
    pf_tss_set(key, NULL);
    pf_tss_delete(key);
    pf_tss_delete(again_key);
    return 0;
}

/* ---- Additions --------------------------------------------------------- */

static int named(void *arg) {
    char buf[64];
    volatile char big[200000];
    big[0] = 1;
    big[sizeof(big) - 1] = 1;
    (void)arg;
    if (pf_thrd_get_name(buf, sizeof(buf)) == PF_ENOTSUP)
        return 1;
    return strcmp(buf, "worker") == 0 && big[0] == big[sizeof(big) - 1];
}

static uint64_t other_id;

static int get_id(void *arg) {
    (void)arg;
    other_id = pf_thrd_id();
    return 0;
}

static int test_attributes(int seed, int i) {
    pf_thrd_attr_t attr;
    pf_thrd_t t;
    char buf[64], small[4];
    int rc;
    (void)seed, (void)i;

    attr.stack_size = 1 << 20;
    attr.name = "worker";
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create_ex(&t, named, NULL, &attr), 0);
    PF_ASSERT_ALWAYS_EQ(join_all(&t, 1), 1);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create_ex(&t, get_id, NULL, NULL), 0);
    join_all(&t, 1);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create_ex(NULL, get_id, NULL, NULL), PF_EINVAL);

    PF_ASSERT_ALWAYS(pf_thrd_id() != 0);
    PF_ASSERT_ALWAYS(other_id != 0 && other_id != pf_thrd_id());
    PF_ASSERT_ALWAYS_EQ(pf_thrd_id(), pf_thrd_id());

    /* Names, cut at a character boundary where the system limits them. */
    rc = pf_thrd_set_name("main-\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9");
    if (rc == PF_ENOTSUP)
        return SKIP("thread names are not supported");
    PF_ASSERT_ALWAYS_EQ(rc, 0);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_get_name(buf, sizeof(buf)), 0);
    #if PF_OS_LINUX
    PF_ASSERT_ALWAYS_STR_EQ(
        buf, "main-\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_thrd_set_name(
            "main-\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
        ),
        0
    );
    PF_ASSERT_ALWAYS_EQ(pf_thrd_get_name(buf, sizeof(buf)), 0);
    PF_ASSERT_ALWAYS_STR_EQ(
        buf, "main-\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
    );
    #endif
    PF_ASSERT_ALWAYS_EQ(pf_thrd_get_name(small, sizeof(small)), PF_ERANGE);
    PF_ASSERT_ALWAYS_STR_EQ(small, "mai");
    PF_ASSERT_ALWAYS_EQ(pf_thrd_get_name(buf, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_name(NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_name("cpolyfill-test"), 0);
    return 0;
}

static int test_affinity(int seed, int i) {
    uint64_t mask[16], saved[16], one[16];
    size_t cpu;
    int rc;
    (void)seed, (void)i;

    rc = pf_thrd_get_affinity(saved, 1024);
    if (rc == PF_ENOTSUP)
        return SKIP("no thread affinity");
    PF_ASSERT_ALWAYS_EQ(rc, 0);
    for (cpu = 0; cpu < 1024 && !(saved[cpu / 64] >> (cpu % 64) & 1); cpu++) { }
    PF_ASSERT_ALWAYS_LT(cpu, 1024);

    memset(one, 0, sizeof(one));
    one[cpu / 64] = (uint64_t)1 << (cpu % 64);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_affinity(one, 1024), 0);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_get_affinity(mask, 1024), 0);
    PF_ASSERT_ALWAYS_MEM_EQ(mask, one, sizeof(one));
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_affinity(saved, 1024), 0);

    memset(mask, 0, sizeof(mask));
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_affinity(mask, 1024), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_affinity(NULL, 64), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_get_affinity(mask, 0), PF_EINVAL);
    return 0;
}

static int lower_priority(void *arg) {
    (void)arg;
    return pf_thrd_set_priority(PF_THRD_PRIORITY_LOWEST);
}

static int test_priority(int seed, int i) {
    pf_thrd_t t;
    int rc;
    (void)seed, (void)i;

    /* Lowering is always allowed; do it in a thread of its own. */
    PF_ASSERT_ALWAYS_EQ(
        pf_thrd_create(&t, lower_priority, NULL), pf_thrd_success
    );
    rc = join_all(&t, 1);
    PF_ASSERT_ALWAYS(rc == 0 || rc == PF_ENOTSUP);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_priority(3), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_priority(-3), PF_EINVAL);
    return 0;
}

/* ---- Reader-writer locks ----------------------------------------------- */

static pf_rwlock_t rw;
static long rw_value;
static PF_ATOMIC(int) readers_in, max_readers;

static int rw_worker(void *arg) {
    int k, writer = arg != NULL;
    for (k = 0; k < 2000; k++) {
        if (writer && k % 4 == 0) {
            PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrlock(&rw), 0);
            PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&readers_in), 0);
            rw_value++;
            PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrunlock(&rw), 0);
        } else {
            int now;
            PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdlock(&rw), 0);
            now = PF_ATOMIC_FETCH_ADD(&readers_in, 1) + 1;
            if (now > PF_ATOMIC_LOAD(&max_readers))
                PF_ATOMIC_STORE(&max_readers, now);
            (void)rw_value;
            PF_ATOMIC_FETCH_SUB(&readers_in, 1);
            PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdunlock(&rw), 0);
        }
    }
    return 0;
}

static int rw_try(void *arg) {
    struct timespec d;
    int writer_holds = arg != NULL;
    deadline_ms(&d, 20);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_trywrlock(&rw), PF_EBUSY);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_timedwrlock(&rw, &d), PF_ETIMEDOUT);
    if (writer_holds) {
        PF_ASSERT_ALWAYS_EQ(pf_rwlock_tryrdlock(&rw), PF_EBUSY);
        deadline_ms(&d, 20);
        PF_ASSERT_ALWAYS_EQ(pf_rwlock_timedrdlock(&rw, &d), PF_ETIMEDOUT);
    } else {
        /* Readers share. */
        PF_ASSERT_ALWAYS_EQ(pf_rwlock_tryrdlock(&rw), 0);
        PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdunlock(&rw), 0);
    }
    return 0;
}

static int test_rwlock(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_rwlock_init(&rw), 0);
    rw_value = 0;
    for (k = 0; k < NTHREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], rw_worker, k % 2 ? &rw : NULL),
            pf_thrd_success
        );
    join_all(t, NTHREADS);
    PF_ASSERT_ALWAYS_EQ(rw_value, NTHREADS / 2 * 500L);

    PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdlock(&rw), 0);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t[0], rw_try, NULL), pf_thrd_success);
    join_all(t, 1);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdunlock(&rw), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrlock(&rw), 0);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t[0], rw_try, &rw), pf_thrd_success);
    join_all(t, 1);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrunlock(&rw), 0);

    PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdunlock(&rw), PF_EPERM);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrunlock(&rw), PF_EPERM);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_trywrlock(&rw), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrunlock(&rw), 0);
    pf_rwlock_free(&rw);
    return 0;
}

/* ---- Spin locks and barriers ------------------------------------------- */

static pf_spin_t spin;

static int spin_up(void *arg) {
    int k;
    (void)arg;
    for (k = 0; k < 20000; k++) {
        PF_ASSERT_ALWAYS_EQ(pf_spin_lock(&spin), 0);
        counter++;
        PF_ASSERT_ALWAYS_EQ(pf_spin_unlock(&spin), 0);
    }
    return 0;
}

static int test_spin(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_spin_init(&spin), 0);
    counter = 0;
    for (k = 0; k < NTHREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], spin_up, NULL), pf_thrd_success
        );
    join_all(t, NTHREADS);
    PF_ASSERT_ALWAYS_EQ(counter, NTHREADS * 20000L);
    PF_ASSERT_ALWAYS_EQ(pf_spin_trylock(&spin), 0);
    PF_ASSERT_ALWAYS_EQ(pf_spin_trylock(&spin), PF_EBUSY);
    PF_ASSERT_ALWAYS_EQ(pf_spin_unlock(&spin), 0);
    pf_spin_free(&spin);
    return 0;
}

    #define ROUNDS 200

static pf_barrier_t barrier;
static PF_ATOMIC(int) phase_count[ROUNDS], serials;

static int barrier_worker(void *arg) {
    int r;
    (void)arg;
    for (r = 0; r < ROUNDS; r++) {
        int rc;
        PF_ATOMIC_FETCH_ADD(&phase_count[r], 1);
        rc = pf_barrier_wait(&barrier);
        PF_ASSERT_ALWAYS(rc == 0 || rc == PF_BARRIER_SERIAL);
        if (rc == PF_BARRIER_SERIAL)
            PF_ATOMIC_FETCH_ADD(&serials, 1);
        /* Nobody passes before everyone arrived. */
        PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&phase_count[r]), NTHREADS);
    }
    return 0;
}

static int test_barrier(int seed, int i) {
    pf_thrd_t t[NTHREADS];
    int k;
    (void)seed, (void)i;

    for (k = 0; k < ROUNDS; k++)
        PF_ATOMIC_STORE(&phase_count[k], 0);
    PF_ATOMIC_STORE(&serials, 0);
    PF_ASSERT_ALWAYS_EQ(pf_barrier_init(&barrier, NTHREADS), 0);
    for (k = 0; k < NTHREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], barrier_worker, NULL), pf_thrd_success
        );
    join_all(t, NTHREADS);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&serials), ROUNDS);
    pf_barrier_free(&barrier);

    PF_ASSERT_ALWAYS_EQ(pf_barrier_init(&barrier, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_barrier_init(&barrier, 1), 0);
    PF_ASSERT_ALWAYS_EQ(pf_barrier_wait(&barrier), PF_BARRIER_SERIAL);
    pf_barrier_free(&barrier);
    return 0;
}

#else /* !PF_HAS_THREADS */

static int no_thread(void *arg) {
    (void)arg;
    return 0;
}

static int test_no_threads(int seed, int i) {
    pf_thrd_t t;
    pf_mtx_t m;
    pf_cnd_t c;
    pf_rwlock_t rw;
    pf_barrier_t b;
    pf_spin_t s;
    pf_tss_t k;
    struct timespec d;
    uint64_t mask = 1;
    int value = 3;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_thrd_create(&t, no_thread, NULL), pf_thrd_error);
    PF_ASSERT_ALWAYS_EQ(
        pf_thrd_create_ex(&t, no_thread, NULL, NULL), PF_ENOSYS
    );
    PF_ASSERT_ALWAYS(pf_thrd_equal(pf_thrd_current(), pf_thrd_current()));
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_affinity(&mask, 1), PF_ENOSYS);
    PF_ASSERT_ALWAYS_EQ(pf_thrd_set_priority(0), PF_ENOSYS);

    PF_ASSERT_ALWAYS_EQ(pf_mtx_init(&m, pf_mtx_timed), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_mtx_lock(&m), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_mtx_trylock(&m), pf_thrd_busy);
    PF_ASSERT_ALWAYS_EQ(pf_mtx_lock(&m), pf_thrd_error);
    deadline_ms(&d, 5);
    PF_ASSERT_ALWAYS_EQ(pf_mtx_timedlock(&m, &d), pf_thrd_timedout);
    PF_ASSERT_ALWAYS_EQ(pf_cnd_init(&c), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_cnd_wait(&c, &m), pf_thrd_error);
    deadline_ms(&d, 5);
    PF_ASSERT_ALWAYS_EQ(pf_cnd_timedwait(&c, &m, &d), pf_thrd_timedout);
    PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&m), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&m), pf_thrd_error);

    PF_ASSERT_ALWAYS_EQ(pf_tss_create(&k, NULL), pf_thrd_success);
    PF_ASSERT_ALWAYS_EQ(pf_tss_set(k, &value), pf_thrd_success);
    PF_ASSERT_ALWAYS(pf_tss_get(k) == &value);
    pf_tss_delete(k);

    PF_ASSERT_ALWAYS_EQ(pf_rwlock_init(&rw), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdlock(&rw), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_trywrlock(&rw), PF_EBUSY);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_wrlock(&rw), PF_EDEADLK);
    PF_ASSERT_ALWAYS_EQ(pf_rwlock_rdunlock(&rw), 0);
    pf_rwlock_free(&rw);

    PF_ASSERT_ALWAYS_EQ(pf_barrier_init(&b, 2), 0);
    PF_ASSERT_ALWAYS_EQ(pf_barrier_wait(&b), PF_EDEADLK);
    pf_barrier_free(&b);

    PF_ASSERT_ALWAYS_EQ(pf_spin_init(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_spin_lock(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_spin_lock(&s), PF_EDEADLK);
    PF_ASSERT_ALWAYS_EQ(pf_spin_unlock(&s), 0);
    return 0;
}

#endif

/* ---- Single thread, both builds ---------------------------------------- */

static int once_count;

static void count_once(void) { once_count++; }

static int test_single_thread(int seed, int i) {
    static pf_once_flag flag = PF_ONCE_FLAG_INIT;
    pf_mtx_t m;
    int type;
    (void)seed, (void)i;

    pf_call_once(&flag, count_once);
    pf_call_once(&flag, count_once);
    PF_ASSERT_ALWAYS_EQ(once_count, 1);

    for (type = 0; type < NTYPES; type++) {
        struct timespec d;
        PF_ASSERT_ALWAYS_EQ(pf_mtx_init(&m, mtx_types[type]), pf_thrd_success);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_lock(&m), pf_thrd_success);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&m), pf_thrd_success);
        deadline_ms(&d, 100);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_timedlock(&m, &d), pf_thrd_success);
        PF_ASSERT_ALWAYS_EQ(pf_mtx_unlock(&m), pf_thrd_success);
        pf_mtx_destroy(&m);
    }
    return 0;
}

pf_test_t suite_threads[] = {
    { test_single_thread, "single_thread" },
#if PF_HAS_THREADS
    { test_create, "create" },
    { test_sleep, "sleep" },
    { test_mutex_counter, "mutex_counter" },
    { test_mutex_contention, "mutex_contention" },
    { test_condition, "condition" },
    { test_once, "once" },
    { test_tss, "tss" },
    { test_attributes, "attributes" },
    { test_affinity, "affinity" },
    { test_priority, "priority" },
    { test_rwlock, "rwlock" },
    { test_spin, "spin" },
    { test_barrier, "barrier" },
#else
    { test_no_threads, "no_threads" },
#endif
    { 0 },
};
