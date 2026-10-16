/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025-2026 Предраг Јовановић

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "../include/pf_assert.h"
#include "../include/pf_ctype.h"
#include "../include/pf_test.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define TEST_CTYPE(m_name)                                  \
    int test_ctype_##m_name(int seed, int rep) {            \
        for (int i = 0; i < 128; i++) {                     \
            pf_assert((!!pf_##m_name(i)) == (!!m_name(i))); \
        }                                                   \
        return 0;                                           \
    }

TEST_CTYPE(isalnum);
TEST_CTYPE(isalpha);
TEST_CTYPE(isblank);
TEST_CTYPE(iscntrl);
TEST_CTYPE(isdigit);
TEST_CTYPE(isgraph);
TEST_CTYPE(islower);
TEST_CTYPE(isprint);
TEST_CTYPE(ispunct);
TEST_CTYPE(isspace);
TEST_CTYPE(isupper);
TEST_CTYPE(isxdigit);
TEST_CTYPE(tolower);
TEST_CTYPE(toupper);

/* Only ASCII is classified, whatever the locale says. */
int test_ctype_outside_ascii(int seed, int rep) {
    for (int i = 128; i < 256; i++) {
        pf_assert(!pf_isalnum(i));
        pf_assert(!pf_isalpha(i));
        pf_assert(!pf_isblank(i));
        pf_assert(!pf_iscntrl(i));
        pf_assert(!pf_isdigit(i));
        pf_assert(!pf_isgraph(i));
        pf_assert(!pf_islower(i));
        pf_assert(!pf_isprint(i));
        pf_assert(!pf_ispunct(i));
        pf_assert(!pf_isspace(i));
        pf_assert(!pf_isupper(i));
        pf_assert(!pf_isxdigit(i));
        pf_assert(pf_tolower(i) == i);
        pf_assert(pf_toupper(i) == i);
    }

    /* EOF and other negative values are not characters either. */
    for (int i = -1; i >= -128; i--) {
        pf_assert(!pf_isalnum(i));
        pf_assert(!pf_isprint(i));
        pf_assert(!pf_isspace(i));
        pf_assert(pf_tolower(i) == i);
        pf_assert(pf_toupper(i) == i);
    }
    pf_assert(!pf_isalpha(EOF));
    pf_assert(pf_tolower(EOF) == EOF);
    return 0;
}

int test_ctype_conversion(int seed, int rep) {
    for (int c = 'a'; c <= 'z'; c++) {
        pf_assert(pf_toupper(c) == c - 32);
        pf_assert(pf_tolower(pf_toupper(c)) == c);
        pf_assert(pf_toupper(pf_toupper(c)) == pf_toupper(c));
    }
    for (int c = 'A'; c <= 'Z'; c++) {
        pf_assert(pf_tolower(c) == c + 32);
        pf_assert(pf_toupper(pf_tolower(c)) == c);
    }
    pf_assert(pf_toupper('0') == '0');
    pf_assert(pf_tolower('@') == '@'); /* just below 'A' */
    pf_assert(pf_tolower('[') == '[');  /* just above 'Z' */
    pf_assert(pf_toupper('`') == '`');  /* just below 'a' */
    pf_assert(pf_toupper('{') == '{');  /* just above 'z' */
    return 0;
}

/* The character set macros hold exactly the members of the class. */
static int in_set(const char *set, int c) {
    return c != 0 && strchr(set, c) != NULL;
}

int test_ctype_sets(int seed, int rep) {
    for (int c = 1; c < 128; c++) {
        pf_assert(in_set(PF_CTYPE_DIGIT, c) == !!pf_isdigit(c));
        pf_assert(in_set(PF_CTYPE_LOWER, c) == !!pf_islower(c));
        pf_assert(in_set(PF_CTYPE_UPPER, c) == !!pf_isupper(c));
        pf_assert(in_set(PF_CTYPE_ALPHA, c) == !!pf_isalpha(c));
        pf_assert(in_set(PF_CTYPE_ALNUM, c) == !!pf_isalnum(c));
        pf_assert(in_set(PF_CTYPE_XDIGIT, c) == !!pf_isxdigit(c));
        pf_assert(in_set(PF_CTYPE_SPACE, c) == !!pf_isspace(c));
        pf_assert(in_set(PF_CTYPE_BLANK, c) == !!pf_isblank(c));
        pf_assert(in_set(PF_CTYPE_PUNCT, c) == !!pf_ispunct(c));
        pf_assert(in_set(PF_CTYPE_GRAPH, c) == !!pf_isgraph(c));
        pf_assert(in_set(PF_CTYPE_PRINT, c) == !!pf_isprint(c));
        pf_assert(in_set(PF_CTYPE_CNTRL, c) == !!pf_iscntrl(c));
    }
    pf_assert(strlen(PF_CTYPE_DIGIT) == 10);
    pf_assert(strlen(PF_CTYPE_ALPHA) == 52);
    pf_assert(strlen(PF_CTYPE_PUNCT) == 32);
    pf_assert(strlen(PF_CTYPE_PRINT) == 95);
    pf_assert(strlen(PF_CTYPE_GRAPH) == 94);
    return 0;
}

int test_ctype_ascii_names(int seed, int rep) {
    pf_assert(PF_ASCII_NUL == 0);
    pf_assert(PF_ASCII_BEL == 7);
    pf_assert(PF_ASCII_BS == 8);
    pf_assert(PF_ASCII_HT == '\t');
    pf_assert(PF_ASCII_LF == '\n');
    pf_assert(PF_ASCII_VT == '\v');
    pf_assert(PF_ASCII_FF == '\f');
    pf_assert(PF_ASCII_CR == '\r');
    pf_assert(PF_ASCII_ESC == 27);
    pf_assert(PF_ASCII_US == 31);
    return 0;
}

int test_ctype_utf8(int seed, int rep) {
    /* The length of a sequence is given by its first byte. */
    for (int c = 0x00; c <= 0x7F; c++)
        pf_assert(pf_utf8_clen(c) == 1);
    for (int c = 0x80; c <= 0xBF; c++)
        pf_assert(pf_utf8_clen(c) == 0); /* continuation byte */
    for (int c = 0xC0; c <= 0xDF; c++)
        pf_assert(pf_utf8_clen(c) == 2);
    for (int c = 0xE0; c <= 0xEF; c++)
        pf_assert(pf_utf8_clen(c) == 3);
    for (int c = 0xF0; c <= 0xF7; c++)
        pf_assert(pf_utf8_clen(c) == 4);
    for (int c = 0xF8; c <= 0xFF; c++)
        pf_assert(pf_utf8_clen(c) == 0);

    for (int c = 0; c < 256; c++)
        pf_assert(!!pf_isutf8(c) == (pf_utf8_clen(c) > 0));

    /* Works on plain (possibly signed) chars too. */
    {
        const char *s = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
        pf_assert(pf_utf8_clen(s[0]) == 2);
        pf_assert(pf_utf8_clen(s[2]) == 3);
        pf_assert(pf_utf8_clen(s[5]) == 4);
        pf_assert(pf_utf8_clen(s[1]) == 0);
    }
    return 0;
}

pf_test_t suite_ctype[] = {
    { &test_ctype_isalnum, "/ctype/isalnum" },
    { &test_ctype_isalpha, "/ctype/isalpha" },
    { &test_ctype_isblank, "/ctype/isblank" },
    { &test_ctype_iscntrl, "/ctype/iscntrl" },
    { &test_ctype_isdigit, "/ctype/isdigit" },
    { &test_ctype_isgraph, "/ctype/isgraph" },
    { &test_ctype_islower, "/ctype/islower" },
    { &test_ctype_isprint, "/ctype/isprint" },
    { &test_ctype_ispunct, "/ctype/ispunct" },
    { &test_ctype_isspace, "/ctype/isspace" },
    { &test_ctype_isupper, "/ctype/isupper" },
    { &test_ctype_isxdigit, "/ctype/isxdigit" },
    { &test_ctype_tolower, "/ctype/tolower" },
    { &test_ctype_toupper, "/ctype/toupper" },
    { &test_ctype_outside_ascii, "/ctype/outside_ascii" },
    { &test_ctype_conversion, "/ctype/conversion" },
    { &test_ctype_sets, "/ctype/sets" },
    { &test_ctype_ascii_names, "/ctype/ascii_names" },
    { &test_ctype_utf8, "/ctype/utf8" },
    { 0 },
};
