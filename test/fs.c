/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/fs.h> */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/string.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !PF_OS_WINDOWS
    #include <pwd.h>
    #include <unistd.h>
#endif

#include "files.h"

/* "/" in the expected paths stands for the system's separator. */
static const char *native(const char *s) {
    static char buf[4][256];
    static int slot;
    char *p = buf[slot = (slot + 1) % 4];
    size_t i;
    for (i = 0; s[i] && i < 255; i++)
        p[i] = s[i] == '/' ? PF_PATH_SEP : s[i];
    p[i] = 0;
    return p;
}

/* ---- Path strings ----------------------------------------------------- */

static int test_root(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len(""), 0u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("a/b"), 0u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("/"), 1u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("/a"), 1u);
    PF_ASSERT_ALWAYS(pf_path_is_absolute("/a"));
    PF_ASSERT_ALWAYS(!pf_path_is_absolute("a"));
    PF_ASSERT_ALWAYS(!pf_path_is_absolute(""));
    PF_ASSERT_ALWAYS(pf_path_is_sep('/'));
    PF_ASSERT_ALWAYS(!pf_path_is_sep('a'));
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS(pf_path_is_sep('\\'));
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("C:\\x"), 3u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("C:x"), 2u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("\\x"), 1u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("\\\\srv\\share\\x"), 13u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("\\\\srv\\share"), 11u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("\\\\?\\C:\\x"), 7u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("\\\\?\\UNC\\srv\\share\\x"), 18u);
    PF_ASSERT_ALWAYS(pf_path_is_absolute("C:\\x"));
    PF_ASSERT_ALWAYS(pf_path_is_absolute("C:/x"));
    PF_ASSERT_ALWAYS(pf_path_is_absolute("\\\\srv\\share"));
    PF_ASSERT_ALWAYS(!pf_path_is_absolute("C:x"));
    PF_ASSERT_ALWAYS(!pf_path_is_absolute("\\x"));
#else
    PF_ASSERT_ALWAYS(!pf_path_is_sep('\\'));
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("\\x"), 0u);
    PF_ASSERT_ALWAYS_EQ(pf_path_root_len("C:/x"), 0u);
#endif
    return 0;
}

static int test_join(int seed, int i) {
    char buf[64];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "a", "b"), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, native("a/b"));
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "a/", "b"), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "a/b");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "", "b"), 1);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "b");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "a", "/b"), 2);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/b");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "/", "b"), 2);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/b");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "a", ""), 2);
    PF_ASSERT_ALWAYS_STR_EQ(buf, native("a/"));
    /* the length only, and a buffer too small */
    PF_ASSERT_ALWAYS_EQ(pf_path_join(NULL, 0, "abc", "de"), 6);
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, 6, "abc", "de"), PF_ERANGE);
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, 7, "abc", "de"), 6);
    /* the output may be the first argument */
    strcpy(buf, "dir");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), buf, "x"), 5);
    PF_ASSERT_ALWAYS_STR_EQ(buf, native("dir/x"));
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), NULL, "x"), PF_EINVAL);
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "C:", "x"), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "C:x");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "a", "D:\\x"), 4);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "D:\\x");
    PF_ASSERT_ALWAYS_EQ(pf_path_join(buf, sizeof(buf), "a", "\\x"), 2);
#endif
    return 0;
}

static int test_normalize(int seed, int i) {
    static const char *const cases[][2] = {
        { "", "." },
        { ".", "." },
        { "./", "." },
        { "a", "a" },
        { "a/", "a" },
        { "a//b", "a/b" },
        { "a/./b/.", "a/b" },
        { "a/b/..", "a" },
        { "a/..", "." },
        { "a/../..", ".." },
        { "../a", "../a" },
        { "../../a/../b", "../../b" },
        { "a/b/../../../c", "../c" },
        { "/", "/" },
        { "//", "/" },
        { "/..", "/" },
        { "/../a", "/a" },
        { "/a/b/../c/./d/", "/a/c/d" },
        { "/a/..", "/" },
        { "a/.../b", "a/.../b" },
        { "a/..b/c", "a/..b/c" },
#if PF_OS_WINDOWS
        { "C:\\a\\..\\..", "C:\\" },
        { "C:/a/b", "C:\\a\\b" },
        { "C:..\\a", "C:..\\a" },
        { "C:a\\..", "C:" },
        { "\\\\srv\\share\\a\\..\\..", "\\\\srv\\share\\" },
        { "\\\\srv\\share", "\\\\srv\\share" },
        { "//srv/share/x", "\\\\srv\\share\\x" },
#endif
    };
    char buf[64];
    size_t k;
    (void)seed, (void)i;

    for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
        const char *want = native(cases[k][1]);
        ptrdiff_t n = pf_path_normalize(buf, sizeof(buf), cases[k][0]);
        PF_ASSERT_ALWAYS_MSG(n >= 0, "%s", cases[k][0]);
        PF_ASSERT_ALWAYS_MSG(strcmp(buf, want) == 0, "%s -> %s", cases[k][0], buf);
        PF_ASSERT_ALWAYS_EQ((size_t)n, strlen(want));
        PF_ASSERT_ALWAYS_EQ(pf_path_normalize(NULL, 0, cases[k][0]), n);
        PF_ASSERT_ALWAYS_EQ(pf_path_normalize(buf, (size_t)n, cases[k][0]), PF_ERANGE);
    }
    PF_ASSERT_ALWAYS_EQ(pf_path_normalize(buf, sizeof(buf), NULL), PF_EINVAL);
    return 0;
}

static void check_part(
    const char *(*fn)(const char *, size_t *), const char *path, const char *want
) {
    size_t len = 99;
    const char *p = fn(path, &len);
    PF_ASSERT_ALWAYS_MSG(
        len == strlen(want) && memcmp(p, want, len) == 0,
        "'%s' -> '%.*s'", path, (int)len, p
    );
}

static int test_parts(int seed, int i) {
    size_t len;
    const char *p, *s;
    (void)seed, (void)i;

    check_part(pf_path_basename, "a/b", "b");
    check_part(pf_path_basename, "a/b/", "b");
    check_part(pf_path_basename, "a//b//", "b");
    check_part(pf_path_basename, "b", "b");
    check_part(pf_path_basename, "/", "/");
    check_part(pf_path_basename, "//", "/");
    check_part(pf_path_basename, "", ".");
    check_part(pf_path_basename, "/a", "a");

    check_part(pf_path_dirname, "a/b", "a");
    check_part(pf_path_dirname, "a/b/", "a");
    check_part(pf_path_dirname, "a//b", "a");
    check_part(pf_path_dirname, "/a/b/c", "/a/b");
    check_part(pf_path_dirname, "b", ".");
    check_part(pf_path_dirname, "b/", ".");
    check_part(pf_path_dirname, "/", "/");
    check_part(pf_path_dirname, "/a", "/");
    check_part(pf_path_dirname, "//a", "/");
    check_part(pf_path_dirname, "", ".");

    check_part(pf_path_extension, "a.txt", ".txt");
    check_part(pf_path_extension, "d.x/a.tar.gz", ".gz");
    check_part(pf_path_extension, "d.x/a", "");
    check_part(pf_path_extension, ".bashrc", "");
    check_part(pf_path_extension, "..", "");
    check_part(pf_path_extension, "..a.b", ".b");
    check_part(pf_path_extension, "a.", ".");
    check_part(pf_path_extension, "a.c/", ".c");
    check_part(pf_path_extension, "/", "");
#if PF_OS_WINDOWS
    check_part(pf_path_basename, "C:\\a\\b", "b");
    check_part(pf_path_basename, "C:\\", "C:\\");
    check_part(pf_path_basename, "C:a", "a");
    check_part(pf_path_dirname, "C:\\a", "C:\\");
    check_part(pf_path_dirname, "C:a", "C:");
    check_part(pf_path_dirname, "\\\\srv\\share\\a", "\\\\srv\\share\\");
    check_part(pf_path_extension, "\\\\.\\x.y", "");
#endif

    /* The results point into the path. */
    s = "dir/name.ext";
    p = pf_path_basename(s, &len);
    PF_ASSERT_ALWAYS(p == s + 4);
    p = pf_path_extension(s, &len);
    PF_ASSERT_ALWAYS(p == s + 8);
    p = pf_path_extension("dir/name", &len);
    PF_ASSERT_ALWAYS_EQ(len, 0u);
    PF_ASSERT_ALWAYS_EQ(*p, 0);
    /* `len` may be NULL */
    PF_ASSERT_ALWAYS(pf_path_dirname(s, NULL) == s);
    return 0;
}

/* ---- File system ------------------------------------------------------ */

static int test_basic(int seed, int i) {
    char dir[PF_PATH_MAX];
    const char *f, *g, *sub;
    pf_stat_t st;
    struct timespec before;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");
    g = test_path(1, dir, "renamed");
    sub = test_path(2, dir, "sub");

    PF_ASSERT_ALWAYS(pf_is_dir(dir));
    PF_ASSERT_ALWAYS(!pf_exists(f));
    PF_ASSERT_ALWAYS_EQ(pf_stat(f, &st), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_file_size(f), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_access(f, PF_F_OK), -1);
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);

    pf_timespec_get(&before, PF_TIME_UTC);
    before.tv_sec -= 2; /* coarse file system clocks */
    test_write(f, "hello");
    PF_ASSERT_ALWAYS(pf_exists(f));
    PF_ASSERT_ALWAYS(pf_is_file(f));
    PF_ASSERT_ALWAYS(!pf_is_dir(f));
    PF_ASSERT_ALWAYS(!pf_is_symlink(f));
    PF_ASSERT_ALWAYS_EQ(pf_file_size(f), 5);
    PF_ASSERT_ALWAYS_EQ(pf_access(f, PF_R_OK | PF_W_OK), 0);

    PF_ASSERT_ALWAYS_EQ(pf_stat(f, &st), 0);
    PF_ASSERT_ALWAYS_EQ(st.type, PF_FILE_REGULAR);
    PF_ASSERT_ALWAYS_EQ(st.size, 5u);
    PF_ASSERT_ALWAYS_EQ(st.nlink, 1u);
    PF_ASSERT_ALWAYS(pf_timespec_cmp(st.mtime, before) > 0);
    PF_ASSERT_ALWAYS(pf_timespec_cmp(st.ctime, before) > 0);
    if (st.btime.tv_sec != 0)
        PF_ASSERT_ALWAYS(pf_timespec_cmp(st.btime, before) > 0);
    PF_ASSERT_ALWAYS(st.perm & 0400);

    /* chmod: read-only */
    PF_ASSERT_ALWAYS_EQ(pf_chmod(f, 0444), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stat(f, &st), 0);
    PF_ASSERT_ALWAYS_EQ(st.perm & 0222, 0u);
#if !PF_OS_WINDOWS
    if (geteuid() != 0) /* root may write anything */
#endif
        PF_ASSERT_ALWAYS_EQ(pf_access(f, PF_W_OK), -1);
    PF_ASSERT_ALWAYS_EQ(pf_chmod(f, 0644), 0);
    PF_ASSERT_ALWAYS_EQ(pf_access(f, PF_W_OK), 0);

    /* truncate */
    PF_ASSERT_ALWAYS_EQ(pf_truncate(f, 2), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(f), "he");
    PF_ASSERT_ALWAYS_EQ(pf_truncate(f, 4), 0);
    PF_ASSERT_ALWAYS_MEM_EQ(test_read(f), "he\0\0", 4);

    /* rename, also over an existing file */
    PF_ASSERT_ALWAYS_EQ(pf_rename(f, g), 0);
    PF_ASSERT_ALWAYS(!pf_exists(f));
    test_write(f, "new");
    PF_ASSERT_ALWAYS_EQ(pf_rename(f, g), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(g), "new");

    /* directories */
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(sub, 0755), 0);
    PF_ASSERT_ALWAYS(pf_is_dir(sub));
    PF_ASSERT_ALWAYS_EQ(pf_stat(sub, &st), 0);
    PF_ASSERT_ALWAYS_EQ(st.type, PF_FILE_DIR);
    PF_ASSERT_ALWAYS_EQ(pf_access(sub, PF_X_OK), 0);
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(sub, 0755), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EEXIST);
    test_write(test_path(3, sub, "x"), "x");
    PF_ASSERT_ALWAYS_EQ(pf_rmdir(sub), -1);
    PF_ASSERT_ALWAYS(errno == ENOTEMPTY || errno == EEXIST);
    PF_ASSERT_ALWAYS_EQ(pf_unlink(test_path(3, sub, "x")), 0);
    PF_ASSERT_ALWAYS_EQ(pf_rmdir(sub), 0);
    PF_ASSERT_ALWAYS(!pf_exists(sub));

    /* unlink */
    PF_ASSERT_ALWAYS_EQ(pf_unlink(g), 0);
    PF_ASSERT_ALWAYS_EQ(pf_unlink(g), -1);
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);

    PF_ASSERT_ALWAYS_EQ(pf_rmdir(dir), 0);
    return 0;
}

static int test_fstat(int seed, int i) {
    char dir[PF_PATH_MAX];
    const char *f;
    pf_stat_t a, b;
    int fd;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");
    fd = pf_open(f, PF_O_RDWR | PF_O_CREAT | PF_O_EXCL, 0600);
    PF_ASSERT_ALWAYS(fd >= 0);
    PF_ASSERT_ALWAYS_EQ(pf_write(fd, "abc", 3), 3);
    PF_ASSERT_ALWAYS_EQ(pf_fstat(fd, &a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stat(f, &b), 0);
    PF_ASSERT_ALWAYS_EQ(a.type, PF_FILE_REGULAR);
    PF_ASSERT_ALWAYS_EQ(a.size, 3u);
    PF_ASSERT_ALWAYS_EQ(a.dev, b.dev);
    PF_ASSERT_ALWAYS_EQ(a.ino, b.ino);
#if !PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_EQ(a.perm, 0600u & ~0u);
    PF_ASSERT_ALWAYS_EQ(a.uid, (uint32_t)geteuid());
#endif
    PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
    PF_ASSERT_ALWAYS(pf_fstat(fd, &a) < 0);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_links(int seed, int i) {
    char dir[PF_PATH_MAX], buf[PF_PATH_MAX];
    const char *f, *hard, *soft, *dsoft;
    pf_stat_t st;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "file");
    hard = test_path(1, dir, "hard");
    soft = test_path(2, dir, "soft");
    dsoft = test_path(3, dir, "dsoft");
    test_write(f, "data");

    PF_ASSERT_ALWAYS_EQ(pf_link(f, hard), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stat(f, &st), 0);
    PF_ASSERT_ALWAYS_EQ(st.nlink, 2u);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(hard), "data");

    if (pf_symlink("file", soft) != 0) {
        PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
        return SKIP("no permission to create symbolic links");
    }
    PF_ASSERT_ALWAYS(pf_is_symlink(soft));
    PF_ASSERT_ALWAYS(pf_is_file(soft));
    PF_ASSERT_ALWAYS_EQ(pf_lstat(soft, &st), 0);
    PF_ASSERT_ALWAYS_EQ(st.type, PF_FILE_SYMLINK);
    PF_ASSERT_ALWAYS_EQ(pf_stat(soft, &st), 0);
    PF_ASSERT_ALWAYS_EQ(st.type, PF_FILE_REGULAR);
    PF_ASSERT_ALWAYS_EQ(pf_readlink(soft, buf, sizeof(buf)), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "file", 4);
    PF_ASSERT_ALWAYS_EQ(pf_readlink(soft, buf, 2), 2); /* truncated */
    PF_ASSERT_ALWAYS_EQ(pf_readlink(f, buf, sizeof(buf)), -1);
    PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(soft), "data");

    /* a link to a directory, removed with unlink */
    PF_ASSERT_ALWAYS_EQ(pf_symlink(".", dsoft), 0);
    PF_ASSERT_ALWAYS(pf_is_dir(dsoft));
    PF_ASSERT_ALWAYS(pf_is_symlink(dsoft));
    PF_ASSERT_ALWAYS_EQ(pf_unlink(dsoft), 0);
    PF_ASSERT_ALWAYS(pf_is_dir(dir));

    /* a dangling link */
    PF_ASSERT_ALWAYS_EQ(pf_unlink(f), 0);
    PF_ASSERT_ALWAYS_EQ(pf_unlink(hard), 0);
    PF_ASSERT_ALWAYS(pf_is_symlink(soft));
    PF_ASSERT_ALWAYS(!pf_exists(soft));
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_cwd(int seed, int i) {
    char dir[PF_PATH_MAX], real[PF_PATH_MAX], buf[PF_PATH_MAX], *old, *p;
    size_t len;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    PF_ASSERT_ALWAYS(pf_realpath(dir, real) == real);

    old = pf_getcwd(NULL, 0);
    PF_ASSERT_ALWAYS(old != NULL);
    PF_ASSERT_ALWAYS_EQ(pf_chdir(dir), 0);
    PF_ASSERT_ALWAYS(pf_getcwd(buf, sizeof(buf)) == buf);
    PF_ASSERT_ALWAYS_STR_EQ(buf, real);
    len = strlen(real);
    PF_ASSERT_ALWAYS(pf_getcwd(buf, len) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ERANGE);
    p = pf_getcwd(NULL, len + 1);
    PF_ASSERT_ALWAYS_STR_EQ(p, real);
    free(p);

    /* relative paths resolve against it */
    test_write("rel", "x");
    PF_ASSERT_ALWAYS(pf_is_file(test_path(0, real, "rel")));
    p = pf_realpath("rel", NULL);
    PF_ASSERT_ALWAYS_STR_EQ(p, test_path(0, real, "rel"));
    free(p);
    PF_ASSERT_ALWAYS(pf_realpath("missing", buf) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_mkdir("sub", 0755), 0);
    PF_ASSERT_ALWAYS(pf_realpath("sub/../rel", buf) == buf);
    PF_ASSERT_ALWAYS_STR_EQ(buf, test_path(0, real, "rel"));

    PF_ASSERT_ALWAYS_EQ(pf_chdir(old), 0);
    free(old);
    PF_ASSERT_ALWAYS_EQ(pf_chdir(test_path(0, real, "missing")), -1);
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_temp(int seed, int i) {
    char dir[PF_PATH_MAX], a[PF_PATH_MAX], b[PF_PATH_MAX];
    int fd, k;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    /* The system's and the library's own (Windows) */
    for (k = 0; k < 2; k++) {
        strcpy(a, test_path(0, dir, "fXXXXXX"));
        fd = k ? pf__mktemp(a, false) : pf_mkstemp(a);
        PF_ASSERT_ALWAYS(fd >= 0);
        PF_ASSERT_ALWAYS(strstr(a, "XXXXXX") == NULL);
        PF_ASSERT_ALWAYS(pf_is_file(a));
        PF_ASSERT_ALWAYS_EQ(pf_write(fd, "z", 1), 1);
        PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
        PF_ASSERT_ALWAYS_EQ(pf_file_size(a), 1);

        strcpy(b, test_path(1, dir, "dXXXXXX"));
        if (k)
            PF_ASSERT_ALWAYS_EQ(pf__mktemp(b, true), 0);
        else
            PF_ASSERT_ALWAYS(pf_mkdtemp(b) == b);
        PF_ASSERT_ALWAYS(strstr(b, "XXXXXX") == NULL);
        PF_ASSERT_ALWAYS(pf_is_dir(b));
#if !PF_OS_WINDOWS
        {
            pf_stat_t st;
            PF_ASSERT_ALWAYS_EQ(pf_stat(b, &st), 0);
            PF_ASSERT_ALWAYS_EQ(st.perm, 0700u);
            PF_ASSERT_ALWAYS_EQ(pf_stat(a, &st), 0);
            PF_ASSERT_ALWAYS_EQ(st.perm, 0600u);
        }
#endif
        strcpy(a, "bad-template");
        PF_ASSERT_ALWAYS_EQ(k ? pf__mktemp(a, false) : pf_mkstemp(a), -1);
        PF_ASSERT_ALWAYS_EQ(errno, EINVAL);
    }
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_trees(int seed, int i) {
    char dir[PF_PATH_MAX], outside[PF_PATH_MAX];
    const char *deep, *f;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    test_tmpdir(outside, sizeof(outside));
    deep = test_path(0, dir, native("a/b/c/d"));
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(deep, 0755), 0);
    PF_ASSERT_ALWAYS(pf_is_dir(deep));
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(deep, 0755), 0); /* exists */
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(test_path(1, dir, "a/b/"), 0755), 0);
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(test_path(1, dir, "a/../e/f"), 0755), 0);
    PF_ASSERT_ALWAYS(pf_is_dir(test_path(1, dir, "e/f")));

    f = test_path(1, dir, "a/file");
    test_write(f, "x");
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(f, 0755), PF_EEXIST);
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(test_path(2, f, "sub"), 0755), PF_ENOTDIR);

    /* remove_all takes links, not what they point to */
    test_write(test_path(2, outside, "keep"), "k");
    if (pf_symlink(outside, test_path(2, dir, "a/b/link")) == 0)
        PF_ASSERT_ALWAYS(pf_is_symlink(test_path(2, dir, "a/b/link")));
    PF_ASSERT_ALWAYS_EQ(pf_chmod(f, 0444), 0); /* read-only */
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    PF_ASSERT_ALWAYS(!pf_exists(dir));
    PF_ASSERT_ALWAYS(pf_is_file(test_path(2, outside, "keep")));
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0); /* missing */

    /* a single file */
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(test_path(2, outside, "keep")), 0);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(outside), 0);
    PF_ASSERT_ALWAYS(!pf_exists(outside));
    return 0;
}

static int test_copy(int seed, int i) {
    char dir[PF_PATH_MAX], big[100000];
    const char *a, *b, *c;
    FILE *f;
    size_t k;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    a = test_path(0, dir, "a");
    b = test_path(1, dir, "b");
    c = test_path(2, dir, "c");

    for (k = 0; k < sizeof(big); k++)
        big[k] = (char)(k * 7 + k / 251);
    f = pf_fopen(a, "wb");
    PF_ASSERT_ALWAYS(f != NULL);
    PF_ASSERT_ALWAYS_EQ(fwrite(big, 1, sizeof(big), f), sizeof(big));
    PF_ASSERT_ALWAYS_EQ(fclose(f), 0);
    PF_ASSERT_ALWAYS_EQ(pf_chmod(a, 0640), 0);

    PF_ASSERT_ALWAYS_EQ(pf_copy_file(a, b, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_file_size(b), (int64_t)sizeof(big));
    {
        static char back[sizeof(big)];
        f = pf_fopen(b, "rb");
        PF_ASSERT_ALWAYS(f != NULL);
        PF_ASSERT_ALWAYS_EQ(fread(back, 1, sizeof(back), f), sizeof(back));
        fclose(f);
        PF_ASSERT_ALWAYS_MEM_EQ(back, big, sizeof(big));
    }
#if !PF_OS_WINDOWS
    {
        pf_stat_t st;
        PF_ASSERT_ALWAYS_EQ(pf_stat(b, &st), 0);
        PF_ASSERT_ALWAYS_EQ(st.perm, 0640u);
    }
#endif

    test_write(c, "small");
    PF_ASSERT_ALWAYS_EQ(pf_copy_file(c, b, 0), PF_EEXIST);
    PF_ASSERT_ALWAYS_EQ(pf_copy_file(c, b, PF_COPY_OVERWRITE), 0);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(b), "small");
    PF_ASSERT_ALWAYS_EQ(pf_copy_file(test_path(3, dir, "none"), b, 0), PF_ENOENT);
    PF_ASSERT_ALWAYS(pf_copy_file(dir, test_path(3, dir, "d"), 0) < 0);
#if !PF_OS_WINDOWS
    /* onto itself */
    PF_ASSERT_ALWAYS_EQ(pf_copy_file(c, c, PF_COPY_OVERWRITE), PF_EINVAL);
    PF_ASSERT_ALWAYS_STR_EQ(test_read(c), "small");
#endif
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_lock(int seed, int i) {
    char dir[PF_PATH_MAX];
    const char *f;
    int a, b;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    f = test_path(0, dir, "lock");
    a = pf_open(f, PF_O_RDWR | PF_O_CREAT, 0600);
    b = pf_open(f, PF_O_RDWR, 0);
    PF_ASSERT_ALWAYS(a >= 0 && b >= 0);

    PF_ASSERT_ALWAYS_EQ(pf_lock_file(a, PF_LOCK_EX), 0);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(b, PF_LOCK_EX | PF_LOCK_NB), PF_EWOULDBLOCK);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(b, PF_LOCK_SH | PF_LOCK_NB), PF_EWOULDBLOCK);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(a, PF_LOCK_UN), 0);

    PF_ASSERT_ALWAYS_EQ(pf_lock_file(a, PF_LOCK_SH), 0);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(b, PF_LOCK_SH | PF_LOCK_NB), 0);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(a, PF_LOCK_UN), 0);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(b, PF_LOCK_UN), 0);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(b, PF_LOCK_EX | PF_LOCK_NB), 0);
    PF_ASSERT_ALWAYS_EQ(pf_lock_file(b, PF_LOCK_UN), 0);

    PF_ASSERT_ALWAYS_EQ(pf_lock_file(a, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_close(a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_close(b), 0);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

/* ---- Known directories ------------------------------------------------ */

static int test_known(int seed, int i) {
    char buf[PF_PATH_MAX], home[PF_PATH_MAX];
    ptrdiff_t n;
    int kind;
    (void)seed, (void)i;

    n = pf_known_dir(PF_DIR_HOME, home, sizeof(home));
    PF_ASSERT_ALWAYS(n > 0);
    PF_ASSERT_ALWAYS_EQ((size_t)n, strlen(home));
    PF_ASSERT_ALWAYS(pf_path_is_absolute(home));
    PF_ASSERT_ALWAYS_EQ(pf_known_dir(PF_DIR_HOME, NULL, 0), n);
    PF_ASSERT_ALWAYS_EQ(pf_known_dir(PF_DIR_HOME, buf, (size_t)n), PF_ERANGE);
    PF_ASSERT_ALWAYS_EQ(pf_home_dir_of(NULL, buf, sizeof(buf)), n);
    PF_ASSERT_ALWAYS_STR_EQ(buf, home);

    for (kind = PF_DIR_HOME; kind <= PF_DIR_PROJECTS; kind++) {
        n = pf_known_dir((pf_known_dir_t)kind, buf, sizeof(buf));
#if PF_OS_APPLE
        if (kind == PF_DIR_TEMPLATES) {
            PF_ASSERT_ALWAYS_EQ(n, PF_ENOENT);
            continue;
        }
#endif
        PF_ASSERT_ALWAYS_MSG(n > 0, "kind %d: %d", kind, (int)n);
        PF_ASSERT_ALWAYS_MSG(pf_path_is_absolute(buf), "kind %d: %s", kind, buf);
        PF_ASSERT_ALWAYS(n == 1 || !pf_path_is_sep(buf[n - 1]));
    }
    PF_ASSERT_ALWAYS(pf_is_dir(home));
    n = pf_known_dir(PF_DIR_TEMP, buf, sizeof(buf));
    PF_ASSERT_ALWAYS(pf_is_dir(buf));
    PF_ASSERT_ALWAYS_EQ(pf_known_dir((pf_known_dir_t)99, buf, sizeof(buf)), PF_EINVAL);
    return 0;
}

#if !PF_OS_WINDOWS && !PF_OS_APPLE

/* Sets an environment variable for one test, saving the old value. */
typedef struct {
    const char *name;
    char *old;
} saved_env;

static void env_set(saved_env *s, const char *name, const char *value) {
    const char *old = getenv(name);
    s->name = name;
    s->old = old ? pf_strdup(old) : NULL;
    if (value)
        PF_ASSERT_ALWAYS_EQ(pf_setenv(name, value, 1), 0);
    else
        PF_ASSERT_ALWAYS_EQ(pf_unsetenv(name), 0);
}

static void env_restore(saved_env *s) {
    if (s->old)
        pf_setenv(s->name, s->old, 1);
    else
        pf_unsetenv(s->name);
    free(s->old);
}

static int test_xdg(int seed, int i) {
    char dir[PF_PATH_MAX], buf[PF_PATH_MAX], home[PF_PATH_MAX];
    saved_env conf, data, run, tmp, hm;
    FILE *f;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    env_set(&hm, "HOME", "/home/someone/");
    env_set(&conf, "XDG_CONFIG_HOME", dir);
    env_set(&data, "XDG_DATA_HOME", "relative/is/ignored");
    env_set(&run, "XDG_RUNTIME_DIR", "/run/test");
    env_set(&tmp, "TMPDIR", "/var/tmp/");

    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_HOME, home, sizeof(home)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(home, "/home/someone");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_CONFIG, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, dir);
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_DATA, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/home/someone/.local/share");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_RUNTIME, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/run/test");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_TEMP, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/var/tmp");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_MUSIC, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/home/someone/Music");

    f = pf_fopen(test_path(0, dir, "user-dirs.dirs"), "w");
    PF_ASSERT_ALWAYS(f != NULL);
    fputs("# comment\n"
          "XDG_DESKTOP_DIR=\"$HOME/Schreibtisch\"\n"
          "  XDG_MUSIC_DIR = \"/srv/music\"\n"
          "XDG_VIDEOS_DIR=\"$HOME/\"\n",
          f);
    fclose(f);
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_DESKTOP, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/home/someone/Schreibtisch");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_MUSIC, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/srv/music");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_VIDEOS, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/home/someone");
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_DOWNLOAD, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "/home/someone/Downloads");

    env_restore(&tmp);
    env_restore(&run);
    env_restore(&data);
    env_restore(&conf);
    env_restore(&hm);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_home_of(int seed, int i) {
    char buf[PF_PATH_MAX];
    struct passwd *pw = getpwuid(getuid());
    (void)seed, (void)i;

    if (!pw)
        return SKIP("no passwd entry");
    PF_ASSERT_ALWAYS(pf_home_dir_of(pw->pw_name, buf, sizeof(buf)) > 0);
    PF_ASSERT_ALWAYS_EQ(strncmp(buf, pw->pw_dir, strlen(buf)), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf_home_dir_of("no-such-user-cpolyfill", buf, sizeof(buf)), PF_ENOENT
    );
    return 0;
}

#endif

pf_test_t suite_fs[] = {
    { test_root, "root" },
    { test_join, "join" },
    { test_normalize, "normalize" },
    { test_parts, "parts" },
    { test_basic, "basic" },
    { test_fstat, "fstat" },
    { test_links, "links" },
    { test_cwd, "cwd" },
    { test_temp, "temp" },
    { test_trees, "trees" },
    { test_copy, "copy" },
    { test_lock, "lock" },
    { test_known, "known" },
#if !PF_OS_WINDOWS && !PF_OS_APPLE
    { test_xdg, "xdg" },
    { test_home_of, "home_of" },
#endif
    { 0 },
};
