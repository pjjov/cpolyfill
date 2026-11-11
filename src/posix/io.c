/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/io.h> on POSIX systems.
*/

#include "../internal.h"

#include <cpolyfill/io.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

PF_API int pf_open(const char *path, int flags, pf_mode_t mode) {
    int fd;

    do
        fd = open(path, flags, mode);
    while (fd < 0 && errno == EINTR);
    return fd;
}

PF_API int pf_close(int fd) {
    /* After EINTR the descriptor is released on Linux and the BSDs, so
       it must not be closed again. */
    int rc = close(fd);
    if (rc < 0 && errno == EINTR)
        return 0;
    return rc;
}

PF_API pf_ssize_t pf_read(int fd, void *buf, size_t n) {
    return read(fd, buf, n);
}

PF_API pf_ssize_t pf_write(int fd, const void *buf, size_t n) {
    return write(fd, buf, n);
}

PF_API pf_ssize_t pf_pread(int fd, void *buf, size_t n, pf_off_t off) {
    if ((off_t)off != off) {
        errno = EOVERFLOW;
        return -1;
    }
    return pread(fd, buf, n, (off_t)off);
}

PF_API pf_ssize_t pf_pwrite(int fd, const void *buf, size_t n, pf_off_t off) {
    if ((off_t)off != off) {
        errno = EOVERFLOW;
        return -1;
    }
    return pwrite(fd, buf, n, (off_t)off);
}

PF_API pf_off_t pf_lseek(int fd, pf_off_t off, int whence) {
    if ((off_t)off != off) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pf_off_t)lseek(fd, (off_t)off, whence);
}

PF_API int pf_fsync(int fd) {
#if PF_OS_APPLE
    /* fsync only reaches the drive's cache on macOS. */
    if (fcntl(fd, F_FULLFSYNC) == 0)
        return 0;
#endif
    return fsync(fd);
}

PF_API int pf_ftruncate(int fd, pf_off_t size) {
    if ((off_t)size != size) {
        errno = EOVERFLOW;
        return -1;
    }
    return ftruncate(fd, (off_t)size);
}

PF_API int pf_dup(int fd) { return dup(fd); }

PF_API int pf_dup2(int fd, int fd2) {
    int rc;

    do
        rc = dup2(fd, fd2);
    while (rc < 0 && errno == EINTR);
    return rc;
}

/* fopen modes to open() flags. Returns -1 with errno EINVAL for a bad
   mode, and the mode for fdopen (without "x" and "e") in `fdmode`. */
static int pf__fopen_flags(const char *mode, char fdmode[4]) {
    int flags;
    const char *p;
    size_t n = 0;

    switch (mode[0]) {
    case 'r':
        flags = O_RDONLY;
        break;
    case 'w':
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case 'a':
        flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    fdmode[n++] = mode[0];
    for (p = mode + 1; *p; p++) {
        switch (*p) {
        case '+':
            flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
            if (!memchr(fdmode, '+', n))
                fdmode[n++] = '+';
            break;
        case 'b':
            break;
        case 'x':
            flags |= O_EXCL;
            break;
        case 'e':
            flags |= O_CLOEXEC;
            break;
        default: /* other letters (glibc's "m", "c", ...) are ignored */
            break;
        }
    }
    fdmode[n] = 0;
    return flags;
}

PF_API FILE *pf_fopen(const char *path, const char *mode) {
    char fdmode[4];
    int flags, fd;
    FILE *f;

    if (!path || !mode) {
        errno = EINVAL;
        return NULL;
    }
    flags = pf__fopen_flags(mode, fdmode);
    if (flags < 0)
        return NULL;
    fd = pf_open(path, flags, 0666);
    if (fd < 0)
        return NULL;
    f = fdopen(fd, fdmode);
    if (!f) {
        int err = errno;
        close(fd);
        errno = err;
    }
    return f;
}

PF_API int pf_fseeko(FILE *f, pf_off_t off, int whence) {
    if ((off_t)off != off) {
        errno = EOVERFLOW;
        return -1;
    }
    return fseeko(f, (off_t)off, whence);
}

PF_API pf_off_t pf_ftello(FILE *f) { return (pf_off_t)ftello(f); }

PF_API int pf_fileno(FILE *f) { return fileno(f); }

PF_API FILE *pf_fmemopen(void *buf, size_t size, const char *mode) {
#if PF_CONFIG_HAVE_FMEMOPEN
    return fmemopen(buf, size, mode);
#else
    (void)buf, (void)size, (void)mode;
    errno = ENOSYS;
    return NULL;
#endif
}

PF_API FILE *pf_open_memstream(char **ptr, size_t *size) {
#if PF_CONFIG_HAVE_OPEN_MEMSTREAM
    return open_memstream(ptr, size);
#else
    (void)ptr, (void)size;
    errno = ENOSYS;
    return NULL;
#endif
}
