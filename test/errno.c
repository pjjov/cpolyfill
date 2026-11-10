/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/errno.h>

#include "check.h"

#include <errno.h>
#include <string.h>

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

    PF_ASSERT_ALWAYS_EQ(PF_OK, 0);
    for (a = 0; a < COUNT; a++) {
        PF_ASSERT_ALWAYS(codes[a] < 0);
        PF_ASSERT_ALWAYS_EQ(codes[a], -values[a]);
        for (b = a + 1; b < COUNT; b++)
            PF_ASSERT_ALWAYS(codes[a] != codes[b]);
    }
    PF_ASSERT_ALWAYS_EQ(PF_EWOULDBLOCK, PF_EAGAIN);
    PF_ASSERT_ALWAYS_EQ(PF_ENOTSUP, PF_EOPNOTSUPP);
    PF_ASSERT_ALWAYS_EQ(PF_EDEADLOCK, PF_EDEADLK);
    return 0;
}

static int test_roundtrip(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < COUNT; k++) {
        int sys = pf_error_to_errno(codes[k]);
        if (sys == EINVAL && codes[k] != PF_EINVAL)
            continue; /* the system has no such errno value */
        PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(sys), codes[k]);
    }
    return 0;
}

static int test_common(int seed, int i) {
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(0), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_error_to_errno(PF_OK), 0);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(ENOENT), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(EINVAL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(ENOMEM), PF_ENOMEM);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(EAGAIN), PF_EAGAIN);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(EWOULDBLOCK), PF_EAGAIN);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(ENOTSUP), PF_ENOTSUP);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(EILSEQ), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf_error_to_errno(PF_ENOENT), ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_error_to_errno(PF_ERANGE), ERANGE);

    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(-12345), PF_EUNKNOWN);
    PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(1 << 20), PF_EUNKNOWN);
    PF_ASSERT_ALWAYS_EQ(pf_error_to_errno(12345), EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_error_to_errno(-123456), EINVAL);
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
        PF_ASSERT_ALWAYS_EQ(pf_error_from_errno(-codes[k]), codes[k]);
        PF_ASSERT_ALWAYS_EQ(pf_error_to_errno(codes[k]), -codes[k]);
    }
    return 0;
#endif
}

#define DESC(name, value, desc) desc,
static const char *const descs[] = { PF__ERROR_LIST(DESC) };
#undef DESC

#define NAME(name, value, desc) #name,
static const char *const names[] = { PF__ERROR_LIST(NAME) };
#undef NAME

static int test_strings(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < COUNT; k++) {
        PF_ASSERT_ALWAYS_STR_EQ(pf_strerror(codes[k]), descs[k]);
        PF_ASSERT_ALWAYS_STR_EQ(pf_error_name(codes[k]), names[k]);
    }
    PF_ASSERT_ALWAYS_STR_EQ(pf_strerror(PF_OK), "Success");
    PF_ASSERT_ALWAYS_STR_EQ(pf_error_name(PF_OK), "OK");
    PF_ASSERT_ALWAYS_STR_EQ(
        pf_strerror(PF_ENOENT), "No such file or directory"
    );
    PF_ASSERT_ALWAYS_STR_EQ(pf_error_name(PF_ENOENT), "ENOENT");
    /* aliases have the name of their canonical code */
    PF_ASSERT_ALWAYS_STR_EQ(pf_error_name(PF_EWOULDBLOCK), "EAGAIN");
    /* not codes */
    PF_ASSERT_ALWAYS_STR_EQ(pf_strerror(2), "Unknown error");
    PF_ASSERT_ALWAYS_STR_EQ(pf_strerror(-100000), "Unknown error");
    PF_ASSERT_ALWAYS_STR_EQ(pf_error_name(2), "EUNKNOWN");
    PF_ASSERT_ALWAYS_STR_EQ(pf_error_name(PF_EUNKNOWN), "EUNKNOWN");
    return 0;
}

static int test_strerror_r(int seed, int i) {
    char buf[256], tiny[4];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_strerror_r(ENOENT, buf, sizeof(buf)), 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, strerror(ENOENT));
    PF_ASSERT_ALWAYS_EQ(pf_strerror_r(EINVAL, buf, sizeof(buf)), 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, strerror(EINVAL));

    /* truncated, still terminated */
    PF_ASSERT_ALWAYS_EQ(pf_strerror_r(ENOENT, tiny, sizeof(tiny)), ERANGE);
    PF_ASSERT_ALWAYS_EQ(strlen(tiny), 3);
    PF_ASSERT_ALWAYS_MEM_EQ(tiny, strerror(ENOENT), 3);
    PF_ASSERT_ALWAYS_EQ(pf_strerror_r(ENOENT, tiny, 0), EINVAL);

    /* an unknown number still gives a message */
    PF_ASSERT_ALWAYS_EQ(pf_strerror_r(123456, buf, sizeof(buf)), 0);
    PF_ASSERT_ALWAYS(buf[0] != '\0');
    return 0;
}

pf_test_t suite_errno[] = {
    { test_values, "values" },
    { test_roundtrip, "roundtrip" },
    { test_common, "common" },
    { test_linux, "linux" },
    { test_strings, "strings" },
    { test_strerror_r, "strerror_r" },
    { 0 },
};
