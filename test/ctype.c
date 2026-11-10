/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/ctype.h>
#include <cpolyfill/macro.h>
#include <cpolyfill/stdc.h>

#include <ctype.h>
#include <limits.h>
#include <locale.h>

#include "check.h"

/* Usable in constant expressions (C++14 constexpr is checked in cxx.cpp). */
static const char *const sets[] = {
    PF_CTYPE_DIGIT, PF_CTYPE_LOWER,  PF_CTYPE_UPPER, PF_CTYPE_ALPHA,
    PF_CTYPE_ALNUM, PF_CTYPE_XDIGIT, PF_CTYPE_SPACE, PF_CTYPE_BLANK,
    PF_CTYPE_PUNCT, PF_CTYPE_GRAPH,  PF_CTYPE_PRINT,
};

PF_STATIC_ASSERT(PF_ASCII_ESC == 27);
PF_STATIC_ASSERT(PF_ASCII_US == 31);
PF_STATIC_ASSERT(PF_ASCII_SP == ' ');
PF_STATIC_ASSERT(PF_ASCII_DEL == 127);

/* Every value from EOF to 255 against <ctype.h> in the "C" locale. */
static int test_against_libc(int seed, int i) {
    int c;
    (void)seed, (void)i;

    setlocale(LC_CTYPE, "C");
    for (c = EOF; c <= UCHAR_MAX; c++) {
        PF_ASSERT_ALWAYS_EQ(pf_isalpha(c), !!isalpha(c));
        PF_ASSERT_ALWAYS_EQ(pf_isdigit(c), !!isdigit(c));
        PF_ASSERT_ALWAYS_EQ(pf_isxdigit(c), !!isxdigit(c));
        PF_ASSERT_ALWAYS_EQ(pf_isalnum(c), !!isalnum(c));
        PF_ASSERT_ALWAYS_EQ(pf_islower(c), !!islower(c));
        PF_ASSERT_ALWAYS_EQ(pf_isupper(c), !!isupper(c));
        PF_ASSERT_ALWAYS_EQ(pf_isspace(c), !!isspace(c));
        PF_ASSERT_ALWAYS_EQ(pf_isblank(c), !!isblank(c));
        PF_ASSERT_ALWAYS_EQ(pf_iscntrl(c), !!iscntrl(c));
        PF_ASSERT_ALWAYS_EQ(pf_isprint(c), !!isprint(c));
        PF_ASSERT_ALWAYS_EQ(pf_isgraph(c), !!isgraph(c));
        PF_ASSERT_ALWAYS_EQ(pf_ispunct(c), !!ispunct(c));
        PF_ASSERT_ALWAYS_EQ(pf_tolower(c), tolower(c));
        PF_ASSERT_ALWAYS_EQ(pf_toupper(c), toupper(c));
        PF_ASSERT_ALWAYS_EQ(pf_isascii(c), c >= 0 && c < 128);
        PF_ASSERT_ALWAYS_EQ(pf_isodigit(c), c >= '0' && c <= '7');
    }
    return 0;
}

/* Values <ctype.h> may not be given: negative chars and above 255. */
static int test_any_int(int seed, int i) {
    static const int odd[] = {
        INT_MIN, -129, -128, -62, -2, 256, 0x141, INT_MAX
    };
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < PF_COUNTOF(odd); k++) {
        int c = odd[k];
        PF_ASSERT_ALWAYS(!pf_isalpha(c) && !pf_isdigit(c) && !pf_isspace(c));
        PF_ASSERT_ALWAYS(!pf_iscntrl(c) && !pf_isprint(c) && !pf_ispunct(c));
        PF_ASSERT_ALWAYS(!pf_isascii(c) && !pf_isxdigit(c));
        PF_ASSERT_ALWAYS_EQ(pf_tolower(c), c);
        PF_ASSERT_ALWAYS_EQ(pf_toupper(c), c);
        PF_ASSERT_ALWAYS_EQ(pf_digit_value(c, 36), -1);
    }
    /* 0x141 is 'A' + 256: it must not be taken for 'A' */
    PF_ASSERT_ALWAYS(!pf_isupper(0x141));
    return 0;
}

static int test_digit_value(int seed, int i) {
    int base, c;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_digit_value('7', 8), 7);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('8', 8), -1);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('f', 16), 15);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('F', 16), 15);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('g', 16), -1);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('z', 36), 35);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('Z', 36), 35);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('1', 2), 1);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('2', 2), -1);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('0', 1), -1);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('0', 37), -1);
    PF_ASSERT_ALWAYS_EQ(pf_digit_value('-', 10), -1);

    /* strtol agrees on every single digit in every base */
    for (base = 2; base <= 36; base++) {
        for (c = 0; c <= UCHAR_MAX; c++) {
            char s[2], *end;
            long v;
            s[0] = (char)c;
            s[1] = '\0';
            v = pf_isalnum(c) ? strtol(s, &end, base) : 0;
            if (pf_isalnum(c) && end == s + 1)
                PF_ASSERT_ALWAYS_EQ(pf_digit_value(c, base), v);
            else
                PF_ASSERT_ALWAYS_EQ(pf_digit_value(c, base), -1);
        }
    }
    return 0;
}

/* The character-set strings hold exactly their class. */
static int test_sets(int seed, int i) {
    static bool (*const fns[])(int) = {
        pf_isdigit, pf_islower, pf_isupper, pf_isalpha, pf_isalnum, pf_isxdigit,
        pf_isspace, pf_isblank, pf_ispunct, pf_isgraph, pf_isprint,
    };
    size_t k;
    int c;
    (void)seed, (void)i;

    PF_STATIC_ASSERT(PF_COUNTOF(fns) == PF_COUNTOF(sets));
    for (k = 0; k < PF_COUNTOF(sets); k++) {
        size_t count = 0;
        for (c = 1; c < 128; c++) {
            bool in = strchr(sets[k], c) != NULL;
            PF_ASSERT_ALWAYS_EQ(in, fns[k](c));
            count += in;
        }
        PF_ASSERT_ALWAYS_EQ(count, strlen(sets[k]));
    }
    PF_ASSERT_ALWAYS_EQ(sizeof(PF_CTYPE_CNTRL), 34);
    for (c = 0; c < 128; c++)
        PF_ASSERT_ALWAYS_EQ(
            memchr(PF_CTYPE_CNTRL, c, sizeof(PF_CTYPE_CNTRL) - 1) != NULL,
            pf_iscntrl(c)
        );
    return 0;
}

pf_test_t suite_ctype[] = {
    { test_against_libc, "against_libc" },
    { test_any_int, "any_int" },
    { test_digit_value, "digit_value" },
    { test_sets, "sets" },
    { 0 },
};
