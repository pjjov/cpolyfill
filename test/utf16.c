/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* UTF-8 <-> UTF-16 conversion (internal). */

#include "../src/internal.h"

#include "check.h"

static size_t u16len(const uint16_t *s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

static int test_valid(int seed, int i) {
    /* "aé€😀" */
    static const char utf8[] = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    static const uint16_t utf16[] = { 'a', 0xE9, 0x20AC, 0xD83D, 0xDE00, 0 };
    uint16_t w[16];
    char s[32];
    (void)seed, (void)i;

    CHECK_INT(pf__utf8_to_utf16(utf8, PF__NTS, NULL, 0), 5);
    CHECK_INT(pf__utf8_to_utf16(utf8, PF__NTS, w, 16), 5);
    CHECK_MEM(w, utf16, sizeof(utf16));

    CHECK_INT(pf__utf16_to_utf8(utf16, PF__NTS, NULL, 0), 10);
    CHECK_INT(pf__utf16_to_utf8(utf16, PF__NTS, s, 32), 10);
    CHECK_STR(s, utf8);

    CHECK_INT(pf__utf8_to_utf16("", PF__NTS, w, 1), 0);
    CHECK(w[0] == 0);
    CHECK_INT(pf__utf16_to_utf8(utf16, 0, s, 1), 0);
    CHECK(s[0] == 0);

    /* Boundaries of each encoding length. */
    CHECK_INT(pf__utf8_to_utf16("\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80", PF__NTS, w, 16), 4);
    CHECK(w[0] == 0x7F && w[1] == 0x80 && w[2] == 0x7FF && w[3] == 0x800);
    CHECK_INT(pf__utf8_to_utf16("\xEF\xBF\xBF\xF4\x8F\xBF\xBF", PF__NTS, w, 16), 3);
    CHECK(w[0] == 0xFFFF && w[1] == 0xDBFF && w[2] == 0xDFFF);
    return 0;
}

static int test_length(int seed, int i) {
    static const uint16_t nul[] = { 'a', 0, 'b' };
    uint16_t w[8];
    char s[8];
    (void)seed, (void)i;

    /* An explicit length converts embedded NULs too. */
    CHECK_INT(pf__utf8_to_utf16("a\0b", 3, w, 8), 3);
    CHECK(w[0] == 'a' && w[1] == 0 && w[2] == 'b' && w[3] == 0);
    CHECK_INT(pf__utf16_to_utf8(nul, 3, s, 8), 3);
    CHECK_MEM(s, "a\0b", 4);

    /* Only part of the input. */
    CHECK_INT(pf__utf8_to_utf16("abcdef", 2, w, 8), 2);
    CHECK(w[2] == 0);
    return 0;
}

static int test_invalid(int seed, int i) {
    static const char *const bad8[] = {
        "\x80",             /* stray continuation */
        "\xC0\x80",         /* overlong NUL */
        "\xC1\xBF",         /* overlong */
        "\xE0\x80\x80",     /* overlong */
        "\xF0\x80\x80\x80", /* overlong */
        "\xED\xA0\x80",     /* surrogate */
        "\xED\xBF\xBF",     /* surrogate */
        "\xF4\x90\x80\x80", /* above U+10FFFF */
        "\xF8\x88\x80\x80\x80",
        "\xFF",
        "\xE2\x82",         /* truncated */
        ("a\xE2\x82" "b"),  /* truncated, then more */
        "\xC3\x28",         /* bad continuation */
    };
    static const uint16_t lone_high[] = { 'a', 0xD800, 0 };
    static const uint16_t lone_low[] = { 0xDC00, 'a', 0 };
    static const uint16_t swapped[] = { 0xDE00, 0xD83D, 0 };
    static const uint16_t high_high[] = { 0xD83D, 0xD83D, 0xDE00, 0 };
    uint16_t w[16];
    char s[16];
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(bad8) / sizeof(bad8[0]); k++) {
        CHECK_INT(pf__utf8_to_utf16(bad8[k], PF__NTS, w, 16), PF_EILSEQ);
        CHECK_INT(pf__utf8_to_utf16(bad8[k], PF__NTS, NULL, 0), PF_EILSEQ);
    }
    CHECK_INT(pf__utf16_to_utf8(lone_high, PF__NTS, s, 16), PF_EILSEQ);
    CHECK_INT(pf__utf16_to_utf8(lone_low, PF__NTS, s, 16), PF_EILSEQ);
    CHECK_INT(pf__utf16_to_utf8(swapped, PF__NTS, s, 16), PF_EILSEQ);
    CHECK_INT(pf__utf16_to_utf8(high_high, PF__NTS, NULL, 0), PF_EILSEQ);
    /* A pair cut by an explicit length. */
    CHECK_INT(pf__utf16_to_utf8(swapped + 1, 1, s, 16), PF_EILSEQ);
    return 0;
}

static int test_capacity(int seed, int i) {
    static const uint16_t pair[] = { 0xD83D, 0xDE00, 0 };
    uint16_t w[4];
    char s[8];
    (void)seed, (void)i;

    CHECK_INT(pf__utf8_to_utf16("abc", PF__NTS, w, 4), 3);
    CHECK_INT(pf__utf8_to_utf16("abcd", PF__NTS, w, 4), PF_ERANGE);
    CHECK_INT(pf__utf8_to_utf16("", PF__NTS, w, 0), PF_ERANGE);
    /* a surrogate pair needs 2 units plus the NUL */
    CHECK_INT(pf__utf8_to_utf16("\xF0\x9F\x98\x80", PF__NTS, w, 2), PF_ERANGE);
    CHECK_INT(pf__utf8_to_utf16("\xF0\x9F\x98\x80", PF__NTS, w, 3), 2);

    CHECK_INT(pf__utf16_to_utf8(pair, PF__NTS, s, 4), PF_ERANGE);
    CHECK_INT(pf__utf16_to_utf8(pair, PF__NTS, s, 5), 4);
    return 0;
}

/* Random valid UTF-16 survives a round trip through UTF-8. */
static int test_roundtrip(int seed, int i) {
    unsigned state = (unsigned)seed * 31u + (unsigned)i;
    uint16_t in[300], back[300];
    char mid[1200];
    size_t n = 0;
    ptrdiff_t len8, len16;

    while (n < 290) {
        unsigned long cp;
        switch (test_rand(&state) % 4) {
        case 0: cp = 1 + test_rand(&state) % 0x7F; break;
        case 1: cp = 0x80 + test_rand(&state) % 0x780; break;
        case 2: cp = 0x800 + (test_rand(&state) * 3u) % 0xF800; break;
        default: cp = 0x10000 + ((unsigned long)test_rand(&state) << 5) % 0x100000; break;
        }
        if (cp >= 0xD800 && cp <= 0xDFFF)
            continue;
        if (cp >= 0x10000) {
            in[n++] = (uint16_t)(0xD800 + ((cp - 0x10000) >> 10));
            in[n++] = (uint16_t)(0xDC00 + ((cp - 0x10000) & 0x3FF));
        } else {
            in[n++] = (uint16_t)cp;
        }
    }
    in[n] = 0;

    len8 = pf__utf16_to_utf8(in, PF__NTS, mid, sizeof(mid));
    CHECK(len8 > 0);
    CHECK_INT(pf__utf16_to_utf8(in, n, NULL, 0), len8);
    len16 = pf__utf8_to_utf16(mid, PF__NTS, back, 300);
    CHECK_INT(len16, n);
    CHECK_MEM(in, back, (n + 1) * sizeof(uint16_t));
    return 0;
}

static int test_wstr(int seed, int i) {
    char longstr[1100], back[1200];
    pf__wstr w;
    size_t k;
    (void)seed, (void)i;

    CHECK_INT(pf__wstr_from_utf8(&w, "short \xC3\xA9"), PF_OK);
    CHECK(w.ptr == w.small);
    CHECK_INT(u16len(w.ptr), 7);
    pf__wstr_free(&w);

    /* Long enough to need the heap. */
    for (k = 0; k < 1000; k++)
        longstr[k] = (char)('a' + k % 26);
    memcpy(longstr + 1000, "\xF0\x9F\x98\x80", 5);
    CHECK_INT(pf__wstr_from_utf8(&w, longstr), PF_OK);
    CHECK(w.ptr != w.small);
    CHECK_INT(u16len(w.ptr), 1002);
    CHECK_INT(pf__utf16_to_utf8(w.ptr, PF__NTS, back, sizeof(back)), 1004);
    CHECK_STR(back, longstr);
    pf__wstr_free(&w);
    CHECK(w.ptr == w.small);

    /* Invalid input, short and long. */
    CHECK_INT(pf__wstr_from_utf8(&w, "bad \xFF"), PF_EILSEQ);
    pf__wstr_free(&w);
    longstr[1000] = '\xFF';
    CHECK_INT(pf__wstr_from_utf8(&w, longstr), PF_EILSEQ);
    pf__wstr_free(&w);
    return 0;
}

pf_test_t suite_utf16[] = {
    { test_valid, "valid" },
    { test_length, "length" },
    { test_invalid, "invalid" },
    { test_capacity, "capacity" },
    { test_roundtrip, "roundtrip" },
    { test_wstr, "wstr" },
    { 0 },
};
