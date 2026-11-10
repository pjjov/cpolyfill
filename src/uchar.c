/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* mbrtoc8/16/32 and c8/16/32rtomb.

   Where the system has them they are called directly. Otherwise two
   fallback "backends" convert between the multibyte encoding and code
   points:

     wc    mbrtowc / wcrtomb, where `wchar_t` holds code points (32 bits).
           The state of mbrtowc is the caller's mbstate_t; the state of the
           UTF-16 / UTF-8 layer on top (a pending surrogate or UTF-8 byte)
           lives in the last 8 bytes of it, which needs a large mbstate_t
           (macOS and the BSDs have 128 bytes and use the first 12).
     utf8  the multibyte encoding is assumed to be UTF-8, which is what the
           Windows CRT does for these functions. The whole mbstate_t is
           ours.

   Both backends are always compiled, so the tests can run them. */

#include "internal.h"

#include <cpolyfill/stdc.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>

#if PF_CONFIG_HAVE_UCHAR_H
    #include <uchar.h>
#endif

#define PF__MB_ERROR ((size_t)-1)
#define PF__MB_INCOMPLETE ((size_t)-2)
#define PF__MB_PENDING ((size_t)-3)

/* The backend used by the public functions where the system lacks them. */
#if WCHAR_MAX > 0xFFFF
    #define PF__MB_WC_USABLE (sizeof(mbstate_t) >= 16)
#else
    #define PF__MB_WC_USABLE 0
#endif

static size_t pf__mb_fail(void) {
    errno = EILSEQ;
    return PF__MB_ERROR;
}

/* Feeds one UTF-8 byte. Returns 1 when a code point is complete (in
   `ext->value`), 0 if more bytes are needed, -1 on invalid input (the
   state is reset). */
static int pf__mb_feed(pf__mbext *ext, unsigned b) {
    if (ext->kind != PF__MBEXT_UTF8_IN) {
        int len = pf_utf8_clen((int)b);
        ext->kind = PF__MBEXT_NONE;
        if (len == 0)
            return -1;
        if (len == 1) {
            ext->value = b;
            return 1;
        }
        ext->value = b & (0x7Fu >> len);
        ext->kind = PF__MBEXT_UTF8_IN;
        ext->lead = (unsigned char)b;
        ext->count = (unsigned char)(len - 1);
        ext->seen = 1;
        return 0;
    } else {
        unsigned lo = 0x80, hi = 0xBF;
        if (ext->seen == 1) {
            switch (ext->lead) {
            case 0xE0:
                lo = 0xA0;
                break;
            case 0xED:
                hi = 0x9F;
                break;
            case 0xF0:
                lo = 0x90;
                break;
            case 0xF4:
                hi = 0x8F;
                break;
            default:
                break;
            }
        }
        if (b < lo || b > hi) {
            ext->kind = PF__MBEXT_NONE;
            return -1;
        }
        ext->value = (ext->value << 6) | (b & 0x3F);
        ext->seen++;
        if (--ext->count == 0) {
            ext->kind = PF__MBEXT_NONE;
            return 1;
        }
        return 0;
    }
}

/* Multibyte -> code point, with the return convention of mbrtoc32. */
static size_t pf__mb_to32(
    pf__mbconv *cv, uint32_t *out, const char *s, size_t n
) {
    size_t i;

    if (!cv->utf8) {
        wchar_t wc;
        size_t r = mbrtowc(&wc, s, n, cv->inner);
        if (r == PF__MB_ERROR || r == PF__MB_INCOMPLETE)
            return r;
        if (wc < 0 || !pf_unicode_isvalid((uint32_t)wc))
            return pf__mb_fail();
        *out = (uint32_t)wc;
        return r;
    }

    for (i = 0; i < n; i++) {
        int rc = pf__mb_feed(&cv->ext, (unsigned char)s[i]);
        if (rc < 0)
            return pf__mb_fail();
        if (rc > 0) {
            *out = cv->ext.value;
            return *out == 0 ? 0 : i + 1;
        }
    }
    return PF__MB_INCOMPLETE;
}

/* Code point -> multibyte, with the return convention of c32rtomb. */
static size_t pf__mb_from32(pf__mbconv *cv, char *s, uint32_t cp) {
    if (!pf_unicode_isvalid(cp))
        return pf__mb_fail();
    if (!cv->utf8)
        return wcrtomb(s, (wchar_t)cp, cv->inner);
    return pf__utf8_put(cp, (unsigned char *)s);
}

size_t pf__mbrtoc32_with(
    pf__mbconv *cv, uint32_t *pc32, const char *s, size_t n
) {
    uint32_t c;
    size_t r;

    if (!s)
        s = "", n = 1, pc32 = NULL;
    r = pf__mb_to32(cv, &c, s, n);
    if (pc32 && r != PF__MB_ERROR && r != PF__MB_INCOMPLETE)
        *pc32 = c;
    return r;
}

size_t pf__mbrtoc16_with(
    pf__mbconv *cv, uint16_t *pc16, const char *s, size_t n
) {
    uint32_t c;
    size_t r;

    if (!s)
        s = "", n = 1, pc16 = NULL;
    if (cv->ext.kind == PF__MBEXT_C16_OUT) {
        if (pc16)
            *pc16 = (uint16_t)cv->ext.value;
        cv->ext.kind = PF__MBEXT_NONE;
        return PF__MB_PENDING;
    }
    r = pf__mb_to32(cv, &c, s, n);
    if (r == PF__MB_ERROR || r == PF__MB_INCOMPLETE)
        return r;
    if (c >= 0x10000) {
        c -= 0x10000;
        cv->ext.kind = PF__MBEXT_C16_OUT;
        cv->ext.value = 0xDC00 + (c & 0x3FF);
        c = 0xD800 + (c >> 10);
    }
    if (pc16)
        *pc16 = (uint16_t)c;
    return r;
}

size_t pf__mbrtoc8_with(
    pf__mbconv *cv, unsigned char *pc8, const char *s, size_t n
) {
    unsigned char buf[4];
    uint32_t c;
    size_t r, len;

    if (!s)
        s = "", n = 1, pc8 = NULL;
    if (cv->ext.kind == PF__MBEXT_C8_OUT) {
        len = pf__utf8_put(cv->ext.value, buf);
        if (pc8)
            *pc8 = buf[cv->ext.count];
        if (++cv->ext.count >= len)
            cv->ext.kind = PF__MBEXT_NONE;
        return PF__MB_PENDING;
    }
    r = pf__mb_to32(cv, &c, s, n);
    if (r == PF__MB_ERROR || r == PF__MB_INCOMPLETE)
        return r;
    len = pf__utf8_put(c, buf);
    if (len > 1) {
        cv->ext.kind = PF__MBEXT_C8_OUT;
        cv->ext.value = c;
        cv->ext.count = 1;
    }
    if (pc8)
        *pc8 = buf[0];
    return r;
}

size_t pf__c32rtomb_with(pf__mbconv *cv, char *s, uint32_t c32) {
    char buf[MB_LEN_MAX > 4 ? MB_LEN_MAX : 4];

    if (!s)
        s = buf, c32 = 0;
    cv->ext.kind = PF__MBEXT_NONE;
    return pf__mb_from32(cv, s, c32);
}

size_t pf__c16rtomb_with(pf__mbconv *cv, char *s, uint16_t c16) {
    char buf[MB_LEN_MAX > 4 ? MB_LEN_MAX : 4];

    if (!s)
        s = buf, c16 = 0;
    if (cv->ext.kind == PF__MBEXT_C16_IN) {
        uint32_t high = cv->ext.value;
        cv->ext.kind = PF__MBEXT_NONE;
        if (c16 < 0xDC00 || c16 > 0xDFFF)
            return pf__mb_fail();
        return pf__mb_from32(
            cv, s, 0x10000 + ((high - 0xD800) << 10) + (c16 - 0xDC00u)
        );
    }
    cv->ext.kind = PF__MBEXT_NONE;
    if (c16 >= 0xD800 && c16 <= 0xDBFF) {
        cv->ext.kind = PF__MBEXT_C16_IN;
        cv->ext.value = c16;
        return 0;
    }
    if (c16 >= 0xDC00 && c16 <= 0xDFFF)
        return pf__mb_fail();
    return pf__mb_from32(cv, s, c16);
}

size_t pf__c8rtomb_with(pf__mbconv *cv, char *s, unsigned char c8) {
    char buf[MB_LEN_MAX > 4 ? MB_LEN_MAX : 4];
    int rc;

    if (!s)
        s = buf, c8 = 0;
    rc = pf__mb_feed(&cv->ext, c8);
    if (rc < 0)
        return pf__mb_fail();
    if (rc == 0)
        return 0;
    return pf__mb_from32(cv, s, cv->ext.value);
}

/* ---- Public functions ------------------------------------------------- */

#if !PF_CONFIG_HAVE_MBRTOC8 || !PF_CONFIG_HAVE_MBRTOC16     \
    || !PF_CONFIG_HAVE_MBRTOC32 || !PF_CONFIG_HAVE_C8RTOMB  \
    || !PF_CONFIG_HAVE_C16RTOMB || !PF_CONFIG_HAVE_C32RTOMB

_Static_assert(
    sizeof(mbstate_t) >= sizeof(pf__mbext),
    "mbstate_t is too small for the uchar fallback"
);

static void pf__mb_open(pf__mbconv *cv, mbstate_t *ps) {
    cv->inner = ps;
    cv->utf8 = !PF__MB_WC_USABLE;
    if (cv->utf8)
        memcpy(&cv->ext, ps, sizeof(cv->ext));
    else
        memcpy(&cv->ext, (char *)(ps + 1) - sizeof(cv->ext), sizeof(cv->ext));
}

static void pf__mb_close(const pf__mbconv *cv, mbstate_t *ps) {
    if (cv->utf8)
        memcpy(ps, &cv->ext, sizeof(cv->ext));
    else
        memcpy((char *)(ps + 1) - sizeof(cv->ext), &cv->ext, sizeof(cv->ext));
}

    #define PF__MB_FALLBACK(call)  \
        do {                       \
            pf__mbconv cv;         \
            size_t r;              \
            pf__mb_open(&cv, ps);  \
            r = call;              \
            pf__mb_close(&cv, ps); \
            return r;              \
        } while (0)
#endif

/* The internal state used when `ps` is NULL, one per function. */
#if PF_HAS_THREAD_LOCAL
    #define PF__MB_STATE static PF_THREAD_LOCAL mbstate_t
#else
    #define PF__MB_STATE static mbstate_t
#endif

PF_API size_t
pf_mbrtoc32(uint32_t *pc32, const char *s, size_t n, mbstate_t *ps) {
    PF__MB_STATE internal;
    if (!ps)
        ps = &internal;
#if PF_CONFIG_HAVE_MBRTOC32
    {
        char32_t c;
        size_t r = mbrtoc32(&c, s, n, ps);
        if (pc32 && r != PF__MB_ERROR && r != PF__MB_INCOMPLETE)
            *pc32 = (uint32_t)c;
        return r;
    }
#else
    PF__MB_FALLBACK(pf__mbrtoc32_with(&cv, pc32, s, n));
#endif
}

PF_API size_t
pf_mbrtoc16(uint16_t *pc16, const char *s, size_t n, mbstate_t *ps) {
    PF__MB_STATE internal;
    if (!ps)
        ps = &internal;
#if PF_CONFIG_HAVE_MBRTOC16
    {
        char16_t c;
        size_t r = mbrtoc16(&c, s, n, ps);
        if (pc16 && r != PF__MB_ERROR && r != PF__MB_INCOMPLETE)
            *pc16 = (uint16_t)c;
        return r;
    }
#else
    PF__MB_FALLBACK(pf__mbrtoc16_with(&cv, pc16, s, n));
#endif
}

PF_API size_t
pf_mbrtoc8(unsigned char *pc8, const char *s, size_t n, mbstate_t *ps) {
    PF__MB_STATE internal;
    if (!ps)
        ps = &internal;
#if PF_CONFIG_HAVE_MBRTOC8
    {
        unsigned char c;
        size_t r = mbrtoc8(&c, s, n, ps);
        if (pc8 && r != PF__MB_ERROR && r != PF__MB_INCOMPLETE)
            *pc8 = c;
        return r;
    }
#else
    PF__MB_FALLBACK(pf__mbrtoc8_with(&cv, pc8, s, n));
#endif
}

PF_API size_t pf_c32rtomb(char *s, uint32_t c32, mbstate_t *ps) {
    PF__MB_STATE internal;
    if (!ps)
        ps = &internal;
#if PF_CONFIG_HAVE_C32RTOMB
    /* glibc encodes values above U+10FFFF */
    if (s && !pf_unicode_isvalid(c32))
        return pf__mb_fail();
    return c32rtomb(s, (char32_t)c32, ps);
#else
    PF__MB_FALLBACK(pf__c32rtomb_with(&cv, s, c32));
#endif
}

PF_API size_t pf_c16rtomb(char *s, uint16_t c16, mbstate_t *ps) {
    PF__MB_STATE internal;
    if (!ps)
        ps = &internal;
#if PF_CONFIG_HAVE_C16RTOMB
    return c16rtomb(s, (char16_t)c16, ps);
#else
    PF__MB_FALLBACK(pf__c16rtomb_with(&cv, s, c16));
#endif
}

PF_API size_t pf_c8rtomb(char *s, unsigned char c8, mbstate_t *ps) {
    PF__MB_STATE internal;
    if (!ps)
        ps = &internal;
#if PF_CONFIG_HAVE_C8RTOMB
    return c8rtomb(s, c8, ps);
#else
    PF__MB_FALLBACK(pf__c8rtomb_with(&cv, s, c8));
#endif
}
