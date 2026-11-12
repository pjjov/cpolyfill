/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>

#if PF_OS_WINDOWS
    #include <windows.h>
#endif

#define NS_PER_SEC 1000000000LL

/* ---- Calendar arithmetic (timegm) ------------------------------------- */

/* Days since 1970-01-01 of a proleptic Gregorian date, for any year (H.
   Hinnant's days_from_civil). `m` is 1..12. */
static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
    int64_t era, yoe, doy, doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* The inverse: the date of a day number. */
static void civil_from_days(int64_t z, int64_t *y, int *m, int *d) {
    int64_t era, doe, yoe, doy, mp;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

/* Floor division and modulo. */
static int64_t fdiv(int64_t a, int64_t b) {
    return a / b - (a % b != 0 && (a < 0) != (b < 0));
}

time_t pf__timegm_portable(struct tm *tm) {
    int64_t mon = tm->tm_mon, year, days, secs, sod, y;
    int m, d;
    time_t t;

    /* Normalize the month first: the day of the month depends on it. */
    year = (int64_t)tm->tm_year + 1900 + fdiv(mon, 12);
    mon -= fdiv(mon, 12) * 12;
    days = days_from_civil(year, mon + 1, 1) + (int64_t)tm->tm_mday - 1;
    secs = days * 86400 + (int64_t)tm->tm_hour * 3600
         + (int64_t)tm->tm_min * 60 + tm->tm_sec;

    t = (time_t)secs;
    if ((int64_t)t != secs) {
        errno = EOVERFLOW;
        return (time_t)-1;
    }

    days = fdiv(secs, 86400);
    sod = secs - days * 86400;
    civil_from_days(days, &y, &m, &d);
    if (y - 1900 < INT_MIN || y - 1900 > INT_MAX) {
        errno = EOVERFLOW;
        return (time_t)-1;
    }
    tm->tm_year = (int)(y - 1900);
    tm->tm_mon = m - 1;
    tm->tm_mday = d;
    tm->tm_hour = (int)(sod / 3600);
    tm->tm_min = (int)(sod / 60 % 60);
    tm->tm_sec = (int)(sod % 60);
    tm->tm_wday = (int)(days + 4 - fdiv(days + 4, 7) * 7); /* 1970: Thu */
    tm->tm_yday = (int)(days - days_from_civil(y, 1, 1));
    tm->tm_isdst = 0;
    return t;
}

PF_API time_t pf_timegm(struct tm *tm) {
#if PF_CONFIG_HAVE_TIMEGM && !PF_OS_WINDOWS
    /* The system's handles the time zone fields (tm_gmtoff, tm_zone). */
    return timegm(tm);
#else
    return pf__timegm_portable(tm);
#endif
}

/* ---- Windows ----------------------------------------------------------- */

#if PF_OS_WINDOWS

/* 100 ns intervals between 1601-01-01 and 1970-01-01. */
    #define EPOCH_DIFF 116444736000000000ULL

static void from_100ns(uint64_t t, struct timespec *ts) {
    ts->tv_sec = (time_t)(t / 10000000u);
    ts->tv_nsec = (long)(t % 10000000u * 100u);
}

static uint64_t filetime_u64(FILETIME ft) {
    return (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
}

static LONGLONG qpc_frequency(void) {
    static volatile LONGLONG freq;
    LONGLONG f = freq;
    if (!f) {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li); /* never fails since XP */
        f = freq = li.QuadPart;
    }
    return f;
}

PF_API int pf_clock_gettime(pf_clockid_t clock, struct timespec *ts) {
    FILETIME a, b, kernel, user;

    switch (clock) {
    case PF_CLOCK_REALTIME:
        GetSystemTimePreciseAsFileTime(&a);
        from_100ns(filetime_u64(a) - EPOCH_DIFF, ts);
        return 0;
    case PF_CLOCK_MONOTONIC: {
        LARGE_INTEGER c;
        LONGLONG f = qpc_frequency();
        QueryPerformanceCounter(&c);
        ts->tv_sec = (time_t)(c.QuadPart / f);
        ts->tv_nsec = (long)(c.QuadPart % f * NS_PER_SEC / f);
        return 0;
    }
    case PF_CLOCK_PROCESS_CPUTIME_ID:
        if (!GetProcessTimes(GetCurrentProcess(), &a, &b, &kernel, &user))
            break;
        from_100ns(filetime_u64(kernel) + filetime_u64(user), ts);
        return 0;
    case PF_CLOCK_THREAD_CPUTIME_ID:
        if (!GetThreadTimes(GetCurrentThread(), &a, &b, &kernel, &user))
            break;
        from_100ns(filetime_u64(kernel) + filetime_u64(user), ts);
        return 0;
    default:
        break;
    }
    errno = EINVAL;
    return -1;
}

PF_API int pf_clock_getres(pf_clockid_t clock, struct timespec *res) {
    struct timespec r = { 0, 100 };

    switch (clock) {
    case PF_CLOCK_MONOTONIC: {
        LONGLONG f = qpc_frequency();
        r.tv_nsec = f >= NS_PER_SEC ? 1 : (long)((NS_PER_SEC + f - 1) / f);
        break;
    }
    case PF_CLOCK_REALTIME:
    case PF_CLOCK_PROCESS_CPUTIME_ID:
    case PF_CLOCK_THREAD_CPUTIME_ID:
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    if (res)
        *res = r;
    return 0;
}

    #ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
        #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
    #endif

PF_API int pf_nanosleep(const struct timespec *req, struct timespec *rem) {
    HANDLE timer;
    LARGE_INTEGER due;
    uint64_t ticks;

    if (!req || req->tv_nsec < 0 || req->tv_nsec >= NS_PER_SEC
        || req->tv_sec < 0) {
        errno = EINVAL;
        return -1;
    }
    if (rem)
        rem->tv_sec = 0, rem->tv_nsec = 0;

    /* 100 ns units, rounded up so the sleep is never shorter. */
    ticks = (uint64_t)req->tv_sec * 10000000u
          + ((uint64_t)req->tv_nsec + 99u) / 100u;
    if (ticks == 0) {
        SwitchToThread();
        return 0;
    }
    if (ticks > (uint64_t)INT64_MAX)
        ticks = (uint64_t)INT64_MAX;

    /* High resolution timers exist since Windows 10 1803. */
    timer = CreateWaitableTimerExW(
        NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS
    );
    if (!timer)
        timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    if (!timer) {
        /* Sleep takes milliseconds. */
        uint64_t ms = (ticks + 9999u) / 10000u;
        while (ms > 0) {
            DWORD part = ms > 0x7FFFFFFFu ? 0x7FFFFFFFu : (DWORD)ms;
            Sleep(part);
            ms -= part;
        }
        return 0;
    }
    due.QuadPart = -(LONGLONG)ticks; /* negative: relative */
    if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
        WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return 0;
}

PF_API struct tm *pf_gmtime_r(const time_t *t, struct tm *tm) {
    int rc = gmtime_s(tm, t);
    if (rc) {
        errno = rc;
        return NULL;
    }
    return tm;
}

PF_API struct tm *pf_localtime_r(const time_t *t, struct tm *tm) {
    int rc = localtime_s(tm, t);
    if (rc) {
        errno = rc;
        return NULL;
    }
    return tm;
}

/* ---- POSIX ------------------------------------------------------------- */

#else

static int native_clock(pf_clockid_t clock, clockid_t *out) {
    switch (clock) {
    case PF_CLOCK_REALTIME: *out = CLOCK_REALTIME; return 0;
    case PF_CLOCK_MONOTONIC: *out = CLOCK_MONOTONIC; return 0;
    #ifdef CLOCK_PROCESS_CPUTIME_ID
    case PF_CLOCK_PROCESS_CPUTIME_ID: *out = CLOCK_PROCESS_CPUTIME_ID; return 0;
    #endif
    #ifdef CLOCK_THREAD_CPUTIME_ID
    case PF_CLOCK_THREAD_CPUTIME_ID: *out = CLOCK_THREAD_CPUTIME_ID; return 0;
    #endif
    default: errno = EINVAL; return -1;
    }
}

PF_API int pf_clock_gettime(pf_clockid_t clock, struct timespec *ts) {
    clockid_t id;
    if (native_clock(clock, &id))
        return -1;
    return clock_gettime(id, ts);
}

PF_API int pf_clock_getres(pf_clockid_t clock, struct timespec *res) {
    clockid_t id;
    if (native_clock(clock, &id))
        return -1;
    return clock_getres(id, res);
}

PF_API int pf_nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req) {
        errno = EINVAL;
        return -1;
    }
    return nanosleep(req, rem);
}

PF_API struct tm *pf_gmtime_r(const time_t *t, struct tm *tm) {
    return gmtime_r(t, tm);
}

PF_API struct tm *pf_localtime_r(const time_t *t, struct tm *tm) {
    return localtime_r(t, tm);
}

#endif

/* ---- Common ------------------------------------------------------------ */

static pf_clockid_t clock_of_base(int base) {
    switch (base) {
    case PF_TIME_UTC: return PF_CLOCK_REALTIME;
    case PF_TIME_MONOTONIC: return PF_CLOCK_MONOTONIC;
    case PF_TIME_ACTIVE: return PF_CLOCK_PROCESS_CPUTIME_ID;
    case PF_TIME_THREAD_ACTIVE: return PF_CLOCK_THREAD_CPUTIME_ID;
    default: return -1;
    }
}

PF_API int pf_timespec_get(struct timespec *ts, int base) {
    pf_clockid_t clock = clock_of_base(base);
    if (clock < 0 || !ts || pf_clock_gettime(clock, ts))
        return 0;
    return base;
}

PF_API int pf_timespec_getres(struct timespec *res, int base) {
    pf_clockid_t clock = clock_of_base(base);
    if (clock < 0 || pf_clock_getres(clock, res))
        return 0;
    return base;
}

PF_API uint64_t pf_time_ns(void) {
    struct timespec ts;
    if (pf_clock_gettime(PF_CLOCK_MONOTONIC, &ts))
        return 0;
    return (uint64_t)ts.tv_sec * (uint64_t)NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

PF_API int pf_sleep_ns(uint64_t ns) {
    struct timespec req, rem;

    req.tv_sec = (time_t)(ns / (uint64_t)NS_PER_SEC);
    req.tv_nsec = (long)(ns % (uint64_t)NS_PER_SEC);
    if ((uint64_t)req.tv_sec != ns / (uint64_t)NS_PER_SEC)
        return PF_EOVERFLOW;
    while (pf_nanosleep(&req, &rem)) {
        if (errno != EINTR)
            return pf_error_from_errno(errno);
        req = rem;
    }
    return 0;
}

PF_API unsigned pf_sleep(unsigned seconds) {
    (void)pf_sleep_ns((uint64_t)seconds * (uint64_t)NS_PER_SEC);
    return 0;
}

PF_API int pf_usleep(unsigned long usec) {
    (void)pf_sleep_ns((uint64_t)usec * 1000u);
    return 0;
}

PF_API int pf_deadline_ns(struct timespec *ts, int64_t ns) {
    if (!ts)
        return PF_EINVAL;
    if (!pf_timespec_get(ts, PF_TIME_UTC))
        return PF_EINVAL;
    *ts = pf_timespec_add(*ts, pf_timespec_from_ns(ns));
    return 0;
}
