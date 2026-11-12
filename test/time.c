/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/time.h>, and the portable timegm. */

#include "../src/internal.h"

#include <cpolyfill/time.h>

#include <errno.h>
#include <stdint.h>

#include "check.h"

static int test_clocks(int seed, int i) {
    static const pf_clockid_t clocks[] = {
        PF_CLOCK_REALTIME,
        PF_CLOCK_MONOTONIC,
        PF_CLOCK_PROCESS_CPUTIME_ID,
        PF_CLOCK_THREAD_CPUTIME_ID,
    };
    struct timespec ts, prev, res;
    size_t k;
    int n;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(clocks) / sizeof(clocks[0]); k++) {
        PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(clocks[k], &ts), 0);
        PF_ASSERT_ALWAYS(ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000L);
        PF_ASSERT_ALWAYS_EQ(pf_clock_getres(clocks[k], &res), 0);
        PF_ASSERT_ALWAYS(res.tv_sec > 0 || res.tv_nsec > 0);
    }

    /* The wall clock agrees with time(). */
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_REALTIME, &ts), 0);
    PF_ASSERT_ALWAYS(ts.tv_sec - time(NULL) <= 1 && time(NULL) - ts.tv_sec <= 1);

    /* The monotonic clock never goes back. */
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_MONOTONIC, &prev), 0);
    for (n = 0; n < 10000; n++) {
        PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_MONOTONIC, &ts), 0);
        PF_ASSERT_ALWAYS(pf_timespec_cmp(prev, ts) <= 0);
        prev = ts;
    }

    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(42, &ts), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_clock_getres(-1, &res), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    return 0;
}

static int test_timespec_get(int seed, int i) {
    struct timespec ts;
    int base;
    (void)seed, (void)i;

    for (base = PF_TIME_UTC; base <= PF_TIME_THREAD_ACTIVE; base++) {
        PF_ASSERT_ALWAYS_EQ(pf_timespec_get(&ts, base), base);
        PF_ASSERT_ALWAYS_EQ(pf_timespec_getres(&ts, base), base);
    }
    PF_ASSERT_ALWAYS_EQ(pf_timespec_get(&ts, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_get(&ts, 99), 0);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_getres(&ts, 99), 0);
    return 0;
}

static int test_sleep(int seed, int i) {
    struct timespec req, ts;
    uint64_t start, elapsed;
    (void)seed, (void)i;

    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_sleep_ns(20000000u), 0);
    elapsed = pf_time_ns() - start;
    PF_ASSERT_ALWAYS_GE(elapsed, 20000000u);

    req.tv_sec = 0;
    req.tv_nsec = 5000000;
    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_nanosleep(&req, NULL), 0);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 5000000u);

    req.tv_nsec = 0;
    PF_ASSERT_ALWAYS_EQ(pf_nanosleep(&req, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sleep_ns(0), 0);

    start = pf_time_ns();
    PF_ASSERT_ALWAYS_EQ(pf_usleep(3000), 0);
    PF_ASSERT_ALWAYS_GE(pf_time_ns() - start, 3000000u);
    PF_ASSERT_ALWAYS_EQ(pf_usleep(0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sleep(0), 0u);

    errno = 0;
    req.tv_nsec = 1000000000L;
    PF_ASSERT_ALWAYS_EQ(pf_nanosleep(&req, NULL), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    errno = 0;
    req.tv_nsec = -1;
    PF_ASSERT_ALWAYS_EQ(pf_nanosleep(&req, NULL), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);

    /* A deadline lies ahead of the wall clock by the given amount. */
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(&req, 2000000000LL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_get(&ts, PF_TIME_UTC), PF_TIME_UTC);
    PF_ASSERT_ALWAYS(pf_timespec_cmp(ts, req) < 0);
    PF_ASSERT_ALWAYS_LE(pf_timespec_to_ns(pf_timespec_sub(req, ts)), 2000000000LL);
    PF_ASSERT_ALWAYS_EQ(pf_deadline_ns(NULL, 0), PF_EINVAL);
    return 0;
}

static int test_helpers(int seed, int i) {
    struct timespec a, b, c;
    (void)seed, (void)i;

    a = pf_timespec_from_ns(1500000000LL);
    PF_ASSERT_ALWAYS_EQ(a.tv_sec, 1);
    PF_ASSERT_ALWAYS_EQ(a.tv_nsec, 500000000L);
    b = pf_timespec_from_ns(-1);
    PF_ASSERT_ALWAYS_EQ(b.tv_sec, -1);
    PF_ASSERT_ALWAYS_EQ(b.tv_nsec, 999999999L);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_to_ns(b), -1);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_to_ns(a), 1500000000LL);

    c = pf_timespec_add(a, a);
    PF_ASSERT_ALWAYS_EQ(c.tv_sec, 3);
    PF_ASSERT_ALWAYS_EQ(c.tv_nsec, 0);
    c = pf_timespec_sub(a, pf_timespec_from_ns(700000000LL));
    PF_ASSERT_ALWAYS_EQ(c.tv_sec, 0);
    PF_ASSERT_ALWAYS_EQ(c.tv_nsec, 800000000L);
    c = pf_timespec_sub(b, a);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_to_ns(c), -1500000001LL);

    PF_ASSERT_ALWAYS_EQ(pf_timespec_cmp(a, a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_cmp(b, a), -1);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_cmp(a, b), 1);
    PF_ASSERT_ALWAYS_EQ(pf_timespec_cmp(a, pf_timespec_from_ns(1500000001LL)), -1);
    return 0;
}

static void check_tm(
    const struct tm *tm, int year, int mon, int mday, int hour, int min,
    int sec, int wday, int yday
) {
    PF_ASSERT_ALWAYS_EQ(tm->tm_year + 1900, year);
    PF_ASSERT_ALWAYS_EQ(tm->tm_mon + 1, mon);
    PF_ASSERT_ALWAYS_EQ(tm->tm_mday, mday);
    PF_ASSERT_ALWAYS_EQ(tm->tm_hour, hour);
    PF_ASSERT_ALWAYS_EQ(tm->tm_min, min);
    PF_ASSERT_ALWAYS_EQ(tm->tm_sec, sec);
    PF_ASSERT_ALWAYS_EQ(tm->tm_wday, wday);
    PF_ASSERT_ALWAYS_EQ(tm->tm_yday, yday);
}

static int test_gmtime(int seed, int i) {
    struct tm tm;
    time_t t;
    (void)seed, (void)i;

    t = 0;
    PF_ASSERT_ALWAYS(pf_gmtime_r(&t, &tm) == &tm);
    check_tm(&tm, 1970, 1, 1, 0, 0, 0, 4, 0);

    t = 951782400; /* 2000-02-29, a leap day of a century year */
    PF_ASSERT_ALWAYS(pf_gmtime_r(&t, &tm) == &tm);
    check_tm(&tm, 2000, 2, 29, 0, 0, 0, 2, 59);

    t = time(NULL);
    PF_ASSERT_ALWAYS(pf_localtime_r(&t, &tm) == &tm);
    PF_ASSERT_ALWAYS(tm.tm_year >= 120);
    return 0;
}

typedef time_t (*timegm_fn)(struct tm *);

static void check_timegm(timegm_fn fn, unsigned *rng) {
    struct tm tm, back;
    time_t t;
    int n;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 70;
    tm.tm_mday = 1;
    PF_ASSERT_ALWAYS_EQ(fn(&tm), 0);
    check_tm(&tm, 1970, 1, 1, 0, 0, 0, 4, 0);

    /* Out-of-range fields are normalized: month 13 of 1999 is 2000-02,
       its day 0 is 2000-01-31, and second -1 is the day before. */
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 99;
    tm.tm_mon = 13;
    tm.tm_mday = 0;
    tm.tm_sec = -1;
    t = fn(&tm);
    check_tm(&tm, 2000, 1, 30, 23, 59, 59, 0, 29);
    PF_ASSERT_ALWAYS_EQ(t, 949276799);

    /* Negative months go back into the previous years. */
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 100;
    tm.tm_mon = -14; /* 1998-11 */
    tm.tm_mday = 15;
    fn(&tm);
    check_tm(&tm, 1998, 11, 15, 0, 0, 0, 0, 318);

    /* Round trips through gmtime, before and after 1970. */
    for (n = 0; n < 2000; n++) {
        int64_t v = ((int64_t)test_rand(rng) << 30) ^ ((int64_t)test_rand(rng) << 15)
                  ^ test_rand(rng);
        if (n & 1)
            v = -v;
        t = (time_t)(v % 20000000000LL);
        if (t < 0 && PF_OS_WINDOWS)
            t = -t;
        if (sizeof(time_t) < 8 && (int64_t)t != v % 20000000000LL)
            continue;
        PF_ASSERT_ALWAYS(pf_gmtime_r(&t, &tm) != NULL);
        back = tm;
        back.tm_wday = back.tm_yday = -1;
        PF_ASSERT_ALWAYS_EQ(fn(&back), t);
        check_tm(&back, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday, tm.tm_yday);
    }
}

static int test_timegm(int seed, int i) {
    unsigned rng = (unsigned)seed;
    (void)i;

    check_timegm(pf_timegm, &rng);
    check_timegm(pf__timegm_portable, &rng);

    /* Years far outside the range of gmtime on some systems. */
    if (sizeof(time_t) >= 8) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = -10000 - 1900; /* 10001 BCE */
        tm.tm_mday = 1;
        PF_ASSERT_ALWAYS(pf__timegm_portable(&tm) < 0);
        PF_ASSERT_ALWAYS_EQ(tm.tm_year, -10000 - 1900);
        PF_ASSERT_ALWAYS_EQ(tm.tm_yday, 0);
    }
    return 0;
}

pf_test_t suite_time[] = {
    { test_clocks, "clocks" },
    { test_timespec_get, "timespec_get" },
    { test_sleep, "sleep" },
    { test_helpers, "helpers" },
    { test_gmtime, "gmtime" },
    { test_timegm, "timegm" },
    { 0 },
};
