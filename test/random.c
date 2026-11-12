/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/random.h> */

#include "../src/internal.h"

#include <cpolyfill/random.h>

#include <errno.h>
#include <string.h>

#include "check.h"

/* Counts the set bits, to see that random bytes are not constant. */
static unsigned popcount_bytes(const unsigned char *p, size_t n) {
    unsigned c = 0;
    size_t k;
    for (k = 0; k < n; k++) {
        unsigned b = p[k];
        while (b) {
            c += b & 1u;
            b >>= 1;
        }
    }
    return c;
}

static int test_getrandom(int seed, int i) {
    static unsigned char a[4096], b[4096];
    unsigned bits;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_getrandom(NULL, 0), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_getrandom(NULL, 1), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_getrandom(a, sizeof(a)), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_getrandom(b, sizeof(b)), PF_OK);
    PF_ASSERT_ALWAYS(memcmp(a, b, sizeof(a)) != 0);
    /* 32768 bits: about half set, far within 5 standard deviations */
    bits = popcount_bytes(a, sizeof(a));
    PF_ASSERT_ALWAYS(bits > 16384 - 500 && bits < 16384 + 500);

    /* odd sizes and offsets */
    memset(a, 0, sizeof(a));
    PF_ASSERT_ALWAYS_EQ(pf_getrandom(a + 3, 5), PF_OK);
    PF_ASSERT_ALWAYS_EQ(a[0] | a[1] | a[2] | a[8], 0);
    return 0;
}

static int test_helpers(int seed, int i) {
    unsigned counts[6] = { 0 };
    uint64_t or64 = 0;
    int k;
    (void)seed, (void)i;

    for (k = 0; k < 64; k++)
        or64 |= pf_random_u64();
    PF_ASSERT_ALWAYS(or64 >> 63 && (uint32_t)or64 != 0);
    PF_ASSERT_ALWAYS(pf_random_u32() != pf_random_u32()
                     || pf_random_u32() != pf_random_u32());

    PF_ASSERT_ALWAYS_EQ(pf_random_uniform(0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_random_uniform(1), 0);
    for (k = 0; k < 6000; k++) {
        uint32_t r = pf_random_uniform(6);
        PF_ASSERT_ALWAYS_LT(r, 6);
        counts[r]++;
    }
    for (k = 0; k < 6; k++)
        PF_ASSERT_ALWAYS(counts[k] > 800 && counts[k] < 1200);
    PF_ASSERT_ALWAYS_LT(pf_random_uniform(0x80000001u), 0x80000001u);
    return 0;
}

static int test_bsd(int seed, int i) {
    unsigned char buf[257];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_getentropy(buf, 256), 0);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_getentropy(buf, 257), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_getentropy(buf, 0), 0);

    memset(buf, 0, sizeof(buf));
    pf_arc4random_buf(buf, sizeof(buf));
    PF_ASSERT_ALWAYS(popcount_bytes(buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_LT(pf_arc4random_uniform(10), 10);
    (void)pf_arc4random();
    return 0;
}

static int test_rng(int seed, int i) {
    pf_rng_t a, b;
    double sum = 0;
    int k;
    (void)i;

    /* reference values of xoshiro256** seeded through splitmix64(0) */
    pf_rng_seed(&a, 0);
    PF_ASSERT_ALWAYS_EQ(a.s[0], 0xE220A8397B1DCDAFull);
    PF_ASSERT_ALWAYS_EQ(a.s[1], 0x6E789E6AA1B965F4ull);
    PF_ASSERT_ALWAYS_EQ(a.s[2], 0x06C45D188009454Full);
    PF_ASSERT_ALWAYS_EQ(a.s[3], 0xF88BB8A8724C81ECull);
    pf_rng_seed(&a, 0);
    PF_ASSERT_ALWAYS_EQ(pf_rng_u64(&a), 0x99EC5F36CB75F2B4ull);

    /* the same seed, the same sequence */
    pf_rng_seed(&a, (uint64_t)seed);
    pf_rng_seed(&b, (uint64_t)seed);
    for (k = 0; k < 100; k++)
        PF_ASSERT_ALWAYS_EQ(pf_rng_u64(&a), pf_rng_u64(&b));
    PF_ASSERT_ALWAYS_EQ(pf_rng_u32(&a), pf_rng_u32(&b));

    PF_ASSERT_ALWAYS_EQ(pf_rng_uniform(&a, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rng_uniform(&a, 1), 0);
    for (k = 0; k < 1000; k++) {
        double d = pf_rng_double(&a);
        PF_ASSERT_ALWAYS(d >= 0.0 && d < 1.0);
        sum += d;
        PF_ASSERT_ALWAYS_LT(pf_rng_uniform(&a, 7), 7);
        PF_ASSERT_ALWAYS_LT(
            pf_rng_uniform(&a, 0x8000000000000001ull), 0x8000000000000001ull
        );
    }
    PF_ASSERT_ALWAYS(sum > 400 && sum < 600);

    PF_ASSERT_ALWAYS_EQ(pf_rng_seed_random(&a), PF_OK);
    PF_ASSERT_ALWAYS_EQ(pf_rng_seed_random(&b), PF_OK);
    PF_ASSERT_ALWAYS(pf_rng_u64(&a) != pf_rng_u64(&b));
    return 0;
}

pf_test_t suite_random[] = {
    { test_getrandom, "getrandom" },
    { test_helpers, "helpers" },
    { test_bsd, "bsd" },
    { test_rng, "rng" },
    { 0 },
};
