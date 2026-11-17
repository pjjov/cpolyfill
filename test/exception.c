/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#define PF_EXCEPTION_MODULE 7
#include <cpolyfill/exception.h>

#include <cpolyfill/assert.h>
#include <cpolyfill/errno.h>
#include <cpolyfill/threads.h>

#include <setjmp.h>
#include <string.h>

#include "check.h"

int exception_throw_noloc(pf_exception_stack_t *s, int code);

static int test_unhandled(int seed, int i) {
    pf_exception_stack_t s;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    PF_ASSERT_ALWAYS_EQ(PF_THROW(&s, 5, "boom"), 5);
    PF_ASSERT_ALWAYS_EQ(PF_THROW(&s, -3, "negative"), -3);
    PF_ASSERT_ALWAYS_EQ(PF_THROW(&s, 0, "not an error"), 0);
    PF_ASSERT_ALWAYS_EQ(PF_THROW(NULL, 9, "no stack"), 9);
    PF_ASSERT_ALWAYS_EQ(PF_THROWF(NULL, 9, "no stack %d", 1), 9);
    PF_ASSERT_ALWAYS_EQ(PF_THROWF(&s, 4, "no arguments"), 4);
    pf_exception_stack_init(NULL, NULL, 0); /* must not crash */
    return 0;
}

static int test_catch(int seed, int i) {
    pf_exception_stack_t s;
    volatile int reached = 0, caught = 0, after = 0, line = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    PF_TRY(&s, 0, 0) {
        line = __LINE__ + 1;
        PF_THROW(&s, 21, "caught");
        reached = 1;
    }
    PF_CATCH(e) {
        caught = 1;
        PF_ASSERT_ALWAYS_EQ(e->code, 21);
        PF_ASSERT_ALWAYS_EQ(e->module, 7);
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "caught");
        PF_ASSERT_ALWAYS(strstr(e->file, "exception.c") != NULL);
        PF_ASSERT_ALWAYS_EQ(e->line, line);
        PF_ASSERT_ALWAYS_STR_EQ(e->func, "test_catch");
    }
    PF_TRY_END;
    after = 1;
    PF_ASSERT_ALWAYS(!reached && caught && after);
    /* The try block is gone: nothing catches now. */
    PF_ASSERT_ALWAYS_EQ(PF_THROW(&s, 3, "uncaught"), 3);
    return 0;
}

static int test_no_throw(int seed, int i) {
    pf_exception_stack_t s;
    volatile int body = 0, caught = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    PF_TRY(&s, 0, 0) { body = 1; }
    PF_CATCH(e) { caught = 1; }
    PF_TRY_END;
    PF_ASSERT_ALWAYS(body && !caught);
    PF_ASSERT_ALWAYS_EQ(PF_THROW(&s, 3, "uncaught"), 3);

    /* Without a stack the body runs and nothing is caught. */
    body = 0;
    PF_TRY(NULL, 0, 0) { body = 1; }
    PF_CATCH(e) { caught = 1; }
    PF_TRY_END;
    PF_ASSERT_ALWAYS(body && !caught);

    /* A statement, usable as the body of an if. */
    if (body)
        PF_TRY(&s, 0, 0) { body = 2; }
    PF_CATCH(e) { caught = 1; }
    PF_TRY_END;
    else body = 3;
    PF_ASSERT_ALWAYS_EQ(body, 2);
    return 0;
}

static int throw_from_depth(pf_exception_stack_t *s, int depth) {
    if (depth == 0)
        return PF_THROW(s, 9, "deep");
    return throw_from_depth(s, depth - 1) + 1;
}

/* Through several calls, and deeper than v1's 16 handlers. */
static int nest(pf_exception_stack_t *s, int depth, volatile int *caught) {
    if (depth == 0)
        return throw_from_depth(s, 10);
    PF_TRY(s, 0, depth == 20 ? 9 : 1000 + depth) { nest(s, depth - 1, caught); }
    PF_CATCH(e) {
        *caught = depth;
        PF_ASSERT_ALWAYS_EQ(e->code, 9);
    }
    PF_TRY_END;
    return 0;
}

static int test_unwind(int seed, int i) {
    pf_exception_stack_t s;
    volatile int caught = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    nest(&s, 40, &caught);
    PF_ASSERT_ALWAYS_EQ(caught, 20);
    /* Every try block is gone. */
    PF_ASSERT_ALWAYS_EQ(PF_THROW(&s, 9, "again"), 9);
    return 0;
}

/* The innermost try block that matches gets the exception. */
static int test_filters(int seed, int i) {
    pf_exception_stack_t s;
    volatile int which = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    PF_TRY(&s, 0, 0) {
        PF_TRY(&s, 0, 5) {
            PF_TRY(&s, 99, 0) {
                PF_THROW(&s, 5, "five"); /* module 7 */
                which = -1;
            }
            PF_CATCH(e) { which = 3; }
            PF_TRY_END;
        }
        PF_CATCH(e) { which = 2; }
        PF_TRY_END;
        /* The outer block is still there. */
        PF_THROW(&s, 6, "six");
        which = -2;
    }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_EQ(which, 2);
        PF_ASSERT_ALWAYS_EQ(e->code, 6);
        which = 1;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(which, 1);

    /* Module filters, and an exception of another module. */
    which = 0;
    PF_TRY(&s, 7, 0) {
        PF_TRY(&s, 9, 0) { PF_THROW(&s, 1, "seven"); }
        PF_CATCH(e) { which = 9; }
        PF_TRY_END;
    }
    PF_CATCH(e) { which = 7; }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(which, 7);
    PF_TRY(&s, 7, 0) {
        PF_TRY(&s, 9, 0) { exception_throw_noloc(&s, 1); }
        PF_CATCH(e) { which = 9; }
        PF_TRY_END;
    }
    PF_CATCH(e) { which = 7; }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(which, 9);
    return 0;
}

static int test_messages(int seed, int i) {
    pf_exception_stack_t s;
    char buf[16];
    char text[32];
    volatile int caught = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, buf, sizeof(buf));
    PF_TRY(&s, 0, 0) { PF_THROWF(&s, 3, "value %d of %s", 42, "x"); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "value 42 of x");
        PF_ASSERT_ALWAYS(e->msg == buf);
        caught++;
    }
    PF_TRY_END;

    /* Copied, so the original may go away; cut to fit. */
    strcpy(text, "0123456789abcdefghij");
    PF_TRY(&s, 0, 0) { PF_THROW(&s, 3, text); }
    PF_CATCH(e) {
        memset(text, 0, sizeof(text));
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "0123456789abcde");
        caught++;
    }
    PF_TRY_END;

    /* Not in the middle of a character. */
    PF_TRY(&s, 0, 0) { PF_THROWF(&s, 3, "%s", "0123456789abcd\xc3\xa9"); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "0123456789abcd");
        caught++;
    }
    PF_TRY_END;

    /* With the previous message as an argument. */
    PF_TRY(&s, 0, 0) {
        PF_TRY(&s, 0, 0) { PF_THROW(&s, 3, "inner"); }
        PF_CATCH(e) { PF_THROWF(&s, 4, "outer: %s", e->msg); }
        PF_TRY_END;
    }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "outer: inner");
        caught++;
    }
    PF_TRY_END;

    /* Without a buffer the pointers are kept; NULL messages too. */
    pf_exception_stack_init(&s, NULL, 0);
    PF_TRY(&s, 0, 0) { PF_THROWF(&s, 3, "fmt %d", 1); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "fmt %d");
        caught++;
    }
    PF_TRY_END;
    pf_exception_stack_init(&s, buf, sizeof(buf));
    PF_TRY(&s, 0, 0) { PF_THROW(&s, 3, NULL); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_NULL(e->msg);
        caught++;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(caught, 6);
    return 0;
}

static int test_location(int seed, int i) {
    pf_exception_stack_t s;
    volatile int caught = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    PF_TRY(&s, 0, 0) { exception_throw_noloc(&s, 2); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_EQ(e->module, 9);
        PF_ASSERT_ALWAYS_NULL(e->file);
        PF_ASSERT_ALWAYS_EQ(e->line, 0);
        PF_ASSERT_ALWAYS_NULL(e->func);
        caught = 1;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS(caught);

    /* What the macros call, with the caller's own site. */
    PF_TRY(&s, 0, 0) { pf_throw_at(&s, 3, 4, "f.c", 12, "fn", "msg"); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_EQ(e->module, 3);
        PF_ASSERT_ALWAYS_EQ(e->code, 4);
        PF_ASSERT_ALWAYS_STR_EQ(e->file, "f.c");
        PF_ASSERT_ALWAYS_EQ(e->line, 12);
        PF_ASSERT_ALWAYS_STR_EQ(e->func, "fn");
        caught = 2;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(caught, 2);
    return 0;
}

static int test_rethrow(int seed, int i) {
    pf_exception_stack_t s, other;
    pf_exception_t e2;
    volatile int stage = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    PF_ASSERT_ALWAYS_EQ(pf_rethrow(&s, NULL), PF_EINVAL);
    memset(&e2, 0, sizeof(e2));
    e2.code = 4;
    PF_ASSERT_ALWAYS_EQ(pf_rethrow(NULL, &e2), 4);
    PF_ASSERT_ALWAYS_EQ(pf_rethrow(&s, &e2), 4);

    PF_TRY(&s, 0, 0) {
        PF_TRY(&s, 0, 0) { PF_THROW(&s, 4, "first"); }
        PF_CATCH(e) {
            stage = 1;
            pf_rethrow(&s, e);
            stage = -1;
        }
        PF_TRY_END;
    }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "first");
        PF_ASSERT_ALWAYS_EQ(e->code, 4);
        stage += 10;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(stage, 11);

    /* An exception of another stack. */
    pf_exception_stack_init(&other, NULL, 0);
    PF_TRY(&other, 0, 0) { PF_THROW(&other, 8, "elsewhere"); }
    PF_CATCH(e) { e2 = *e; }
    PF_TRY_END;
    stage = 0;
    PF_TRY(&s, 0, 8) { pf_rethrow(&s, &e2); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "elsewhere");
        PF_ASSERT_ALWAYS_EQ(e->module, 7);
        stage = 1;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS_EQ(stage, 1);
    return 0;
}

/* Leaving a try block with return is noticed by the enclosing one. */
static jmp_buf env;

static void jump_back(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    (void)expr, (void)file, (void)line, (void)func, (void)message;
    longjmp(env, 1);
}

static int leave_early(pf_exception_stack_t *s) {
    PF_TRY(s, 0, 0) { return 1; }
    PF_CATCH(e) { }
    PF_TRY_END;
    return 0;
}

static int test_left_early(int seed, int i) {
    pf_exception_stack_t s;
    pf_assert_handler_fn prev;
    volatile int noticed = 0;
    (void)seed, (void)i;

    pf_exception_stack_init(&s, NULL, 0);
    prev = pf_set_assert_handler(jump_back);
    if (setjmp(env) == 0) {
        PF_TRY(&s, 0, 0) { leave_early(&s); }
        PF_CATCH(e) { }
        PF_TRY_END;
    } else {
        noticed = 1;
    }
    pf_set_assert_handler(prev);
    PF_ASSERT_ALWAYS(noticed);
    return 0;
}

static int thread_stack(void *arg) {
    pf_exception_stack_t *s = pf_exception_thread_stack();
    volatile int caught = 0;

    *(pf_exception_stack_t **)arg = s;
    PF_ASSERT_ALWAYS(s == pf_exception_thread_stack());
    PF_TRY(s, 0, 0) { PF_THROWF(s, 2, "in thread %d", 2); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS_STR_EQ(e->msg, "in thread 2");
        caught = 1;
    }
    PF_TRY_END;
    return caught;
}

static int test_thread_stack(int seed, int i) {
    pf_exception_stack_t *s = pf_exception_thread_stack(), *other = NULL;
    volatile int caught = 0;
    char big[400];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_NOT_NULL(s);
    PF_ASSERT_ALWAYS(s == pf_exception_thread_stack());

    /* Messages are kept, cut at 255 bytes. */
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    PF_TRY(s, 0, 0) { PF_THROW(s, 1, big); }
    PF_CATCH(e) {
        PF_ASSERT_ALWAYS(e->msg != big);
        PF_ASSERT_ALWAYS_EQ(strlen(e->msg), 255);
        caught = 1;
    }
    PF_TRY_END;
    PF_ASSERT_ALWAYS(caught);

#if PF_CONFIG_THREADS
    {
        pf_thrd_t t;
        int res = 0;
        PF_TRY(s, 0, 0) {
            PF_ASSERT_ALWAYS_EQ(
                pf_thrd_create(&t, thread_stack, &other), pf_thrd_success
            );
            PF_ASSERT_ALWAYS_EQ(pf_thrd_join(t, &res), pf_thrd_success);
        }
        PF_CATCH(e) { caught = 2; }
        PF_TRY_END;
        PF_ASSERT_ALWAYS_EQ(res, 1);
        PF_ASSERT_ALWAYS_EQ(caught, 1);
        PF_ASSERT_ALWAYS(other != NULL && other != s);
    }
#else
    (void)thread_stack, (void)other;
#endif
    return 0;
}

pf_test_t suite_exception[] = {
    { test_unhandled, "unhandled" },
    { test_catch, "catch" },
    { test_no_throw, "no_throw" },
    { test_unwind, "unwind" },
    { test_filters, "filters" },
    { test_messages, "messages" },
    { test_location, "location" },
    { test_rethrow, "rethrow" },
    { test_left_early, "left_early" },
    { test_thread_stack, "thread_stack" },
    { 0 },
};
