/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Scratch directories for the file tests. */

#ifndef PF_TEST_FILES_H
#define PF_TEST_FILES_H

#include <cpolyfill/fs.h>

#include "check.h"

/* Creates a new empty directory under the temporary directory, for one
   test; remove it with pf_remove_all. */
static inline void test_tmpdir(char *buf, size_t size) {
    char tmp[PF_PATH_MAX];
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_TEMP, tmp, sizeof(tmp)) > 0);
    PF_ASSERT_ALWAYS(
        pf_path_join(buf, size, tmp, "cpolyfill-test-XXXXXX") > 0
    );
    PF_ASSERT_ALWAYS(pf_mkdtemp(buf) == buf);
}

/* `dir` joined with `name`, in a static buffer (one per `slot`). */
static inline const char *test_path(int slot, const char *dir, const char *name) {
    static char bufs[4][PF_PATH_MAX];
    PF_ASSERT_ALWAYS(
        pf_path_join(bufs[slot], sizeof(bufs[slot]), dir, name) > 0
    );
    return bufs[slot];
}

/* Writes `text` to a new file. */
static inline void test_write(const char *path, const char *text) {
    FILE *f = pf_fopen(path, "wb");
    PF_ASSERT_ALWAYS(f != NULL);
    PF_ASSERT_ALWAYS_EQ(fwrite(text, 1, strlen(text), f), strlen(text));
    PF_ASSERT_ALWAYS_EQ(fclose(f), 0);
}

/* Reads a whole small file into a static buffer. */
static inline const char *test_read(const char *path) {
    static char buf[4096];
    size_t n;
    FILE *f = pf_fopen(path, "rb");
    PF_ASSERT_ALWAYS(f != NULL);
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

#endif /* PF_TEST_FILES_H */
