/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The standard-name polyfills of the integer and text headers, forced
   even where the system has the names (PF__FORCE_STD_NAMES_POLYFILL is
   internal). The system headers come first and their names are removed,
   so the polyfill defines its own. */

#include "check.h"

#include <limits.h>
#include <stdint.h>

#undef htobe16
#undef htole16
#undef be16toh
#undef le16toh
#undef htobe32
#undef htole32
#undef be32toh
#undef le32toh
#undef htobe64
#undef htole64
#undef be64toh
#undef le64toh
#undef bswap_16
#undef bswap_32
#undef bswap_64

#define PF_STD_NAMES
#define PF__FORCE_STD_NAMES_POLYFILL
#include <cpolyfill/bitwise.h>
#include <cpolyfill/endian.h>
#include <cpolyfill/overflow.h>

#include <cpolyfill/errno.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/string.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>
#include <cpolyfill/unicode.h>

#include <errno.h>

#ifndef __cplusplus
    #define CHECK_STDBIT(sfx, type, bits)                                  \
        do {                                                               \
            type x = (type)v;                                              \
            PF_ASSERT_ALWAYS_EQ(stdc_leading_zeros##sfx(x), pf_clz##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_leading_ones##sfx(x), pf_clo##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_trailing_zeros##sfx(x), pf_ctz##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_trailing_ones##sfx(x), pf_cto##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(                                           \
                stdc_first_leading_zero##sfx(x), pf_flz##bits(x)           \
            );                                                             \
            PF_ASSERT_ALWAYS_EQ(                                           \
                stdc_first_leading_one##sfx(x), pf_flo##bits(x)            \
            );                                                             \
            PF_ASSERT_ALWAYS_EQ(                                           \
                stdc_first_trailing_zero##sfx(x), pf_ftz##bits(x)          \
            );                                                             \
            PF_ASSERT_ALWAYS_EQ(                                           \
                stdc_first_trailing_one##sfx(x), pf_fto##bits(x)           \
            );                                                             \
            PF_ASSERT_ALWAYS_EQ(stdc_count_zeros##sfx(x), pf_zerocount##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_count_ones##sfx(x), pf_popcount##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_has_single_bit##sfx(x), pf_ispow2##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_bit_width##sfx(x), pf_bitwidth##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(stdc_bit_floor##sfx(x), pf_pow2floor##bits(x)); \
            PF_ASSERT_ALWAYS_EQ(                                           \
                stdc_bit_ceil##sfx(x), x <= 1 ? 1 : pf_pow2ceil##bits(x)   \
            );                                                             \
        } while (0)
#endif

static int test_stdbit(int seed, int i) {
    unsigned state = (unsigned)seed + (unsigned)i;
    int k;
    (void)i;

    /* C23 semantics where they differ from pf_*. */
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil_ui(0), 1);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil_ui(1), 1);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil_uc(129), 0);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil_ull(ULLONG_MAX), 0);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil_us(1000), 1024);
    PF_ASSERT_ALWAYS(sizeof(stdc_bit_floor_uc(3)) == 1);
    PF_ASSERT_ALWAYS(sizeof(stdc_leading_zeros_uc(3)) == sizeof(unsigned));

    for (k = 0; k < 2000; k++) {
        unsigned long long v = ((unsigned long long)test_rand(&state) << 45)
            ^ ((unsigned long long)test_rand(&state) << 30)
            ^ ((unsigned long long)test_rand(&state) << 15)
            ^ test_rand(&state);
        v >>= test_rand(&state) % 64;
        CHECK_STDBIT(_uc, unsigned char, 8);
        CHECK_STDBIT(_us, unsigned short, 16);
        CHECK_STDBIT(_ui, unsigned int, );
        CHECK_STDBIT(_ul, unsigned long, l);
        CHECK_STDBIT(_ull, unsigned long long, ll);
    }

#if PF_HAS_BITWISE_GENERIC
    PF_ASSERT_ALWAYS_EQ(stdc_leading_zeros((unsigned char)1), 7);
    PF_ASSERT_ALWAYS_EQ(stdc_count_ones(0xFFFFull), 16);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil(0u), 1);
    PF_ASSERT_ALWAYS_EQ(stdc_bit_ceil((unsigned short)300), 512);
    PF_ASSERT_ALWAYS(sizeof(stdc_bit_ceil((unsigned char)3)) == 1);
    PF_ASSERT_ALWAYS(stdc_has_single_bit(8ul));
    PF_ASSERT_ALWAYS_EQ(stdc_first_trailing_one(12u), 3);
#endif

    PF_ASSERT_ALWAYS(__STDC_ENDIAN_LITTLE__ != __STDC_ENDIAN_BIG__);
    PF_ASSERT_ALWAYS_EQ(
        __STDC_ENDIAN_NATIVE__,
        PF_ENDIAN_LITTLE ? __STDC_ENDIAN_LITTLE__ : __STDC_ENDIAN_BIG__
    );
    return 0;
}

static int test_ckdint(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_CKD
    {
        int r = 0;
        unsigned short us = 0;
        PF_ASSERT_ALWAYS(ckd_add(&r, INT_MAX, 1));
        PF_ASSERT_ALWAYS_EQ(r, INT_MIN);
        PF_ASSERT_ALWAYS(!ckd_sub(&us, 70000, 5000) && us == 65000);
        PF_ASSERT_ALWAYS(ckd_mul(&us, 256, 256));
    }
#endif
    return 0;
}

static int test_endian(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(htobe16(0x1234), pf_htobe16(0x1234));
    PF_ASSERT_ALWAYS_EQ(htole16(0x1234), pf_htole16(0x1234));
    PF_ASSERT_ALWAYS_EQ(be16toh(0x1234), pf_be16toh(0x1234));
    PF_ASSERT_ALWAYS_EQ(le16toh(0x1234), pf_le16toh(0x1234));
    PF_ASSERT_ALWAYS_EQ(htobe32(0x12345678), pf_htobe32(0x12345678));
    PF_ASSERT_ALWAYS_EQ(le32toh(0x12345678), pf_le32toh(0x12345678));
    PF_ASSERT_ALWAYS_EQ(htobe64(1), pf_htobe64(1));
    PF_ASSERT_ALWAYS_EQ(le64toh(1), pf_le64toh(1));
    PF_ASSERT_ALWAYS_EQ(bswap_16(0x1234), 0x3412);
    PF_ASSERT_ALWAYS_EQ(bswap_32(0x11223344), 0x44332211);
    PF_ASSERT_ALWAYS_EQ(bswap_64(1), UINT64_C(1) << 56);
    return 0;
}

#define XSTR(x) STR(x)
#define STR(x) #x

static int cmp_char(const void *a, const void *b, void *arg) {
    (void)arg;
    return *(const char *)a - *(const char *)b;
}

static int test_text(int seed, int i) {
    static const char *const mapped[][2] = {
        { XSTR(strdup), "pf_strdup" },
        { XSTR(strndup), "pf_strndup" },
        { XSTR(strnlen), "pf_strnlen" },
        { XSTR(strlcpy), "pf_strlcpy" },
        { XSTR(strlcat), "pf_strlcat" },
        { XSTR(strcasecmp), "pf_strcasecmp" },
        { XSTR(strncasecmp), "pf_strncasecmp" },
        { XSTR(strtok_r), "pf_strtok_r" },
        { XSTR(strsep), "pf_strsep" },
        { XSTR(strchrnul), "pf_strchrnul" },
        { XSTR(memmem), "pf_memmem" },
        { XSTR(memrchr), "pf_memrchr" },
        { XSTR(mempcpy), "pf_mempcpy" },
        { XSTR(stpcpy), "pf_stpcpy" },
        { XSTR(stpncpy), "pf_stpncpy" },
        { XSTR(memccpy), "pf_memccpy" },
        { XSTR(memset_explicit), "pf_memset_explicit" },
        { XSTR(asprintf), "pf_asprintf" },
        { XSTR(vasprintf), "pf_vasprintf" },
        { XSTR(reallocarray), "pf_reallocarray" },
        { XSTR(getline), "pf_getline" },
        { XSTR(getdelim), "pf_getdelim" },
        { XSTR(setenv), "pf_setenv" },
        { XSTR(unsetenv), "pf_unsetenv" },
        { XSTR(qsort_r), "pf_qsort_r" },
        { XSTR(strerror_r), "pf_strerror_r" },
        { XSTR(mbrtoc8), "pf_mbrtoc8" },
        { XSTR(mbrtoc16), "pf_mbrtoc16" },
        { XSTR(mbrtoc32), "pf_mbrtoc32" },
        { XSTR(c8rtomb), "pf_c8rtomb" },
        { XSTR(c16rtomb), "pf_c16rtomb" },
        { XSTR(c32rtomb), "pf_c32rtomb" },
    };
    char buf[16], letters[] = "dcba", *s = NULL;
    char16_t c16 = 0;
    char32_t c32 = 0;
    mbstate_t st;
    size_t k;
    int rc;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(mapped) / sizeof(mapped[0]); k++)
        PF_ASSERT_ALWAYS_STR_EQ(mapped[k][0], mapped[k][1]);

    /* the standard signatures */
    PF_ASSERT_ALWAYS_EQ(strlcpy(buf, "hello", 4), 5);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "hel");
    PF_ASSERT_ALWAYS_EQ(asprintf(&s, "%d", 12), 2);
    PF_ASSERT_ALWAYS_STR_EQ(s, "12");
    free(s);
    qsort_r(letters, 4, 1, cmp_char, NULL);
    PF_ASSERT_ALWAYS_STR_EQ(letters, "abcd");
    rc = strerror_r(EINVAL, buf, sizeof(buf));
    PF_ASSERT_ALWAYS(rc == 0 || rc == ERANGE);
    {
        ssize_t n;
        size_t cap = 0;
        char *line = NULL;
        FILE *f = tmpfile();
        fputs("x\n", f);
        rewind(f);
        n = getline(&line, &cap, f);
        PF_ASSERT_ALWAYS_EQ(n, 2);
        free(line);
        fclose(f);
    }
    memset(&st, 0, sizeof(st));
    PF_ASSERT_ALWAYS_EQ(mbrtoc16(&c16, "A", 1, &st), 1);
    PF_ASSERT_ALWAYS_EQ(c16, 'A');
    PF_ASSERT_ALWAYS_EQ(mbrtoc32(&c32, "B", 1, &st), 1);
    PF_ASSERT_ALWAYS_EQ(c32, 'B');
    return 0;
}

static int test_time(int seed, int i) {
    static const char *const mapped[][2] = {
        { XSTR(clock_gettime), "pf_clock_gettime" },
        { XSTR(clock_getres), "pf_clock_getres" },
        { XSTR(nanosleep), "pf_nanosleep" },
        { XSTR(timespec_get), "pf_timespec_get" },
        { XSTR(timespec_getres), "pf_timespec_getres" },
        { XSTR(gmtime_r), "pf_gmtime_r" },
        { XSTR(localtime_r), "pf_localtime_r" },
        { XSTR(timegm), "pf_timegm" },
    };
    struct timespec ts;
    struct tm tm;
    time_t t = 86400;
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(mapped) / sizeof(mapped[0]); k++)
        PF_ASSERT_ALWAYS_STR_EQ(mapped[k][0], mapped[k][1]);
    PF_ASSERT_ALWAYS_EQ(CLOCK_MONOTONIC, PF_CLOCK_MONOTONIC);
    PF_ASSERT_ALWAYS_EQ(TIME_MONOTONIC, PF_TIME_MONOTONIC);

    PF_ASSERT_ALWAYS_EQ(clock_gettime(CLOCK_MONOTONIC, &ts), 0);
    PF_ASSERT_ALWAYS_EQ(timespec_get(&ts, TIME_UTC), TIME_UTC);
    PF_ASSERT_ALWAYS(gmtime_r(&t, &tm) == &tm);
    PF_ASSERT_ALWAYS_EQ(timegm(&tm), 86400);
    ts.tv_sec = 0;
    ts.tv_nsec = 1000;
    PF_ASSERT_ALWAYS_EQ(nanosleep(&ts, NULL), 0);
    return 0;
}

static int test_threads(int seed, int i) {
    static const char *const mapped[][2] = {
        { XSTR(thrd_create), "pf_thrd_create" },
        { XSTR(thrd_join), "pf_thrd_join" },
        { XSTR(mtx_timedlock), "pf_mtx_timedlock" },
        { XSTR(cnd_wait), "pf_cnd_wait" },
        { XSTR(tss_create), "pf_tss_create" },
        { XSTR(thrd_success), "pf_thrd_success" },
        { XSTR(mtx_recursive), "pf_mtx_recursive" },
    };
    mtx_t m;
    cnd_t c;
    tss_t key;
    thrd_t self = thrd_current();
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(mapped) / sizeof(mapped[0]); k++)
        PF_ASSERT_ALWAYS_STR_EQ(mapped[k][0], mapped[k][1]);

    PF_ASSERT_ALWAYS(thrd_equal(self, thrd_current()));
    PF_ASSERT_ALWAYS_EQ(mtx_init(&m, mtx_timed), thrd_success);
    PF_ASSERT_ALWAYS_EQ(cnd_init(&c), thrd_success);
    PF_ASSERT_ALWAYS_EQ(mtx_lock(&m), thrd_success);
    PF_ASSERT_ALWAYS_EQ(cnd_signal(&c), thrd_success);
    PF_ASSERT_ALWAYS_EQ(mtx_unlock(&m), thrd_success);
    cnd_destroy(&c);
    mtx_destroy(&m);
    PF_ASSERT_ALWAYS_EQ(tss_create(&key, NULL), thrd_success);
    PF_ASSERT_ALWAYS_EQ(tss_set(key, &key), thrd_success);
    PF_ASSERT_ALWAYS(tss_get(key) == &key);
    tss_delete(key);
    PF_ASSERT_ALWAYS_EQ(TSS_DTOR_ITERATIONS, PF_TSS_DTOR_ITERATIONS);
    return 0;
}

pf_test_t suite_std_polyfill[] = {
    { test_stdbit, "stdbit" },
    { test_ckdint, "ckdint" },
    { test_endian, "endian" },
    { test_text, "text" },
    { test_time, "time" },
    { test_threads, "threads" },
    { 0 },
};
