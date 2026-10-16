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

/* Helpers shared by the test suites. */

#ifndef PF_TEST_CHECK_H
#define PF_TEST_CHECK_H

#include "../include/pf_assert.h"
#include "../include/pf_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unlike `pf_assert`, this check is never compiled out by NDEBUG, so a
   release build of the tests cannot silently pass. */
#define CHECK(...)                                                    \
    pf_assert_always("Check `" #__VA_ARGS__ "` failed!", __VA_ARGS__)

#define CHECK_STR(a, b)                                                \
    pf_assert_always(                                                  \
        "Strings `" #a "` and `" #b "` differ!", 0 == strcmp((a), (b)) \
    )

#define CHECK_MEM(a, b, n)                                                 \
    pf_assert_always(                                                      \
        "Memory `" #a "` and `" #b "` differ!", 0 == memcmp((a), (b), (n)) \
    )

/* Tests that exercise a confirmed defect of a header are registered with
   `KNOWN_BUG(fn)`. They are reported as TODO until the build is configured
   with `-DPF_TEST_KNOWN_BUGS=1` (meson: `-Dknown_bugs=true`). */
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

#endif /* PF_TEST_CHECK_H */
