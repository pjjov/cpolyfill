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

#include "../include/pf_test.h"

/* clang-format off */

extern pf_test_t suite_bitwise[];
extern pf_test_t suite_ctype[];
extern pf_test_t suite_endian[];
extern pf_test_t suite_macro[];
extern pf_test_t suite_overflow[];

/* clang-format on */

int main(int argc, char *argv[]) {
    static const pf_suite_t suites[] = {
        { "bitwise", 1, suite_bitwise },   { "ctype", 1, suite_ctype },
        { "endian", 1, suite_endian },     { "macro", 1, suite_macro },
        { "overflow", 1, suite_overflow }, { 0 },
    };

    return pf_test_main(argc, argv, suites);
}
