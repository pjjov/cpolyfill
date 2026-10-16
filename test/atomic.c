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

/* The fallback built on top of threads needs them to be included first. */
#include "../include/pf_threads.h"
#include "../include/pf_atomic.h"
#include "check.h"
#include "util.h"
#include <stddef.h>
#include <stdint.h>

#ifdef PF_NO_ATOMICS
    #define SKIP_ALL() return SKIP("no atomics on this platform")
#else
    #define SKIP_ALL() (void)0
#endif

#ifndef PF_NO_ATOMICS

    /* ---------------------------------------------------------------- */
    /* Per-type tests                                                    */
    /* ---------------------------------------------------------------- */

    /* TYPE is the tested type, UTYPE the unsigned type of the same size. */
    #define DEFINE_ATOMIC_TESTS(NAME, TYPE, UTYPE)                            \
        static const UTYPE umax_##NAME = (UTYPE) ~(UTYPE)0;                   \
        static const int signed_##NAME = ((TYPE) - 1 < (TYPE)0);              \
                                                                              \
        static TYPE maxv_##NAME(void) {                                       \
            return signed_##NAME ? (TYPE)(umax_##NAME >> 1)                   \
                                 : (TYPE)umax_##NAME;                         \
        }                                                                     \
        static TYPE minv_##NAME(void) {                                       \
            return signed_##NAME ? (TYPE)(-maxv_##NAME() - 1) : (TYPE)0;      \
        }                                                                     \
                                                                              \
        static int test_atomic_load_store_##NAME(int seed, int rep) {         \
            _Atomic(TYPE) a;                                                  \
            atomic_init(&a, (TYPE)3);                                         \
            CHECK(atomic_load(&a) == (TYPE)3);                                \
            atomic_store(&a, (TYPE)100);                                      \
            CHECK(atomic_load(&a) == (TYPE)100);                              \
            atomic_store(&a, maxv_##NAME());                                  \
            CHECK(atomic_load(&a) == maxv_##NAME());                          \
            atomic_store(&a, minv_##NAME());                                  \
            CHECK(atomic_load(&a) == minv_##NAME());                          \
            atomic_store(&a, (TYPE)0);                                        \
            CHECK(atomic_load(&a) == (TYPE)0);                                \
            atomic_store_explicit(&a, (TYPE)9, memory_order_release);         \
            CHECK(atomic_load_explicit(&a, memory_order_acquire) == (TYPE)9); \
            atomic_store_explicit(&a, (TYPE)8, memory_order_relaxed);         \
            CHECK(atomic_load_explicit(&a, memory_order_relaxed) == (TYPE)8); \
            atomic_store_explicit(&a, (TYPE)7, memory_order_seq_cst);         \
            CHECK(atomic_load_explicit(&a, memory_order_seq_cst) == (TYPE)7); \
            return 0;                                                         \
        }                                                                     \
                                                                              \
        static int test_atomic_exchange_##NAME(int seed, int rep) {           \
            _Atomic(TYPE) a;                                                  \
            atomic_init(&a, (TYPE)1);                                         \
            CHECK(atomic_exchange(&a, (TYPE)2) == (TYPE)1);                   \
            CHECK(atomic_exchange(&a, maxv_##NAME()) == (TYPE)2);             \
            CHECK(atomic_exchange(&a, minv_##NAME()) == maxv_##NAME());       \
            CHECK(atomic_load(&a) == minv_##NAME());                          \
            CHECK(                                                            \
                atomic_exchange_explicit(&a, (TYPE)6, memory_order_acq_rel)   \
                == minv_##NAME()                                              \
            );                                                                \
            CHECK(atomic_load(&a) == (TYPE)6);                                \
            return 0;                                                         \
        }                                                                     \
                                                                              \
        static int test_atomic_arith_##NAME(int seed, int rep) {              \
            _Atomic(TYPE) a;                                                  \
            atomic_init(&a, (TYPE)10);                                        \
            CHECK(atomic_fetch_add(&a, (TYPE)5) == (TYPE)10);                 \
            CHECK(atomic_load(&a) == (TYPE)15);                               \
            CHECK(atomic_fetch_sub(&a, (TYPE)3) == (TYPE)15);                 \
            CHECK(atomic_load(&a) == (TYPE)12);                               \
            CHECK(                                                            \
                atomic_fetch_add_explicit(&a, (TYPE)1, memory_order_relaxed)  \
                == (TYPE)12                                                   \
            );                                                                \
            CHECK(                                                            \
                atomic_fetch_sub_explicit(&a, (TYPE)1, memory_order_relaxed)  \
                == (TYPE)13                                                   \
            );                                                                \
            CHECK(atomic_load(&a) == (TYPE)12);                               \
            /* Arithmetic wraps around, instead of being undefined. */        \
            atomic_store(&a, maxv_##NAME());                                  \
            CHECK(atomic_fetch_add(&a, (TYPE)1) == maxv_##NAME());            \
            CHECK(atomic_load(&a) == minv_##NAME());                          \
            CHECK(atomic_fetch_sub(&a, (TYPE)1) == minv_##NAME());            \
            CHECK(atomic_load(&a) == maxv_##NAME());                          \
            atomic_store(&a, (TYPE)0);                                        \
            CHECK(atomic_fetch_sub(&a, (TYPE)1) == (TYPE)0);                  \
            CHECK(atomic_load(&a) == (TYPE)(umax_##NAME));                    \
            return 0;                                                         \
        }                                                                     \
                                                                              \
        static int test_atomic_bitwise_##NAME(int seed, int rep) {            \
            _Atomic(TYPE) a;                                                  \
            atomic_init(&a, (TYPE)0x0C);                                      \
            CHECK(atomic_fetch_or(&a, (TYPE)0x03) == (TYPE)0x0C);             \
            CHECK(atomic_load(&a) == (TYPE)0x0F);                             \
            CHECK(atomic_fetch_and(&a, (TYPE)0x0A) == (TYPE)0x0F);            \
            CHECK(atomic_load(&a) == (TYPE)0x0A);                             \
            CHECK(atomic_fetch_xor(&a, (TYPE)0x0F) == (TYPE)0x0A);            \
            CHECK(atomic_load(&a) == (TYPE)0x05);                             \
            CHECK(atomic_fetch_xor(&a, (TYPE)0x05) == (TYPE)0x05);            \
            CHECK(atomic_load(&a) == (TYPE)0);                                \
            CHECK(                                                            \
                atomic_fetch_or_explicit(&a, (TYPE)1, memory_order_seq_cst)   \
                == (TYPE)0                                                    \
            );                                                                \
            CHECK(                                                            \
                atomic_fetch_and_explicit(&a, (TYPE)1, memory_order_seq_cst)  \
                == (TYPE)1                                                    \
            );                                                                \
            CHECK(                                                            \
                atomic_fetch_xor_explicit(&a, (TYPE)1, memory_order_seq_cst)  \
                == (TYPE)1                                                    \
            );                                                                \
            CHECK(atomic_load(&a) == (TYPE)0);                                \
            /* The upper bits of the type take part as well. */               \
            atomic_store(&a, (TYPE)0);                                        \
            atomic_fetch_or(&a, minv_##NAME() | (TYPE)1);                     \
            CHECK(atomic_load(&a) == (TYPE)(minv_##NAME() | (TYPE)1));        \
            atomic_fetch_and(&a, (TYPE) ~(TYPE)1);                            \
            CHECK(atomic_load(&a) == minv_##NAME());                          \
            return 0;                                                         \
        }                                                                     \
                                                                              \
        static int test_atomic_cas_##NAME(int seed, int rep) {                \
            _Atomic(TYPE) a;                                                  \
            TYPE expected;                                                    \
            atomic_init(&a, (TYPE)5);                                         \
                                                                              \
            /* A mismatch leaves the object and reports what it held. */      \
            expected = (TYPE)4;                                               \
            CHECK(!atomic_compare_exchange_strong(&a, &expected, (TYPE)9));   \
            CHECK(expected == (TYPE)5);                                       \
            CHECK(atomic_load(&a) == (TYPE)5);                                \
                                                                              \
            CHECK(atomic_compare_exchange_strong(&a, &expected, (TYPE)9));    \
            CHECK(expected == (TYPE)5);                                       \
            CHECK(atomic_load(&a) == (TYPE)9);                                \
                                                                              \
            /* The weak form may fail spuriously, but not forever. */         \
            expected = (TYPE)9;                                               \
            {                                                                 \
                int tries = 0;                                                \
                while (!atomic_compare_exchange_weak(                         \
                    &a, &expected, maxv_##NAME()                              \
                ))                                                            \
                    CHECK(++tries < 1000000 && (expected = (TYPE)9, 1));      \
            }                                                                 \
            CHECK(atomic_load(&a) == maxv_##NAME());                          \
                                                                              \
            expected = maxv_##NAME();                                         \
            CHECK(atomic_compare_exchange_strong_explicit(                    \
                &a,                                                           \
                &expected,                                                    \
                minv_##NAME(),                                                \
                memory_order_seq_cst,                                         \
                memory_order_relaxed                                          \
            ));                                                               \
            CHECK(atomic_load(&a) == minv_##NAME());                          \
            return 0;                                                         \
        }

/* clang-format off */
#define ATOMIC_TYPES(X)                                  \
    X(char,       char,               unsigned char)     \
    X(schar,      signed char,        unsigned char)     \
    X(uchar,      unsigned char,      unsigned char)     \
    X(short,      short,              unsigned short)    \
    X(ushort,     unsigned short,     unsigned short)    \
    X(int,        int,                unsigned int)      \
    X(uint,       unsigned int,       unsigned int)      \
    X(long,       long,               unsigned long)     \
    X(ulong,      unsigned long,      unsigned long)     \
    X(llong,      long long,          unsigned long long)\
    X(ullong,     unsigned long long, unsigned long long)\
    X(int8,       int8_t,             uint8_t)           \
    X(uint8,      uint8_t,            uint8_t)           \
    X(int16,      int16_t,            uint16_t)          \
    X(uint16,     uint16_t,           uint16_t)          \
    X(int32,      int32_t,            uint32_t)          \
    X(uint32,     uint32_t,           uint32_t)          \
    X(int64,      int64_t,            uint64_t)          \
    X(uint64,     uint64_t,           uint64_t)          \
    X(size,       size_t,             size_t)            \
    X(ptrdiff,    ptrdiff_t,          size_t)            \
    X(intptr,     intptr_t,           uintptr_t)         \
    X(uintptr,    uintptr_t,          uintptr_t)         \
    X(intmax,     intmax_t,           uintmax_t)         \
    X(uintmax,    uintmax_t,          uintmax_t)
/* clang-format on */

    #define X(NAME, TYPE, UTYPE) DEFINE_ATOMIC_TESTS(NAME, TYPE, UTYPE)
ATOMIC_TYPES(X)
    #undef X

/* ---------------------------------------------------------------- */
/* Booleans and flags                                                */
/* ---------------------------------------------------------------- */

static int test_atomic_bool(int seed, int rep) {
    _Atomic(_Bool) b;
    _Bool expected;

    atomic_init(&b, 0);
    CHECK(atomic_load(&b) == 0);
    atomic_store(&b, 1);
    CHECK(atomic_load(&b) == 1);
    CHECK(atomic_exchange(&b, 0) == 1);
    CHECK(atomic_load(&b) == 0);

    expected = 1;
    CHECK(!atomic_compare_exchange_strong(&b, &expected, 1));
    CHECK(expected == 0);
    CHECK(atomic_compare_exchange_strong(&b, &expected, 1));
    CHECK(atomic_load(&b) == 1);
    return 0;
}

static int test_atomic_flag(int seed, int rep) {
    atomic_flag f = ATOMIC_FLAG_INIT;

    pf_atomic_flag_init(&f);
    CHECK(!atomic_flag_test_and_set(&f));
    CHECK(atomic_flag_test_and_set(&f));
    CHECK(atomic_flag_test_and_set(&f));
    atomic_flag_clear(&f);
    CHECK(!atomic_flag_test_and_set(&f));
    atomic_flag_clear(&f);

    CHECK(!atomic_flag_test_and_set_explicit(&f, memory_order_acquire));
    CHECK(atomic_flag_test_and_set_explicit(&f, memory_order_acquire));
    atomic_flag_clear_explicit(&f, memory_order_release);
    CHECK(!atomic_flag_test_and_set_explicit(&f, memory_order_seq_cst));
    return 0;
}

static int test_atomic_flag_reinit(int seed, int rep) {
    atomic_flag f;

    pf_atomic_flag_init(&f);
    CHECK(!atomic_flag_test_and_set(&f));
    pf_atomic_flag_init(&f);
    CHECK(!atomic_flag_test_and_set(&f)); /* initialization clears it */
    return 0;
}

/* ---------------------------------------------------------------- */
/* Misc                                                              */
/* ---------------------------------------------------------------- */

static int test_atomic_memory_order(int seed, int rep) {
    CHECK(memory_order_relaxed != memory_order_consume);
    CHECK(memory_order_relaxed != memory_order_acquire);
    CHECK(memory_order_relaxed != memory_order_release);
    CHECK(memory_order_relaxed != memory_order_acq_rel);
    CHECK(memory_order_relaxed != memory_order_seq_cst);
    CHECK(memory_order_acquire != memory_order_release);
    CHECK(memory_order_acq_rel != memory_order_seq_cst);

    {
        memory_order o = memory_order_seq_cst;
        _Atomic(int) a;
        atomic_init(&a, 1);
        CHECK(atomic_load_explicit(&a, o) == 1);
        (void)o;
    }
    return 0;
}

static int test_atomic_fences(int seed, int rep) {
    atomic_thread_fence(memory_order_seq_cst);
    atomic_thread_fence(memory_order_acquire);
    atomic_thread_fence(memory_order_release);
    atomic_signal_fence(memory_order_seq_cst);
    CHECK(kill_dependency(7) == 7);
    CHECK(kill_dependency(seed) == seed);
    return 0;
}

static int test_atomic_lock_free(int seed, int rep) {
    _Atomic(int) i;
    _Atomic(char) c;
    _Atomic(long long) l;

    atomic_init(&i, 0);
    atomic_init(&c, 0);
    atomic_init(&l, 0);

    /* Any answer is allowed, but it must be consistent. */
    CHECK(!!atomic_is_lock_free(&i) == !!atomic_is_lock_free(&i));
    (void)atomic_is_lock_free(&c);
    (void)atomic_is_lock_free(&l);

    /* 0: never, 1: sometimes, 2: always. */
    CHECK(ATOMIC_INT_LOCK_FREE >= 0 && ATOMIC_INT_LOCK_FREE <= 2);
    CHECK(ATOMIC_CHAR_LOCK_FREE >= 0 && ATOMIC_CHAR_LOCK_FREE <= 2);
    CHECK(ATOMIC_SHORT_LOCK_FREE >= 0 && ATOMIC_SHORT_LOCK_FREE <= 2);
    CHECK(ATOMIC_LONG_LOCK_FREE >= 0 && ATOMIC_LONG_LOCK_FREE <= 2);
    CHECK(ATOMIC_LLONG_LOCK_FREE >= 0 && ATOMIC_LLONG_LOCK_FREE <= 2);
    CHECK(ATOMIC_POINTER_LOCK_FREE >= 0 && ATOMIC_POINTER_LOCK_FREE <= 2);
    CHECK(ATOMIC_BOOL_LOCK_FREE >= 0 && ATOMIC_BOOL_LOCK_FREE <= 2);
    CHECK(ATOMIC_WCHAR_T_LOCK_FREE >= 0 && ATOMIC_WCHAR_T_LOCK_FREE <= 2);
    return 0;
}

static int test_atomic_typedefs(int seed, int rep) {
    atomic_bool b;
    atomic_char c;
    atomic_uchar uc;
    atomic_short s;
    atomic_uint ui;
    atomic_long l;
    atomic_ullong ull;
    atomic_int_least8_t l8;
    atomic_uint_least16_t l16;
    atomic_int_fast32_t f32;
    atomic_uint_fast64_t f64;
    atomic_intptr_t ip;
    atomic_size_t sz;
    atomic_ptrdiff_t pd;
    atomic_uintmax_t um;

    atomic_init(&b, 1);
    atomic_init(&c, 'a');
    atomic_init(&uc, 255);
    atomic_init(&s, -2);
    atomic_init(&ui, 3u);
    atomic_init(&l, -4);
    atomic_init(&ull, 5);
    atomic_init(&l8, -6);
    atomic_init(&l16, 7);
    atomic_init(&f32, -8);
    atomic_init(&f64, 9);
    atomic_init(&ip, -10);
    atomic_init(&sz, 11);
    atomic_init(&pd, -12);
    atomic_init(&um, 13);

    CHECK(atomic_load(&b) == 1);
    CHECK(atomic_load(&c) == 'a');
    CHECK(atomic_load(&uc) == 255);
    CHECK(atomic_load(&s) == -2);
    CHECK(atomic_load(&ui) == 3u);
    CHECK(atomic_load(&l) == -4);
    CHECK(atomic_load(&ull) == 5);
    CHECK(atomic_load(&l8) == -6);
    CHECK(atomic_load(&l16) == 7);
    CHECK(atomic_load(&f32) == -8);
    CHECK(atomic_load(&f64) == 9);
    CHECK(atomic_load(&ip) == -10);
    CHECK(atomic_load(&sz) == 11);
    CHECK(atomic_load(&pd) == -12);
    CHECK(atomic_load(&um) == 13);
    return 0;
}

/* Values narrower than the object are converted, as in an assignment. */
static int test_atomic_narrowing(int seed, int rep) {
    _Atomic(uint8_t) u8;
    _Atomic(int8_t) i8;
    _Atomic(uint16_t) u16;
    volatile unsigned wide = 0x1FF; /* not a constant, to avoid warnings */

    atomic_init(&u8, 0);
    atomic_store(&u8, wide);
    CHECK(atomic_load(&u8) == 0xFF);
    wide = 256;
    atomic_store(&u8, wide);
    CHECK(atomic_load(&u8) == 0);

    atomic_init(&i8, 0);
    atomic_store(&i8, -1);
    CHECK(atomic_load(&i8) == -1);
    CHECK(atomic_fetch_add(&i8, 1) == -1);
    CHECK(atomic_load(&i8) == 0);

    atomic_init(&u16, 0xFFFF);
    CHECK(atomic_fetch_add(&u16, 2) == 0xFFFF);
    CHECK(atomic_load(&u16) == 1);
    return 0;
}

static int test_atomic_neighbours(int seed, int rep) {
    /* Operations must stay within their own object. */
    struct {
        _Atomic(uint8_t) a;
        _Atomic(uint8_t) b;
        _Atomic(uint8_t) c;
    } s;

    atomic_init(&s.a, 0xAA);
    atomic_init(&s.b, 0);
    atomic_init(&s.c, 0xCC);

    atomic_fetch_add(&s.b, 0xFF);
    atomic_fetch_add(&s.b, 0xFF);
    atomic_fetch_or(&s.b, 0xF0);
    atomic_fetch_xor(&s.b, 0x0F);
    atomic_exchange(&s.b, 0x11);
    CHECK(atomic_load(&s.a) == 0xAA);
    CHECK(atomic_load(&s.c) == 0xCC);
    CHECK(atomic_load(&s.b) == 0x11);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Threads                                                           */
/* ---------------------------------------------------------------- */

    #define THREADS 4
    #define ROUNDS 20000

struct shared {
    _Atomic(int) counter;
    _Atomic(unsigned long long) big;
    _Atomic(int) cas_counter;
    _Atomic(uint8_t) narrow;
    atomic_flag lock;
    long guarded;
};

static int hammer(void *arg) {
    struct shared *s = arg;
    int i;

    for (i = 0; i < ROUNDS; i++) {
        int expected;

        atomic_fetch_add(&s->counter, 1);
        atomic_fetch_add(&s->big, 3);
        atomic_fetch_add(&s->narrow, 1);

        expected = atomic_load(&s->cas_counter);
        while (!atomic_compare_exchange_weak(
            &s->cas_counter, &expected, expected + 1
        ))
            ;

        while (
            atomic_flag_test_and_set_explicit(&s->lock, memory_order_acquire))
            thrd_yield();
        s->guarded++;
        atomic_flag_clear_explicit(&s->lock, memory_order_release);
    }
    return 0;
}

static int test_atomic_contention(int seed, int rep) {
    struct shared s;
    thrd_t t[THREADS];
    int i;

    atomic_init(&s.counter, 0);
    atomic_init(&s.big, 0);
    atomic_init(&s.cas_counter, 0);
    atomic_init(&s.narrow, 0);
    pf_atomic_flag_init(&s.lock);
    s.guarded = 0;

    for (i = 0; i < THREADS; i++)
        CHECK(thrd_create(&t[i], hammer, &s) == thrd_success);
    for (i = 0; i < THREADS; i++)
        CHECK(thrd_join(t[i], NULL) == thrd_success);

    CHECK(atomic_load(&s.counter) == THREADS * ROUNDS);
    CHECK(atomic_load(&s.big) == 3ull * THREADS * ROUNDS);
    CHECK(atomic_load(&s.cas_counter) == THREADS * ROUNDS);
    CHECK(atomic_load(&s.narrow) == (uint8_t)(THREADS * ROUNDS));
    CHECK(s.guarded == (long)THREADS * ROUNDS);
    return 0;
}

struct handoff {
    int data[64];
    _Atomic(int) ready;
};

static int producer(void *arg) {
    struct handoff *h = arg;
    int i;
    for (i = 0; i < 64; i++)
        h->data[i] = i * i;
    atomic_store_explicit(&h->ready, 1, memory_order_release);
    return 0;
}

static int consumer(void *arg) {
    struct handoff *h = arg;
    int i;
    while (!atomic_load_explicit(&h->ready, memory_order_acquire))
        thrd_yield();
    for (i = 0; i < 64; i++)
        if (h->data[i] != i * i)
            return 1;
    return 0;
}

static int test_atomic_release_acquire(int seed, int rep) {
    struct handoff h;
    thrd_t p, c;
    int bad = -1;

    memset(h.data, 0, sizeof h.data);
    atomic_init(&h.ready, 0);

    CHECK(thrd_create(&c, consumer, &h) == thrd_success);
    CHECK(thrd_create(&p, producer, &h) == thrd_success);
    CHECK(thrd_join(p, NULL) == thrd_success);
    CHECK(thrd_join(c, &bad) == thrd_success);
    CHECK(bad == 0);
    return 0;
}

static int claim_slot(void *arg) {
    _Atomic(int) *next = arg;
    int mine, got = 0;
    while ((mine = atomic_fetch_add(next, 1)) < 1000)
        got++;
    return got;
}

/* Every index is handed out to exactly one thread. */
static int test_atomic_work_sharing(int seed, int rep) {
    _Atomic(int) next;
    thrd_t t[THREADS];
    int i, total = 0, r;

    atomic_init(&next, 0);
    for (i = 0; i < THREADS; i++)
        CHECK(thrd_create(&t[i], claim_slot, &next) == thrd_success);
    for (i = 0; i < THREADS; i++) {
        CHECK(thrd_join(t[i], &r) == thrd_success);
        total += r;
    }
    CHECK(total == 1000);
    return 0;
}

#endif /* !PF_NO_ATOMICS */

/* ---------------------------------------------------------------- */
/* Registration                                                      */
/* ---------------------------------------------------------------- */

#define ENTRIES(NAME, TYPE, UTYPE)                                  \
    { test_atomic_load_store_##NAME, "/atomic/load_store_" #NAME }, \
        { test_atomic_exchange_##NAME, "/atomic/exchange_" #NAME }, \
        { test_atomic_arith_##NAME, "/atomic/arith_" #NAME },       \
        { test_atomic_bitwise_##NAME, "/atomic/bitwise_" #NAME },   \
        { test_atomic_cas_##NAME, "/atomic/cas_" #NAME },

pf_test_t suite_atomic[] = {
#ifndef PF_NO_ATOMICS
    ATOMIC_TYPES(ENTRIES) { test_atomic_bool, "/atomic/bool" },
    { test_atomic_flag, "/atomic/flag" },
    { test_atomic_flag_reinit, "/atomic/flag_reinit" },
    { test_atomic_memory_order, "/atomic/memory_order" },
    { test_atomic_fences, "/atomic/fences" },
    { test_atomic_lock_free, "/atomic/lock_free" },
    { test_atomic_typedefs, "/atomic/typedefs" },
    { test_atomic_narrowing, "/atomic/narrowing" },
    { test_atomic_neighbours, "/atomic/neighbours" },
    { test_atomic_contention, "/atomic/contention" },
    { test_atomic_release_acquire, "/atomic/release_acquire" },
    { test_atomic_work_sharing, "/atomic/work_sharing" },
#endif
    { 0 },
};
