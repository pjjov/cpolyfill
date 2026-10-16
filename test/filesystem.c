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

/* Before C11, pf_filesystem.h relies on pf_assert.h for `static_assert`. */
#include "check.h"
#include "../include/pf_filesystem.h"
#include "util.h"
#include <errno.h>

/* The error values are declared negative. Some paths of the header return
   them negated, which makes them positive, so the individual tests accept
   either sign and `error_sign` checks the convention itself. */
#define IS_ERR(result, code) ((result) == (code) || (result) == -(code))

/* ---------------------------------------------------------------- */
/* Queries                                                           */
/* ---------------------------------------------------------------- */

static int test_fs_isdir_isfile_exists(int seed, int rep) {
    char dir[TEST_PATH_MAX], file[TEST_PATH_MAX], none[TEST_PATH_MAX];

    test_path(dir, sizeof dir, "fs-dir");
    test_join(file, sizeof file, dir, "file.txt");
    test_join(none, sizeof none, dir, "none");

    CHECK(!pf_exists(dir) && !pf_isdir(dir) && !pf_isfile(dir));

    CHECK(test_mkdir(dir) == 0);
    CHECK(pf_exists(dir));
    CHECK(pf_isdir(dir));
    CHECK(!pf_isfile(dir));

    CHECK(test_write(file, "x", 1) == 0);
    CHECK(pf_exists(file));
    CHECK(pf_isfile(file));
    CHECK(!pf_isdir(file));

    CHECK(!pf_exists(none));
    CHECK(!pf_isfile(none));
    CHECK(!pf_isdir(none));

    test_unlink(file);
    CHECK(!pf_exists(file));
    test_rmdir(dir);
    CHECK(!pf_exists(dir));
    return 0;
}

static int test_fs_null(int seed, int rep) {
    CHECK(!pf_exists(NULL));
    CHECK(!pf_isdir(NULL));
    CHECK(!pf_isfile(NULL));
    CHECK(!pf_canread(NULL));
    CHECK(!pf_canwrite(NULL));
    CHECK(!pf_canexecute(NULL));
    CHECK(pf_filesize(NULL) == -1);
    CHECK(pf_filesizef(NULL) == -1);
    return 0;
}

static int test_fs_current_directory(int seed, int rep) {
    CHECK(pf_isdir("."));
    CHECK(pf_exists("."));
    CHECK(!pf_isfile("."));
    CHECK(pf_isdir(test_tmpdir()));
    return 0;
}

static int test_fs_permissions(int seed, int rep) {
    char file[TEST_PATH_MAX], none[TEST_PATH_MAX];

    test_path(file, sizeof file, "fs-perm");
    test_path(none, sizeof none, "fs-perm-none");
    CHECK(test_write(file, "x", 1) == 0);

    CHECK(pf_canread(file));
    CHECK(pf_canwrite(file));
    CHECK(!pf_canread(none));
    CHECK(!pf_canwrite(none));
    CHECK(!pf_canexecute(none));

    /* A fresh data file is not executable (Windows goes by extension). */
    CHECK(!pf_canexecute(file));

    test_unlink(file);
    return 0;
}

#ifndef _WIN32
static int test_fs_permission_bits(int seed, int rep) {
    char file[TEST_PATH_MAX];

    test_path(file, sizeof file, "fs-bits");
    CHECK(test_write(file, "x", 1) == 0);

    CHECK(chmod(file, 0700) == 0);
    CHECK(pf_canread(file) && pf_canwrite(file) && pf_canexecute(file));

    CHECK(chmod(file, 0400) == 0);
    if (geteuid() == 0) {
        /* The superuser ignores the permission bits. */
        test_unlink(file);
        return SKIP("running as root");
    }
    CHECK(pf_canread(file));
    CHECK(!pf_canwrite(file));
    CHECK(!pf_canexecute(file));

    CHECK(chmod(file, 0000) == 0);
    CHECK(!pf_canread(file));
    CHECK(pf_exists(file)); /* existence does not need permissions */

    chmod(file, 0600);
    test_unlink(file);
    return 0;
}
#else
static int test_fs_executable_extensions(int seed, int rep) {
    char dir[TEST_PATH_MAX], path[TEST_PATH_MAX];
    static const char *const yes[] = { "a.exe", "a.EXE", "a.com",
                                       "a.bat", "a.cmd", 0 };
    static const char *const no[] = { "a.txt", "a", "a.exe.txt", 0 };
    int i;

    test_path(dir, sizeof dir, "fs-exec");
    CHECK(test_mkdir(dir) == 0);

    for (i = 0; yes[i]; i++) {
        test_join(path, sizeof path, dir, yes[i]);
        CHECK(test_write(path, "", 0) == 0);
        CHECK(pf_canexecute(path));
        test_unlink(path);
    }
    for (i = 0; no[i]; i++) {
        test_join(path, sizeof path, dir, no[i]);
        CHECK(test_write(path, "", 0) == 0);
        CHECK(!pf_canexecute(path));
        test_unlink(path);
    }

    /* Directories are never executable. */
    CHECK(!pf_canexecute(dir));
    test_rmdir(dir);
    return 0;
}
#endif

/* ---------------------------------------------------------------- */
/* File size                                                         */
/* ---------------------------------------------------------------- */

static int test_fs_filesize(int seed, int rep) {
    char path[TEST_PATH_MAX], data[5000];
    FILE *f;

    test_path(path, sizeof path, "fs-size");
    memset(data, 'x', sizeof data);

    CHECK(pf_filesize(path) == -1); /* missing */

    CHECK(test_write(path, "", 0) == 0);
    CHECK(pf_filesize(path) == 0);

    CHECK(test_write(path, data, 1) == 0);
    CHECK(pf_filesize(path) == 1);

    CHECK(test_write(path, data, sizeof data) == 0);
    CHECK(pf_filesize(path) == (int64_t)sizeof data);

    f = fopen(path, "rb");
    CHECK(f != NULL);
    CHECK(pf_filesizef(f) == (int64_t)sizeof data);
    fclose(f);

    test_unlink(path);
    return 0;
}

static int test_fs_filesizef_position(int seed, int rep) {
    char path[TEST_PATH_MAX], data[100], buf[10];
    FILE *f;

    test_path(path, sizeof path, "fs-pos");
    memset(data, 'x', sizeof data);
    CHECK(test_write(path, data, sizeof data) == 0);

    f = fopen(path, "rb");
    CHECK(f != NULL);
    CHECK(fread(buf, 1, sizeof buf, f) == sizeof buf);

    /* Measuring must not move the position. */
    CHECK(pf_filesizef(f) == 100);
    CHECK(ftell(f) == 10);
    CHECK(pf__filesize(f) == 100);
    CHECK(ftell(f) == 10);
    CHECK(fread(buf, 1, sizeof buf, f) == sizeof buf);

    fclose(f);
    test_unlink(path);
    return 0;
}

static int test_fs_filesize_tmpfile(int seed, int rep) {
    FILE *f = tmpfile();

    CHECK(f != NULL);
    CHECK(pf_filesizef(f) == 0);
    CHECK(fwrite("12345", 1, 5, f) == 5);
    fflush(f);
    CHECK(pf_filesizef(f) == 5);
    fclose(f);
    return 0;
}

/* ---------------------------------------------------------------- */
/* mkdir, rmdir, chdir, getcwd                                       */
/* ---------------------------------------------------------------- */

static int test_fs_mkdir_rmdir(int seed, int rep) {
    char dir[TEST_PATH_MAX], file[TEST_PATH_MAX];

    test_path(dir, sizeof dir, "fs-mk");
    test_join(file, sizeof file, dir, "f");

    CHECK(mkdir(dir, 0755) == 0);
    CHECK(pf_isdir(dir));

    errno = 0;
    CHECK(mkdir(dir, 0755) == -1); /* exists */
    CHECK(errno != 0);

    CHECK(test_write(file, "x", 1) == 0);
    CHECK(rmdir(dir) == -1); /* not empty */
    test_unlink(file);

    CHECK(rmdir(dir) == 0);
    CHECK(!pf_exists(dir));
    CHECK(rmdir(dir) == -1); /* gone */
    return 0;
}

static int test_fs_mkdir_missing_parent(int seed, int rep) {
    char parent[TEST_PATH_MAX], child[TEST_PATH_MAX];

    test_path(parent, sizeof parent, "fs-parent");
    test_join(child, sizeof child, parent, "child");
    CHECK(mkdir(child, 0755) == -1);
    CHECK(!pf_exists(child));
    return 0;
}

static int test_fs_chdir_getcwd(int seed, int rep) {
    char original[PF_PATH_MAX], now[PF_PATH_MAX], dir[TEST_PATH_MAX];

    CHECK(getcwd(original, sizeof original) != NULL);
    CHECK(original[0] != '\0');
    CHECK(pf_isdir(original));

    test_path(dir, sizeof dir, "fs-cd");
    CHECK(mkdir(dir, 0755) == 0);
    CHECK(chdir(dir) == 0);

    CHECK(getcwd(now, sizeof now) != NULL);
    CHECK(strcmp(now, original) != 0);
    /* The new directory ends with the leaf of the path we changed to. */
    CHECK(strstr(now, strrchr(dir, TEST_SEP) + 1) != NULL);

    CHECK(chdir(original) == 0);
    CHECK(getcwd(now, sizeof now) != NULL);
    CHECK_STR(now, original);

    CHECK(rmdir(dir) == 0);

    errno = 0;
    CHECK(chdir(dir) == -1);
    CHECK(errno != 0);
    return 0;
}

static int test_fs_getcwd_small_buffer(int seed, int rep) {
    char tiny[2];

    errno = 0;
    CHECK(getcwd(tiny, sizeof tiny) == NULL);
    CHECK(errno != 0);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Special directories                                               */
/* ---------------------------------------------------------------- */

static int absolute(const char *path) {
#ifdef _WIN32
    return path[0] != '\0' && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

static int test_fs_dirs_exist(int seed, int rep) {
    char buf[PF_PATH_MAX];
    int (*const funcs[])(char *, size_t) = {
        pf_datadir, pf_confdir, pf_statedir, pf_cachedir, pf_runtimedir
    };
    size_t i;

    CHECK(pf_homedir(NULL, buf, sizeof buf) > 0);
    CHECK(absolute(buf));

    for (i = 0; i < sizeof funcs / sizeof funcs[0]; i++) {
        int n = funcs[i](buf, sizeof buf);
        CHECK(n > 0);
        CHECK((size_t)n == strlen(buf));
        CHECK(absolute(buf));
    }
    return 0;
}

static int test_fs_userdirs(int seed, int rep) {
    char buf[PF_PATH_MAX];
    int kind;

    for (kind = 0; kind < PF__USERDIR_COUNT; kind++) {
        int n = pf_userdir(kind, buf, sizeof buf);
        CHECK(n > 0);
        CHECK((size_t)n == strlen(buf));
        CHECK(absolute(buf));
    }

    CHECK(IS_ERR(pf_userdir(-1, buf, sizeof buf), PF_FS_EINVAL));
    CHECK(IS_ERR(pf_userdir(PF__USERDIR_COUNT, buf, sizeof buf), PF_FS_EINVAL));
    CHECK(IS_ERR(pf_userdir(1000, buf, sizeof buf), PF_FS_EINVAL));
    return 0;
}

#ifndef _WIN32

static int test_fs_homedir_env(int seed, int rep) {
    char buf[PF_PATH_MAX];

    CHECK(test_setenv("HOME", "/pf/home") == 0);
    CHECK(pf_homedir(NULL, buf, sizeof buf) == 8);
    CHECK_STR(buf, "/pf/home");
    CHECK(pf_homedir("", buf, sizeof buf) == 8);

    /* Exactly fitting, including the terminator. */
    CHECK(pf_homedir(NULL, buf, 9) == 8);
    return 0;
}

static int test_fs_homedir_small_buffer(int seed, int rep) {
    char buf[4];

    CHECK(test_setenv("HOME", "/pf/home") == 0);
    /* Errors are negative, as the pf_filesystem_error values are. */
    CHECK(IS_ERR(pf_homedir(NULL, buf, sizeof buf), PF_FS_ERANGE));
    CHECK(IS_ERR(pf_homedir(NULL, buf, 8), PF_FS_ERANGE));
    CHECK(IS_ERR(pf_homedir(NULL, buf, 0), PF_FS_ERANGE));
    return 0;
}

static int test_fs_homedir_passwd(int seed, int rep) {
    char buf[PF_PATH_MAX];
    struct passwd *pw;

    pw = getpwuid(getuid());
    if (!pw || !pw->pw_dir)
        return SKIP("no passwd entry");

    /* Without HOME the passwd database is used. */
    CHECK(test_setenv("HOME", NULL) == 0);
    CHECK(pf_homedir(NULL, buf, sizeof buf) == (int)strlen(pw->pw_dir));
    CHECK_STR(buf, pw->pw_dir);

    CHECK(test_setenv("HOME", "") == 0);
    CHECK_STR((pf_homedir(NULL, buf, sizeof buf), buf), pw->pw_dir);

    /* By name. */
    CHECK(pf_homedir(pw->pw_name, buf, sizeof buf) == (int)strlen(pw->pw_dir));
    CHECK_STR(buf, pw->pw_dir);

    CHECK(
        IS_ERR(pf_homedir("pf-no-such-user-xyz", buf, sizeof buf), PF_FS_ENOENT)
    );
    return 0;
}

static int test_fs_xdg_directories(int seed, int rep) {
    char buf[PF_PATH_MAX];

    CHECK(test_setenv("HOME", "/pf/home") == 0);

    /* Defaults relative to the home directory. */
    CHECK(test_setenv("XDG_DATA_HOME", NULL) == 0);
    CHECK(test_setenv("XDG_CONFIG_HOME", NULL) == 0);
    CHECK(test_setenv("XDG_STATE_HOME", NULL) == 0);
    CHECK(test_setenv("XDG_CACHE_HOME", NULL) == 0);
    CHECK(pf_datadir(buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/.local/share");
    CHECK(pf_confdir(buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/.config");
    CHECK(pf_statedir(buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/.local/state");
    CHECK(pf_cachedir(buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/.cache");

    /* Absolute overrides win. */
    CHECK(test_setenv("XDG_DATA_HOME", "/x/data") == 0);
    CHECK(test_setenv("XDG_CONFIG_HOME", "/x/config") == 0);
    CHECK(test_setenv("XDG_STATE_HOME", "/x/state") == 0);
    CHECK(test_setenv("XDG_CACHE_HOME", "/x/cache") == 0);
    CHECK(pf_datadir(buf, sizeof buf) == 7);
    CHECK_STR(buf, "/x/data");
    CHECK(pf_confdir(buf, sizeof buf) == 9);
    CHECK_STR(buf, "/x/config");
    CHECK(pf_statedir(buf, sizeof buf) == 8);
    CHECK_STR(buf, "/x/state");
    CHECK(pf_cachedir(buf, sizeof buf) == 8);
    CHECK_STR(buf, "/x/cache");

    /* The specification says to ignore relative paths. */
    CHECK(test_setenv("XDG_DATA_HOME", "relative/data") == 0);
    CHECK(test_setenv("XDG_CONFIG_HOME", "") == 0);
    CHECK(pf_datadir(buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/.local/share");
    CHECK(pf_confdir(buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/.config");
    return 0;
}

static int test_fs_runtimedir(int seed, int rep) {
    char buf[PF_PATH_MAX];

    CHECK(test_setenv("XDG_RUNTIME_DIR", "/run/pf") == 0);
    CHECK(pf_runtimedir(buf, sizeof buf) == 7);
    CHECK_STR(buf, "/run/pf");

    /* Without the variable a usable directory is still returned. */
    CHECK(test_setenv("XDG_RUNTIME_DIR", NULL) == 0);
    CHECK(pf_runtimedir(buf, sizeof buf) > 0);
    CHECK(pf_isdir(buf));

    CHECK(test_setenv("XDG_RUNTIME_DIR", "relative") == 0);
    CHECK(pf_runtimedir(buf, sizeof buf) > 0);
    CHECK(buf[0] == '/');
    CHECK(strcmp(buf, "relative") != 0);
    return 0;
}

static int test_fs_xdg_small_buffer(int seed, int rep) {
    char buf[3];

    CHECK(test_setenv("HOME", "/pf/home") == 0);
    CHECK(test_setenv("XDG_DATA_HOME", "/x/data") == 0);
    CHECK(IS_ERR(pf_datadir(buf, sizeof buf), PF_FS_ERANGE));
    CHECK(test_setenv("XDG_DATA_HOME", NULL) == 0);
    CHECK(IS_ERR(pf_datadir(buf, sizeof buf), PF_FS_ERANGE));
    return 0;
}

/* Sets HOME and XDG_CONFIG_HOME to a scratch tree holding `content` as the
   user-dirs.dirs file, and returns the directory to remove afterwards. */
static int userdirs_fixture(char *root, char *conf, const char *content) {
    char file[TEST_PATH_MAX];

    test_path(root, TEST_PATH_MAX, "fs-user");
    test_join(conf, TEST_PATH_MAX, root, "conf");
    test_join(file, sizeof file, conf, "user-dirs.dirs");
    if (test_mkdir(root) || test_mkdir(conf))
        return -1;
    if (test_write(file, content, strlen(content)))
        return -1;
    if (test_setenv("HOME", "/pf/home") || test_setenv("XDG_CONFIG_HOME", conf))
        return -1;
    return 0;
}

static void userdirs_cleanup(const char *root, const char *conf) {
    char file[TEST_PATH_MAX];
    test_join(file, sizeof file, conf, "user-dirs.dirs");
    test_unlink(file);
    test_rmdir(conf);
    test_rmdir(root);
}

static int test_fs_userdirs_config(int seed, int rep) {
    char root[TEST_PATH_MAX], conf[TEST_PATH_MAX], buf[PF_PATH_MAX];

    CHECK(
        userdirs_fixture(
            root,
            conf,
            "# comment\n"
            "XDG_DESKTOP_DIR=\"$HOME/Skrivbord\"\n"
            "  XDG_DOWNLOAD_DIR = \"/mnt/downloads\"\n"
            "XDG_MUSIC_DIR=\"$HOME/\"\n"
        )
        == 0
    );

    CHECK(pf_userdir(PF_USERDIR_DESKTOP, buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/Skrivbord");
    CHECK(pf_userdir(PF_USERDIR_DOWNLOAD, buf, sizeof buf) == 14);
    CHECK_STR(buf, "/mnt/downloads");

    /* `$HOME/` alone is the home directory (xdg-user-dirs uses it to
       disable a directory), with or without the separator. */
    CHECK(pf_userdir(PF_USERDIR_MUSIC, buf, sizeof buf) > 0);
    CHECK(strcmp(buf, "/pf/home") == 0 || strcmp(buf, "/pf/home/") == 0);

    userdirs_cleanup(root, conf);
    return 0;
}

/* Keys that are missing, or lines that are malformed, use the defaults. */
static int test_fs_userdirs_fallback(int seed, int rep) {
    char root[TEST_PATH_MAX], conf[TEST_PATH_MAX], buf[PF_PATH_MAX];

    CHECK(userdirs_fixture(root, conf, "XDG_VIDEOS_DIR=$HOME/NoQuotes\n") == 0);

    CHECK(pf_userdir(PF_USERDIR_DOCUMENTS, buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/Documents");
    CHECK(pf_userdir(PF_USERDIR_VIDEOS, buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/Videos");

    /* Projects has no XDG entry. */
    CHECK(pf_userdir(PF_USERDIR_PROJECTS, buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/Projects");

    userdirs_cleanup(root, conf);
    return 0;
}

static int test_fs_userdirs_defaults(int seed, int rep) {
    static const char *const leaf[] = { "Desktop", "Documents", "Downloads",
                                        "Music",   "Pictures",  "Projects",
                                        "Public",  "Templates", "Videos" };
    char root[TEST_PATH_MAX], conf[TEST_PATH_MAX], buf[PF_PATH_MAX];
    char want[PF_PATH_MAX];
    int kind;

    /* An empty configuration gives the English defaults. */
    CHECK(userdirs_fixture(root, conf, "") == 0);
    for (kind = 0; kind < PF__USERDIR_COUNT; kind++) {
        CHECK(pf_userdir(kind, buf, sizeof buf) > 0);
        snprintf(want, sizeof want, "/pf/home/%s", leaf[kind]);
        CHECK_STR(buf, want);
    }
    userdirs_cleanup(root, conf);

    /* So does a missing file. */
    CHECK(test_setenv("XDG_CONFIG_HOME", "/pf/no/such/config") == 0);
    CHECK(pf_userdir(PF_USERDIR_PICTURES, buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/Pictures");
    return 0;
}

static int test_fs_parse_user_dirs(int seed, int rep) {
    char root[TEST_PATH_MAX], conf[TEST_PATH_MAX], buf[PF_PATH_MAX];

    CHECK(
        userdirs_fixture(
            root, conf, "XDG_A_DIR=\"$HOME/a\"\nXDG_AB_DIR=\"/ab\"\n"
        )
        == 0
    );

    CHECK(pf_parse_user_dirs_dirs("XDG_A_DIR", buf, sizeof buf) > 0);
    CHECK_STR(buf, "/pf/home/a");
    CHECK(pf_parse_user_dirs_dirs("XDG_AB_DIR", buf, sizeof buf) > 0);
    CHECK_STR(buf, "/ab");
    CHECK(IS_ERR(
        pf_parse_user_dirs_dirs("XDG_NONE_DIR", buf, sizeof buf), PF_FS_ENOENT
    ));

    userdirs_cleanup(root, conf);
    return 0;
}

static int test_fs_userdirs_small_buffer(int seed, int rep) {
    char root[TEST_PATH_MAX], conf[TEST_PATH_MAX], buf[4];

    CHECK(
        userdirs_fixture(root, conf, "XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n") == 0
    );
    CHECK(
        IS_ERR(pf_userdir(PF_USERDIR_DESKTOP, buf, sizeof buf), PF_FS_ERANGE)
    );
    CHECK(
        IS_ERR(pf_userdir(PF_USERDIR_PROJECTS, buf, sizeof buf), PF_FS_ERANGE)
    );
    userdirs_cleanup(root, conf);
    return 0;
}

static int test_fs_copy_helpers(int seed, int rep) {
    char buf[8];

    CHECK(pf__fs_copy(buf, sizeof buf, "abc") == 3);
    CHECK_STR(buf, "abc");
    CHECK(pf__fs_copy(buf, sizeof buf, "1234567") == 7);
    CHECK(pf__fs_copy(buf, sizeof buf, "") == 0);
    CHECK_STR(buf, "");

    CHECK(pf__fs_copy_join(buf, sizeof buf, "ab", "cd") == 5);
    CHECK_STR(buf, "ab/cd");
    CHECK(pf__fs_copy_join(buf, sizeof buf, "ab/", "cd") == 5);
    CHECK_STR(buf, "ab/cd");
    CHECK(pf__fs_copy_join(buf, sizeof buf, "", "cd") == 2);
    CHECK_STR(buf, "cd");
    CHECK(pf__fs_copy_join(buf, sizeof buf, "abc", "de") == 6);
    return 0;
}

static int test_fs_copy_errors(int seed, int rep) {
    char buf[8];

    CHECK(IS_ERR(pf__fs_copy(buf, sizeof buf, "12345678"), PF_FS_ERANGE));
    CHECK(IS_ERR(pf__fs_copy(buf, sizeof buf, NULL), PF_FS_EINVAL));
    CHECK(IS_ERR(pf__fs_copy(NULL, 8, "abc"), PF_FS_EINVAL));
    CHECK(IS_ERR(pf__fs_copy_join(buf, 5, "abc", "def"), PF_FS_ERANGE));
    CHECK(IS_ERR(pf__fs_copy_join(NULL, 16, "abc", "def"), PF_FS_EINVAL));
    return 0;
}

/* Documented by the enum: errors are negative. */
static int test_fs_error_sign(int seed, int rep) {
    char buf[PF_PATH_MAX];

    CHECK(PF_FS_ENOENT < 0 && PF_FS_ERANGE < 0 && PF_FS_EINVAL < 0);
    CHECK(pf_userdir(-1, buf, sizeof buf) < 0);
    CHECK(pf_homedir("pf-no-such-user-xyz", buf, sizeof buf) < 0);
    CHECK(pf__fs_copy(buf, 2, "abc") < 0);
    CHECK(pf__fs_copy(buf, 8, NULL) < 0);
    return 0;
}

#endif /* !_WIN32 */

pf_test_t suite_filesystem[] = {
    { test_fs_isdir_isfile_exists, "/fs/isdir_isfile_exists" },
    { test_fs_null, "/fs/null" },
    { test_fs_current_directory, "/fs/current_directory" },
    { test_fs_permissions, "/fs/permissions" },
#ifndef _WIN32
    { test_fs_permission_bits, "/fs/permission_bits" },
#else
    { test_fs_executable_extensions, "/fs/executable_extensions" },
#endif
    { test_fs_filesize, "/fs/filesize" },
    { test_fs_filesizef_position, "/fs/filesizef_position" },
    { test_fs_filesize_tmpfile, "/fs/filesize_tmpfile" },
    { test_fs_mkdir_rmdir, "/fs/mkdir_rmdir" },
    { test_fs_mkdir_missing_parent, "/fs/mkdir_missing_parent" },
    { test_fs_chdir_getcwd, "/fs/chdir_getcwd" },
    { test_fs_getcwd_small_buffer, "/fs/getcwd_small_buffer" },
    { test_fs_dirs_exist, "/fs/dirs_exist" },
    { test_fs_userdirs, "/fs/userdirs" },
#ifndef _WIN32
    { test_fs_homedir_env, "/fs/homedir_env" },
    { test_fs_homedir_small_buffer, "/fs/homedir_small_buffer" },
    { test_fs_homedir_passwd, "/fs/homedir_passwd" },
    { test_fs_xdg_directories, "/fs/xdg_directories" },
    { test_fs_runtimedir, "/fs/runtimedir" },
    { test_fs_xdg_small_buffer, "/fs/xdg_small_buffer" },
    { test_fs_userdirs_config, "/fs/userdirs_config" },
    { test_fs_userdirs_fallback, "/fs/userdirs_fallback" },
    { test_fs_userdirs_defaults, "/fs/userdirs_defaults" },
    { test_fs_parse_user_dirs, "/fs/parse_user_dirs" },
    { test_fs_userdirs_small_buffer, "/fs/userdirs_small_buffer" },
    { test_fs_copy_helpers, "/fs/copy_helpers" },
    { test_fs_copy_errors, "/fs/copy_errors" },
    { test_fs_error_sign, "/fs/error_sign" },
#endif
    { 0 },
};
