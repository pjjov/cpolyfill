/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a wrapper for POSIX and Windows named pipes.

    SPDX-FileCopyrightText: 2026 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Predrag Jovanović

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
**/

#ifndef POLYFILL_PIPE
#define POLYFILL_PIPE

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef _WIN32
    #include <stdio.h>
    #include <string.h>
    #include <windows.h>
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <sys/ioctl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

enum pf_pipe_mode {
    PF_PIPE_READ = 1,
    PF_PIPE_WRITE = 2,
    PF_PIPE_RDWR = PF_PIPE_READ | PF_PIPE_WRITE,
};

typedef struct pf_pipe_t {
    int error;
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
} pf_pipe_t;

#ifndef PF_PIPE_BUFFER_SIZE
    #define PF_PIPE_BUFFER_SIZE 1024
#endif

#define PF_PIPE_MAX_NAME 265

enum pf_pipe_bool {
    PF_PIPE_FALSE,
    PF_PIPE_TRUE,
};

enum pf_pipe_error {
    PF_PIPE_OK = 0,
    PF_PIPE_EPERM = -1,
    PF_PIPE_EIO = -5,
    PF_PIPE_EAGAIN = -11,
    PF_PIPE_EINVAL = -22,
    PF_PIPE_ENAMETOOLONG = -36,
    PF_PIPE_ETIMEDOUT = -110,
};

#ifdef _WIN32
PF_API int pf__pipe_fix_path(const char *path, char *buf) {
    int len = snprintf(buf, PF_PIPE_MAX_NAME, "\\\\.\\pipe\\%s", path);
    if (len >= PF_PIPE_MAX_NAME)
        return PF_PIPE_FALSE;

    const char *curr = &buf[9];
    while ((curr = strchr(curr, '\\')))
        *curr = '/';

    return PF_PIPE_TRUE;
}
#endif

PF_API int pf_pipe_create(pf_pipe_t *pipe, const char *path, int flags) {
    if (!path || !pipe)
        return PF_PIPE_EINVAL;

#ifdef _WIN32
    char buf[PF_PIPE_MAX_NAME];
    if (!pf__pipe_fix_path(path, buf))
        return PF_PIPE_ENAMETOOLONG;

    int _flags = FILE_FLAG_FIRST_PIPE_INSTANCE;
    if (flags & PF_PIPE_RDWR)
        _flags |= PIPE_ACCESS_DUPLEX;
    else if (flags & PF_PIPE_READ)
        _flags |= PIPE_ACCESS_INBOUND;
    else if (flags & PF_PIPE_WRITE)
        _flags |= PIPE_ACCESS_OUTBOUND;

    pipe->handle = CreateNamedPipeA(
        buf,
        _flags,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        PF_PIPE_BUFFER_SIZE,
        PF_PIPE_BUFFER_SIZE,
        0,
        NULL
    );

    if (pipe->handle == INVALID_HANDLE_VALUE)
        return PF_PIPE_EIO;

    return PF_PIPE_OK;
#else
    if (mkfifo(path, 0666) == -1)
        return errno;

    int _flags = 0;
    if (flags & PF_PIPE_RDWR)
        _flags |= O_RDWR;
    else if (flags & PF_PIPE_READ)
        _flags |= O_RDONLY;
    else if (flags & PF_PIPE_WRITE)
        _flags |= O_WRONLY;

    pipe->fd = open(path, _flags);

    if (pipe->fd == -1)
        return errno;
    return PF_PIPE_OK;
#endif
}

PF_API int pf_pipe_open(pf_pipe_t *pipe, const char *path, int mode) {
    if (!path || !pipe)
        return PF_PIPE_EINVAL;

#ifdef _WIN32
    char buf[PF_PIPE_MAX_NAME];
    if (!pf__pipe_fix_path(path, buf))
        return PF_PIPE_ENAMETOOLONG;

    int _mode = 0;
    if (mode & PF_PIPE_READ)
        _mode |= GENERIC_READ;
    if (mode & PF_PIPE_WRITE)
        _mode |= GENERIC_WRITE;

    pipe->handle = CreateFile(buf, _mode, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (pipe->handle == INVALID_HANDLE_VALUE)
        return PF_PIPE_EIO;
    return PF_PIPE_OK;
#else
    int _mode = 0;
    if (mode == PF_PIPE_RDWR)
        _mode |= O_RDWR;
    else if (mode & PF_PIPE_READ)
        _mode |= O_RDONLY;
    else if (mode & PF_PIPE_WRITE)
        _mode |= O_WRONLY;

    pipe->fd = open(path, _mode);
    return pipe->fd == -1 ? errno : PF_PIPE_OK;
#endif
}

PF_API int pf_pipe_connect(pf_pipe_t *pipe) {
    if (!pipe)
        return PF_PIPE_EINVAL;

#ifdef _WIN32
    if (!ConnectNamedPipe(pipe->handle))
        return PF_PIPE_EIO;
    return PF_PIPE_OK;
#else
    return PF_PIPE_OK;
#endif
}

PF_API int pf_pipe_disconnect(pf_pipe_t *pipe) {
    if (!pipe)
        return PF_PIPE_EINVAL;

#ifdef _WIN32
    FlushFileBuffers(pipe->handle);
    if (!DisconnectNamedPipe(pipe->handle))
        return PF_PIPE_EIO;
    return PF_PIPE_OK;
#else
    return PF_PIPE_OK;
#endif
}

PF_API size_t pf_pipe_read(pf_pipe_t *pipe, void *buf, size_t size) {
    if (!pipe || !buf || size == 0)
        return 0;
    pipe->error = PF_PIPE_OK;

#ifdef _WIN32
    DWORD result;
    if (!ReadFile(pipe->handle, buf, size, &result, NULL))
        pipe->error = PF_PIPE_EIO;
    return result;
#else
    ssize_t result = read(pipe->fd, buf, size);

    if (result < 0) {
        pipe->error = errno;
        return 0;
    }

    return result;
#endif
}

PF_API size_t pf_pipe_write(pf_pipe_t *pipe, const void *buf, size_t size) {
    if (!pipe || !buf || size == 0)
        return 0;
    pipe->error = PF_PIPE_OK;

#ifdef _WIN32
    DWORD result;
    if (!WriteFile(pipe->handle, buf, size, &result, NULL))
        pipe->error = PF_PIPE_EIO;
    return result;
#else
    ssize_t result = write(pipe->fd, buf, size);

    if (result < 0) {
        pipe->error = errno;
        return 0;
    }

    return result;
#endif
}

PF_API int pf_pipe_read_timeout(
    pf_pipe_t *pipe, void *buf, size_t size, size_t *nread, int timeout_ms
) {
    if (!pipe || !buf || !nread || size == 0)
        return PF_PIPE_EINVAL;

    *nread = 0;

#ifdef _WIN32
    DWORD wait_ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(pipe->handle, wait_ms);

    if (result == WAIT_TIMEOUT)
        return PF_PIPE_ETIMEDOUT; /* 1460 – maps to a timeout condition */
    if (result != WAIT_OBJECT_0)
        return PF_PIPE_EIO;

    DWORD got = 0;
    if (!ReadFile(pipe->handle, buf, (DWORD)size, &got, NULL))
        return PF_PIPE_EIO;

    *nread = (size_t)got;
    return PF_PIPE_OK;
#else
    struct pollfd pfd;
    pfd.fd = pipe->fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);

    if (ret == 0)
        return PF_PIPE_ETIMEDOUT;
    if (ret < 0)
        return errno;
    if (!(pfd.revents & POLLIN))
        return PF_PIPE_EIO;

    ssize_t got = read(pipe->fd, buf, size);
    if (got < 0)
        return errno;

    *nread = (size_t)got;
    return PF_PIPE_OK;
#endif
}

PF_API int pf_pipe_set_nonblocking(pf_pipe_t *pipe, int enabled) {
    if (!pipe)
        return PF_PIPE_EINVAL;

#ifdef _WIN32
    DWORD mode = PIPE_READMODE_BYTE | (enabled ? PIPE_NOWAIT : PIPE_WAIT);
    if (!SetNamedPipeHandleState(pipe->handle, &mode, NULL, NULL))
        return PF_PIPE_EIO;
    return PF_PIPE_OK;
#else
    int flags = fcntl(pipe->fd, F_GETFL, 0);
    if (flags == -1)
        return errno;
    if (enabled)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(pipe->fd, F_SETFL, flags) == -1)
        return errno;
    return PF_PIPE_OK;
#endif
}

PF_API size_t pf_pipe_peek(pf_pipe_t *pipe) {
    if (!pipe)
        return 0;
    pipe->error = PF_PIPE_OK;

#ifdef _WIN32
    DWORD avail = 0;
    if (!PeekNamedPipe(pipe->handle, NULL, 0, NULL, &avail, NULL)) {
        pipe->error = PF_PIPE_EIO;
        return 0;
    }
    return (size_t)avail;
#else
    int avail = 0;
    if (ioctl(pipe->fd, FIONREAD, &avail) == -1) {
        pipe->error = errno;
        return 0;
    }
    return (size_t)avail;
#endif
}

PF_API int pf_pipe_flush(pf_pipe_t *pipe) {
    if (!pipe)
        return PF_PIPE_EINVAL;
#ifdef _WIN32
    if (!FlushFileBuffers(pipe->handle))
        return PF_PIPE_EIO;
#endif
    return PF_PIPE_OK;
}

PF_API void pf_pipe_close(pf_pipe_t *pipe) {
    if (!pipe)
        return;
#ifdef _WIN32
    pf_pipe_disconnect(pipe);
    CloseHandle(pipe->handle);
    pipe->handle = INVALID_HANDLE_VALUE;
#else
    close(pipe->fd);
    pipe->fd = -1;
#endif
}

PF_API void pf_pipe_destroy(const char *path) {
    if (!path)
        return;

#ifdef _WIN32
    (void)path;
#else
    unlink(path);
#endif
}

#endif
