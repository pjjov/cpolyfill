/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/errno.h>

#include "check.h"

#include <errno.h>

#define CODE(name, value, desc) PF_##name,
static const int codes[] = { PF__ERROR_LIST(CODE) };
#undef CODE

#define VALUE(name, value, desc) value,
static const int values[] = { PF__ERROR_LIST(VALUE) };
#undef VALUE

#define COUNT (sizeof(codes) / sizeof(codes[0]))

static int test_values(int seed, int i) {
    size_t a, b;
    (void)seed, (void)i;

    CHECK_INT(PF_OK, 0);
    for (a = 0; a < COUNT; a++) {
        CHECK(codes[a] < 0);
        CHECK_INT(codes[a], -values[a]);
        for (b = a + 1; b < COUNT; b++)
            CHECK(codes[a] != codes[b]);
    }
    CHECK_INT(PF_EWOULDBLOCK, PF_EAGAIN);
    CHECK_INT(PF_ENOTSUP, PF_EOPNOTSUPP);
    CHECK_INT(PF_EDEADLOCK, PF_EDEADLK);
    return 0;
}

static int test_roundtrip(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < COUNT; k++) {
        int sys = pf_error_to_errno(codes[k]);
        if (sys == EINVAL && codes[k] != PF_EINVAL)
            continue; /* the system has no such errno value */
        CHECK_INT(pf_error_from_errno(sys), codes[k]);
    }
    return 0;
}

static int test_common(int seed, int i) {
    (void)seed, (void)i;

    CHECK_INT(pf_error_from_errno(0), PF_OK);
    CHECK_INT(pf_error_to_errno(PF_OK), 0);
    CHECK_INT(pf_error_from_errno(ENOENT), PF_ENOENT);
    CHECK_INT(pf_error_from_errno(EINVAL), PF_EINVAL);
    CHECK_INT(pf_error_from_errno(ENOMEM), PF_ENOMEM);
    CHECK_INT(pf_error_from_errno(EAGAIN), PF_EAGAIN);
    CHECK_INT(pf_error_from_errno(EWOULDBLOCK), PF_EAGAIN);
    CHECK_INT(pf_error_from_errno(ENOTSUP), PF_ENOTSUP);
    CHECK_INT(pf_error_from_errno(EILSEQ), PF_EILSEQ);
    CHECK_INT(pf_error_to_errno(PF_ENOENT), ENOENT);
    CHECK_INT(pf_error_to_errno(PF_ERANGE), ERANGE);

    CHECK_INT(pf_error_from_errno(-12345), PF_EUNKNOWN);
    CHECK_INT(pf_error_from_errno(1 << 20), PF_EUNKNOWN);
    CHECK_INT(pf_error_to_errno(12345), EINVAL);
    CHECK_INT(pf_error_to_errno(-123456), EINVAL);
    return 0;
}

/* The codes are the negated Linux values. */
static int test_linux(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

#ifndef __linux__
    return SKIP("not Linux");
#else
    for (k = 0; k < COUNT; k++) {
        if (codes[k] == PF_EUNKNOWN)
            continue;
        CHECK_INT(pf_error_from_errno(-codes[k]), codes[k]);
        CHECK_INT(pf_error_to_errno(codes[k]), -codes[k]);
    }
    return 0;
#endif
}

pf_test_t suite_errno[] = {
    { test_values, "values" },
    { test_roundtrip, "roundtrip" },
    { test_common, "common" },
    { test_linux, "linux" },
    { 0 },
};
