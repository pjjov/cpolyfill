/** cpolyfill - Filling the gaps between C standards and compilers

    Files, directories and paths, the same on every platform.

    Paths are UTF-8. On Windows they are converted to UTF-16 for the W
    APIs, both '/' and '\' separate components, and paths too long for
    the classic MAX_PATH limit work. Windows file names may hold unpaired
    UTF-16 surrogates, which UTF-8 cannot express: paths use WTF-8 there
    (UTF-8 that also encodes lone surrogates), so every name read from the
    system can be passed back to it. Paths the library returns use the
    system's separator.

        PF_PATH_SEP, PF_PATH_SEP_STR    '\\' and "\\" on Windows, '/' and
                                        "/" elsewhere.
        PF_PATH_MAX                     4096: the size of the buffer
                                        pf_realpath fills.
        bool pf_path_is_sep(int c)      '/', and also '\\' on Windows.

    Stand-ins for POSIX functions (0, or -1 with errno; the ones returning
    a pointer return NULL with errno):

        int pf_mkdir(const char *path, pf_mode_t mode)
        int pf_rmdir(const char *path)
        int pf_unlink(const char *path)
                On Windows, unlink also removes read-only files and
                symbolic links to directories.
        int pf_rename(const char *from, const char *to)
                Replaces an existing `to`, also on Windows.
        int pf_chdir(const char *path)
        char *pf_getcwd(char *buf, size_t size)
                With `buf == NULL`, the result is new memory from `malloc`
                (at least `size` bytes).
        int pf_access(const char *path, int mode)
                PF_F_OK (exists), or any of PF_R_OK, PF_W_OK, PF_X_OK. On
                Windows a file is writable unless read-only, and
                executable if a directory or named .exe, .com, .bat or
                .cmd.
        char *pf_realpath(const char *path, char *resolved)
                The absolute path without `.`, `..` or symbolic links,
                into `resolved` (PF_PATH_MAX bytes), or new memory from
                `malloc` if it is NULL. The file must exist.
        int pf_mkstemp(char *tmpl)
        char *pf_mkdtemp(char *tmpl)
                Replace the "XXXXXX" that ends `tmpl` and create a file
                (opened for reading and writing, mode 0600) or directory
                (0700).
        int pf_symlink(const char *target, const char *linkpath)
                On Windows the link points to a directory if `target`
                (relative to the link) is one when the link is created.
                Needs developer mode or the privilege to create links.
        pf_ssize_t pf_readlink(const char *path, char *buf, size_t size)
                The target of a symbolic link, not NUL-terminated
                (truncated to `size`).
        int pf_link(const char *existing, const char *linkpath)
        int pf_truncate(const char *path, pf_off_t size)
        int pf_chmod(const char *path, pf_mode_t mode)
                On Windows only the read-only attribute changes: set when
                `mode` has no 0200 bit.

    File status (0, or a negative PF_E* code):

        pf_file_type            PF_FILE_REGULAR, PF_FILE_DIR,
                                PF_FILE_SYMLINK, PF_FILE_FIFO,
                                PF_FILE_SOCKET, PF_FILE_CHAR,
                                PF_FILE_BLOCK, PF_FILE_OTHER
        pf_stat_t               type    pf_file_type
                                perm    permission bits (07777)
                                size    size in bytes (uint64_t)
                                dev, ino  device and file number
                                nlink   number of hard links
                                uid, gid  owner (0 on Windows)
                                atime, mtime  last access and change of
                                        the content
                                ctime   last change of the status
                                btime   creation, 0 if unknown
                                (all `struct timespec`)
        int pf_stat(const char *path, pf_stat_t *st)
        int pf_lstat(const char *path, pf_stat_t *st)
        int pf_fstat(int fd, pf_stat_t *st)
                On Windows symbolic links and junctions (reparse points
                that name another path) are PF_FILE_SYMLINK; `perm` is
                0444 or 0666 depending on the read-only attribute, with
                0111 for directories and programs.
        bool pf_exists(const char *path)        (follows symlinks)
        bool pf_is_dir(const char *path)        (follows symlinks)
        bool pf_is_file(const char *path)       (regular file, follows)
        bool pf_is_symlink(const char *path)
                False on any error.
        int64_t pf_file_size(const char *path)
                The size, or a negative PF_E* code.

    Whole trees and files (0, or a negative PF_E* code):

        int pf_mkdirs(const char *path, pf_mode_t mode)
                Creates the directory and its missing parents, like
                `mkdir -p`. An existing directory is not an error.
        int pf_remove_all(const char *path)
                Removes a file or a directory with everything in it, like
                `rm -rf`. Symbolic links are removed, never followed. A
                missing `path` is not an error.
        int pf_copy_file(const char *from, const char *to, unsigned flags)
                Copies a file's content and permission bits. Fails with
                PF_EEXIST if `to` exists, unless `flags` has
                PF_COPY_OVERWRITE. Uses copy_file_range, copyfile or
                CopyFileExW, so file systems that can share the data do.
        int pf_lock_file(int fd, int op)
                Locks the whole file: PF_LOCK_SH (shared) or PF_LOCK_EX
                (exclusive), waiting unless `op` has PF_LOCK_NB (then
                PF_EWOULDBLOCK), or PF_LOCK_UN to unlock. The lock
                belongs to the open file (as with flock): another open of
                the same file, even in the same process, conflicts. It is
                advisory on POSIX (only other locks are blocked); on
                Windows it is mandatory (reads and writes through other
                handles fail) and a held lock cannot be changed from
                shared to exclusive: unlock it first.

    Known directories (the length, or PF_ERANGE if `size` is too small,
    PF_ENOENT if there is none; with `buf == NULL` only the length):

        ptrdiff_t pf_known_dir(pf_known_dir_t kind, char *buf, size_t size)
                PF_DIR_HOME      the user's home
                PF_DIR_TEMP      temporary files ($TMPDIR or /tmp)
                PF_DIR_DATA      application data
                PF_DIR_CONFIG    application settings
                PF_DIR_STATE     state that survives a restart (logs,
                                 history)
                PF_DIR_CACHE     caches
                PF_DIR_RUNTIME   sockets and other runtime files
                PF_DIR_DESKTOP, PF_DIR_DOCUMENTS, PF_DIR_DOWNLOAD,
                PF_DIR_MUSIC, PF_DIR_PICTURES, PF_DIR_PUBLICSHARE,
                PF_DIR_TEMPLATES, PF_DIR_VIDEOS, PF_DIR_PROJECTS
                         the user's folders
                Unix: the XDG base directories ($XDG_DATA_HOME or
                ~/.local/share, $XDG_CONFIG_HOME or ~/.config,
                $XDG_STATE_HOME or ~/.local/state, $XDG_CACHE_HOME or
                ~/.cache, $XDG_RUNTIME_DIR or /run/user/<uid> or the
                temporary directory) and user-dirs.dirs, with the English
                defaults (~/Desktop, ...; ~/Projects).
                macOS: ~/Library/Application Support (DATA, CONFIG,
                STATE), ~/Library/Caches, the temporary directory for
                RUNTIME, ~/Movies for VIDEOS, no TEMPLATES.
                Windows: the known folders: LocalAppData (DATA, STATE,
                CACHE), RoamingAppData (CONFIG), the temporary directory
                for RUNTIME, Documents\Projects for PROJECTS.
        ptrdiff_t pf_home_dir_of(const char *user, char *buf, size_t size)
                The home directory of another user.

    Path strings (no file system access; "a/b" stands for either
    separator on Windows):

        ptrdiff_t pf_path_join(char *buf, size_t size, const char *a,
                               const char *b)
                `a` and `b` with a separator between them; just `b` if it
                has a root (is absolute, or on Windows starts with a
                drive or a separator). Returns the length, or PF_ERANGE.
        ptrdiff_t pf_path_normalize(char *buf, size_t size, const char *path)
                Removes repeated separators, `.` components, `name/..`
                pairs and a trailing separator, without looking at the
                file system (so a/link/.. becomes `a`). `..` at the start
                of a relative path stays; after the root it is dropped.
                An empty result is ".". Uses the system's separator.
        const char *pf_path_basename(const char *path, size_t *len)
        const char *pf_path_dirname(const char *path, size_t *len)
                The last component and everything before it, as in POSIX
                basename and dirname but without changing `path`: a
                pointer into `path` (or to a static ".") and the length in
                `*len`. "a/b/" has basename "b" and dirname "a"; "/" has
                "/" for both; "b" has dirname ".".
        const char *pf_path_extension(const char *path, size_t *len)
                The extension of the basename, from its last '.' ("a.tar.gz"
                gives ".gz"), or an empty string at the end of the
                basename if it has none (also for ".bashrc" and "..").
        bool pf_path_is_absolute(const char *path)
                Starts at a root: '/' on POSIX; a drive and a separator,
                or "\\server\share", or a \\?\ path on Windows.
        size_t pf_path_root_len(const char *path)
                The length of the root at the start of `path`: 1 for "/",
                and on Windows 3 for "C:\", 2 for "C:", 1 for "\", the
                whole "\\server\share\" or "\\?\C:\" prefix; 0 if none.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_FS: on Windows
    `mkdir`, `rmdir`, `unlink`, `rename`, `chdir`, `getcwd`, `access`,
    `realpath`, `mkstemp`, `mkdtemp`, `symlink`, `readlink`, `link`,
    `truncate`, `chmod` and `F_OK`, `R_OK`, `W_OK`, `X_OK`; elsewhere the
    ones the library found missing when it was built. `stat` cannot be
    mapped (its struct has the same name).

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_FS_H
#define CPOLYFILL_FS_H

#include <cpolyfill/config.h>
#include <cpolyfill/io.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#if PF_OS_WINDOWS
    #define PF_PATH_SEP '\\'
    #define PF_PATH_SEP_STR "\\"
#else
    #define PF_PATH_SEP '/'
    #define PF_PATH_SEP_STR "/"
#endif
#define PF_PATH_MAX 4096

#define PF_F_OK 0
#define PF_X_OK 1
#define PF_W_OK 2
#define PF_R_OK 4

typedef enum pf_file_type {
    PF_FILE_REGULAR,
    PF_FILE_DIR,
    PF_FILE_SYMLINK,
    PF_FILE_FIFO,
    PF_FILE_SOCKET,
    PF_FILE_CHAR,
    PF_FILE_BLOCK,
    PF_FILE_OTHER
} pf_file_type;

typedef struct pf_stat_t {
    pf_file_type type;
    uint32_t perm;
    uint64_t size;
    uint64_t dev;
    uint64_t ino;
    uint64_t nlink;
    uint32_t uid;
    uint32_t gid;
    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;
    struct timespec btime;
} pf_stat_t;

#define PF_COPY_OVERWRITE 1u

#define PF_LOCK_SH 1
#define PF_LOCK_EX 2
#define PF_LOCK_NB 4
#define PF_LOCK_UN 8

typedef enum pf_known_dir_t {
    PF_DIR_HOME,
    PF_DIR_TEMP,
    PF_DIR_DATA,
    PF_DIR_CONFIG,
    PF_DIR_STATE,
    PF_DIR_CACHE,
    PF_DIR_RUNTIME,
    PF_DIR_DESKTOP,
    PF_DIR_DOCUMENTS,
    PF_DIR_DOWNLOAD,
    PF_DIR_MUSIC,
    PF_DIR_PICTURES,
    PF_DIR_PUBLICSHARE,
    PF_DIR_TEMPLATES,
    PF_DIR_VIDEOS,
    PF_DIR_PROJECTS
} pf_known_dir_t;

PF_INLINE bool pf_path_is_sep(int c) {
#if PF_OS_WINDOWS
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

PF_BEGIN_DECLS

PF_API int pf_mkdir(const char *path, pf_mode_t mode);
PF_API int pf_rmdir(const char *path);
PF_API int pf_unlink(const char *path);
PF_API int pf_rename(const char *from, const char *to);
PF_API int pf_chdir(const char *path);
PF_API char *pf_getcwd(char *buf, size_t size);
PF_API int pf_access(const char *path, int mode);
PF_API char *pf_realpath(const char *path, char *resolved);
PF_API int pf_mkstemp(char *tmpl);
PF_API char *pf_mkdtemp(char *tmpl);
PF_API int pf_symlink(const char *target, const char *linkpath);
PF_API pf_ssize_t pf_readlink(const char *path, char *buf, size_t size);
PF_API int pf_link(const char *existing, const char *linkpath);
PF_API int pf_truncate(const char *path, pf_off_t size);
PF_API int pf_chmod(const char *path, pf_mode_t mode);

PF_API int pf_stat(const char *path, pf_stat_t *st);
PF_API int pf_lstat(const char *path, pf_stat_t *st);
PF_API int pf_fstat(int fd, pf_stat_t *st);
PF_API bool pf_exists(const char *path);
PF_API bool pf_is_dir(const char *path);
PF_API bool pf_is_file(const char *path);
PF_API bool pf_is_symlink(const char *path);
PF_API int64_t pf_file_size(const char *path);

PF_API int pf_mkdirs(const char *path, pf_mode_t mode);
PF_API int pf_remove_all(const char *path);
PF_API int pf_copy_file(const char *from, const char *to, unsigned flags);
PF_API int pf_lock_file(int fd, int op);

PF_API ptrdiff_t pf_known_dir(pf_known_dir_t kind, char *buf, size_t size);
PF_API ptrdiff_t pf_home_dir_of(const char *user, char *buf, size_t size);

PF_API ptrdiff_t
pf_path_join(char *buf, size_t size, const char *a, const char *b);
PF_API ptrdiff_t pf_path_normalize(char *buf, size_t size, const char *path);
PF_API const char *pf_path_basename(const char *path, size_t *len);
PF_API const char *pf_path_dirname(const char *path, size_t *len);
PF_API const char *pf_path_extension(const char *path, size_t *len);
PF_API bool pf_path_is_absolute(const char *path);
PF_API size_t pf_path_root_len(const char *path);

PF_END_DECLS

#endif /* CPOLYFILL_FS_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__FS_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_FS)) \
        || defined(PF_STD_NAMES_FS))
#define PF__FS_STD_NAMES

#ifdef PF__FORCE_STD_NAMES_POLYFILL
    #define PF__FS_MAP(have) 1
#else
    #define PF__FS_MAP(have) (PF_OS_WINDOWS || !(have))
#endif

#if PF_OS_WINDOWS
    /* The CRT declares most of these in <direct.h> and <io.h>, with
           other signatures (MinGW's mkdir takes one argument). */
    #include <direct.h>
    #include <io.h>
    #include <stdio.h>
    #undef mkdir
    #define mkdir pf_mkdir
    #undef rmdir
    #define rmdir pf_rmdir
    #undef unlink
    #define unlink pf_unlink
    #undef rename
    #define rename pf_rename
    #undef chdir
    #define chdir pf_chdir
    #undef getcwd
    #define getcwd pf_getcwd
    #undef access
    #define access pf_access
    #undef chmod
    #define chmod pf_chmod
    #undef F_OK
    #define F_OK PF_F_OK
    #undef R_OK
    #define R_OK PF_R_OK
    #undef W_OK
    #define W_OK PF_W_OK
    #undef X_OK
    #define X_OK PF_X_OK
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_REALPATH)
    #undef realpath
    #define realpath pf_realpath
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_MKSTEMP)
    #undef mkstemp
    #define mkstemp pf_mkstemp
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_MKDTEMP)
    #undef mkdtemp
    #define mkdtemp pf_mkdtemp
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_SYMLINK)
    #undef symlink
    #define symlink pf_symlink
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_READLINK)
    #undef readlink
    #define readlink pf_readlink
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_LINK)
    #undef link
    #define link pf_link
#endif
#if PF__FS_MAP(PF_CONFIG_HAVE_TRUNCATE)
    #undef truncate
    #define truncate pf_truncate
#endif
#endif
