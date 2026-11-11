/** cpolyfill - Filling the gaps between C standards and compilers

    File descriptors, stdio files, whole files and byte streams, the same
    on every platform: paths are UTF-8 (WTF-8 on Windows, see
    <cpolyfill/fs.h>), offsets are 64-bit and files are binary on
    Windows.

        pf_off_t                File offsets and sizes: int64_t everywhere.
        pf_mode_t               Permission bits: mode_t, `unsigned` on
                                MSVC.

    File descriptors (POSIX conventions: -1 with errno on failure):

        int pf_open(const char *path, int flags, pf_mode_t mode)
                open(). `flags` are PF_O_RDONLY, PF_O_WRONLY, PF_O_RDWR,
                combined with PF_O_CREAT, PF_O_EXCL, PF_O_TRUNC,
                PF_O_APPEND and PF_O_CLOEXEC (not inherited by child
                processes); the system's own O_* flags work too. On Windows
                the file is always opened in binary mode, and `mode` only
                decides whether a new file is read-only (no 0200 bit).
        int pf_close(int fd)
        pf_ssize_t pf_read(int fd, void *buf, size_t n)
        pf_ssize_t pf_write(int fd, const void *buf, size_t n)
                read() and write(). On Windows a call moves at most
                INT_MAX bytes.
        pf_ssize_t pf_pread(int fd, void *buf, size_t n, pf_off_t off)
        pf_ssize_t pf_pwrite(int fd, const void *buf, size_t n, pf_off_t off)
                Read or write at offset `off`. On Windows they move the
                file position (POSIX leaves it alone).
        pf_off_t pf_lseek(int fd, pf_off_t off, int whence)
                lseek() with SEEK_SET, SEEK_CUR, SEEK_END.
        int pf_fsync(int fd)
                Writes the file's data and metadata to the device.
        int pf_ftruncate(int fd, pf_off_t size)
        int pf_dup(int fd)
        int pf_dup2(int fd, int fd2)
                dup2() returns `fd2`, also on Windows.

    stdio:

        FILE *pf_fopen(const char *path, const char *mode)
                fopen() with a UTF-8 path. `mode` is "r", "w" or "a",
                optionally with "+", then any of "b", "x" (C11: fail if
                the file exists) and "e" (close-on-exec, as in glibc and
                the BSDs), on every platform.
        int pf_fseeko(FILE *f, pf_off_t off, int whence)
        pf_off_t pf_ftello(FILE *f)
                fseek and ftell with 64-bit offsets.
        int pf_fileno(FILE *f)
        FILE *pf_fmemopen(void *buf, size_t size, const char *mode)
        FILE *pf_open_memstream(char **ptr, size_t *size)
                POSIX memory files, where the C library has them. On
                Windows pf_fmemopen works only in read modes ("r", "rb"),
                through a temporary file that is deleted when closed (the
                file holds a copy: changes to `buf` are not seen), and
                pf_open_memstream fails with ENOSYS. Elsewhere without
                them, both fail with ENOSYS.

    Whole files (0, or a negative PF_E* code):

        int pf_read_file(const char *path, void **data, size_t *size,
                         struct allocator_t *alloc)
                Reads a whole file into new memory from `alloc` (NULL: the
                library's default), followed by a NUL that `*size` does not
                count, so text can be used as a string. Works for files
                whose size is not known in advance (pipes, /proc). Release
                the memory with pf_read_file_free.
        void pf_read_file_free(void *data, size_t size,
                               struct allocator_t *alloc)
        int pf_write_file(const char *path, const void *data, size_t size,
                          unsigned flags)
                Creates or replaces a file with `data`. With
                PF_WRITE_ATOMIC the data goes to a temporary file in the
                same directory, which is flushed to the device and renamed
                over `path`: readers see the old content or the new one,
                never a part, even after a crash. The file keeps the
                permissions of the one it replaces.

    Streams: a byte stream is a table of functions and their state.

        struct pf_stream_vt {
            pf_ssize_t (*read)(void *ctx, void *buf, size_t n);
            pf_ssize_t (*write)(void *ctx, const void *buf, size_t n);
            int64_t (*seek)(void *ctx, int64_t off, int whence);
            int (*flush)(void *ctx);
            int (*close)(void *ctx);
        };
        pf_stream_t             { const struct pf_stream_vt *vt; void *ctx; }

    Every function returns a negative PF_E* code on failure. `read` returns
    the bytes read (fewer than `n` is fine, 0 only at the end), `write` the
    bytes written (at least 1 unless `n` is 0), `seek` (SEEK_SET, SEEK_CUR,
    SEEK_END) the new position. A NULL `read`, `write` or `seek` means the
    stream cannot do it (PF_EBADF, PF_EBADF, PF_ESPIPE); `flush` and
    `close` may be NULL when there is nothing to do.

        int pf_stream_init(pf_stream_t *s, const struct pf_stream_vt *vt,
                           void *ctx)
        pf_ssize_t pf_stream_read(pf_stream_t *s, void *buf, size_t n)
        pf_ssize_t pf_stream_read_full(pf_stream_t *s, void *buf, size_t n)
                Reads until `n` bytes or the end: fewer only at the end.
        pf_ssize_t pf_stream_write(pf_stream_t *s, const void *buf, size_t n)
        int pf_stream_write_all(pf_stream_t *s, const void *buf, size_t n)
                Writes all `n` bytes: 0 or an error.
        int64_t pf_stream_seek(pf_stream_t *s, int64_t off, int whence)
        int64_t pf_stream_tell(pf_stream_t *s)
        int pf_stream_flush(pf_stream_t *s)
        int pf_stream_close(pf_stream_t *s)
                Releases the stream (its `close`), which cannot be used
                afterwards.
        int pf_stream_putc(pf_stream_t *s, int c)
        int pf_stream_puts(pf_stream_t *s, const char *str)
                0 or an error; puts adds no newline.
        int pf_stream_printf(pf_stream_t *s, const char *fmt, ...)
        int pf_stream_vprintf(pf_stream_t *s, const char *fmt, va_list ap)
                The number of bytes written, or an error. No length limit.
        int64_t pf_stream_copy(pf_stream_t *dst, pf_stream_t *src)
                Copies the rest of `src` to `dst`: the number of bytes.

    Streams of the library (0, or an error):

        int pf_stream_from_file(pf_stream_t *s, FILE *f, bool own)
                On a stdio file, closed with the stream if `own`.
        int pf_stream_fopen(pf_stream_t *s, const char *path,
                            const char *mode)
                pf_fopen, owned by the stream.
        int pf_stream_from_fd(pf_stream_t *s, int fd, bool own)
                On a file descriptor, closed with the stream if `own`.
        int pf_stream_memory(pf_stream_t *s, void *buf, size_t size,
                             bool writable, struct allocator_t *alloc)
                On a fixed buffer: reads and writes stop at `size` (a write
                there fails with PF_ENOSPC). Writes fail with PF_EBADF
                unless `writable`.
        int pf_stream_buffer(pf_stream_t *s, struct allocator_t *alloc)
                On memory that grows as it is written (seeking past the
                end and writing fills the gap with zeros).
        const void *pf_stream_buffer_data(const pf_stream_t *s,
                                          size_t *size)
                The content of a pf_stream_buffer, NUL-terminated (the NUL
                is not counted), valid until the next write or close; NULL
                for other streams.
        int pf_stream_null(pf_stream_t *s)
                Reads nothing, discards what is written.
        The memory streams allocate their state from `alloc` (NULL: the
        library's default).

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_IO, always on Windows
    and elsewhere where the library found them missing when it was built:
    `fseeko`, `ftello`, `pread`, `pwrite`, `fsync`, `ftruncate`,
    `fmemopen`, `open_memstream`. `open`, `read`, `write`, `close` and
    the like are never mapped: they are common names of struct members.

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

#ifndef CPOLYFILL_IO_H
#define CPOLYFILL_IO_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/config.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/stdlib.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#if !PF_OS_WINDOWS
    #include <sys/types.h>
#endif
#ifndef __cplusplus
    #include <stdbool.h>
#endif

typedef int64_t pf_off_t;
#if PF_OS_WINDOWS
typedef unsigned pf_mode_t;
#else
typedef mode_t pf_mode_t;
#endif

#if PF_OS_WINDOWS
    #define PF_O_RDONLY _O_RDONLY
    #define PF_O_WRONLY _O_WRONLY
    #define PF_O_RDWR _O_RDWR
    #define PF_O_CREAT _O_CREAT
    #define PF_O_EXCL _O_EXCL
    #define PF_O_TRUNC _O_TRUNC
    #define PF_O_APPEND _O_APPEND
    #define PF_O_CLOEXEC _O_NOINHERIT
#else
    #define PF_O_RDONLY O_RDONLY
    #define PF_O_WRONLY O_WRONLY
    #define PF_O_RDWR O_RDWR
    #define PF_O_CREAT O_CREAT
    #define PF_O_EXCL O_EXCL
    #define PF_O_TRUNC O_TRUNC
    #define PF_O_APPEND O_APPEND
    /* O_CLOEXEC is hidden from strict C99, its value comes from the
       library's build. */
    #define PF_O_CLOEXEC PF_CONFIG_O_CLOEXEC
#endif

#define PF_WRITE_ATOMIC 1u

struct allocator_t;

struct pf_stream_vt {
    pf_ssize_t (*read)(void *ctx, void *buf, size_t n);
    pf_ssize_t (*write)(void *ctx, const void *buf, size_t n);
    int64_t (*seek)(void *ctx, int64_t off, int whence);
    int (*flush)(void *ctx);
    int (*close)(void *ctx);
};

typedef struct pf_stream_t {
    const struct pf_stream_vt *vt;
    void *ctx;
} pf_stream_t;

PF_BEGIN_DECLS

PF_API int pf_open(const char *path, int flags, pf_mode_t mode);
PF_API int pf_close(int fd);
PF_API pf_ssize_t pf_read(int fd, void *buf, size_t n);
PF_API pf_ssize_t pf_write(int fd, const void *buf, size_t n);
PF_API pf_ssize_t pf_pread(int fd, void *buf, size_t n, pf_off_t off);
PF_API pf_ssize_t pf_pwrite(int fd, const void *buf, size_t n, pf_off_t off);
PF_API pf_off_t pf_lseek(int fd, pf_off_t off, int whence);
PF_API int pf_fsync(int fd);
PF_API int pf_ftruncate(int fd, pf_off_t size);
PF_API int pf_dup(int fd);
PF_API int pf_dup2(int fd, int fd2);

PF_API FILE *pf_fopen(const char *path, const char *mode);
PF_API int pf_fseeko(FILE *f, pf_off_t off, int whence);
PF_API pf_off_t pf_ftello(FILE *f);
PF_API int pf_fileno(FILE *f);
PF_API FILE *pf_fmemopen(void *buf, size_t size, const char *mode);
PF_API FILE *pf_open_memstream(char **ptr, size_t *size);

PF_API int pf_read_file(
    const char *path, void **data, size_t *size, struct allocator_t *alloc
);
PF_API void pf_read_file_free(
    void *data, size_t size, struct allocator_t *alloc
);
PF_API int pf_write_file(
    const char *path, const void *data, size_t size, unsigned flags
);

PF_API int pf_stream_init(
    pf_stream_t *s, const struct pf_stream_vt *vt, void *ctx
);
PF_API pf_ssize_t pf_stream_read(pf_stream_t *s, void *buf, size_t n);
PF_API pf_ssize_t pf_stream_read_full(pf_stream_t *s, void *buf, size_t n);
PF_API pf_ssize_t pf_stream_write(pf_stream_t *s, const void *buf, size_t n);
PF_API int pf_stream_write_all(pf_stream_t *s, const void *buf, size_t n);
PF_API int64_t pf_stream_seek(pf_stream_t *s, int64_t off, int whence);
PF_API int64_t pf_stream_tell(pf_stream_t *s);
PF_API int pf_stream_flush(pf_stream_t *s);
PF_API int pf_stream_close(pf_stream_t *s);
PF_API int pf_stream_putc(pf_stream_t *s, int c);
PF_API int pf_stream_puts(pf_stream_t *s, const char *str);
PF_PRINTF(2, 3)
PF_API int pf_stream_printf(pf_stream_t *s, const char *fmt, ...);
PF_PRINTF(2, 0)
PF_API int pf_stream_vprintf(pf_stream_t *s, const char *fmt, va_list ap);
PF_API int64_t pf_stream_copy(pf_stream_t *dst, pf_stream_t *src);

PF_API int pf_stream_from_file(pf_stream_t *s, FILE *f, bool own);
PF_API int pf_stream_fopen(pf_stream_t *s, const char *path, const char *mode);
PF_API int pf_stream_from_fd(pf_stream_t *s, int fd, bool own);
PF_API int pf_stream_memory(
    pf_stream_t *s,
    void *buf,
    size_t size,
    bool writable,
    struct allocator_t *alloc
);
PF_API int pf_stream_buffer(pf_stream_t *s, struct allocator_t *alloc);
PF_API const void *pf_stream_buffer_data(const pf_stream_t *s, size_t *size);
PF_API int pf_stream_null(pf_stream_t *s);

PF_END_DECLS

#endif /* CPOLYFILL_IO_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__IO_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_IO)) \
        || defined(PF_STD_NAMES_IO))
    #define PF__IO_STD_NAMES

    #ifdef PF__FORCE_STD_NAMES_POLYFILL
        #define PF__IO_MAP(have) 1
    #else
        #define PF__IO_MAP(have) (PF_OS_WINDOWS || !(have))
    #endif

    #if PF__IO_MAP(PF_CONFIG_HAVE_FSEEKO)
        #undef fseeko
        #define fseeko pf_fseeko
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_FTELLO)
        #undef ftello
        #define ftello pf_ftello
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_PREAD)
        #undef pread
        #define pread pf_pread
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_PWRITE)
        #undef pwrite
        #define pwrite pf_pwrite
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_FSYNC)
        #undef fsync
        #define fsync pf_fsync
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_FTRUNCATE)
        #undef ftruncate
        #define ftruncate pf_ftruncate
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_FMEMOPEN)
        #undef fmemopen
        #define fmemopen pf_fmemopen
    #endif
    #if PF__IO_MAP(PF_CONFIG_HAVE_OPEN_MEMSTREAM)
        #undef open_memstream
        #define open_memstream pf_open_memstream
    #endif
#endif
