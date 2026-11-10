/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/unicode.h>

#include <string.h>

/* ---- Decoding --------------------------------------------------------- */

/* Decodes the sequence at the start of `s[0..n)`, n > 0. Returns the
   number of bytes it spans. On invalid input `*cp` is set to -1 and the
   returned length is that of the maximal subpart of an ill-formed
   sequence (Unicode 15, section 3.9, "U+FFFD Substitution of Maximal
   Subparts"): at least 1, and the bytes that could still have started a
   valid sequence. */
size_t pf__utf8_next(const unsigned char *s, size_t n, long *cp) {
    unsigned c = s[0];
    unsigned lo = 0x80, hi = 0xBF;
    size_t len, k;
    unsigned long v;

    if (c < 0x80) {
        *cp = (long)c;
        return 1;
    }
    len = (size_t)pf_utf8_clen((int)c);
    if (len == 0) {
        *cp = -1;
        return 1;
    }
    /* The second byte has a narrower range for some leads, which rules
       out overlong forms, surrogates and values above U+10FFFF. */
    switch (c) {
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

    v = c & (0x7Fu >> len);
    for (k = 1; k < len; k++) {
        unsigned b;
        if (k >= n) {
            *cp = -1;
            return k;
        }
        b = s[k];
        if (b < lo || b > hi) {
            *cp = -1;
            return k;
        }
        lo = 0x80, hi = 0xBF;
        v = (v << 6) | (b & 0x3F);
    }
    *cp = (long)v;
    return len;
}

/* The same for UTF-16. An unpaired surrogate is invalid, length 1. */
static size_t pf__utf16_next(const uint16_t *s, size_t n, long *cp) {
    unsigned long c = s[0];

    if (c >= 0xD800 && c <= 0xDBFF) {
        if (n >= 2 && s[1] >= 0xDC00 && s[1] <= 0xDFFF) {
            *cp = (long)(0x10000 + ((c - 0xD800) << 10) + (s[1] - 0xDC00u));
            return 2;
        }
        *cp = -1;
    } else if (c >= 0xDC00 && c <= 0xDFFF) {
        *cp = -1;
    } else {
        *cp = (long)c;
    }
    return 1;
}

PF_API int pf_utf8_decode(const char *s, size_t n, uint32_t *cp) {
    long v;
    size_t len;

    if (n == 0)
        return 0;
    len = pf__utf8_next((const unsigned char *)s, n, &v);
    if (v < 0)
        return PF_EILSEQ;
    if (cp)
        *cp = (uint32_t)v;
    return (int)len;
}

PF_API int pf_utf16_decode(const uint16_t *s, size_t n, uint32_t *cp) {
    long v;
    size_t len;

    if (n == 0)
        return 0;
    len = pf__utf16_next(s, n, &v);
    if (v < 0)
        return PF_EILSEQ;
    if (cp)
        *cp = (uint32_t)v;
    return (int)len;
}

/* ---- Encoding --------------------------------------------------------- */

/* Encodes a valid code point. Returns the number of bytes. */
size_t pf__utf8_put(uint32_t cp, unsigned char *out) {
    if (cp < 0x80) {
        out[0] = (unsigned char)cp;
        return 1;
    } else if (cp < 0x800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (unsigned char)(0xF0 | (cp >> 18));
    out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (cp & 0x3F));
    return 4;
}

PF_API int pf_utf8_encode(uint32_t cp, char out[4]) {
    unsigned char buf[4];
    size_t len;

    if (!pf_unicode_isvalid(cp))
        return PF_EILSEQ;
    len = pf__utf8_put(cp, buf);
    memcpy(out, buf, len);
    return (int)len;
}

PF_API int pf_utf16_encode(uint32_t cp, uint16_t out[2]) {
    if (!pf_unicode_isvalid(cp))
        return PF_EILSEQ;
    if (cp < 0x10000) {
        out[0] = (uint16_t)cp;
        return 1;
    }
    cp -= 0x10000;
    out[0] = (uint16_t)(0xD800 + (cp >> 10));
    out[1] = (uint16_t)(0xDC00 + (cp & 0x3FF));
    return 2;
}

/* ---- Whole strings ---------------------------------------------------- */

PF_API size_t pf_utf8_valid_prefix(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0;

    if (n == PF_NTS)
        n = strlen(s);
    while (i < n) {
        long cp;
        size_t len;

        /* ASCII fast path */
        if (p[i] < 0x80) {
            i++;
            continue;
        }
        len = pf__utf8_next(p + i, n - i, &cp);
        if (cp < 0)
            break;
        i += len;
    }
    return i;
}

PF_API ptrdiff_t pf_utf8_count(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0;
    ptrdiff_t count = 0;

    if (n == PF_NTS)
        n = strlen(s);
    while (i < n) {
        long cp;
        i += pf__utf8_next(p + i, n - i, &cp);
        if (cp < 0)
            return PF_EILSEQ;
        count++;
    }
    return count;
}

/* ---- Conversions ------------------------------------------------------ */

enum pf__enc { PF__ENC_UTF8, PF__ENC_UTF16, PF__ENC_UTF32, PF__ENC_WCHAR };

#if WCHAR_MAX <= 0xFFFF
    #define PF__WCHAR_UTF16 1
#else
    #define PF__WCHAR_UTF16 0
#endif

static size_t pf__strlen_units(enum pf__enc enc, const void *src) {
    size_t n = 0;

    switch (enc) {
    case PF__ENC_UTF8:
        return strlen((const char *)src);
    case PF__ENC_UTF16:
        while (((const uint16_t *)src)[n])
            n++;
        return n;
    case PF__ENC_UTF32:
        while (((const uint32_t *)src)[n])
            n++;
        return n;
    case PF__ENC_WCHAR:
        return wcslen((const wchar_t *)src);
    }
    return 0;
}

/* Decodes the code point at unit `i` of `src`, sets `*cp` (-1 if
   invalid) and returns the number of units it spans. */
static size_t pf__next(
    enum pf__enc enc, const void *src, size_t i, size_t n, long *cp
) {
    switch (enc) {
    case PF__ENC_UTF8:
        return pf__utf8_next((const unsigned char *)src + i, n - i, cp);
    case PF__ENC_UTF16:
        return pf__utf16_next((const uint16_t *)src + i, n - i, cp);
    case PF__ENC_UTF32: {
        uint32_t c = ((const uint32_t *)src)[i];
        *cp = pf_unicode_isvalid(c) ? (long)c : -1;
        return 1;
    }
    case PF__ENC_WCHAR: {
        const wchar_t *w = (const wchar_t *)src + i;
#if PF__WCHAR_UTF16
        uint16_t pair[2];
        pair[0] = (uint16_t)w[0];
        pair[1] = n - i >= 2 ? (uint16_t)w[1] : 0;
        return pf__utf16_next(pair, n - i >= 2 ? 2 : 1, cp);
#else
        /* wchar_t may be signed */
        *cp = (w[0] >= 0 && pf_unicode_isvalid((uint32_t)w[0])) ? (long)w[0]
                                                                : -1;
        return 1;
#endif
    }
    }
    *cp = -1;
    return 1;
}

/* Encodes `cp` into `units`, returns how many. */
static size_t pf__encode(enum pf__enc enc, uint32_t cp, uint32_t units[4]) {
    unsigned char b[4];
    uint16_t w[2];
    size_t k, len;

    switch (enc) {
    case PF__ENC_UTF8:
        len = pf__utf8_put(cp, b);
        for (k = 0; k < len; k++)
            units[k] = b[k];
        return len;
    case PF__ENC_WCHAR:
#if !PF__WCHAR_UTF16
        units[0] = cp;
        return 1;
#endif
        /* fall through */
    case PF__ENC_UTF16:
        len = (size_t)pf_utf16_encode(cp, w);
        units[0] = w[0];
        units[1] = w[1];
        return len;
    case PF__ENC_UTF32:
        units[0] = cp;
        return 1;
    }
    return 0;
}

static void pf__store(enum pf__enc enc, void *dst, size_t i, uint32_t unit) {
    switch (enc) {
    case PF__ENC_UTF8:
        ((unsigned char *)dst)[i] = (unsigned char)unit;
        break;
    case PF__ENC_UTF16:
        ((uint16_t *)dst)[i] = (uint16_t)unit;
        break;
    case PF__ENC_UTF32:
        ((uint32_t *)dst)[i] = unit;
        break;
    case PF__ENC_WCHAR:
        ((wchar_t *)dst)[i] = (wchar_t)unit;
        break;
    }
}

static ptrdiff_t pf__convert(
    enum pf__enc from,
    const void *src,
    size_t len,
    enum pf__enc to,
    void *dst,
    size_t cap,
    unsigned flags
) {
    size_t i = 0, out = 0;

    if (len == PF_NTS)
        len = pf__strlen_units(from, src);

    while (i < len) {
        uint32_t units[4];
        size_t count, k;
        long cp;

        i += pf__next(from, src, i, len, &cp);
        if (cp < 0) {
            if (!(flags & PF_UNICODE_REPLACE))
                return PF_EILSEQ;
            cp = PF_UNICODE_REPLACEMENT;
        }
        count = pf__encode(to, (uint32_t)cp, units);
        if (dst) {
            if (cap < out || cap - out < count + 1)
                return PF_ERANGE;
            for (k = 0; k < count; k++)
                pf__store(to, dst, out + k, units[k]);
        }
        if ((size_t)PTRDIFF_MAX - out < count)
            return PF_ERANGE;
        out += count;
    }

    if (dst) {
        if (cap <= out)
            return PF_ERANGE;
        pf__store(to, dst, out, 0);
    }
    return (ptrdiff_t)out;
}

PF_API ptrdiff_t pf_utf8_to_utf16(
    const char *src, size_t len, uint16_t *dst, size_t cap, unsigned flags
) {
    return pf__convert(PF__ENC_UTF8, src, len, PF__ENC_UTF16, dst, cap, flags);
}

PF_API ptrdiff_t pf_utf16_to_utf8(
    const uint16_t *src, size_t len, char *dst, size_t cap, unsigned flags
) {
    return pf__convert(PF__ENC_UTF16, src, len, PF__ENC_UTF8, dst, cap, flags);
}

PF_API ptrdiff_t pf_utf8_to_utf32(
    const char *src, size_t len, uint32_t *dst, size_t cap, unsigned flags
) {
    return pf__convert(PF__ENC_UTF8, src, len, PF__ENC_UTF32, dst, cap, flags);
}

PF_API ptrdiff_t pf_utf32_to_utf8(
    const uint32_t *src, size_t len, char *dst, size_t cap, unsigned flags
) {
    return pf__convert(PF__ENC_UTF32, src, len, PF__ENC_UTF8, dst, cap, flags);
}

PF_API ptrdiff_t pf_utf8_to_wcs(
    const char *src, size_t len, wchar_t *dst, size_t cap, unsigned flags
) {
    return pf__convert(PF__ENC_UTF8, src, len, PF__ENC_WCHAR, dst, cap, flags);
}

PF_API ptrdiff_t pf_wcs_to_utf8(
    const wchar_t *src, size_t len, char *dst, size_t cap, unsigned flags
) {
    return pf__convert(PF__ENC_WCHAR, src, len, PF__ENC_UTF8, dst, cap, flags);
}

/* ---- Internal UTF-16 strings ------------------------------------------ */

int pf__wstr_from_utf8(pf__wstr *out, const char *src) {
    ptrdiff_t n;

    out->ptr = out->small;
    out->heap_size = 0;

    n = pf_utf8_to_utf16(src, PF_NTS, out->small, PF__WSTR_SMALL, 0);
    if (n != PF_ERANGE)
        return n < 0 ? (int)n : PF_OK;

    n = pf_utf8_to_utf16(src, PF_NTS, NULL, 0, 0);
    if (n < 0)
        return (int)n;

    out->heap_size = ((size_t)n + 1) * sizeof(uint16_t);
    out->ptr = (uint16_t *)allocate(NULL, out->heap_size);
    if (!out->ptr) {
        out->ptr = out->small;
        out->heap_size = 0;
        out->small[0] = 0;
        return PF_ENOMEM;
    }
    n = pf_utf8_to_utf16(src, PF_NTS, out->ptr, (size_t)n + 1, 0);
    return n < 0 ? (int)n : PF_OK;
}

void pf__wstr_free(pf__wstr *str) {
    if (str->heap_size)
        deallocate(NULL, str->ptr, str->heap_size);
    str->ptr = str->small;
    str->heap_size = 0;
    str->small[0] = 0;
}
