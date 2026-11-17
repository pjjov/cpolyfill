/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/watch.h>, on the system's backend and on scanning. */

#include "../src/internal.h"

#include "../src/watch_impl.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/poller.h>
#include <cpolyfill/time.h>
#include <cpolyfill/watch.h>

#include <stdio.h>
#include <string.h>

#include "check.h"
#include "files.h"

#define MS 1000000LL
#define SEP PF_PATH_SEP_STR

/* Runs `body(w, dir)` with a fresh watcher of every backend and a fresh
   directory. */
#define EACH_BACKEND(w, dir, body)                                    \
    do {                                                              \
        int be_;                                                      \
        for (be_ = PF__WATCH_DEFAULT; be_ <= PF__WATCH_SCAN; be_++) { \
            pf_watcher_t w;                                           \
            char dir[PF_PATH_MAX];                                    \
            test_tmpdir(dir, sizeof(dir));                            \
            PF_ASSERT_ALWAYS_EQ(                                      \
                pf__watcher_init_with(&w, NULL, be_, 20 * MS), 0      \
            );                                                        \
            body;                                                     \
            PF_ASSERT_ALWAYS_EQ(pf_watcher_free(&w), 0);              \
            PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);               \
        }                                                             \
    } while (0)

/* The events seen so far, copied. */
typedef struct seen_t {
    int id;
    int kind;
    char path[256];
    char old[256];
    bool has_old;
    bool dir;
} seen_t;

static seen_t seen[256];
static int nseen;

static void forget(void) { nseen = 0; }

/* Reads for up to `ms`, keeping what comes. Stops early once an event
   like the one asked for is seen (`path` NULL: any): whether it was. */
static bool expect_ms(
    pf_watcher_t *w, int kind, const char *path, const char *old, long ms
) {
    int64_t end = (int64_t)pf_time_ns() + ms * MS;
    int k;
    for (;;) {
        pf_watch_event_t ev[8];
        int64_t left = end - (int64_t)pf_time_ns();
        int n, i;
        for (k = 0; k < nseen; k++) {
            seen_t *s = &seen[k];
            if (s->kind == kind && (!path || strcmp(s->path, path) == 0)
                && (!old || (s->has_old && strcmp(s->old, old) == 0)))
                return true;
        }
        if (left <= 0)
            return false;
        n = pf_watcher_read_ns(w, ev, 8, left);
        PF_ASSERT_ALWAYS(n >= 0);
        for (i = 0; i < n && nseen < 256; i++) {
            seen_t *s = &seen[nseen++];
            s->id = ev[i].id;
            s->kind = ev[i].kind;
            snprintf(s->path, sizeof(s->path), "%s", ev[i].path);
            s->has_old = ev[i].old_path != NULL;
            snprintf(
                s->old,
                sizeof(s->old),
                "%s",
                ev[i].old_path ? ev[i].old_path : ""
            );
            s->dir = ev[i].dir;
        }
    }
}

static bool expect(
    pf_watcher_t *w, int kind, const char *path, const char *old
) {
    return expect_ms(w, kind, path, old, 3000);
}

/* Nothing more within `ms`. */
static void quiet(pf_watcher_t *w, long ms) {
    forget();
    expect_ms(w, -1, NULL, NULL, ms);
    PF_ASSERT_ALWAYS_EQ(nseen, 0);
}

/* Contents of a different size, so the change shows whatever the time
   stamps' resolution. */
static void put(const char *path, const char *text) { test_write(path, text); }

static void files_body(pf_watcher_t *w, const char *dir) {
    int id = pf_watcher_add(w, dir, 0);
    PF_ASSERT_ALWAYS(id > 0);
    quiet(w, 60);

    forget();
    put(test_path(0, dir, "a.txt"), "one");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "a.txt", NULL));
    PF_ASSERT_ALWAYS_EQ(seen[0].id, id);
    PF_ASSERT_ALWAYS(!seen[0].dir);
    PF_ASSERT_ALWAYS(!seen[0].has_old);

    forget();
    put(test_path(0, dir, "a.txt"), "two two");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_MODIFIED, "a.txt", NULL));

    forget();
    PF_ASSERT_ALWAYS_EQ(
        pf_rename(test_path(0, dir, "a.txt"), test_path(1, dir, "b.txt")), 0
    );
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_RENAMED, "b.txt", "a.txt"));

    forget();
    PF_ASSERT_ALWAYS_EQ(pf_unlink(test_path(0, dir, "b.txt")), 0);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_REMOVED, "b.txt", NULL));
    quiet(w, 60);

    /* A directory, not followed without PF_WATCH_RECURSIVE. */
    forget();
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(test_path(0, dir, "sub"), 0755), 0);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "sub", NULL));
    PF_ASSERT_ALWAYS(seen[nseen - 1].dir);
    quiet(w, 60);
    put(test_path(0, dir, "sub" SEP "x"), "x");
    quiet(w, 100);

    PF_ASSERT_ALWAYS_EQ(pf_watcher_remove(w, id), 0);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_remove(w, id), PF_ENOENT);
    put(test_path(0, dir, "c.txt"), "c");
    quiet(w, 60);
}

static int test_files(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(w, dir, files_body(&w, dir));
    return 0;
}

static void recursive_body(pf_watcher_t *w, const char *dir) {
    int id;
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(test_path(0, dir, "old"), 0755), 0);
    put(test_path(0, dir, "old" SEP "f"), "f");
    id = pf_watcher_add(w, dir, PF_WATCH_RECURSIVE);
    PF_ASSERT_ALWAYS(id > 0);
    quiet(w, 60);

    /* A new tree is reported entry by entry. */
    forget();
    PF_ASSERT_ALWAYS_EQ(pf_mkdirs(test_path(0, dir, "a" SEP "b"), 0755), 0);
    put(test_path(0, dir, "a" SEP "b" SEP "deep"), "deep");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "a", NULL));
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "a" SEP "b", NULL));
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "a" SEP "b" SEP "deep", NULL));

    /* Changes deep inside. */
    forget();
    put(test_path(0, dir, "a" SEP "b" SEP "deep"), "deeper");
    PF_ASSERT_ALWAYS(
        expect(w, PF_WATCH_MODIFIED, "a" SEP "b" SEP "deep", NULL)
    );
    forget();
    put(test_path(0, dir, "old" SEP "g"), "g");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "old" SEP "g", NULL));

    /* A directory renamed: one event, and its contents are followed under
       the new name. */
    forget();
    PF_ASSERT_ALWAYS_EQ(
        pf_rename(test_path(0, dir, "old"), test_path(1, dir, "new")), 0
    );
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_RENAMED, "new", "old"));
    PF_ASSERT_ALWAYS(seen[nseen - 1].dir);
    quiet(w, 60);
    forget();
    put(test_path(0, dir, "new" SEP "h"), "h");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "new" SEP "h", NULL));

    /* Moved between directories of the tree. */
    forget();
    PF_ASSERT_ALWAYS_EQ(
        pf_rename(
            test_path(0, dir, "new" SEP "h"), test_path(1, dir, "a" SEP "h2")
        ),
        0
    );
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_RENAMED, "a" SEP "h2", "new" SEP "h"));

    /* A tree removed. */
    forget();
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(test_path(0, dir, "a")), 0);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_REMOVED, "a", NULL));
    PF_ASSERT_ALWAYS_EQ(pf_watcher_remove(w, id), 0);
}

static int test_recursive(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(w, dir, recursive_body(&w, dir));
    return 0;
}

static void file_body(pf_watcher_t *w, const char *dir) {
    const char *f = test_path(2, dir, "watched.txt");
    int id;

    put(f, "v1");
    id = pf_watcher_add(w, f, 0);
    PF_ASSERT_ALWAYS(id > 0);
    quiet(w, 60);

    /* Other files of the directory do not count. */
    put(test_path(0, dir, "other"), "o");
    quiet(w, 60);

    forget();
    put(f, "v2 longer");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_MODIFIED, "watched.txt", NULL));

    /* Replaced the way editors save. */
    forget();
    put(test_path(0, dir, "watched.tmp"), "v3 new contents");
    PF_ASSERT_ALWAYS_EQ(pf_rename(test_path(0, dir, "watched.tmp"), f), 0);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_MODIFIED, "watched.txt", NULL));

    forget();
    PF_ASSERT_ALWAYS_EQ(pf_unlink(f), 0);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_REMOVED, "watched.txt", NULL));
    forget();
    put(f, "back");
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "watched.txt", NULL));

    PF_ASSERT_ALWAYS_EQ(pf_watcher_add(w, f, PF_WATCH_RECURSIVE), PF_ENOTDIR);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_remove(w, id), 0);
}

static int test_file(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(w, dir, file_body(&w, dir));
    return 0;
}

static void gone_body(pf_watcher_t *w, const char *dir) {
    const char *sub = test_path(2, dir, "watched");
    pf_watch_event_t ev[4];
    int id;

    PF_ASSERT_ALWAYS_EQ(pf_mkdir(sub, 0755), 0);
    id = pf_watcher_add(w, sub, 0);
    PF_ASSERT_ALWAYS(id > 0);
    forget();
    PF_ASSERT_ALWAYS_EQ(pf_rmdir(sub), 0);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_GONE, "", NULL));
    /* Quiet afterwards, also when the directory comes back. */
    PF_ASSERT_ALWAYS_EQ(pf_mkdir(sub, 0755), 0);
    put(test_path(0, sub, "x"), "x");
    quiet(w, 60);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_remove(w, id), 0);

    PF_ASSERT_ALWAYS_EQ(
        pf_watcher_add(w, test_path(0, dir, "missing"), 0), PF_ENOENT
    );
    PF_ASSERT_ALWAYS_EQ(pf_watcher_add(w, "", 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_add(w, dir, 0x80), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_read_ns(w, ev, 0, 0), PF_EINVAL);
}

static int test_gone(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(w, dir, gone_body(&w, dir));
    return 0;
}

/* More events than room: the rest wait for the next read. */
static void small_body(pf_watcher_t *w, const char *dir) {
    pf_watch_event_t ev[1];
    int id = pf_watcher_add(w, dir, 0), k, total = 0;
    int64_t end;
    PF_ASSERT_ALWAYS(id > 0);
    for (k = 0; k < 5; k++) {
        char name[16];
        snprintf(name, sizeof(name), "f%d", k);
        put(test_path(0, dir, name), "x");
    }
    end = (int64_t)pf_time_ns() + 3000 * MS;
    while (total < 5 && (int64_t)pf_time_ns() < end) {
        int n = pf_watcher_read_ns(w, ev, 1, 100 * MS);
        PF_ASSERT_ALWAYS(n == 0 || n == 1);
        if (n)
            PF_ASSERT_ALWAYS_EQ(ev[0].kind, PF_WATCH_CREATED);
        total += n;
    }
    PF_ASSERT_ALWAYS_EQ(total, 5);
    quiet(w, 60);
}

static int test_small(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(w, dir, small_body(&w, dir));
    return 0;
}

/* Events lost: the watch is scanned again, and the loss reported. */
static void overflow_body(pf_watcher_t *w, const char *dir) {
    int id = pf_watcher_add(w, dir, 0);
    PF_ASSERT_ALWAYS(id > 0);
    forget();
    put(test_path(0, dir, "x"), "x");
    pf__watch_overflow((pf__watcher *)w->pf__impl, NULL);
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_OVERFLOW, "", NULL));
    PF_ASSERT_ALWAYS(expect(w, PF_WATCH_CREATED, "x", NULL));
}

static int test_overflow(int seed, int i) {
    (void)seed, (void)i;
    EACH_BACKEND(w, dir, overflow_body(&w, dir));
    return 0;
}

static int test_poller(int seed, int i) {
    pf_watcher_t w;
    pf_poller_t p;
    pf_poll_event_t pev[4];
    pf_watch_event_t ev[8];
    char dir[PF_PATH_MAX];
    int pid, n;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    PF_ASSERT_ALWAYS_EQ(pf_poller_init(&p, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf__watcher_init_with(&w, NULL, PF__WATCH_SCAN, 20 * MS), 0
    );
    PF_ASSERT_ALWAYS_EQ(pf_poller_add_watcher(&p, &w, NULL), PF_ENOSYS);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_free(&w), 0);

    PF_ASSERT_ALWAYS_EQ(pf_watcher_init(&w, NULL), 0);
    PF_ASSERT_ALWAYS(pf_watcher_add(&w, dir, 0) > 0);
    pid = pf_poller_add_watcher(&p, &w, &w);
    if (pid == PF_ENOSYS) {
        pf_watcher_free(&w);
        pf_poller_free(&p);
        pf_remove_all(dir);
        return SKIP("the watcher scans here");
    }
    PF_ASSERT_ALWAYS(pid > 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_wait_ns(&p, pev, 4, 30 * MS), 0);
    put(test_path(0, dir, "x"), "x");
    n = pf_poller_wait_ns(&p, pev, 4, 3000 * MS);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_EQ(pev[0].id, pid);
    PF_ASSERT_ALWAYS(pev[0].data == &w);
    PF_ASSERT_ALWAYS_EQ(pev[0].events, PF_POLL_IN);
    n = pf_watcher_read_ns(&w, ev, 8, 0);
    PF_ASSERT_ALWAYS(n >= 1);
    PF_ASSERT_ALWAYS_EQ(ev[0].kind, PF_WATCH_CREATED);
    PF_ASSERT_ALWAYS_EQ(strcmp(ev[0].path, "x"), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_remove(&p, pid), 0);
    PF_ASSERT_ALWAYS_EQ(pf_watcher_free(&w), 0);
    PF_ASSERT_ALWAYS_EQ(pf_poller_free(&p), 0);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

pf_test_t suite_watch[] = {
    { test_files, "files" },   { test_recursive, "recursive" },
    { test_file, "file" },     { test_gone, "gone" },
    { test_small, "small" },   { test_overflow, "overflow" },
    { test_poller, "poller" }, { 0 },
};
