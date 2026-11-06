/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Helpers shared by the test suites. */

#ifndef PF_TEST_CHECK_H
#define PF_TEST_CHECK_H

#include "runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Aborts on failure, so a forked test reports it. Never compiled out.
   Moves to <cpolyfill/assert.h> once that module is ported. */
#define CHECK(...)                                                        \
    do {                                                                  \
        if (!(__VA_ARGS__)) {                                             \
            fprintf(                                                      \
                stderr, "%s:%d: Check `%s` failed!\n", __FILE__, __LINE__, \
                #__VA_ARGS__                                              \
            );                                                            \
            abort();                                                      \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b, fmt)                                               \
    do {                                                                  \
        long long check_a_ = (long long)(a), check_b_ = (long long)(b);   \
        if (check_a_ != check_b_) {                                       \
            fprintf(                                                      \
                stderr, "%s:%d: `%s` == `%s` failed: " fmt " != " fmt "\n", \
                __FILE__, __LINE__, #a, #b, check_a_, check_b_            \
            );                                                            \
            abort();                                                      \
        }                                                                 \
    } while (0)

#define CHECK_INT(a, b) CHECK_EQ(a, b, "%lld")

#define CHECK_STR(a, b) CHECK(0 == strcmp((a), (b)))
#define CHECK_MEM(a, b, n) CHECK(0 == memcmp((a), (b), (n)))

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
