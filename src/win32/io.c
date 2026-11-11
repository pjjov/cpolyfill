/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/io.h> on Windows, on the CRT's descriptors.
*/

#include "../internal.h"

#include <cpolyfill/io.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <share.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

static HANDLE pf__handle(int fd) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        errno = EBADF;
    return h;
}

static int pf__wopen(const char *path, int flags, pf_mode_t mode) {
    pf__wstr w;
    int fd, rc, pmode = _S_IREAD;

    if ((rc = pf__wpath(&w, path)) != 0)
        return pf__errno_fail(rc);
    if (mode & 0200)
        pmode |= _S_IWRITE;
    /* Share everything, as POSIX does: other processes may read, write,
       rename and delete the file while it is open. */
    rc = _wsopen_s(&fd, (const wchar_t *)w.ptr, flags, _SH_DENYNO, pmode);
    pf__wstr_free(&w);
    if (rc) {
        errno = rc;
        return -1;
    }
    return fd;
}

PF_API int pf_open(const char *path, int flags, pf_mode_t mode) {
    flags &= ~_O_TEXT;
    return pf__wopen(path, flags | _O_BINARY, mode);
}

PF_API int pf_close(int fd) { return _close(fd); }

PF_API pf_ssize_t pf_read(int fd, void *buf, size_t n) {
    return _read(fd, buf, n > INT_MAX ? INT_MAX : (unsigned)n);
}

PF_API pf_ssize_t pf_write(int fd, const void *buf, size_t n) {
    return _write(fd, buf, n > INT_MAX ? INT_MAX : (unsigned)n);
}

static pf_ssize_t pf__rw_at(
    int fd, void *buf, size_t n, pf_off_t off, bool write
) {
    OVERLAPPED ov;
    DWORD done = 0;
    HANDLE h;
    BOOL ok;

    if (off < 0) {
        errno = EINVAL;
        return -1;
    }
    if ((h = pf__handle(fd)) == INVALID_HANDLE_VALUE)
        return -1;
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)((uint64_t)off & 0xFFFFFFFFu);
    ov.OffsetHigh = (DWORD)((uint64_t)off >> 32);
    if (n > INT_MAX)
        n = INT_MAX;
    ok = write ? WriteFile(h, buf, (DWORD)n, &done, &ov)
               : ReadFile(h, buf, (DWORD)n, &done, &ov);
    if (!ok) {
        if (!write && GetLastError() == ERROR_HANDLE_EOF)
            return 0;
        return pf__errno_win32();
    }
    return (pf_ssize_t)done;
}

PF_API pf_ssize_t pf_pread(int fd, void *buf, size_t n, pf_off_t off) {
    return pf__rw_at(fd, buf, n, off, false);
}

PF_API pf_ssize_t pf_pwrite(int fd, const void *buf, size_t n, pf_off_t off) {
    return pf__rw_at(fd, (void *)buf, n, off, true);
}

PF_API pf_off_t pf_lseek(int fd, pf_off_t off, int whence) {
    return _lseeki64(fd, off, whence);
}

PF_API int pf_fsync(int fd) {
    HANDLE h = pf__handle(fd);

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!FlushFileBuffers(h)) {
        /* Consoles and pipes have nothing to flush. */
        if (GetLastError() == ERROR_INVALID_HANDLE)
            return 0;
        return pf__errno_win32();
    }
    return 0;
}

PF_API int pf_ftruncate(int fd, pf_off_t size) {
    errno_t rc = _chsize_s(fd, size);
    if (rc) {
        errno = rc;
        return -1;
    }
    return 0;
}

PF_API int pf_dup(int fd) { return _dup(fd); }

PF_API int pf_dup2(int fd, int fd2) {
    if (_dup2(fd, fd2) != 0)
        return -1;
    return fd2;
}

PF_API FILE *pf_fopen(const char *path, const char *mode) {
    char fdmode[5];
    size_t n = 0;
    int flags, fd;
    bool binary = false;
    const char *p;
    FILE *f;

    if (!path || !mode) {
        errno = EINVAL;
        return NULL;
    }
    switch (mode[0]) {
    case 'r':
        flags = _O_RDONLY;
        break;
    case 'w':
        flags = _O_WRONLY | _O_CREAT | _O_TRUNC;
        break;
    case 'a':
        flags = _O_WRONLY | _O_CREAT | _O_APPEND;
        break;
    default:
        errno = EINVAL;
        return NULL;
    }
    fdmode[n++] = mode[0];
    for (p = mode + 1; *p; p++) {
        switch (*p) {
        case '+':
            flags = (flags & ~(_O_RDONLY | _O_WRONLY)) | _O_RDWR;
            if (!memchr(fdmode, '+', n))
                fdmode[n++] = '+';
            break;
        case 'b':
            binary = true;
            break;
        case 'x':
            flags |= _O_EXCL;
            break;
        case 'e':
            flags |= _O_NOINHERIT;
            break;
        default:
            break;
        }
    }
    if (binary)
        fdmode[n++] = 'b';
    fdmode[n] = 0;

    fd = pf__wopen(path, flags | (binary ? _O_BINARY : _O_TEXT), 0666);
    if (fd < 0)
        return NULL;
    f = _fdopen(fd, fdmode);
    if (!f) {
        int err = errno;
        _close(fd);
        errno = err;
    }
    return f;
}

PF_API int pf_fseeko(FILE *f, pf_off_t off, int whence) {
    return _fseeki64(f, off, whence);
}

PF_API pf_off_t pf_ftello(FILE *f) { return _ftelli64(f); }

PF_API int pf_fileno(FILE *f) { return _fileno(f); }

/* A read-only memory file: a copy of `buf` in a temporary file that is
   deleted when the last handle to it closes. */
PF_API FILE *pf_fmemopen(void *buf, size_t size, const char *mode) {
    wchar_t dir[MAX_PATH + 1], name[MAX_PATH + 1];
    const char *p = (const char *)buf;
    HANDLE h;
    int fd;
    FILE *f;

    if (!mode || mode[0] != 'r' || (mode[1] && strcmp(mode + 1, "b") != 0)) {
        errno = mode && mode[0] ? ENOSYS : EINVAL;
        return NULL;
    }
    if (!buf && size) {
        errno = EINVAL;
        return NULL;
    }
    if (!GetTempPathW(MAX_PATH + 1, dir) || !GetTempFileNameW(dir, L"pfm", 0, name)) {
        pf__errno_win32();
        return NULL;
    }
    h = CreateFileW(
        name,
        GENERIC_READ | GENERIC_WRITE | DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        NULL
    );
    if (h == INVALID_HANDLE_VALUE) {
        pf__errno_win32();
        DeleteFileW(name);
        return NULL;
    }
    while (size) {
        DWORD chunk = size > 0x40000000u ? 0x40000000u : (DWORD)size, done;
        if (!WriteFile(h, p, chunk, &done, NULL)) {
            pf__errno_win32();
            CloseHandle(h);
            return NULL;
        }
        p += done;
        size -= done;
    }
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    fd = _open_osfhandle((intptr_t)h, _O_RDONLY | _O_BINARY);
    if (fd < 0) {
        CloseHandle(h);
        return NULL;
    }
    f = _fdopen(fd, "rb");
    if (!f) {
        int err = errno;
        _close(fd);
        errno = err;
    }
    return f;
}

PF_API FILE *pf_open_memstream(char **ptr, size_t *size) {
    (void)ptr, (void)size;
    errno = ENOSYS;
    return NULL;
}
