/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The library's default allocator (internal). */

#include "../src/internal.h"

#include "check.h"

static int is_aligned(const void *p, size_t align) {
    return ((uintptr_t)p & (align - 1)) == 0;
}

static int all_zero(const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    size_t k;
    for (k = 0; k < n; k++) {
        if (b[k])
            return 0;
    }
    return 1;
}

static int test_basic(int seed, int i) {
    unsigned state = (unsigned)seed + (unsigned)i;
    size_t size = 1 + test_rand(&state) % 5000;
    unsigned char *p;
    (void)i;

    p = (unsigned char *)allocate(NULL, size);
    PF_ASSERT_ALWAYS(p != NULL);
    PF_ASSERT_ALWAYS(is_aligned(p, 2 * sizeof(void *)));
    memset(p, 0xAB, size);
    deallocate(NULL, p, size);

    p = (unsigned char *)zallocate(NULL, size);
    PF_ASSERT_ALWAYS(p != NULL);
    PF_ASSERT_ALWAYS(all_zero(p, size));
    deallocate(NULL, p, size);
    return 0;
}

static int test_aligned(int seed, int i) {
    static const size_t aligns[] = { 2, 8, 16, 32, 64, 256, 4096 };
    unsigned state = (unsigned)seed + (unsigned)i;
    size_t k;

    for (k = 0; k < sizeof(aligns) / sizeof(aligns[0]); k++) {
        size_t size = 1 + test_rand(&state) % 3000;
        void *p = allocate_aligned(NULL, size, aligns[k]);
        PF_ASSERT_ALWAYS(p != NULL);
        PF_ASSERT_ALWAYS(is_aligned(p, aligns[k]));
        memset(p, 0x5A, size);
        deallocate(NULL, p, size);

        p = zallocate_aligned(NULL, size, aligns[k]);
        PF_ASSERT_ALWAYS(p != NULL);
        PF_ASSERT_ALWAYS(is_aligned(p, aligns[k]));
        PF_ASSERT_ALWAYS(all_zero(p, size));
        deallocate(NULL, p, size);
    }
    return 0;
}

static int test_realloc(int seed, int i) {
    static const size_t aligns[] = { 0, 64, 4096 };
    unsigned state = (unsigned)seed + (unsigned)i;
    size_t k, n;

    for (k = 0; k < sizeof(aligns) / sizeof(aligns[0]); k++) {
        size_t align = aligns[k], old = 1 + test_rand(&state) % 500;
        size_t grow = old + 1 + test_rand(&state) % 5000;
        unsigned char *p = (unsigned char *)allocate_aligned(NULL, old, align);
        PF_ASSERT_ALWAYS(p != NULL);
        for (n = 0; n < old; n++)
            p[n] = (unsigned char)n;

        /* grow, zeroing the new part */
        p = (unsigned char *)zreallocate_aligned(NULL, p, old, grow, align);
        PF_ASSERT_ALWAYS(p != NULL);
        PF_ASSERT_ALWAYS(!align || is_aligned(p, align));
        for (n = 0; n < old; n++)
            PF_ASSERT_ALWAYS(p[n] == (unsigned char)n);
        PF_ASSERT_ALWAYS(all_zero(p + old, grow - old));

        /* shrink */
        p = (unsigned char *)reallocate_aligned(
            NULL, p, grow, old / 2 + 1, align
        );
        PF_ASSERT_ALWAYS(p != NULL);
        PF_ASSERT_ALWAYS(!align || is_aligned(p, align));
        for (n = 0; n < old / 2 + 1; n++)
            PF_ASSERT_ALWAYS(p[n] == (unsigned char)n);
        deallocate(NULL, p, old / 2 + 1);
    }
    return 0;
}

static int test_edges(int seed, int i) {
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(allocate(NULL, 0) == NULL);
    PF_ASSERT_ALWAYS(
        allocate_aligned(NULL, 16, 24) == NULL
    ); /* not a power of 2 */
    deallocate(NULL, NULL, 0);
    deallocate_all(NULL);
    return 0;
}

pf_test_t suite_alloc[] = {
    { test_basic, "basic" },
    { test_aligned, "aligned" },
    { test_realloc, "realloc" },
    { test_edges, "edges" },
    { 0 },
};
