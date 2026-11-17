/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/atomic.h>: the operations with the compiler's implementation
   and the library's (atomic_lib.c), the lock table, and waiting. */

#include "../src/internal.h"

#define ATOMIC_OPS_NAME atomic_ops_native
#include "atomic_ops.h"

#include <cpolyfill/time.h>

int atomic_ops_lib(void);
#if PF_HAS_THREADS
int atomic_ops_lib_threads(void);
#endif

static int test_operations(int seed, int i) {
    (void)seed, (void)i;
    atomic_ops_native();
    atomic_ops_lib();
    return 0;
}

static int test_concurrent(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_THREADS
    atomic_ops_native_threads();
    atomic_ops_lib_threads();
    return 0;
#else
    return SKIP("no threads");
#endif
}

/* ---- The lock table directly (used for sizes that are not lock-free) --- */

static int test_lock_table(int seed, int i) {
    struct big b = { 1, 2, 3 }, v = { 4, 5, 6 }, out, e;
    unsigned char bytes[3] = { 1, 2, 3 }, three[3] = { 9, 9, 9 };
    uint32_t w = 10;
    (void)seed, (void)i;

    pf__atomic_load_locked(&b, sizeof(b), &out);
    PF_ASSERT_ALWAYS_MEM_EQ(&out, &b, sizeof(b));
    pf__atomic_store_locked(&b, sizeof(b), &v);
    PF_ASSERT_ALWAYS_EQ(b.c, 6);
    /* Exchange may use one buffer for both value and result. */
    out.a = 7, out.b = 8, out.c = 9;
    pf__atomic_exchange_locked(&b, sizeof(b), &out, &out);
    PF_ASSERT_ALWAYS_EQ(out.a, 4);
    PF_ASSERT_ALWAYS_EQ(b.a, 7);
    e = v;
    PF_ASSERT_ALWAYS(!pf__atomic_cas_locked(&b, sizeof(b), &e, &v));
    PF_ASSERT_ALWAYS_EQ(e.a, 7);
    PF_ASSERT_ALWAYS(pf__atomic_cas_locked(&b, sizeof(b), &e, &v));
    PF_ASSERT_ALWAYS_EQ(b.a, 4);

    pf__atomic_exchange_locked(bytes, 3, three, three);
    PF_ASSERT_ALWAYS_EQ(bytes[2], 9);
    PF_ASSERT_ALWAYS_EQ(three[0], 1);

    pf__atomic_fetch_locked(&w, 4, PF__ATOMIC_OP_SUB, 3, &out);
    PF_ASSERT_ALWAYS_EQ(w, 7);
    PF_ASSERT_ALWAYS_EQ(*(uint32_t *)&out, 10);
    return 0;
}

/* ---- Waiting ----------------------------------------------------------- */

typedef int (*wait_fn)(
    const volatile void *, uint32_t, const struct timespec *
);
typedef void (*wake_fn)(const volatile void *);

static int fallback_wait(
    const volatile void *addr, uint32_t expected, const struct timespec *t
) {
    return pf__atomic_wait_fallback(addr, expected, t);
}

static void fallback_wake_one(const volatile void *addr) {
    pf__atomic_wake_fallback(addr, false);
}

static void fallback_wake_all(const volatile void *addr) {
    pf__atomic_wake_fallback(addr, true);
}

static const struct {
    wait_fn wait;
    wake_fn one, all;
} waiters[] = {
    { pf_atomic_wait, pf_atomic_wake_one, pf_atomic_wake_all },
    { fallback_wait, fallback_wake_one, fallback_wake_all },
};

#if PF_HAS_THREADS

static PF_ATOMIC(uint32_t) word;
static PF_ATOMIC(int) woken;
static int which;

/* Waits for `word` to become nonzero. */
static int waiter(void *arg) {
    (void)arg;
    while (PF_ATOMIC_LOAD(&word) == 0)
        PF_ASSERT_ALWAYS_EQ(waiters[which].wait(&word, 0, NULL), 0);
    PF_ATOMIC_FETCH_ADD(&woken, 1);
    return 0;
}

/* Two threads take turns: each waits for its parity, then flips it. */
static int ping(void *arg) {
    uint32_t mine = (uint32_t)(uintptr_t)arg, v;
    int k;
    for (k = 0; k < 500; k++) {
        while ((v = PF_ATOMIC_LOAD(&word)) % 2 != mine)
            waiters[which].wait(&word, v, NULL);
        PF_ATOMIC_STORE(&word, v + 1);
        waiters[which].one(&word);
    }
    return 0;
}

#endif

static int test_wait(int seed, int i) {
    struct timespec t = { 0, 20000000 }, bad = { 0, 1000000000L };
    uint32_t value = 3;
    uint64_t start;
    size_t w;
    (void)seed, (void)i;

    for (w = 0; w < sizeof(waiters) / sizeof(waiters[0]); w++) {
        /* A different value returns at once. */
        PF_ASSERT_ALWAYS_EQ(waiters[w].wait(&value, 4, NULL), 0);
        start = pf_time_ns();
        PF_ASSERT_ALWAYS_EQ(waiters[w].wait(&value, 3, &t), PF_ETIMEDOUT);
        PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 15000000u);
        PF_ASSERT_ALWAYS_EQ(waiters[w].wait(&value, 3, &bad), PF_EINVAL);
        /* Waking with no waiters is harmless. */
        waiters[w].one(&value);
        waiters[w].all(&value);

#if PF_HAS_THREADS
        {
            pf_thrd_t th[4];
            int k;

            which = (int)w;
            PF_ATOMIC_STORE(&word, 0);
            PF_ATOMIC_STORE(&woken, 0);
            for (k = 0; k < 4; k++)
                PF_ASSERT_ALWAYS_EQ(
                    pf_thrd_create(&th[k], waiter, NULL), pf_thrd_success
                );
            pf_sleep_ns(10000000);
            PF_ATOMIC_STORE(&word, 1);
            waiters[w].all(&word);
            for (k = 0; k < 4; k++)
                PF_ASSERT_ALWAYS_EQ(pf_thrd_join(th[k], NULL), pf_thrd_success);
            PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&woken), 4);

            PF_ATOMIC_STORE(&word, 0);
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&th[0], ping, (void *)0), pf_thrd_success
            );
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&th[1], ping, (void *)1), pf_thrd_success
            );
            PF_ASSERT_ALWAYS_EQ(pf_thrd_join(th[0], NULL), pf_thrd_success);
            PF_ASSERT_ALWAYS_EQ(pf_thrd_join(th[1], NULL), pf_thrd_success);
            PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&word), 1000);
        }
#else
        PF_ASSERT_ALWAYS_EQ(waiters[w].wait(&value, 3, NULL), PF_EDEADLK);
#endif
    }
    return 0;
}

pf_test_t suite_atomic[] = {
    { test_operations, "operations" },
    { test_concurrent, "concurrent" },
    { test_lock_table, "lock_table" },
    { test_wait, "wait" },
    { 0 },
};
