/*
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
*/

#include "check.h"
#include "../include/pf_threads.h"
#include "util.h"
#include <stdint.h>

#ifdef _WIN32
    #define HAVE_TSS_DTOR_TEST 0
#else
    #define HAVE_TSS_DTOR_TEST 1
#endif

/* ---------------------------------------------------------------- */
/* A flag shared between threads, guarded by a mutex                 */
/* ---------------------------------------------------------------- */

struct flag {
    mtx_t lock;
    int value;
};

static void flag_init(struct flag *f) {
    mtx_init(&f->lock, mtx_plain);
    f->value = 0;
}

static void flag_set(struct flag *f) {
    mtx_lock(&f->lock);
    f->value = 1;
    mtx_unlock(&f->lock);
}

static int flag_get(struct flag *f) {
    int v;
    mtx_lock(&f->lock);
    v = f->value;
    mtx_unlock(&f->lock);
    return v;
}

/* Waits for up to four seconds. */
static int flag_wait(struct flag *f) {
    int i;
    for (i = 0; i < 2000 && !flag_get(f); i++)
        test_sleep_ms(2);
    return flag_get(f);
}

/* ---------------------------------------------------------------- */
/* Threads                                                           */
/* ---------------------------------------------------------------- */

static int return_arg(void *arg) { return (int)(intptr_t)arg; }

static int test_threads_create_join(int seed, int rep) {
    thrd_t t;
    int result = -1;

    CHECK(thrd_create(&t, return_arg, (void *)(intptr_t)42) == thrd_success);
    CHECK(thrd_join(t, &result) == thrd_success);
    CHECK(result == 42);

    /* The result is optional, and may be negative. */
    CHECK(thrd_create(&t, return_arg, (void *)(intptr_t)-7) == thrd_success);
    CHECK(thrd_join(t, &result) == thrd_success);
    CHECK(result == -7);

    CHECK(thrd_create(&t, return_arg, NULL) == thrd_success);
    CHECK(thrd_join(t, NULL) == thrd_success);
    return 0;
}

static int test_threads_many(int seed, int rep) {
    enum { COUNT = 32 };
    thrd_t t[COUNT];
    int i, sum = 0;

    for (i = 0; i < COUNT; i++)
        CHECK(
            thrd_create(&t[i], return_arg, (void *)(intptr_t)i) == thrd_success
        );
    for (i = 0; i < COUNT; i++) {
        int r = -1;
        CHECK(thrd_join(t[i], &r) == thrd_success);
        CHECK(r == i);
        sum += r;
    }
    CHECK(sum == COUNT * (COUNT - 1) / 2);
    return 0;
}

struct ids {
    thrd_t main_id;
    thrd_t child_id;
};

static int record_id(void *arg) {
    struct ids *ids = arg;
    ids->child_id = thrd_current();
    return thrd_equal(ids->child_id, ids->main_id) ? 1 : 0;
}

static int test_threads_current_equal(int seed, int rep) {
    struct ids ids;
    thrd_t t;
    int differs = -1;

    ids.main_id = thrd_current();
    CHECK(thrd_equal(ids.main_id, thrd_current()));
    CHECK(thrd_equal(thrd_current(), thrd_current()));

    CHECK(thrd_create(&t, record_id, &ids) == thrd_success);
    CHECK(thrd_join(t, &differs) == thrd_success);
    CHECK(differs == 0);
    CHECK(!thrd_equal(ids.main_id, ids.child_id));
    return 0;
}

static int exits_early(void *arg) {
    (void)arg;
    thrd_exit(55);
    return 1; /* not reached */
}

static int test_threads_exit(int seed, int rep) {
    thrd_t t;
    int result = 0;

    CHECK(thrd_create(&t, exits_early, NULL) == thrd_success);
    CHECK(thrd_join(t, &result) == thrd_success);
    CHECK(result == 55);
    return 0;
}

static int detached_body(void *arg) {
    test_sleep_ms(10);
    flag_set(arg);
    return 0;
}

static int test_threads_detach(int seed, int rep) {
    static struct flag done;
    thrd_t t;

    flag_init(&done);
    CHECK(thrd_create(&t, detached_body, &done) == thrd_success);
    CHECK(thrd_detach(t) == thrd_success);
    CHECK(flag_wait(&done));
    return 0;
}

static int test_threads_sleep_yield(int seed, int rep) {
    struct timespec d, rem;
    double start;

    d.tv_sec = 0;
    d.tv_nsec = 50 * 1000000L;

    start = test_now();
    CHECK(thrd_sleep(&d, NULL) == 0);
    CHECK(test_now() - start >= 0.04);

    rem.tv_sec = rem.tv_nsec = 0;
    CHECK(thrd_sleep(&d, &rem) == 0);

    thrd_yield();
    return 0;
}

/* ---------------------------------------------------------------- */
/* Mutexes                                                           */
/* ---------------------------------------------------------------- */

struct counter {
    mtx_t lock;
    long value;
    int rounds;
};

static int count_up(void *arg) {
    struct counter *c = arg;
    int i;

    for (i = 0; i < c->rounds; i++) {
        mtx_lock(&c->lock);
        c->value++;
        mtx_unlock(&c->lock);
    }
    return 0;
}

static int test_threads_mutex_exclusion(int seed, int rep) {
    enum { THREADS = 6, ROUNDS = 20000 };
    struct counter c;
    thrd_t t[THREADS];
    int i;

    CHECK(mtx_init(&c.lock, mtx_plain) == thrd_success);
    c.value = 0;
    c.rounds = ROUNDS;

    for (i = 0; i < THREADS; i++)
        CHECK(thrd_create(&t[i], count_up, &c) == thrd_success);
    for (i = 0; i < THREADS; i++)
        CHECK(thrd_join(t[i], NULL) == thrd_success);

    CHECK(c.value == (long)THREADS * ROUNDS);
    mtx_destroy(&c.lock);
    return 0;
}

static int test_threads_mutex_recursive(int seed, int rep) {
    mtx_t m;

    CHECK(mtx_init(&m, mtx_recursive) == thrd_success);
    CHECK(mtx_lock(&m) == thrd_success);
    CHECK(mtx_lock(&m) == thrd_success);
    CHECK(mtx_trylock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);
    mtx_destroy(&m);

    CHECK(mtx_init(&m, mtx_recursive | mtx_timed) == thrd_success);
    CHECK(mtx_lock(&m) == thrd_success);
    CHECK(mtx_lock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);
    mtx_destroy(&m);
    return 0;
}

struct holder {
    mtx_t *lock;
    struct flag locked;
    struct flag release;
};

static int hold_lock(void *arg) {
    struct holder *h = arg;

    mtx_lock(h->lock);
    flag_set(&h->locked);
    flag_wait(&h->release);
    mtx_unlock(h->lock);
    return 0;
}

static void holder_init(struct holder *h, mtx_t *lock) {
    h->lock = lock;
    flag_init(&h->locked);
    flag_init(&h->release);
}

static int test_threads_trylock(int seed, int rep) {
    mtx_t m;
    struct holder h;
    thrd_t t;

    CHECK(mtx_init(&m, mtx_plain) == thrd_success);
    CHECK(mtx_trylock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);

    holder_init(&h, &m);
    CHECK(thrd_create(&t, hold_lock, &h) == thrd_success);
    CHECK(flag_wait(&h.locked));

    /* Held by the other thread. */
    CHECK(mtx_trylock(&m) == thrd_busy);

    flag_set(&h.release);
    CHECK(thrd_join(t, NULL) == thrd_success);
    CHECK(mtx_trylock(&m) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);
    mtx_destroy(&m);
    return 0;
}

static int test_threads_timedlock(int seed, int rep) {
    mtx_t m;
    struct holder h;
    struct timespec ts;
    thrd_t t;
    double start;

    CHECK(mtx_init(&m, mtx_timed) == thrd_success);

    /* Free: acquired at once. */
    test_deadline(&ts, 2000);
    CHECK(mtx_timedlock(&m, &ts) == thrd_success);
    CHECK(mtx_unlock(&m) == thrd_success);

    holder_init(&h, &m);
    CHECK(thrd_create(&t, hold_lock, &h) == thrd_success);
    CHECK(flag_wait(&h.locked));

    test_deadline(&ts, 60);
    start = test_now();
    CHECK(mtx_timedlock(&m, &ts) == thrd_timedout);
    CHECK(test_now() - start >= 0.04);
    CHECK(test_now() - start < 3.0);

    flag_set(&h.release);
    CHECK(thrd_join(t, NULL) == thrd_success);
    mtx_destroy(&m);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Condition variables                                               */
/* ---------------------------------------------------------------- */

struct gate {
    mtx_t lock;
    cnd_t cond;
    int open;
    int waiting;
    int passed;
};

static int wait_at_gate(void *arg) {
    struct gate *g = arg;

    mtx_lock(&g->lock);
    g->waiting++;
    while (!g->open)
        cnd_wait(&g->cond, &g->lock);
    g->passed++;
    mtx_unlock(&g->lock);
    return 0;
}

static void gate_wait_waiting(struct gate *g, int count) {
    int i, waiting = 0;
    for (i = 0; i < 2000 && waiting < count; i++) {
        mtx_lock(&g->lock);
        waiting = g->waiting;
        mtx_unlock(&g->lock);
        if (waiting < count)
            test_sleep_ms(2);
    }
}

static int test_threads_cond_signal(int seed, int rep) {
    struct gate g;
    thrd_t t;

    CHECK(mtx_init(&g.lock, mtx_plain) == thrd_success);
    CHECK(cnd_init(&g.cond) == thrd_success);
    g.open = g.waiting = g.passed = 0;

    CHECK(thrd_create(&t, wait_at_gate, &g) == thrd_success);
    gate_wait_waiting(&g, 1);

    mtx_lock(&g.lock);
    CHECK(g.passed == 0);
    g.open = 1;
    CHECK(cnd_signal(&g.cond) == thrd_success);
    mtx_unlock(&g.lock);

    CHECK(thrd_join(t, NULL) == thrd_success);
    CHECK(g.passed == 1);

    cnd_destroy(&g.cond);
    mtx_destroy(&g.lock);
    return 0;
}

static int test_threads_cond_broadcast(int seed, int rep) {
    enum { WAITERS = 5 };
    struct gate g;
    thrd_t t[WAITERS];
    int i;

    CHECK(mtx_init(&g.lock, mtx_plain) == thrd_success);
    CHECK(cnd_init(&g.cond) == thrd_success);
    g.open = g.waiting = g.passed = 0;

    for (i = 0; i < WAITERS; i++)
        CHECK(thrd_create(&t[i], wait_at_gate, &g) == thrd_success);
    gate_wait_waiting(&g, WAITERS);

    mtx_lock(&g.lock);
    g.open = 1;
    CHECK(cnd_broadcast(&g.cond) == thrd_success);
    mtx_unlock(&g.lock);

    for (i = 0; i < WAITERS; i++)
        CHECK(thrd_join(t[i], NULL) == thrd_success);
    CHECK(g.passed == WAITERS);

    cnd_destroy(&g.cond);
    mtx_destroy(&g.lock);
    return 0;
}

static int test_threads_cond_timedwait(int seed, int rep) {
    mtx_t m;
    cnd_t c;
    struct timespec ts;
    double start;

    /* Condition variables need a mutex that supports timed waits on some
       platforms; a plain one is the portable choice here. */
    CHECK(mtx_init(&m, mtx_plain) == thrd_success);
    CHECK(cnd_init(&c) == thrd_success);

    CHECK(mtx_lock(&m) == thrd_success);
    test_deadline(&ts, 60);
    start = test_now();
    CHECK(cnd_timedwait(&c, &m, &ts) == thrd_timedout);
    CHECK(test_now() - start >= 0.04);
    CHECK(test_now() - start < 3.0);
    CHECK(mtx_unlock(&m) == thrd_success);

    cnd_destroy(&c);
    mtx_destroy(&m);
    return 0;
}

/* ---------------------------------------------------------------- */
/* call_once and thread specific storage                             */
/* ---------------------------------------------------------------- */

static once_flag once = ONCE_FLAG_INIT;
static int once_runs;

static void once_body(void) {
    test_sleep_ms(5);
    once_runs++;
}

static int call_it(void *arg) {
    (void)arg;
    call_once(&once, once_body);
    return once_runs;
}

static int test_threads_call_once(int seed, int rep) {
    enum { THREADS = 8 };
    thrd_t t[THREADS];
    int i, r;

    once_runs = 0;
    for (i = 0; i < THREADS; i++)
        CHECK(thrd_create(&t[i], call_it, NULL) == thrd_success);
    for (i = 0; i < THREADS; i++) {
        CHECK(thrd_join(t[i], &r) == thrd_success);
        CHECK(r == 1); /* done before any call returned */
    }
    CHECK(once_runs == 1);

    call_once(&once, once_body);
    CHECK(once_runs == 1);
    return 0;
}

static tss_t key;
static volatile int dtor_calls;

static void count_dtor(void *p) {
    (void)p;
    dtor_calls++;
}

static int use_tss(void *arg) {
    intptr_t mine = (intptr_t)arg;

    if (tss_get(key) != NULL)
        return -1; /* starts empty in every thread */
    tss_set(key, (void *)mine);
    test_sleep_ms(10);
    return tss_get(key) == (void *)mine ? 0 : -2;
}

static int test_threads_tss(int seed, int rep) {
    enum { THREADS = 4 };
    thrd_t t[THREADS];
    int i, r;

    CHECK(tss_create(&key, NULL) == thrd_success);
    CHECK(tss_get(key) == NULL);
    CHECK(tss_set(key, (void *)(intptr_t)99) == thrd_success);

    for (i = 0; i < THREADS; i++)
        CHECK(
            thrd_create(&t[i], use_tss, (void *)(intptr_t)(i + 1))
            == thrd_success
        );
    for (i = 0; i < THREADS; i++) {
        CHECK(thrd_join(t[i], &r) == thrd_success);
        CHECK(r == 0);
    }

    /* The other threads did not touch this thread's value. */
    CHECK(tss_get(key) == (void *)(intptr_t)99);
    tss_delete(key);
    return 0;
}

static int set_with_dtor(void *arg) {
    (void)arg;
    tss_set(key, (void *)1);
    return 0;
}

static int test_threads_tss_destructor(int seed, int rep) {
#if HAVE_TSS_DTOR_TEST
    thrd_t t;

    dtor_calls = 0;
    CHECK(tss_create(&key, count_dtor) == thrd_success);
    CHECK(thrd_create(&t, set_with_dtor, NULL) == thrd_success);
    CHECK(thrd_join(t, NULL) == thrd_success);
    CHECK(dtor_calls == 1);
    tss_delete(key);
    return 0;
#else
    (void)count_dtor;
    (void)set_with_dtor;
    return SKIP("destructors need pf_tss_global on Windows");
#endif
}

/* ---------------------------------------------------------------- */
/* Extensions: read-write locks, spin locks, barriers                */
/* ---------------------------------------------------------------- */

static int test_threads_rwlock(int seed, int rep) {
    pf_rwlock_t l;

    CHECK(pf_rwlock_init(&l) == thrd_success);

    /* Readers share. */
    CHECK(pf_rwlock_try_rd(&l) == thrd_success);
    CHECK(pf_rwlock_try_rd(&l) == thrd_success);
    CHECK(pf_rwlock_try_wr(&l) != thrd_success);
    CHECK(pf_rwlock_exit_rd(&l) == thrd_success);
    CHECK(pf_rwlock_try_wr(&l) != thrd_success);
    CHECK(pf_rwlock_exit_rd(&l) == thrd_success);

    /* A writer is exclusive. */
    CHECK(pf_rwlock_try_wr(&l) == thrd_success);
    CHECK(pf_rwlock_try_rd(&l) != thrd_success);
    CHECK(pf_rwlock_try_wr(&l) != thrd_success);
    CHECK(pf_rwlock_exit_wr(&l) == thrd_success);

    CHECK(pf_rwlock_try_rd(&l) == thrd_success);
    CHECK(pf_rwlock_exit_rd(&l) == thrd_success);

    /* Blocking variants, when nothing is in the way. */
    CHECK(pf_rwlock_wait_rd(&l, NULL) == thrd_success);
    CHECK(pf_rwlock_exit_rd(&l) == thrd_success);
    CHECK(pf_rwlock_wait_wr(&l, NULL) == thrd_success);
    CHECK(pf_rwlock_exit_wr(&l) == thrd_success);

    pf_rwlock_free(&l);
    return 0;
}

static int test_threads_rwlock_null(int seed, int rep) {
    CHECK(pf_rwlock_init(NULL) != thrd_success);
    CHECK(pf_rwlock_try_rd(NULL) != thrd_success);
    CHECK(pf_rwlock_try_wr(NULL) != thrd_success);
    CHECK(pf_rwlock_exit_rd(NULL) != thrd_success);
    CHECK(pf_rwlock_exit_wr(NULL) != thrd_success);
    pf_rwlock_free(NULL);
    return 0;
}

struct rw {
    pf_rwlock_t lock;
    long value;
    int rounds;
};

static int rw_writer(void *arg) {
    struct rw *rw = arg;
    int i;
    for (i = 0; i < rw->rounds; i++) {
        pf_rwlock_wait_wr(&rw->lock, NULL);
        rw->value++;
        pf_rwlock_exit_wr(&rw->lock);
    }
    return 0;
}

static int rw_reader(void *arg) {
    struct rw *rw = arg;
    int i, bad = 0;
    long last = 0;
    for (i = 0; i < rw->rounds; i++) {
        pf_rwlock_wait_rd(&rw->lock, NULL);
        if (rw->value < last)
            bad = 1; /* the value only ever grows */
        last = rw->value;
        pf_rwlock_exit_rd(&rw->lock);
    }
    return bad;
}

static int test_threads_rwlock_threads(int seed, int rep) {
    enum { WRITERS = 3, READERS = 3, ROUNDS = 5000 };
    struct rw rw;
    thrd_t w[WRITERS], r[READERS];
    int i, bad;

    CHECK(pf_rwlock_init(&rw.lock) == thrd_success);
    rw.value = 0;
    rw.rounds = ROUNDS;

    for (i = 0; i < WRITERS; i++)
        CHECK(thrd_create(&w[i], rw_writer, &rw) == thrd_success);
    for (i = 0; i < READERS; i++)
        CHECK(thrd_create(&r[i], rw_reader, &rw) == thrd_success);
    for (i = 0; i < WRITERS; i++)
        CHECK(thrd_join(w[i], NULL) == thrd_success);
    for (i = 0; i < READERS; i++) {
        CHECK(thrd_join(r[i], &bad) == thrd_success);
        CHECK(!bad);
    }
    CHECK(rw.value == (long)WRITERS * ROUNDS);
    pf_rwlock_free(&rw.lock);
    return 0;
}

#ifndef _WIN32
static int test_threads_rwlock_timed(int seed, int rep) {
    pf_rwlock_t l;
    struct timespec ts;
    double start;

    CHECK(pf_rwlock_init(&l) == thrd_success);
    CHECK(pf_rwlock_try_rd(&l) == thrd_success);

    /* A writer cannot get in while a reader holds the lock. */
    test_deadline(&ts, 60);
    start = test_now();
    CHECK(pf_rwlock_wait_wr(&l, &ts) != thrd_success);
    CHECK(test_now() - start >= 0.04);

    /* Readers can. */
    test_deadline(&ts, 2000);
    CHECK(pf_rwlock_wait_rd(&l, &ts) == thrd_success);
    CHECK(pf_rwlock_exit_rd(&l) == thrd_success);
    CHECK(pf_rwlock_exit_rd(&l) == thrd_success);

    pf_rwlock_free(&l);
    return 0;
}
#endif

static int test_threads_spinlock(int seed, int rep) {
    pf_spinlock_t s;

    CHECK(pf_spin_init(&s) == thrd_success);
    CHECK(pf_spin_trylock(&s) == 0);
    CHECK(pf_spin_trylock(&s) == 1); /* busy */
    CHECK(pf_spin_unlock(&s) == thrd_success);
    CHECK(pf_spin_lock(&s) == thrd_success);
    CHECK(pf_spin_trylock(&s) == 1);
    CHECK(pf_spin_unlock(&s) == thrd_success);
    CHECK(pf_spin_trylock(&s) == 0);
    CHECK(pf_spin_unlock(&s) == thrd_success);
    pf_spin_free(&s);

    CHECK(pf_spin_init(NULL) != thrd_success);
    CHECK(pf_spin_lock(NULL) != thrd_success);
    CHECK(pf_spin_trylock(NULL) != thrd_success);
    CHECK(pf_spin_unlock(NULL) != thrd_success);
    return 0;
}

struct spin {
    pf_spinlock_t lock;
    long value;
    int rounds;
};

static int spin_count(void *arg) {
    struct spin *s = arg;
    int i;
    for (i = 0; i < s->rounds; i++) {
        pf_spin_lock(&s->lock);
        s->value++;
        pf_spin_unlock(&s->lock);
    }
    return 0;
}

static int test_threads_spinlock_threads(int seed, int rep) {
    enum { THREADS = 4, ROUNDS = 20000 };
    struct spin s;
    thrd_t t[THREADS];
    int i;

    CHECK(pf_spin_init(&s.lock) == thrd_success);
    s.value = 0;
    s.rounds = ROUNDS;

    for (i = 0; i < THREADS; i++)
        CHECK(thrd_create(&t[i], spin_count, &s) == thrd_success);
    for (i = 0; i < THREADS; i++)
        CHECK(thrd_join(t[i], NULL) == thrd_success);

    CHECK(s.value == (long)THREADS * ROUNDS);
    pf_spin_free(&s.lock);
    return 0;
}

struct barrier_test {
    pf_barrier_t barrier;
    mtx_t lock;
    int arrived;
    int rounds;
    int serial;
    int early; /* threads that got through before all had arrived */
    int parties;
};

static int barrier_body(void *arg) {
    struct barrier_test *b = arg;
    int round, i;

    for (round = 0; round < b->rounds; round++) {
        mtx_lock(&b->lock);
        b->arrived++;
        mtx_unlock(&b->lock);

        i = pf_barrier_wait(&b->barrier);

        mtx_lock(&b->lock);
        if (b->arrived < (round + 1) * b->parties)
            b->early++;
        if (i == thrd_success)
            b->serial++;
        mtx_unlock(&b->lock);

        /* Nobody starts the next round before everyone has checked. */
        pf_barrier_wait(&b->barrier);
    }
    return 0;
}

static int test_threads_barrier(int seed, int rep) {
    enum { PARTIES = 5, ROUNDS = 20 };
    struct barrier_test b;
    thrd_t t[PARTIES];
    int i;

    CHECK(pf_barrier_init(&b.barrier, PARTIES) == thrd_success);
    CHECK(mtx_init(&b.lock, mtx_plain) == thrd_success);
    b.arrived = b.serial = b.early = 0;
    b.rounds = ROUNDS;
    b.parties = PARTIES;

    for (i = 0; i < PARTIES; i++)
        CHECK(thrd_create(&t[i], barrier_body, &b) == thrd_success);
    for (i = 0; i < PARTIES; i++)
        CHECK(thrd_join(t[i], NULL) == thrd_success);

    CHECK(b.early == 0);
    CHECK(b.serial == ROUNDS); /* one thread per round is singled out */

    pf_barrier_free(&b.barrier);
    mtx_destroy(&b.lock);
    return 0;
}

static int test_threads_barrier_invalid(int seed, int rep) {
    pf_barrier_t b;

    CHECK(pf_barrier_init(NULL, 2) != thrd_success);
    CHECK(pf_barrier_init(&b, 0) != thrd_success);
    CHECK(pf_barrier_init(&b, -1) != thrd_success);
    CHECK(pf_barrier_wait(NULL) != thrd_success);
    pf_barrier_free(NULL);

    /* A barrier for one is passed at once, and always as the serial thread. */
    CHECK(pf_barrier_init(&b, 1) == thrd_success);
    CHECK(pf_barrier_wait(&b) == thrd_success);
    CHECK(pf_barrier_wait(&b) == thrd_success);
    pf_barrier_free(&b);
    return 0;
}

pf_test_t suite_threads[] = {
    { test_threads_create_join, "/threads/create_join" },
    { test_threads_many, "/threads/many" },
    { test_threads_current_equal, "/threads/current_equal" },
    { test_threads_exit, "/threads/exit" },
    { test_threads_detach, "/threads/detach" },
    { test_threads_sleep_yield, "/threads/sleep_yield" },
    { test_threads_mutex_exclusion, "/threads/mutex_exclusion" },
    { test_threads_mutex_recursive, "/threads/mutex_recursive" },
    { test_threads_trylock, "/threads/trylock" },
    { test_threads_timedlock, "/threads/timedlock" },
    { test_threads_cond_signal, "/threads/cond_signal" },
    { test_threads_cond_broadcast, "/threads/cond_broadcast" },
    { test_threads_cond_timedwait, "/threads/cond_timedwait" },
    { test_threads_call_once, "/threads/call_once" },
    { test_threads_tss, "/threads/tss" },
    { test_threads_tss_destructor, "/threads/tss_destructor" },
    { test_threads_rwlock, "/threads/rwlock" },
    { test_threads_rwlock_null, "/threads/rwlock_null" },
    { test_threads_rwlock_threads, "/threads/rwlock_threads" },
#ifndef _WIN32
    { test_threads_rwlock_timed, "/threads/rwlock_timed" },
#endif
    { test_threads_spinlock, "/threads/spinlock" },
    { test_threads_spinlock_threads, "/threads/spinlock_threads" },
    { test_threads_barrier, "/threads/barrier" },
    { test_threads_barrier_invalid, "/threads/barrier_invalid" },
    { 0 },
};
