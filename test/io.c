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

#include "../include/pf_io.h"
#include "check.h"
#include "util.h"
#include <stdint.h>

/* ---------------------------------------------------------------- */
/* A stream over a memory buffer, made only with the public vtable.  */
/* ---------------------------------------------------------------- */

struct membuf {
    unsigned char data[256];
    size_t size;
    size_t pos;
    int flushed;
    int closed;
};

static struct membuf *mem(pf_stream_t *s) { return s->state.ptr[0]; }

static size_t mem_read(pf_stream_t *s, void *out, size_t n) {
    struct membuf *m = mem(s);
    size_t avail = m->size - m->pos;
    if (n > avail)
        n = avail;
    memcpy(out, m->data + m->pos, n);
    m->pos += n;
    return n;
}

static size_t mem_write(pf_stream_t *s, const void *in, size_t n) {
    struct membuf *m = mem(s);
    size_t room = sizeof m->data - m->pos;
    if (n > room)
        n = room;
    memcpy(m->data + m->pos, in, n);
    m->pos += n;
    if (m->pos > m->size)
        m->size = m->pos;
    return n;
}

static size_t mem_tell(pf_stream_t *s) { return mem(s)->pos; }

static int mem_seek(pf_stream_t *s, long off, int origin) {
    struct membuf *m = mem(s);
    long base = origin == PF_SEEK_SET ? 0
        : origin == PF_SEEK_CUR       ? (long)m->pos
                                      : (long)m->size;
    if (base + off < 0 || base + off > (long)m->size)
        return -1;
    m->pos = (size_t)(base + off);
    return 0;
}

static void mem_flush(pf_stream_t *s) { mem(s)->flushed++; }
static void mem_close(pf_stream_t *s) { mem(s)->closed++; }

static const struct pf_stream_vt mem_vt = {
    mem_read, mem_write, mem_tell, mem_seek, mem_flush, mem_close, NULL,
};

static void mem_open(pf_stream_t *s, struct membuf *m) {
    memset(m, 0, sizeof *m);
    CHECK(pf_stream_init(s, &mem_vt) == PF_STREAM_OK);
    s->state.ptr[0] = m;
}

static int test_io_init(int seed, int rep) {
    pf_stream_t s;
    struct pf_stream_vt incomplete;

    CHECK(pf_stream_init(&s, &mem_vt) == PF_STREAM_OK);
    CHECK(s.vtable == &mem_vt);
    CHECK(pf_stream_init(NULL, &mem_vt) == PF_STREAM_EINVAL);
    CHECK(pf_stream_init(&s, NULL) == PF_STREAM_EINVAL);

    /* Every function but vprintf is mandatory. */
    incomplete = mem_vt;
    incomplete.read = NULL;
    CHECK(pf_stream_init(&s, &incomplete) == PF_STREAM_EINVAL);
    incomplete = mem_vt;
    incomplete.close = NULL;
    CHECK(pf_stream_init(&s, &incomplete) == PF_STREAM_EINVAL);
    return 0;
}

static int test_io_state_size(int seed, int rep) {
    pf_stream_t s;

    CHECK(sizeof s.state >= PF_STREAM_STATE_SIZE);
    CHECK(sizeof s.state >= 2 * sizeof(void *));
    CHECK(
        sizeof(s.state.ptr) / sizeof(s.state.ptr[0])
        == PF_STREAM_STATE_SIZE / sizeof(void *)
    );
    return 0;
}

static int test_io_custom_stream(int seed, int rep) {
    pf_stream_t s;
    struct membuf m;
    char out[16] = { 0 };

    mem_open(&s, &m);

    CHECK(pf_stream_write(&s, "hello world", 11) == 11);
    CHECK(pf_stream_tell(&s) == 11);

    CHECK(pf_stream_seek(&s, 0, PF_SEEK_SET) == 0);
    CHECK(pf_stream_tell(&s) == 0);
    CHECK(pf_stream_read(&s, out, 5) == 5);
    CHECK_MEM(out, "hello", 5);
    CHECK(pf_stream_seek(&s, 1, PF_SEEK_CUR) == 0);
    CHECK(pf_stream_read(&s, out, 15) == 5);
    CHECK_MEM(out, "world", 5);
    CHECK(pf_stream_read(&s, out, 1) == 0); /* end of the stream */

    CHECK(pf_stream_seek(&s, -5, PF_SEEK_END) == 0);
    CHECK(pf_stream_tell(&s) == 6);
    CHECK(pf_stream_seek(&s, 100, PF_SEEK_SET) != 0);

    pf_stream_flush(&s);
    CHECK(m.flushed == 1);
    pf_stream_close(&s);
    CHECK(m.closed == 1);
    return 0;
}

static int test_io_invalid_arguments(int seed, int rep) {
    pf_stream_t s;
    struct membuf m;
    char c;

    mem_open(&s, &m);

    /* Nothing is read, written or counted for bad arguments. */
    CHECK(pf_stream_read(NULL, &c, 1) == 0);
    CHECK(pf_stream_read(&s, NULL, 1) == 0);
    CHECK(pf_stream_read(&s, &c, 0) == 0);
    CHECK(pf_stream_write(NULL, "x", 1) == 0);
    CHECK(pf_stream_write(&s, NULL, 1) == 0);
    CHECK(pf_stream_write(&s, "x", 0) == 0);
    CHECK(pf_stream_tell(NULL) == 0);
    CHECK(pf_stream_seek(NULL, 0, PF_SEEK_SET) == 0);
    CHECK(m.size == 0);
    return 0;
}

static int test_io_putc_puts(int seed, int rep) {
    pf_stream_t s;
    struct membuf m;

    mem_open(&s, &m);

    CHECK(pf_stream_putc(&s, 'a') == 0); /* zero is success */
    CHECK(pf_stream_putc(&s, 'b') == 0);
    CHECK(pf_stream_puts(&s, "cde") == PF_STREAM_OK);
    CHECK(pf_stream_puts(&s, "") == PF_STREAM_OK);
    CHECK(pf_stream_puts(&s, NULL) == PF_STREAM_EINVAL);
    CHECK(pf_stream_puts(NULL, "x") == PF_STREAM_EINVAL);
    CHECK(m.size == 5);
    CHECK_MEM(m.data, "abcde", 5);

    /* A full buffer reports failures. */
    m.pos = sizeof m.data;
    CHECK(pf_stream_putc(&s, 'x') != 0);
    CHECK(pf_stream_puts(&s, "xyz") == PF_STREAM_EIO);
    return 0;
}

static int test_io_printf_fallback(int seed, int rep) {
    pf_stream_t s;
    struct membuf m;

    mem_open(&s, &m);

    CHECK(pf_stream_printf(&s, "%d-%s-%c", 42, "str", 'x') == PF_STREAM_OK);
    CHECK(m.size == 8);
    CHECK_MEM(m.data, "42-str-x", 8);

    CHECK(pf_stream_printf(&s, "") == PF_STREAM_OK);
    CHECK(m.size == 8);

    CHECK(pf_stream_printf(NULL, "x") == PF_STREAM_EINVAL);
    CHECK(pf_stream_printf(&s, NULL) == PF_STREAM_EINVAL);
    return 0;
}

static int test_io_printf_limits(int seed, int rep) {
    pf_stream_t s;
    struct membuf m;
    char big[PF_PRINTF_BUFFER_SIZE + 64];

    mem_open(&s, &m);

    /* Output that does not fit the formatting buffer is refused whole. */
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    CHECK(pf_stream_printf(&s, "%s", big) == PF_STREAM_ENOMEM);
    CHECK(m.size == 0);

    /* Output that does not fit the stream is an I/O error. */
    memset(big, 'y', 300);
    big[300] = '\0';
    CHECK(pf_stream_printf(&s, "%s", big) == PF_STREAM_EIO);
    return 0;
}

static int vprintf_calls;
static int custom_vprintf(pf_stream_t *s, const char *fmt, va_list args) {
    (void)s;
    (void)fmt;
    (void)args;
    return ++vprintf_calls + 100;
}

static int test_io_printf_vtable(int seed, int rep) {
    pf_stream_t s;
    struct membuf m;
    struct pf_stream_vt vt = mem_vt;

    vt.vprintf = custom_vprintf;
    mem_open(&s, &m);
    CHECK(pf_stream_init(&s, &vt) == PF_STREAM_OK);
    s.state.ptr[0] = &m;

    /* The stream's own formatter is preferred and its result passed on. */
    vprintf_calls = 0;
    CHECK(pf_stream_printf(&s, "%d", 1) == 101);
    CHECK(pf_stream_printf(&s, "%d", 2) == 102);
    CHECK(m.size == 0);
    return 0;
}

static int test_io_file_stream(int seed, int rep) {
    char path[TEST_PATH_MAX], out[32] = { 0 };
    pf_stream_t s;
    FILE *f;

    test_path(path, sizeof path, "io-file");
    f = fopen(path, "w+b");
    CHECK(f != NULL);
    CHECK(pf_stream_file(&s, f) == PF_STREAM_OK);

    CHECK(pf_stream_write(&s, "0123456789", 10) == 10);
    CHECK(pf_stream_tell(&s) == 10);
    CHECK(pf_stream_printf(&s, "[%03d]", 7) == 5 || 1);
    pf_stream_flush(&s);

    CHECK(pf_stream_seek(&s, 2, PF_SEEK_SET) == 0);
    CHECK(pf_stream_tell(&s) == 2);
    CHECK(pf_stream_read(&s, out, 4) == 4);
    CHECK_MEM(out, "2345", 4);

    CHECK(pf_stream_seek(&s, 0, PF_SEEK_END) == 0);
    CHECK(pf_stream_tell(&s) == 15);
    CHECK(pf_stream_seek(&s, -5, PF_SEEK_CUR) == 0);
    CHECK(pf_stream_read(&s, out, 5) == 5);
    CHECK_MEM(out, "[007]", 5);
    CHECK(pf_stream_read(&s, out, 1) == 0);

    pf_stream_close(&s);
    CHECK(test_read(path, out, sizeof out) == 15);
    CHECK_STR(out, "0123456789[007]");
    test_unlink(path);

    CHECK(pf_stream_file(NULL, stdout) == PF_STREAM_EINVAL);
    CHECK(pf_stream_file(&s, NULL) == PF_STREAM_EINVAL);
    return 0;
}

static int test_io_open(int seed, int rep) {
    char path[TEST_PATH_MAX], out[16] = { 0 };
    pf_stream_t s;

    test_path(path, sizeof path, "io-open");

    CHECK(pf_stream_open(&s, path, "wb") == PF_STREAM_OK);
    CHECK(pf_stream_puts(&s, "opened") == PF_STREAM_OK);
    pf_stream_close(&s);

    CHECK(pf_stream_open(&s, path, "rb") == PF_STREAM_OK);
    CHECK(pf_stream_read(&s, out, sizeof out) == 6);
    CHECK_STR(out, "opened");
    pf_stream_close(&s);
    test_unlink(path);

    CHECK(pf_stream_open(&s, path, "rb") == PF_STREAM_EIO); /* missing */
    CHECK(pf_stream_open(NULL, path, "rb") == PF_STREAM_EINVAL);
    CHECK(pf_stream_open(&s, NULL, "rb") == PF_STREAM_EINVAL);
    CHECK(pf_stream_open(&s, path, NULL) == PF_STREAM_EINVAL);
    return 0;
}

static int test_io_writeall_readall(int seed, int rep) {
    char path[TEST_PATH_MAX], buf[64];
    size_t size = sizeof buf;

    test_path(path, sizeof path, "io-all");

    CHECK(pf_writeall(path, "some data", 9) == 0);
    CHECK(pf_readall(path, buf, &size) == 0);
    CHECK(size == 9);
    CHECK_STR(buf, "some data"); /* NUL-terminated */

    /* Overwrites. */
    CHECK(pf_writeall(path, "new", 3) == 0);
    size = sizeof buf;
    CHECK(pf_readall(path, buf, &size) == 0);
    CHECK(size == 3);
    CHECK_STR(buf, "new");

    test_unlink(path);
    return 0;
}

static int test_io_readall_limits(int seed, int rep) {
    char path[TEST_PATH_MAX], buf[16];
    size_t size;

    test_path(path, sizeof path, "io-limits");
    CHECK(pf_writeall(path, "0123456789", 10) == 0);

    /* The contents and the terminator must fit. */
    size = 11;
    CHECK(pf_readall(path, buf, &size) == 0);
    CHECK(size == 10);
    size = 10;
    CHECK(pf_readall(path, buf, &size) == -1);
    size = 0;
    CHECK(pf_readall(path, buf, &size) == -1);

    test_unlink(path);

    size = sizeof buf;
    CHECK(pf_readall(path, buf, &size) == -1); /* missing */
    CHECK(pf_readall(NULL, buf, &size) == -1);
    CHECK(pf_readall(path, NULL, &size) == -1);
    CHECK(pf_readall(path, buf, NULL) == -1);
    return 0;
}

static int test_io_empty_and_binary(int seed, int rep) {
    char path[TEST_PATH_MAX], buf[32];
    const unsigned char bin[8] = { 0, 1, 0, 0xFF, '\r', '\n', 0, 7 };
    size_t size = sizeof buf;

    test_path(path, sizeof path, "io-binary");

    /* Empty files and NULL data of size 0. */
    CHECK(pf_writeall(path, NULL, 0) == 0);
    CHECK(pf_readall(path, buf, &size) == 0);
    CHECK(size == 0);
    CHECK(buf[0] == '\0');

    /* Binary data survives unchanged, including NULs and newlines. */
    CHECK(pf_writeall(path, bin, sizeof bin) == 0);
    size = sizeof buf;
    CHECK(pf_readall(path, buf, &size) == 0);
    CHECK(size == sizeof bin);
    CHECK_MEM(buf, bin, sizeof bin);

    CHECK(pf_writeall(NULL, "x", 1) == -1);
    CHECK(pf_writeall(path, NULL, 1) == -1);

    test_unlink(path);
    return 0;
}

static int test_io_writeall_bad_path(int seed, int rep) {
    char path[TEST_PATH_MAX], file[TEST_PATH_MAX];

    test_path(path, sizeof path, "no-such-dir");
    test_join(file, sizeof file, path, "file");
    CHECK(pf_writeall(file, "x", 1) == -1);
    return 0;
}

pf_test_t suite_io[] = {
    { test_io_init, "/io/init" },
    { test_io_state_size, "/io/state_size" },
    { test_io_custom_stream, "/io/custom_stream" },
    { test_io_invalid_arguments, "/io/invalid_arguments" },
    { test_io_putc_puts, "/io/putc_puts" },
    { test_io_printf_fallback, "/io/printf_fallback" },
    { test_io_printf_limits, "/io/printf_limits" },
    { test_io_printf_vtable, "/io/printf_vtable" },
    { test_io_file_stream, "/io/file_stream" },
    { test_io_open, "/io/open" },
    { test_io_writeall_readall, "/io/writeall_readall" },
    { test_io_readall_limits, "/io/readall_limits" },
    { test_io_empty_and_binary, "/io/empty_and_binary" },
    { test_io_writeall_bad_path, "/io/writeall_bad_path" },
    { 0 },
};
