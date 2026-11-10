/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#define OVERFLOW_OPS_NAME overflow_fast
#include "overflow_ops.h"

#include <limits.h>
#include <setjmp.h>

#include "check.h"

int overflow_portable(
    int type, int fam, int op, uint64_t a, uint64_t b, uint64_t *out
);

#define IS_SIGNED(type) ((type) < T_UINT)

static const unsigned ovf_width[T_COUNT] = {
    sizeof(int) * CHAR_BIT,
    sizeof(long) * CHAR_BIT,
    sizeof(long long) * CHAR_BIT,
    8,
    16,
    32,
    64,
    sizeof(intmax_t) * CHAR_BIT,
    sizeof(intptr_t) * CHAR_BIT,
    sizeof(ptrdiff_t) * CHAR_BIT,
    sizeof(unsigned int) * CHAR_BIT,
    sizeof(unsigned long) * CHAR_BIT,
    sizeof(unsigned long long) * CHAR_BIT,
    8,
    16,
    32,
    64,
    sizeof(uintmax_t) * CHAR_BIT,
    sizeof(uintptr_t) * CHAR_BIT,
    sizeof(size_t) * CHAR_BIT,
};

static const char *const op_name[OP_COUNT] = { "add", "sub", "mul" };

/* A value of the type from random bits: sign-extended for signed types,
   zero-extended for unsigned ones. */
static uint64_t normalize(int type, uint64_t v) {
    unsigned w = ovf_width[type];
    if (w >= 64)
        return v;
    v &= ((uint64_t)1 << w) - 1;
    if (IS_SIGNED(type) && (v >> (w - 1)))
        v |= ~(uint64_t)0 << w;
    return v;
}

#ifdef __SIZEOF_INT128__
    #define HAVE_REFERENCE 1

/* The exact result, the wrapped one, and whether it overflowed. */
static int reference(
    int type, int op, uint64_t ua, uint64_t ub, uint64_t *wrapped,
    uint64_t *saturated
) {
    unsigned w = ovf_width[type];
    __int128 a, b, r, min, max;
    int over;
    if (IS_SIGNED(type)) {
        a = (int64_t)ua;
        b = (int64_t)ub;
        max = ((__int128)1 << (w - 1)) - 1;
        min = -max - 1;
    } else {
        a = ua;
        b = ub;
        max = ((__int128)1 << w) - 1;
        min = 0;
    }
    if (op == OP_ADD)
        r = a + b;
    else if (op == OP_SUB)
        r = a - b;
    else if (!IS_SIGNED(type) && w == 64)
        /* Does not fit in __int128 when it overflows. */
        r = (ua != 0 && ub > UINT64_MAX / ua)
            ? max + 1
            : (__int128)((unsigned __int128)ua * ub);
    else
        r = a * b;
    over = r < min || r > max;
    if (!IS_SIGNED(type) && w == 64 && op == OP_MUL)
        *wrapped = ua * ub;
    else
        *wrapped = normalize(type, (uint64_t)(unsigned __int128)r);
    *saturated = (uint64_t)(r > max ? max : r < min ? min : r);
    return over;
}
#else
    #define HAVE_REFERENCE 0
#endif

static void check_pair(int type, uint64_t a, uint64_t b) {
    int op;
    a = normalize(type, a);
    b = normalize(type, b);
    for (op = 0; op < OP_COUNT; op++) {
        uint64_t fw = 0, sw = 0, fs = 0, ss = 0;
        int fo = overflow_fast(type, FAM_OVERFLOW, op, a, b, &fw);
        int so = overflow_portable(type, FAM_OVERFLOW, op, a, b, &sw);
        overflow_fast(type, FAM_SATURATED, op, a, b, &fs);
        overflow_portable(type, FAM_SATURATED, op, a, b, &ss);
#if HAVE_REFERENCE
        uint64_t rw = 0, rs = 0;
        int ro = reference(type, op, a, b, &rw, &rs);
#else
        /* Without a reference, the builtins and the portable code must
           at least agree. */
        uint64_t rw = sw, rs = ss;
        int ro = so;
#endif
        if (fo != ro || so != ro || fw != rw || sw != rw || fs != rs
            || ss != rs) {
            fprintf(
                stderr,
                "%s%s, %u bits, a = 0x%llx, b = 0x%llx: expected %d 0x%llx "
                "(saturated 0x%llx), builtin %d 0x%llx (0x%llx), portable %d "
                "0x%llx (0x%llx)\n",
                IS_SIGNED(type) ? "s" : "u",
                op_name[op],
                ovf_width[type],
                (unsigned long long)a,
                (unsigned long long)b,
                ro,
                (unsigned long long)rw,
                (unsigned long long)rs,
                fo,
                (unsigned long long)fw,
                (unsigned long long)fs,
                so,
                (unsigned long long)sw,
                (unsigned long long)ss
            );
            PF_ASSERT_ALWAYS(0 && "mismatch");
        }
    }
}

static int test_exhaustive(int seed, int i) {
    unsigned a, b;
    (void)seed, (void)i;
    for (a = 0; a < 256; a++) {
        for (b = 0; b < 256; b++) {
            check_pair(T_I8, a, b);
            check_pair(T_U8, a, b);
        }
    }
    return 0;
}

/* Every pair from the edges of each type. */
static int test_boundaries(int seed, int i) {
    int type;
    (void)seed, (void)i;
    for (type = 0; type < T_COUNT; type++) {
        unsigned w = ovf_width[type];
        uint64_t top = (uint64_t)1 << (w - 1); /* MIN, or half of MAX */
        uint64_t edges[] = {
            0,       1,       2,       3,           ~(uint64_t)0,
            ~(uint64_t)1, top, top + 1, top - 1,   top - 2,
            top >> 1, (top >> 1) + 1, (top >> 1) - 1,
            (uint64_t)1 << (w / 2), ((uint64_t)1 << (w / 2)) - 1,
            ((uint64_t)1 << (w / 2)) + 1,
        };
        size_t x, y, n = sizeof(edges) / sizeof(edges[0]);
        for (x = 0; x < n; x++)
            for (y = 0; y < n; y++) {
                check_pair(type, edges[x], edges[y]);
                check_pair(type, edges[x], 0 - edges[y]);
            }
    }
    return 0;
}

static int test_random(int seed, int i) {
    unsigned state = (unsigned)seed + (unsigned)i * 104729u;
    int type, k;
    for (type = 0; type < T_COUNT; type++) {
        for (k = 0; k < 3000; k++) {
            uint64_t a = 0, b = 0;
            int j;
            for (j = 0; j < 5; j++) {
                a = (a << 15) ^ test_rand(&state);
                b = (b << 15) ^ test_rand(&state);
            }
            /* Small magnitudes, where multiplication sometimes fits. */
            if (k % 2)
                a >>= test_rand(&state) % 64;
            if (k % 3)
                b >>= test_rand(&state) % 64;
            check_pair(type, a, b);
        }
    }
    return 0;
}

static jmp_buf env;

static void jump_back(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *message
) {
    (void)expr, (void)file, (void)line, (void)func, (void)message;
    longjmp(env, 1);
}

static int test_checked(int seed, int i) {
    volatile int failed = 0;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_checked_sadd(2, 3), 5);
    PF_ASSERT_ALWAYS_EQ(pf_checked_ssub8(-100, 28), -128);
    PF_ASSERT_ALWAYS_EQ(pf_checked_umulsize(1u << 10, 1u << 10), 1u << 20);
    PF_ASSERT_ALWAYS_EQ(pf_checked_smuldiff(-4, 5), -20);

    pf_set_assert_handler(jump_back);
    if (setjmp(env) == 0)
        (void)pf_checked_sadd(INT_MAX, 1);
    else
        failed++;
    if (setjmp(env) == 0)
        (void)pf_checked_usub64(0, 1);
    else
        failed++;
    if (setjmp(env) == 0)
        (void)pf_checked_smul8(16, 8);
    else
        failed++;
    pf_set_assert_handler(NULL);
    PF_ASSERT_ALWAYS_EQ(failed, 3);
    return 0;
}

static int test_ckd(int seed, int i) {
    (void)seed, (void)i;
#if PF_HAS_CKD
    {
        uint8_t u8 = 0;
        int16_t i16 = 0;
        int64_t i64 = 0;
        uint32_t u32 = 0;
        unsigned long long ull = 0;

        /* Exact operands, then the conversion. */
        PF_ASSERT_ALWAYS(!PF_CKD_ADD(&u8, 300, -100));
        PF_ASSERT_ALWAYS_EQ(u8, 200);
        PF_ASSERT_ALWAYS(PF_CKD_ADD(&u8, 200, 100));
        PF_ASSERT_ALWAYS_EQ(u8, 44);
        PF_ASSERT_ALWAYS(PF_CKD_SUB(&u32, 0u, 1));
        PF_ASSERT_ALWAYS_EQ(u32, UINT32_MAX);
        PF_ASSERT_ALWAYS(!PF_CKD_SUB(&i16, 5u, 10u) && i16 == -5);
        PF_ASSERT_ALWAYS(PF_CKD_MUL(&i64, INT64_MAX, 2));
        PF_ASSERT_ALWAYS_EQ(i64, -2);
        PF_ASSERT_ALWAYS(!PF_CKD_MUL(&i64, -3, 4000000000u));
        PF_ASSERT_ALWAYS_EQ(i64, -12000000000);
        PF_ASSERT_ALWAYS(!PF_CKD_MUL(&ull, UINT32_MAX, UINT32_MAX));
        PF_ASSERT_ALWAYS(PF_CKD_MUL(&ull, -1, 1));
    }
#endif
    return 0;
}

pf_test_t suite_overflow[] = {
    { test_exhaustive, "exhaustive" },
    { test_boundaries, "boundaries" },
    { test_random, "random" },
    { test_checked, "checked" },
    { test_ckd, "ckd" },
    { 0 },
};
