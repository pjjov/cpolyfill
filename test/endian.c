/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/endian.h>
#include <cpolyfill/stdc.h>

#include "check.h"

/* The macros are constant expressions. */
PF_STATIC_ASSERT(PF_BSWAP16(0x1234) == 0x3412);
PF_STATIC_ASSERT(PF_BSWAP32(0x11223344) == 0x44332211);
PF_STATIC_ASSERT(
    PF_BSWAP64(0x0102030405060708) == UINT64_C(0x0807060504030201)
);
PF_STATIC_ASSERT(PF_ENDIAN_LITTLE + PF_ENDIAN_BIG == 1);

static const uint32_t table[] = { PF_BSWAP32(1), PF_BSWAP32(0xFF000000) };

static int test_byte_order(int seed, int i) {
    const uint32_t one = 1;
    unsigned char first;
    (void)seed, (void)i;

    memcpy(&first, &one, 1);
    PF_ASSERT_ALWAYS_EQ(first, PF_ENDIAN_LITTLE ? 1 : 0);
#if PF_ENDIAN_LITTLE
    PF_ASSERT_ALWAYS_EQ(pf_htole32(5), 5);
    PF_ASSERT_ALWAYS_EQ(pf_htobe32(5), 0x05000000);
#elif PF_ENDIAN_BIG
    PF_ASSERT_ALWAYS_EQ(pf_htobe32(5), 5);
    PF_ASSERT_ALWAYS_EQ(pf_htole32(5), 0x05000000);
#endif
    return 0;
}

static int test_bswap(int seed, int i) {
    unsigned state = (unsigned)seed + (unsigned)i;
    int k;
    (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_bswap16(0x1234), 0x3412);
    PF_ASSERT_ALWAYS_EQ(pf_bswap32(0x11223344), 0x44332211);
    PF_ASSERT_ALWAYS_EQ(
        pf_bswap64(UINT64_C(0x0102030405060708)), UINT64_C(0x0807060504030201)
    );
    PF_ASSERT_ALWAYS_EQ(table[0], 0x01000000);
    PF_ASSERT_ALWAYS_EQ(table[1], 0xFF);

    for (k = 0; k < 1000; k++) {
        uint64_t x = ((uint64_t)test_rand(&state) << 49)
            ^ ((uint64_t)test_rand(&state) << 34)
            ^ ((uint64_t)test_rand(&state) << 19)
            ^ ((uint64_t)test_rand(&state) << 4) ^ test_rand(&state);
        PF_ASSERT_ALWAYS_EQ(pf_bswap16((uint16_t)x), PF_BSWAP16(x));
        PF_ASSERT_ALWAYS_EQ(pf_bswap32((uint32_t)x), PF_BSWAP32(x));
        PF_ASSERT_ALWAYS_EQ(pf_bswap64(x), PF_BSWAP64(x));
        PF_ASSERT_ALWAYS_EQ(pf_bswap64(pf_bswap64(x)), x);
        PF_ASSERT_ALWAYS_EQ(pf_be16toh(pf_htobe16((uint16_t)x)), (uint16_t)x);
        PF_ASSERT_ALWAYS_EQ(pf_le32toh(pf_htole32((uint32_t)x)), (uint32_t)x);
        PF_ASSERT_ALWAYS_EQ(pf_be64toh(pf_htobe64(x)), x);
        PF_ASSERT_ALWAYS_EQ(pf_le64toh(pf_htole64(x)), x);
    }
    return 0;
}

static int test_load_store(int seed, int i) {
    static const unsigned char bytes[] = { 0xEE, 0x01, 0x02, 0x03, 0x04,
                                           0x05, 0x06, 0x07, 0x08, 0xEE };
    unsigned char buf[16];
    size_t off;
    (void)seed, (void)i;

    /* Odd offsets: the pointers are not aligned. */
    PF_ASSERT_ALWAYS_EQ(pf_load_le16(bytes + 1), 0x0201);
    PF_ASSERT_ALWAYS_EQ(pf_load_be16(bytes + 1), 0x0102);
    PF_ASSERT_ALWAYS_EQ(pf_load_le32(bytes + 1), 0x04030201);
    PF_ASSERT_ALWAYS_EQ(pf_load_be32(bytes + 1), 0x01020304);
    PF_ASSERT_ALWAYS_EQ(pf_load_le64(bytes + 1), UINT64_C(0x0807060504030201));
    PF_ASSERT_ALWAYS_EQ(pf_load_be64(bytes + 1), UINT64_C(0x0102030405060708));
    PF_ASSERT_ALWAYS_EQ(
        pf_load32(bytes + 1), PF_ENDIAN_LITTLE ? 0x04030201u : 0x01020304u
    );

    for (off = 1; off < 4; off++) {
        memset(buf, 0xAA, sizeof(buf));
        pf_store_be64(buf + off, UINT64_C(0x0102030405060708));
        PF_ASSERT_ALWAYS_MEM_EQ(buf + off, bytes + 1, 8);
        PF_ASSERT_ALWAYS_EQ(buf[off - 1], 0xAA);
        PF_ASSERT_ALWAYS_EQ(buf[off + 8], 0xAA);

        memset(buf, 0xAA, sizeof(buf));
        pf_store_le32(buf + off, 0x04030201);
        PF_ASSERT_ALWAYS_MEM_EQ(buf + off, bytes + 1, 4);
        pf_store_be16(buf + off, 0x0102);
        PF_ASSERT_ALWAYS_MEM_EQ(buf + off, bytes + 1, 2);
        pf_store_le16(buf + off, 0x0201);
        PF_ASSERT_ALWAYS_MEM_EQ(buf + off, bytes + 1, 2);
        pf_store_be32(buf + off, 0x01020304);
        PF_ASSERT_ALWAYS_MEM_EQ(buf + off, bytes + 1, 4);
        pf_store_le64(buf + off, UINT64_C(0x0807060504030201));
        PF_ASSERT_ALWAYS_MEM_EQ(buf + off, bytes + 1, 8);

        pf_store16(buf + off, 0xBEEF);
        PF_ASSERT_ALWAYS_EQ(pf_load16(buf + off), 0xBEEF);
        pf_store32(buf + off, 0xDEADBEEF);
        PF_ASSERT_ALWAYS_EQ(pf_load32(buf + off), 0xDEADBEEF);
        pf_store64(buf + off, UINT64_C(0x0123456789ABCDEF));
        PF_ASSERT_ALWAYS_EQ(pf_load64(buf + off), UINT64_C(0x0123456789ABCDEF));
    }
    return 0;
}

static int test_memreverse(int seed, int i) {
    unsigned char buf[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    static const unsigned char rev8[] = { 8, 7, 6, 5, 4, 3, 2, 1, 9 };
    static const unsigned char rev9[] = { 9, 1, 2, 3, 4, 5, 6, 7, 8 };
    (void)seed, (void)i;

    pf_memreverse(buf, 0);
    pf_memreverse(buf, 1);
    PF_ASSERT_ALWAYS_EQ(buf[0], 1);
    pf_memreverse(buf, 8);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, rev8, 9);
    pf_memreverse(buf, 8);
    pf_memreverse(buf, 9);
    pf_memreverse(buf + 1, 8);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, rev9, 9);
    return 0;
}

pf_test_t suite_endian[] = {
    { test_byte_order, "byte_order" },
    { test_bswap, "bswap" },
    { test_load_store, "load_store" },
    { test_memreverse, "memreverse" },
    { 0 },
};
