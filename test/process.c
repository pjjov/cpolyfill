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

#include "../include/pf_process.h"
#include "check.h"
#include "util.h"

#ifndef _WIN32
    #include <sys/wait.h>
#endif

static int test_process_getpid(int seed, int rep) {
    int pid = getpid();

    CHECK(pid > 0);
    CHECK(pid == getpid());
    CHECK((long)pid == test_pid());
    return 0;
}

static int test_process_sleep(int seed, int rep) {
    double start = test_now();

    CHECK(sleep(0) == 0);
    CHECK(test_now() - start < 0.5);
    return 0;
}

static int test_process_usleep(int seed, int rep) {
    double start, elapsed;

    CHECK(usleep(0) == 0);

    start = test_now();
    CHECK(usleep(20000) == 0);
    elapsed = test_now() - start;
    CHECK(elapsed >= 0.015); /* some clocks are coarse, but never this much */
    CHECK(elapsed < 2.0);

    /* Short delays are busy-waited on Windows. */
    start = test_now();
    CHECK(usleep(200) == 0);
    elapsed = test_now() - start;
    CHECK(elapsed >= 0.0001);
    CHECK(elapsed < 1.0);
    return 0;
}

static int test_process_popen(int seed, int rep) {
    char line[64] = { 0 };
    FILE *p = popen("echo pf-popen-test", "r");
    int status;

    CHECK(p != NULL);
    CHECK(fgets(line, sizeof line, p) != NULL);
    status = pclose(p);
    CHECK(status == 0);
    CHECK(strncmp(line, "pf-popen-test", 13) == 0);
    return 0;
}

static int test_process_popen_write(int seed, int rep) {
    char path[TEST_PATH_MAX], cmd[TEST_PATH_MAX + 32], out[32];
    FILE *p;

    test_path(path, sizeof path, "popen-write");
    snprintf(cmd, sizeof cmd, "more > \"%s\"", path);
#ifndef _WIN32
    snprintf(cmd, sizeof cmd, "cat > '%s'", path);
#endif

    p = popen(cmd, "w");
    CHECK(p != NULL);
    fputs("written", p);
    CHECK(pclose(p) == 0);

    CHECK(test_read(path, out, sizeof out) >= 7);
    CHECK(strncmp(out, "written", 7) == 0);
    test_unlink(path);
    return 0;
}

static int test_process_pclose_status(int seed, int rep) {
    FILE *p = popen("exit 3", "r");
    int status;

    CHECK(p != NULL);
    status = pclose(p);
#ifdef _WIN32
    CHECK(status == 3);
#else
    CHECK(status != 0); /* encoded wait status */
    #ifdef WEXITSTATUS
    CHECK(WEXITSTATUS(status) == 3);
    #endif
#endif
    return 0;
}

pf_test_t suite_process[] = {
    { test_process_getpid, "/process/getpid" },
    { test_process_sleep, "/process/sleep" },
    { test_process_usleep, "/process/usleep" },
    { test_process_popen, "/process/popen" },
    { test_process_popen_write, "/process/popen_write" },
    { test_process_pclose_status, "/process/pclose_status" },
    { 0 },
};
