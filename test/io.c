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
    PF_ASSERT_ALWAYS_EQ(pf_open(f, PF_O_RDWR | PF_O_CREAT | PF_O_EXCL, 0644), -1);
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
    PF_ASSERT_ALWAYS_EQ(pf_lseek(fd, (pf_off_t)5 << 30, SEEK_SET), (pf_off_t)5 << 30);
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

    PF_ASSERT_ALWAYS_EQ(pf_open(test_path(1, dir, "none/x"), PF_O_RDONLY, 0), -1);
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

pf_test_t suite_io[] = {
    { test_fd, "fd" },
    { test_fopen, "fopen" },
    { test_memfile, "memfile" },
    { 0 },
};
