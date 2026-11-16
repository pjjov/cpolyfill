/** cpolyfill - Filling the gaps between C standards and compilers

    UTF-8, UTF-16 and UTF-32: classification, decoding, encoding,
    validation and conversion. UTF-16 and UTF-32 code units are `uint16_t`
    and `uint32_t`, in host byte order. Only Unicode scalar values (U+0000
    to U+10FFFF without the surrogates U+D800..U+DFFF) are valid; overlong
    UTF-8, encoded surrogates, unpaired UTF-16 surrogates and truncated
    sequences are errors.

    Inline helpers:

        int  pf_utf8_clen(int lead)    Length of the sequence starting with
                                       byte `lead` (1-4), or 0 if `lead`
                                       cannot start one (continuation bytes,
                                       C0, C1, F5-FF). `lead` may be a
                                       negative `char`.
        bool pf_utf8_iscont(int c)     `c` is a continuation byte (80-BF).
        bool pf_unicode_isvalid(uint32_t cp)      a scalar value
        bool pf_unicode_issurrogate(uint32_t cp)  U+D800..U+DFFF

        enum pf_unicode_special        PF_UNICODE_ANNOTATION_ANCHOR ...
                                       PF_UNICODE_REPLACEMENT (U+FFFD),
                                       PF_UNICODE_MAX (U+10FFFF)

    One code point at a time:

        int pf_utf8_decode(const char *s, size_t n, uint32_t *cp)
        int pf_utf16_decode(const uint16_t *s, size_t n, uint32_t *cp)
                Decode the code point at the start of the `n` units at `s`.
                Returns the number of units used (1-4 for UTF-8, 1-2 for
                UTF-16), 0 if `n` is 0, or PF_EILSEQ if the input does not
                start with a complete valid sequence.
        int pf_utf8_encode(uint32_t cp, char out[4])
        int pf_utf16_encode(uint32_t cp, uint16_t out[2])
                Encode `cp`. Returns the number of units written, or
                PF_EILSEQ if `cp` is not a scalar value.

    Whole strings (`n` may be PF_NTS for a NUL-terminated string):

        size_t pf_utf8_valid_prefix(const char *s, size_t n)
                The length in bytes of the longest valid UTF-8 prefix; the
                string is valid if it is `n` (or strlen(s) for PF_NTS).
        ptrdiff_t pf_utf8_count(const char *s, size_t n)
                The number of code points, or PF_EILSEQ.

    Conversions:

        ptrdiff_t pf_utf8_to_utf16(const char *src, size_t len,
                                   uint16_t *dst, size_t cap, unsigned flags)
        ptrdiff_t pf_utf16_to_utf8(const uint16_t *src, size_t len,
                                   char *dst, size_t cap, unsigned flags)
        ptrdiff_t pf_utf8_to_utf32(const char *src, size_t len,
                                   uint32_t *dst, size_t cap, unsigned flags)
        ptrdiff_t pf_utf32_to_utf8(const uint32_t *src, size_t len,
                                   char *dst, size_t cap, unsigned flags)
        ptrdiff_t pf_utf8_to_wcs(const char *src, size_t len,
                                 wchar_t *dst, size_t cap, unsigned flags)
        ptrdiff_t pf_wcs_to_utf8(const wchar_t *src, size_t len,
                                 char *dst, size_t cap, unsigned flags)
                Convert `len` code units of `src`, or up to its NUL with
                `len == PF_NTS` (an explicit length converts NULs too).
                `cap` is the size of `dst` in code units. The output is
                always NUL-terminated. Returns the number of code units
                written without the NUL, or
                  PF_EILSEQ  invalid input (without PF_UNICODE_REPLACE)
                  PF_ERANGE  `cap` is too small: `dst` holds no useful data
                With `dst == NULL` nothing is written and the length the
                output needs (without the NUL) is returned.
                `wchar_t` is UTF-16 where it has 16 bits (Windows) and
                UTF-32 elsewhere.
                Flags:
                  PF_UNICODE_STRICT   (0) fail on invalid input
                  PF_UNICODE_REPLACE  replace each maximal invalid
                                      subsequence with U+FFFD, as the
                                      Unicode standard recommends

    The <uchar.h> conversions, between the multibyte encoding of the
    current locale and UTF-8 (`unsigned char`), UTF-16 and UTF-32, with the
    semantics of C11 and C23:

        size_t pf_mbrtoc8(unsigned char *pc8, const char *s, size_t n,
                          mbstate_t *ps)
        size_t pf_mbrtoc16(uint16_t *pc16, const char *s, size_t n,
                           mbstate_t *ps)
        size_t pf_mbrtoc32(uint32_t *pc32, const char *s, size_t n,
                           mbstate_t *ps)
        size_t pf_c8rtomb(char *s, unsigned char c8, mbstate_t *ps)
        size_t pf_c16rtomb(char *s, uint16_t c16, mbstate_t *ps)
        size_t pf_c32rtomb(char *s, uint32_t c32, mbstate_t *ps)

    They call the system's functions where it has them. Elsewhere they are
    built on mbrtowc / wcrtomb where `wchar_t` holds code points, and as a
    last resort assume the multibyte encoding is UTF-8 (as the Windows CRT
    does for these functions).

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_UNICODE, in C only
    (C++ has char16_t and char32_t as keywords and <cuchar>): where the
    library found <uchar.h> or one of the functions missing, `mbrtoc16`,
    `c16rtomb`, `mbrtoc32`, `c32rtomb`, `mbrtoc8` and `c8rtomb` map to the
    functions above, and without <uchar.h> `char16_t` and `char32_t` (and
    in C23 `char8_t`) are defined.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_UNICODE_H
#define CPOLYFILL_UNICODE_H

#include <cpolyfill/errno.h>
#include <cpolyfill/platform.h>

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#if PF_CPP >= 201402L
    #define PF__UNICODE_FN PF_INLINE constexpr
#else
    #define PF__UNICODE_FN PF_INLINE
#endif

/* A length meaning "up to the terminating NUL". */
#define PF_NTS ((size_t)-1)

#define PF_UNICODE_STRICT 0u
#define PF_UNICODE_REPLACE 1u

PF_BEGIN_DECLS

enum pf_unicode_special {
    PF_UNICODE_ANNOTATION_ANCHOR = 0xFFF9,
    PF_UNICODE_ANNOTATION_SEPARATOR = 0xFFFA,
    PF_UNICODE_ANNOTATION_TERMINATOR = 0xFFFB,
    PF_UNICODE_OBJECT_REPLACEMENT = 0xFFFC,
    PF_UNICODE_REPLACEMENT = 0xFFFD,
    PF_UNICODE_NONCHARACTER1 = 0xFFFE,
    PF_UNICODE_NONCHARACTER2 = 0xFFFF,
    PF_UNICODE_MAX = 0x10FFFF
};

PF__UNICODE_FN int pf_utf8_clen(int lead) {
    return (lead < -128 || lead > 255) ? 0
        : (lead & 0xFF) < 0x80         ? 1
        : (lead & 0xFF) < 0xC2         ? 0
        : (lead & 0xFF) < 0xE0         ? 2
        : (lead & 0xFF) < 0xF0         ? 3
        : (lead & 0xFF) < 0xF5         ? 4
                                       : 0;
}

PF__UNICODE_FN bool pf_utf8_iscont(int c) {
    return c >= -128 && c <= 255 && (c & 0xC0) == 0x80;
}

PF__UNICODE_FN bool pf_unicode_issurrogate(uint32_t cp) {
    return cp >= 0xD800 && cp <= 0xDFFF;
}

PF__UNICODE_FN bool pf_unicode_isvalid(uint32_t cp) {
    return cp <= 0x10FFFF && !pf_unicode_issurrogate(cp);
}

PF_API int pf_utf8_decode(const char *s, size_t n, uint32_t *cp);
PF_API int pf_utf16_decode(const uint16_t *s, size_t n, uint32_t *cp);
PF_API int pf_utf8_encode(uint32_t cp, char out[4]);
PF_API int pf_utf16_encode(uint32_t cp, uint16_t out[2]);

PF_API size_t pf_utf8_valid_prefix(const char *s, size_t n);
PF_API ptrdiff_t pf_utf8_count(const char *s, size_t n);

PF_API ptrdiff_t pf_utf8_to_utf16(
    const char *src, size_t len, uint16_t *dst, size_t cap, unsigned flags
);
PF_API ptrdiff_t pf_utf16_to_utf8(
    const uint16_t *src, size_t len, char *dst, size_t cap, unsigned flags
);
PF_API ptrdiff_t pf_utf8_to_utf32(
    const char *src, size_t len, uint32_t *dst, size_t cap, unsigned flags
);
PF_API ptrdiff_t pf_utf32_to_utf8(
    const uint32_t *src, size_t len, char *dst, size_t cap, unsigned flags
);
PF_API ptrdiff_t pf_utf8_to_wcs(
    const char *src, size_t len, wchar_t *dst, size_t cap, unsigned flags
);
PF_API ptrdiff_t pf_wcs_to_utf8(
    const wchar_t *src, size_t len, char *dst, size_t cap, unsigned flags
);

PF_API size_t pf_mbrtoc8(
    unsigned char *pc8, const char *s, size_t n, mbstate_t *ps
);
PF_API size_t pf_mbrtoc16(uint16_t *pc16, const char *s, size_t n, mbstate_t *ps);
PF_API size_t pf_mbrtoc32(uint32_t *pc32, const char *s, size_t n, mbstate_t *ps);
PF_API size_t pf_c8rtomb(char *s, unsigned char c8, mbstate_t *ps);
PF_API size_t pf_c16rtomb(char *s, uint16_t c16, mbstate_t *ps);
PF_API size_t pf_c32rtomb(char *s, uint32_t c32, mbstate_t *ps);

PF_END_DECLS

#endif /* CPOLYFILL_UNICODE_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(__cplusplus) && !defined(PF__UNICODE_STD_NAMES)              \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_UNICODE))      \
        || defined(PF_STD_NAMES_UNICODE))
    #define PF__UNICODE_STD_NAMES

    #if PF_CONFIG_HAVE_UCHAR_H && !defined(PF__FORCE_STD_NAMES_POLYFILL)
        #include <uchar.h>
    #else
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
        #if PF_STD_C23
typedef unsigned char char8_t;
        #endif
    #endif

    #if !PF_CONFIG_HAVE_MBRTOC16 || defined(PF__FORCE_STD_NAMES_POLYFILL)
        #undef mbrtoc16
        #define mbrtoc16 pf_mbrtoc16
    #endif
    #if !PF_CONFIG_HAVE_C16RTOMB || defined(PF__FORCE_STD_NAMES_POLYFILL)
        #undef c16rtomb
        #define c16rtomb pf_c16rtomb
    #endif
    #if !PF_CONFIG_HAVE_MBRTOC32 || defined(PF__FORCE_STD_NAMES_POLYFILL)
        #undef mbrtoc32
        #define mbrtoc32 pf_mbrtoc32
    #endif
    #if !PF_CONFIG_HAVE_C32RTOMB || defined(PF__FORCE_STD_NAMES_POLYFILL)
        #undef c32rtomb
        #define c32rtomb pf_c32rtomb
    #endif
    #if !PF_CONFIG_HAVE_MBRTOC8 || defined(PF__FORCE_STD_NAMES_POLYFILL)
        #undef mbrtoc8
        #define mbrtoc8 pf_mbrtoc8
    #endif
    #if !PF_CONFIG_HAVE_C8RTOMB || defined(PF__FORCE_STD_NAMES_POLYFILL)
        #undef c8rtomb
        #define c8rtomb pf_c8rtomb
    #endif
#endif
