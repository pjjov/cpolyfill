/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The atomic operations, built once with the compiler's implementation
   (atomic.c) and once with the library's (atomic_lib.c, which defines
   PF__ATOMIC_FORCE_LOCKED). Defines `int ATOMIC_OPS_NAME(void)` running
   them all in one thread, and `int ATOMIC_OPS_NAME_threads(void)` running
   them concurrently. */

#include <cpolyfill/atomic.h>
#include <cpolyfill/macro.h>
#include <cpolyfill/threads.h>

#include <stdint.h>
#include <string.h>

#include "check.h"

/* Large atomics are slow, and these are on purpose. */
#if defined(__clang__)
    #pragma clang diagnostic ignored "-Watomic-alignment"
#endif

#define ATOMIC_OPS_FN(suffix) PF_CONCAT(ATOMIC_OPS_NAME, suffix)

struct triple {
    unsigned char a, b, c;
};

struct big {
    uint64_t a, b, c;
};

static void ATOMIC_OPS_FN(_ints)(void) {
    PF_ATOMIC(int) i = 5;
    PF_ATOMIC(unsigned char) c = 250;
    PF_ATOMIC(short) s = -3;
    PF_ATOMIC(unsigned long long) u = 0x0123456789ABCDEFULL;
    int e;
    unsigned long long ue;

    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&i), 5);
    PF_ATOMIC_STORE(&i, 9);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD_EXPLICIT(&i, PF_MEMORY_ORDER_ACQUIRE), 9);
    PF_ATOMIC_STORE_EXPLICIT(&i, -1, PF_MEMORY_ORDER_RELEASE);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_EXCHANGE(&i, 4), -1);
    PF_ASSERT_ALWAYS_EQ(
        PF_ATOMIC_EXCHANGE_EXPLICIT(&i, 6, PF_MEMORY_ORDER_ACQ_REL), 4
    );

    e = 5;
    PF_ASSERT_ALWAYS(!PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&i, &e, 7));
    PF_ASSERT_ALWAYS_EQ(e, 6);
    PF_ASSERT_ALWAYS(PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&i, &e, 7));
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&i), 7);
    e = 7;
    while (!PF_ATOMIC_COMPARE_EXCHANGE_WEAK(&i, &e, 8)) {
    }
    e = 8;
    while (!PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT(
        &i, &e, 10, PF_MEMORY_ORDER_SEQ_CST, PF_MEMORY_ORDER_RELAXED
    )) {
    }
    e = 10;
    PF_ASSERT_ALWAYS(PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(
        &i, &e, 10, PF_MEMORY_ORDER_ACQ_REL, PF_MEMORY_ORDER_ACQUIRE
    ));

    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_ADD(&i, 5), 10);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_ADD(&i, -20), 15);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_SUB(&i, -2), -5);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_OR(&i, 0x10), -3);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_AND(&i, 0xFF), -3);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_XOR_EXPLICIT(&i, 0xF, PF_MEMORY_ORDER_RELAXED), 0xFD);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&i), 0xF2);

    /* Small types wrap at their size. */
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_ADD(&c, 10), 250);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&c), 4);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_SUB(&s, 1), -3);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&s), -4);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_ADD_EXPLICIT(&s, 32767, PF_MEMORY_ORDER_RELAXED), -4);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&s), 32763);

    /* All 64 bits. */
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_ADD(&u, 0x1000000000000000ULL), 0x0123456789ABCDEFULL);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&u), 0x1123456789ABCDEFULL);
    ue = 0x1123456789ABCDEFULL;
    PF_ASSERT_ALWAYS(PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&u, &ue, ~0ULL));
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_FETCH_AND(&u, 0xF0F0F0F0F0F0F0F0ULL), ~0ULL);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_EXCHANGE(&u, 1), 0xF0F0F0F0F0F0F0F0ULL);

    PF_ATOMIC_INIT(&i, 3);
    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&i), 3);
    PF_ASSERT_ALWAYS(PF_ATOMIC_IS_LOCK_FREE(&i));
}

static void ATOMIC_OPS_FN(_pointers)(void) {
    static int array[8];
    PF_ATOMIC(int *) p = array;
    int *e = array + 1;

    /* Steps are in elements. */
    PF_ASSERT_ALWAYS(PF_ATOMIC_FETCH_ADD(&p, 2) == array);
    PF_ASSERT_ALWAYS(PF_ATOMIC_LOAD(&p) == array + 2);
    PF_ASSERT_ALWAYS(PF_ATOMIC_FETCH_SUB(&p, 1) == array + 2);
    PF_ASSERT_ALWAYS(PF_ATOMIC_LOAD(&p) == array + 1);
    PF_ASSERT_ALWAYS(PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&p, &e, array + 7));
    PF_ASSERT_ALWAYS(PF_ATOMIC_EXCHANGE(&p, NULL) == array + 7);
    PF_ASSERT_ALWAYS(PF_ATOMIC_LOAD(&p) == NULL);
    PF_ATOMIC_STORE(&p, array);
    PF_ASSERT_ALWAYS(PF_ATOMIC_LOAD(&p) == array);
}

static void ATOMIC_OPS_FN(_others)(void) {
    PF_ATOMIC(double) d = 1.5;
    PF_ATOMIC(struct triple) t;
    PF_ATOMIC(struct big) b;
    struct triple t1 = { 1, 2, 3 }, t2 = { 4, 5, 6 }, te;
    struct big b1 = { 1, 2, 3 }, b2 = { 7, 8, 9 }, be;
    pf_atomic_flag_t flag = PF_ATOMIC_FLAG_INIT;

    PF_ASSERT_ALWAYS(PF_ATOMIC_LOAD(&d) == 1.5);
    PF_ATOMIC_STORE(&d, -2.25);
    PF_ASSERT_ALWAYS(PF_ATOMIC_EXCHANGE(&d, 3.0) == -2.25);
    PF_ASSERT_ALWAYS(PF_ATOMIC_LOAD(&d) == 3.0);

    PF_ATOMIC_INIT(&t, t1);
    te = PF_ATOMIC_LOAD(&t);
    PF_ASSERT_ALWAYS_MEM_EQ(&te, &t1, sizeof(te));
    te = PF_ATOMIC_EXCHANGE(&t, t2);
    PF_ASSERT_ALWAYS_MEM_EQ(&te, &t1, sizeof(te));
    PF_ASSERT_ALWAYS(!PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&t, &te, t1));
    PF_ASSERT_ALWAYS_MEM_EQ(&te, &t2, sizeof(te));
    PF_ASSERT_ALWAYS(PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&t, &te, t1));

    PF_ATOMIC_INIT(&b, b1);
    PF_ATOMIC_STORE(&b, b2);
    be = PF_ATOMIC_LOAD(&b);
    PF_ASSERT_ALWAYS_MEM_EQ(&be, &b2, sizeof(be));
    PF_ASSERT_ALWAYS(PF_ATOMIC_COMPARE_EXCHANGE_STRONG(&b, &be, b1));
    be = PF_ATOMIC_EXCHANGE(&b, b2);
    PF_ASSERT_ALWAYS_MEM_EQ(&be, &b1, sizeof(be));

    PF_ASSERT_ALWAYS(!PF_ATOMIC_FLAG_TEST_AND_SET(&flag));
    PF_ASSERT_ALWAYS(PF_ATOMIC_FLAG_TEST_AND_SET(&flag));
    PF_ATOMIC_FLAG_CLEAR(&flag);
    PF_ASSERT_ALWAYS(!PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT(&flag, PF_MEMORY_ORDER_ACQUIRE));
    PF_ATOMIC_FLAG_CLEAR_EXPLICIT(&flag, PF_MEMORY_ORDER_RELEASE);
    PF_ASSERT_ALWAYS(!PF_ATOMIC_FLAG_TEST_AND_SET(&flag));

    PF_ATOMIC_THREAD_FENCE(PF_MEMORY_ORDER_SEQ_CST);
    PF_ATOMIC_SIGNAL_FENCE(PF_MEMORY_ORDER_ACQ_REL);
    PF_ASSERT_ALWAYS_EQ(PF_KILL_DEPENDENCY(4), 4);
    PF_ASSERT_ALWAYS(PF_ATOMIC_INT_LOCK_FREE >= 1);
    PF_ASSERT_ALWAYS(PF_ATOMIC_POINTER_LOCK_FREE >= 1);
    PF_ASSERT_ALWAYS(PF_ATOMIC_BOOL_LOCK_FREE >= 1);
    PF_ASSERT_ALWAYS(PF_ATOMIC_LLONG_LOCK_FREE >= 0);
}

int ATOMIC_OPS_NAME(void) {
    ATOMIC_OPS_FN(_ints)();
    ATOMIC_OPS_FN(_pointers)();
    ATOMIC_OPS_FN(_others)();
    return 0;
}

#if PF_HAS_THREADS

    #define OPS_THREADS 8
    #define OPS_ROUNDS 20000

static PF_ATOMIC(long) ATOMIC_OPS_FN(_counter);
static PF_ATOMIC(unsigned long long) ATOMIC_OPS_FN(_wide);
static PF_ATOMIC(struct big) ATOMIC_OPS_FN(_big);
static pf_atomic_flag_t ATOMIC_OPS_FN(_flag) = PF_ATOMIC_FLAG_INIT;
static long ATOMIC_OPS_FN(_guarded);

static int ATOMIC_OPS_FN(_worker)(void *arg) {
    int k;
    (void)arg;
    for (k = 0; k < OPS_ROUNDS; k++) {
        unsigned long long w = PF_ATOMIC_LOAD(&ATOMIC_OPS_FN(_wide));
        struct big b = PF_ATOMIC_LOAD(&ATOMIC_OPS_FN(_big)), nb;

        (void)PF_ATOMIC_FETCH_ADD(&ATOMIC_OPS_FN(_counter), 1);
        while (!PF_ATOMIC_COMPARE_EXCHANGE_WEAK(
            &ATOMIC_OPS_FN(_wide), &w, w + 0x100000001ULL
        )) {
        }
        /* The fields always move together. */
        do {
            PF_ASSERT_ALWAYS(b.a == b.b && b.b == b.c);
            nb.a = nb.b = nb.c = b.a + 1;
        } while (!PF_ATOMIC_COMPARE_EXCHANGE_WEAK(&ATOMIC_OPS_FN(_big), &b, nb));

        while (PF_ATOMIC_FLAG_TEST_AND_SET(&ATOMIC_OPS_FN(_flag))) {
        }
        ATOMIC_OPS_FN(_guarded)++;
        PF_ATOMIC_FLAG_CLEAR(&ATOMIC_OPS_FN(_flag));
    }
    return 0;
}

int ATOMIC_OPS_FN(_threads)(void) {
    pf_thrd_t t[OPS_THREADS];
    struct big zero = { 0, 0, 0 }, b;
    int k;

    PF_ATOMIC_STORE(&ATOMIC_OPS_FN(_counter), 0);
    PF_ATOMIC_STORE(&ATOMIC_OPS_FN(_wide), 0);
    PF_ATOMIC_STORE(&ATOMIC_OPS_FN(_big), zero);
    ATOMIC_OPS_FN(_guarded) = 0;
    for (k = 0; k < OPS_THREADS; k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_thrd_create(&t[k], ATOMIC_OPS_FN(_worker), NULL), pf_thrd_success
        );
    for (k = 0; k < OPS_THREADS; k++)
        PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t[k], NULL), pf_thrd_success);

    PF_ASSERT_ALWAYS_EQ(PF_ATOMIC_LOAD(&ATOMIC_OPS_FN(_counter)), (long)OPS_THREADS * OPS_ROUNDS);
    PF_ASSERT_ALWAYS_EQ(
        PF_ATOMIC_LOAD(&ATOMIC_OPS_FN(_wide)),
        (unsigned long long)OPS_THREADS * OPS_ROUNDS * 0x100000001ULL
    );
    b = PF_ATOMIC_LOAD(&ATOMIC_OPS_FN(_big));
    PF_ASSERT_ALWAYS_EQ(b.a, (uint64_t)OPS_THREADS * OPS_ROUNDS);
    PF_ASSERT_ALWAYS_EQ(ATOMIC_OPS_FN(_guarded), (long)OPS_THREADS * OPS_ROUNDS);
    return 0;
}

#endif
