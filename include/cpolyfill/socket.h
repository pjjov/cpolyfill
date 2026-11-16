/** cpolyfill - Filling the gaps between C standards and compilers

    Sockets: the BSD socket API with one behaviour on every platform.

    Including this header includes the system's socket headers too
    (<sys/socket.h>, <netinet/in.h>, <netinet/tcp.h>, <arpa/inet.h>,
    <netdb.h>, <sys/un.h>, <poll.h>; <winsock2.h>, <ws2tcpip.h> and
    <afunix.h> on Windows), so AF_INET, SOCK_STREAM, IPPROTO_TCP, POLLIN,
    struct sockaddr_in and the rest come from there. On Windows the
    library defines struct sockaddr_un itself where the SDK has no
    <afunix.h>.

    Every function returns 0 (or a count) on success and a negative PF_E*
    code on failure (Winsock errors are mapped too). There is no set-up
    call: on Windows, Winsock is started on first use. Sockets are always
    created close-on-exec (not inherited by child processes), interrupted
    calls (EINTR) are restarted, and writing to a connection the peer has
    closed fails with PF_EPIPE (or PF_ECONNRESET) instead of raising
    SIGPIPE.

        pf_socket_t             A socket: `int`, `SOCKET` on Windows.
        PF_SOCKET_INVALID       No socket.
        pf_socklen_t            socklen_t; `int` on Windows.
        pf_pollfd_t             struct pollfd (WSAPOLLFD on Windows).

    Addresses:

        pf_sockaddr_t           Any socket address with its length:
                                  u.sa, u.in4, u.in6, u.un, u.storage
                                  len    the length of the address
        int pf_sockaddr_ip(pf_sockaddr_t *a, const char *ip, unsigned port)
                A numeric IPv4 or IPv6 address ("10.0.0.1", "::1",
                "fe80::1%eth0"); PF_EINVAL if `ip` is not one.
        int pf_sockaddr_any(pf_sockaddr_t *a, int family, unsigned port)
        int pf_sockaddr_loopback(pf_sockaddr_t *a, int family,
                                 unsigned port)
                The wildcard address or the loopback address of AF_INET
                or AF_INET6.
        int pf_sockaddr_unix(pf_sockaddr_t *a, const char *path)
                An AF_UNIX address; PF_ENAMETOOLONG if `path` does not
                fit. A path starting with '@' is in the Linux abstract
                namespace (PF_ENOSYS elsewhere).
        int pf_sockaddr_family(const pf_sockaddr_t *a)
        int pf_sockaddr_port(const pf_sockaddr_t *a)
                The port of an IP address; PF_EAFNOSUPPORT otherwise.
        int pf_sockaddr_set_port(pf_sockaddr_t *a, unsigned port)
        bool pf_sockaddr_equal(const pf_sockaddr_t *a,
                               const pf_sockaddr_t *b)
        ptrdiff_t pf_sockaddr_format(const pf_sockaddr_t *a, char *buf,
                                     size_t size)
                "10.0.0.1:80", "[::1]:80", "[fe80::1%2]:80" (numeric scope
                id), the path of an AF_UNIX address ("@name" if abstract,
                "" if unnamed): the length, PF_ERANGE if `buf` is too
                small, only the length with `buf == NULL`.

    Sockets:

        int pf_socket(pf_socket_t *s, int family, int type, int protocol,
                      unsigned flags)
        int pf_socketpair(pf_socket_t s[2], int family, int type,
                          unsigned flags)
                Two connected sockets. On Windows only AF_UNIX
                SOCK_STREAM (PF_ENOSYS otherwise): an AF_UNIX connection
                through a temporary path, or a TCP loopback connection
                where AF_UNIX does not work.
                `flags`: PF_SOCK_NONBLOCK, the socket does not block.
        int pf_bind(pf_socket_t s, const pf_sockaddr_t *addr)
        int pf_listen(pf_socket_t s, int backlog)
                `backlog` 0 or less: SOMAXCONN.
        int pf_accept(pf_socket_t s, pf_socket_t *out, pf_sockaddr_t *peer,
                      unsigned flags)
                `peer` may be NULL. The new socket blocks unless `flags`
                has PF_SOCK_NONBLOCK, whatever the listening socket does.
        int pf_connect(pf_socket_t s, const pf_sockaddr_t *addr,
                       const struct timespec *deadline)
                Without a deadline, connect(); a non-blocking socket gets
                PF_EINPROGRESS (then wait with pf_socket_wait for POLLOUT,
                and read the result with pf_socket_error). With an
                absolute UTC deadline, PF_ETIMEDOUT once it passes, and the
                socket is left blocking.
        pf_ssize_t pf_send(pf_socket_t s, const void *buf, size_t n,
                           int flags)
        pf_ssize_t pf_recv(pf_socket_t s, void *buf, size_t n, int flags)
        pf_ssize_t pf_sendto(pf_socket_t s, const void *buf, size_t n,
                             int flags, const pf_sockaddr_t *to)
        pf_ssize_t pf_recvfrom(pf_socket_t s, void *buf, size_t n,
                               int flags, pf_sockaddr_t *from)
                The bytes sent or received (0: the peer closed the
                connection). `flags` are the system's MSG_*. On Windows a
                call moves at most INT_MAX bytes. A non-blocking socket
                that would block gives PF_EAGAIN.
        int pf_shutdown(pf_socket_t s, int how)
                PF_SHUT_RD, PF_SHUT_WR or PF_SHUT_RDWR.
        int pf_socket_close(pf_socket_t s)
                0 for PF_SOCKET_INVALID.
        int pf_getsockname(pf_socket_t s, pf_sockaddr_t *addr)
        int pf_getpeername(pf_socket_t s, pf_sockaddr_t *addr)
        int pf_getsockopt(pf_socket_t s, int level, int name, void *val,
                          pf_socklen_t *len)
        int pf_setsockopt(pf_socket_t s, int level, int name,
                          const void *val, pf_socklen_t len)
        int pf_socket_error(pf_socket_t s)
                The pending error of the socket (SO_ERROR) as a PF_E*
                code, and clears it: the result of a non-blocking connect.

    On macOS the sockets this library creates do not raise SIGPIPE; others
    passed to pf_send may.

    Waiting:

        int pf_poll(pf_pollfd_t *fds, size_t n, const struct timespec *deadline)
                poll() until the absolute UTC `deadline` (NULL: none):
                the number of entries with events, 0 once the deadline
                passes. Windows: WSAPoll, sockets only.
        int pf_socket_wait(pf_socket_t s, int events,
                           const struct timespec *deadline)
                Waits for POLLIN and / or POLLOUT on one socket: the
                events that happened (POLLERR and POLLHUP too), or
                PF_ETIMEDOUT. On Windows this reports a failed connect,
                which WSAPoll does not before Windows 10 2004.

    Options (0 or a PF_E* code):

        int pf_socket_set_nonblocking(pf_socket_t s, bool on)
        int pf_socket_set_nodelay(pf_socket_t s, bool on)    TCP_NODELAY
        int pf_socket_set_keepalive(pf_socket_t s, bool on)
        int pf_socket_set_reuseaddr(pf_socket_t s, bool on)
                SO_REUSEADDR: bind to a port with connections in
                TIME_WAIT. Does nothing on Windows, where that is the
                default (and SO_REUSEADDR lets others take the port).
        int pf_socket_set_reuseport(pf_socket_t s, bool on)
                SO_REUSEPORT; PF_ENOSYS where the system has none
                (Windows).
        int pf_socket_set_ipv6only(pf_socket_t s, bool on)
        int pf_socket_set_linger(pf_socket_t s, bool on, int seconds)
        int pf_socket_set_recv_timeout(pf_socket_t s, unsigned ms)
        int pf_socket_set_send_timeout(pf_socket_t s, unsigned ms)
                0: none. A timed-out call fails with PF_EAGAIN.
        int pf_socket_set_recv_buffer(pf_socket_t s, int size)
        int pf_socket_set_send_buffer(pf_socket_t s, int size)

    Names (host names are UTF-8 on every platform):

        int pf_getaddrinfo(const char *host, const char *service,
                           const struct addrinfo *hints,
                           struct addrinfo **res)
        void pf_freeaddrinfo(struct addrinfo *res)
                getaddrinfo() returning a PF_E* code: EAI_NONAME and
                EAI_NODATA are PF_ENOENT, EAI_AGAIN PF_EAGAIN, EAI_FAIL
                PF_EIO, EAI_MEMORY PF_ENOMEM, EAI_FAMILY PF_EAFNOSUPPORT,
                EAI_SERVICE and EAI_SOCKTYPE PF_EINVAL, EAI_SYSTEM the
                errno. Free the list with pf_freeaddrinfo only.
        int pf_getnameinfo(const pf_sockaddr_t *a, char *host,
                           size_t hostlen, char *serv, size_t servlen,
                           int flags)
        int pf_resolve(const char *host, const char *service, int family,
                       int type, unsigned flags, pf_sockaddr_t *out,
                       size_t max)
                The addresses of `host` (family AF_UNSPEC, AF_INET or
                AF_INET6; type SOCK_STREAM, SOCK_DGRAM or 0) into
                `out[0..max)`: how many (at least 1), or an error.
                `flags`: PF_RESOLVE_PASSIVE (a NULL `host` is the wildcard
                address rather than loopback), PF_RESOLVE_NUMERIC (no
                name lookups).
        int pf_inet_pton(int family, const char *src, void *dst)
        const char *pf_inet_ntop(int family, const void *src, char *dst,
                                 size_t size)
                inet_pton and inet_ntop (POSIX conventions).

    Shortcuts:

        int pf_tcp_connect(pf_socket_t *s, const char *host,
                           const char *service,
                           const struct timespec *deadline)
                Tries each address of `host` in turn until one connects,
                all before `deadline`; the error of the last one.
        int pf_tcp_listen(pf_socket_t *s, const char *host,
                          const char *service, int backlog)
                A listening socket on `host` (NULL: every address, IPv6
                and IPv4 together where possible) and `service` ("0": a
                free port, read it back with pf_getsockname), with
                pf_socket_set_reuseaddr.
        int pf_unix_connect(pf_socket_t *s, const char *path)
        int pf_unix_listen(pf_socket_t *s, const char *path, int backlog)
                AF_UNIX stream sockets (pf_sockaddr_unix paths).
        int pf_stream_from_socket(pf_stream_t *st, pf_socket_t s, bool own)
                A pf_stream_t (<cpolyfill/io.h>) on a connected socket,
                closed with the stream if `own`.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_SOCKET, on Windows:
    SHUT_RD, SHUT_WR, SHUT_RDWR, MSG_NOSIGNAL (0: Windows has no SIGPIPE),
    `inet_pton`, `inet_ntop`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_SOCKET_H
#define CPOLYFILL_SOCKET_H

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
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #if PF_CONFIG_HAVE_AFUNIX_H
        #include <afunix.h>
    #else
        #ifndef UNIX_PATH_MAX
            #define UNIX_PATH_MAX 108
        #endif
struct sockaddr_un {
    ADDRESS_FAMILY sun_family;
    char sun_path[UNIX_PATH_MAX];
};
    #endif
    #ifndef AF_UNIX
        #define AF_UNIX 1
    #endif
typedef uintptr_t pf_socket_t;
typedef int pf_socklen_t;
typedef WSAPOLLFD pf_pollfd_t;
    #define PF_SOCKET_INVALID ((pf_socket_t) ~(uintptr_t)0)
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/un.h>
typedef int pf_socket_t;
typedef socklen_t pf_socklen_t;
typedef struct pollfd pf_pollfd_t;
    #define PF_SOCKET_INVALID (-1)
#endif

#define PF_SOCK_NONBLOCK 0x1u

#define PF_SHUT_RD 0
#define PF_SHUT_WR 1
#define PF_SHUT_RDWR 2

#define PF_RESOLVE_PASSIVE 0x1u
#define PF_RESOLVE_NUMERIC 0x2u

typedef struct pf_sockaddr_t {
    union {
        struct sockaddr sa;
        struct sockaddr_in in4;
        struct sockaddr_in6 in6;
        struct sockaddr_un un;
        struct sockaddr_storage storage;
    } u;
    pf_socklen_t len;
} pf_sockaddr_t;

struct addrinfo;

PF_BEGIN_DECLS

PF_API int pf_sockaddr_ip(pf_sockaddr_t *a, const char *ip, unsigned port);
PF_API int pf_sockaddr_any(pf_sockaddr_t *a, int family, unsigned port);
PF_API int pf_sockaddr_loopback(pf_sockaddr_t *a, int family, unsigned port);
PF_API int pf_sockaddr_unix(pf_sockaddr_t *a, const char *path);
PF_API int pf_sockaddr_family(const pf_sockaddr_t *a);
PF_API int pf_sockaddr_port(const pf_sockaddr_t *a);
PF_API int pf_sockaddr_set_port(pf_sockaddr_t *a, unsigned port);
PF_API bool pf_sockaddr_equal(const pf_sockaddr_t *a, const pf_sockaddr_t *b);
PF_API ptrdiff_t
pf_sockaddr_format(const pf_sockaddr_t *a, char *buf, size_t size);

PF_API int pf_socket(
    pf_socket_t *s, int family, int type, int protocol, unsigned flags
);
PF_API int pf_socketpair(
    pf_socket_t s[2], int family, int type, unsigned flags
);
PF_API int pf_bind(pf_socket_t s, const pf_sockaddr_t *addr);
PF_API int pf_listen(pf_socket_t s, int backlog);
PF_API int pf_accept(
    pf_socket_t s, pf_socket_t *out, pf_sockaddr_t *peer, unsigned flags
);
PF_API int pf_connect(
    pf_socket_t s, const pf_sockaddr_t *addr, const struct timespec *deadline
);
PF_API pf_ssize_t pf_send(pf_socket_t s, const void *buf, size_t n, int flags);
PF_API pf_ssize_t pf_recv(pf_socket_t s, void *buf, size_t n, int flags);
PF_API pf_ssize_t pf_sendto(
    pf_socket_t s, const void *buf, size_t n, int flags, const pf_sockaddr_t *to
);
PF_API pf_ssize_t
pf_recvfrom(pf_socket_t s, void *buf, size_t n, int flags, pf_sockaddr_t *from);
PF_API int pf_shutdown(pf_socket_t s, int how);
PF_API int pf_socket_close(pf_socket_t s);
PF_API int pf_getsockname(pf_socket_t s, pf_sockaddr_t *addr);
PF_API int pf_getpeername(pf_socket_t s, pf_sockaddr_t *addr);
PF_API int pf_getsockopt(
    pf_socket_t s, int level, int name, void *val, pf_socklen_t *len
);
PF_API int pf_setsockopt(
    pf_socket_t s, int level, int name, const void *val, pf_socklen_t len
);
PF_API int pf_socket_error(pf_socket_t s);

PF_API int pf_poll(pf_pollfd_t *fds, size_t n, const struct timespec *deadline);
PF_API int pf_socket_wait(
    pf_socket_t s, int events, const struct timespec *deadline
);

PF_API int pf_socket_set_nonblocking(pf_socket_t s, bool on);
PF_API int pf_socket_set_nodelay(pf_socket_t s, bool on);
PF_API int pf_socket_set_keepalive(pf_socket_t s, bool on);
PF_API int pf_socket_set_reuseaddr(pf_socket_t s, bool on);
PF_API int pf_socket_set_reuseport(pf_socket_t s, bool on);
PF_API int pf_socket_set_ipv6only(pf_socket_t s, bool on);
PF_API int pf_socket_set_linger(pf_socket_t s, bool on, int seconds);
PF_API int pf_socket_set_recv_timeout(pf_socket_t s, unsigned ms);
PF_API int pf_socket_set_send_timeout(pf_socket_t s, unsigned ms);
PF_API int pf_socket_set_recv_buffer(pf_socket_t s, int size);
PF_API int pf_socket_set_send_buffer(pf_socket_t s, int size);

PF_API int pf_getaddrinfo(
    const char *host,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res
);
PF_API void pf_freeaddrinfo(struct addrinfo *res);
PF_API int pf_getnameinfo(
    const pf_sockaddr_t *a,
    char *host,
    size_t hostlen,
    char *serv,
    size_t servlen,
    int flags
);
PF_API int pf_resolve(
    const char *host,
    const char *service,
    int family,
    int type,
    unsigned flags,
    pf_sockaddr_t *out,
    size_t max
);
PF_API int pf_inet_pton(int family, const char *src, void *dst);
PF_API const char *pf_inet_ntop(
    int family, const void *src, char *dst, size_t size
);

PF_API int pf_tcp_connect(
    pf_socket_t *s,
    const char *host,
    const char *service,
    const struct timespec *deadline
);
PF_API int pf_tcp_listen(
    pf_socket_t *s, const char *host, const char *service, int backlog
);
PF_API int pf_unix_connect(pf_socket_t *s, const char *path);
PF_API int pf_unix_listen(pf_socket_t *s, const char *path, int backlog);
PF_API int pf_stream_from_socket(pf_stream_t *st, pf_socket_t s, bool own);

PF_END_DECLS

#if PF_OS_WINDOWS                                                   \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_SOCKET)) \
        || defined(PF_STD_NAMES_SOCKET))
    #ifndef SHUT_RD
        #define SHUT_RD PF_SHUT_RD
    #endif
    #ifndef SHUT_WR
        #define SHUT_WR PF_SHUT_WR
    #endif
    #ifndef SHUT_RDWR
        #define SHUT_RDWR PF_SHUT_RDWR
    #endif
    #ifndef MSG_NOSIGNAL
        #define MSG_NOSIGNAL 0
    #endif
    #undef inet_pton
    #define inet_pton pf_inet_pton
    #undef inet_ntop
    #define inet_ntop pf_inet_ntop
#endif

#endif /* CPOLYFILL_SOCKET_H */
