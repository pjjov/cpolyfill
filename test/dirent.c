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

#include "../include/pf_dirent.h"
#include "check.h"
#include "util.h"
#include <errno.h>

/* A directory holding the files a.txt, b.txt and the directory sub. */
struct fixture {
    char dir[TEST_PATH_MAX];
    char a[TEST_PATH_MAX];
    char b[TEST_PATH_MAX];
    char sub[TEST_PATH_MAX];
};

static int fixture_make(struct fixture *f) {
    test_path(f->dir, sizeof f->dir, "dirent");
    test_join(f->a, sizeof f->a, f->dir, "a.txt");
    test_join(f->b, sizeof f->b, f->dir, "b.txt");
    test_join(f->sub, sizeof f->sub, f->dir, "sub");

    if (test_mkdir(f->dir) != 0)
        return -1;
    if (test_write(f->a, "A", 1) != 0 || test_write(f->b, "BB", 2) != 0)
        return -1;
    return test_mkdir(f->sub);
}

static void fixture_free(struct fixture *f) {
    test_unlink(f->a);
    test_unlink(f->b);
    test_rmdir(f->sub);
    test_rmdir(f->dir);
}

/* Collects bit 0 for a.txt, bit 1 for b.txt, bit 2 for sub, bit 3 for
   anything unexpected, bit 4 for ".", bit 5 for "..". */
static int classify(const char *name) {
    if (!strcmp(name, "a.txt"))
        return 1;
    if (!strcmp(name, "b.txt"))
        return 2;
    if (!strcmp(name, "sub"))
        return 4;
    if (!strcmp(name, "."))
        return 16;
    if (!strcmp(name, ".."))
        return 32;
    return 8;
}

static int test_dirent_read(int seed, int rep) {
    struct fixture f;
    DIR *dir;
    struct dirent *e;
    int seen = 0, count = 0;

    CHECK(fixture_make(&f) == 0);

    dir = opendir(f.dir);
    CHECK(dir != NULL);
    while ((e = readdir(dir)) != NULL) {
        seen |= classify(e->d_name);
        count++;
    }
    CHECK(closedir(dir) == 0);
    fixture_free(&f);

    CHECK((seen & 7) == 7);
    CHECK((seen & 8) == 0);
    CHECK(count >= 3 && count <= 5); /* with or without "." and ".." */
    return 0;
}

static int test_dirent_trailing_slash(int seed, int rep) {
    struct fixture f;
    char path[TEST_PATH_MAX + 2];
    DIR *dir;
    int count = 0;

    CHECK(fixture_make(&f) == 0);
    snprintf(path, sizeof path, "%s%c", f.dir, TEST_SEP);

    dir = opendir(path);
    CHECK(dir != NULL);
    while (readdir(dir))
        count++;
    CHECK(closedir(dir) == 0);
    CHECK(count >= 3);

    fixture_free(&f);
    return 0;
}

static int test_dirent_forward_slashes(int seed, int rep) {
    struct fixture f;
    char path[TEST_PATH_MAX];
    DIR *dir;
    size_t i;

    CHECK(fixture_make(&f) == 0);
    snprintf(path, sizeof path, "%s", f.dir);
    for (i = 0; path[i]; i++)
        if (path[i] == '\\')
            path[i] = '/';

    dir = opendir(path);
    CHECK(dir != NULL);
    CHECK(closedir(dir) == 0);

    fixture_free(&f);
    return 0;
}

static int test_dirent_missing(int seed, int rep) {
    struct fixture f;
    char path[TEST_PATH_MAX];

    test_path(path, sizeof path, "no-such-dir");
    errno = 0;
    CHECK(opendir(path) == NULL);
    CHECK(errno != 0);

    /* A file is not a directory. */
    CHECK(fixture_make(&f) == 0);
    CHECK(opendir(f.a) == NULL);
    fixture_free(&f);
    return 0;
}

static int test_dirent_rewind(int seed, int rep) {
    struct fixture f;
    DIR *dir;
    int first = 0, second = 0;

    CHECK(fixture_make(&f) == 0);
    dir = opendir(f.dir);
    CHECK(dir != NULL);

    while (readdir(dir))
        first++;
    CHECK(readdir(dir) == NULL);

    rewinddir(dir);
    while (readdir(dir))
        second++;
    CHECK(first == second);
    CHECK(first >= 3);

    CHECK(closedir(dir) == 0);
    fixture_free(&f);
    return 0;
}

static int test_dirent_seek_tell(int seed, int rep) {
    struct fixture f;
    DIR *dir;
    struct dirent *e;
    long pos;
    char expected[512];

    CHECK(fixture_make(&f) == 0);
    dir = opendir(f.dir);
    CHECK(dir != NULL);

    CHECK(readdir(dir) != NULL);
    pos = telldir(dir);
    e = readdir(dir);
    CHECK(e != NULL);
    snprintf(expected, sizeof expected, "%s", e->d_name);
    CHECK(readdir(dir) != NULL);

    seekdir(dir, pos);
    e = readdir(dir);
    CHECK(e != NULL);
    CHECK_STR(e->d_name, expected);

    CHECK(closedir(dir) == 0);
    fixture_free(&f);
    return 0;
}

struct collect {
    int seen;
    int calls;
    int stop_after;
};

static int on_entry(const struct dirent *entry, void *user) {
    struct collect *c = user;
    c->seen |= classify(entry->d_name);
    c->calls++;
    return c->stop_after > 0 && c->calls >= c->stop_after ? 7 : 0;
}

static int test_dirent_iterdir(int seed, int rep) {
    struct fixture f;
    struct collect c = { 0, 0, 0 };

    CHECK(fixture_make(&f) == 0);
    CHECK(pf_iterdir(f.dir, on_entry, &c) == 0);
    CHECK((c.seen & 7) == 7);
    CHECK((c.seen & 8) == 0);
    CHECK(c.calls >= 3);
    fixture_free(&f);
    return 0;
}

static int test_dirent_iterdir_stop(int seed, int rep) {
    struct fixture f;
    struct collect c = { 0, 0, 2 };

    CHECK(fixture_make(&f) == 0);
    /* A non-zero result stops the iteration and is passed on. */
    CHECK(pf_iterdir(f.dir, on_entry, &c) == 7);
    CHECK(c.calls == 2);
    fixture_free(&f);
    return 0;
}

static int test_dirent_iterdir_errors(int seed, int rep) {
    struct collect c = { 0, 0, 0 };
    char path[TEST_PATH_MAX];

    test_path(path, sizeof path, "no-such-dir");
    CHECK(pf_iterdir(path, on_entry, &c) == -1);
    CHECK(c.calls == 0);
    CHECK(pf_iterdir(path, NULL, &c) == -1);
    return 0;
}

pf_test_t suite_dirent[] = {
    { test_dirent_read, "/dirent/read" },
    { test_dirent_trailing_slash, "/dirent/trailing_slash" },
    { test_dirent_forward_slashes, "/dirent/forward_slashes" },
    { test_dirent_missing, "/dirent/missing" },
    { test_dirent_rewind, "/dirent/rewind" },
    { test_dirent_seek_tell, "/dirent/seek_tell" },
    { test_dirent_iterdir, "/dirent/iterdir" },
    { test_dirent_iterdir_stop, "/dirent/iterdir_stop" },
    { test_dirent_iterdir_errors, "/dirent/iterdir_errors" },
    { 0 },
};
