/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The language layer used from C++, where several macros have their own
   implementation (templates instead of GNU C builtins). Failures abort. */

#include <cpolyfill/assert.h>
#include <cpolyfill/attribute.h>
#include <cpolyfill/bitwise.h>
#include <cpolyfill/debug.h>
#include <cpolyfill/endian.h>
#include <cpolyfill/errno.h>
#include <cpolyfill/macro.h>
#include <cpolyfill/overflow.h>
#include <cpolyfill/stdc.h>
#include <cpolyfill/types.h>

#include <stdint.h>

PF_STATIC_ASSERT(sizeof(char) == 1);
PF_STATIC_ASSERT(sizeof(int) >= 2, "with a message");

struct item {
    int key;
    char name[8];
};

static int constructed = 0;
PF_CONSTRUCTOR(cxx_constructor) { constructed = 1; }

PF_NODISCARD("x") PF_CONST static int twice(int x) { return 2 * x; }

static void test_types() {
    int n = 0;
    const int cn = 1;
    long l = 0;
    char arr[4] = { 0, 0, 0, 0 };
    char *p = arr;
    const char *cp = arr;

    PF_STATIC_ASSERT(PF_HAS_TYPE_CHECKS);
    PF_STATIC_ASSERT(PF_IS_TYPE(int, n));
    PF_STATIC_ASSERT(PF_IS_TYPE(int, cn));
    PF_STATIC_ASSERT(!PF_IS_TYPE(long, n));
    PF_STATIC_ASSERT(PF_IS_TYPE(char[4], arr));
    PF_STATIC_ASSERT(!PF_IS_TYPE(char *, arr));
    PF_STATIC_ASSERT(PF_IS_TYPE(int, (n))); /* decltype((n)) is int & */
    PF_STATIC_ASSERT(PF_SAME_TYPE(n, cn));
    PF_STATIC_ASSERT(!PF_SAME_TYPE(n, l));
    PF_STATIC_ASSERT(PF_IS_CONST(cn));
    PF_STATIC_ASSERT(!PF_IS_CONST(n));
    PF_STATIC_ASSERT(PF_IS_CONST(*cp));
    PF_STATIC_ASSERT(!PF_IS_CONST(cp));
    PF_STATIC_ASSERT(PF_IS_ARRAY(arr));
    PF_STATIC_ASSERT(!PF_IS_ARRAY(p));
    PF_STATIC_ASSERT(PF_IS_POINTER(p));
    PF_STATIC_ASSERT(!PF_IS_POINTER(arr));
    PF_STATIC_ASSERT(PF_COUNTOF(arr) == 4);

    PF_CHECK_TYPE(int, n) = 5; /* an lvalue stays one */
    PF_ASSERT_ALWAYS_EQ(n, 5);
    PF_ASSERT_ALWAYS_EQ(PF_IF_TYPE(int, n, 1, 2), 1);
    PF_ASSERT_ALWAYS_EQ(PF_IF_CONST(n, 1, 2), 2);
    (void)l, (void)p, (void)cp;
}

static void test_stdc() {
    PF_TYPEOF(1 + 1) x = 2;
    const int c = x;
    PF_TYPEOF_UNQUAL(c) y = c;
    PF_ALIGNAS(16) char buf[4];
    int *np = PF_NULLPTR;
    pf_max_align_t max;

    y++;
    PF_ASSERT_ALWAYS_EQ(y, 3);
    PF_ASSERT_ALWAYS_EQ((uintptr_t)buf % 16, 0);
    PF_ASSERT_ALWAYS_EQ(PF_ALIGNOF(pf_max_align_t), alignof(max_align_t));
    PF_ASSERT_ALWAYS_NULL(np);
    PF_ASSERT_ALWAYS_EQ(sizeof(pf_char8_t), 1);
    (void)buf, (void)max;
}

static void test_macro() {
    int a = 3, b = -4, n = 0;
    item it;
    uint64_t w = 0xFFFFFFFF00000003ULL;

    PF_ASSERT_ALWAYS_EQ(PF_MIN(a, b), -4);
    PF_ASSERT_ALWAYS_EQ(PF_MAX(n++, 0), 0);
    PF_ASSERT_ALWAYS_EQ(PF_ABS(b), 4);
    PF_ASSERT_ALWAYS_EQ(PF_CLAMP(9, 0, 5), 5);
    PF_SWAP(a, b);
    PF_ASSERT_ALWAYS_EQ(a, -4);
    PF_ASSERT_ALWAYS(PF_CONTAINER_OF(&it.key, item, key) == &it);
    PF_ASSERT_ALWAYS_EQ(PF_FLAG_CLEAR(w, 0x1u), 0xFFFFFFFF00000002ULL);
    PF_ASSERT_ALWAYS_EQ(PF_BIT_SET(w, 40), w | (1ULL << 40));
    PF_ASSERT_ALWAYS_EQ(PF_ALIGN_UP(13, 8), 16);
    PF_ASSERT_ALWAYS_EQ(PF_NARGS(a, b, c), 3);
    PF_ASSERT_ALWAYS(PF_LIKELY(a < 0));
    PF_ASSERT_ALWAYS_EQ(twice(a), -8);
}

static void test_assert() {
    int n = 3;
    PF_ASSERT(n == 3);
    PF_ASSERT_LT(-1, 0u);
    PF_ASSERT_STR_EQ("a", "a");
    PF_ASSERT_ALWAYS(PF_CHECK_EQ(n, 3));
    PF_ASSERT_ALWAYS(PF_CHECK_MSG(n > 0, "n is %d", n));
}

static void test_debug() {
    void *frames[8];
    int n = pf_backtrace(frames, 8);
    PF_ASSERT_ALWAYS(n > 0 || n == PF_ENOSYS);
    (void)pf_debugger_attached();
}

#if PF_CPP >= 201402L
/* constexpr in C++14 and later. */
PF_STATIC_ASSERT(pf_clz32(1) == 31);
PF_STATIC_ASSERT(pf_pow2ceil8(5) == 8);
PF_STATIC_ASSERT(pf_rotr16(1, 1) == 0x8000);
PF_STATIC_ASSERT(PF_POPCOUNT(0xFFull) == 8);
PF_STATIC_ASSERT(pf_bswap32(0x11223344) == 0x44332211);
#endif
PF_STATIC_ASSERT(PF_BSWAP16(0x1234) == 0x3412);

template <class T, class U> static bool same_type(T, U) { return false; }
template <class T> static bool same_type(T, T) { return true; }

static void test_integers() {
    unsigned char uc = 0x30;
    unsigned short us = 0x300;
    unsigned long long ull = 1ull << 40;
    unsigned char out = 0;
    int r = 0;

    PF_STATIC_ASSERT(PF_HAS_BITWISE_GENERIC);
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(uc), 2);
    PF_ASSERT_ALWAYS_EQ(PF_CLZ(us), 6);
    PF_ASSERT_ALWAYS_EQ(PF_CTZ(ull), 40);
    PF_ASSERT_ALWAYS_EQ(PF_FLO(1u), 32);
    PF_ASSERT_ALWAYS_EQ(PF_POW2CEIL(uc), 0x40);
    PF_ASSERT_ALWAYS_EQ(PF_ROTL(uc, 4), 0x03);
    PF_ASSERT_ALWAYS(same_type(PF_POW2CEIL(uc), uc));
    PF_ASSERT_ALWAYS(same_type(PF_ROTR(us, 1), us));
    PF_ASSERT_ALWAYS(same_type(PF_BITREVERSE(ull), ull));
    PF_ASSERT_ALWAYS(same_type(PF_CLZ(ull), 0));

    PF_ASSERT_ALWAYS(pf_overflow_sadd(2147483647, 1, &r));
    PF_ASSERT_ALWAYS_EQ(pf_saturated_umul8(16, 16), 255);
    PF_ASSERT_ALWAYS_EQ(pf_checked_ssub(1, 2), -1);
#if PF_HAS_CKD
    PF_ASSERT_ALWAYS(!PF_CKD_ADD(&out, 300, -100) && out == 200);
#endif
    (void)out;

    PF_ASSERT_ALWAYS_EQ(pf_load_be16("\x12\x34"), 0x1234);
    PF_ASSERT_ALWAYS_EQ(pf_be32toh(pf_htobe32(7)), 7u);
}

int main() {
    PF_ASSERT_ALWAYS_EQ(constructed, 1);
    test_types();
    test_stdc();
    test_macro();
    test_assert();
    test_debug();
    test_integers();
    return 0;
}
