/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/mman.h> */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/mman.h>

#include <errno.h>
#include <string.h>

#include "files.h"

static int test_sizes(int seed, int i) {
    size_t page = pf_page_size(), gran = pf_map_granularity();
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(page >= 4096);
    PF_ASSERT_ALWAYS_EQ(page & (page - 1), 0u);
    PF_ASSERT_ALWAYS(gran >= page);
    PF_ASSERT_ALWAYS_EQ(gran % page, 0u);
    return 0;
}

static int test_anonymous(int seed, int i) {
    size_t len = 3 * pf_page_size(), k;
    unsigned char *p;
    (void)seed, (void)i;

    p = (unsigned char *)pf_mmap(
        NULL, len, PF_PROT_READ | PF_PROT_WRITE,
        PF_MAP_PRIVATE | PF_MAP_ANONYMOUS, -1, 0
    );
    PF_ASSERT_ALWAYS(p != PF_MAP_FAILED);
    for (k = 0; k < len; k++)
        PF_ASSERT_ALWAYS_EQ(p[k], 0);
    memset(p, 0xAB, len);
    PF_ASSERT_ALWAYS_EQ(p[len - 1], 0xAB);

    PF_ASSERT_ALWAYS_EQ(pf_mprotect(p, len, PF_PROT_READ), 0);
    PF_ASSERT_ALWAYS_EQ(p[0], 0xAB);
    PF_ASSERT_ALWAYS_EQ(pf_mprotect(p, len, PF_PROT_READ | PF_PROT_WRITE), 0);
    p[0] = 1;
    PF_ASSERT_ALWAYS_EQ(pf_mprotect(p, len, 0x100), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);

    if (pf_mlock(p, pf_page_size()) == 0) /* may exceed RLIMIT_MEMLOCK */
        PF_ASSERT_ALWAYS_EQ(pf_munlock(p, pf_page_size()), 0);
    PF_ASSERT_ALWAYS_EQ(pf_munmap(p, len), 0);

    /* shared anonymous memory works too */
    p = (unsigned char *)pf_mmap(
        NULL, len, PF_PROT_READ | PF_PROT_WRITE,
        PF_MAP_SHARED | PF_MAP_ANONYMOUS, -1, 0
    );
    PF_ASSERT_ALWAYS(p != PF_MAP_FAILED);
    p[1] = 2;
    PF_ASSERT_ALWAYS_EQ(pf_munmap(p, len), 0);
    return 0;
}

static int test_invalid(int seed, int i) {
    int rw = PF_PROT_READ | PF_PROT_WRITE;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(pf_mmap(NULL, 0, rw, PF_MAP_PRIVATE | PF_MAP_ANON, -1, 0) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    PF_ASSERT_ALWAYS(pf_mmap(NULL, 4096, rw, PF_MAP_ANON, -1, 0) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    PF_ASSERT_ALWAYS(pf_mmap(NULL, 4096, rw, PF_MAP_SHARED | PF_MAP_PRIVATE | PF_MAP_ANON, -1, 0) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS(pf_mmap(NULL, 4096, rw, PF_MAP_PRIVATE | PF_MAP_ANON | 0x4000, -1, 0) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS(pf_mmap(NULL, 4096, rw, PF_MAP_PRIVATE | PF_MAP_ANON, -1, 1) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS(pf_mmap(NULL, 4096, rw, PF_MAP_PRIVATE, -1, 0) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS_EQ(errno, EBADF);
    return 0;
}

static int test_file(int seed, int i) {
    char dir[PF_PATH_MAX];
    size_t gran = pf_map_granularity(), len = 2 * gran;
    const char *f;
    char *p, buf[8];
    int fd;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");
    fd = pf_open(f, PF_O_RDWR | PF_O_CREAT, 0600);
    PF_ASSERT_ALWAYS(fd >= 0);
    PF_ASSERT_ALWAYS_EQ(pf_ftruncate(fd, (pf_off_t)len), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pwrite(fd, "first", 5, 0), 5);
    PF_ASSERT_ALWAYS_EQ(pf_pwrite(fd, "second", 6, (pf_off_t)gran), 6);

    /* shared: writes reach the file */
    p = (char *)pf_mmap(NULL, len, PF_PROT_READ | PF_PROT_WRITE, PF_MAP_SHARED, fd, 0);
    PF_ASSERT_ALWAYS(p != PF_MAP_FAILED);
    PF_ASSERT_ALWAYS_MEM_EQ(p, "first", 5);
    PF_ASSERT_ALWAYS_MEM_EQ(p + gran, "second", 6);
    memcpy(p, "FIRST", 5);
    PF_ASSERT_ALWAYS_EQ(pf_msync(p, len, PF_MS_SYNC), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pread(fd, buf, 5, 0), 5);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "FIRST", 5);
    PF_ASSERT_ALWAYS_EQ(pf_munmap(p, len), 0);

    /* private: they do not */
    p = (char *)pf_mmap(NULL, len, PF_PROT_READ | PF_PROT_WRITE, PF_MAP_PRIVATE, fd, 0);
    PF_ASSERT_ALWAYS(p != PF_MAP_FAILED);
    memcpy(p, "xxxxx", 5);
    PF_ASSERT_ALWAYS_EQ(pf_pread(fd, buf, 5, 0), 5);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "FIRST", 5);
    PF_ASSERT_ALWAYS_EQ(pf_munmap(p, len), 0);

    /* from an offset */
    p = (char *)pf_mmap(NULL, gran, PF_PROT_READ, PF_MAP_SHARED, fd, (pf_off_t)gran);
    PF_ASSERT_ALWAYS(p != PF_MAP_FAILED);
    PF_ASSERT_ALWAYS_MEM_EQ(p, "second", 6);
    PF_ASSERT_ALWAYS_EQ(pf_munmap(p, gran), 0);
    PF_ASSERT_ALWAYS(pf_mmap(NULL, gran, PF_PROT_READ, PF_MAP_SHARED, fd, (pf_off_t)gran / 2) == PF_MAP_FAILED);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);

    PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_map_file(int seed, int i) {
    char dir[PF_PATH_MAX];
    pf_mapped_file_t m;
    const char *f, *e;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");
    e = test_path(1, dir, "empty");
    test_write(f, "mapped content");
    test_write(e, "");

    PF_ASSERT_ALWAYS_EQ(pf_map_file(&m, f, 0), 0);
    PF_ASSERT_ALWAYS_EQ(m.size, 14u);
    PF_ASSERT_ALWAYS_MEM_EQ(m.data, "mapped content", 14);
    PF_ASSERT_ALWAYS_EQ(pf_unmap_file(&m), 0);
    PF_ASSERT_ALWAYS(m.data == NULL);
    PF_ASSERT_ALWAYS_EQ(m.size, 0u);

    PF_ASSERT_ALWAYS_EQ(pf_map_file(&m, f, PF_MAP_FILE_WRITE), 0);
    memcpy(m.data, "MAPPED", 6);
    PF_ASSERT_ALWAYS_EQ(pf_unmap_file(&m), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(f), "MAPPED content");

    PF_ASSERT_ALWAYS_EQ(pf_map_file(&m, e, 0), 0);
    PF_ASSERT_ALWAYS(m.data == NULL);
    PF_ASSERT_ALWAYS_EQ(m.size, 0u);
    PF_ASSERT_ALWAYS_EQ(pf_unmap_file(&m), 0);

    PF_ASSERT_ALWAYS_EQ(pf_map_file(&m, test_path(2, dir, "none"), 0), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_map_file(&m, dir, 0), PF_EISDIR);
    PF_ASSERT_ALWAYS_EQ(pf_map_file(&m, f, 0x80), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

pf_test_t suite_mman[] = {
    { test_sizes, "sizes" },
    { test_anonymous, "anonymous" },
    { test_invalid, "invalid" },
    { test_file, "file" },
    { test_map_file, "map_file" },
    { 0 },
};
