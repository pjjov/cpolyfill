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

#include "../include/pf_assert.h"
#include "../include/pf_endian.h"
#include "../include/pf_test.h"
#include <stdint.h>
#include <string.h>

int test_bswap16(int seed, int rep) {
    pf_assert(0x0000ull == pf_bswap16(0x0000ull));
    pf_assert(0x3412ull == pf_bswap16(0x1234ull));
    pf_assert(0x7856ull == pf_bswap16((uint16_t)0x12345678ull));
    pf_assert(0xFFFFull == pf_bswap16(0xFFFFull));
    return 0;
}

int test_bswap32(int seed, int rep) {
    pf_assert(0x00000000ull == pf_bswap32(0x00000000ull));
    pf_assert(0x78563412ull == pf_bswap32(0x12345678ull));
    pf_assert(0x78563412ull == pf_bswap32((uint32_t)0x912345678ull));
    pf_assert(0xFFFFFFFFull == pf_bswap32(0xFFFFFFFFull));
    return 0;
}

int test_bswap64(int seed, int rep) {
    pf_assert(0x0000000000000000ull == pf_bswap64(0x0000000000000000ull));
    pf_assert(0xF0DEBC9A78563412ull == pf_bswap64(0x123456789ABCDEF0ull));
    pf_assert(0xFFFFFFFFFFFFFFFFull == pf_bswap64(0xFFFFFFFFFFFFFFFFull));
    return 0;
}

int test_htobe(int seed, int rep) {
    if (PF_BIG_ENDIAN) {
        pf_assert(0x1234ull == htobe16(0x1234ull));
        pf_assert(0x12345678ull == htobe32(0x12345678ull));
        pf_assert(0x123456789ABCDEF0ull == htobe64(0x123456789ABCDEF0ull));
    } else {
        pf_assert(0x3412ull == htobe16(0x1234ull));
        pf_assert(0x78563412ull == htobe32(0x12345678ull));
        pf_assert(0xF0DEBC9A78563412ull == htobe64(0x123456789ABCDEF0ull));
    }
    return 0;
}

int test_htole(int seed, int rep) {
    if (PF_LITTLE_ENDIAN) {
        pf_assert(0x1234ull == htole16(0x1234ull));
        pf_assert(0x12345678ull == htole32(0x12345678ull));
        pf_assert(0x123456789ABCDEF0ull == htole64(0x123456789ABCDEF0ull));
    } else {
        pf_assert(0x3412ull == htole16(0x1234ull));
        pf_assert(0x78563412ull == htole32(0x12345678ull));
        pf_assert(0xF0DEBC9A78563412ull == htole64(0x123456789ABCDEF0ull));
    }
    return 0;
}

int test_betoh(int seed, int rep) {
    if (PF_BIG_ENDIAN) {
        pf_assert(0x1234ull == be16toh(0x1234ull));
        pf_assert(0x12345678ull == be32toh(0x12345678ull));
        pf_assert(0x123456789ABCDEF0ull == be64toh(0x123456789ABCDEF0ull));
    } else {
        pf_assert(0x3412ull == be16toh(0x1234ull));
        pf_assert(0x78563412ull == be32toh(0x12345678ull));
        pf_assert(0xF0DEBC9A78563412ull == be64toh(0x123456789ABCDEF0ull));
    }
    return 0;
}

int test_letoh(int seed, int rep) {
    if (PF_LITTLE_ENDIAN) {
        pf_assert(0x1234ull == le16toh(0x1234ull));
        pf_assert(0x12345678ull == le32toh(0x12345678ull));
        pf_assert(0x123456789ABCDEF0ull == le64toh(0x123456789ABCDEF0ull));
    } else {
        pf_assert(0x3412ull == le16toh(0x1234ull));
        pf_assert(0x78563412ull == le32toh(0x12345678ull));
        pf_assert(0xF0DEBC9A78563412ull == le64toh(0x123456789ABCDEF0ull));
    }
    return 0;
}

int test_endian_detection(int seed, int rep) {
    union {
        unsigned int i;
        unsigned char b[sizeof(unsigned int)];
    } probe = { 1 };

    /* Exactly one of the two holds, and it is the one of this machine. */
    pf_assert((!!PF_BIG_ENDIAN) != (!!PF_LITTLE_ENDIAN));
    pf_assert(!!PF_LITTLE_ENDIAN == (probe.b[0] == 1));
    pf_assert(!!PF_BIG_ENDIAN == (probe.b[sizeof probe.b - 1] == 1));
    return 0;
}

int test_endian_bswap_involution(int seed, int rep) {
    uint64_t x = 0x9E3779B97F4A7C15ull * (uint64_t)(seed + rep + 1);

    for (int i = 0; i < 1000; i++) {
        x = x * 6364136223846793005ull + 1442695040888963407ull;
        pf_assert(pf_bswap16(pf_bswap16((uint16_t)x)) == (uint16_t)x);
        pf_assert(pf_bswap32(pf_bswap32((uint32_t)x)) == (uint32_t)x);
        pf_assert(pf_bswap64(pf_bswap64(x)) == x);

        /* Swapping a wide value swaps its halves as well. */
        pf_assert(
            pf_bswap64(x)
            == (((uint64_t)pf_bswap32((uint32_t)x) << 32)
                | pf_bswap32((uint32_t)(x >> 32)))
        );
    }
    return 0;
}

int test_endian_memory_layout(int seed, int rep) {
    uint16_t v16;
    uint32_t v32;
    uint64_t v64;
    unsigned char b[8];

    v16 = htobe16(0x1234);
    memcpy(b, &v16, 2);
    pf_assert(b[0] == 0x12 && b[1] == 0x34);
    v16 = htole16(0x1234);
    memcpy(b, &v16, 2);
    pf_assert(b[0] == 0x34 && b[1] == 0x12);

    v32 = htobe32(0x12345678u);
    memcpy(b, &v32, 4);
    pf_assert(b[0] == 0x12 && b[1] == 0x34 && b[2] == 0x56 && b[3] == 0x78);
    v32 = htole32(0x12345678u);
    memcpy(b, &v32, 4);
    pf_assert(b[0] == 0x78 && b[1] == 0x56 && b[2] == 0x34 && b[3] == 0x12);

    v64 = htobe64(0x0102030405060708ull);
    memcpy(b, &v64, 8);
    for (int i = 0; i < 8; i++)
        pf_assert(b[i] == i + 1);
    v64 = htole64(0x0102030405060708ull);
    memcpy(b, &v64, 8);
    for (int i = 0; i < 8; i++)
        pf_assert(b[i] == 8 - i);
    return 0;
}

int test_endian_roundtrip(int seed, int rep) {
    uint64_t x = 0xD1B54A32D192ED03ull * (uint64_t)(seed + rep + 1);

    for (int i = 0; i < 1000; i++) {
        x = x * 6364136223846793005ull + 1442695040888963407ull;
        pf_assert(be16toh(htobe16((uint16_t)x)) == (uint16_t)x);
        pf_assert(le16toh(htole16((uint16_t)x)) == (uint16_t)x);
        pf_assert(be32toh(htobe32((uint32_t)x)) == (uint32_t)x);
        pf_assert(le32toh(htole32((uint32_t)x)) == (uint32_t)x);
        pf_assert(be64toh(htobe64(x)) == x);
        pf_assert(le64toh(htole64(x)) == x);

        /* The two orders are each other's reverse. */
        pf_assert(htobe32((uint32_t)x) == pf_bswap32(htole32((uint32_t)x)));
    }
    return 0;
}

int test_endian_widths(int seed, int rep) {
    /* The values keep the width of their type, no promotion to int. */
    pf_assert(sizeof(htobe16(1)) == 2 || sizeof(htobe16(1)) == sizeof(int));
    pf_assert(htobe16(0xFFFF) == 0xFFFF);
    pf_assert(htobe32(0xFFFFFFFFu) == 0xFFFFFFFFu);
    pf_assert(htole64(0xFFFFFFFFFFFFFFFFull) == 0xFFFFFFFFFFFFFFFFull);
    pf_assert(htobe16(0) == 0 && htole32(0) == 0 && htobe64(0) == 0);
    return 0;
}

int test_endian_generic(int seed, int rep) {
    unsigned char in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, out[8];
    unsigned char expect_be[8], expect_le[8];
    static const size_t sizes[] = { 2, 4, 8 };

    for (size_t s = 0; s < 3; s++) {
        size_t n = sizes[s];

        for (size_t i = 0; i < n; i++) {
            expect_be[i] = PF_BIG_ENDIAN ? in[i] : in[n - 1 - i];
            expect_le[i] = PF_BIG_ENDIAN ? in[n - 1 - i] : in[i];
        }

        memset(out, 0, sizeof out);
        htobex(out, in, n);
        pf_assert(memcmp(out, expect_be, n) == 0);

        memset(out, 0, sizeof out);
        htolex(out, in, n);
        pf_assert(memcmp(out, expect_le, n) == 0);

        memset(out, 0, sizeof out);
        bextoh(out, in, n);
        pf_assert(memcmp(out, expect_be, n) == 0);

        memset(out, 0, sizeof out);
        lextoh(out, in, n);
        pf_assert(memcmp(out, expect_le, n) == 0);
    }
    return 0;
}

pf_test_t suite_endian[] = {
    { &test_bswap16, "/endian/bswap16" }, { &test_bswap32, "/endian/bswap32" },
    { &test_bswap64, "/endian/bswap64" }, { &test_htobe, "/endian/htobe" },
    { &test_htole, "/endian/htole" },     { &test_betoh, "/endian/betoh" },
    { &test_letoh, "/endian/letoh" },
    { &test_endian_detection, "/endian/detection" },
    { &test_endian_bswap_involution, "/endian/bswap_involution" },
    { &test_endian_memory_layout, "/endian/memory_layout" },
    { &test_endian_roundtrip, "/endian/roundtrip" },
    { &test_endian_widths, "/endian/widths" },
    { &test_endian_generic, "/endian/generic" },
    { 0 },
};
