/*
    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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

pf_test suite_endian[] = {
    { &test_bswap16, "/endian/bswap16", 1 },
    { &test_bswap32, "/endian/bswap32", 1 },
    { &test_bswap64, "/endian/bswap64", 1 },
    { &test_htobe, "/endian/htobe", 1 },
    { &test_htole, "/endian/htole", 1 },
    { &test_betoh, "/endian/betoh", 1 },
    { &test_letoh, "/endian/letoh", 1 },
    { 0 },
};
