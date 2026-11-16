/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Pipes: anonymous pipes, and named pipes as Windows named pipes or
    AF_UNIX stream sockets.
*/

#include "internal.h"

#include <cpolyfill/pipe.h>
#include <cpolyfill/socket.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if PF_OS_WINDOWS
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <sddl.h>
#else
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

/* ---- Anonymous pipes --------------------------------------------------- */

PF_API int pf_pipe(int fds[2], unsigned flags) {
    if (!fds)
        return PF_EINVAL;
#if PF_OS_WINDOWS
    if (flags & PF_PIPE_NONBLOCK)
        return PF_ENOSYS;
    if (_pipe(fds, 65536, _O_BINARY | _O_NOINHERIT) != 0)
        return pf_error_from_errno(errno);
    return PF_OK;
#else
    {
        int raw[2];
    #if defined(O_CLOEXEC) && !PF_OS_APPLE
        if (pipe2(raw, O_CLOEXEC | ((flags & PF_PIPE_NONBLOCK) ? O_NONBLOCK : 0))
            != 0)
            return pf_error_from_errno(errno);
    #else
        int i;
        if (pipe(raw) != 0)
            return pf_error_from_errno(errno);
        for (i = 0; i < 2; i++) {
            int fl = fcntl(raw[i], F_GETFD);
            bool ok = fl >= 0 && fcntl(raw[i], F_SETFD, fl | FD_CLOEXEC) == 0;
            if (ok && (flags & PF_PIPE_NONBLOCK)) {
                fl = fcntl(raw[i], F_GETFL);
                ok = fl >= 0 && fcntl(raw[i], F_SETFL, fl | O_NONBLOCK) == 0;
            }
            if (!ok) {
                int err = errno;
                close(raw[0]);
                close(raw[1]);
                return pf_error_from_errno(err);
            }
        }
    #endif
        fds[0] = raw[0];
        fds[1] = raw[1];
        return PF_OK;
    }
#endif
}

/* ---- Named pipes ------------------------------------------------------- */

#if PF_OS_WINDOWS

typedef struct pf__pipe_srv {
    wchar_t *name;      /* \\.\pipe\... */
    HANDLE pending;     /* the instance the next client connects to */
    HANDLE event;
    OVERLAPPED ov;
    bool waiting;       /* ConnectNamedPipe is in progress on `pending` */
} pf__pipe_srv;

/* The full pipe name in new memory (malloc). */
static int pf__np_name(const char *name, wchar_t **out) {
    static const char prefix[] = "\\\\.\\pipe\\";
    char buf[300];
    pf__wstr w;
    size_t n;
    int rc;

    if (!name || !*name)
        return PF_EINVAL;
    if (strncmp(name, prefix, sizeof(prefix) - 1) == 0)
        n = (size_t)snprintf(buf, sizeof(buf), "%s", name);
    else
        n = (size_t)snprintf(buf, sizeof(buf), "%s%s", prefix, name);
    /* Windows allows 256 characters in all. */
    if (n >= 256 + sizeof(prefix) - 1 || n >= sizeof(buf))
        return PF_ENAMETOOLONG;
    rc = pf__wstr_from_utf8(&w, buf);
    if (rc)
        return rc;
    {
        size_t len = wcslen((const wchar_t *)w.ptr);
        *out = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
        if (*out)
            memcpy(*out, w.ptr, (len + 1) * sizeof(wchar_t));
    }
    pf__wstr_free(&w);
    return *out ? PF_OK : PF_ENOMEM;
}

/* A new server instance, for the owner and SYSTEM only, local clients
   only. */
static HANDLE pf__np_create(const wchar_t *name, bool first, int *rc) {
    SECURITY_ATTRIBUTES sa;
    PSECURITY_DESCRIPTOR sd = NULL;
    HANDLE h;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;OW)", SDDL_REVISION_1, &sd, NULL
        )) {
        *rc = pf_error_last_win32();
        return INVALID_HANDLE_VALUE;
    }
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    h = CreateNamedPipeW(
        name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED
            | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT
            | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, &sa
    );
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        *rc = (first && e == ERROR_ACCESS_DENIED) ? PF_EADDRINUSE
                                                   : pf_error_from_win32(e);
    } else {
        *rc = PF_OK;
    }
    LocalFree(sd);
    return h;
}

PF_API int pf_pipe_listen(pf_pipe_server_t *srv, const char *name) {
    pf__pipe_srv *impl;
    int rc;

    if (!srv)
        return PF_EINVAL;
    srv->pf__impl = NULL;
    impl = (pf__pipe_srv *)calloc(1, sizeof(*impl));
    if (!impl)
        return PF_ENOMEM;
    rc = pf__np_name(name, &impl->name);
    if (rc) {
        free(impl);
        return rc;
    }
    impl->event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!impl->event) {
        rc = pf_error_last_win32();
    } else {
        impl->pending = pf__np_create(impl->name, true, &rc);
    }
    if (rc) {
        if (impl->event)
            CloseHandle(impl->event);
        free(impl->name);
        free(impl);
        return rc;
    }
    srv->pf__impl = impl;
    return PF_OK;
}

PF_API int pf_pipe_accept(
    pf_pipe_server_t *srv, pf_pipe_t *p, const struct timespec *deadline
) {
    pf__pipe_srv *impl;
    int rc;

    if (!srv || !srv->pf__impl || !p)
        return PF_EINVAL;
    impl = (pf__pipe_srv *)srv->pf__impl;
    p->pf__h = -1;
    p->pf__overlapped = 0;
    if (impl->pending == INVALID_HANDLE_VALUE) {
        impl->pending = pf__np_create(impl->name, false, &rc);
        if (rc)
            return rc;
    }
    if (!impl->waiting) {
        memset(&impl->ov, 0, sizeof(impl->ov));
        ResetEvent(impl->event);
        impl->ov.hEvent = impl->event;
        if (!ConnectNamedPipe(impl->pending, &impl->ov)) {
            DWORD e = GetLastError();
            if (e == ERROR_IO_PENDING)
                impl->waiting = true;
            else if (e != ERROR_PIPE_CONNECTED)
                return pf_error_from_win32(e);
        }
    }
    while (impl->waiting) {
        int ms = pf__deadline_ms(deadline);
        DWORD r, n;
        r = WaitForSingleObject(impl->event, ms < 0 ? INFINITE : (DWORD)ms);
        if (r == WAIT_TIMEOUT) {
            if (pf__deadline_ms(deadline) == 0)
                return PF_ETIMEDOUT;
            continue;
        }
        if (r != WAIT_OBJECT_0)
            return pf_error_last_win32();
        impl->waiting = false;
        if (!GetOverlappedResult(impl->pending, &impl->ov, &n, FALSE)) {
            /* The client left already: start over on a fresh instance. */
            rc = pf_error_last_win32();
            CloseHandle(impl->pending);
            impl->pending = INVALID_HANDLE_VALUE;
            return rc;
        }
    }
    p->pf__h = (intptr_t)impl->pending;
    p->pf__overlapped = 1;
    impl->pending = pf__np_create(impl->name, false, &rc);
    /* Without a new instance now, the next accept tries again. */
    return PF_OK;
}

int pf__pipe_server_poll_handle(pf_pipe_server_t *srv, intptr_t *h) {
    pf__pipe_srv *impl;
    int rc;

    if (!srv || !srv->pf__impl)
        return PF_EINVAL;
    impl = (pf__pipe_srv *)srv->pf__impl;
    if (impl->pending == INVALID_HANDLE_VALUE) {
        impl->pending = pf__np_create(impl->name, false, &rc);
        if (rc)
            return rc;
    }
    if (!impl->waiting) {
        memset(&impl->ov, 0, sizeof(impl->ov));
        ResetEvent(impl->event);
        impl->ov.hEvent = impl->event;
        if (!ConnectNamedPipe(impl->pending, &impl->ov)) {
            DWORD e = GetLastError();
            if (e == ERROR_IO_PENDING)
                impl->waiting = true;
            else if (e == ERROR_PIPE_CONNECTED)
                SetEvent(impl->event); /* pf_pipe_accept takes it */
            else
                return pf_error_from_win32(e);
        }
    }
    *h = (intptr_t)impl->event;
    return PF_OK;
}

PF_API int pf_pipe_server_close(pf_pipe_server_t *srv) {
    pf__pipe_srv *impl;
    if (!srv || !srv->pf__impl)
        return PF_OK;
    impl = (pf__pipe_srv *)srv->pf__impl;
    if (impl->waiting) {
        DWORD n;
        CancelIoEx(impl->pending, &impl->ov);
        GetOverlappedResult(impl->pending, &impl->ov, &n, TRUE);
    }
    if (impl->pending != INVALID_HANDLE_VALUE)
        CloseHandle(impl->pending);
    CloseHandle(impl->event);
    free(impl->name);
    free(impl);
    srv->pf__impl = NULL;
    return PF_OK;
}

PF_API int pf_pipe_connect(
    pf_pipe_t *p, const char *name, const struct timespec *deadline
) {
    wchar_t *wname;
    int rc;

    if (!p)
        return PF_EINVAL;
    p->pf__h = -1;
    p->pf__overlapped = 0;
    rc = pf__np_name(name, &wname);
    if (rc)
        return rc;
    for (;;) {
        /* Overlapped, so a poller can wait on it (pf__np_io waits for each
           call). */
        HANDLE h = CreateFileW(
            wname, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT
                | SECURITY_IDENTIFICATION,
            NULL
        );
        DWORD e;
        int ms;
        if (h != INVALID_HANDLE_VALUE) {
            p->pf__h = (intptr_t)h;
            p->pf__overlapped = 1;
            rc = PF_OK;
            break;
        }
        e = GetLastError();
        if (e != ERROR_PIPE_BUSY) {
            rc = e == ERROR_FILE_NOT_FOUND ? PF_ENOENT : pf_error_from_win32(e);
            break;
        }
        ms = pf__deadline_ms(deadline);
        if (ms == 0) {
            rc = PF_ETIMEDOUT;
            break;
        }
        if (!WaitNamedPipeW(wname, ms < 0 ? NMPWAIT_WAIT_FOREVER : (DWORD)ms)) {
            e = GetLastError();
            if (e == ERROR_FILE_NOT_FOUND) {
                rc = PF_ENOENT;
                break;
            }
            if (e != ERROR_SEM_TIMEOUT) {
                rc = pf_error_from_win32(e);
                break;
            }
        }
    }
    free(wname);
    return rc;
}

static pf_ssize_t pf__np_io(pf_pipe_t *p, void *buf, size_t n, bool write) {
    HANDLE h = (HANDLE)p->pf__h;
    DWORD len = n > MAXDWORD ? MAXDWORD : (DWORD)n, done = 0;
    BOOL ok;

    if (p->pf__overlapped) {
        /* An event per call, so a read and a write may run at once. */
        OVERLAPPED ov;
        memset(&ov, 0, sizeof(ov));
        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!ov.hEvent)
            return pf_error_last_win32();
        ok = write ? WriteFile(h, buf, len, NULL, &ov)
                   : ReadFile(h, buf, len, NULL, &ov);
        if (ok || GetLastError() == ERROR_IO_PENDING)
            ok = GetOverlappedResult(h, &ov, &done, TRUE);
        {
            DWORD e = GetLastError();
            CloseHandle(ov.hEvent);
            SetLastError(e);
        }
    } else {
        ok = write ? WriteFile(h, buf, len, &done, NULL)
                   : ReadFile(h, buf, len, &done, NULL);
    }
    if (!ok) {
        DWORD e = GetLastError();
        if (e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED
            || e == ERROR_NO_DATA)
            return write ? PF_EPIPE : 0;
        return pf_error_from_win32(e);
    }
    return (pf_ssize_t)done;
}

PF_API pf_ssize_t pf_pipe_read(pf_pipe_t *p, void *buf, size_t n) {
    if (!p || p->pf__h == -1)
        return PF_EBADF;
    if (n == 0)
        return 0;
    return pf__np_io(p, buf, n, false);
}

PF_API pf_ssize_t pf_pipe_write(pf_pipe_t *p, const void *buf, size_t n) {
    if (!p || p->pf__h == -1)
        return PF_EBADF;
    if (n == 0)
        return 0;
    return pf__np_io(p, (void *)buf, n, true);
}

PF_API int pf_pipe_close(pf_pipe_t *p) {
    if (!p || p->pf__h == -1)
        return PF_OK;
    if (!CloseHandle((HANDLE)p->pf__h)) {
        p->pf__h = -1;
        return pf_error_last_win32();
    }
    p->pf__h = -1;
    return PF_OK;
}

#else /* POSIX */

typedef struct pf__pipe_srv {
    pf_socket_t fd;
    dev_t dev;
    ino_t ino;
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
} pf__pipe_srv;

/* The socket path of `name`. */
static int pf__pipe_path(const char *name, char *buf, size_t size) {
    const char *dir;
    size_t n, len;

    if (!name || !*name)
        return PF_EINVAL;
    if (strchr(name, '/')) {
        n = strlen(name);
        if (n >= size)
            return PF_ENAMETOOLONG;
        memcpy(buf, name, n + 1);
        return PF_OK;
    }
    dir = getenv("XDG_RUNTIME_DIR");
    if (!dir || !*dir)
        dir = getenv("TMPDIR");
    if (!dir || !*dir)
        dir = "/tmp";
    len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/')
        len--;
    n = (size_t)snprintf(buf, size, "%.*s/%s", (int)len, dir, name);
    return n >= size ? PF_ENAMETOOLONG : PF_OK;
}

PF_API int pf_pipe_listen(pf_pipe_server_t *srv, const char *name) {
    pf__pipe_srv *impl;
    pf_sockaddr_t addr;
    struct stat st;
    int rc;

    if (!srv)
        return PF_EINVAL;
    srv->pf__impl = NULL;
    impl = (pf__pipe_srv *)calloc(1, sizeof(*impl));
    if (!impl)
        return PF_ENOMEM;
    impl->fd = PF_SOCKET_INVALID;
    rc = pf__pipe_path(name, impl->path, sizeof(impl->path));
    if (!rc)
        rc = pf_sockaddr_unix(&addr, impl->path);
    if (!rc)
        rc = pf_socket(&impl->fd, AF_UNIX, SOCK_STREAM, 0, PF_SOCK_NONBLOCK);
    if (!rc) {
        rc = pf_bind(impl->fd, &addr);
        if (rc == PF_EADDRINUSE) {
            /* Replace a socket file nobody listens on any more. */
            pf_socket_t c;
            int crc = pf_unix_connect(&c, impl->path);
            if (crc == PF_OK)
                pf_socket_close(c);
            else if (crc == PF_ECONNREFUSED && lstat(impl->path, &st) == 0
                     && S_ISSOCK(st.st_mode) && unlink(impl->path) == 0)
                rc = pf_bind(impl->fd, &addr);
        }
        if (!rc) {
            if (chmod(impl->path, 0600) != 0 || stat(impl->path, &st) != 0) {
                rc = pf_error_from_errno(errno);
                unlink(impl->path);
            } else {
                impl->dev = st.st_dev;
                impl->ino = st.st_ino;
                rc = pf_listen(impl->fd, 0);
                if (rc)
                    unlink(impl->path);
            }
        }
    }
    if (rc) {
        pf_socket_close(impl->fd);
        free(impl);
        return rc;
    }
    srv->pf__impl = impl;
    return PF_OK;
}

PF_API int pf_pipe_accept(
    pf_pipe_server_t *srv, pf_pipe_t *p, const struct timespec *deadline
) {
    pf__pipe_srv *impl;

    if (!srv || !srv->pf__impl || !p)
        return PF_EINVAL;
    impl = (pf__pipe_srv *)srv->pf__impl;
    p->pf__h = -1;
    p->pf__overlapped = 0;
    for (;;) {
        pf_socket_t c;
        int rc = pf_accept(impl->fd, &c, NULL, 0);
        if (rc == PF_OK) {
            p->pf__h = c;
            return PF_OK;
        }
        if (rc != PF_EAGAIN && rc != PF_ECONNABORTED)
            return rc;
        rc = pf_socket_wait(impl->fd, POLLIN, deadline);
        if (rc < 0)
            return rc;
    }
}

int pf__pipe_server_poll_handle(pf_pipe_server_t *srv, intptr_t *h) {
    if (!srv || !srv->pf__impl)
        return PF_EINVAL;
    *h = ((pf__pipe_srv *)srv->pf__impl)->fd;
    return PF_OK;
}

PF_API int pf_pipe_server_close(pf_pipe_server_t *srv) {
    pf__pipe_srv *impl;
    struct stat st;
    int rc;

    if (!srv || !srv->pf__impl)
        return PF_OK;
    impl = (pf__pipe_srv *)srv->pf__impl;
    /* Remove the file only if it is still ours. */
    if (stat(impl->path, &st) == 0 && st.st_dev == impl->dev
        && st.st_ino == impl->ino)
        unlink(impl->path);
    rc = pf_socket_close(impl->fd);
    free(impl);
    srv->pf__impl = NULL;
    return rc;
}

PF_API int pf_pipe_connect(
    pf_pipe_t *p, const char *name, const struct timespec *deadline
) {
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    pf_sockaddr_t addr;
    int backoff = 1;
    int rc;

    if (!p)
        return PF_EINVAL;
    p->pf__h = -1;
    p->pf__overlapped = 0;
    rc = pf__pipe_path(name, path, sizeof(path));
    if (!rc)
        rc = pf_sockaddr_unix(&addr, path);
    if (rc)
        return rc;
    for (;;) {
        pf_socket_t c;
        int ms;
        rc = pf_socket(&c, AF_UNIX, SOCK_STREAM, 0, 0);
        if (rc)
            return rc;
        /* Blocking, the connect waits while the server's queue is full;
           with a deadline, it fails with PF_EAGAIN then. */
        rc = pf_connect(c, &addr, deadline);
        if (rc == PF_OK) {
            p->pf__h = c;
            return PF_OK;
        }
        pf_socket_close(c);
        if (rc == PF_ECONNREFUSED)
            return PF_ENOENT;
        if (rc != PF_EAGAIN)
            return rc;
        ms = pf__deadline_ms(deadline);
        if (ms == 0)
            return PF_ETIMEDOUT;
        pf_usleep((unsigned)(backoff < ms ? backoff : ms) * 1000u);
        if (backoff < 50)
            backoff *= 2;
    }
}

PF_API pf_ssize_t pf_pipe_read(pf_pipe_t *p, void *buf, size_t n) {
    if (!p || p->pf__h < 0)
        return PF_EBADF;
    return pf_recv((pf_socket_t)p->pf__h, buf, n, 0);
}

PF_API pf_ssize_t pf_pipe_write(pf_pipe_t *p, const void *buf, size_t n) {
    pf_ssize_t r;
    if (!p || p->pf__h < 0)
        return PF_EBADF;
    r = pf_send((pf_socket_t)p->pf__h, buf, n, 0);
    return r == PF_ECONNRESET ? PF_EPIPE : r;
}

PF_API int pf_pipe_close(pf_pipe_t *p) {
    int rc;
    if (!p || p->pf__h < 0)
        return PF_OK;
    rc = pf_socket_close((pf_socket_t)p->pf__h);
    p->pf__h = -1;
    return rc;
}

#endif

/* ---- Streams ----------------------------------------------------------- */

static pf_ssize_t pf__pipe_sread(void *ctx, void *buf, size_t n) {
    return pf_pipe_read((pf_pipe_t *)ctx, buf, n);
}

static pf_ssize_t pf__pipe_swrite(void *ctx, const void *buf, size_t n) {
    return pf_pipe_write((pf_pipe_t *)ctx, buf, n);
}

static int pf__pipe_sclose(void *ctx) {
    return pf_pipe_close((pf_pipe_t *)ctx);
}

static const struct pf_stream_vt pf__pipe_vt = {
    pf__pipe_sread, pf__pipe_swrite, NULL, NULL, NULL,
};

static const struct pf_stream_vt pf__pipe_own_vt = {
    pf__pipe_sread, pf__pipe_swrite, NULL, NULL, pf__pipe_sclose,
};

PF_API int pf_stream_from_pipe(pf_stream_t *s, pf_pipe_t *p, bool own) {
    if (!p || p->pf__h == -1)
        return PF_EBADF;
    return pf_stream_init(s, own ? &pf__pipe_own_vt : &pf__pipe_vt, p);
}
