/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides an implementation of functions from <ctype.h>
    that ignore the localization features of the standard library.

    In addition to those functions, this file also provides:
    - functions for detecting UTF-8 characters and their length.
    - enumerations for ASCII control characters.
    - unicode characters from the special block

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef POLYFILL_CTYPE
#define POLYFILL_CTYPE

#ifndef PF_API
    #define PF_API static inline
#endif

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
};

enum pf_unicode_special {
    PF_UTF_ANNOTATION_ANCHOR = 0xFFF9,
    PF_UTF_ANNOTATION_SEPARATOR = 0xFFFA,
    PF_UTF_ANNOTATION_TERMINATOR = 0xFFFB,
    PF_UTF_OBJECT_REPLACEMENT = 0xFFFC,
    PF_UTF_REPLACEMENT = 0xFFFD,
    PF_UTF_NONCHARACTER1 = 0xFFFE,
    PF_UTF_NONCHARACTER2 = 0xFFFF,
};

PF_API int pf_utf8_clen(int c) {
    if ((c & 0xF8) == 0xF0)
        return 4;
    else if ((c & 0xF0) == 0xE0)
        return 3;
    else if ((c & 0xE0) == 0xC0)
        return 2;
    else if (!(c & 0x80))
        return 1;
    else
        return 0;
}

PF_API int pf_isutf8(int c) { return pf_utf8_clen(c) > 0; }

PF_API int pf_isblank(int c) { return c == 9 || c == 32; }
PF_API int pf_isspace(int c) { return (c >= 9 && c <= 13) || c == 32; }
PF_API int pf_iscntrl(int c) { return (c >= 0 && c <= 31) || c == 127; }
PF_API int pf_isgraph(int c) { return c >= 33 && c <= 126; }
PF_API int pf_isprint(int c) { return c >= 32 && c <= 126; }
PF_API int pf_isdigit(int c) { return c >= '0' && c <= '9'; }
PF_API int pf_islower(int c) { return c >= 'a' && c <= 'z'; }
PF_API int pf_isupper(int c) { return c >= 'A' && c <= 'Z'; }
PF_API int pf_isalpha(int c) { return pf_isupper(c) || pf_islower(c); }
PF_API int pf_isalnum(int c) { return pf_isalpha(c) || pf_isdigit(c); }
PF_API int pf_ispunct(int c) { return pf_isgraph(c) && !pf_isalnum(c); }

PF_API int pf_isxdigit(int c) {
    return pf_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

PF_API int pf_tolower(int c) {
    if (!pf_isupper(c))
        return c;
    return c - ('A' - 'a');
}

PF_API int pf_toupper(int c) {
    if (!pf_islower(c))
        return c;
    return c + ('A' - 'a');
}

#endif
