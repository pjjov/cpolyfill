/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <string.h>

/* Decodes one code point from `s[*i..n)`, advancing `*i`. Returns -1 on
   invalid UTF-8. */
static long pf__utf8_decode(const unsigned char *s, size_t n, size_t *i) {
    unsigned char c = s[*i];
    size_t len, k;
    unsigned long cp, min;

    if (c < 0x80) {
        *i += 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        len = 2, cp = c & 0x1F, min = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        len = 3, cp = c & 0x0F, min = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        len = 4, cp = c & 0x07, min = 0x10000;
    } else {
        return -1;
    }

    if (n - *i < len)
        return -1;
    for (k = 1; k < len; k++) {
        if ((s[*i + k] & 0xC0) != 0x80)
            return -1;
        cp = (cp << 6) | (s[*i + k] & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return -1;

    *i += len;
    return (long)cp;
}

ptrdiff_t pf__utf8_to_utf16(
    const char *src, size_t len, uint16_t *dst, size_t cap
) {
    const unsigned char *s = (const unsigned char *)src;
    size_t i = 0, out = 0;

    if (len == PF__NTS)
        len = strlen(src);

    while (i < len) {
        long cp = pf__utf8_decode(s, len, &i);
        size_t units = cp >= 0x10000 ? 2 : 1;

        if (cp < 0)
            return PF_EILSEQ;
        if (dst) {
            if (cap < out || cap - out < units + 1)
                return PF_ERANGE;
            if (units == 2) {
                cp -= 0x10000;
                dst[out] = (uint16_t)(0xD800 + (cp >> 10));
                dst[out + 1] = (uint16_t)(0xDC00 + (cp & 0x3FF));
            } else {
                dst[out] = (uint16_t)cp;
            }
        }
        out += units;
    }

    if (dst) {
        if (cap <= out)
            return PF_ERANGE;
        dst[out] = 0;
    }
    return (ptrdiff_t)out;
}

ptrdiff_t pf__utf16_to_utf8(
    const uint16_t *src, size_t len, char *dst, size_t cap
) {
    size_t i = 0, out = 0;

    if (len == PF__NTS) {
        len = 0;
        while (src[len])
            len++;
    }

    while (i < len) {
        unsigned long cp = src[i++];
        unsigned char buf[4];
        size_t units;

        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i == len || src[i] < 0xDC00 || src[i] > 0xDFFF)
                return PF_EILSEQ;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (src[i++] - 0xDC00);
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return PF_EILSEQ;
        }

        if (cp < 0x80) {
            buf[0] = (unsigned char)cp;
            units = 1;
        } else if (cp < 0x800) {
            buf[0] = (unsigned char)(0xC0 | (cp >> 6));
            buf[1] = (unsigned char)(0x80 | (cp & 0x3F));
            units = 2;
        } else if (cp < 0x10000) {
            buf[0] = (unsigned char)(0xE0 | (cp >> 12));
            buf[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            buf[2] = (unsigned char)(0x80 | (cp & 0x3F));
            units = 3;
        } else {
            buf[0] = (unsigned char)(0xF0 | (cp >> 18));
            buf[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            buf[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            buf[3] = (unsigned char)(0x80 | (cp & 0x3F));
            units = 4;
        }

        if (dst) {
            if (cap < out || cap - out < units + 1)
                return PF_ERANGE;
            memcpy(dst + out, buf, units);
        }
        out += units;
    }

    if (dst) {
        if (cap <= out)
            return PF_ERANGE;
        dst[out] = 0;
    }
    return (ptrdiff_t)out;
}

int pf__wstr_from_utf8(pf__wstr *out, const char *src) {
    ptrdiff_t n;

    out->ptr = out->small;
    out->heap_size = 0;

    n = pf__utf8_to_utf16(src, PF__NTS, out->small, PF__WSTR_SMALL);
    if (n != PF_ERANGE)
        return n < 0 ? (int)n : PF_OK;

    n = pf__utf8_to_utf16(src, PF__NTS, NULL, 0);
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
    n = pf__utf8_to_utf16(src, PF__NTS, out->ptr, (size_t)n + 1);
    return n < 0 ? (int)n : PF_OK;
}

void pf__wstr_free(pf__wstr *str) {
    if (str->heap_size)
        deallocate(NULL, str->ptr, str->heap_size);
    str->ptr = str->small;
    str->heap_size = 0;
    str->small[0] = 0;
}
