/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The standard-name polyfills of the integer headers, forced even where
   the system has the names (PF__FORCE_STD_NAMES_POLYFILL is internal).
   The system headers come first and their names are removed, so the
   polyfill defines its own. */

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

pf_test_t suite_std_polyfill[] = {
    { test_stdbit, "stdbit" },
    { test_ckdint, "ckdint" },
    { test_endian, "endian" },
    { 0 },
};
