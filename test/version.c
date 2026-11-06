/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/version.h>

#include "check.h"

static int test_linked(int seed, int i) {
    char expected[32];
    (void)seed, (void)i;

    CHECK_INT(pf_version(), PF_VERSION_NUMBER);
    CHECK_STR(pf_version_string(), PF_VERSION_STRING);

    snprintf(
        expected,
        sizeof(expected),
        "%d.%d.%d",
        PF_VERSION_MAJOR,
        PF_VERSION_MINOR,
        PF_VERSION_PATCH
    );
    CHECK_STR(pf_version_string(), expected);
    return 0;
}

static int test_prereq(int seed, int i) {
    (void)seed, (void)i;

    CHECK(PF_VERSION_MAJOR == 2);
    CHECK(PF_VERSION_PREREQ(2, 0));
    CHECK(PF_VERSION_PREREQ(PF_VERSION_MAJOR, PF_VERSION_MINOR));
    CHECK(!PF_VERSION_PREREQ(PF_VERSION_MAJOR, PF_VERSION_MINOR + 1));
    CHECK(!PF_VERSION_PREREQ(PF_VERSION_MAJOR + 1, 0));
    return 0;
}

pf_test_t suite_version[] = {
    { test_linked, "linked" },
    { test_prereq, "prereq" },
    { 0 },
};
