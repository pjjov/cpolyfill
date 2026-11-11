/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    pf_opendir and friends on POSIX <dirent.h>.
*/

#include "../internal.h"

#include <cpolyfill/dirent.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

struct pf_dir_t {
    DIR *dir;
    struct pf_dirent ent;
};

PF_API pf_dir_t *pf_opendir(const char *path) {
    pf_dir_t *d;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    d = (pf_dir_t *)allocate(NULL, sizeof(*d));
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }
    d->dir = opendir(path);
    if (!d->dir) {
        int err = errno;
        deallocate(NULL, d, sizeof(*d));
        errno = err;
        return NULL;
    }
    return d;
}

static unsigned char pf__dt_from_mode(mode_t mode) {
    if (S_ISREG(mode))
        return PF_DT_REG;
    if (S_ISDIR(mode))
        return PF_DT_DIR;
    if (S_ISLNK(mode))
        return PF_DT_LNK;
    if (S_ISFIFO(mode))
        return PF_DT_FIFO;
    if (S_ISSOCK(mode))
        return PF_DT_SOCK;
    if (S_ISCHR(mode))
        return PF_DT_CHR;
    if (S_ISBLK(mode))
        return PF_DT_BLK;
    return PF_DT_UNKNOWN;
}

PF_API struct pf_dirent *pf_readdir(pf_dir_t *d) {
    int saved = errno;
    struct dirent *e;
    size_t len;
    unsigned char type = PF_DT_UNKNOWN;

    if (!d) {
        errno = EBADF;
        return NULL;
    }
    errno = 0;
    e = readdir(d->dir);
    if (!e) {
        if (errno == 0)
            errno = saved;
        return NULL;
    }
    len = strlen(e->d_name);
    if (len > PF_NAME_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    memcpy(d->ent.d_name, e->d_name, len + 1);
    d->ent.d_ino = (uint64_t)e->d_ino;

#ifdef DT_UNKNOWN
    switch (e->d_type) {
    case DT_REG:
        type = PF_DT_REG;
        break;
    case DT_DIR:
        type = PF_DT_DIR;
        break;
    case DT_LNK:
        type = PF_DT_LNK;
        break;
    case DT_FIFO:
        type = PF_DT_FIFO;
        break;
    case DT_SOCK:
        type = PF_DT_SOCK;
        break;
    case DT_CHR:
        type = PF_DT_CHR;
        break;
    case DT_BLK:
        type = PF_DT_BLK;
        break;
    default:
        break;
    }
#endif
    if (type == PF_DT_UNKNOWN) {
        /* Some file systems (and systems) leave d_type unknown. */
        struct stat st;
        if (fstatat(dirfd(d->dir), e->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0)
            type = pf__dt_from_mode(st.st_mode);
    }
    d->ent.d_type = type;
    errno = saved;
    return &d->ent;
}

PF_API void pf_rewinddir(pf_dir_t *d) {
    if (d)
        rewinddir(d->dir);
}

PF_API int pf_closedir(pf_dir_t *d) {
    int rc;

    if (!d) {
        errno = EBADF;
        return -1;
    }
    rc = closedir(d->dir);
    deallocate(NULL, d, sizeof(*d));
    return rc;
}
