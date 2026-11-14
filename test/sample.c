/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Test runner with tests whose outcome is known, run by the `test` suite
   (through PF_TEST_SAMPLE) to check <cpolyfill/test.h> from outside. */

#include <cpolyfill/assert.h>
#include <cpolyfill/test.h>
#include <cpolyfill/time.h>

#include <stdio.h>
#include <stdlib.h>

static int t_pass(int seed, int i) {
    (void)seed, (void)i;
    return PF_TEST_PASS;
}

static int t_fail(int seed, int i) {
    (void)seed, (void)i;
    return 3;
}

static int t_assert(int seed, int i) {
    int a = 1;
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(a, 2);
    return PF_TEST_PASS;
}

static int t_crash(int seed, int i) {
    (void)seed, (void)i;
    abort();
}

static int t_skip(int seed, int i) {
    (void)seed, (void)i;
    return pf_test_skip("no %s", "network");
}

static int t_slow(int seed, int i) {
    (void)seed, (void)i;
    pf_sleep(10);
    return PF_TEST_PASS;
}

/* Prints its arguments, fails at the third repetition. */
static int t_seed(int seed, int i) {
    printf("seed=%d i=%d\n", seed, i);
    return i == 2 ? PF_TEST_FAIL : PF_TEST_PASS;
}

static const pf_test_t sample[] = {
    { t_pass, "pass" },
    { t_fail, "fail" },
    { t_assert, "assert" },
    { t_crash, "crash" },
    { t_skip, "skip" },
    { NULL, "todo" },
    { t_slow, "slow" },
    { t_seed, "seed" },
    { 0 },
};

static const pf_test_t other[] = {
    { t_pass, "a" },
    { t_pass, "b" },
    { 0 },
};

static const pf_suite_t suites[] = {
    { "sample", 1, sample },
    { "other", 2, other },
    { "later", 0, other },
    { 0 },
};

int main(int argc, char **argv) { return pf_test_main(argc, argv, suites); }
