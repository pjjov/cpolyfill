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
#include "util.h"
#include <errno.h>
#include <fcntl.h> /* before the header, which only defines O_CREAT if missing */
#include <limits.h>
#include "../include/pf_semaphore.h"
#include "../include/pf_threads.h"

/* Unnamed semaphores are not available everywhere (macOS refuses them). */
static int sem_make(sem_t *sem, unsigned value) {
    if (sem_init(sem, 0, value) == 0)
        return 1;
    return 0;
}

#define NEED_SEM(sem, value)                                 \
    if (!sem_make(&(sem), (value)))                          \
    return SKIP("unnamed semaphores are not supported here")

static int test_sem_init_getvalue(int seed, int rep) {
    sem_t sem;
    int v = -1;

    NEED_SEM(sem, 3);
    CHECK(sem_getvalue(&sem, &v) == 0);
    CHECK(v == 3);
    CHECK(sem_destroy(&sem) == 0);

    NEED_SEM(sem, 0);
    CHECK(sem_getvalue(&sem, &v) == 0);
    CHECK(v == 0);
    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

static int test_sem_trywait(int seed, int rep) {
    sem_t sem;
    int i;

    NEED_SEM(sem, 3);
    for (i = 0; i < 3; i++)
        CHECK(sem_trywait(&sem) == 0);

    errno = 0;
    CHECK(sem_trywait(&sem) == -1);
    CHECK(errno == EAGAIN);

    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

static int test_sem_post_wait(int seed, int rep) {
    sem_t sem;
    int v = -1, i;

    NEED_SEM(sem, 0);
    for (i = 1; i <= 5; i++) {
        CHECK(sem_post(&sem) == 0);
        CHECK(sem_getvalue(&sem, &v) == 0);
        CHECK(v == i);
    }
    for (i = 4; i >= 0; i--) {
        CHECK(sem_wait(&sem) == 0);
        CHECK(sem_getvalue(&sem, &v) == 0);
        CHECK(v == i);
    }
    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

static int test_sem_timedwait(int seed, int rep) {
    sem_t sem;
    struct timespec ts;
    double start, elapsed;

    NEED_SEM(sem, 1);

    /* Available: returns at once, whatever the deadline. */
    test_deadline(&ts, 5000);
    start = test_now();
    CHECK(sem_timedwait(&sem, &ts) == 0);
    CHECK(test_now() - start < 2.0);

    /* Empty: times out after about the deadline. */
    test_deadline(&ts, 60);
    start = test_now();
    errno = 0;
    CHECK(sem_timedwait(&sem, &ts) == -1);
    elapsed = test_now() - start;
    CHECK(errno == ETIMEDOUT);
    CHECK(elapsed >= 0.04);
    CHECK(elapsed < 3.0);

    /* A deadline in the past is a poll. */
    test_deadline(&ts, 0);
    ts.tv_sec -= 5;
    errno = 0;
    CHECK(sem_timedwait(&sem, &ts) == -1);
    CHECK(errno == ETIMEDOUT);

    CHECK(sem_post(&sem) == 0);
    CHECK(sem_timedwait(&sem, &ts) == 0); /* even if already expired */

    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

static int test_sem_overflow(int seed, int rep) {
    sem_t sem;

    NEED_SEM(sem, SEM_VALUE_MAX);
    errno = 0;
    CHECK(sem_post(&sem) == -1);
    CHECK(errno == EOVERFLOW);
    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

struct poster {
    sem_t *sem;
    unsigned delay_ms;
    int count;
};

static int post_after(void *arg) {
    struct poster *p = arg;
    int i;

    test_sleep_ms(p->delay_ms);
    for (i = 0; i < p->count; i++)
        sem_post(p->sem);
    return 0;
}

/* sem_wait blocks until another thread posts. */
static int test_sem_wait_blocks(int seed, int rep) {
    sem_t sem;
    thrd_t t;
    struct poster p;
    double start;

    NEED_SEM(sem, 0);
    p.sem = &sem;
    p.delay_ms = 80;
    p.count = 1;

    start = test_now();
    CHECK(thrd_create(&t, post_after, &p) == thrd_success);
    CHECK(sem_wait(&sem) == 0);
    CHECK(test_now() - start >= 0.05);
    CHECK(thrd_join(t, NULL) == thrd_success);

    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

struct worker {
    sem_t *sem;
    int *counter;
    int rounds;
};

static int increment_under_sem(void *arg) {
    struct worker *w = arg;
    int i;

    for (i = 0; i < w->rounds; i++) {
        sem_wait(w->sem);
        ++*w->counter;
        sem_post(w->sem);
    }
    return 0;
}

/* A binary semaphore works as a lock. */
static int test_sem_mutual_exclusion(int seed, int rep) {
    enum { THREADS = 4, ROUNDS = 5000 };
    sem_t sem;
    thrd_t t[THREADS];
    struct worker w;
    int counter = 0, i;

    NEED_SEM(sem, 1);
    w.sem = &sem;
    w.counter = &counter;
    w.rounds = ROUNDS;

    for (i = 0; i < THREADS; i++)
        CHECK(thrd_create(&t[i], increment_under_sem, &w) == thrd_success);
    for (i = 0; i < THREADS; i++)
        CHECK(thrd_join(t[i], NULL) == thrd_success);

    CHECK(counter == THREADS * ROUNDS);
    CHECK(sem_destroy(&sem) == 0);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Named semaphores                                                  */
/* ---------------------------------------------------------------- */

static char *sem_name(char *buf, size_t size) {
    static int counter = 0;
    snprintf(buf, size, "/pf-test-%ld-%d", test_pid(), counter++);
    return buf;
}

#define NEED_NAMED(sem, name, value)                                         \
    do {                                                                     \
        (sem) = sem_open((name), O_CREAT | O_EXCL, 0600, (unsigned)(value)); \
        if ((sem) == SEM_FAILED)                                             \
            return SKIP("named semaphores are not available here");          \
    } while (0)

static int test_sem_named(int seed, int rep) {
    char name[64];
    sem_t *a, *b;
    int v = -1;

    sem_name(name, sizeof name);
    NEED_NAMED(a, name, 2);

    /* A second handle reaches the same semaphore. */
    b = sem_open(name, 0);
    CHECK(b != SEM_FAILED);
    CHECK(sem_getvalue(b, &v) == 0);
    CHECK(v == 2);

    CHECK(sem_wait(a) == 0);
    CHECK(sem_getvalue(b, &v) == 0);
    CHECK(v == 1);
    CHECK(sem_post(b) == 0);
    CHECK(sem_post(b) == 0);
    CHECK(sem_getvalue(a, &v) == 0);
    CHECK(v == 3);

    CHECK(sem_close(b) == 0);
    CHECK(sem_close(a) == 0);
    CHECK(sem_unlink(name) == 0);
    return 0;
}

static int test_sem_named_exclusive(int seed, int rep) {
    char name[64];
    sem_t *a, *b;

    sem_name(name, sizeof name);
    NEED_NAMED(a, name, 0);

    errno = 0;
    b = sem_open(name, O_CREAT | O_EXCL, 0600, 0u);
    CHECK(b == SEM_FAILED);
    CHECK(errno == EEXIST);

    /* Without O_EXCL an existing semaphore is simply opened. */
    b = sem_open(name, O_CREAT, 0600, 9u);
    CHECK(b != SEM_FAILED);
    {
        int v = -1;
        CHECK(sem_getvalue(b, &v) == 0);
        CHECK(v == 0); /* the initial value only applies at creation */
    }

    CHECK(sem_close(b) == 0);
    CHECK(sem_close(a) == 0);
    CHECK(sem_unlink(name) == 0);
    return 0;
}

static int test_sem_named_missing(int seed, int rep) {
    char name[64];
    sem_t *a;

    sem_name(name, sizeof name);

    errno = 0;
    CHECK(sem_open(name, 0) == SEM_FAILED);
    CHECK(errno == ENOENT);

    errno = 0;
    CHECK(sem_unlink(name) == -1);
    CHECK(errno == ENOENT);

    /* Once unlinked the name is gone, and the handle keeps working. */
    NEED_NAMED(a, name, 1);
    CHECK(sem_unlink(name) == 0);
    errno = 0;
    CHECK(sem_open(name, 0) == SEM_FAILED);
    CHECK(errno == ENOENT);
    CHECK(sem_trywait(a) == 0);
    CHECK(sem_close(a) == 0);
    return 0;
}

pf_test_t suite_semaphore[] = {
    { test_sem_init_getvalue, "/semaphore/init_getvalue" },
    { test_sem_trywait, "/semaphore/trywait" },
    { test_sem_post_wait, "/semaphore/post_wait" },
    { test_sem_timedwait, "/semaphore/timedwait" },
    { test_sem_overflow, "/semaphore/overflow" },
    { test_sem_wait_blocks, "/semaphore/wait_blocks" },
    { test_sem_mutual_exclusion, "/semaphore/mutual_exclusion" },
    { test_sem_named, "/semaphore/named" },
    { test_sem_named_exclusive, "/semaphore/named_exclusive" },
    { test_sem_named_missing, "/semaphore/named_missing" },
    { 0 },
};
