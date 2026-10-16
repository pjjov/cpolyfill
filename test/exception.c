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

#include <stdio.h>
#include <string.h>

/* The stack needs room for the handlers. */
#define PF_EXCEPTION_STACK_SIZE 8
#define PF_EXCEPTION_MODULE 7
#include "../include/pf_exception.h"
#include "check.h"

static int test_exception_init(int seed, int rep) {
    pf_exception_stack_t stack;
    char msg[32];

    memset(&stack, 0xAB, sizeof stack);
    pf_init_exception_stack(&stack, 4, (int)sizeof msg, msg);

    CHECK(stack.capacity == 4);
    CHECK(stack.length == 0);
    CHECK(stack.msgBuffer == msg);
    CHECK(stack.msgCapacity == (int)sizeof msg);

    pf_init_exception_stack(NULL, 4, 0, NULL); /* must not crash */
    return 0;
}

/* Without handlers nothing is thrown: the code is just returned. */
static int test_exception_throw_unhandled(int seed, int rep) {
    pf_exception_stack_t stack;

    pf_init_exception_stack(&stack, 4, 0, NULL);

    CHECK(pf_throw(&stack, 5, "boom") == 5);
    CHECK(pf_throw(&stack, -3, "negative") == -3);
    CHECK(pf_throw(&stack, 0, "not an error") == 0);
    CHECK(pf_throw(NULL, 9, "no stack") == 9);
    CHECK(stack.length == 0);
    return 0;
}

static int test_exception_current(int seed, int rep) {
    pf_exception_stack_t stack;
    int line;

    pf_init_exception_stack(&stack, 4, 0, NULL);

    line = __LINE__ + 1;
    CHECK(pf_throw(&stack, 12, "the message") == 12);

    /* The last exception is kept for inspection. */
    CHECK(stack.current.code == 12);
    CHECK(stack.current.module == PF_EXCEPTION_MODULE);
    CHECK(stack.current.msg != NULL);
    CHECK_STR(stack.current.msg, "the message");
#ifndef NDEBUG
    CHECK(stack.current.line == line);
    CHECK(strstr(stack.current.file, "exception.c") != NULL);
    CHECK_STR(stack.current.func, "test_exception_current");
#else
    (void)line;
#endif

    /* Code 0 is not an exception and leaves the state alone. */
    CHECK(pf_throw(&stack, 0, "ignored") == 0);
    CHECK(stack.current.code == 12);
    return 0;
}

static int test_exception_rethrow_args(int seed, int rep) {
    pf_exception_stack_t stack;
    pf_exception_t e;

    pf_init_exception_stack(&stack, 4, 0, NULL);
    memset(&e, 0, sizeof e);
    e.code = 4;

    CHECK(pf_rethrow(NULL, &e) == -1);
    CHECK(pf_rethrow(&stack, NULL) == -1);

    /* No handler: the code comes back, and the exception is recorded. */
    CHECK(pf_rethrow(&stack, &e) == 4);
    CHECK(stack.current.code == 4);
    return 0;
}

static int test_exception_capacity(int seed, int rep) {
    pf_exception_stack_t stack;

    /* No room at all: registering fails with NULL and registers nothing. */
    pf_init_exception_stack(&stack, 0, 0, NULL);
    CHECK(pf_try_catch(&stack, 0, 0) == NULL);
    CHECK(stack.length == 0);

    CHECK(pf_try_catch(NULL, 0, 0) == NULL);
    return 0;
}

static int test_exception_formatted(int seed, int rep) {
    pf_exception_stack_t stack;
    char buffer[64];

    pf_init_exception_stack(&stack, 4, (int)sizeof buffer, buffer);

    CHECK(pf_throwf(&stack, 3, "value %d of %s", 42, "x") == 3);
    CHECK(stack.current.msg != NULL);
    CHECK_STR(stack.current.msg, "value 42 of x");
    CHECK(stack.current.code == 3);

    CHECK(pf_throwf(&stack, 0, "code %d", 0) == 0);
    CHECK(pf_throwf(NULL, 6, "stackless %d", 1) == 6);
    return 0;
}

/* Output longer than the buffer is cut, but still terminated. */
static int test_exception_formatted_truncation(int seed, int rep) {
    pf_exception_stack_t stack;
    char buffer[8];

    pf_init_exception_stack(&stack, 4, (int)sizeof buffer, buffer);
    CHECK(pf_throwf(&stack, 3, "%s", "0123456789") == 3);
    CHECK(stack.current.msg == buffer);
    CHECK(strlen(buffer) < sizeof buffer);
    CHECK(strncmp(buffer, "0123456789", strlen(buffer)) == 0);
    return 0;
}

/* The throw jumps back to the handler, which then returns the exception. */
static int test_exception_catch(int seed, int rep) {
    pf_exception_stack_t stack;
    pf_exception_t *e;
    volatile int reached = 0, caught = 0;

    pf_init_exception_stack(&stack, 4, 0, NULL);

    e = pf_try_catch(&stack, 0, 0);
    if (e == NULL) {
        CHECK(stack.length == 1);
        pf_throw(&stack, 21, "caught");
        reached = 1; /* not reached */
    } else {
        caught = 1;
        CHECK(e->code == 21);
        CHECK_STR(e->msg, "caught");
    }
    CHECK(!reached && caught);
    CHECK(stack.length == 0); /* the handler is gone */
    return 0;
}

static int test_exception_no_throw(int seed, int rep) {
    pf_exception_stack_t stack;
    pf_exception_t *e;

    pf_init_exception_stack(&stack, 4, 0, NULL);
    e = pf_try_catch(&stack, 0, 0);
    CHECK(e == NULL);
    CHECK(stack.length == 1);
    pf_try_end(&stack);
    CHECK(stack.length == 0);
    pf_try_end(&stack); /* nothing left to end */
    pf_try_end(NULL);
    CHECK(stack.length == 0);
    return 0;
}

static int throw_from_depth(pf_exception_stack_t *stack, int depth) {
    if (depth == 0)
        return pf_throw(stack, 9, "deep");
    return throw_from_depth(stack, depth - 1);
}

/* The exception unwinds through several calls. */
static int test_exception_unwind(int seed, int rep) {
    pf_exception_stack_t stack;
    volatile int result = 0;

    pf_init_exception_stack(&stack, 4, 0, NULL);

    if (pf_try_catch(&stack, 0, 0) == NULL) {
        throw_from_depth(&stack, 10);
        result = -1;
    } else {
        result = stack.current.code;
    }
    CHECK(result == 9);
    return 0;
}

/* Handlers can be limited to a module or to a code. The innermost handler
   that matches gets the exception. */
static int test_exception_masks(int seed, int rep) {
    pf_exception_stack_t stack;
    volatile int which = 0;

    pf_init_exception_stack(&stack, 4, 0, NULL);

    if (pf_try_catch(&stack, 0, 0) == NULL) { /* catches everything */
        if (pf_try_catch(&stack, 0, 5) == NULL) { /* only code 5 */
            if (pf_try_catch(&stack, 99, 0) == NULL) { /* only module 99 */
                pf_throw(&stack, 5, "five"); /* module 7 by default */
                which = -1;
            } else {
                which = 3;
            }
        } else {
            which = 2;
        }
    } else {
        which = 1;
    }
    CHECK(which == 2); /* the module 99 handler is skipped */
    CHECK(stack.length == 1); /* the outer handler is still registered */
    pf_try_end(&stack);

    which = 0;
    if (pf_try_catch(&stack, 0, 0) == NULL) {
        if (pf_try_catch(&stack, 0, 5) == NULL) {
            pf_throw(&stack, 6, "six");
            which = -1;
        } else {
            which = 2;
        }
    } else {
        which = 1;
    }
    CHECK(which == 1); /* the code 5 handler is skipped */
    CHECK(stack.length == 0);
    return 0;
}

static int test_exception_rethrow(int seed, int rep) {
    pf_exception_stack_t stack;
    volatile int stage = 0;

    pf_init_exception_stack(&stack, 4, 0, NULL);

    if (pf_try_catch(&stack, 0, 0) == NULL) {
        pf_exception_t *inner = pf_try_catch(&stack, 0, 0);
        if (inner == NULL) {
            pf_throw(&stack, 4, "first");
        } else {
            stage = 1;
            pf_rethrow(&stack, inner); /* to the outer handler */
            stage = -1;
        }
    } else {
        CHECK_STR(stack.current.msg, "first");
        stage += 10;
    }
    CHECK(stage == 11);
    return 0;
}

pf_test_t suite_exception[] = {
    { test_exception_init, "/exception/init" },
    { test_exception_throw_unhandled, "/exception/throw_unhandled" },
    { test_exception_current, "/exception/current" },
    { test_exception_rethrow_args, "/exception/rethrow_args" },
    { test_exception_capacity, "/exception/capacity" },
    { test_exception_formatted, "/exception/formatted" },
    { test_exception_formatted_truncation,
      "/exception/formatted_truncation" },
    { test_exception_catch, "/exception/catch" },
    { test_exception_no_throw, "/exception/no_throw" },
    { test_exception_unwind, "/exception/unwind" },
    { test_exception_masks, "/exception/masks" },
    { test_exception_rethrow, "/exception/rethrow" },
    { 0 },
};
