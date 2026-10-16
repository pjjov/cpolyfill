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

#include "../include/pf_types.h"
#include "../include/pf_array.h"
#include "check.h"
#include <stdint.h>

struct triple {
    int a;
    short b;
    char c[5];
};

typedef PF_ARRAY(int) int_array;
typedef PF_ARRAY(struct triple) triple_array;

static int test_array_init(int seed, int rep) {
    int_array a;

    CHECK(PF_ARRAY_INIT(&a, 4) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_LEN(&a) == 0);
    CHECK(PF_ARRAY_CAP(&a) == 4);
    CHECK(a.base.items != NULL);
    PF_ARRAY_FREE(&a);

    CHECK(PF_ARRAY_INIT(&a, 0) == PF_ARRAY_EINVAL);
    CHECK(pf__array_init(NULL, 8) == PF_ARRAY_EINVAL);
    return 0;
}

static int test_array_null(int seed, int rep) {
    int x = 1;

    CHECK(pf__array_len(NULL) == 0);
    CHECK(pf__array_cap(NULL) == 0);
    CHECK(pf__array_get(NULL, 0) == NULL);
    CHECK(pf__array_slot(NULL, 0) == NULL);
    CHECK(pf__array_push(NULL, &x, sizeof x) == PF_ARRAY_EINVAL);
    CHECK(pf__array_pop(NULL, &x, sizeof x) == PF_ARRAY_EINVAL);
    CHECK(pf__array_shift(NULL, &x, sizeof x) == PF_ARRAY_EINVAL);
    CHECK(pf__array_unshift(NULL, &x, sizeof x) == PF_ARRAY_EINVAL);
    CHECK(pf__array_incr(NULL, sizeof x) == NULL);
    CHECK(pf__array_decr(NULL, sizeof x) == NULL);
    CHECK(pf__array_reserve(NULL, sizeof x) == PF_ARRAY_EINVAL);
    CHECK(pf__array_resize(NULL, sizeof x) == PF_ARRAY_EINVAL);
    CHECK(pf__array_setlen(NULL, 0) == PF_ARRAY_EINVAL);
    CHECK(pf__array_make(NULL, &x, 0, 0) == PF_ARRAY_EINVAL);
    pf__array_free(NULL);
    return 0;
}

static int test_array_push_get(int seed, int rep) {
    int_array a;
    int i;

    CHECK(PF_ARRAY_INIT(&a, 1) == PF_ARRAY_OK);

    for (i = 0; i < 1000; i++) {
        int v = i * 3 - 7;
        CHECK(PF_ARRAY_PUSH(&a, &v, 1) == PF_ARRAY_OK);
        CHECK(PF_ARRAY_LEN(&a) == (size_t)i + 1);
        CHECK(PF_ARRAY_CAP(&a) >= PF_ARRAY_LEN(&a));
    }

    for (i = 0; i < 1000; i++) {
        int *p = PF_ARRAY_GET(&a, i);
        CHECK(p != NULL);
        CHECK(*p == i * 3 - 7);
    }

    /* Out of bounds. */
    CHECK(PF_ARRAY_GET(&a, 1000) == NULL);
    CHECK(PF_ARRAY_GET(&a, 123456) == NULL);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_push_many(int seed, int rep) {
    int_array a;
    int v[5] = { 1, 2, 3, 4, 5 };
    int i;

    CHECK(PF_ARRAY_INIT(&a, 2) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_PUSH(&a, v, 5) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_PUSH(&a, v, 5) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_LEN(&a) == 10);

    for (i = 0; i < 10; i++)
        CHECK(*PF_ARRAY_GET(&a, i) == v[i % 5]);

    CHECK(PF_ARRAY_PUSH(&a, v, 0) == PF_ARRAY_EINVAL);
    CHECK(PF_ARRAY_LEN(&a) == 10);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_pop(int seed, int rep) {
    int_array a;
    int i, out = -1;

    CHECK(PF_ARRAY_INIT(&a, 4) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_POP(&a, &out, 1) == PF_ARRAY_EINVAL);

    for (i = 0; i < 6; i++)
        CHECK(PF_ARRAY_PUSH(&a, &i, 1) == PF_ARRAY_OK);

    for (i = 5; i >= 0; i--) {
        CHECK(PF_ARRAY_POP(&a, &out, 1) == PF_ARRAY_OK);
        CHECK(out == i);
        CHECK(PF_ARRAY_LEN(&a) == (size_t)i);
    }

    CHECK(PF_ARRAY_POP(&a, &out, 1) == PF_ARRAY_EINVAL);

    /* The output is optional. */
    i = 9;
    CHECK(PF_ARRAY_PUSH(&a, &i, 1) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_POP(&a, NULL, 1) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_LEN(&a) == 0);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_pop_many(int seed, int rep) {
    int_array a;
    int v[4] = { 10, 20, 30, 40 }, out[3] = { 0 };

    CHECK(PF_ARRAY_INIT(&a, 8) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_PUSH(&a, v, 4) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_POP(&a, out, 5) == PF_ARRAY_EINVAL);
    CHECK(PF_ARRAY_LEN(&a) == 4);
    CHECK(PF_ARRAY_POP(&a, out, 3) == PF_ARRAY_OK);
    CHECK(out[0] == 20 && out[1] == 30 && out[2] == 40);
    CHECK(PF_ARRAY_LEN(&a) == 1);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_shift_unshift(int seed, int rep) {
    int_array a;
    int i, out = 0;

    CHECK(PF_ARRAY_INIT(&a, 1) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_SHIFT(&a, &out, 1) == PF_ARRAY_EINVAL);

    for (i = 0; i < 20; i++)
        CHECK(PF_ARRAY_UNSHIFT(&a, &i, 1) == PF_ARRAY_OK);

    /* Unshifting reverses the order. */
    for (i = 0; i < 20; i++)
        CHECK(*PF_ARRAY_GET(&a, i) == 19 - i);

    for (i = 19; i >= 0; i--) {
        CHECK(PF_ARRAY_SHIFT(&a, &out, 1) == PF_ARRAY_OK);
        CHECK(out == i);
    }

    CHECK(PF_ARRAY_LEN(&a) == 0);
    CHECK(PF_ARRAY_SHIFT(&a, &out, 1) == PF_ARRAY_EINVAL);
    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_incr_decr(int seed, int rep) {
    int_array a;
    int *p;

    CHECK(PF_ARRAY_INIT(&a, 2) == PF_ARRAY_OK);

    p = PF_ARRAY_INCR(&a, 3);
    CHECK(p != NULL);
    CHECK(PF_ARRAY_LEN(&a) == 3);
    p[0] = 1;
    p[1] = 2;
    p[2] = 3;

    p = PF_ARRAY_INCR(&a, 1);
    CHECK(p != NULL);
    *p = 4;
    CHECK(PF_ARRAY_LEN(&a) == 4);
    CHECK(*PF_ARRAY_GET(&a, 0) == 1);
    CHECK(*PF_ARRAY_GET(&a, 3) == 4);

    CHECK(PF_ARRAY_INCR(&a, 0) == NULL);
    CHECK(PF_ARRAY_DECR(&a, 0) == NULL);
    CHECK(PF_ARRAY_DECR(&a, 5) == NULL);
    CHECK(PF_ARRAY_LEN(&a) == 4);

    p = PF_ARRAY_DECR(&a, 2);
    CHECK(p != NULL);
    CHECK(p[0] == 3 && p[1] == 4);
    CHECK(PF_ARRAY_LEN(&a) == 2);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_capacity(int seed, int rep) {
    int_array a;
    int i;

    CHECK(PF_ARRAY_INIT(&a, 2) == PF_ARRAY_OK);
    for (i = 0; i < 2; i++)
        CHECK(PF_ARRAY_PUSH(&a, &i, 1) == PF_ARRAY_OK);

    /* Reserving what is available keeps the buffer. */
    CHECK(PF_ARRAY_RESERVE(&a, 0) == PF_ARRAY_EINVAL);
    CHECK(PF_ARRAY_RESERVE(&a, 10) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_CAP(&a) >= 12);
    CHECK(PF_ARRAY_LEN(&a) == 2);

    /* Resizing keeps the contents. */
    CHECK(PF_ARRAY_RESIZE(&a, 64) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_CAP(&a) == 64);
    CHECK(*PF_ARRAY_GET(&a, 0) == 0 && *PF_ARRAY_GET(&a, 1) == 1);
    CHECK(PF_ARRAY_RESIZE(&a, 0) == PF_ARRAY_EINVAL);

    /* Shrinking below the length truncates it. */
    CHECK(PF_ARRAY_RESIZE(&a, 1) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_CAP(&a) == 1);
    CHECK(PF_ARRAY_LEN(&a) == 1);
    CHECK(*PF_ARRAY_GET(&a, 0) == 0);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_setlen_slot(int seed, int rep) {
    int_array a;
    int *p;

    CHECK(PF_ARRAY_INIT(&a, 8) == PF_ARRAY_OK);

    /* Slots exist up to the capacity, items only up to the length. */
    p = PF_ARRAY_SLOT(&a, 7);
    CHECK(p != NULL);
    *p = 77;
    CHECK(PF_ARRAY_SLOT(&a, 8) == NULL);
    CHECK(PF_ARRAY_GET(&a, 7) == NULL);

    CHECK(PF_ARRAY_SETLEN(&a, 9) == PF_ARRAY_EINVAL);
    CHECK(PF_ARRAY_LEN(&a) == 0);
    CHECK(PF_ARRAY_SETLEN(&a, 8) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_LEN(&a) == 8);
    CHECK(*PF_ARRAY_GET(&a, 7) == 77);
    CHECK(PF_ARRAY_SETLEN(&a, 0) == PF_ARRAY_OK);

    PF_ARRAY_FREE(&a);
    return 0;
}

static int test_array_make(int seed, int rep) {
    int_array a;
    int buffer[6] = { 1, 2, 3, 4, 5, 6 };

    CHECK(PF_ARRAY_MAKE(&a, buffer, 4, 6) == PF_ARRAY_OK);
    CHECK(PF_ARRAY_LEN(&a) == 4);
    CHECK(PF_ARRAY_CAP(&a) == 6);
    CHECK(PF_ARRAY_GET(&a, 0) == &buffer[0]);
    CHECK(PF_ARRAY_GET(&a, 3) == &buffer[3]);
    CHECK(PF_ARRAY_GET(&a, 4) == NULL);
    CHECK(PF_ARRAY_SLOT(&a, 5) == &buffer[5]);
    return 0;
}

/* The element size scales every operation. */
static int test_array_struct(int seed, int rep) {
    triple_array a;
    struct triple t, out;
    int i;

    CHECK(PF_ARRAY_INIT(&a, 2) == PF_ARRAY_OK);
    CHECK(a.base.capacity == 2 * sizeof(struct triple));

    for (i = 0; i < 50; i++) {
        memset(&t, 0, sizeof t);
        t.a = i;
        t.b = (short)(i * 2);
        snprintf(t.c, sizeof t.c, "%d", i % 1000);
        CHECK(PF_ARRAY_PUSH(&a, &t, 1) == PF_ARRAY_OK);
    }

    CHECK(PF_ARRAY_LEN(&a) == 50);
    CHECK(a.base.length == 50 * sizeof(struct triple));

    for (i = 0; i < 50; i++) {
        struct triple *p = PF_ARRAY_GET(&a, i);
        CHECK(p != NULL);
        CHECK(p->a == i);
        CHECK(p->b == i * 2);
    }
    CHECK(PF_ARRAY_GET(&a, 50) == NULL);

    CHECK(PF_ARRAY_POP(&a, &out, 1) == PF_ARRAY_OK);
    CHECK(out.a == 49);
    CHECK(PF_ARRAY_SHIFT(&a, &out, 1) == PF_ARRAY_OK);
    CHECK(out.a == 0);
    CHECK(PF_ARRAY_LEN(&a) == 48);
    CHECK(PF_ARRAY_GET(&a, 0)->a == 1);

    PF_ARRAY_FREE(&a);
    return 0;
}

/* Random operations checked against a plain C array. */
static int test_array_model(int seed, int rep) {
    enum { MAX = 512 };
    int_array a;
    int model[MAX];
    size_t len = 0;
    unsigned state = (unsigned)seed * 2654435761u + (unsigned)rep;
    size_t i;
    int step;

    CHECK(PF_ARRAY_INIT(&a, 1) == PF_ARRAY_OK);

    for (step = 0; step < 2000; step++) {
        int value, out;

        state = state * 1664525u + 1013904223u;
        value = (int)(state >> 8);

        switch ((state >> 4) % 4) {
        case 0:
            if (len < MAX) {
                CHECK(PF_ARRAY_PUSH(&a, &value, 1) == PF_ARRAY_OK);
                model[len++] = value;
            }
            break;
        case 1:
            if (len < MAX) {
                CHECK(PF_ARRAY_UNSHIFT(&a, &value, 1) == PF_ARRAY_OK);
                memmove(model + 1, model, len * sizeof *model);
                model[0] = value;
                len++;
            }
            break;
        case 2:
            if (len > 0) {
                CHECK(PF_ARRAY_POP(&a, &out, 1) == PF_ARRAY_OK);
                CHECK(out == model[--len]);
            }
            break;
        default:
            if (len > 0) {
                CHECK(PF_ARRAY_SHIFT(&a, &out, 1) == PF_ARRAY_OK);
                CHECK(out == model[0]);
                memmove(model, model + 1, --len * sizeof *model);
            }
            break;
        }

        CHECK(PF_ARRAY_LEN(&a) == len);
    }

    for (i = 0; i < len; i++)
        CHECK(*PF_ARRAY_GET(&a, i) == model[i]);

    PF_ARRAY_FREE(&a);
    return 0;
}

pf_test_t suite_array[] = {
    { test_array_init, "/array/init" },
    { test_array_null, "/array/null" },
    { test_array_push_get, "/array/push_get" },
    { test_array_push_many, "/array/push_many" },
    { test_array_pop, "/array/pop" },
    { test_array_pop_many, "/array/pop_many" },
    { test_array_shift_unshift, "/array/shift_unshift" },
    { test_array_incr_decr, "/array/incr_decr" },
    { test_array_capacity, "/array/capacity" },
    { test_array_setlen_slot, "/array/setlen_slot" },
    { test_array_make, "/array/make" },
    { test_array_struct, "/array/struct" },
    { test_array_model, "/array/model" },
    { 0 },
};
