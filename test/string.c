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

#include "../include/pf_string.h"
#include "check.h"

static int test_string_strnlen(int seed, int rep) {
    char buf[8] = { 'a', 'b', 'c', 0, 'd', 'e', 'f', 'g' };

    CHECK(pf_strnlen("", 10) == 0);
    CHECK(pf_strnlen("hello", 10) == 5);
    CHECK(pf_strnlen("hello", 5) == 5);
    CHECK(pf_strnlen("hello", 3) == 3);
    CHECK(pf_strnlen("hello", 0) == 0);
    CHECK(pf_strnlen(NULL, 10) == 0);

    /* Never reads past the limit: no terminator in the last four bytes. */
    CHECK(pf_strnlen(buf, sizeof buf) == 3);
    CHECK(pf_strnlen(buf + 4, 4) == 4);
    return 0;
}

static int test_string_strnlen_alias(int seed, int rep) {
    /* Non-literals: compilers warn when the bound exceeds a literal. */
    char abc[16] = "abc", empty[16] = "";
    const char *s3 = abc, *s0 = empty;

    /* Whichever implementation the macros picked must agree. */
    CHECK(strnlen(s3, 10) == 3);
    CHECK(strnlen(s3, 2) == 2);
    CHECK(strnlen(s0, 4) == 0);
    CHECK(strnlen_s(s3, 10) == 3);
    CHECK(strnlen_s(s3, 2) == 2);
    return 0;
}

static int test_string_casecmp(int seed, int rep) {
    CHECK(strcasecmp("hello", "HELLO") == 0);
    CHECK(strcasecmp("", "") == 0);
    CHECK(strcasecmp("abc", "ABD") < 0);
    CHECK(strcasecmp("abd", "ABC") > 0);
    CHECK(strcasecmp("abc", "abcd") < 0);
    CHECK(strcasecmp("abcd", "ABC") > 0);

    CHECK(strncasecmp("HELLO world", "hello WORLD", 5) == 0);
    CHECK(strncasecmp("HELLO world", "hello WORLD", 11) == 0);
    CHECK(strncasecmp("abcX", "ABCY", 3) == 0);
    CHECK(strncasecmp("abcX", "ABCY", 4) < 0);
    CHECK(strncasecmp("abc", "xyz", 0) == 0);
    return 0;
}

static int test_string_strdup(int seed, int rep) {
    const char *src = "duplicate me";
    char *copy = strdup(src);

    CHECK(copy != NULL);
    CHECK(copy != src);
    CHECK_STR(copy, src);

    /* The copy owns its memory. */
    copy[0] = 'D';
    CHECK(src[0] == 'd');
    free(copy);

    copy = strdup("");
    CHECK(copy != NULL);
    CHECK(copy[0] == '\0');
    free(copy);
    return 0;
}

#ifdef pf_strdup
static int test_string_pf_strdup(int seed, int rep) {
    char *copy = pf_strdup("abc");

    CHECK(pf_strdup(NULL) == NULL);
    CHECK(copy != NULL);
    CHECK_STR(copy, "abc");
    free(copy);
    return 0;
}
#endif

pf_test_t suite_string[] = {
    { test_string_strnlen, "/string/strnlen" },
    { test_string_strnlen_alias, "/string/strnlen_alias" },
    { test_string_casecmp, "/string/casecmp" },
    { test_string_strdup, "/string/strdup" },
    { 0 },
};
