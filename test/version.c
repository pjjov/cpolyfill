/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/version.h>

#include "check.h"

static int test_linked(int seed, int i) {
    char expected[32];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_version(), PF_VERSION_NUMBER);
    PF_ASSERT_ALWAYS_STR_EQ(pf_version_string(), PF_VERSION_STRING);

    snprintf(
        expected,
        sizeof(expected),
        "%d.%d.%d",
        PF_VERSION_MAJOR,
        PF_VERSION_MINOR,
        PF_VERSION_PATCH
    );
    PF_ASSERT_ALWAYS_STR_EQ(pf_version_string(), expected);
    return 0;
}

static int test_prereq(int seed, int i) {
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(PF_VERSION_MAJOR == 2);
    PF_ASSERT_ALWAYS(PF_VERSION_PREREQ(2, 0));
    PF_ASSERT_ALWAYS(PF_VERSION_PREREQ(PF_VERSION_MAJOR, PF_VERSION_MINOR));
    PF_ASSERT_ALWAYS(
        !PF_VERSION_PREREQ(PF_VERSION_MAJOR, PF_VERSION_MINOR + 1)
    );
    PF_ASSERT_ALWAYS(!PF_VERSION_PREREQ(PF_VERSION_MAJOR + 1, 0));
    return 0;
}

pf_test_t suite_version[] = {
    { test_linked, "linked" },
    { test_prereq, "prereq" },
    { 0 },
};
