/*
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
*/

#include "../include/pf_assert.h"
#include "../include/pf_ctype.h"
#include "../include/pf_test.h"
#include <ctype.h>

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

pf_test suite_ctype[] = {
    { &test_ctype_isalnum, "/ctype/isalnum", 1 },
    { &test_ctype_isalpha, "/ctype/isalpha", 1 },
    { &test_ctype_isblank, "/ctype/isblank", 1 },
    { &test_ctype_iscntrl, "/ctype/iscntrl", 1 },
    { &test_ctype_isdigit, "/ctype/isdigit", 1 },
    { &test_ctype_isgraph, "/ctype/isgraph", 1 },
    { &test_ctype_islower, "/ctype/islower", 1 },
    { &test_ctype_isprint, "/ctype/isprint", 1 },
    { &test_ctype_ispunct, "/ctype/ispunct", 1 },
    { &test_ctype_isspace, "/ctype/isspace", 1 },
    { &test_ctype_isupper, "/ctype/isupper", 1 },
    { &test_ctype_isxdigit, "/ctype/isxdigit", 1 },
    { &test_ctype_tolower, "/ctype/tolower", 1 },
    { &test_ctype_toupper, "/ctype/toupper", 1 },
};
