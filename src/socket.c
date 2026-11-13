/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Sockets: BSD sockets and Winsock behind one API.
*/

#include "internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/process.h>
#include <cpolyfill/random.h>
#include <cpolyfill/socket.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !PF_OS_WINDOWS
    #include <fcntl.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif

#define PF__UN_OFFSET offsetof(struct sockaddr_un, sun_path)

/* ---- Common helpers ---------------------------------------------------- */

int pf__deadline_ms(const struct timespec *deadline) {
    struct timespec now, left;
    if (!deadline)
        return -1;
    if (pf_clock_gettime(PF_CLOCK_REALTIME, &now) != 0)
        return 0;
    left = pf_timespec_sub(*deadline, now);
    if (left.tv_sec < 0)
        return 0;
    if (left.tv_sec >= INT_MAX / 1000 - 1)
        return INT_MAX;
    return (int)(left.tv_sec * 1000 + (left.tv_nsec + 999999) / 1000000);
}

#if PF_OS_WINDOWS

static pf_once_flag pf__wsa_once = PF_ONCE_FLAG_INIT;
static int pf__wsa_rc;

static void pf__wsa_start(void) {
    WSADATA data;
    int err = WSAStartup(MAKEWORD(2, 2), &data);
    pf__wsa_rc = err ? pf_error_from_wsa(err) : PF_OK;
}

static int pf__net_init(void) {
    pf_call_once(&pf__wsa_once, pf__wsa_start);
    return pf__wsa_rc;
}

static int pf__neterr(void) {
    return pf_error_from_wsa(WSAGetLastError());
}

    #define PF__SOCK(s) ((SOCKET)(s))

#else

static int pf__net_init(void) {
    return PF_OK;
}

static int pf__neterr(void) {
    return pf_error_from_errno(errno);
}

    #define PF__SOCK(s) (s)

#endif

#define PF__INIT()                                                          \
    do {                                                                    \
        int pf__rc = pf__net_init();                                        \
        if (pf__rc)                                                         \
            return pf__rc;                                                  \
    } while (0)

/* ---- Addresses --------------------------------------------------------- */

static void pf__set_sa_len(pf_sockaddr_t *a) {
#if PF_OS_APPLE || PF_OS_BSD
    a->u.sa.sa_len = (unsigned char)a->len;
#else
    (void)a;
#endif
}

static int pf__inet(pf_sockaddr_t *a, int family, unsigned port, bool loop) {
    if (!a || port > 65535)
        return PF_EINVAL;
    memset(a, 0, sizeof(*a));
    if (family == AF_INET) {
        a->u.in4.sin_family = AF_INET;
        a->u.in4.sin_port = htons((unsigned short)port);
        a->u.in4.sin_addr.s_addr = htonl(loop ? INADDR_LOOPBACK : INADDR_ANY);
        a->len = (pf_socklen_t)sizeof(a->u.in4);
    } else if (family == AF_INET6) {
        a->u.in6.sin6_family = AF_INET6;
        a->u.in6.sin6_port = htons((unsigned short)port);
        a->u.in6.sin6_addr = loop ? in6addr_loopback : in6addr_any;
        a->len = (pf_socklen_t)sizeof(a->u.in6);
    } else {
        return PF_EAFNOSUPPORT;
    }
    pf__set_sa_len(a);
    return PF_OK;
}

PF_API int pf_sockaddr_any(pf_sockaddr_t *a, int family, unsigned port) {
    return pf__inet(a, family, port, false);
}

PF_API int pf_sockaddr_loopback(pf_sockaddr_t *a, int family, unsigned port) {
    return pf__inet(a, family, port, true);
}

PF_API int pf_sockaddr_ip(pf_sockaddr_t *a, const char *ip, unsigned port) {
    int rc;
    if (!a || !ip || port > 65535)
        return PF_EINVAL;
    rc = pf_resolve(ip, NULL, AF_UNSPEC, SOCK_STREAM, PF_RESOLVE_NUMERIC, a, 1);
    if (rc < 0)
        return rc == PF_ENOENT ? PF_EINVAL : rc;
    return pf_sockaddr_set_port(a, port);
}

PF_API int pf_sockaddr_unix(pf_sockaddr_t *a, const char *path) {
    size_t n;
    if (!a || !path || !*path)
        return PF_EINVAL;
    memset(a, 0, sizeof(*a));
    a->u.un.sun_family = AF_UNIX;
    n = strlen(path);
    if (path[0] == '@') {
#if PF_OS_LINUX || PF_OS_ANDROID
        if (n > sizeof(a->u.un.sun_path))
            return PF_ENAMETOOLONG;
        memcpy(a->u.un.sun_path + 1, path + 1, n - 1);
        a->len = (pf_socklen_t)(PF__UN_OFFSET + n);
        return PF_OK;
#else
        return PF_ENOSYS;
#endif
    }
    if (n >= sizeof(a->u.un.sun_path))
        return PF_ENAMETOOLONG;
    memcpy(a->u.un.sun_path, path, n + 1);
    a->len = (pf_socklen_t)(PF__UN_OFFSET + n + 1);
    pf__set_sa_len(a);
    return PF_OK;
}

PF_API int pf_sockaddr_family(const pf_sockaddr_t *a) {
    return a ? a->u.sa.sa_family : AF_UNSPEC;
}

PF_API int pf_sockaddr_port(const pf_sockaddr_t *a) {
    if (!a)
        return PF_EINVAL;
    if (a->u.sa.sa_family == AF_INET)
        return ntohs(a->u.in4.sin_port);
    if (a->u.sa.sa_family == AF_INET6)
        return ntohs(a->u.in6.sin6_port);
    return PF_EAFNOSUPPORT;
}

PF_API int pf_sockaddr_set_port(pf_sockaddr_t *a, unsigned port) {
    if (!a || port > 65535)
        return PF_EINVAL;
    if (a->u.sa.sa_family == AF_INET)
        a->u.in4.sin_port = htons((unsigned short)port);
    else if (a->u.sa.sa_family == AF_INET6)
        a->u.in6.sin6_port = htons((unsigned short)port);
    else
        return PF_EAFNOSUPPORT;
    return PF_OK;
}

/* The bytes of an AF_UNIX path (up to the NUL for a normal path, all of
   them for an abstract one). */
static size_t pf__un_len(const pf_sockaddr_t *a) {
    size_t max;
    if ((size_t)a->len <= PF__UN_OFFSET)
        return 0;
    max = (size_t)a->len - PF__UN_OFFSET;
    if (max > sizeof(a->u.un.sun_path))
        max = sizeof(a->u.un.sun_path);
    if (a->u.un.sun_path[0] == 0)
        return max;
    {
        const char *nul = (const char *)memchr(a->u.un.sun_path, 0, max);
        return nul ? (size_t)(nul - a->u.un.sun_path) : max;
    }
}

PF_API bool pf_sockaddr_equal(const pf_sockaddr_t *a, const pf_sockaddr_t *b) {
    if (!a || !b || a->u.sa.sa_family != b->u.sa.sa_family)
        return false;
    switch (a->u.sa.sa_family) {
    case AF_INET:
        return a->u.in4.sin_port == b->u.in4.sin_port
            && a->u.in4.sin_addr.s_addr == b->u.in4.sin_addr.s_addr;
    case AF_INET6:
        return a->u.in6.sin6_port == b->u.in6.sin6_port
            && a->u.in6.sin6_scope_id == b->u.in6.sin6_scope_id
            && memcmp(&a->u.in6.sin6_addr, &b->u.in6.sin6_addr, 16) == 0;
    case AF_UNIX: {
        size_t n = pf__un_len(a);
        return n == pf__un_len(b)
            && memcmp(a->u.un.sun_path, b->u.un.sun_path, n) == 0;
    }
    default:
        return a->len == b->len
            && memcmp(&a->u, &b->u, (size_t)a->len) == 0;
    }
}

PF_API ptrdiff_t pf_sockaddr_format(
    const pf_sockaddr_t *a, char *buf, size_t size
) {
    char tmp[160];
    size_t len;

    if (!a)
        return PF_EINVAL;
    switch (a->u.sa.sa_family) {
    case AF_INET: {
        char ip[INET_ADDRSTRLEN];
        if (!pf_inet_ntop(AF_INET, &a->u.in4.sin_addr, ip, sizeof(ip)))
            return PF_EINVAL;
        len = (size_t)snprintf(
            tmp, sizeof(tmp), "%s:%u", ip, (unsigned)ntohs(a->u.in4.sin_port)
        );
        break;
    }
    case AF_INET6: {
        char ip[INET6_ADDRSTRLEN];
        char scope[16] = "";
        if (!pf_inet_ntop(AF_INET6, &a->u.in6.sin6_addr, ip, sizeof(ip)))
            return PF_EINVAL;
        if (a->u.in6.sin6_scope_id)
            snprintf(
                scope, sizeof(scope), "%%%lu",
                (unsigned long)a->u.in6.sin6_scope_id
            );
        len = (size_t)snprintf(
            tmp, sizeof(tmp), "[%s%s]:%u", ip, scope,
            (unsigned)ntohs(a->u.in6.sin6_port)
        );
        break;
    }
    case AF_UNIX: {
        size_t n = pf__un_len(a);
        if (n && a->u.un.sun_path[0] == 0) {
            tmp[0] = '@';
            memcpy(tmp + 1, a->u.un.sun_path + 1, n - 1);
            len = n;
        } else {
            memcpy(tmp, a->u.un.sun_path, n);
            len = n;
        }
        tmp[len] = 0;
        break;
    }
    default:
        return PF_EAFNOSUPPORT;
    }
    return pf__copy_out(buf, size, tmp, len);
}

/* ---- Creating sockets -------------------------------------------------- */

#if PF_OS_WINDOWS

static int pf__win_nonblock(SOCKET s, bool on) {
    u_long mode = on ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0 ? PF_OK : pf__neterr();
}

/* Not inherited, and blocking or not as `flags` says. */
static int pf__setup(SOCKET s, unsigned flags, bool fresh) {
    SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
    if (fresh && !(flags & PF_SOCK_NONBLOCK))
        return PF_OK;
    return pf__win_nonblock(s, (flags & PF_SOCK_NONBLOCK) != 0);
}

#else

/* Close-on-exec, no SIGPIPE, and blocking or not as `flags` says. `atomic`:
   the socket was made with SOCK_CLOEXEC / SOCK_NONBLOCK already. */
static int pf__setup(int fd, unsigned flags, bool atomic) {
    if (!atomic) {
        int fl = fcntl(fd, F_GETFD);
        if (fl < 0 || fcntl(fd, F_SETFD, fl | FD_CLOEXEC) < 0)
            return pf__neterr();
        fl = fcntl(fd, F_GETFL);
        if (fl < 0)
            return pf__neterr();
        fl = (flags & PF_SOCK_NONBLOCK) ? (fl | O_NONBLOCK)
                                        : (fl & ~O_NONBLOCK);
        if (fcntl(fd, F_SETFL, fl) < 0)
            return pf__neterr();
    }
    #ifdef SO_NOSIGPIPE
    {
        int one = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) != 0)
            return pf__neterr();
    }
    #endif
    return PF_OK;
}

#endif

PF_API int pf_socket(
    pf_socket_t *s, int family, int type, int protocol, unsigned flags
) {
    int rc;
    if (!s)
        return PF_EINVAL;
    *s = PF_SOCKET_INVALID;
    PF__INIT();
#if PF_OS_WINDOWS
    {
        SOCKET h = WSASocketW(
            family, type, protocol, NULL, 0,
            WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT
        );
        if (h == INVALID_SOCKET)
            return pf__neterr();
        rc = pf__setup(h, flags, true);
        if (rc) {
            closesocket(h);
            return rc;
        }
        *s = (pf_socket_t)h;
    }
#else
    {
        int fd;
        bool atomic = false;
    #if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
        fd = socket(
            family,
            type | SOCK_CLOEXEC | ((flags & PF_SOCK_NONBLOCK) ? SOCK_NONBLOCK : 0),
            protocol
        );
        atomic = true;
    #else
        fd = socket(family, type, protocol);
    #endif
        if (fd < 0)
            return pf__neterr();
        rc = pf__setup(fd, flags, atomic);
        if (rc) {
            close(fd);
            return rc;
        }
        *s = fd;
    }
#endif
    return PF_OK;
}

#if PF_OS_WINDOWS

/* A connected pair through a listening socket on `addr` (an AF_UNIX path
   or the loopback address with port 0). */
static int pf__pair_via(pf_socket_t s[2], pf_sockaddr_t *addr) {
    pf_socket_t lst = PF_SOCKET_INVALID, a = PF_SOCKET_INVALID;
    pf_socket_t b = PF_SOCKET_INVALID;
    int family = addr->u.sa.sa_family;
    int rc;

    rc = pf_socket(&lst, family, SOCK_STREAM, 0, 0);
    if (!rc)
        rc = pf_bind(lst, addr);
    if (!rc && family != AF_UNIX)
        rc = pf_getsockname(lst, addr);
    if (!rc)
        rc = pf_listen(lst, 1);
    if (!rc)
        rc = pf_socket(&a, family, SOCK_STREAM, 0, 0);
    if (!rc)
        rc = pf_connect(a, addr, NULL);
    if (!rc)
        rc = pf_accept(lst, &b, NULL, 0);
    if (!rc && family != AF_UNIX) {
        /* Another local program may have connected first. */
        pf_sockaddr_t mine, peer;
        rc = pf_getsockname(a, &mine);
        if (!rc)
            rc = pf_getpeername(b, &peer);
        if (!rc && !pf_sockaddr_equal(&mine, &peer))
            rc = PF_ECONNREFUSED;
    }
    pf_socket_close(lst);
    if (rc) {
        pf_socket_close(a);
        pf_socket_close(b);
        return rc;
    }
    s[0] = a;
    s[1] = b;
    return PF_OK;
}

static int pf__pair_unix(pf_socket_t s[2]) {
    char dir[PF_PATH_MAX], path[PF_PATH_MAX];
    pf_sockaddr_t addr;
    int rc;

    if (pf_known_dir(PF_DIR_TEMP, dir, sizeof(dir)) < 0)
        return PF_ENOENT;
    snprintf(
        path, sizeof(path), "%s\\pf-%lu-%08lx.sock", dir,
        (unsigned long)pf_getpid(), (unsigned long)pf_random_u32()
    );
    rc = pf_sockaddr_unix(&addr, path);
    if (rc)
        return rc;
    rc = pf__pair_via(s, &addr);
    pf_unlink(path);
    return rc;
}

#endif

PF_API int pf_socketpair(
    pf_socket_t s[2], int family, int type, unsigned flags
) {
    if (!s)
        return PF_EINVAL;
    s[0] = s[1] = PF_SOCKET_INVALID;
    PF__INIT();
#if PF_OS_WINDOWS
    {
        pf_sockaddr_t addr;
        int rc;
        if (family != AF_UNIX || type != SOCK_STREAM)
            return PF_ENOSYS;
        rc = pf__pair_unix(s);
        if (rc) {
            pf__inet(&addr, AF_INET, 0, true);
            rc = pf__pair_via(s, &addr);
        }
        if (!rc && (flags & PF_SOCK_NONBLOCK)) {
            rc = pf_socket_set_nonblocking(s[0], true);
            if (!rc)
                rc = pf_socket_set_nonblocking(s[1], true);
            if (rc) {
                pf_socket_close(s[0]);
                pf_socket_close(s[1]);
                s[0] = s[1] = PF_SOCKET_INVALID;
            }
        }
        return rc;
    }
#else
    {
        int fds[2], rc;
        bool atomic = false;
    #if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
        if (socketpair(
                family,
                type | SOCK_CLOEXEC
                    | ((flags & PF_SOCK_NONBLOCK) ? SOCK_NONBLOCK : 0),
                0, fds
            )
            != 0)
            return pf__neterr();
        atomic = true;
    #else
        if (socketpair(family, type, 0, fds) != 0)
            return pf__neterr();
    #endif
        rc = pf__setup(fds[0], flags, atomic);
        if (!rc)
            rc = pf__setup(fds[1], flags, atomic);
        if (rc) {
            close(fds[0]);
            close(fds[1]);
            return rc;
        }
        s[0] = fds[0];
        s[1] = fds[1];
        return PF_OK;
    }
#endif
}

PF_API int pf_bind(pf_socket_t s, const pf_sockaddr_t *addr) {
    if (!addr)
        return PF_EINVAL;
    PF__INIT();
    if (bind(PF__SOCK(s), &addr->u.sa, addr->len) != 0)
        return pf__neterr();
    return PF_OK;
}

PF_API int pf_listen(pf_socket_t s, int backlog) {
    PF__INIT();
    if (listen(PF__SOCK(s), backlog > 0 ? backlog : SOMAXCONN) != 0)
        return pf__neterr();
    return PF_OK;
}

PF_API int pf_accept(
    pf_socket_t s, pf_socket_t *out, pf_sockaddr_t *peer, unsigned flags
) {
    pf_sockaddr_t tmp;
    pf_sockaddr_t *pa = peer ? peer : &tmp;
    int rc;

    if (!out)
        return PF_EINVAL;
    *out = PF_SOCKET_INVALID;
    PF__INIT();
    memset(pa, 0, sizeof(*pa));
    pa->len = (pf_socklen_t)sizeof(pa->u);
#if PF_OS_WINDOWS
    {
        SOCKET h = accept(PF__SOCK(s), &pa->u.sa, &pa->len);
        if (h == INVALID_SOCKET)
            return pf__neterr();
        rc = pf__setup(h, flags, false);
        if (rc) {
            closesocket(h);
            return rc;
        }
        *out = (pf_socket_t)h;
    }
#else
    {
        int fd;
        bool atomic;
        for (;;) {
    #if PF__HAVE_ACCEPT4
            fd = accept4(
                s, &pa->u.sa, &pa->len,
                SOCK_CLOEXEC | ((flags & PF_SOCK_NONBLOCK) ? SOCK_NONBLOCK : 0)
            );
            atomic = true;
    #else
            fd = accept(s, &pa->u.sa, &pa->len);
            atomic = false;
    #endif
            if (fd >= 0 || errno != EINTR)
                break;
        }
        if (fd < 0)
            return pf__neterr();
        rc = pf__setup(fd, flags, atomic);
        if (rc) {
            close(fd);
            return rc;
        }
        *out = fd;
    }
#endif
    return PF_OK;
}

/* Waits for `events` (POLLIN, POLLOUT) on one socket: the events, or
   PF_ETIMEDOUT. Windows uses select, which reports failed connects. */
static int pf__wait_one(
    pf_socket_t s, int events, const struct timespec *deadline
) {
#if PF_OS_WINDOWS
    for (;;) {
        fd_set rd, wr, ex;
        struct timeval tv, *ptv = NULL;
        int ms = pf__deadline_ms(deadline);
        int r, got = 0;

        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&ex);
        if (events & POLLIN)
            FD_SET(PF__SOCK(s), &rd);
        if (events & POLLOUT)
            FD_SET(PF__SOCK(s), &wr);
        FD_SET(PF__SOCK(s), &ex);
        if (ms >= 0) {
            tv.tv_sec = ms / 1000;
            tv.tv_usec = (ms % 1000) * 1000;
            ptv = &tv;
        }
        r = select(0, &rd, &wr, &ex, ptv);
        if (r < 0)
            return pf__neterr();
        if (r == 0) {
            if (ms == 0 || pf__deadline_ms(deadline) == 0)
                return PF_ETIMEDOUT;
            continue;
        }
        if (FD_ISSET(PF__SOCK(s), &rd))
            got |= POLLIN;
        if (FD_ISSET(PF__SOCK(s), &wr))
            got |= POLLOUT;
        if (FD_ISSET(PF__SOCK(s), &ex))
            got |= POLLERR;
        return got;
    }
#else
    pf_pollfd_t p;
    int r;
    p.fd = s;
    p.events = (short)events;
    p.revents = 0;
    r = pf_poll(&p, 1, deadline);
    if (r < 0)
        return r;
    return r == 0 ? PF_ETIMEDOUT : p.revents;
#endif
}

PF_API int pf_poll(
    pf_pollfd_t *fds, size_t n, const struct timespec *deadline
) {
    PF__INIT();
#if PF_OS_WINDOWS
    if (n > ULONG_MAX)
        return PF_EINVAL;
#else
    if (n > (nfds_t)-1)
        return PF_EINVAL;
#endif
    for (;;) {
        int ms = pf__deadline_ms(deadline);
        int r;
#if PF_OS_WINDOWS
        r = WSAPoll(fds, (ULONG)n, ms);
        if (r < 0)
            return pf__neterr();
#else
        r = poll(fds, (nfds_t)n, ms);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return pf__neterr();
        }
#endif
        /* poll may wake a little early, or the deadline may be further
           than INT_MAX milliseconds away. */
        if (r == 0 && ms != 0 && pf__deadline_ms(deadline) != 0)
            continue;
        return r;
    }
}

PF_API int pf_socket_wait(
    pf_socket_t s, int events, const struct timespec *deadline
) {
    PF__INIT();
    return pf__wait_one(s, events, deadline);
}

PF_API int pf_socket_error(pf_socket_t s) {
    int err = 0;
    pf_socklen_t len = (pf_socklen_t)sizeof(err);
    int rc = pf_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
    if (rc)
        return rc;
    if (!err)
        return PF_OK;
#if PF_OS_WINDOWS
    return pf_error_from_wsa(err);
#else
    return pf_error_from_errno(err);
#endif
}

/* Whether the last connect() is still going on. */
static bool pf__in_progress(void) {
#if PF_OS_WINDOWS
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EINPROGRESS || errno == EINTR;
#endif
}

PF_API int pf_connect(
    pf_socket_t s, const pf_sockaddr_t *addr, const struct timespec *deadline
) {
    int rc;
    if (!addr)
        return PF_EINVAL;
    PF__INIT();
    if (!deadline) {
        if (connect(PF__SOCK(s), &addr->u.sa, addr->len) == 0)
            return PF_OK;
#if PF_OS_WINDOWS
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return PF_EINPROGRESS;
        return pf__neterr();
#else
        if (errno != EINTR)
            return pf__neterr();
        /* Interrupted: the connection goes on in the background. */
        rc = pf__wait_one(s, POLLOUT, NULL);
        return rc < 0 ? rc : pf_socket_error(s);
#endif
    }

    rc = pf_socket_set_nonblocking(s, true);
    if (rc)
        return rc;
    if (connect(PF__SOCK(s), &addr->u.sa, addr->len) == 0) {
        rc = PF_OK;
    } else if (pf__in_progress()) {
        rc = pf__wait_one(s, POLLOUT, deadline);
        if (rc >= 0)
            rc = pf_socket_error(s);
    } else {
        rc = pf__neterr();
    }
    {
        int rc2 = pf_socket_set_nonblocking(s, false);
        return rc ? rc : rc2;
    }
}

/* ---- Data -------------------------------------------------------------- */

#if PF_OS_WINDOWS
/* A failed send or recv: a timeout (SO_RCVTIMEO, SO_SNDTIMEO) is PF_EAGAIN
   as on POSIX. */
static int pf__io_err(void) {
    int e = WSAGetLastError();
    return e == WSAETIMEDOUT ? PF_EAGAIN : pf_error_from_wsa(e);
}

static int pf__cap(size_t n) {
    return n > INT_MAX ? INT_MAX : (int)n;
}
#endif

#ifdef MSG_NOSIGNAL
    #define PF__NOSIGNAL MSG_NOSIGNAL
#else
    #define PF__NOSIGNAL 0
#endif

PF_API pf_ssize_t pf_send(pf_socket_t s, const void *buf, size_t n, int flags) {
    return pf_sendto(s, buf, n, flags, NULL);
}

PF_API pf_ssize_t pf_recv(pf_socket_t s, void *buf, size_t n, int flags) {
    return pf_recvfrom(s, buf, n, flags, NULL);
}

PF_API pf_ssize_t pf_sendto(
    pf_socket_t s,
    const void *buf,
    size_t n,
    int flags,
    const pf_sockaddr_t *to
) {
    const struct sockaddr *sa = to ? &to->u.sa : NULL;
    pf_socklen_t len = to ? to->len : 0;
    PF__INIT();
#if PF_OS_WINDOWS
    {
        int r = sendto(PF__SOCK(s), (const char *)buf, pf__cap(n), flags, sa, len);
        return r == SOCKET_ERROR ? pf__io_err() : r;
    }
#else
    for (;;) {
        ssize_t r = sendto(s, buf, n, flags | PF__NOSIGNAL, sa, len);
        if (r >= 0)
            return r;
        if (errno != EINTR)
            return pf__neterr();
    }
#endif
}

PF_API pf_ssize_t pf_recvfrom(
    pf_socket_t s, void *buf, size_t n, int flags, pf_sockaddr_t *from
) {
    struct sockaddr *sa = NULL;
    pf_socklen_t *plen = NULL;
    PF__INIT();
    if (from) {
        memset(from, 0, sizeof(*from));
        from->len = (pf_socklen_t)sizeof(from->u);
        sa = &from->u.sa;
        plen = &from->len;
    }
#if PF_OS_WINDOWS
    {
        int r = recvfrom(PF__SOCK(s), (char *)buf, pf__cap(n), flags, sa, plen);
        return r == SOCKET_ERROR ? pf__io_err() : r;
    }
#else
    for (;;) {
        ssize_t r = recvfrom(s, buf, n, flags, sa, plen);
        if (r >= 0)
            return r;
        if (errno != EINTR)
            return pf__neterr();
    }
#endif
}

PF_API int pf_shutdown(pf_socket_t s, int how) {
    PF__INIT();
    if (how < PF_SHUT_RD || how > PF_SHUT_RDWR)
        return PF_EINVAL;
    if (shutdown(PF__SOCK(s), how) != 0)
        return pf__neterr();
    return PF_OK;
}

PF_API int pf_socket_close(pf_socket_t s) {
    if (s == PF_SOCKET_INVALID)
        return PF_OK;
#if PF_OS_WINDOWS
    PF__INIT();
    if (closesocket(PF__SOCK(s)) != 0)
        return pf__neterr();
#else
    /* The descriptor is gone even when close is interrupted. */
    if (close(s) != 0 && errno != EINTR)
        return pf__neterr();
#endif
    return PF_OK;
}

PF_API int pf_getsockname(pf_socket_t s, pf_sockaddr_t *addr) {
    if (!addr)
        return PF_EINVAL;
    PF__INIT();
    memset(addr, 0, sizeof(*addr));
    addr->len = (pf_socklen_t)sizeof(addr->u);
    if (getsockname(PF__SOCK(s), &addr->u.sa, &addr->len) != 0)
        return pf__neterr();
    return PF_OK;
}

PF_API int pf_getpeername(pf_socket_t s, pf_sockaddr_t *addr) {
    if (!addr)
        return PF_EINVAL;
    PF__INIT();
    memset(addr, 0, sizeof(*addr));
    addr->len = (pf_socklen_t)sizeof(addr->u);
    if (getpeername(PF__SOCK(s), &addr->u.sa, &addr->len) != 0)
        return pf__neterr();
    return PF_OK;
}

PF_API int pf_getsockopt(
    pf_socket_t s, int level, int name, void *val, pf_socklen_t *len
) {
    PF__INIT();
    if (getsockopt(PF__SOCK(s), level, name, (char *)val, len) != 0)
        return pf__neterr();
    return PF_OK;
}

PF_API int pf_setsockopt(
    pf_socket_t s, int level, int name, const void *val, pf_socklen_t len
) {
    PF__INIT();
    if (setsockopt(PF__SOCK(s), level, name, (const char *)val, len) != 0)
        return pf__neterr();
    return PF_OK;
}

/* ---- Options ----------------------------------------------------------- */

static int pf__opt_int(pf_socket_t s, int level, int name, int value) {
    return pf_setsockopt(s, level, name, &value, (pf_socklen_t)sizeof(value));
}

PF_API int pf_socket_set_nonblocking(pf_socket_t s, bool on) {
    PF__INIT();
#if PF_OS_WINDOWS
    return pf__win_nonblock(PF__SOCK(s), on);
#else
    {
        int fl = fcntl(s, F_GETFL);
        if (fl < 0)
            return pf__neterr();
        fl = on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK);
        if (fcntl(s, F_SETFL, fl) < 0)
            return pf__neterr();
        return PF_OK;
    }
#endif
}

PF_API int pf_socket_set_nodelay(pf_socket_t s, bool on) {
    return pf__opt_int(s, IPPROTO_TCP, TCP_NODELAY, on);
}

PF_API int pf_socket_set_keepalive(pf_socket_t s, bool on) {
    return pf__opt_int(s, SOL_SOCKET, SO_KEEPALIVE, on);
}

PF_API int pf_socket_set_reuseaddr(pf_socket_t s, bool on) {
#if PF_OS_WINDOWS
    (void)s, (void)on;
    return PF_OK;
#else
    return pf__opt_int(s, SOL_SOCKET, SO_REUSEADDR, on);
#endif
}

PF_API int pf_socket_set_reuseport(pf_socket_t s, bool on) {
#if defined(SO_REUSEPORT) && !PF_OS_WINDOWS
    return pf__opt_int(s, SOL_SOCKET, SO_REUSEPORT, on);
#else
    (void)s, (void)on;
    return PF_ENOSYS;
#endif
}

PF_API int pf_socket_set_ipv6only(pf_socket_t s, bool on) {
    return pf__opt_int(s, IPPROTO_IPV6, IPV6_V6ONLY, on);
}

PF_API int pf_socket_set_linger(pf_socket_t s, bool on, int seconds) {
    struct linger l;
    if (seconds < 0 || seconds > 65535)
        return PF_EINVAL;
    memset(&l, 0, sizeof(l));
#if PF_OS_WINDOWS
    l.l_onoff = (u_short)(on ? 1 : 0);
    l.l_linger = (u_short)seconds;
#else
    l.l_onoff = on ? 1 : 0;
    l.l_linger = seconds;
#endif
    return pf_setsockopt(s, SOL_SOCKET, SO_LINGER, &l, (pf_socklen_t)sizeof(l));
}

static int pf__opt_timeout(pf_socket_t s, int name, unsigned ms) {
#if PF_OS_WINDOWS
    DWORD v = ms;
    return pf_setsockopt(s, SOL_SOCKET, name, &v, (pf_socklen_t)sizeof(v));
#else
    struct timeval tv;
    tv.tv_sec = (time_t)(ms / 1000);
    tv.tv_usec = (suseconds_t)((ms % 1000) * 1000);
    return pf_setsockopt(s, SOL_SOCKET, name, &tv, (pf_socklen_t)sizeof(tv));
#endif
}

PF_API int pf_socket_set_recv_timeout(pf_socket_t s, unsigned ms) {
    return pf__opt_timeout(s, SO_RCVTIMEO, ms);
}

PF_API int pf_socket_set_send_timeout(pf_socket_t s, unsigned ms) {
    return pf__opt_timeout(s, SO_SNDTIMEO, ms);
}

PF_API int pf_socket_set_recv_buffer(pf_socket_t s, int size) {
    return pf__opt_int(s, SOL_SOCKET, SO_RCVBUF, size);
}

PF_API int pf_socket_set_send_buffer(pf_socket_t s, int size) {
    return pf__opt_int(s, SOL_SOCKET, SO_SNDBUF, size);
}

/* ---- Names ------------------------------------------------------------- */

/* An EAI_* code as a PF_E* code. On Windows the EAI_* values are Winsock
   errors, and EAI_NODATA may equal EAI_NONAME: no switch. */
static int pf__gai_error(int e) {
    if (e == 0)
        return PF_OK;
    if (e == EAI_NONAME)
        return PF_ENOENT;
#ifdef EAI_NODATA
    if (e == EAI_NODATA)
        return PF_ENOENT;
#endif
#ifdef EAI_ADDRFAMILY
    if (e == EAI_ADDRFAMILY)
        return PF_ENOENT;
#endif
    if (e == EAI_AGAIN)
        return PF_EAGAIN;
    if (e == EAI_FAIL)
        return PF_EIO;
    if (e == EAI_MEMORY)
        return PF_ENOMEM;
    if (e == EAI_FAMILY)
        return PF_EAFNOSUPPORT;
    if (e == EAI_SERVICE || e == EAI_SOCKTYPE || e == EAI_BADFLAGS)
        return PF_EINVAL;
#ifdef EAI_OVERFLOW
    if (e == EAI_OVERFLOW)
        return PF_ERANGE;
#endif
#if PF_OS_WINDOWS
    return pf_error_from_wsa(e);
#else
    #ifdef EAI_SYSTEM
    if (e == EAI_SYSTEM)
        return errno ? pf_error_from_errno(errno) : PF_EUNKNOWN;
    #endif
    return PF_EUNKNOWN;
#endif
}

#if PF_OS_WINDOWS

/* UTF-16 to UTF-8 into new memory (malloc): NULL on failure. */
static char *pf__utf8_dup(const wchar_t *w) {
    ptrdiff_t n = pf_utf16_to_utf8((const uint16_t *)w, PF_NTS, NULL, 0, 0);
    char *s;
    if (n < 0)
        return NULL;
    s = (char *)malloc((size_t)n + 1);
    if (s && pf_utf16_to_utf8((const uint16_t *)w, PF_NTS, s, (size_t)n + 1, 0) < 0) {
        free(s);
        s = NULL;
    }
    return s;
}

/* Our struct addrinfo list: each node is one malloc block with its address
   and canonical name after it. */
static struct addrinfo *pf__ai_from_wide(const ADDRINFOW *w, int *rc) {
    struct addrinfo *head = NULL, **tail = &head;
    *rc = PF_OK;
    for (; w; w = w->ai_next) {
        char *canon = NULL;
        size_t canon_len = 0, size;
        struct addrinfo *ai;

        if (w->ai_canonname) {
            canon = pf__utf8_dup(w->ai_canonname);
            if (!canon) {
                *rc = PF_ENOMEM;
                break;
            }
            canon_len = strlen(canon) + 1;
        }
        size = sizeof(*ai) + w->ai_addrlen + canon_len;
        ai = (struct addrinfo *)calloc(1, size);
        if (!ai) {
            free(canon);
            *rc = PF_ENOMEM;
            break;
        }
        ai->ai_flags = w->ai_flags;
        ai->ai_family = w->ai_family;
        ai->ai_socktype = w->ai_socktype;
        ai->ai_protocol = w->ai_protocol;
        ai->ai_addrlen = w->ai_addrlen;
        ai->ai_addr = (struct sockaddr *)(ai + 1);
        memcpy(ai->ai_addr, w->ai_addr, w->ai_addrlen);
        if (canon) {
            ai->ai_canonname = (char *)ai->ai_addr + w->ai_addrlen;
            memcpy(ai->ai_canonname, canon, canon_len);
            free(canon);
        }
        *tail = ai;
        tail = &ai->ai_next;
    }
    if (*rc) {
        pf_freeaddrinfo(head);
        head = NULL;
    }
    return head;
}

#endif

PF_API int pf_getaddrinfo(
    const char *host,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res
) {
    if (!res || (!host && !service))
        return PF_EINVAL;
    *res = NULL;
    PF__INIT();
#if PF_OS_WINDOWS
    {
        pf__wstr wh, ws;
        ADDRINFOW whints, *wres = NULL;
        int rc, e;

        rc = host ? pf__wstr_from_utf8(&wh, host) : PF_OK;
        if (rc)
            return rc;
        rc = service ? pf__wstr_from_utf8(&ws, service) : PF_OK;
        if (rc) {
            if (host)
                pf__wstr_free(&wh);
            return rc;
        }
        memset(&whints, 0, sizeof(whints));
        if (hints) {
            whints.ai_flags = hints->ai_flags;
            whints.ai_family = hints->ai_family;
            whints.ai_socktype = hints->ai_socktype;
            whints.ai_protocol = hints->ai_protocol;
        }
        e = GetAddrInfoW(
            host ? (const wchar_t *)wh.ptr : NULL,
            service ? (const wchar_t *)ws.ptr : NULL,
            hints ? &whints : NULL, &wres
        );
        if (host)
            pf__wstr_free(&wh);
        if (service)
            pf__wstr_free(&ws);
        if (e)
            return pf__gai_error(e);
        *res = pf__ai_from_wide(wres, &rc);
        FreeAddrInfoW(wres);
        return rc;
    }
#else
    {
        int e;
        errno = 0;
        e = getaddrinfo(host, service, hints, res);
        if (e) {
            *res = NULL;
            return pf__gai_error(e);
        }
        return PF_OK;
    }
#endif
}

PF_API void pf_freeaddrinfo(struct addrinfo *res) {
#if PF_OS_WINDOWS
    while (res) {
        struct addrinfo *next = res->ai_next;
        free(res);
        res = next;
    }
#else
    if (res)
        freeaddrinfo(res);
#endif
}

PF_API int pf_getnameinfo(
    const pf_sockaddr_t *a,
    char *host,
    size_t hostlen,
    char *serv,
    size_t servlen,
    int flags
) {
    if (!a)
        return PF_EINVAL;
    PF__INIT();
#if PF_OS_WINDOWS
    {
        wchar_t wh[NI_MAXHOST], ws[NI_MAXSERV];
        int e = GetNameInfoW(
            &a->u.sa, a->len, host ? wh : NULL, host ? NI_MAXHOST : 0,
            serv ? ws : NULL, serv ? NI_MAXSERV : 0, flags
        );
        if (e)
            return pf__gai_error(e);
        if (host && hostlen
            && pf_utf16_to_utf8((const uint16_t *)wh, PF_NTS, host, hostlen, 0) < 0)
            return PF_ERANGE;
        if (serv && servlen
            && pf_utf16_to_utf8((const uint16_t *)ws, PF_NTS, serv, servlen, 0) < 0)
            return PF_ERANGE;
        return PF_OK;
    }
#else
    {
        int e;
        if (hostlen > (socklen_t)-1 || servlen > (socklen_t)-1)
            return PF_EINVAL;
        errno = 0;
        e = getnameinfo(
            &a->u.sa, a->len, host, (socklen_t)(host ? hostlen : 0), serv,
            (socklen_t)(serv ? servlen : 0), flags
        );
        return pf__gai_error(e);
    }
#endif
}

PF_API int pf_resolve(
    const char *host,
    const char *service,
    int family,
    int type,
    unsigned flags,
    pf_sockaddr_t *out,
    size_t max
) {
    struct addrinfo hints, *res = NULL, *ai;
    size_t count = 0;
    int rc;

    if (!out || max == 0)
        return PF_EINVAL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = type;
    if (flags & PF_RESOLVE_PASSIVE)
        hints.ai_flags |= AI_PASSIVE;
    if (flags & PF_RESOLVE_NUMERIC)
        hints.ai_flags |= AI_NUMERICHOST;
    if (!host && !service)
        service = "0";
    rc = pf_getaddrinfo(host, service, &hints, &res);
    if (rc)
        return rc;
    for (ai = res; ai && count < max; ai = ai->ai_next) {
        pf_sockaddr_t a;
        size_t i;
        bool dup = false;
        if ((size_t)ai->ai_addrlen > sizeof(a.u))
            continue;
        memset(&a, 0, sizeof(a));
        memcpy(&a.u, ai->ai_addr, (size_t)ai->ai_addrlen);
        a.len = (pf_socklen_t)ai->ai_addrlen;
        /* Without a socket type, each address comes once per type. */
        for (i = 0; i < count && !dup; i++)
            dup = pf_sockaddr_equal(&out[i], &a);
        if (!dup)
            out[count++] = a;
    }
    pf_freeaddrinfo(res);
    if (count == 0)
        return PF_ENOENT;
    return count > INT_MAX ? INT_MAX : (int)count;
}

PF_API int pf_inet_pton(int family, const char *src, void *dst) {
#if PF_OS_WINDOWS
    if (pf__net_init()) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    {
        int r = inet_pton(family, src, dst);
        if (r < 0)
            errno = EAFNOSUPPORT;
        return r;
    }
#else
    return inet_pton(family, src, dst);
#endif
}

PF_API const char *pf_inet_ntop(
    int family, const void *src, char *dst, size_t size
) {
#if PF_OS_WINDOWS
    const char *r;
    if (pf__net_init()) {
        errno = EAFNOSUPPORT;
        return NULL;
    }
    r = inet_ntop(family, src, dst, size);
    if (!r)
        errno = WSAGetLastError() == WSAEAFNOSUPPORT ? EAFNOSUPPORT : ENOSPC;
    return r;
#else
    if (size > (socklen_t)-1)
        size = (socklen_t)-1;
    return inet_ntop(family, src, dst, (socklen_t)size);
#endif
}

/* ---- Shortcuts --------------------------------------------------------- */

PF_API int pf_tcp_connect(
    pf_socket_t *s,
    const char *host,
    const char *service,
    const struct timespec *deadline
) {
    pf_sockaddr_t addrs[16];
    int n, i, rc;

    if (!s)
        return PF_EINVAL;
    *s = PF_SOCKET_INVALID;
    n = pf_resolve(host, service, AF_UNSPEC, SOCK_STREAM, 0, addrs, 16);
    if (n < 0)
        return n;
    rc = PF_ECONNREFUSED;
    for (i = 0; i < n; i++) {
        pf_socket_t c;
        rc = pf_socket(&c, addrs[i].u.sa.sa_family, SOCK_STREAM, 0, 0);
        if (rc)
            continue;
        rc = pf_connect(c, &addrs[i], deadline);
        if (!rc) {
            *s = c;
            return PF_OK;
        }
        pf_socket_close(c);
        if (deadline && pf__deadline_ms(deadline) == 0)
            return PF_ETIMEDOUT;
    }
    return rc;
}

static int pf__listen_on(
    pf_socket_t *s, const pf_sockaddr_t *addr, bool dual, int backlog
) {
    pf_socket_t l;
    int rc = pf_socket(&l, addr->u.sa.sa_family, SOCK_STREAM, 0, 0);
    if (rc)
        return rc;
    rc = pf_socket_set_reuseaddr(l, true);
    if (!rc && addr->u.sa.sa_family == AF_INET6)
        rc = pf_socket_set_ipv6only(l, !dual);
    if (!rc)
        rc = pf_bind(l, addr);
    if (!rc)
        rc = pf_listen(l, backlog);
    if (rc) {
        pf_socket_close(l);
        return rc;
    }
    *s = l;
    return PF_OK;
}

PF_API int pf_tcp_listen(
    pf_socket_t *s, const char *host, const char *service, int backlog
) {
    pf_sockaddr_t addrs[16];
    int n, i, rc;

    if (!s)
        return PF_EINVAL;
    *s = PF_SOCKET_INVALID;
    if (!host) {
        /* IPv6 and IPv4 on one socket, else IPv4 alone. */
        n = pf_resolve(
            NULL, service, AF_INET6, SOCK_STREAM, PF_RESOLVE_PASSIVE, addrs, 1
        );
        if (n > 0 && pf__listen_on(s, &addrs[0], true, backlog) == PF_OK)
            return PF_OK;
        n = pf_resolve(
            NULL, service, AF_INET, SOCK_STREAM, PF_RESOLVE_PASSIVE, addrs, 1
        );
        if (n < 0)
            return n;
        return pf__listen_on(s, &addrs[0], false, backlog);
    }
    n = pf_resolve(
        host, service, AF_UNSPEC, SOCK_STREAM, PF_RESOLVE_PASSIVE, addrs, 16
    );
    if (n < 0)
        return n;
    rc = PF_EADDRNOTAVAIL;
    for (i = 0; i < n; i++) {
        rc = pf__listen_on(s, &addrs[i], false, backlog);
        if (!rc)
            return PF_OK;
    }
    return rc;
}

PF_API int pf_unix_connect(pf_socket_t *s, const char *path) {
    pf_sockaddr_t a;
    pf_socket_t c;
    int rc;

    if (!s)
        return PF_EINVAL;
    *s = PF_SOCKET_INVALID;
    rc = pf_sockaddr_unix(&a, path);
    if (!rc)
        rc = pf_socket(&c, AF_UNIX, SOCK_STREAM, 0, 0);
    if (rc)
        return rc;
    rc = pf_connect(c, &a, NULL);
    if (rc) {
        pf_socket_close(c);
        return rc;
    }
    *s = c;
    return PF_OK;
}

PF_API int pf_unix_listen(pf_socket_t *s, const char *path, int backlog) {
    pf_sockaddr_t a;
    pf_socket_t l;
    int rc;

    if (!s)
        return PF_EINVAL;
    *s = PF_SOCKET_INVALID;
    rc = pf_sockaddr_unix(&a, path);
    if (!rc)
        rc = pf_socket(&l, AF_UNIX, SOCK_STREAM, 0, 0);
    if (rc)
        return rc;
    rc = pf_bind(l, &a);
    if (!rc)
        rc = pf_listen(l, backlog);
    if (rc) {
        pf_socket_close(l);
        return rc;
    }
    *s = l;
    return PF_OK;
}

/* ---- Streams ----------------------------------------------------------- */

#define PF__CTX_SOCK(ctx) ((pf_socket_t)(uintptr_t)(ctx))

static pf_ssize_t pf__sock_read(void *ctx, void *buf, size_t n) {
    return pf_recv(PF__CTX_SOCK(ctx), buf, n, 0);
}

static pf_ssize_t pf__sock_write(void *ctx, const void *buf, size_t n) {
    return pf_send(PF__CTX_SOCK(ctx), buf, n, 0);
}

static int pf__sock_close(void *ctx) {
    return pf_socket_close(PF__CTX_SOCK(ctx));
}

static const struct pf_stream_vt pf__sock_vt = {
    pf__sock_read, pf__sock_write, NULL, NULL, NULL,
};

static const struct pf_stream_vt pf__sock_own_vt = {
    pf__sock_read, pf__sock_write, NULL, NULL, pf__sock_close,
};

PF_API int pf_stream_from_socket(pf_stream_t *st, pf_socket_t s, bool own) {
    if (s == PF_SOCKET_INVALID)
        return PF_EBADF;
    return pf_stream_init(
        st, own ? &pf__sock_own_vt : &pf__sock_vt, (void *)(uintptr_t)s
    );
}
