/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a custom interface for byte/character streams. The file
    also provides a wrapper around the standard library input/output functions.

    Last-updated: August 2026
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
**/

#ifndef POLYFILL_IO
#define POLYFILL_IO

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PF_STREAM_STATE_SIZE 56
#define PF_PRINTF_BUFFER_SIZE 4096

enum pf_stream_error {
    PF_STREAM_OK = 0,
    PF_STREAM_EIO = -5,
    PF_STREAM_ENOMEM = -12,
    PF_STREAM_EINVAL = -22,
};

typedef struct pf_stream_t pf_stream_t;

struct pf_stream_vt {
    size_t (*read)(pf_stream_t *stream, void *buffer, size_t size);
    size_t (*write)(pf_stream_t *stream, const void *buffer, size_t size);
    size_t (*tell)(pf_stream_t *stream);
    int (*seek)(pf_stream_t *stream, long offset, int origin);
    void (*flush)(pf_stream_t *stream);
    void (*close)(pf_stream_t *stream);
    int (*vprintf)(pf_stream_t *stream, const char *format, va_list vlist);
};

struct pf_stream_t {
    const struct pf_stream_vt *vtable;
    union {
        char _size[PF_STREAM_STATE_SIZE];
        void *_align;
        void *ptr[PF_STREAM_STATE_SIZE / sizeof(void *)];
    } state;
};

PF_API int pf_stream_init(pf_stream_t *out, const struct pf_stream_vt *vtable) {
    if (!out || !vtable)
        return PF_STREAM_EINVAL;

    int fail = 0;
    fail |= !vtable->read;
    fail |= !vtable->write;
    fail |= !vtable->tell;
    fail |= !vtable->seek;
    fail |= !vtable->flush;
    fail |= !vtable->close;

    out->vtable = vtable;
    return fail ? PF_STREAM_EINVAL : PF_STREAM_OK;
}

#ifndef NDEBUG
    #define PF_STREAM_ASSERT(VAR, FUNC, RET)        \
        if (!(VAR)->vtable || !(VAR)->vtable->FUNC) \
            return (RET);
#else
    #define PF_STREAM_ASSERT(...)
#endif

PF_API int pf_stream_seek(pf_stream_t *stream, long offset, int origin) {
    if (!stream)
        return 0;
    PF_STREAM_ASSERT(stream, seek, 0);
    return stream->vtable->seek(stream, offset, origin);
}

PF_API size_t pf_stream_tell(pf_stream_t *stream) {
    if (!stream)
        return 0;
    PF_STREAM_ASSERT(stream, tell, 0);
    return stream->vtable->tell(stream);
}

PF_API size_t pf_stream_read(pf_stream_t *stream, void *buffer, size_t size) {
    if (!stream || !buffer || size == 0)
        return 0;
    PF_STREAM_ASSERT(stream, read, 0);
    return stream->vtable->read(stream, buffer, size);
}

PF_API size_t
pf_stream_write(pf_stream_t *stream, const void *buffer, size_t size) {
    if (!stream || !buffer || size == 0)
        return 0;
    PF_STREAM_ASSERT(stream, write, 0);
    return stream->vtable->write(stream, buffer, size);
}

PF_API void pf_stream_flush(pf_stream_t *stream) {
    PF_STREAM_ASSERT(stream, flush, (void)0);
    stream->vtable->flush(stream);
}

PF_API void pf_stream_close(pf_stream_t *stream) {
    PF_STREAM_ASSERT(stream, close, (void)0);
    return stream->vtable->close(stream);
}

PF_API int pf_stream_putc(pf_stream_t *stream, char chr) {
    return 1 != pf_stream_write(stream, &chr, 1);
}

PF_API int pf_stream_puts(pf_stream_t *stream, const char *str) {
    if (!stream || !str)
        return PF_STREAM_EINVAL;

    size_t length = strlen(str);
    if (length == 0)
        return PF_STREAM_OK;

    size_t written = pf_stream_write(stream, str, length);
    return length != written ? PF_STREAM_EIO : PF_STREAM_OK;
}

PF_API int pf_stream_vprintf(
    pf_stream_t *stream, const char *fmt, va_list args
) {
    if (!stream || !fmt)
        return PF_STREAM_EINVAL;

    if (stream->vtable && stream->vtable->vprintf)
        return stream->vtable->vprintf(stream, fmt, args);

    char buffer[PF_PRINTF_BUFFER_SIZE];

    int res = vsnprintf(buffer, PF_PRINTF_BUFFER_SIZE, fmt, args);

    if (res >= PF_PRINTF_BUFFER_SIZE)
        return PF_STREAM_ENOMEM;

    if (res < 0 || res != pf_stream_write(stream, buffer, res))
        return PF_STREAM_EIO;

    return PF_STREAM_OK;
}

PF_API int pf_stream_printf(pf_stream_t *stream, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = pf_stream_vprintf(stream, fmt, args);
    va_end(args);
    return result;
}

static size_t pf__file_read(pf_stream_t *stream, void *buffer, size_t size) {
    FILE *file = stream->state.ptr[0];
    return fread(buffer, 1, size, file);
}

static size_t pf__file_write(
    pf_stream_t *stream, const void *buffer, size_t size
) {
    FILE *file = stream->state.ptr[0];
    return fwrite(buffer, 1, size, file);
}

static int pf__file_seek(pf_stream_t *stream, long offset, int origin) {
    FILE *file = stream->state.ptr[0];
    return fseek(file, offset, origin);
}

static size_t pf__file_tell(pf_stream_t *stream) {
    FILE *file = stream->state.ptr[0];
    return ftell(file);
}

static void pf__file_flush(pf_stream_t *stream) {
    FILE *file = stream->state.ptr[0];
    fflush(file);
}

static void pf__file_close(pf_stream_t *stream) {
    FILE *file = stream->state.ptr[0];
    fclose(file);
}

static int pf__file_vprintf(
    pf_stream_t *stream, const char *format, va_list vlist
) {
    FILE *file = stream->state.ptr[0];
    return vfprintf(file, format, vlist);
}

PF_API int pf_stream_file(pf_stream_t *out, FILE *file) {
    if (!out || !file)
        return PF_STREAM_EINVAL;

    static const struct pf_stream_vt vtable = {
        .read = pf__file_read,
        .write = pf__file_write,
        .tell = pf__file_tell,
        .seek = pf__file_seek,
        .flush = pf__file_flush,
        .close = pf__file_close,
        .vprintf = pf__file_vprintf,
    };

    out->vtable = &vtable;
    out->state.ptr[0] = file;
    return PF_STREAM_OK;
}

PF_API int pf_stream_open(
    pf_stream_t *out, const char *path, const char *mode
) {
    if (!out || !path || !mode)
        return PF_STREAM_EINVAL;

    FILE *file = fopen(path, mode);
    if (pf_stream_file(out, file))
        return PF_STREAM_EIO;

    return PF_STREAM_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_IO */
