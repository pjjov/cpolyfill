/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/unicode.h>, and the internal pf__wstr. */

#include "../src/internal.h"

#include <cpolyfill/macro.h>
#include <cpolyfill/unicode.h>

#include "check.h"

static size_t u16len(const uint16_t *s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

/* Invalid UTF-8, with the number of U+FFFD each becomes in replace mode
   (one per maximal subpart, Unicode 15 table 3-8 and friends). */
static const struct {
    const char *s;
    int fffd;
} bad8[] = {
    { "\x80", 1 }, /* stray continuation */
    { "\x80\xBF", 2 },
    { "\xC0\x80", 2 }, /* overlong NUL: C0 is never valid */
    { "\xC1\xBF", 2 },
    { "\xE0\x80\x80", 3 }, /* overlong: 80 is not allowed after E0 */
    { "\xF0\x80\x80\x80", 4 },
    { "\xED\xA0\x80", 3 }, /* surrogate */
    { "\xED\xBF\xBF", 3 },
    { "\xF4\x90\x80\x80", 4 }, /* above U+10FFFF */
    { "\xF5\x80\x80\x80", 4 },
    { "\xF8\x88\x80\x80\x80", 5 },
    { "\xFF", 1 },
    { "\xE2\x82", 1 }, /* truncated: one maximal subpart */
    { "\xF0\x9F\x98", 1 },
    { "\xC3\x28", 1 },                 /* bad continuation, then '(' */
    { "\xF1\x80\x80\xE1\x80\xC2", 3 }, /* Unicode's own example, part */
};

/* ---- Inline helpers --------------------------------------------------- */

static int test_helpers(int seed, int i) {
    int c;
    (void)seed, (void)i;

    for (c = 0; c < 0x80; c++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(c), 1);
    for (c = 0x80; c < 0xC2; c++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(c), 0);
    for (c = 0xC2; c < 0xE0; c++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(c), 2);
    for (c = 0xE0; c < 0xF0; c++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(c), 3);
    for (c = 0xF0; c < 0xF5; c++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(c), 4);
    for (c = 0xF5; c < 0x100; c++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(c), 0);
    /* a negative char works like the unsigned byte */
    PF_ASSERT_ALWAYS_EQ(pf_utf8_clen((char)0xE2), 3);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(-1), 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(0x141), 0);

    PF_ASSERT_ALWAYS(pf_utf8_iscont(0x80) && pf_utf8_iscont(0xBF));
    PF_ASSERT_ALWAYS(pf_utf8_iscont((char)0x9F));
    PF_ASSERT_ALWAYS(!pf_utf8_iscont(0x7F) && !pf_utf8_iscont(0xC0));
    PF_ASSERT_ALWAYS(!pf_utf8_iscont(0x180));

    PF_ASSERT_ALWAYS(pf_unicode_isvalid(0) && pf_unicode_isvalid(0xD7FF));
    PF_ASSERT_ALWAYS(
        !pf_unicode_isvalid(0xD800) && !pf_unicode_isvalid(0xDFFF)
    );
    PF_ASSERT_ALWAYS(
        pf_unicode_isvalid(0xE000) && pf_unicode_isvalid(PF_UNICODE_MAX)
    );
    PF_ASSERT_ALWAYS(
        !pf_unicode_isvalid(0x110000) && !pf_unicode_isvalid(UINT32_MAX)
    );
    PF_ASSERT_ALWAYS(
        pf_unicode_issurrogate(0xDBFF) && !pf_unicode_issurrogate(0xE000)
    );
    PF_ASSERT_ALWAYS_EQ(PF_UNICODE_REPLACEMENT, 0xFFFD);
    return 0;
}

/* ---- Single code points ----------------------------------------------- */

/* Every scalar value encodes, decodes back, and agrees across encodings. */
static int test_every_code_point(int seed, int i) {
    uint32_t cp;
    (void)seed;

    if (i > 0)
        return 0; /* deterministic: once is enough */
    for (cp = 0; cp <= 0x10FFFF + 1; cp++) {
        char b[4];
        uint16_t w[2];
        uint32_t back = 0;
        int n8 = pf_utf8_encode(cp, b);
        int n16 = pf_utf16_encode(cp, w);

        if (!pf_unicode_isvalid(cp)) {
            PF_ASSERT_ALWAYS_EQ(n8, PF_EILSEQ);
            PF_ASSERT_ALWAYS_EQ(n16, PF_EILSEQ);
            continue;
        }
        PF_ASSERT_ALWAYS_EQ(
            n8,
            cp < 0x80          ? 1
                : cp < 0x800   ? 2
                : cp < 0x10000 ? 3
                               : 4
        );
        PF_ASSERT_ALWAYS_EQ(n16, cp < 0x10000 ? 1 : 2);
        PF_ASSERT_ALWAYS_EQ(pf_utf8_clen(b[0]), n8);
        PF_ASSERT_ALWAYS_EQ(pf_utf8_decode(b, (size_t)n8, &back), n8);
        PF_ASSERT_ALWAYS_EQ(back, cp);
        PF_ASSERT_ALWAYS_EQ(pf_utf16_decode(w, (size_t)n16, &back), n16);
        PF_ASSERT_ALWAYS_EQ(back, cp);
        /* one unit short is truncated */
        PF_ASSERT_ALWAYS_EQ(
            pf_utf8_decode(b, (size_t)n8 - 1, &back), n8 == 1 ? 0 : PF_EILSEQ
        );
    }
    return 0;
}

static int test_decode(int seed, int i) {
    static const uint16_t lone[] = { 0xD800, 'a' };
    static const uint16_t low[] = { 0xDC00 };
    uint32_t cp = 7;
    size_t k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_utf8_decode("", 0, &cp), 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_decode("\xC3\xA9xyz", 5, &cp), 2);
    PF_ASSERT_ALWAYS_EQ(cp, 0xE9);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_decode("\0", 1, &cp), 1);
    PF_ASSERT_ALWAYS_EQ(cp, 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_decode("\xE2\x82\xAC", 3, NULL), 3);
    for (k = 0; k < PF_COUNTOF(bad8); k++)
        PF_ASSERT_ALWAYS_EQ(
            pf_utf8_decode(bad8[k].s, strlen(bad8[k].s), &cp), PF_EILSEQ
        );

    PF_ASSERT_ALWAYS_EQ(pf_utf16_decode(lone, 2, &cp), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_decode(lone, 1, &cp), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_decode(low, 1, &cp), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_decode(low, 0, &cp), 0);
    return 0;
}

static int test_validate(int seed, int i) {
    size_t k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix("", PF_NTS), 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix("abc", PF_NTS), 3);
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_valid_prefix(
            "a\xC3\xA9\xFF"
            "b",
            PF_NTS
        ),
        3
    );
    PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix("ab\xE2\x82", PF_NTS), 2);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix("ab\xE2\x82\xAC", 4), 2);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix("a\0b", 3), 3);
    /* every entry is invalid from its first byte */
    for (k = 0; k < PF_COUNTOF(bad8); k++)
        PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix(bad8[k].s, PF_NTS), 0);

    PF_ASSERT_ALWAYS_EQ(pf_utf8_count("", PF_NTS), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_count("a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", PF_NTS), 4
    );
    PF_ASSERT_ALWAYS_EQ(pf_utf8_count("a\0b", 3), 3);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_count("a\xC3", PF_NTS), PF_EILSEQ);
    return 0;
}

/* ---- Conversions ------------------------------------------------------ */

static int test_valid(int seed, int i) {
    /* "aé€😀" */
    static const char utf8[] = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    static const uint16_t utf16[] = { 'a', 0xE9, 0x20AC, 0xD83D, 0xDE00, 0 };
    static const uint32_t utf32[] = { 'a', 0xE9, 0x20AC, 0x1F600, 0 };
    static const wchar_t wide[] = L"aé€\U0001F600";
    uint16_t w[16];
    uint32_t u[16];
    wchar_t wc[16];
    char s[32];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16(utf8, PF_NTS, NULL, 0, 0), 5);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16(utf8, PF_NTS, w, 16, 0), 5);
    PF_ASSERT_ALWAYS_MEM_EQ(w, utf16, sizeof(utf16));
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(utf16, PF_NTS, NULL, 0, 0), 10);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(utf16, PF_NTS, s, 32, 0), 10);
    PF_ASSERT_ALWAYS_STR_EQ(s, utf8);

    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf32(utf8, PF_NTS, NULL, 0, 0), 4);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf32(utf8, PF_NTS, u, 16, 0), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(u, utf32, sizeof(utf32));
    PF_ASSERT_ALWAYS_EQ(pf_utf32_to_utf8(utf32, PF_NTS, s, 32, 0), 10);
    PF_ASSERT_ALWAYS_STR_EQ(s, utf8);

    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_wcs(utf8, PF_NTS, wc, 16, 0), (ptrdiff_t)wcslen(wide)
    );
    PF_ASSERT_ALWAYS(wcscmp(wc, wide) == 0);
    PF_ASSERT_ALWAYS_EQ(pf_wcs_to_utf8(wide, PF_NTS, s, 32, 0), 10);
    PF_ASSERT_ALWAYS_STR_EQ(s, utf8);

    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16("", PF_NTS, w, 1, 0), 0);
    PF_ASSERT_ALWAYS(w[0] == 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(utf16, 0, s, 1, 0), 0);
    PF_ASSERT_ALWAYS(s[0] == 0);

    /* Boundaries of each encoding length. */
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf16("\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80", PF_NTS, w, 16, 0),
        4
    );
    PF_ASSERT_ALWAYS(
        w[0] == 0x7F && w[1] == 0x80 && w[2] == 0x7FF && w[3] == 0x800
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf16("\xEF\xBF\xBF\xF4\x8F\xBF\xBF", PF_NTS, w, 16, 0), 3
    );
    PF_ASSERT_ALWAYS(w[0] == 0xFFFF && w[1] == 0xDBFF && w[2] == 0xDFFF);
    return 0;
}

static int test_length(int seed, int i) {
    static const uint16_t nul[] = { 'a', 0, 'b' };
    uint16_t w[8];
    char s[8];
    (void)seed, (void)i;

    /* An explicit length converts embedded NULs too. */
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16("a\0b", 3, w, 8, 0), 3);
    PF_ASSERT_ALWAYS(w[0] == 'a' && w[1] == 0 && w[2] == 'b' && w[3] == 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(nul, 3, s, 8, 0), 3);
    PF_ASSERT_ALWAYS_MEM_EQ(s, "a\0b", 4);

    /* Only part of the input. */
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16("abcdef", 2, w, 8, 0), 2);
    PF_ASSERT_ALWAYS(w[2] == 0);
    return 0;
}

static int test_invalid(int seed, int i) {
    static const uint16_t lone_high[] = { 'a', 0xD800, 0 };
    static const uint16_t lone_low[] = { 0xDC00, 'a', 0 };
    static const uint16_t swapped[] = { 0xDE00, 0xD83D, 0 };
    static const uint16_t high_high[] = { 0xD83D, 0xD83D, 0xDE00, 0 };
    static const uint32_t bad32[] = { 'a', 0xD800, 0 };
    static const uint32_t big32[] = { 0x110000, 0 };
    uint16_t w[16];
    uint32_t u[16];
    char s[16];
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < PF_COUNTOF(bad8); k++) {
        PF_ASSERT_ALWAYS_EQ(
            pf_utf8_to_utf16(bad8[k].s, PF_NTS, w, 16, 0), PF_EILSEQ
        );
        PF_ASSERT_ALWAYS_EQ(
            pf_utf8_to_utf16(bad8[k].s, PF_NTS, NULL, 0, 0), PF_EILSEQ
        );
        PF_ASSERT_ALWAYS_EQ(
            pf_utf8_to_utf32(bad8[k].s, PF_NTS, u, 16, 0), PF_EILSEQ
        );
    }
    PF_ASSERT_ALWAYS_EQ(
        pf_utf16_to_utf8(lone_high, PF_NTS, s, 16, 0), PF_EILSEQ
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_utf16_to_utf8(lone_low, PF_NTS, s, 16, 0), PF_EILSEQ
    );
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(swapped, PF_NTS, s, 16, 0), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(
        pf_utf16_to_utf8(high_high, PF_NTS, NULL, 0, 0), PF_EILSEQ
    );
    /* A pair cut by an explicit length. */
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(swapped + 1, 1, s, 16, 0), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf_utf32_to_utf8(bad32, PF_NTS, s, 16, 0), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf_utf32_to_utf8(big32, PF_NTS, s, 16, 0), PF_EILSEQ);
#if WCHAR_MAX > 0xFFFF
    {
        static const wchar_t badw[] = { 'a', (wchar_t)0xDC00, 0 };
        PF_ASSERT_ALWAYS_EQ(pf_wcs_to_utf8(badw, PF_NTS, s, 16, 0), PF_EILSEQ);
    }
#endif
    return 0;
}

static int test_replace(int seed, int i) {
    static const uint16_t lone[] = { 'a', 0xD800, 'b', 0xDC00, 0 };
    static const uint32_t bad32[] = { 0xD800, 'x', 0x110000, 0 };
    uint32_t u[32];
    char s[64];
    size_t k;
    int j;
    (void)seed, (void)i;

    for (k = 0; k < PF_COUNTOF(bad8); k++) {
        const char *in = bad8[k].s;
        ptrdiff_t n = pf_utf8_to_utf32(in, PF_NTS, u, 32, PF_UNICODE_REPLACE);
        int fffd = 0;

        PF_ASSERT_ALWAYS(n > 0);
        PF_ASSERT_ALWAYS_EQ(
            pf_utf8_to_utf32(in, PF_NTS, NULL, 0, PF_UNICODE_REPLACE), n
        );
        for (j = 0; j < n; j++)
            fffd += u[j] == PF_UNICODE_REPLACEMENT;
        PF_ASSERT_ALWAYS_EQ(fffd, bad8[k].fffd);
    }
    /* the valid parts survive */
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf32("\xC3\x28", PF_NTS, u, 32, PF_UNICODE_REPLACE), 2
    );
    PF_ASSERT_ALWAYS(u[0] == 0xFFFD && u[1] == '(');
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf32(
            "a\xF0\x9F\x98"
            "b\xF0\x9F\x98\x80",
            PF_NTS,
            u,
            32,
            PF_UNICODE_REPLACE
        ),
        4
    );
    PF_ASSERT_ALWAYS(
        u[0] == 'a' && u[1] == 0xFFFD && u[2] == 'b' && u[3] == 0x1F600
    );

    PF_ASSERT_ALWAYS_EQ(
        pf_utf16_to_utf8(lone, PF_NTS, s, 64, PF_UNICODE_REPLACE), 8
    );
    PF_ASSERT_ALWAYS_STR_EQ(
        s,
        "a\xEF\xBF\xBD"
        "b\xEF\xBF\xBD"
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_utf32_to_utf8(bad32, PF_NTS, s, 64, PF_UNICODE_REPLACE), 7
    );
    PF_ASSERT_ALWAYS_STR_EQ(s, "\xEF\xBF\xBDx\xEF\xBF\xBD");
    return 0;
}

static int test_capacity(int seed, int i) {
    static const uint16_t pair[] = { 0xD83D, 0xDE00, 0 };
    uint16_t w[4];
    uint32_t u[2];
    char s[8];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16("abc", PF_NTS, w, 4, 0), 3);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16("abcd", PF_NTS, w, 4, 0), PF_ERANGE);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16("", PF_NTS, w, 0, 0), PF_ERANGE);
    /* a surrogate pair needs 2 units plus the NUL */
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf16("\xF0\x9F\x98\x80", PF_NTS, w, 2, 0), PF_ERANGE
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf16("\xF0\x9F\x98\x80", PF_NTS, w, 3, 0), 2
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_utf8_to_utf32("\xF0\x9F\x98\x80", PF_NTS, u, 2, 0), 1
    );
    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf32("ab", PF_NTS, u, 2, 0), PF_ERANGE);

    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(pair, PF_NTS, s, 4, 0), PF_ERANGE);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(pair, PF_NTS, s, 5, 0), 4);
    return 0;
}

/* Random valid UTF-16 survives round trips through UTF-8 and UTF-32. */
static int test_roundtrip(int seed, int i) {
    unsigned state = (unsigned)seed * 31u + (unsigned)i;
    uint16_t in[300], back[300];
    uint32_t wide[300];
    char mid[1200], mid2[1200];
    size_t n = 0;
    ptrdiff_t len8, len16, len32;

    while (n < 290) {
        unsigned long cp;
        switch (test_rand(&state) % 4) {
        case 0:
            cp = 1 + test_rand(&state) % 0x7F;
            break;
        case 1:
            cp = 0x80 + test_rand(&state) % 0x780;
            break;
        case 2:
            cp = 0x800 + (test_rand(&state) * 3u) % 0xF800;
            break;
        default:
            cp = 0x10000 + ((unsigned long)test_rand(&state) << 5) % 0x100000;
            break;
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

    len8 = pf_utf16_to_utf8(in, PF_NTS, mid, sizeof(mid), 0);
    PF_ASSERT_ALWAYS(len8 > 0);
    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(in, n, NULL, 0, 0), len8);
    PF_ASSERT_ALWAYS_EQ(pf_utf8_valid_prefix(mid, PF_NTS), len8);
    len16 = pf_utf8_to_utf16(mid, PF_NTS, back, 300, 0);
    PF_ASSERT_ALWAYS_EQ(len16, n);
    PF_ASSERT_ALWAYS_MEM_EQ(in, back, (n + 1) * sizeof(uint16_t));

    len32 = pf_utf8_to_utf32(mid, PF_NTS, wide, 300, 0);
    PF_ASSERT_ALWAYS_EQ(len32, pf_utf8_count(mid, PF_NTS));
    PF_ASSERT_ALWAYS_EQ(
        pf_utf32_to_utf8(wide, PF_NTS, mid2, sizeof(mid2), 0), len8
    );
    PF_ASSERT_ALWAYS_STR_EQ(mid, mid2);
    return 0;
}

static int test_wstr(int seed, int i) {
    char longstr[1100], back[1200];
    pf__wstr w;
    size_t k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf__wstr_from_utf8(&w, "short \xC3\xA9"), PF_OK);
    PF_ASSERT_ALWAYS(w.ptr == w.small);
    PF_ASSERT_ALWAYS_EQ(u16len(w.ptr), 7);
    pf__wstr_free(&w);

    /* Long enough to need the heap. */
    for (k = 0; k < 1000; k++)
        longstr[k] = (char)('a' + k % 26);
    memcpy(longstr + 1000, "\xF0\x9F\x98\x80", 5);
    PF_ASSERT_ALWAYS_EQ(pf__wstr_from_utf8(&w, longstr), PF_OK);
    PF_ASSERT_ALWAYS(w.ptr != w.small);
    PF_ASSERT_ALWAYS_EQ(u16len(w.ptr), 1002);
    PF_ASSERT_ALWAYS_EQ(
        pf_utf16_to_utf8(w.ptr, PF_NTS, back, sizeof(back), 0), 1004
    );
    PF_ASSERT_ALWAYS_STR_EQ(back, longstr);
    pf__wstr_free(&w);
    PF_ASSERT_ALWAYS(w.ptr == w.small);

    /* Invalid input, short and long. */
    PF_ASSERT_ALWAYS_EQ(pf__wstr_from_utf8(&w, "bad \xFF"), PF_EILSEQ);
    pf__wstr_free(&w);
    longstr[1000] = '\xFF';
    PF_ASSERT_ALWAYS_EQ(pf__wstr_from_utf8(&w, longstr), PF_EILSEQ);
    pf__wstr_free(&w);
    return 0;
}

/* WTF-8, for Windows paths: unpaired surrogates survive the round trip,
   everything else converts as with the strict functions. */
static int test_wtf8(int seed, int i) {
    static const uint16_t lone[] = { 'a', 0xD800, 'b', 0xDC01, 0xD83D, 0xDE00, 0xDBFF };
    static const char lone8[] = "a\xED\xA0\x80" "b\xED\xB0\x81\xF0\x9F\x98\x80\xED\xAF\xBF";
    uint16_t w[16];
    char s[32];
    pf__wstr ws;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_utf16_to_utf8(lone, 7, s, sizeof(s), 0), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf__utf16_to_wtf8(lone, 7, s, sizeof(s)), (ptrdiff_t)strlen(lone8));
    PF_ASSERT_ALWAYS_STR_EQ(s, lone8);
    PF_ASSERT_ALWAYS_EQ(pf__utf16_to_wtf8(lone, 7, NULL, 0), (ptrdiff_t)strlen(lone8));
    PF_ASSERT_ALWAYS_EQ(pf__utf16_to_wtf8(lone, 7, s, strlen(lone8)), PF_ERANGE);

    PF_ASSERT_ALWAYS_EQ(pf_utf8_to_utf16(lone8, PF_NTS, w, 16, 0), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf__wtf8_to_utf16(lone8, PF_NTS, w, 16), 7);
    PF_ASSERT_ALWAYS_MEM_EQ(w, lone, sizeof(lone));

    /* A pair written as two surrogates is not WTF-8 (it has a 4-byte
       form), and the other invalid UTF-8 stays invalid. */
    PF_ASSERT_ALWAYS_EQ(pf__wtf8_to_utf16("\xED\xA0\xBD\xED\xB8\x80", PF_NTS, w, 16), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf__wtf8_to_utf16("\xED\xA0", PF_NTS, w, 16), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf__wtf8_to_utf16("\xC0\x80", PF_NTS, w, 16), PF_EILSEQ);
    PF_ASSERT_ALWAYS_EQ(pf__wtf8_to_utf16("\xF4\x90\x80\x80", PF_NTS, w, 16), PF_EILSEQ);
    /* a low surrogate then a high one is fine (not a pair) */
    PF_ASSERT_ALWAYS_EQ(pf__wtf8_to_utf16("\xED\xB0\x80\xED\xA0\x80", PF_NTS, w, 16), 2);

    PF_ASSERT_ALWAYS_EQ(pf__wstr_from_wtf8(&ws, lone8), 0);
    PF_ASSERT_ALWAYS_MEM_EQ(ws.ptr, lone, sizeof(lone));
    PF_ASSERT_ALWAYS_EQ(ws.ptr[7], 0);
    pf__wstr_free(&ws);
    PF_ASSERT_ALWAYS_EQ(pf__wstr_from_utf8(&ws, lone8), PF_EILSEQ);
    pf__wstr_free(&ws);
    return 0;
}

pf_test_t suite_unicode[] = {
    { test_helpers, "helpers" },
    { test_every_code_point, "every_code_point" },
    { test_decode, "decode" },
    { test_validate, "validate" },
    { test_valid, "valid" },
    { test_length, "length" },
    { test_invalid, "invalid" },
    { test_replace, "replace" },
    { test_capacity, "capacity" },
    { test_roundtrip, "roundtrip" },
    { test_wstr, "wstr" },
    { test_wtf8, "wtf8" },
    { 0 },
};
