/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Helpers shared by the test suites. */

#ifndef PF_TEST_CHECK_H
#define PF_TEST_CHECK_H

#include "runner.h"

/* The tests check with PF_ASSERT_ALWAYS*: never compiled out, and the
   default handler aborts, so a forked test reports the failure. */
#include <cpolyfill/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tests that exercise a confirmed defect are registered with
   `KNOWN_BUG(fn)`. They are reported as TODO until the build is configured
   with `-Dknown_bugs=true`. */
#ifndef PF_TEST_KNOWN_BUGS
    #define PF_TEST_KNOWN_BUGS 0
#endif
#define KNOWN_BUG(fn) (PF_TEST_KNOWN_BUGS ? (fn) : (pf_test_fn)0)

/* Marks a test that cannot run on this platform. It passes, but says why
   when the PF_TEST_VERBOSE environment variable is set. */
#define SKIP(why) pf__test_skip(__func__, (why))

static inline int pf__test_skip(const char *name, const char *why) {
    if (getenv("PF_TEST_VERBOSE"))
        fprintf(stderr, "# skip %s: %s\n", name, why);
    return 0;
}

/* A small deterministic PRNG for tests that draw on the seed. */
static inline unsigned test_rand(unsigned *state) {
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7FFF;
}

#endif /* PF_TEST_CHECK_H */
