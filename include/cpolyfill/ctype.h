/** cpolyfill - Filling the gaps between C standards and compilers

    Character classification that ignores the locale: the functions behave
    like those of <ctype.h> in the "C" locale, whatever setlocale() was
    called with. They accept any `int`, so a negative `char`, EOF and values
    above 255 are safe to pass (they are not in any class and are returned
    unchanged by the conversions).

        bool pf_isalpha(int c)   A-Z a-z
        bool pf_isdigit(int c)   0-9
        bool pf_isxdigit(int c)  0-9 A-F a-f
        bool pf_isodigit(int c)  0-7
        bool pf_isalnum(int c)   pf_isalpha or pf_isdigit
        bool pf_islower(int c)   a-z
        bool pf_isupper(int c)   A-Z
        bool pf_isspace(int c)   space \t \n \v \f \r
        bool pf_isblank(int c)   space \t
        bool pf_iscntrl(int c)   0-31 and 127
        bool pf_isprint(int c)   32-126
        bool pf_isgraph(int c)   33-126
        bool pf_ispunct(int c)   pf_isgraph but not pf_isalnum
        bool pf_isascii(int c)   0-127
        int  pf_tolower(int c)   A-Z to a-z, anything else unchanged
        int  pf_toupper(int c)   a-z to A-Z, anything else unchanged
        int  pf_digit_value(int c, int base)
                                 The value of the digit `c` (0-9, then
                                 a-z or A-Z for 10-35) if it is below
                                 `base`, else -1. -1 for a base outside
                                 2..36.

    The functions are `constexpr` in C++14 and later.

        PF_CTYPE_DIGIT, PF_CTYPE_LOWER, PF_CTYPE_UPPER, PF_CTYPE_ALPHA,
        PF_CTYPE_ALNUM, PF_CTYPE_XDIGIT, PF_CTYPE_SPACE, PF_CTYPE_BLANK,
        PF_CTYPE_PUNCT, PF_CTYPE_GRAPH, PF_CTYPE_PRINT, PF_CTYPE_CNTRL
                                 String literals with every character of
                                 the class (for strspn, strpbrk, ...).
                                 PF_CTYPE_CNTRL ends with an explicit NUL.

        enum pf_ascii_control    PF_ASCII_NUL ... PF_ASCII_US (0-31),
                                 PF_ASCII_SP (32), PF_ASCII_DEL (127).

    UTF-8 helpers are in <cpolyfill/unicode.h>.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_CTYPE_H
#define CPOLYFILL_CTYPE_H

#include <cpolyfill/platform.h>

#ifndef __cplusplus
    #include <stdbool.h>
#endif

#if PF_CPP >= 201402L
    #define PF__CTYPE_FN PF_INLINE constexpr
#else
    #define PF__CTYPE_FN PF_INLINE
#endif

#define PF_CTYPE_DIGIT "0123456789"
#define PF_CTYPE_LOWER "abcdefghijklmnopqrstuvwxyz"
#define PF_CTYPE_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define PF_CTYPE_ALPHA PF_CTYPE_UPPER PF_CTYPE_LOWER
#define PF_CTYPE_ALNUM PF_CTYPE_UPPER PF_CTYPE_LOWER PF_CTYPE_DIGIT
#define PF_CTYPE_XDIGIT "0123456789ABCDEFabcdef"
#define PF_CTYPE_SPACE " \t\n\r\f\v"
#define PF_CTYPE_BLANK " \t"
#define PF_CTYPE_PUNCT "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
#define PF_CTYPE_GRAPH                                   \
    "!\"#$%&'()*+,-./"                                   \
    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`" \
    "abcdefghijklmnopqrstuvwxyz{|}~"
#define PF_CTYPE_PRINT " " PF_CTYPE_GRAPH
#define PF_CTYPE_CNTRL                                                         \
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12" \
    "\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f\x00"

PF_BEGIN_DECLS

enum pf_ascii_control {
    PF_ASCII_NUL = 0,
    PF_ASCII_SOH,
    PF_ASCII_STX,
    PF_ASCII_ETX,
    PF_ASCII_EOT,
    PF_ASCII_ENQ,
    PF_ASCII_ACK,
    PF_ASCII_BEL,
    PF_ASCII_BS,
    PF_ASCII_HT,
    PF_ASCII_LF,
    PF_ASCII_VT,
    PF_ASCII_FF,
    PF_ASCII_CR,
    PF_ASCII_SO,
    PF_ASCII_SI,
    PF_ASCII_DLE,
    PF_ASCII_DC1,
    PF_ASCII_DC2,
    PF_ASCII_DC3,
    PF_ASCII_DC4,
    PF_ASCII_NAK,
    PF_ASCII_SYN,
    PF_ASCII_ETB,
    PF_ASCII_CAN,
    PF_ASCII_EM,
    PF_ASCII_SUB,
    PF_ASCII_ESC,
    PF_ASCII_FS,
    PF_ASCII_GS,
    PF_ASCII_RS,
    PF_ASCII_US,
    PF_ASCII_SP = 32,
    PF_ASCII_DEL = 127
};

PF__CTYPE_FN bool pf_isascii(int c) { return c >= 0 && c <= 127; }
PF__CTYPE_FN bool pf_isblank(int c) { return c == ' ' || c == '\t'; }
PF__CTYPE_FN bool pf_isspace(int c) {
    return c == ' ' || (c >= '\t' && c <= '\r');
}
PF__CTYPE_FN bool pf_iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
PF__CTYPE_FN bool pf_isprint(int c) { return c >= 32 && c <= 126; }
PF__CTYPE_FN bool pf_isgraph(int c) { return c >= 33 && c <= 126; }
PF__CTYPE_FN bool pf_isdigit(int c) { return c >= '0' && c <= '9'; }
PF__CTYPE_FN bool pf_isodigit(int c) { return c >= '0' && c <= '7'; }
PF__CTYPE_FN bool pf_islower(int c) { return c >= 'a' && c <= 'z'; }
PF__CTYPE_FN bool pf_isupper(int c) { return c >= 'A' && c <= 'Z'; }
PF__CTYPE_FN bool pf_isalpha(int c) { return pf_islower(c) || pf_isupper(c); }
PF__CTYPE_FN bool pf_isalnum(int c) { return pf_isalpha(c) || pf_isdigit(c); }
PF__CTYPE_FN bool pf_ispunct(int c) { return pf_isgraph(c) && !pf_isalnum(c); }
PF__CTYPE_FN bool pf_isxdigit(int c) {
    return pf_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

PF__CTYPE_FN int pf_tolower(int c) { return pf_isupper(c) ? c - 'A' + 'a' : c; }
PF__CTYPE_FN int pf_toupper(int c) { return pf_islower(c) ? c - 'a' + 'A' : c; }

PF__CTYPE_FN int pf_digit_value(int c, int base) {
    return (base < 2 || base > 36) ? -1
        : pf_isdigit(c)            ? (c - '0' < base ? c - '0' : -1)
        : pf_isalpha(c)
        ? (pf_tolower(c) - 'a' + 10 < base ? pf_tolower(c) - 'a' + 10 : -1)
        : -1;
}

PF_END_DECLS

#endif /* CPOLYFILL_CTYPE_H */
