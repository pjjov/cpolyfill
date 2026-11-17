/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/fs.h> on POSIX systems.
*/

#include "../internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/string.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if PF_OS_APPLE
    #include <copyfile.h>
#endif

/* ---- Stand-ins -------------------------------------------------------- */

PF_API int pf_mkdir(const char *path, pf_mode_t mode) {
    return mkdir(path, mode);
}

PF_API int pf_rmdir(const char *path) { return rmdir(path); }

PF_API int pf_unlink(const char *path) { return unlink(path); }

PF_API int pf_rename(const char *from, const char *to) {
    return rename(from, to);
}

PF_API int pf_chdir(const char *path) { return chdir(path); }

PF_API char *pf_getcwd(char *buf, size_t size) {
    size_t cap;

    if (buf)
        return getcwd(buf, size);
    if (size) {
        buf = (char *)malloc(size);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
        if (!getcwd(buf, size)) {
            int err = errno;
            free(buf);
            errno = err;
            return NULL;
        }
        return buf;
    }
    for (cap = 256;; cap *= 2) {
        buf = (char *)malloc(cap);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
        if (getcwd(buf, cap))
            return buf;
        free(buf);
        if (errno != ERANGE)
            return NULL;
    }
}

PF_API int pf_access(const char *path, int mode) { return access(path, mode); }

PF_API char *pf_realpath(const char *path, char *resolved) {
    char *full;
    size_t len;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    full = realpath(path, NULL);
    if (!full || !resolved)
        return full;
    len = strlen(full);
    if (len >= PF_PATH_MAX) {
        free(full);
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(resolved, full, len + 1);
    free(full);
    return resolved;
}

PF_API int pf_mkstemp(char *tmpl) {
#if PF_CONFIG_HAVE_MKSTEMP
    return mkstemp(tmpl);
#else
    return pf__mktemp(tmpl, false, 0600);
#endif
}

PF_API char *pf_mkdtemp(char *tmpl) {
#if PF_CONFIG_HAVE_MKDTEMP
    return mkdtemp(tmpl);
#else
    return pf__mktemp(tmpl, true, 0700) == 0 ? tmpl : NULL;
#endif
}

PF_API int pf_symlink(const char *target, const char *linkpath) {
    return symlink(target, linkpath);
}

PF_API pf_ssize_t pf_readlink(const char *path, char *buf, size_t size) {
    return readlink(path, buf, size);
}

PF_API int pf_link(const char *existing, const char *linkpath) {
    return link(existing, linkpath);
}

PF_API int pf_truncate(const char *path, pf_off_t size) {
    if ((off_t)size != size) {
        errno = EOVERFLOW;
        return -1;
    }
    return truncate(path, (off_t)size);
}

PF_API int pf_chmod(const char *path, pf_mode_t mode) {
    return chmod(path, mode);
}

/* ---- Status ----------------------------------------------------------- */

static pf_file_type pf__type_from_mode(mode_t mode) {
    if (S_ISREG(mode))
        return PF_FILE_REGULAR;
    if (S_ISDIR(mode))
        return PF_FILE_DIR;
    if (S_ISLNK(mode))
        return PF_FILE_SYMLINK;
    if (S_ISFIFO(mode))
        return PF_FILE_FIFO;
    if (S_ISSOCK(mode))
        return PF_FILE_SOCKET;
    if (S_ISCHR(mode))
        return PF_FILE_CHAR;
    if (S_ISBLK(mode))
        return PF_FILE_BLOCK;
    return PF_FILE_OTHER;
}

#if PF__HAVE_STATX

static struct timespec pf__ts_statx(struct statx_timestamp t) {
    struct timespec ts;
    ts.tv_sec = (time_t)t.tv_sec;
    ts.tv_nsec = (long)t.tv_nsec;
    return ts;
}

/* -1: statx is not available (old kernel or seccomp filter). */
static int pf__statx(int dirfd, const char *path, int flags, pf_stat_t *st) {
    struct statx sx;

    if (statx(dirfd, path, flags, STATX_BASIC_STATS | STATX_BTIME, &sx) != 0)
        return errno == ENOSYS || errno == EPERM ? -1
                                                 : pf_error_from_errno(errno);
    memset(st, 0, sizeof(*st));
    st->type = pf__type_from_mode(sx.stx_mode);
    st->perm = sx.stx_mode & 07777;
    st->size = sx.stx_size;
    st->dev = ((uint64_t)sx.stx_dev_major << 32) | sx.stx_dev_minor;
    st->ino = sx.stx_ino;
    st->nlink = sx.stx_nlink;
    st->uid = sx.stx_uid;
    st->gid = sx.stx_gid;
    st->atime = pf__ts_statx(sx.stx_atime);
    st->mtime = pf__ts_statx(sx.stx_mtime);
    st->ctime = pf__ts_statx(sx.stx_ctime);
    if (sx.stx_mask & STATX_BTIME)
        st->btime = pf__ts_statx(sx.stx_btime);
    return PF_OK;
}

#endif

#if PF_OS_APPLE
    #define PF__ST_TIME(st, x) ((st)->st_##x##timespec)
#else
    #define PF__ST_TIME(st, x) ((st)->st_##x##tim)
#endif

static void pf__from_stat(const struct stat *s, pf_stat_t *st) {
    memset(st, 0, sizeof(*st));
    st->type = pf__type_from_mode(s->st_mode);
    st->perm = (uint32_t)(s->st_mode & 07777);
    st->size = (uint64_t)s->st_size;
    st->dev = (uint64_t)s->st_dev;
    st->ino = (uint64_t)s->st_ino;
    st->nlink = (uint64_t)s->st_nlink;
    st->uid = (uint32_t)s->st_uid;
    st->gid = (uint32_t)s->st_gid;
    st->atime = PF__ST_TIME(s, a);
    st->mtime = PF__ST_TIME(s, m);
    st->ctime = PF__ST_TIME(s, c);
#if PF_OS_APPLE || PF_OS_NETBSD
    st->btime = s->st_birthtimespec;
#elif PF_OS_FREEBSD || PF_OS_DRAGONFLY
    st->btime = s->st_birthtim;
#endif
}

PF_API int pf_stat(const char *path, pf_stat_t *st) {
    struct stat s;

    if (!path || !st)
        return PF_EINVAL;
#if PF__HAVE_STATX
    {
        int rc = pf__statx(AT_FDCWD, path, 0, st);
        if (rc != -1)
            return rc;
    }
#endif
    if (stat(path, &s) != 0)
        return pf_error_from_errno(errno);
    pf__from_stat(&s, st);
    return PF_OK;
}

PF_API int pf_lstat(const char *path, pf_stat_t *st) {
    struct stat s;

    if (!path || !st)
        return PF_EINVAL;
#if PF__HAVE_STATX
    {
        int rc = pf__statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, st);
        if (rc != -1)
            return rc;
    }
#endif
    if (lstat(path, &s) != 0)
        return pf_error_from_errno(errno);
    pf__from_stat(&s, st);
    return PF_OK;
}

PF_API int pf_fstat(int fd, pf_stat_t *st) {
    struct stat s;

    if (!st)
        return PF_EINVAL;
#if PF__HAVE_STATX
    {
        int rc = pf__statx(fd, "", AT_EMPTY_PATH, st);
        if (rc != -1)
            return rc;
    }
#endif
    if (fstat(fd, &s) != 0)
        return pf_error_from_errno(errno);
    pf__from_stat(&s, st);
    return PF_OK;
}

/* ---- Copying and locking ---------------------------------------------- */

static int pf__copy_data(int in, int out) {
    char buf[65536];

#if PF__HAVE_COPY_FILE_RANGE
    for (;;) {
        ssize_t n = copy_file_range(in, NULL, out, NULL, 1 << 30, 0);
        if (n == 0)
            return PF_OK;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            /* Not between these files: copy by hand from where it is. */
            if (errno == EXDEV || errno == ENOSYS || errno == EINVAL
                || errno == EOPNOTSUPP || errno == EBADF || errno == EPERM)
                break;
            return pf_error_from_errno(errno);
        }
    }
#elif PF_OS_APPLE
    if (fcopyfile(in, out, NULL, COPYFILE_DATA) == 0)
        return PF_OK;
#endif

    for (;;) {
        ssize_t n = read(in, buf, sizeof(buf)), done = 0;
        if (n == 0)
            return PF_OK;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return pf_error_from_errno(errno);
        }
        while (done < n) {
            ssize_t w = write(out, buf + done, (size_t)(n - done));
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                return pf_error_from_errno(errno);
            }
            done += w;
        }
    }
}

PF_API int pf_copy_file(const char *from, const char *to, unsigned flags) {
    struct stat si, so;
    int in, out, rc, oflags = O_WRONLY | O_CREAT | O_CLOEXEC;

    if (!from || !to)
        return PF_EINVAL;
    in = open(from, O_RDONLY | O_CLOEXEC);
    if (in < 0)
        return pf_error_from_errno(errno);
    if (fstat(in, &si) != 0) {
        rc = pf_error_from_errno(errno);
        close(in);
        return rc;
    }
    if (S_ISDIR(si.st_mode)) {
        close(in);
        return PF_EISDIR;
    }
    if (!(flags & PF_COPY_OVERWRITE))
        oflags |= O_EXCL;
    out = open(to, oflags, si.st_mode & 0777);
    if (out < 0) {
        rc = pf_error_from_errno(errno);
        close(in);
        return rc;
    }
    /* Copying a file onto itself would truncate it. */
    if (fstat(out, &so) != 0) {
        rc = pf_error_from_errno(errno);
    } else if (so.st_dev == si.st_dev && so.st_ino == si.st_ino) {
        rc = PF_EINVAL;
    } else if (ftruncate(out, 0) != 0) {
        rc = pf_error_from_errno(errno);
    } else {
        rc = pf__copy_data(in, out);
        if (rc == 0 && fchmod(out, si.st_mode & 07777) != 0)
            rc = pf_error_from_errno(errno);
    }
    close(in);
    if (close(out) != 0 && rc == 0 && errno != EINTR)
        rc = pf_error_from_errno(errno);
    if (rc && rc != PF_EINVAL)
        unlink(to);
    return rc;
}

PF_API int pf_lock_file(int fd, int op) {
    int how;

    switch (op & ~PF_LOCK_NB) {
    case PF_LOCK_SH:
        how = LOCK_SH;
        break;
    case PF_LOCK_EX:
        how = LOCK_EX;
        break;
    case PF_LOCK_UN:
        how = LOCK_UN;
        break;
    default:
        return PF_EINVAL;
    }
    if (op & PF_LOCK_NB)
        how |= LOCK_NB;
    while (flock(fd, how) != 0) {
        if (errno == EINTR)
            continue;
        return pf_error_from_errno(errno);
    }
    return PF_OK;
}

/* ---- Known directories ------------------------------------------------ */

/* `s` without separators at the end (but "/" stays). */
static ptrdiff_t pf__copy_dir(char *buf, size_t size, const char *s) {
    size_t len = strlen(s);
    while (len > 1 && s[len - 1] == '/')
        len--;
    return pf__copy_out(buf, size, s, len);
}

static ptrdiff_t pf__passwd_dir(
    const char *user, uid_t uid, char *buf, size_t size
) {
    struct passwd pw, *res = NULL;
    long hint = sysconf(_SC_GETPW_R_SIZE_MAX);
    size_t cap = hint > 0 ? (size_t)hint : 1024;
    ptrdiff_t rc;
    char *tmp;

    for (;;) {
        int err;
        tmp = (char *)allocate(NULL, cap);
        if (!tmp)
            return PF_ENOMEM;
        err = user ? getpwnam_r(user, &pw, tmp, cap, &res)
                   : getpwuid_r(uid, &pw, tmp, cap, &res);
        if (err != ERANGE)
            break;
        deallocate(NULL, tmp, cap);
        cap *= 2;
    }
    if (!res || !pw.pw_dir || !*pw.pw_dir)
        rc = PF_ENOENT;
    else
        rc = pf__copy_dir(buf, size, pw.pw_dir);
    deallocate(NULL, tmp, cap);
    return rc;
}

static ptrdiff_t pf__home(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (home && *home == '/')
        return pf__copy_dir(buf, size, home);
    return pf__passwd_dir(NULL, getuid(), buf, size);
}

/* The home directory joined with `leaf`. */
static ptrdiff_t pf__home_join(char *buf, size_t size, const char *leaf) {
    char home[PF_PATH_MAX];
    ptrdiff_t n = pf__home(home, sizeof(home));
    if (n < 0)
        return n == PF_ERANGE ? PF_ENAMETOOLONG : n;
    return pf_path_join(buf, size, home, leaf);
}

static ptrdiff_t pf__temp_dir(char *buf, size_t size) {
    const char *tmp = getenv("TMPDIR");
    if (tmp && *tmp == '/')
        return pf__copy_dir(buf, size, tmp);
#if PF_OS_APPLE
    {
        char dir[PF_PATH_MAX];
        size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, dir, sizeof(dir));
        if (n > 0 && n <= sizeof(dir))
            return pf__copy_dir(buf, size, dir);
    }
#endif
    return pf__copy_out(buf, size, "/tmp", 4);
}

#if !PF_OS_APPLE

/* An XDG base directory: the variable if it holds an absolute path, else
   the default under the home directory. */
static ptrdiff_t pf__xdg(
    char *buf, size_t size, const char *var, const char *leaf
) {
    const char *dir = getenv(var);
    if (dir && *dir == '/')
        return pf__copy_dir(buf, size, dir);
    return pf__home_join(buf, size, leaf);
}

/* A directory from user-dirs.dirs (xdg-user-dirs): lines like
   XDG_DESKTOP_DIR="$HOME/Desktop". PF_ENOENT if the file or the key is
   missing. */
static ptrdiff_t pf__user_dirs(char *buf, size_t size, const char *key) {
    char conf[PF_PATH_MAX], file[PF_PATH_MAX];
    size_t klen = strlen(key), cap = 0;
    ptrdiff_t rc = PF_ENOENT;
    char *line = NULL;
    FILE *f;

    if (pf__xdg(conf, sizeof(conf), "XDG_CONFIG_HOME", ".config") < 0
        || pf_path_join(file, sizeof(file), conf, "user-dirs.dirs") < 0)
        return PF_ENOENT;
    f = fopen(file, "re");
    if (!f)
        return PF_ENOENT;

    while (pf_getline(&line, &cap, f) >= 0) {
        char *p = line, *end;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, key, klen) != 0)
            continue;
        p += klen;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p++ != '=')
            continue;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p++ != '"')
            continue;
        end = strchr(p, '"');
        if (!end)
            continue;
        *end = 0;
        if (strncmp(p, "$HOME", 5) == 0 && (p[5] == '/' || p[5] == 0)) {
            p += 5;
            while (*p == '/')
                p++;
            rc = *p ? pf__home_join(buf, size, p) : pf__home(buf, size);
        } else if (*p == '/') {
            rc = pf__copy_dir(buf, size, p);
        }
        break;
    }
    free(line);
    fclose(f);
    return rc;
}

#endif

PF_API ptrdiff_t pf_known_dir(pf_known_dir_t kind, char *buf, size_t size) {
#if PF_OS_APPLE
    switch (kind) {
    case PF_DIR_HOME:
        return pf__home(buf, size);
    case PF_DIR_TEMP:
    case PF_DIR_RUNTIME:
        return pf__temp_dir(buf, size);
    case PF_DIR_DATA:
    case PF_DIR_CONFIG:
    case PF_DIR_STATE:
        return pf__home_join(buf, size, "Library/Application Support");
    case PF_DIR_CACHE:
        return pf__home_join(buf, size, "Library/Caches");
    case PF_DIR_DESKTOP:
        return pf__home_join(buf, size, "Desktop");
    case PF_DIR_DOCUMENTS:
        return pf__home_join(buf, size, "Documents");
    case PF_DIR_DOWNLOAD:
        return pf__home_join(buf, size, "Downloads");
    case PF_DIR_MUSIC:
        return pf__home_join(buf, size, "Music");
    case PF_DIR_PICTURES:
        return pf__home_join(buf, size, "Pictures");
    case PF_DIR_PUBLICSHARE:
        return pf__home_join(buf, size, "Public");
    case PF_DIR_TEMPLATES:
        return PF_ENOENT;
    case PF_DIR_VIDEOS:
        return pf__home_join(buf, size, "Movies");
    case PF_DIR_PROJECTS:
        return pf__home_join(buf, size, "Projects");
    }
    return PF_EINVAL;
#else
    /* xdg-user-dirs keys and English defaults, from PF_DIR_DESKTOP on. */
    static const char *const user_keys[][2] = {
        { "XDG_DESKTOP_DIR", "Desktop" },
        { "XDG_DOCUMENTS_DIR", "Documents" },
        { "XDG_DOWNLOAD_DIR", "Downloads" },
        { "XDG_MUSIC_DIR", "Music" },
        { "XDG_PICTURES_DIR", "Pictures" },
        { "XDG_PUBLICSHARE_DIR", "Public" },
        { "XDG_TEMPLATES_DIR", "Templates" },
        { "XDG_VIDEOS_DIR", "Videos" },
        { "XDG_PROJECTS_DIR", "Projects" },
    };

    switch (kind) {
    case PF_DIR_HOME:
        return pf__home(buf, size);
    case PF_DIR_TEMP:
        return pf__temp_dir(buf, size);
    case PF_DIR_DATA:
        return pf__xdg(buf, size, "XDG_DATA_HOME", ".local/share");
    case PF_DIR_CONFIG:
        return pf__xdg(buf, size, "XDG_CONFIG_HOME", ".config");
    case PF_DIR_STATE:
        return pf__xdg(buf, size, "XDG_STATE_HOME", ".local/state");
    case PF_DIR_CACHE:
        return pf__xdg(buf, size, "XDG_CACHE_HOME", ".cache");
    case PF_DIR_RUNTIME: {
        const char *dir = getenv("XDG_RUNTIME_DIR");
        char guess[64];
        struct stat st;
        if (dir && *dir == '/')
            return pf__copy_dir(buf, size, dir);
        snprintf(
            guess, sizeof(guess), "/run/user/%lu", (unsigned long)getuid()
        );
        if (stat(guess, &st) == 0 && S_ISDIR(st.st_mode)
            && st.st_uid == getuid())
            return pf__copy_out(buf, size, guess, strlen(guess));
        return pf__temp_dir(buf, size);
    }
    case PF_DIR_DESKTOP:
    case PF_DIR_DOCUMENTS:
    case PF_DIR_DOWNLOAD:
    case PF_DIR_MUSIC:
    case PF_DIR_PICTURES:
    case PF_DIR_PUBLICSHARE:
    case PF_DIR_TEMPLATES:
    case PF_DIR_VIDEOS:
    case PF_DIR_PROJECTS: {
        const char *const *k = user_keys[kind - PF_DIR_DESKTOP];
        ptrdiff_t rc = pf__user_dirs(buf, size, k[0]);
        if (rc != PF_ENOENT)
            return rc;
        return pf__home_join(buf, size, k[1]);
    }
    }
    return PF_EINVAL;
#endif
}

PF_API ptrdiff_t pf_home_dir_of(const char *user, char *buf, size_t size) {
    if (!user || !*user)
        return pf__home(buf, size);
    return pf__passwd_dir(user, 0, buf, size);
}
