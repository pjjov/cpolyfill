/** cpolyfill - Filling the gaps between C standards and compilers

    Clocks, sleeping and calendar time, the same on every platform.

        pf_clockid_t            A clock: PF_CLOCK_REALTIME (wall clock, UTC),
                                PF_CLOCK_MONOTONIC (never goes back, not
                                adjusted), PF_CLOCK_PROCESS_CPUTIME_ID,
                                PF_CLOCK_THREAD_CPUTIME_ID (CPU time of the
                                process / calling thread).

        int pf_clock_gettime(pf_clockid_t clock, struct timespec *ts)
        int pf_clock_getres(pf_clockid_t clock, struct timespec *res)
                POSIX clock_gettime and clock_getres: 0, or -1 with errno
                EINVAL for an unknown clock.
        int pf_nanosleep(const struct timespec *req, struct timespec *rem)
                POSIX nanosleep: 0, or -1 with errno EINTR (the time left
                is stored in `rem` if it is not NULL) or EINVAL. On Windows
                the sleep is never interrupted.

        int pf_timespec_get(struct timespec *ts, int base)
        int pf_timespec_getres(struct timespec *res, int base)
                C11 timespec_get and C23 timespec_getres: return `base`, or
                0 on failure. Bases: PF_TIME_UTC (PF_CLOCK_REALTIME),
                PF_TIME_MONOTONIC, PF_TIME_ACTIVE (process CPU time),
                PF_TIME_THREAD_ACTIVE (thread CPU time).

        struct tm *pf_gmtime_r(const time_t *t, struct tm *tm)
        struct tm *pf_localtime_r(const time_t *t, struct tm *tm)
                POSIX gmtime_r and localtime_r: `tm`, or NULL with errno
                EOVERFLOW (or EINVAL). Windows rejects times before 1970.
        time_t pf_timegm(struct tm *tm)
                C23 timegm, the inverse of gmtime: the fields of `tm` may be
                out of range and are normalized, and `tm_wday` / `tm_yday`
                set. Returns (time_t)-1 with errno EOVERFLOW if the result
                does not fit. Works for every year on every platform.

        uint64_t pf_time_ns(void)
                The monotonic clock in nanoseconds, for measuring intervals.
        int pf_sleep_ns(uint64_t ns)
                Sleeps at least `ns` nanoseconds, resuming after signals:
                0 or a negative PF_E* code.
        unsigned pf_sleep(unsigned seconds)
                POSIX sleep: sleeps `seconds`, resuming after signals, and
                returns 0.
        int pf_usleep(unsigned long usec)
                usleep (removed from POSIX, still common): sleeps at least
                `usec` microseconds; 0.
        int pf_deadline_ns(struct timespec *ts, int64_t ns)
                Sets `ts` to the current UTC time plus `ns`: the absolute
                deadline the C11 timed waits (pf_mtx_timedlock,
                pf_cnd_timedwait, ...) take. 0 or a negative PF_E* code.

    Header-only helpers on normalized timespecs (0 <= tv_nsec < 10^9):

        int64_t pf_timespec_to_ns(struct timespec ts)
        struct timespec pf_timespec_from_ns(int64_t ns)
        struct timespec pf_timespec_add(struct timespec a, struct timespec b)
        struct timespec pf_timespec_sub(struct timespec a, struct timespec b)
        int pf_timespec_cmp(struct timespec a, struct timespec b)
                -1, 0 or 1.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_TIME, where the
    library found them missing when it was built: `clockid_t` and
    `CLOCK_*` (with `clock_gettime`), `clock_gettime`, `clock_getres`,
    `nanosleep`, `timespec_get` (and `TIME_UTC`, `TIME_MONOTONIC`,
    `TIME_ACTIVE`, `TIME_THREAD_ACTIVE`), `timespec_getres`, `gmtime_r`,
    `localtime_r`, `timegm`, `sleep`, `usleep`.

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
**/

#ifndef CPOLYFILL_TIME_H
#define CPOLYFILL_TIME_H

#include <cpolyfill/platform.h>

#include <stdint.h>
#include <time.h>

/* struct timespec, also in strict C99 where <time.h> has none: POSIX
   requires <sched.h> to define it, but only glibc and musl do so whatever
   the feature macros. */
#if PF_OS_LINUX || PF_OS_ANDROID
    #include <sched.h>
#elif !PF_OS_WINDOWS
    #include <sys/select.h>
#endif

typedef int pf_clockid_t;

#define PF_CLOCK_REALTIME 0
#define PF_CLOCK_MONOTONIC 1
#define PF_CLOCK_PROCESS_CPUTIME_ID 2
#define PF_CLOCK_THREAD_CPUTIME_ID 3

#define PF_TIME_UTC 1
#define PF_TIME_MONOTONIC 2
#define PF_TIME_ACTIVE 3
#define PF_TIME_THREAD_ACTIVE 4

PF_BEGIN_DECLS

PF_API int pf_clock_gettime(pf_clockid_t clock, struct timespec *ts);
PF_API int pf_clock_getres(pf_clockid_t clock, struct timespec *res);
PF_API int pf_nanosleep(const struct timespec *req, struct timespec *rem);

PF_API int pf_timespec_get(struct timespec *ts, int base);
PF_API int pf_timespec_getres(struct timespec *res, int base);

PF_API struct tm *pf_gmtime_r(const time_t *t, struct tm *tm);
PF_API struct tm *pf_localtime_r(const time_t *t, struct tm *tm);
PF_API time_t pf_timegm(struct tm *tm);

PF_API uint64_t pf_time_ns(void);
PF_API int pf_sleep_ns(uint64_t ns);
PF_API unsigned pf_sleep(unsigned seconds);
PF_API int pf_usleep(unsigned long usec);
PF_API int pf_deadline_ns(struct timespec *ts, int64_t ns);

#define PF__NS_PER_SEC 1000000000L

PF_INLINE int64_t pf_timespec_to_ns(struct timespec ts) {
    return (int64_t)ts.tv_sec * PF__NS_PER_SEC + ts.tv_nsec;
}

PF_INLINE struct timespec pf_timespec_from_ns(int64_t ns) {
    struct timespec ts;
    int64_t sec = ns / PF__NS_PER_SEC, nsec = ns % PF__NS_PER_SEC;
    if (nsec < 0) {
        nsec += PF__NS_PER_SEC;
        sec--;
    }
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)nsec;
    return ts;
}

PF_INLINE struct timespec pf_timespec_add(
    struct timespec a, struct timespec b
) {
    a.tv_sec += b.tv_sec;
    a.tv_nsec += b.tv_nsec;
    if (a.tv_nsec >= PF__NS_PER_SEC) {
        a.tv_nsec -= PF__NS_PER_SEC;
        a.tv_sec++;
    }
    return a;
}

PF_INLINE struct timespec pf_timespec_sub(
    struct timespec a, struct timespec b
) {
    a.tv_sec -= b.tv_sec;
    a.tv_nsec -= b.tv_nsec;
    if (a.tv_nsec < 0) {
        a.tv_nsec += PF__NS_PER_SEC;
        a.tv_sec--;
    }
    return a;
}

PF_INLINE int pf_timespec_cmp(struct timespec a, struct timespec b) {
    if (a.tv_sec != b.tv_sec)
        return a.tv_sec < b.tv_sec ? -1 : 1;
    if (a.tv_nsec != b.tv_nsec)
        return a.tv_nsec < b.tv_nsec ? -1 : 1;
    return 0;
}

PF_END_DECLS

#endif /* CPOLYFILL_TIME_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__TIME_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_TIME)) \
        || defined(PF_STD_NAMES_TIME))
    #define PF__TIME_STD_NAMES

    #ifdef PF__FORCE_STD_NAMES_POLYFILL
        #define PF__TIME_MAP(have) 1
    #else
        #define PF__TIME_MAP(have) (!(have))
    #endif

    #if PF__TIME_MAP(PF_CONFIG_HAVE_CLOCK_GETTIME)
        #ifndef PF__FORCE_STD_NAMES_POLYFILL
typedef pf_clockid_t clockid_t;
        #endif
        #undef CLOCK_REALTIME
        #undef CLOCK_MONOTONIC
        #undef CLOCK_PROCESS_CPUTIME_ID
        #undef CLOCK_THREAD_CPUTIME_ID
        #define CLOCK_REALTIME PF_CLOCK_REALTIME
        #define CLOCK_MONOTONIC PF_CLOCK_MONOTONIC
        #define CLOCK_PROCESS_CPUTIME_ID PF_CLOCK_PROCESS_CPUTIME_ID
        #define CLOCK_THREAD_CPUTIME_ID PF_CLOCK_THREAD_CPUTIME_ID
        #undef clock_gettime
        #define clock_gettime pf_clock_gettime
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_CLOCK_GETRES)
        #undef clock_getres
        #define clock_getres pf_clock_getres
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_NANOSLEEP)
        #undef nanosleep
        #define nanosleep pf_nanosleep
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_TIMESPEC_GET)
        #undef TIME_UTC
        #undef TIME_MONOTONIC
        #undef TIME_ACTIVE
        #undef TIME_THREAD_ACTIVE
        #define TIME_UTC PF_TIME_UTC
        #define TIME_MONOTONIC PF_TIME_MONOTONIC
        #define TIME_ACTIVE PF_TIME_ACTIVE
        #define TIME_THREAD_ACTIVE PF_TIME_THREAD_ACTIVE
        #undef timespec_get
        #define timespec_get pf_timespec_get
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_TIMESPEC_GETRES)
        #undef timespec_getres
        #define timespec_getres pf_timespec_getres
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_GMTIME_R)
        #undef gmtime_r
        #define gmtime_r pf_gmtime_r
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_SLEEP)
        #undef sleep
        #define sleep pf_sleep
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_USLEEP)
        #undef usleep
        #define usleep pf_usleep
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_LOCALTIME_R)
        #undef localtime_r
        #define localtime_r pf_localtime_r
    #endif
    #if PF__TIME_MAP(PF_CONFIG_HAVE_TIMEGM)
        #undef timegm
        #define timegm pf_timegm
    #endif
#endif
