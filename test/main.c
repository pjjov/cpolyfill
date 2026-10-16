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

extern pf_test_t suite_argparse[];
extern pf_test_t suite_array[];
extern pf_test_t suite_assert[];
extern pf_test_t suite_atomic[];
extern pf_test_t suite_attribute[];
extern pf_test_t suite_bitwise[];
extern pf_test_t suite_cli[];
extern pf_test_t suite_cpuinfo[];
extern pf_test_t suite_ctype[];
extern pf_test_t suite_daemon[];
extern pf_test_t suite_dirent[];
extern pf_test_t suite_dlsym[];
extern pf_test_t suite_endian[];
extern pf_test_t suite_errno[];
extern pf_test_t suite_exception[];
extern pf_test_t suite_filesystem[];
extern pf_test_t suite_io[];
extern pf_test_t suite_macro[];
extern pf_test_t suite_overflow[];
extern pf_test_t suite_pipe[];
extern pf_test_t suite_process[];
extern pf_test_t suite_semaphore[];
extern pf_test_t suite_socket[];
extern pf_test_t suite_string[];
extern pf_test_t suite_test[];
extern pf_test_t suite_threads[];
extern pf_test_t suite_typeid[];
extern pf_test_t suite_types[];

/* clang-format on */

/* How often each test of a suite is repeated. Tests that draw on the seed
   run more than once, the others once. */
int main(int argc, char *argv[]) {
    static const pf_suite_t suites[] = {
        { "argparse", 1, suite_argparse },
        { "array", 5, suite_array },
        { "assert", 1, suite_assert },
        { "atomic", 1, suite_atomic },
        { "attribute", 1, suite_attribute },
        { "bitwise", 1, suite_bitwise },
        { "cli", 1, suite_cli },
        { "cpuinfo", 1, suite_cpuinfo },
        { "ctype", 1, suite_ctype },
        { "daemon", 1, suite_daemon },
        { "dirent", 1, suite_dirent },
        { "dlsym", 1, suite_dlsym },
        { "endian", 3, suite_endian },
        { "errno", 1, suite_errno },
        { "exception", 1, suite_exception },
        { "filesystem", 1, suite_filesystem },
        { "io", 1, suite_io },
        { "macro", 1, suite_macro },
        { "overflow", 1, suite_overflow },
        { "pipe", 1, suite_pipe },
        { "process", 1, suite_process },
        { "semaphore", 1, suite_semaphore },
        { "socket", 1, suite_socket },
        { "string", 1, suite_string },
        { "test", 1, suite_test },
        { "threads", 1, suite_threads },
        { "typeid", 1, suite_typeid },
        { "types", 3, suite_types },
        { 0 },
    };

    return pf_test_main(argc, argv, suites);
}
