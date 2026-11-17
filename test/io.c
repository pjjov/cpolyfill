/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/io.h> */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if !PF_OS_WINDOWS
    #include <fcntl.h>
#endif

#include "files.h"

static int test_fd(int seed, int i) {
    char dir[PF_PATH_MAX], buf[16];
    const char *f;
    int fd, fd2, fd3;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");

    fd = pf_open(f, PF_O_RDWR | PF_O_CREAT | PF_O_EXCL | PF_O_CLOEXEC, 0644);
    PF_ASSERT_ALWAYS(fd >= 0);
#if !PF_OS_WINDOWS
    PF_ASSERT_ALWAYS(fcntl(fd, F_GETFD) & FD_CLOEXEC);
#endif
    PF_ASSERT_ALWAYS_EQ(
        pf_open(f, PF_O_RDWR | PF_O_CREAT | PF_O_EXCL, 0644), -1
    );
    PF_ASSERT_ALWAYS_EQ(errno, EEXIST);

    /* binary: "\n" stays one byte on Windows */
    PF_ASSERT_ALWAYS_EQ(pf_write(fd, "ab\ncdef", 7), 7);
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd, 0, SEEK_CUR), 7);
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd, 1, SEEK_SET), 1);
    PF_ASSERT_ALWAYS_EQ(pf_read(fd, buf, 3), 3);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "b\nc", 3);
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd, 0, SEEK_END), 7);
    PF_ASSERT_ALWAYS_EQ(pf_read(fd, buf, 3), 0);

    PF_ASSERT_ALWAYS_EQ(pf_pread(fd, buf, 2, 5), 2);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "ef", 2);
    PF_ASSERT_ALWAYS_EQ(pf_pread(fd, buf, 2, 100), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pwrite(fd, "XY", 2, 3), 2);
    PF_ASSERT_ALWAYS_EQ(pf_pread(fd, buf, 7, 0), 7);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "ab\nXYef", 7);

    PF_ASSERT_ALWAYS_EQ(pf_fsync(fd), 0);
    PF_ASSERT_ALWAYS_EQ(pf_ftruncate(fd, 3), 0);
    PF_ASSERT_ALWAYS_EQ(pf_file_size(f), 3);

    /* 64-bit offsets */
    PF_ASSERT_ALWAYS_EQ(
        pf_lseek(fd, (pf_off_t)5 << 30, SEEK_SET), (pf_off_t)5 << 30
    );
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd, 0, SEEK_CUR), (pf_off_t)5 << 30);

    fd2 = pf_dup(fd);
    PF_ASSERT_ALWAYS(fd2 >= 0 && fd2 != fd);
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd2, 0, SEEK_CUR), (pf_off_t)5 << 30);
    fd3 = pf_open(f, PF_O_RDONLY, 0);
    PF_ASSERT_ALWAYS(fd3 >= 0);
    PF_ASSERT_ALWAYS_EQ(pf_dup2(fd, fd3), fd3);
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd3, 0, SEEK_CUR), (pf_off_t)5 << 30);

    PF_ASSERT_ALWAYS_EQ(pf_close(fd3), 0);
    PF_ASSERT_ALWAYS_EQ(pf_close(fd2), 0);
    PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
    PF_ASSERT_ALWAYS_EQ(pf_close(fd), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EBADF);

    PF_ASSERT_ALWAYS_EQ(
        pf_open(test_path(1, dir, "none/x"), PF_O_RDONLY, 0), -1
    );
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_fopen(int seed, int i) {
    char dir[PF_PATH_MAX], buf[16];
    const char *f;
    FILE *fp;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");

    PF_ASSERT_ALWAYS(pf_fopen(f, "r") == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);

    fp = pf_fopen(f, "wxbe");
    PF_ASSERT_ALWAYS(fp != NULL);
#if !PF_OS_WINDOWS
    PF_ASSERT_ALWAYS(fcntl(pf_fileno(fp), F_GETFD) & FD_CLOEXEC);
#endif
    PF_ASSERT_ALWAYS(fputs("one\ntwo\n", fp) >= 0);
    PF_ASSERT_ALWAYS_EQ(fclose(fp), 0);
    PF_ASSERT_ALWAYS(pf_fopen(f, "wx") == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, EEXIST);

    fp = pf_fopen(f, "ab");
    PF_ASSERT_ALWAYS(fp != NULL);
    PF_ASSERT_ALWAYS(fputs("3\n", fp) >= 0);
    PF_ASSERT_ALWAYS_EQ(fclose(fp), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(f), "one\ntwo\n3\n");

    fp = pf_fopen(f, "r+b");
    PF_ASSERT_ALWAYS(fp != NULL);
    PF_ASSERT_ALWAYS_EQ(pf_fseeko(fp, 4, SEEK_SET), 0);
    PF_ASSERT_ALWAYS_EQ(pf_ftello(fp), 4);
    PF_ASSERT_ALWAYS_EQ(fread(buf, 1, 3, fp), 3u);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "two", 3);
    PF_ASSERT_ALWAYS_EQ(pf_fseeko(fp, 0, SEEK_SET), 0);
    PF_ASSERT_ALWAYS_EQ(fputc('O', fp), 'O');
    PF_ASSERT_ALWAYS_EQ(pf_fseeko(fp, (pf_off_t)3 << 30, SEEK_SET), 0);
    PF_ASSERT_ALWAYS_EQ(pf_ftello(fp), (pf_off_t)3 << 30);
    PF_ASSERT_ALWAYS_EQ(fclose(fp), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(f), "One\ntwo\n3\n");

    PF_ASSERT_ALWAYS(pf_fopen(f, "q") == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_memfile(int seed, int i) {
    char data[] = "memory file", buf[32];
    FILE *fp;
    (void)seed, (void)i;

    fp = pf_fmemopen(data, strlen(data), "r");
    if (!fp) {
        PF_ASSERT_ALWAYS_EQ(errno, ENOSYS);
        return SKIP("no fmemopen");
    }
    PF_ASSERT_ALWAYS_EQ(fread(buf, 1, sizeof(buf), fp), strlen(data));
    PF_ASSERT_ALWAYS_MEM_EQ(buf, data, strlen(data));
    PF_ASSERT_ALWAYS_EQ(fclose(fp), 0);

#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS(pf_fmemopen(data, sizeof(data), "w") == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ENOSYS);
#endif
    {
        char *p = NULL;
        size_t n = 0;
        fp = pf_open_memstream(&p, &n);
        if (!fp) {
            PF_ASSERT_ALWAYS_EQ(errno, ENOSYS);
            return 0;
        }
        PF_ASSERT_ALWAYS(fputs("abc", fp) >= 0);
        PF_ASSERT_ALWAYS_EQ(fclose(fp), 0);
        PF_ASSERT_ALWAYS_EQ(n, 3u);
        PF_ASSERT_ALWAYS_STR_EQ(p, "abc");
        free(p);
    }
    return 0;
}

/* ---- Whole files ------------------------------------------------------ */

/* An allocator that counts what is outstanding, to check the sizes the
   library frees with. */
typedef struct {
    allocator_t base;
    size_t bytes, blocks;
} counting_alloc;

static void *counting_fn(
    allocator_t *self, void *ptr, size_t old, size_t size, size_t zalign
) {
    counting_alloc *a = (counting_alloc *)self;
    void *p = pf__default_allocate(ptr, old, size, zalign);
    if (size == 0 || p) {
        if (ptr) {
            a->bytes -= old;
            a->blocks--;
        }
        if (size) {
            a->bytes += size;
            a->blocks++;
        }
    }
    return p;
}

static int test_whole_files(int seed, int i) {
    counting_alloc alloc = { { counting_fn }, 0, 0 };
    char dir[PF_PATH_MAX], big[20000];
    const char *f, *g;
    void *data;
    size_t size, k;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");
    g = test_path(1, dir, "atomic");

    PF_ASSERT_ALWAYS_EQ(pf_write_file(f, "text\n", 5, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_read_file(f, &data, &size, &alloc.base), 0);
    PF_ASSERT_ALWAYS_EQ(size, 5u);
    PF_ASSERT_ALWAYS_STR_EQ((const char *)data, "text\n");
    PF_ASSERT_ALWAYS_EQ(alloc.bytes, 6u); /* exactly size + 1 */
    pf_read_file_free(data, size, &alloc.base);
    PF_ASSERT_ALWAYS_EQ(alloc.blocks, 0u);

    /* larger than the first guess of a file of unknown size */
    for (k = 0; k < sizeof(big); k++)
        big[k] = (char)('a' + k % 26);
    PF_ASSERT_ALWAYS_EQ(pf_write_file(f, big, sizeof(big), 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_read_file(f, &data, &size, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(size, sizeof(big));
    PF_ASSERT_ALWAYS_MEM_EQ(data, big, sizeof(big));
    pf_read_file_free(data, size, NULL);
#if PF_OS_LINUX
    /* /proc files report size 0 */
    PF_ASSERT_ALWAYS_EQ(
        pf_read_file("/proc/self/status", &data, &size, &alloc.base), 0
    );
    PF_ASSERT_ALWAYS(size > 0);
    PF_ASSERT_ALWAYS(strstr((const char *)data, "Name:") != NULL);
    PF_ASSERT_ALWAYS_EQ(alloc.bytes, size + 1);
    pf_read_file_free(data, size, &alloc.base);
#endif

    /* empty */
    PF_ASSERT_ALWAYS_EQ(pf_write_file(f, NULL, 0, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_read_file(f, &data, &size, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(size, 0u);
    PF_ASSERT_ALWAYS_EQ(*(const char *)data, 0);
    pf_read_file_free(data, size, NULL);

    /* atomic: replaces, keeps the permissions, leaves nothing behind */
    PF_ASSERT_ALWAYS_EQ(pf_write_file(g, "one", 3, PF_WRITE_ATOMIC), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(g), "one");
    PF_ASSERT_ALWAYS_EQ(pf_chmod(g, 0640), 0);
    PF_ASSERT_ALWAYS_EQ(pf_write_file(g, "two!", 4, PF_WRITE_ATOMIC), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(g), "two!");
#if !PF_OS_WINDOWS
    {
        pf_stat_t st;
        PF_ASSERT_ALWAYS_EQ(pf_stat(g, &st), 0);
        PF_ASSERT_ALWAYS_EQ(st.perm, 0640u);
    }
#endif
    PF_ASSERT_ALWAYS_EQ(pf_unlink(f), 0);
    PF_ASSERT_ALWAYS_EQ(pf_unlink(g), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rmdir(dir), 0); /* no temporary file left */

    PF_ASSERT_ALWAYS_EQ(pf_read_file(f, &data, &size, NULL), PF_ENOENT);
    PF_ASSERT_ALWAYS(data == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_write_file(f, "x", 1, 0), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_write_file(f, "x", 1, PF_WRITE_ATOMIC), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_write_file(f, NULL, 1, 0), PF_EINVAL);
    return 0;
}

/* ---- Streams ---------------------------------------------------------- */

/* Writes and reads back through a seekable stream. */
static void check_rw(pf_stream_t *s) {
    char buf[64];

    PF_ASSERT_ALWAYS_EQ(pf_stream_puts(s, "hello "), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_printf(s, "%s %d", "world", 42), 8);
    PF_ASSERT_ALWAYS_EQ(pf_stream_putc(s, '!'), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_tell(s), 15);
    PF_ASSERT_ALWAYS_EQ(pf_stream_flush(s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(s, 6, SEEK_SET), 6);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read_full(s, buf, 5), 5);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "world", 5);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(s, -3, SEEK_END), 12);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read_full(s, buf, sizeof(buf)), 3);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "42!", 3);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read(s, buf, sizeof(buf)), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(s, 0, 99), PF_EINVAL);
}

static int test_streams(int seed, int i) {
    char dir[PF_PATH_MAX], fixed[8];
    const char *f;
    pf_stream_t s, t;
    const char *data;
    size_t size;
    int fd;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");

    /* stdio */
    PF_ASSERT_ALWAYS_EQ(pf_stream_fopen(&s, f, "w+b"), 0);
    check_rw(&s);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);
    PF_ASSERT_ALWAYS(s.vt == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), PF_EINVAL);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(f), "hello world 42!");
    PF_ASSERT_ALWAYS_EQ(
        pf_stream_fopen(&s, test_path(1, dir, "none/x"), "r"), PF_ENOENT
    );

    /* a FILE that stays open */
    {
        FILE *fp = pf_fopen(f, "rb");
        char c;
        PF_ASSERT_ALWAYS_EQ(pf_stream_from_file(&s, fp, false), 0);
        PF_ASSERT_ALWAYS_EQ(pf_stream_read(&s, &c, 1), 1);
        PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);
        PF_ASSERT_ALWAYS_EQ(fgetc(fp), 'e');
        fclose(fp);
    }

    /* descriptor */
    fd = pf_open(f, PF_O_RDWR | PF_O_TRUNC, 0);
    PF_ASSERT_ALWAYS(fd >= 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_from_fd(&s, fd, true), 0);
    check_rw(&s);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_close(fd), -1); /* closed by the stream */
    PF_ASSERT_ALWAYS_EQ(pf_stream_from_fd(&s, -1, false), PF_EBADF);

    /* growing buffer */
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&s, NULL), 0);
    data = (const char *)pf_stream_buffer_data(&s, &size);
    PF_ASSERT_ALWAYS_EQ(size, 0u);
    PF_ASSERT_ALWAYS_STR_EQ(data, "");
    check_rw(&s);
    data = (const char *)pf_stream_buffer_data(&s, &size);
    PF_ASSERT_ALWAYS_EQ(size, 15u);
    PF_ASSERT_ALWAYS_STR_EQ(data, "hello world 42!");
    /* a gap is zeros */
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&s, 20, SEEK_SET), 20);
    PF_ASSERT_ALWAYS_EQ(pf_stream_putc(&s, 'z'), 0);
    data = (const char *)pf_stream_buffer_data(&s, &size);
    PF_ASSERT_ALWAYS_EQ(size, 21u);
    PF_ASSERT_ALWAYS_MEM_EQ(data + 15, "\0\0\0\0\0z", 7);
    /* a long printf */
    PF_ASSERT_ALWAYS_EQ(pf_stream_printf(&s, "%0500d", 7), 500);
    pf_stream_buffer_data(&s, &size);
    PF_ASSERT_ALWAYS_EQ(size, 521u);

    /* copy into another buffer */
    PF_ASSERT_ALWAYS_EQ(pf_stream_buffer(&t, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&s, 0, SEEK_SET), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_copy(&t, &s), 521);
    data = (const char *)pf_stream_buffer_data(&t, &size);
    PF_ASSERT_ALWAYS_EQ(size, 521u);
    PF_ASSERT_ALWAYS_MEM_EQ(data, "hello world 42!", 15);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&t), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);

    /* fixed buffer */
    memset(fixed, '.', sizeof(fixed));
    PF_ASSERT_ALWAYS_EQ(
        pf_stream_memory(&s, fixed, sizeof(fixed), true, NULL), 0
    );
    PF_ASSERT_ALWAYS(pf_stream_buffer_data(&s, &size) == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_stream_write(&s, "0123456789", 10), 8);
    PF_ASSERT_ALWAYS_EQ(pf_stream_write(&s, "x", 1), PF_ENOSPC);
    PF_ASSERT_ALWAYS_EQ(pf_stream_puts(&s, "x"), PF_ENOSPC);
    PF_ASSERT_ALWAYS_MEM_EQ(fixed, "01234567", 8);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&s, 9, SEEK_SET), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&s, -1, SEEK_SET), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&s, 2, SEEK_SET), 2);
    {
        char buf[16];
        PF_ASSERT_ALWAYS_EQ(pf_stream_read(&s, buf, sizeof(buf)), 6);
        PF_ASSERT_ALWAYS_MEM_EQ(buf, "234567", 6);
    }
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf_stream_memory(&s, fixed, sizeof(fixed), false, NULL), 0
    );
    PF_ASSERT_ALWAYS_EQ(pf_stream_write(&s, "x", 1), PF_EBADF);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);

    /* null */
    PF_ASSERT_ALWAYS_EQ(pf_stream_null(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_printf(&s, "%d", 123), 3);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read(&s, fixed, 1), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);

    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

/* A custom stream: write only, counting, no seek. */
static pf_ssize_t count_write(void *ctx, const void *buf, size_t n) {
    (void)buf;
    *(size_t *)ctx += n > 3 ? 3 : n; /* short writes */
    return (pf_ssize_t)(n > 3 ? 3 : n);
}

static int test_custom_stream(int seed, int i) {
    static const struct pf_stream_vt vt = {
        NULL, count_write, NULL, NULL, NULL
    };
    pf_stream_t s;
    size_t count = 0;
    char c;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_stream_init(&s, NULL, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_stream_init(&s, &vt, &count), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_write_all(&s, "0123456789", 10), 0);
    PF_ASSERT_ALWAYS_EQ(count, 10u);
    PF_ASSERT_ALWAYS_EQ(pf_stream_printf(&s, "%s", "abcdefg"), 7);
    PF_ASSERT_ALWAYS_EQ(count, 17u);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read(&s, &c, 1), PF_EBADF);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&s, 0, SEEK_SET), PF_ESPIPE);
    PF_ASSERT_ALWAYS_EQ(pf_stream_tell(&s), PF_ESPIPE);
    PF_ASSERT_ALWAYS_EQ(pf_stream_flush(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&s), 0);
    return 0;
}

pf_test_t suite_io[] = {
    { test_fd, "fd" },
    { test_fopen, "fopen" },
    { test_memfile, "memfile" },
    { test_whole_files, "whole_files" },
    { test_streams, "streams" },
    { test_custom_stream, "custom_stream" },
    { 0 },
};
