/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* pf_mbrtoc8/16/32 and pf_c8/16/32rtomb: the public functions (the
   system's where it has them), and both fallbacks of src/uchar.c, which
   the public functions use elsewhere. */

#include "../src/internal.h"

#include <cpolyfill/unicode.h>

#include <errno.h>
#include <locale.h>

#include "check.h"

enum backend { PUBLIC, WC, UTF8 };

static const char *const backend_names[] = { "public", "wc", "utf8" };

typedef struct conv {
    enum backend be;
    mbstate_t ps; /* PUBLIC, and the inner state of WC */
    pf__mbconv cv;
} conv;

static void conv_init(conv *c, enum backend be) {
    memset(c, 0, sizeof(*c));
    c->be = be;
    c->cv.inner = &c->ps;
    c->cv.utf8 = be == UTF8;
}

#define ERR ((size_t)-1)
#define MORE ((size_t)-2)
#define PENDING ((size_t)-3)

static size_t to32(conv *c, uint32_t *pc, const char *s, size_t n) {
    return c->be == PUBLIC ? pf_mbrtoc32(pc, s, n, &c->ps)
                           : pf__mbrtoc32_with(&c->cv, pc, s, n);
}
static size_t to16(conv *c, uint16_t *pc, const char *s, size_t n) {
    return c->be == PUBLIC ? pf_mbrtoc16(pc, s, n, &c->ps)
                           : pf__mbrtoc16_with(&c->cv, pc, s, n);
}
static size_t to8(conv *c, unsigned char *pc, const char *s, size_t n) {
    return c->be == PUBLIC ? pf_mbrtoc8(pc, s, n, &c->ps)
                           : pf__mbrtoc8_with(&c->cv, pc, s, n);
}
static size_t from32(conv *c, char *s, uint32_t v) {
    return c->be == PUBLIC ? pf_c32rtomb(s, v, &c->ps)
                           : pf__c32rtomb_with(&c->cv, s, v);
}
static size_t from16(conv *c, char *s, uint16_t v) {
    return c->be == PUBLIC ? pf_c16rtomb(s, v, &c->ps)
                           : pf__c16rtomb_with(&c->cv, s, v);
}
static size_t from8(conv *c, char *s, unsigned char v) {
    return c->be == PUBLIC ? pf_c8rtomb(s, v, &c->ps)
                           : pf__c8rtomb_with(&c->cv, s, v);
}

/* Switches LC_CTYPE to a UTF-8 locale. Returns the previous one (to give
   to restore_locale), or NULL if there is no UTF-8 locale. */
static char *utf8_locale(char *saved, size_t size) {
    static const char *const names[] = {
        "C.UTF-8", "C.utf8", "en_US.UTF-8", "en_US.utf8", "UTF-8",
    };
    const char *cur = setlocale(LC_CTYPE, NULL);
    size_t k;

    snprintf(saved, size, "%s", cur ? cur : "C");
    for (k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
        if (setlocale(LC_CTYPE, names[k]))
            return saved;
    }
    return NULL;
}

static void run_scenario(enum backend be) {
    conv c;
    uint32_t u32 = 0;
    uint16_t u16 = 0;
    unsigned char u8 = 0;
    char out[16];
    const char *s;

    if (getenv("PF_TEST_VERBOSE"))
        fprintf(stderr, "# backend %s\n", backend_names[be]);

    /* ---- multibyte -> UTF-32 ---- */
    conv_init(&c, be);
    s = "a\xC3\xA9\xF0\x9F\x98\x80";
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, s, 7), 1);
    PF_ASSERT_ALWAYS_EQ(u32, 'a');
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, s + 1, 6), 2);
    PF_ASSERT_ALWAYS_EQ(u32, 0xE9);
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, s + 3, 4), 4);
    PF_ASSERT_ALWAYS_EQ(u32, 0x1F600);
    /* a sequence split over calls */
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, "\xE2", 1), MORE);
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, "\x82", 1), MORE);
    PF_ASSERT_ALWAYS_EQ(
        to32(
            &c,
            &u32,
            "\xAC"
            "z",
            2
        ),
        1
    );
    PF_ASSERT_ALWAYS_EQ(u32, 0x20AC);
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, "", 0), MORE);
    /* the null character */
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, "\0x", 2), 0);
    PF_ASSERT_ALWAYS_EQ(u32, 0);
    /* NULL `s` resets */
    PF_ASSERT_ALWAYS_EQ(to32(&c, NULL, NULL, 0), 0);
    /* invalid input */
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, "\xFF", 1), ERR);
    PF_ASSERT_ALWAYS_EQ(errno, EILSEQ);
    conv_init(&c, be);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(to32(&c, &u32, "\xED\xA0\x80", 3), ERR);
    PF_ASSERT_ALWAYS_EQ(errno, EILSEQ);

    /* ---- multibyte -> UTF-16 ---- */
    conv_init(&c, be);
    PF_ASSERT_ALWAYS_EQ(
        to16(
            &c,
            &u16,
            "\xF0\x9F\x98\x80"
            "b",
            5
        ),
        4
    );
    PF_ASSERT_ALWAYS_EQ(u16, 0xD83D);
    PF_ASSERT_ALWAYS_EQ(to16(&c, &u16, "b", 1), PENDING);
    PF_ASSERT_ALWAYS_EQ(u16, 0xDE00);
    PF_ASSERT_ALWAYS_EQ(to16(&c, &u16, "b", 1), 1);
    PF_ASSERT_ALWAYS_EQ(u16, 'b');
    PF_ASSERT_ALWAYS_EQ(to16(&c, &u16, "\xE2\x82\xAC", 3), 3);
    PF_ASSERT_ALWAYS_EQ(u16, 0x20AC);

    /* ---- multibyte -> UTF-8 ---- */
    conv_init(&c, be);
    PF_ASSERT_ALWAYS_EQ(
        to8(&c,
            &u8,
            "\xE2\x82\xAC"
            "c",
            4),
        3
    );
    PF_ASSERT_ALWAYS_EQ(u8, 0xE2);
    PF_ASSERT_ALWAYS_EQ(to8(&c, &u8, "c", 1), PENDING);
    PF_ASSERT_ALWAYS_EQ(u8, 0x82);
    PF_ASSERT_ALWAYS_EQ(to8(&c, &u8, "c", 1), PENDING);
    PF_ASSERT_ALWAYS_EQ(u8, 0xAC);
    PF_ASSERT_ALWAYS_EQ(to8(&c, &u8, "c", 1), 1);
    PF_ASSERT_ALWAYS_EQ(u8, 'c');

    /* ---- UTF-32 -> multibyte ---- */
    conv_init(&c, be);
    PF_ASSERT_ALWAYS_EQ(from32(&c, out, 0x1F600), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(out, "\xF0\x9F\x98\x80", 4);
    PF_ASSERT_ALWAYS_EQ(from32(&c, out, 'x'), 1);
    PF_ASSERT_ALWAYS_EQ(out[0], 'x');
    PF_ASSERT_ALWAYS_EQ(from32(&c, out, 0), 1);
    PF_ASSERT_ALWAYS_EQ(out[0], '\0');
    PF_ASSERT_ALWAYS_EQ(from32(&c, NULL, 'q'), 1);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(from32(&c, out, 0x110000), ERR);
    PF_ASSERT_ALWAYS_EQ(errno, EILSEQ);

    /* ---- UTF-16 -> multibyte ---- */
    conv_init(&c, be);
    PF_ASSERT_ALWAYS_EQ(from16(&c, out, 0xD83D), 0);
    PF_ASSERT_ALWAYS_EQ(from16(&c, out, 0xDE00), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(out, "\xF0\x9F\x98\x80", 4);
    PF_ASSERT_ALWAYS_EQ(from16(&c, out, 0xE9), 2);
    PF_ASSERT_ALWAYS_MEM_EQ(out, "\xC3\xA9", 2);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(from16(&c, out, 0xDE00), ERR);
    PF_ASSERT_ALWAYS_EQ(errno, EILSEQ);

    /* ---- UTF-8 -> multibyte ---- */
    conv_init(&c, be);
    PF_ASSERT_ALWAYS_EQ(from8(&c, out, 0xE2), 0);
    PF_ASSERT_ALWAYS_EQ(from8(&c, out, 0x82), 0);
    PF_ASSERT_ALWAYS_EQ(from8(&c, out, 0xAC), 3);
    PF_ASSERT_ALWAYS_MEM_EQ(out, "\xE2\x82\xAC", 3);
    PF_ASSERT_ALWAYS_EQ(from8(&c, out, 'a'), 1);
    PF_ASSERT_ALWAYS_EQ(out[0], 'a');
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(from8(&c, out, 0x80), ERR);
    PF_ASSERT_ALWAYS_EQ(errno, EILSEQ);
}

static int test_utf8_fallback(int seed, int i) {
    (void)seed, (void)i;
    /* independent of the locale */
    run_scenario(UTF8);
    return 0;
}

static int test_utf8_locale(int seed, int i) {
    char saved[128];
    (void)seed, (void)i;

    if (!utf8_locale(saved, sizeof(saved)))
        return SKIP("no UTF-8 locale");
    run_scenario(PUBLIC);
#if WCHAR_MAX > 0xFFFF
    run_scenario(WC);
#endif
    setlocale(LC_CTYPE, saved);
    return 0;
}

/* NULL `ps` uses an internal state per function. */
static int test_internal_state(int seed, int i) {
    char saved[128];
    uint16_t u16;
    (void)seed, (void)i;

    if (!utf8_locale(saved, sizeof(saved)))
        return SKIP("no UTF-8 locale");
    PF_ASSERT_ALWAYS_EQ(pf_mbrtoc16(&u16, "\xF0\x9F\x98\x80", 4, NULL), 4);
    PF_ASSERT_ALWAYS_EQ(u16, 0xD83D);
    PF_ASSERT_ALWAYS_EQ(pf_mbrtoc16(&u16, "", 1, NULL), PENDING);
    PF_ASSERT_ALWAYS_EQ(u16, 0xDE00);
    setlocale(LC_CTYPE, saved);
    return 0;
}

pf_test_t suite_uchar[] = {
    { test_utf8_fallback, "utf8_fallback" },
    { test_utf8_locale, "utf8_locale" },
    { test_internal_state, "internal_state" },
    { 0 },
};
