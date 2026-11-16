/** cpolyfill - Filling the gaps between C standards and compilers

    Reading directories, the same on every platform: names are UTF-8
    (WTF-8 on Windows, see <cpolyfill/fs.h>) and the type of every entry is
    known.

        pf_dir_t                An open directory (opaque).
        struct pf_dirent        d_ino   file number (0 on Windows)
                                d_type  PF_DT_REG, PF_DT_DIR, PF_DT_LNK,
                                        PF_DT_FIFO, PF_DT_SOCK, PF_DT_CHR,
                                        PF_DT_BLK, or PF_DT_UNKNOWN if the
                                        entry vanished before its type
                                        could be read
                                d_name  the name, NUL-terminated
        PF_NAME_MAX             The longest name, in bytes (1023; 255
                                UTF-16 units of Windows can take 765).

        pf_dir_t *pf_opendir(const char *path)
                NULL with errno on failure.
        struct pf_dirent *pf_readdir(pf_dir_t *dir)
                The next entry, `.` and `..` included, or NULL at the end
                (errno unchanged) or on an error (errno set). The entry is
                overwritten by the next call on the same `dir`.
        void pf_rewinddir(pf_dir_t *dir)
        int pf_closedir(pf_dir_t *dir)
                0, or -1 with errno.

        int pf_dir_walk(const char *path, int max_depth, unsigned flags,
                        pf_walk_fn *fn, void *user)
                Calls `fn(entry_path, entry, depth, user)` for every entry
                under the directory `path` (without `.` and `..`), depth
                first; `entry_path` is `path` joined with the names, and
                `depth` is 1 for the entries of `path`. `max_depth` limits
                the depth (1: only `path` itself, 0: no limit).
                `fn` returns 0 to go on, PF_WALK_SKIP to not enter the
                directory it was given, anything else to stop: pf_dir_walk
                then returns that value. pf_dir_walk returns 0 after the
                last entry, or a negative PF_E* code if a directory cannot
                be read. Flags:
                  PF_WALK_POSTORDER  directories after their content
                                     (PF_WALK_SKIP has no effect)
                  PF_WALK_FOLLOW     enter symbolic links to directories
                                     (`d_type` is then PF_DT_DIR); a link
                                     back to a directory being walked is
                                     not entered
                Changing the tree while it is walked (other than removing
                entries already reported, as pf_remove_all does) may
                report entries twice or not at all.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_DIRENT, only where
    the library found no <dirent.h> (Windows): `DIR`, `struct dirent`,
    `opendir`, `readdir`, `rewinddir`, `closedir`, `DT_*`, `NAME_MAX`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_DIRENT_H
#define CPOLYFILL_DIRENT_H

#include <cpolyfill/config.h>
#include <cpolyfill/platform.h>

#include <stdint.h>

#define PF_NAME_MAX 1023

/* The values of Linux and the BSDs. */
#define PF_DT_UNKNOWN 0
#define PF_DT_FIFO 1
#define PF_DT_CHR 2
#define PF_DT_DIR 4
#define PF_DT_BLK 6
#define PF_DT_REG 8
#define PF_DT_LNK 10
#define PF_DT_SOCK 12

#define PF_WALK_SKIP 1
#define PF_WALK_POSTORDER 1u
#define PF_WALK_FOLLOW 2u

typedef struct pf_dir_t pf_dir_t;

struct pf_dirent {
    uint64_t d_ino;
    unsigned char d_type;
    char d_name[PF_NAME_MAX + 1];
};

typedef int(pf_walk_fn)(
    const char *path, const struct pf_dirent *entry, int depth, void *user
);

PF_BEGIN_DECLS

PF_API pf_dir_t *pf_opendir(const char *path);
PF_API struct pf_dirent *pf_readdir(pf_dir_t *dir);
PF_API void pf_rewinddir(pf_dir_t *dir);
PF_API int pf_closedir(pf_dir_t *dir);
PF_API int pf_dir_walk(
    const char *path, int max_depth, unsigned flags, pf_walk_fn *fn, void *user
);

PF_END_DECLS

#endif /* CPOLYFILL_DIRENT_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__DIRENT_STD_NAMES)                                      \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_DIRENT))     \
        || defined(PF_STD_NAMES_DIRENT))
    #define PF__DIRENT_STD_NAMES

    #if !PF_CONFIG_HAVE_DIRENT_H
typedef pf_dir_t DIR;
        #define dirent pf_dirent
        #define opendir pf_opendir
        #define readdir pf_readdir
        #define rewinddir pf_rewinddir
        #define closedir pf_closedir
        #define DT_UNKNOWN PF_DT_UNKNOWN
        #define DT_FIFO PF_DT_FIFO
        #define DT_CHR PF_DT_CHR
        #define DT_DIR PF_DT_DIR
        #define DT_BLK PF_DT_BLK
        #define DT_REG PF_DT_REG
        #define DT_LNK PF_DT_LNK
        #define DT_SOCK PF_DT_SOCK
        #ifndef NAME_MAX
            #define NAME_MAX PF_NAME_MAX
        #endif
    #endif
#endif
