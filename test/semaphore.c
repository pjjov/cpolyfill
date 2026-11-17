/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/semaphore.h>, and the library's own semaphore that macOS and
   Windows use. */

#include "../src/internal.h"

#include <cpolyfill/semaphore.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <stdio.h>

#include "check.h"

/* The same checks on pf_sem_t and on the library's own semaphore. */
typedef struct {
    int (*init)(void *s, unsigned value);
    int (*destroy)(void *s);
    int (*wait)(void *s);
    int (*trywait)(void *s);
    int (*timedwait)(void *s, const struct timespec *deadline);
    int (*post)(void *s);
    int (*getvalue)(void *s, int *value);
} sem_ops;

static int pub_init(void *s, unsigned v) {
    return pf_sem_init((pf_sem_t *)s, 0, v);
}
static int pub_destroy(void *s) { return pf_sem_destroy((pf_sem_t *)s); }
static int pub_wait(void *s) { return pf_sem_wait((pf_sem_t *)s); }
static int pub_trywait(void *s) { return pf_sem_trywait((pf_sem_t *)s); }
static int pub_timedwait(void *s, const struct timespec *d) {
    return pf_sem_timedwait((pf_sem_t *)s, d);
}
static int pub_post(void *s) { return pf_sem_post((pf_sem_t *)s); }
static int pub_getvalue(void *s, int *v) {
    return pf_sem_getvalue((pf_sem_t *)s, v);
}

static int loc_init(void *s, unsigned v) {
    return pf__sem_local_init((struct pf__sem_local *)s, v);
}
static int loc_destroy(void *s) {
    return pf__sem_local_destroy((struct pf__sem_local *)s);
}
static int loc_wait(void *s) {
    return pf__sem_local_wait((struct pf__sem_local *)s, NULL);
}
static int loc_trywait(void *s) {
    return pf__sem_local_trywait((struct pf__sem_local *)s);
}
static int loc_timedwait(void *s, const struct timespec *d) {
    return pf__sem_local_wait((struct pf__sem_local *)s, d);
}
static int loc_post(void *s) {
    return pf__sem_local_post((struct pf__sem_local *)s);
}
static int loc_getvalue(void *s, int *v) {
    return pf__sem_local_getvalue((struct pf__sem_local *)s, v);
}

static const sem_ops implementations[] = {
    { pub_init,
      pub_destroy,
      pub_wait,
      pub_trywait,
      pub_timedwait,
      pub_post,
      pub_getvalue },
    { loc_init,
      loc_destroy,
      loc_wait,
      loc_trywait,
      loc_timedwait,
      loc_post,
      loc_getvalue },
};

static union {
    pf_sem_t pub;
    struct pf__sem_local loc;
} sem;

static const sem_ops *ops;

static void check_value(int expected) {
    int v = -1;
    PF_ASSERT_ALWAYS_EQ(ops->getvalue(&sem, &v), 0);
    PF_ASSERT_ALWAYS_EQ(v, expected);
}

static void check_errno(int rc, int err) {
    PF_ASSERT_ALWAYS_EQ(rc, -1);
    PF_ASSERT_ALWAYS_EQ(errno, err);
}

static int test_basic(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(implementations) / sizeof(implementations[0]); k++) {
        struct timespec d;
        uint64_t start;

        ops = &implementations[k];
        PF_ASSERT_ALWAYS_EQ(ops->init(&sem, 2), 0);
        check_value(2);
        PF_ASSERT_ALWAYS_EQ(ops->trywait(&sem), 0);
        PF_ASSERT_ALWAYS_EQ(ops->wait(&sem), 0);
        check_errno(ops->trywait(&sem), EAGAIN);
        check_value(0);

        PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&d, 20000000), 0);
        start = pf_time_ns();
        check_errno(ops->timedwait(&sem, &d), ETIMEDOUT);
        PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 15000000u);

        PF_ASSERT_ALWAYS_EQ(ops->post(&sem), 0);
        PF_ASSERT_ALWAYS_EQ(ops->post(&sem), 0);
        check_value(2);
        PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&d, 1000000000), 0);
        PF_ASSERT_ALWAYS_EQ(ops->timedwait(&sem, &d), 0);
        d.tv_nsec = -5;
        check_errno(ops->timedwait(&sem, &d), EINVAL);
        PF_ASSERT_ALWAYS_EQ(ops->destroy(&sem), 0);

        check_errno(ops->init(&sem, (unsigned)PF_SEM_VALUE_MAX + 1), EINVAL);
        PF_ASSERT_ALWAYS_EQ(ops->init(&sem, PF_SEM_VALUE_MAX), 0);
        check_errno(ops->post(&sem), EOVERFLOW);
        PF_ASSERT_ALWAYS_EQ(ops->destroy(&sem), 0);
    }
    return 0;
}

static int test_clockwait(int seed, int i) {
    struct timespec d;
    uint64_t start;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_sem_init(&sem.pub, 0, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_MONOTONIC, &d), 0);
    d = pf_timespec_add(d, pf_timespec_from_ns(20000000));
    start = pf_time_ns();
    check_errno(pf_sem_clockwait(&sem.pub, PF_CLOCK_MONOTONIC, &d), ETIMEDOUT);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 15000000u);

    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&d, 10000000), 0);
    check_errno(pf_sem_clockwait(&sem.pub, PF_CLOCK_REALTIME, &d), ETIMEDOUT);
    check_errno(
        pf_sem_clockwait(&sem.pub, PF_CLOCK_THREAD_CPUTIME_ID, &d), EINVAL
    );

    PF_ASSERT_ALWAYS_EQ(pf_sem_post(&sem.pub), 0);
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_MONOTONIC, &d), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sem_clockwait(&sem.pub, PF_CLOCK_MONOTONIC, &d), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sem_destroy(&sem.pub), 0);
    return 0;
}

#if PF_HAS_THREADS

    #define CONSUMERS 4
    #define ITEMS 2000

static int consume(void *arg) {
    int k;
    (void)arg;
    for (k = 0; k < ITEMS; k++)
        PF_ASSERT_ALWAYS_EQ(ops->wait(&sem), 0);
    return 0;
}

static int test_threads(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(implementations) / sizeof(implementations[0]); k++) {
        pf_thrd_t t[CONSUMERS];
        int n;

        ops = &implementations[k];
        PF_ASSERT_ALWAYS_EQ(ops->init(&sem, 0), 0);
        for (n = 0; n < CONSUMERS; n++)
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&t[n], consume, NULL), pf_thrd_success
            );
        for (n = 0; n < CONSUMERS * ITEMS; n++)
            PF_ASSERT_ALWAYS_EQ(ops->post(&sem), 0);
        for (n = 0; n < CONSUMERS; n++)
            PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t[n], NULL), pf_thrd_success);
        check_value(0);
        PF_ASSERT_ALWAYS_EQ(ops->destroy(&sem), 0);
    }
    return 0;
}

static int test_named(int seed, int i) {
    char name[64];
    pf_sem_t *a, *b;
    (void)seed, (void)i;

    snprintf(
        name, sizeof(name), "/cpolyfill-test-%lu", (unsigned long)pf_thrd_id()
    );
    pf_sem_unlink(name);

    a = pf_sem_open(name, PF_O_CREAT | PF_O_EXCL, 0600u, 1u);
    PF_ASSERT_ALWAYS(a != PF_SEM_FAILED);
    errno = 0;
    PF_ASSERT_ALWAYS(
        pf_sem_open(name, PF_O_CREAT | PF_O_EXCL, 0600u, 1u) == PF_SEM_FAILED
    );
    PF_ASSERT_ALWAYS_EQ(errno, EEXIST);
    b = pf_sem_open(name, 0);
    PF_ASSERT_ALWAYS(b != PF_SEM_FAILED);

    /* The same semaphore through both. */
    PF_ASSERT_ALWAYS_EQ(pf_sem_trywait(b), 0);
    check_errno(pf_sem_trywait(a), EAGAIN);
    PF_ASSERT_ALWAYS_EQ(pf_sem_post(a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sem_wait(b), 0);
    {
        struct timespec d;
        PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&d, 10000000), 0);
        check_errno(pf_sem_timedwait(a, &d), ETIMEDOUT);
    }
    PF_ASSERT_ALWAYS_EQ(pf_sem_close(a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sem_close(b), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sem_unlink(name), 0);

    errno = 0;
    PF_ASSERT_ALWAYS(
        pf_sem_open("/cpolyfill-test-missing-sem", 0) == PF_SEM_FAILED
    );
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);
    return 0;
}

#else

static int test_no_threads(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_sem_init(&sem.pub, 0, 0), 0);
    check_errno(pf_sem_wait(&sem.pub), EDEADLK);
    PF_ASSERT_ALWAYS_EQ(pf_sem_destroy(&sem.pub), 0);
    errno = 0;
    PF_ASSERT_ALWAYS(
        pf_sem_open("/cpolyfill-test", PF_O_CREAT, 0600u, 1u) == PF_SEM_FAILED
    );
    PF_ASSERT_ALWAYS_EQ(errno, ENOSYS);
    return 0;
}

#endif

pf_test_t suite_semaphore[] = {
    { test_basic, "basic" },
    { test_clockwait, "clockwait" },
#if PF_HAS_THREADS
    { test_threads, "threads" },
    { test_named, "named" },
#else
    { test_no_threads, "no_threads" },
#endif
    { 0 },
};
