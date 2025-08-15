/*
    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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

#include "../include/pf_assert.h"
#include "../include/pf_macro.h"
#include "../include/pf_test.h"
#include <limits.h>
#include <stdint.h>

int test_macro_math(int seed, int rep) {
    pf_assert(PF_ABS(0) == 0);
    pf_assert(PF_ABS(1) == 1);
    pf_assert(PF_ABS(-1) == 1);
    pf_assert(PF_ABS(INT_MAX) == INT_MAX);
    pf_assert(PF_ABS(INT_MIN) == INT_MIN);
    pf_assert(PF_MIN(0, 1) == 0);
    pf_assert(PF_MIN(1, 1) == 1);
    pf_assert(PF_MIN(INT_MIN, LLONG_MIN) == LLONG_MIN);
    pf_assert(PF_MAX(0, 1) == 1);
    pf_assert(PF_MAX(1, 1) == 1);
    pf_assert(PF_MAX(INT_MAX, LLONG_MAX) == LLONG_MAX);

    return 0;
}

int test_macro_countof(int seed, int rep) {
    int i[1], j[2], k[3], *l[3];

    pf_assert(PF_COUNTOF(i) == 1);
    pf_assert(PF_COUNTOF(j) == 2);
    pf_assert(PF_COUNTOF(k) == 3);
    pf_assert(PF_COUNTOF(l) == 3);

    return 0;
}

pf_test suite_macro[] = {
    { test_macro_math, "/macro/math", 1 },
    { test_macro_countof, "/macro/countof", 1 },
    { 0 },
};
