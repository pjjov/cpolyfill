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

#include "check.h"
#include "../include/pf_pipe.h"
#include "util.h"
#include <errno.h>

/* A pipe name: a path for FIFOs, a plain name for Windows named pipes. */
static char *pipe_name(char *buf, size_t size) {
#ifdef _WIN32
    snprintf(buf, size, "pf-test-pipe-%ld-%d", test_pid(), (int)(size & 0xF));
    return buf;
#else
    return test_path(buf, size, "pipe");
#endif
}

/* Accepts the code as the enum has it (negative) or as errno (positive). */
#define IS_ERR(result, code) ((result) == (code) || (result) == -(code))

/* A pipe that can be read and written by a single process. */
static int pipe_make(pf_pipe_t *p, char *name, size_t size) {
    pipe_name(name, size);
    return pf_pipe_create(p, name, PF_PIPE_RDWR);
}

static void pipe_free(pf_pipe_t *p, const char *name) {
    pf_pipe_close(p);
    pf_pipe_destroy(name);
}

static int test_pipe_create_destroy(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX];

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
#ifndef _WIN32
    {
        struct stat st;
        CHECK(stat(name, &st) == 0);
        CHECK(S_ISFIFO(st.st_mode));
    }
#endif
    pf_pipe_close(&p);
    pf_pipe_destroy(name);
#ifndef _WIN32
    CHECK(access(name, F_OK) != 0);
#endif
    return 0;
}

#ifndef _WIN32
static int test_pipe_create_exists(int seed, int rep) {
    pf_pipe_t p, second;
    char name[TEST_PATH_MAX];

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
    CHECK(pf_pipe_create(&second, name, PF_PIPE_RDWR) != PF_PIPE_OK);
    pipe_free(&p, name);
    return 0;
}
#endif

static int test_pipe_invalid_arguments(int seed, int rep) {
    pf_pipe_t p;
    char buf[4], name[TEST_PATH_MAX];
    size_t n = 0;

    pipe_name(name, sizeof name);

    CHECK(IS_ERR(pf_pipe_create(NULL, name, PF_PIPE_RDWR), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_create(&p, NULL, PF_PIPE_RDWR), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_open(NULL, name, PF_PIPE_READ), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_open(&p, NULL, PF_PIPE_READ), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_connect(NULL), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_disconnect(NULL), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_flush(NULL), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_set_nonblocking(NULL, 1), PF_PIPE_EINVAL));
    CHECK(IS_ERR(pf_pipe_read_timeout(NULL, buf, 4, &n, 0), PF_PIPE_EINVAL));
    CHECK(pf_pipe_read(NULL, buf, 4) == 0);
    CHECK(pf_pipe_write(NULL, buf, 4) == 0);
    CHECK(pf_pipe_peek(NULL) == 0);
    pf_pipe_close(NULL);
    pf_pipe_destroy(NULL);
    return 0;
}

static int test_pipe_open_missing(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX];

    pipe_name(name, sizeof name);
    CHECK(pf_pipe_open(&p, name, PF_PIPE_READ) != PF_PIPE_OK);
    return 0;
}

static int test_pipe_write_read(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX], buf[16] = { 0 };

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);

    CHECK(pf_pipe_peek(&p) == 0);
    CHECK(pf_pipe_write(&p, "hello", 5) == 5);
    CHECK(p.error == PF_PIPE_OK);
    CHECK(pf_pipe_peek(&p) == 5);
    CHECK(pf_pipe_flush(&p) == PF_PIPE_OK);

    CHECK(pf_pipe_read(&p, buf, sizeof buf) == 5);
    CHECK(p.error == PF_PIPE_OK);
    CHECK_STR(buf, "hello");
    CHECK(pf_pipe_peek(&p) == 0);

    pipe_free(&p, name);
    return 0;
}

static int test_pipe_partial_reads(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX], buf[16] = { 0 };

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);

    CHECK(pf_pipe_write(&p, "0123456789", 10) == 10);
    CHECK(pf_pipe_read(&p, buf, 4) == 4);
    CHECK_MEM(buf, "0123", 4);
    CHECK(pf_pipe_peek(&p) == 6);
    CHECK(pf_pipe_read(&p, buf, 6) == 6);
    CHECK_MEM(buf, "456789", 6);

    pipe_free(&p, name);
    return 0;
}

static int test_pipe_message_order(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX], buf[64] = { 0 };
    int i;

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
    for (i = 0; i < 10; i++) {
        char msg[8];
        int n = snprintf(msg, sizeof msg, "<%d>", i);
        CHECK(pf_pipe_write(&p, msg, (size_t)n) == (size_t)n);
    }
    CHECK(pf_pipe_read(&p, buf, sizeof buf) == 30);
    CHECK_STR(buf, "<0><1><2><3><4><5><6><7><8><9>");

    pipe_free(&p, name);
    return 0;
}

/* A second handle, as another process would open it. */
static int test_pipe_open_client(int seed, int rep) {
    pf_pipe_t server, client;
    char name[TEST_PATH_MAX], buf[16] = { 0 };

    CHECK(pipe_make(&server, name, sizeof name) == PF_PIPE_OK);
    CHECK(pf_pipe_open(&client, name, PF_PIPE_WRITE) == PF_PIPE_OK);

    CHECK(pf_pipe_write(&client, "from client", 11) == 11);
    CHECK(pf_pipe_read(&server, buf, sizeof buf) == 11);
    CHECK_STR(buf, "from client");

    pf_pipe_close(&client);
    pipe_free(&server, name);
    return 0;
}

static int test_pipe_read_timeout(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX], buf[16] = { 0 };
    size_t n = 99;
    double start, elapsed;

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);

    /* Nothing to read: the call waits for about the timeout. */
    start = test_now();
    CHECK(IS_ERR(
        pf_pipe_read_timeout(&p, buf, sizeof buf, &n, 60), PF_PIPE_ETIMEDOUT
    ));
    elapsed = test_now() - start;
    CHECK(n == 0);
    CHECK(elapsed >= 0.04);
    CHECK(elapsed < 3.0);

    /* With data it returns immediately. */
    CHECK(pf_pipe_write(&p, "abc", 3) == 3);
    start = test_now();
    CHECK(pf_pipe_read_timeout(&p, buf, sizeof buf, &n, 5000) == PF_PIPE_OK);
    CHECK(test_now() - start < 2.0);
    CHECK(n == 3);
    CHECK_MEM(buf, "abc", 3);

    /* A zero timeout polls. */
    CHECK(IS_ERR(
        pf_pipe_read_timeout(&p, buf, sizeof buf, &n, 0), PF_PIPE_ETIMEDOUT
    ));

    pipe_free(&p, name);
    return 0;
}

static int test_pipe_nonblocking(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX], buf[8];

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
    CHECK(pf_pipe_set_nonblocking(&p, 1) == PF_PIPE_OK);

    /* An empty pipe does not block. */
    CHECK(pf_pipe_read(&p, buf, sizeof buf) == 0);
#ifndef _WIN32
    CHECK(IS_ERR(p.error, PF_PIPE_EAGAIN));
#endif

    CHECK(pf_pipe_write(&p, "xy", 2) == 2);
    CHECK(pf_pipe_read(&p, buf, sizeof buf) == 2);

    CHECK(pf_pipe_set_nonblocking(&p, 0) == PF_PIPE_OK);
    CHECK(pf_pipe_write(&p, "z", 1) == 1);
    CHECK(pf_pipe_read(&p, buf, sizeof buf) == 1);

    pipe_free(&p, name);
    return 0;
}

/* The pipe is a byte stream, so large payloads arrive complete. */
static int test_pipe_large_transfer(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX];
    unsigned char out[512], in[512];
    size_t i, total = 0;

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
    for (i = 0; i < sizeof out; i++)
        out[i] = (unsigned char)(i * 7 + seed);

    /* Several rounds, each well within the pipe's capacity. */
    for (i = 0; i < 32; i++) {
        size_t got = 0;
        CHECK(pf_pipe_write(&p, out, sizeof out) == sizeof out);
        while (got < sizeof in) {
            size_t n = pf_pipe_read(&p, in + got, sizeof in - got);
            CHECK(n > 0);
            got += n;
        }
        CHECK_MEM(in, out, sizeof out);
        total += got;
    }
    CHECK(total == 32 * sizeof out);

    pipe_free(&p, name);
    return 0;
}

#ifndef _WIN32
static int test_pipe_connect_noop(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX];

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
    /* FIFOs need no connection step. */
    CHECK(pf_pipe_connect(&p) == PF_PIPE_OK);
    CHECK(pf_pipe_disconnect(&p) == PF_PIPE_OK);
    CHECK(pf_pipe_write(&p, "k", 1) == 1);
    pipe_free(&p, name);
    return 0;
}

static int test_pipe_close_invalidates(int seed, int rep) {
    pf_pipe_t p;
    char name[TEST_PATH_MAX], buf[4];

    CHECK(pipe_make(&p, name, sizeof name) == PF_PIPE_OK);
    pf_pipe_close(&p);
    CHECK(p.fd == -1);

    /* I/O on a closed pipe fails and reports it. */
    CHECK(pf_pipe_write(&p, "x", 1) == 0);
    CHECK(p.error != PF_PIPE_OK);
    CHECK(pf_pipe_read(&p, buf, sizeof buf) == 0);
    CHECK(p.error != PF_PIPE_OK);
    pf_pipe_destroy(name);
    return 0;
}
#endif

pf_test_t suite_pipe[] = {
    { test_pipe_create_destroy, "/pipe/create_destroy" },
#ifndef _WIN32
    { test_pipe_create_exists, "/pipe/create_exists" },
#endif
    { test_pipe_invalid_arguments, "/pipe/invalid_arguments" },
    { test_pipe_open_missing, "/pipe/open_missing" },
    { test_pipe_write_read, "/pipe/write_read" },
    { test_pipe_partial_reads, "/pipe/partial_reads" },
    { test_pipe_message_order, "/pipe/message_order" },
    { test_pipe_open_client, "/pipe/open_client" },
    { test_pipe_read_timeout, "/pipe/read_timeout" },
    { test_pipe_nonblocking, "/pipe/nonblocking" },
    { test_pipe_large_transfer, "/pipe/large_transfer" },
#ifndef _WIN32
    { test_pipe_connect_noop, "/pipe/connect_noop" },
    { test_pipe_close_invalidates, "/pipe/close_invalidates" },
#endif
    { 0 },
};
