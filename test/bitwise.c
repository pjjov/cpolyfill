/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#define BITWISE_OPS_NAME bitwise_fast
#include "bitwise_ops.h"

#include <limits.h>

#include "check.h"

uint64_t bitwise_portable(int type, int op, uint64_t v, unsigned n);

static const unsigned bit_width[T_COUNT] = {
    sizeof(unsigned int) * CHAR_BIT,
    sizeof(unsigned long) * CHAR_BIT,
    sizeof(unsigned long long) * CHAR_BIT,
    8,
    16,
    32,
    64,
    sizeof(size_t) * CHAR_BIT,
    sizeof(uintptr_t) * CHAR_BIT,
    sizeof(uintmax_t) * CHAR_BIT,
};

static const char *const op_name[OP_COUNT] = {
    "clz",      "ctz",       "clo",    "cto",      "flz",
    "flo",      "ftz",       "fto",    "popcount", "zerocount",
    "parity",   "ispow2",    "pow2ceil", "pow2floor", "bitwidth",
    "bitreverse", "rotl",    "rotr",
};

static uint64_t mask_of(unsigned w) {
    return w >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << w) - 1;
}

static uint64_t bit(uint64_t x, unsigned i) { return (x >> i) & 1u; }

/* The documented semantics, bit by bit. */
static uint64_t reference(unsigned w, int op, uint64_t x, unsigned n) {
    uint64_t m = mask_of(w), r = 0;
    unsigned i;
    x &= m;
    switch (op) {
    case OP_CLZ:
        for (i = w; i > 0 && !bit(x, i - 1); i--)
            r++;
        return r;
    case OP_CTZ:
        for (i = 0; i < w && !bit(x, i); i++)
            r++;
        return r;
    case OP_CLO: return reference(w, OP_CLZ, ~x, 0);
    case OP_CTO: return reference(w, OP_CTZ, ~x, 0);
    case OP_FLZ:
        for (i = w; i > 0; i--)
            if (!bit(x, i - 1))
                return w - i + 1;
        return 0;
    case OP_FLO:
        for (i = w; i > 0; i--)
            if (bit(x, i - 1))
                return w - i + 1;
        return 0;
    case OP_FTZ:
        for (i = 0; i < w; i++)
            if (!bit(x, i))
                return i + 1;
        return 0;
    case OP_FTO:
        for (i = 0; i < w; i++)
            if (bit(x, i))
                return i + 1;
        return 0;
    case OP_POPCOUNT:
        for (i = 0; i < w; i++)
            r += bit(x, i);
        return r;
    case OP_ZEROCOUNT: return w - reference(w, OP_POPCOUNT, x, 0);
    case OP_PARITY: return reference(w, OP_POPCOUNT, x, 0) & 1u;
    case OP_ISPOW2: return reference(w, OP_POPCOUNT, x, 0) == 1;
    case OP_POW2CEIL:
        for (i = 0; i < w; i++)
            if (((uint64_t)1 << i) >= x && x != 0)
                return (uint64_t)1 << i;
        return 0;
    case OP_POW2FLOOR:
        for (i = w; i > 0; i--)
            if (bit(x, i - 1))
                return (uint64_t)1 << (i - 1);
        return 0;
    case OP_BITWIDTH: return w - reference(w, OP_CLZ, x, 0);
    case OP_BITREVERSE:
        for (i = 0; i < w; i++)
            r |= bit(x, i) << (w - 1 - i);
        return r;
    case OP_ROTL:
        for (i = 0; i < w; i++)
            r |= bit(x, i) << ((i + n) % w);
        return r;
    case OP_ROTR:
        for (i = 0; i < w; i++)
            r |= bit(x, (i + n) % w) << i;
        return r;
    }
    return 0;
}

/* Checks one value with every operation, both implementations. */
static void check_value(int type, uint64_t x, unsigned n) {
    unsigned w = bit_width[type];
    int op;
    x &= mask_of(w);
    for (op = 0; op < OP_COUNT; op++) {
        uint64_t want = reference(w, op, x, n);
        uint64_t fast = bitwise_fast(type, op, x, n);
        uint64_t slow = bitwise_portable(type, op, x, n);
        if (fast != want || slow != want) {
            fprintf(
                stderr,
                "%s, %u bits, x = 0x%llx, n = %u: expected 0x%llx, builtin "
                "0x%llx, portable 0x%llx\n",
                op_name[op],
                w,
                (unsigned long long)x,
                n,
                (unsigned long long)want,
                (unsigned long long)fast,
                (unsigned long long)slow
            );
            PF_ASSERT_ALWAYS_EQ(fast, want);
            PF_ASSERT_ALWAYS_EQ(slow, want);
        }
    }
}

static int test_exhaustive(int seed, int i) {
    uint64_t x;
    (void)seed, (void)i;
    for (x = 0; x < 256; x++)
        check_value(T_8, x, (unsigned)x % 19);
    for (x = 0; x < 65536; x++)
        check_value(T_16, x, (unsigned)x % 37);
    return 0;
}

/* Edge patterns: 0, all ones, every single bit, its neighbours and its
   complement. */
static int test_patterns(int seed, int i) {
    int type;
    (void)seed, (void)i;
    for (type = 0; type < T_COUNT; type++) {
        unsigned w = bit_width[type], b, n;
        check_value(type, 0, 0);
        check_value(type, ~(uint64_t)0, 1);
        for (b = 0; b < w; b++) {
            uint64_t one = (uint64_t)1 << b;
            check_value(type, one, b);
            check_value(type, one - 1, b + 1);
            check_value(type, one + 1, w - b);
            check_value(type, ~one, b * 3);
            check_value(type, one | (one >> 1), w + b);
        }
        for (n = 0; n < 3 * w; n++)
            check_value(type, 0x8000000000000001ull | 0x0123456789ull, n);
    }
    return 0;
}

static int test_random(int seed, int i) {
    unsigned state = (unsigned)seed + (unsigned)i * 7919u;
    int type, k;
    for (type = 0; type < T_COUNT; type++) {
        for (k = 0; k < 2000; k++) {
            uint64_t x = 0;
            int j;
            for (j = 0; j < 5; j++)
                x = (x << 15) ^ test_rand(&state);
            /* Sparse and dense values too. */
            if (k % 3 == 1)
                x &= (uint64_t)test_rand(&state) << (test_rand(&state) % 48);
            else if (k % 3 == 2)
                x |= ~((uint64_t)test_rand(&state) << (test_rand(&state) % 48));
            check_value(type, x, test_rand(&state) % 200);
        }
    }
    return 0;
}

#if PF_HAS_BITWISE_GENERIC
/* Two expressions of the same type, by size and signedness. */
    #define SAME_TYPE(a, b) \
        (sizeof(a) == sizeof(b) && ((a) * 0 - 1 > 0) == ((b) * 0 - 1 > 0))
#endif

static int test_generic(int seed, int i) {
    unsigned char uc = 0x30;
    unsigned short us = 0x0300;
    unsigned int ui = 0x30000u;
    unsigned long ul = 0x300000ul;
    unsigned long long ull = 0x3000000000ull;
    (void)seed, (void)i;
    (void)uc, (void)us, (void)ui, (void)ul, (void)ull;
#if PF_HAS_BITWISE_GENERIC
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(uc), 2);
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(us), 6);
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(ui), pf_clz(ui));
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(ul), pf_clzl(ul));
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(ull), pf_clzll(ull));
    PF_ASSERT_ALWAYS_EQ(PF_CTZ(uc), 4);
    PF_ASSERT_ALWAYS_EQ(PF_CTZ(ull), 36);
    PF_ASSERT_ALWAYS_EQ(PF_CLO((unsigned char)0xF0), 4);
    PF_ASSERT_ALWAYS_EQ(PF_CTO(7u), 3);
    PF_ASSERT_ALWAYS_EQ(PF_FLZ((unsigned char)0xF0), 5);
    PF_ASSERT_ALWAYS_EQ(PF_FLO(us), 7);
    PF_ASSERT_ALWAYS_EQ(PF_FTZ(7ul), 4);
    PF_ASSERT_ALWAYS_EQ(PF_FTO(uc), 5);
    PF_ASSERT_ALWAYS_EQ(PF_POPCOUNT(ull), 2);
    PF_ASSERT_ALWAYS_EQ(PF_ZEROCOUNT(uc), 6);
    PF_ASSERT_ALWAYS_EQ(PF_PARITY(us), 0);
    PF_ASSERT_ALWAYS_EQ(PF_ISPOW2((unsigned short)256), 1);
    PF_ASSERT_ALWAYS_EQ(PF_BITWIDTH(uc), 6);
    PF_ASSERT_ALWAYS_EQ(PF_POW2CEIL(uc), 0x40);
    PF_ASSERT_ALWAYS_EQ(PF_POW2CEIL((unsigned char)0x81), 0);
    PF_ASSERT_ALWAYS_EQ(PF_POW2FLOOR(ul), 0x200000ul);
    PF_ASSERT_ALWAYS_EQ(PF_BITREVERSE(uc), 0x0C);
    PF_ASSERT_ALWAYS_EQ(PF_ROTL(uc, 4), 0x03);
    PF_ASSERT_ALWAYS_EQ(PF_ROTR(us, 12), 0x3000);

    /* The value-returning macros keep the argument type. */
    PF_ASSERT_ALWAYS(SAME_TYPE(PF_POW2CEIL(uc), uc));
    PF_ASSERT_ALWAYS(SAME_TYPE(PF_BITREVERSE(us), us));
    PF_ASSERT_ALWAYS(SAME_TYPE(PF_ROTL(ull, 1), ull));
    PF_ASSERT_ALWAYS(SAME_TYPE(PF_CLZ(ull), 0));
#endif
    return 0;
}

/* A user type through PF_IMPL_BITWISE. */
PF_IMPL_BITWISE(_ushort_test, unsigned short, USHRT_MAX)

static int test_impl(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(
        pf_clz_ushort_test(1), (int)(sizeof(unsigned short) * CHAR_BIT) - 1
    );
    PF_ASSERT_ALWAYS_EQ(pf_pow2ceil_ushort_test(3), 4);
    PF_ASSERT_ALWAYS_EQ(pf_pow2ceil_ushort_test(USHRT_MAX), 0);
    return 0;
}

pf_test_t suite_bitwise[] = {
    { test_exhaustive, "exhaustive" },
    { test_patterns, "patterns" },
    { test_random, "random" },
    { test_generic, "generic" },
    { test_impl, "impl" },
    { 0 },
};
