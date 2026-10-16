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

/* Operating system helpers for the test suites. They deliberately use the
   native APIs instead of the polyfills, so a broken polyfill cannot break
   the fixtures of its own test. */

#ifndef PF_TEST_UTIL_H
#define PF_TEST_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

#ifdef _WIN32
    #define TEST_SEP '\\'
#else
    #define TEST_SEP '/'
#endif

#define TEST_PATH_MAX 512

static inline long test_pid(void) {
#ifdef _WIN32
    return (long)GetCurrentProcessId();
#else
    return (long)getpid();
#endif
}

/* Monotonic time in seconds. */
static inline double test_now(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

static inline void test_sleep_ms(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0)
        ;
#endif
}

/* An absolute wall clock deadline `ms` milliseconds from now, as used by
   the timed waiting functions. */
static inline void test_deadline(struct timespec *ts, unsigned ms) {
#if defined(CLOCK_REALTIME) && !defined(_WIN32)
    clock_gettime(CLOCK_REALTIME, ts);
#else
    timespec_get(ts, TIME_UTC);
#endif
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

/* The directory for scratch files, without a trailing separator. */
static inline const char *test_tmpdir(void) {
    const char *d = getenv("TMPDIR");
    if (!d || !*d)
        d = getenv("TEMP");
    if (!d || !*d)
        d = getenv("TMP");
#ifdef _WIN32
    if (!d || !*d)
        d = ".";
#else
    if (!d || !*d)
        d = "/tmp";
#endif
    return d;
}

/* A unique path in the temporary directory. Unique per process and call. */
static inline char *test_path(char *buf, size_t size, const char *tag) {
    static unsigned counter = 0;
    int n = snprintf(
        buf,
        size,
        "%s%cpf-%ld-%u-%s",
        test_tmpdir(),
        TEST_SEP,
        test_pid(),
        counter++,
        tag
    );
    if (n < 0 || (size_t)n >= size)
        buf[0] = '\0'; /* too long: the callers' checks will fail */
    return buf;
}

/* `dir/name` */
static inline char *test_join(
    char *buf, size_t size, const char *dir, const char *name
) {
    int n = snprintf(buf, size, "%s%c%s", dir, TEST_SEP, name);
    if (n < 0 || (size_t)n >= size)
        buf[0] = '\0';
    return buf;
}

static inline int test_mkdir(const char *path) {
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    return mkdir(path, 0700);
#endif
}

static inline int test_rmdir(const char *path) {
#ifdef _WIN32
    return RemoveDirectoryA(path) ? 0 : -1;
#else
    return rmdir(path);
#endif
}

static inline int test_unlink(const char *path) {
#ifdef _WIN32
    return DeleteFileA(path) ? 0 : -1;
#else
    return unlink(path);
#endif
}

/* Sets (or with NULL, removes) an environment variable. */
static inline int test_setenv(const char *name, const char *value) {
#ifdef _WIN32
    return _putenv_s(name, value ? value : "") == 0 ? 0 : -1;
#else
    return value ? setenv(name, value, 1) : unsetenv(name);
#endif
}

/* Writes `size` bytes to `path`. Returns 0 on success. */
static inline int test_write(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    size_t n;
    if (!f)
        return -1;
    n = size ? fwrite(data, 1, size, f) : 0;
    return (fclose(f) == 0 && n == size) ? 0 : -1;
}

/* Reads at most `size - 1` bytes and NUL-terminates. Returns the length
   or -1 if the file cannot be opened. */
static inline long test_read(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f)
        return -1;
    n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return (long)n;
}

#endif /* PF_TEST_UTIL_H */
