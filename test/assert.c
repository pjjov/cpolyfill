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
#include <stdlib.h>
#include <string.h>

/* Replace abort and the output stream, so failures can be observed. */
static int g_aborts = 0;
static FILE *g_out = NULL;
static void test_abort(void) { g_aborts++; }

#undef NDEBUG
#define PF_ASSERT_ABORT test_abort
#define PF_ASSERT_STDERR g_out
#include "../include/pf_assert.h"
#include "../include/pf_test.h"

/* The usual `CHECK` would abort or be recorded here, so use plain returns. */
#define REQUIRE(...)                                                      \
    do {                                                                  \
        if (!(__VA_ARGS__)) {                                             \
            fprintf(                                                      \
                stderr, "%s:%d: `%s`\n", __FILE__, __LINE__, #__VA_ARGS__ \
            );                                                            \
            return 1;                                                     \
        }                                                                 \
    } while (0)

/* Starts capturing, and returns what was printed on `stop`. */
static int start(void) {
    g_aborts = 0;
    g_out = tmpfile();
    return g_out != NULL;
}

static void stop(char *buf, size_t size) {
    size_t n;
    rewind(g_out);
    n = fread(buf, 1, size - 1, g_out);
    buf[n] = '\0';
    fclose(g_out);
    g_out = NULL;
}

static int test_assert_pass(int seed, int rep) {
    char out[256];
    REQUIRE(start());

    pf_assert(1 == 1);
    pf_assert(((int[]) { 1, 2 })[1] == 2); /* commas are fine */
    pf_assert_true(2 > 1);
    pf_assert_false(2 < 1);
    pf_assert_ok(0);
    pf_assert_error(5);
    pf_assert_null((void *)0);
    pf_assert_not_null("x");
    pf_assert_always("never printed", 1);

    stop(out, sizeof out);
    REQUIRE(g_aborts == 0);
    REQUIRE(out[0] == '\0');
    return 0;
}

static int test_assert_fail(int seed, int rep) {
    char out[256];
    REQUIRE(start());

    pf_assert(1 == 2);
    REQUIRE(g_aborts == 1);
    pf_assert_true(0);
    REQUIRE(g_aborts == 2);
    pf_assert_false(1);
    REQUIRE(g_aborts == 3);
    pf_assert_ok(1);
    REQUIRE(g_aborts == 4);
    pf_assert_error(0);
    REQUIRE(g_aborts == 5);
    pf_assert_null("x");
    REQUIRE(g_aborts == 6);
    pf_assert_not_null((void *)0);
    REQUIRE(g_aborts == 7);
    pf_assert_always("custom", 0);
    REQUIRE(g_aborts == 8);

    stop(out, sizeof out);
    return 0;
}

static int test_assert_message(int seed, int rep) {
    char out[256];
    int line;
    REQUIRE(start());

    line = __LINE__ + 1;
    pf_assert(1 + 1 == 3);

    stop(out, sizeof out);
    REQUIRE(g_aborts == 1);

    /* The expression is printed as written, not expanded. */
    REQUIRE(strstr(out, "1 + 1 == 3") != NULL);
    REQUIRE(strstr(out, "assert.c") != NULL);
    {
        char want[32];
        snprintf(want, sizeof want, ":%d:", line);
        REQUIRE(strstr(out, want) != NULL);
    }
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    REQUIRE(strstr(out, "test_assert_message") != NULL);
#endif
    REQUIRE(out[strlen(out) - 1] == '\n');
    return 0;
}

#define TWO 2
static int test_assert_no_expansion(int seed, int rep) {
    char out[256];
    REQUIRE(start());
    pf_assert(TWO == 3);
    stop(out, sizeof out);
    REQUIRE(strstr(out, "TWO == 3") != NULL);
    return 0;
}

static int test_assert_check(int seed, int rep) {
    char out[256];
    REQUIRE(start());

    REQUIRE(pf_check(1 == 1) == 1);
    REQUIRE(pf_check_true(1) == 1);
    REQUIRE(pf_check_false(0) == 1);
    REQUIRE(pf_check_ok(0) == 1);
    REQUIRE(pf_check_error(3) == 1);
    REQUIRE(pf_check_null((void *)0) == 1);
    REQUIRE(pf_check_not_null("x") == 1);
    REQUIRE(g_aborts == 0);

    /* Failing checks report, but carry on and evaluate to 0. */
    REQUIRE(pf_check(1 == 2) == 0);
    REQUIRE(g_aborts == 1);
    REQUIRE(pf_check_true(0) == 0);
    REQUIRE(pf_check_false(1) == 0);
    REQUIRE(pf_check_ok(1) == 0);
    REQUIRE(pf_check_error(0) == 0);
    REQUIRE(pf_check_null("x") == 0);
    REQUIRE(pf_check_not_null((void *)0) == 0);
    REQUIRE(g_aborts == 7);

    REQUIRE(pf_check_always("m", 1) == 1);
    REQUIRE(pf_check_always("m", 0) == 0);
    REQUIRE(g_aborts == 8);

    stop(out, sizeof out);
    return 0;
}

static int test_assert_check_in_condition(int seed, int rep) {
    char out[256];
    int handled = 0;
    REQUIRE(start());

    if (!pf_check(seed == seed + 1))
        handled = 1;

    stop(out, sizeof out);
    REQUIRE(handled);
    return 0;
}

static int test_assert_memory_strings(int seed, int rep) {
    char out[256];
    REQUIRE(start());

    pf_assert_memcmp("abc", "abd", 2);
    pf_assert_not_memcmp("abc", "abd", 3);
    pf_assert_strcmp("abc", "abc");
    pf_assert_not_strcmp("abc", "abd");
    (void)pf_check_memcmp("abc", "abd", 2);
    (void)pf_check_not_memcmp("abc", "abd", 3);
    (void)pf_check_strcmp("abc", "abc");
    (void)pf_check_not_strcmp("abc", "abd");
    REQUIRE(g_aborts == 0);

    pf_assert_memcmp("abc", "abd", 3);
    pf_assert_not_memcmp("abc", "abd", 2);
    pf_assert_strcmp("abc", "abd");
    pf_assert_not_strcmp("abc", "abc");
    REQUIRE(g_aborts == 4);

    (void)pf_check_memcmp("abc", "abd", 3);
    (void)pf_check_not_memcmp("abc", "abd", 2);
    (void)pf_check_strcmp("abc", "abd");
    (void)pf_check_not_strcmp("abc", "abc");
    REQUIRE(g_aborts == 8);

    stop(out, sizeof out);
    return 0;
}

static int test_assert_bool(int seed, int rep) {
    pf_bool t = PF_TRUE, f = PF_FALSE;
    REQUIRE(t && !f);
    REQUIRE(PF_TRUE == 1 && PF_FALSE == 0);
    /* The type must hold nothing but 0 or 1. */
    t = 5;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    REQUIRE(t == 1);
#endif
    return 0;
}

static int test_assert_static(int seed, int rep) {
    static_assert(sizeof(char) == 1, "char is one byte");
    REQUIRE(1);
    return 0;
}

pf_test_t suite_assert[] = {
    { test_assert_pass, "/assert/pass" },
    { test_assert_fail, "/assert/fail" },
    { test_assert_message, "/assert/message" },
    { test_assert_no_expansion, "/assert/no_expansion" },
    { test_assert_check, "/assert/check" },
    { test_assert_check_in_condition, "/assert/check_in_condition" },
    { test_assert_memory_strings, "/assert/memory_strings" },
    { test_assert_bool, "/assert/bool" },
    { test_assert_static, "/assert/static" },
    { 0 },
};
