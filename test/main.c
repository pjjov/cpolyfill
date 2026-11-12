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

#include "runner.h"

/* clang-format off */

extern pf_test_t suite_alloc[];
extern pf_test_t suite_assert[];
extern pf_test_t suite_atomic[];
extern pf_test_t suite_attribute[];
extern pf_test_t suite_bitwise[];
extern pf_test_t suite_cpuid[];
extern pf_test_t suite_ctype[];
extern pf_test_t suite_debug[];
extern pf_test_t suite_dirent[];
extern pf_test_t suite_dlfcn[];
extern pf_test_t suite_endian[];
extern pf_test_t suite_errno[];
extern pf_test_t suite_fs[];
extern pf_test_t suite_io[];
extern pf_test_t suite_macro[];
extern pf_test_t suite_mman[];
extern pf_test_t suite_overflow[];
extern pf_test_t suite_platform[];
extern pf_test_t suite_random[];
extern pf_test_t suite_semaphore[];
extern pf_test_t suite_std_names[];
extern pf_test_t suite_std_polyfill[];
extern pf_test_t suite_stdc[];
extern pf_test_t suite_stdlib[];
extern pf_test_t suite_string[];
extern pf_test_t suite_sysinfo[];
extern pf_test_t suite_term[];
extern pf_test_t suite_threads[];
extern pf_test_t suite_time[];
extern pf_test_t suite_types[];
extern pf_test_t suite_uchar[];
extern pf_test_t suite_unicode[];
extern pf_test_t suite_version[];

/* clang-format on */

/* How often each test of a suite is repeated. Tests that draw on the seed
   run more than once, the others once. */
int main(int argc, char *argv[]) {
    static const pf_suite_t suites[] = {
        { "alloc", 5, suite_alloc },
        { "assert", 1, suite_assert },
        { "atomic", 1, suite_atomic },
        { "attribute", 1, suite_attribute },
        { "bitwise", 3, suite_bitwise },
        { "cpuid", 1, suite_cpuid },
        { "ctype", 1, suite_ctype },
        { "debug", 1, suite_debug },
        { "dirent", 1, suite_dirent },
        { "dlfcn", 1, suite_dlfcn },
        { "endian", 3, suite_endian },
        { "errno", 1, suite_errno },
        { "fs", 1, suite_fs },
        { "io", 1, suite_io },
        { "macro", 1, suite_macro },
        { "mman", 1, suite_mman },
        { "overflow", 3, suite_overflow },
        { "platform", 1, suite_platform },
        { "random", 3, suite_random },
        { "semaphore", 1, suite_semaphore },
        { "std_names", 1, suite_std_names },
        { "std_polyfill", 3, suite_std_polyfill },
        { "stdc", 1, suite_stdc },
        { "stdlib", 3, suite_stdlib },
        { "string", 1, suite_string },
        { "sysinfo", 1, suite_sysinfo },
        { "term", 1, suite_term },
        { "threads", 1, suite_threads },
        { "time", 3, suite_time },
        { "types", 1, suite_types },
        { "uchar", 1, suite_uchar },
        { "unicode", 5, suite_unicode },
        { "version", 1, suite_version },
        { 0 },
    };

    return pf_test_main(argc, argv, suites);
}
