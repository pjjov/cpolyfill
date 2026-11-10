/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/stdlib.h>, and the portable getdelim and qsort_r the library
   uses where the system has none. */

#include "../src/internal.h"

#include <cpolyfill/macro.h>
#include <cpolyfill/stdlib.h>

#include <errno.h>
#include <stdint.h>

#include "check.h"
#include "util.h"

static int test_aligned_alloc(int seed, int i) {
    size_t align;
    (void)seed, (void)i;

    for (align = 1; align <= 4096; align *= 2) {
        unsigned char *p = (unsigned char *)pf_aligned_alloc(align, 100);
        PF_ASSERT_ALWAYS(p != NULL);
        PF_ASSERT_ALWAYS_EQ((uintptr_t)p % align, 0);
        memset(p, 0xAB, 100);
        pf_aligned_free(p);
    }
    /* size not a multiple of the alignment, and zero */
    {
        void *p = pf_aligned_alloc(64, 1);
        void *q = pf_aligned_alloc(64, 0);
        PF_ASSERT_ALWAYS(p != NULL && q != NULL);
        pf_aligned_free(p);
        pf_aligned_free(q);
    }
    errno = 0;
    PF_ASSERT_ALWAYS(pf_aligned_alloc(24, 100) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    errno = 0;
    PF_ASSERT_ALWAYS(pf_aligned_alloc(0, 100) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    pf_aligned_free(NULL);
    return 0;
}

static int test_reallocarray(int seed, int i) {
    int *p;
    (void)seed, (void)i;

    p = (int *)pf_reallocarray(NULL, 10, sizeof(int));
    PF_ASSERT_ALWAYS(p != NULL);
    p[9] = 7;
    p = (int *)pf_reallocarray(p, 1000, sizeof(int));
    PF_ASSERT_ALWAYS(p != NULL);
    PF_ASSERT_ALWAYS_EQ(p[9], 7);

    errno = 0;
    PF_ASSERT_ALWAYS(pf_reallocarray(p, SIZE_MAX / 2, 3) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ENOMEM);
    PF_ASSERT_ALWAYS_EQ(p[9], 7); /* untouched on failure */

    /* zero gives a minimal block, never NULL */
    p = (int *)pf_reallocarray(p, 0, sizeof(int));
    PF_ASSERT_ALWAYS(p != NULL);
    free(p);
    return 0;
}

typedef pf_ssize_t (*getdelim_fn)(char **, size_t *, int, FILE *);

static void check_getdelim(getdelim_fn fn) {
    static const char data[] = "first\nsecond line\n\nno newline";
    char *line = NULL;
    size_t cap = 0;
    char longline[5000];
    FILE *f = tmpfile();

    PF_ASSERT_ALWAYS(f != NULL);
    fwrite(data, 1, sizeof(data) - 1, f);
    rewind(f);

    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, '\n', f), 6);
    PF_ASSERT_ALWAYS_STR_EQ(line, "first\n");
    PF_ASSERT_ALWAYS(cap > 6);
    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, '\n', f), 12);
    PF_ASSERT_ALWAYS_STR_EQ(line, "second line\n");
    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, '\n', f), 1);
    PF_ASSERT_ALWAYS_STR_EQ(line, "\n");
    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, '\n', f), 10);
    PF_ASSERT_ALWAYS_STR_EQ(line, "no newline");
    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, '\n', f), -1);
    PF_ASSERT_ALWAYS(feof(f));

    /* another delimiter, and an embedded NUL */
    rewind(f);
    fwrite("a\0b;c", 1, 5, f);
    fflush(f);
    rewind(f);
    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, ';', f), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(line, "a\0b;", 5);
    fclose(f);

    /* a line longer than any initial buffer */
    f = tmpfile();
    PF_ASSERT_ALWAYS(f != NULL);
    memset(longline, 'x', sizeof(longline));
    fwrite(longline, 1, sizeof(longline), f);
    fputc('\n', f);
    rewind(f);
    PF_ASSERT_ALWAYS_EQ(
        fn(&line, &cap, '\n', f), (pf_ssize_t)sizeof(longline) + 1
    );
    PF_ASSERT_ALWAYS(cap > sizeof(longline) + 1);
    PF_ASSERT_ALWAYS_EQ(strspn(line, "x"), sizeof(longline));
    fclose(f);
    free(line);

    /* an empty file with a NULL buffer */
    f = tmpfile();
    line = NULL;
    cap = 0;
    PF_ASSERT_ALWAYS_EQ(fn(&line, &cap, '\n', f), -1);
    free(line);
    fclose(f);
}

static pf_ssize_t call_getline_as_delim(
    char **line, size_t *cap, int delim, FILE *f
) {
    if (delim == '\n')
        return pf_getline(line, cap, f);
    return pf_getdelim(line, cap, delim, f);
}

static int test_getline(int seed, int i) {
    (void)seed, (void)i;
    check_getdelim(pf__getdelim_portable);
    check_getdelim(pf_getdelim);
    check_getdelim(call_getline_as_delim);

    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf__getdelim_portable(NULL, NULL, '\n', stdin), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    return 0;
}

static int test_environment(int seed, int i) {
    char name[64], buf[64];
    (void)seed, (void)i;

    snprintf(name, sizeof(name), "PF_TEST_ENV_%ld", test_pid());
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, sizeof(buf)), PF_ENOENT);

    PF_ASSERT_ALWAYS_EQ(pf_setenv(name, "v\xC3\xA9", 1), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, NULL, 0), 3);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, sizeof(buf)), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "v\xC3\xA9");
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, 3), PF_ERANGE);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, 4), 3);

    /* no overwrite */
    PF_ASSERT_ALWAYS_EQ(pf_setenv(name, "other", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, sizeof(buf)), 3);
    PF_ASSERT_ALWAYS_EQ(pf_setenv(name, "other", 1), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, sizeof(buf)), 5);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "other");
    /* the C library sees it too */
    PF_ASSERT_ALWAYS_STR_EQ(getenv(name), "other");

    PF_ASSERT_ALWAYS_EQ(pf_unsetenv(name), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(name, buf, sizeof(buf)), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_unsetenv(name), 0); /* already gone */

    /* bad names */
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_setenv("", "x", 1), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_setenv("A=B", "x", 1), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_unsetenv("A=B"), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_getenv("", buf, sizeof(buf)), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_getenv(NULL, buf, sizeof(buf)), PF_EINVAL);
    return 0;
}

static int cmp_int(const void *a, const void *b, void *arg) {
    int x = *(const int *)a, y = *(const int *)b;
    ++*(int *)arg;
    return (x > y) - (x < y);
}

/* Sorts by the first member only; the second tells equal keys apart. */
typedef struct pair {
    int key;
    int tag;
} pair;

static int cmp_pair_desc(const void *a, const void *b, void *arg) {
    int x = ((const pair *)a)->key, y = ((const pair *)b)->key;
    (void)arg;
    return (y > x) - (y < x);
}

typedef void (*qsort_r_fn)(
    void *, size_t, size_t, int (*)(const void *, const void *, void *), void *
);

static void check_qsort_r(qsort_r_fn fn, int seed, int i) {
    unsigned state = (unsigned)seed * 7u + (unsigned)i;
    int a[257], calls = 0;
    pair p[100];
    size_t n, k;

    for (n = 0; n < PF_COUNTOF(a); n = n * 2 + 1) {
        for (k = 0; k < n; k++)
            a[k] = (int)(test_rand(&state) % 50) - 25;
        fn(a, n, sizeof(int), cmp_int, &calls);
        for (k = 1; k < n; k++)
            PF_ASSERT_ALWAYS(a[k - 1] <= a[k]);
    }
    PF_ASSERT_ALWAYS(calls > 0); /* `arg` reached the comparison */

    /* already sorted, reversed, and all equal */
    for (k = 0; k < 200; k++)
        a[k] = (int)k;
    fn(a, 200, sizeof(int), cmp_int, &calls);
    for (k = 0; k < 200; k++)
        PF_ASSERT_ALWAYS_EQ(a[k], (int)k);
    for (k = 0; k < 200; k++)
        a[k] = 200 - (int)k;
    fn(a, 200, sizeof(int), cmp_int, &calls);
    for (k = 1; k < 200; k++)
        PF_ASSERT_ALWAYS(a[k - 1] <= a[k]);

    /* elements larger than a word, a multiset of keys is kept */
    for (k = 0; k < PF_COUNTOF(p); k++) {
        p[k].key = (int)(test_rand(&state) % 10);
        p[k].tag = (int)k;
    }
    fn(p, PF_COUNTOF(p), sizeof(pair), cmp_pair_desc, NULL);
    {
        int seen[100] = { 0 };
        for (k = 0; k < PF_COUNTOF(p); k++) {
            if (k)
                PF_ASSERT_ALWAYS(p[k - 1].key >= p[k].key);
            seen[p[k].tag]++;
        }
        for (k = 0; k < PF_COUNTOF(p); k++)
            PF_ASSERT_ALWAYS_EQ(seen[k], 1);
    }
}

static int test_qsort_r(int seed, int i) {
    check_qsort_r(pf__qsort_r_portable, seed, i);
    check_qsort_r(pf_qsort_r, seed, i);
    return 0;
}

pf_test_t suite_stdlib[] = {
    { test_aligned_alloc, "aligned_alloc" },
    { test_reallocarray, "reallocarray" },
    { test_getline, "getline" },
    { test_environment, "environment" },
    { test_qsort_r, "qsort_r" },
    { 0 },
};
