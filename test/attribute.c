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

#include "../include/pf_attribute.h"
#include "check.h"
#include <stdlib.h>

#define STR_(x) #x
#define STR(x) STR_(x)

/* These only have to compile; most compilers ignore unknown attributes. */

/* Never called, as that would warn. Not static, to avoid unused warnings. */
PF_DEPRECATED("do not use")
int pf_test_old_function(void);
int pf_test_old_function(void) { return 1; }

PF_NORETURN static void never_returns(int code) { exit(code); }

PF_NODISCARD("check the result")
static int important(void) { return 7; }

/* C23 attributes appertain to the declarator, so they come after it. */
static int reproducible(const int *p) PF_REPRODUCIBLE;
static int reproducible(const int *p) { return *p; }

static int unsequenced(int a) PF_UNSEQUENCED;
static int unsequenced(int a) { return a + 1; }

static int fall(int x) {
    int r = 0;
    switch (x) {
    case 1:
        r += 1;
        PF_FALLTHROUGH;
    case 2:
        r += 2;
        break;
    default:
        break;
    }
    return r;
}

static int test_attribute_compiles(int seed, int rep) {
    int value = 3;

    CHECK(important() == 7);
    CHECK(reproducible(&value) == 3);
    CHECK(unsequenced(1) == 2);
    CHECK(fall(1) == 3);
    CHECK(fall(2) == 2);
    CHECK(fall(3) == 0);

    if (seed == 0x7fffffff && rep == 0x7fffffff) {
        never_returns(1);
    }
    return 0;
}

static int test_attribute_pf_has_attribute(int seed, int rep) {
    /* Always usable as a condition, even without compiler support. */
    int a = pf_has_attribute(noreturn) ? 1 : 0;
    int b = pf_has_attribute(this_attribute_does_not_exist_xyz) ? 1 : 0;

    CHECK(a == 0 || a == 1);
    CHECK(b == 0);
    return 0;
}

/* With a compiler that has the attribute, the macro must not expand to
   nothing: otherwise it silently annotates nothing at all. */
static int test_attribute_expands(int seed, int rep) {
#if defined(__GNUC__) || defined(__clang__)
    const char *noreturn = STR(PF_NORETURN);
    const char *deprecated = STR(PF_DEPRECATED("x"));
    const char *fallthrough = STR(PF_FALLTHROUGH);

    CHECK(noreturn[0] != '\0');
    CHECK(deprecated[0] != '\0');
    CHECK(fallthrough[0] != '\0');
    return 0;
#else
    return SKIP("no GNU attributes");
#endif
}

pf_test_t suite_attribute[] = {
    { test_attribute_compiles, "/attribute/compiles" },
    { test_attribute_pf_has_attribute, "/attribute/has_attribute" },
    { test_attribute_expands, "/attribute/expands" },
    { 0 },
};
