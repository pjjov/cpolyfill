/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/dirent.h> */

#include "../src/internal.h"

#include <cpolyfill/dirent.h>
#include <cpolyfill/fs.h>

#include <errno.h>
#include <string.h>

#include "files.h"

/* Builds dir/{a (file), b/ (dir), b/c (file), b/d/ (dir), b/d/e (file)}. */
static void make_tree(const char *dir) {
    test_write(test_path(0, dir, "a"), "a");
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(test_path(0, dir, "b"), 0755), 0);
    test_write(test_path(0, dir, "b/c"), "c");
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(test_path(0, dir, "b/d"), 0755), 0);
    test_write(test_path(0, dir, "b/d/e"), "e");
}

static int test_read_dir(int seed, int i) {
    char dir[PF_PATH_MAX];
    struct pf_dirent *e;
    pf_dir_t *d;
    int seen_a = 0, seen_b = 0, seen_dot = 0, seen_dotdot = 0, n = 0;
    int pass;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    make_tree(dir);

    d = pf_opendir(dir);
    PF_ASSERT_ALWAYS(d != NULL);
    for (pass = 0; pass < 2; pass++) {
        seen_a = seen_b = seen_dot = seen_dotdot = n = 0;
        errno = 1234;
        while ((e = pf_readdir(d)) != NULL) {
            n++;
            if (strcmp(e->d_name, "a") == 0) {
                seen_a++;
                PF_ASSERT_ALWAYS_EQ(e->d_type, PF_DT_REG);
            } else if (strcmp(e->d_name, "b") == 0) {
                seen_b++;
                PF_ASSERT_ALWAYS_EQ(e->d_type, PF_DT_DIR);
            } else if (strcmp(e->d_name, ".") == 0) {
                seen_dot++;
                PF_ASSERT_ALWAYS_EQ(e->d_type, PF_DT_DIR);
            } else if (strcmp(e->d_name, "..") == 0) {
                seen_dotdot++;
            } else {
                PF_ASSERT_ALWAYS_MSG(0, "unexpected entry %s", e->d_name);
            }
        }
        PF_ASSERT_ALWAYS_EQ(errno, 1234); /* untouched at the end */
        PF_ASSERT_ALWAYS_EQ(seen_a, 1);
        PF_ASSERT_ALWAYS_EQ(seen_b, 1);
        PF_ASSERT_ALWAYS_EQ(seen_dot, 1);
        PF_ASSERT_ALWAYS_EQ(seen_dotdot, 1);
        PF_ASSERT_ALWAYS_EQ(n, 4);
        PF_ASSERT_ALWAYS(pf_readdir(d) == NULL);
        pf_rewinddir(d);
    }
    PF_ASSERT_ALWAYS_EQ(pf_closedir(d), 0);

    PF_ASSERT_ALWAYS(pf_opendir(test_path(0, dir, "missing")) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ENOENT);
    PF_ASSERT_ALWAYS(pf_opendir(test_path(0, dir, "a")) == NULL);
    PF_ASSERT_ALWAYS_EQ(errno, ENOTDIR);
    PF_ASSERT_ALWAYS(pf_opendir(NULL) == NULL);
    PF_ASSERT_ALWAYS_EQ(pf_closedir(NULL), -1);

    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_symlink_type(int seed, int i) {
    char dir[PF_PATH_MAX];
    struct pf_dirent *e;
    pf_dir_t *d;
    int found = 0;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    if (pf_symlink("target", test_path(0, dir, "link")) != 0) {
        pf_remove_all(dir);
        return SKIP("no permission to create symbolic links");
    }
    d = pf_opendir(dir);
    PF_ASSERT_ALWAYS(d != NULL);
    while ((e = pf_readdir(d)) != NULL) {
        if (strcmp(e->d_name, "link") == 0) {
            PF_ASSERT_ALWAYS_EQ(e->d_type, PF_DT_LNK);
            found = 1;
        }
    }
    PF_ASSERT_ALWAYS(found);
    PF_ASSERT_ALWAYS_EQ(pf_closedir(d), 0);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

/* The walk, recorded as "depth:name/" (directories) or "depth:name". */
typedef struct {
    char log[1024];
    size_t base;   /* the length of the walked path plus the separator */
    const char *stop_at;
    const char *skip;
} walk_log;

static int record(
    const char *path, const struct pf_dirent *e, int depth, void *user
) {
    walk_log *w = (walk_log *)user;
    char item[256];
    size_t k;

    snprintf(
        item, sizeof(item), "%d:%s%s ", depth, path + w->base,
        e->d_type == PF_DT_DIR ? "/" : ""
    );
    for (k = 0; item[k]; k++)
        if (item[k] == '\\')
            item[k] = '/';
    strcat(w->log, item);
    PF_ASSERT_ALWAYS_STR_EQ(e->d_name, pf_path_basename(path, NULL));
    if (w->stop_at && strcmp(e->d_name, w->stop_at) == 0)
        return 42;
    if (w->skip && strcmp(e->d_name, w->skip) == 0)
        return PF_WALK_SKIP;
    return 0;
}

/* Where `item` is in the log. readdir has no order, so the tests only
   compare the positions of entries whose order the walk fixes. */
static const char *pos(const walk_log *w, const char *item) {
    const char *p = strstr(w->log, item);
    PF_ASSERT_ALWAYS_MSG(p != NULL, "%s not in %s", item, w->log);
    return p;
}

static int test_walk(int seed, int i) {
    char dir[PF_PATH_MAX];
    walk_log w;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    make_tree(dir);

    /* preorder */
    memset(&w, 0, sizeof(w));
    w.base = strlen(dir) + 1;
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, 0, record, &w), 0);
    PF_ASSERT_ALWAYS(pos(&w, "1:b/ ") < pos(&w, "2:b/c "));
    PF_ASSERT_ALWAYS(pos(&w, "1:b/ ") < pos(&w, "2:b/d/ "));
    PF_ASSERT_ALWAYS(pos(&w, "2:b/d/ ") < pos(&w, "3:b/d/e "));
    pos(&w, "1:a ");
    PF_ASSERT_ALWAYS_EQ(strlen(w.log), strlen("1:a 1:b/ 2:b/c 2:b/d/ 3:b/d/e "));

    /* postorder */
    memset(w.log, 0, sizeof(w.log));
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, PF_WALK_POSTORDER, record, &w), 0);
    PF_ASSERT_ALWAYS(pos(&w, "2:b/c ") < pos(&w, "1:b/ "));
    PF_ASSERT_ALWAYS(pos(&w, "3:b/d/e ") < pos(&w, "2:b/d/ "));
    PF_ASSERT_ALWAYS(pos(&w, "2:b/d/ ") < pos(&w, "1:b/ "));

    /* max_depth */
    memset(w.log, 0, sizeof(w.log));
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 1, 0, record, &w), 0);
    PF_ASSERT_ALWAYS_EQ(strlen(w.log), strlen("1:a 1:b/ "));
    memset(w.log, 0, sizeof(w.log));
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 2, 0, record, &w), 0);
    PF_ASSERT_ALWAYS(strstr(w.log, "3:") == NULL);
    pos(&w, "2:b/d/ ");

    /* skip */
    memset(w.log, 0, sizeof(w.log));
    w.skip = "d";
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, 0, record, &w), 0);
    PF_ASSERT_ALWAYS(strstr(w.log, "3:") == NULL);
    pos(&w, "2:b/c ");
    w.skip = NULL;

    /* stop */
    memset(w.log, 0, sizeof(w.log));
    w.stop_at = "e";
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, 0, record, &w), 42);
    w.stop_at = NULL;

    /* a path with a trailing separator */
    memset(w.log, 0, sizeof(w.log));
    {
        char slash[PF_PATH_MAX];
        PF_ASSERT_ALWAYS(pf_path_join(slash, sizeof(slash), dir, "") > 0);
        PF_ASSERT_ALWAYS_EQ(pf_dir_walk(slash, 1, 0, record, &w), 0);
        pos(&w, "1:a ");
    }

    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(test_path(0, dir, "none"), 0, 0, record, &w), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, 0, NULL, &w), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_walk_follow(int seed, int i) {
    char dir[PF_PATH_MAX];
    walk_log w;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    make_tree(dir);
    /* b/d/up -> .. (a loop back to b), and l -> b */
    if (pf_symlink("..", test_path(0, dir, "b/d/up")) != 0) {
        pf_remove_all(dir);
        return SKIP("no permission to create symbolic links");
    }
    PF_ASSERT_ALWAYS_EQ(pf_symlink("b", test_path(0, dir, "l")), 0);

    memset(&w, 0, sizeof(w));
    w.base = strlen(dir) + 1;
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, 0, record, &w), 0);
    pos(&w, "1:l ");
    pos(&w, "3:b/d/up ");
    PF_ASSERT_ALWAYS(strstr(w.log, "2:l/c") == NULL);

    memset(w.log, 0, sizeof(w.log));
    PF_ASSERT_ALWAYS_EQ(pf_dir_walk(dir, 0, PF_WALK_FOLLOW, record, &w), 0);
    pos(&w, "1:l/ ");
    pos(&w, "2:l/c ");
    pos(&w, "3:l/d/e ");
    /* up points back to b, which is being walked: reported, not entered */
    pos(&w, "3:b/d/up ");
    PF_ASSERT_ALWAYS(strstr(w.log, "4:b/d/up/") == NULL);

    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

pf_test_t suite_dirent[] = {
    { test_read_dir, "read" },
    { test_symlink_type, "symlink_type" },
    { test_walk, "walk" },
    { test_walk_follow, "walk_follow" },
    { 0 },
};
