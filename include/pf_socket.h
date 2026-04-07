/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a wrapper for POSIX sockets and winsock2.
    While the two libraries are quite compatible with each other, this
    wrapper fixes some inconsistencies and minor differences between them.

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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

#ifndef POLYFILL_SOCKET
#define POLYFILL_SOCKET

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef _WIN32
    #include <limits.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define PF_INVALID_SOCKET INVALID_SOCKET
#else
    #include <arpa/inet.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
    #define PF_INVALID_SOCKET -1
#endif

PF_API int pf_sock_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

PF_API void pf_sock_exit(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

#ifdef _WIN32
typedef int pf_sock_ssize_t;
typedef SOCKET pf_sock_t;
typedef int pf_socklen_t;
typedef SOCKADDR_STORAGE pf_sockaddr_storage_t;
typedef SOCKADDR pf_sockaddr_t;
typedef fd_set pf_sockset_t;
typedef TIMEVAL pf_timeval_t;
typedef ADDRINFOA pf_addrinfo_t;
typedef WSAPOLLFD pf_pollfd_t;
#else
typedef ssize_t pf_sock_ssize_t;
typedef int pf_sock_t;
typedef socklen_t pf_socklen_t;
typedef struct sockaddr_storage pf_sockaddr_storage_t;
typedef struct sockaddr pf_sockaddr_t;
typedef fd_set pf_sockset_t;
typedef struct timeval pf_timeval_t;
typedef struct addrinfo pf_addrinfo_t;
typedef struct pollfd pf_pollfd_t;
#endif

PF_API pf_sock_t pf_sock_open(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

PF_API int pf_sock_bind(
    pf_sock_t fd, const pf_sockaddr_t *addr, pf_socklen_t len
) {
    return bind(fd, addr, len);
}

PF_API int pf_sock_listen(pf_sock_t fd, int backlog) {
    return listen(fd, backlog);
}

PF_API pf_sock_t
pf_sock_accept(pf_sock_t fd, pf_sockaddr_t *addr, pf_socklen_t *len) {
    return accept(fd, addr, len);
}

PF_API int pf_sock_connect(
    pf_sock_t fd, const pf_sockaddr_t *addr, pf_socklen_t len
) {
    return connect(fd, addr, len);
}

PF_API int pf_sock_send(pf_sock_t fd, const void *buf, size_t len, int flags) {
#ifdef _WIN32
    if (len > INT_MAX)
        return -1;
    int r = send(fd, buf, len, flags);
    return r != SOCKET_ERROR ? r : -1;
#else
    return send(fd, buf, len, flags);
#endif
}

PF_API int pf_sock_sendto(
    pf_sock_t fd,
    const void *buf,
    size_t len,
    int flags,
    const pf_sockaddr_t *addr,
    pf_socklen_t addrlen
) {
#ifdef _WIN32
    if (len > INT_MAX)
        return -1;
    int r = sendto(fd, buf, len, flags, addr, addrlen);
    return r != SOCKET_ERROR ? r : -1;
#else
    return sendto(fd, buf, len, flags, addr, addrlen);
#endif
}

PF_API pf_sock_ssize_t
pf_sock_recv(pf_sock_t fd, void *buf, size_t len, int flags) {
#ifdef _WIN32
    if (len > INT_MAX)
        return -1;
    int r = recv(fd, buf, len, flags);
    return r != SOCKET_ERROR ? r : -1;
#else
    return recv(fd, buf, len, flags);
#endif
}

PF_API pf_sock_ssize_t pf_sock_recvfrom(
    pf_sock_t fd,
    void *buf,
    size_t len,
    int flags,
    pf_sockaddr_t *addr,
    pf_socklen_t *addrlen
) {
#ifdef _WIN32
    if (len > INT_MAX)
        return -1;
    int r = recvfrom(fd, buf, len, flags, addr, addrlen);
    return r != SOCKET_ERROR ? r : -1;
#else
    return recvfrom(fd, buf, len, flags, addr, addrlen);
#endif
}

PF_API int pf_sock_select(
    int nfds,
    pf_sockset_t *readfds,
    pf_sockset_t *writefds,
    pf_sockset_t *exceptfds,
    pf_timeval_t *timeout
) {
    return select(nfds, readfds, writefds, exceptfds, timeout);
}

PF_API int pf_sock_poll(pf_pollfd_t *fds, size_t nfds, int timeout) {
#ifdef _WIN32
    if (nfds > ULONG_MAX)
        return -1;
    return WSAPoll(fds, nfds, timeout);
#else
    return poll(fds, nfds, timeout);
#endif
}

PF_API int pf_sock_getopt(
    pf_sock_t fd, int level, int optname, void *optval, pf_socklen_t *optlen
) {
    return getsockopt(fd, level, optname, optval, optlen);
}

PF_API int pf_sock_setopt(
    pf_sock_t fd,
    int level,
    int optname,
    const void *optval,
    pf_socklen_t optlen
) {
    return setsockopt(fd, level, optname, optval, optlen);
}

PF_API int pf_sock_set_nonblocking(pf_sock_t fd, int nonblocking) {
#ifdef _WIN32
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;

    if (nonblocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) == 0 ? 0 : -1;
#endif
}

PF_API int pf_sock_set_reuseaddr(pf_sock_t fd, int enable) {
#ifdef _WIN32
    BOOL val = enable ? TRUE : FALSE;
    return setsockopt(
               fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(val)
           ) == 0
        ? 0
        : -1;
#else
    return setsockopt(
               fd, SOL_SOCKET, SO_REUSEADDR, &enable, (socklen_t)sizeof(enable)
           ) == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_reuseport(pf_sock_t fd, int enable) {
#ifdef _WIN32
    (void)fd;
    (void)enable;
    return 0;
#elif defined(SO_REUSEPORT)
    return setsockopt(
               fd, SOL_SOCKET, SO_REUSEPORT, &enable, (socklen_t)sizeof(enable)
           ) == 0
        ? 0
        : -1;
#else
    (void)fd;
    (void)enable;
    return -1;
#endif
}

PF_API int pf_sock_set_keepalive(pf_sock_t fd, int enable) {
#ifdef _WIN32
    BOOL val = enable ? TRUE : FALSE;
    return setsockopt(
               fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&val, sizeof(val)
           ) == 0
        ? 0
        : -1;
#else
    return setsockopt(
               fd, SOL_SOCKET, SO_KEEPALIVE, &enable, (socklen_t)sizeof(enable)
           ) == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_nodelay(pf_sock_t fd, int enable) {
#ifdef _WIN32
    BOOL val = enable ? TRUE : FALSE;
    return setsockopt(
               fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&val, sizeof(val)
           ) == 0
        ? 0
        : -1;
#else
    return setsockopt(
               fd, IPPROTO_TCP, TCP_NODELAY, &enable, (socklen_t)sizeof(enable)
           ) == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_linger(pf_sock_t fd, int enable, int timeout_sec) {
    struct linger l;
    l.l_onoff = (u_short)enable;
    l.l_linger = (u_short)timeout_sec;
#ifdef _WIN32
    return setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&l, sizeof(l))
            == 0
        ? 0
        : -1;
#else
    return setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, (socklen_t)sizeof(l)) == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_recvtimeo(pf_sock_t fd, int ms) {
#ifdef _WIN32
    DWORD val = (DWORD)ms;
    return setsockopt(
               fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&val, sizeof(val)
           ) == 0
        ? 0
        : -1;
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, (socklen_t)sizeof(tv))
            == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_sendtimeo(pf_sock_t fd, int ms) {
#ifdef _WIN32
    DWORD val = (DWORD)ms;
    return setsockopt(
               fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&val, sizeof(val)
           ) == 0
        ? 0
        : -1;
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, (socklen_t)sizeof(tv))
            == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_recvbuf(pf_sock_t fd, int size) {
#ifdef _WIN32
    return setsockopt(
               fd, SOL_SOCKET, SO_RCVBUF, (const char *)&size, sizeof(size)
           ) == 0
        ? 0
        : -1;
#else
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, (socklen_t)sizeof(size))
            == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_sendbuf(pf_sock_t fd, int size) {
#ifdef _WIN32
    return setsockopt(
               fd, SOL_SOCKET, SO_SNDBUF, (const char *)&size, sizeof(size)
           ) == 0
        ? 0
        : -1;
#else
    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, (socklen_t)sizeof(size))
            == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_set_ipv6only(pf_sock_t fd, int enable) {
#ifdef _WIN32
    DWORD val = enable ? 1 : 0;
    return setsockopt(
               fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&val, sizeof(val)
           ) == 0
        ? 0
        : -1;
#else
    return setsockopt(
               fd, IPPROTO_IPV6, IPV6_V6ONLY, &enable, (socklen_t)sizeof(enable)
           ) == 0
        ? 0
        : -1;
#endif
}

PF_API int pf_sock_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

PF_API int pf_sock_would_block(void) {
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    int e = errno;
    return e == EAGAIN || e == EWOULDBLOCK;
#endif
}

PF_API int pf_sock_close(pf_sock_t fd) {
    if (fd == PF_INVALID_SOCKET)
        return 0;
#ifdef _WIN32
    shutdown(fd, SD_BOTH);
    return closesocket(fd) == 0 ? 0 : -1;
#else
    shutdown(fd, SHUT_RDWR);
    return close(fd) == 0 ? 0 : -1;
#endif
}

PF_API int pf_sock_getsockname(
    pf_sock_t fd, pf_sockaddr_t *addr, pf_socklen_t *len
) {
    return getsockname(fd, addr, len);
}

PF_API int pf_sock_getpeername(
    pf_sock_t fd, pf_sockaddr_t *addr, pf_socklen_t *len
) {
    return getpeername(fd, addr, len);
}

PF_API int pf_getaddrinfo(
    const char *name,
    const char *service,
    const pf_addrinfo_t *hints,
    pf_addrinfo_t **out
) {
    return getaddrinfo(name, service, hints, out);
}

PF_API int pf_getnameinfo(
    const pf_sockaddr_t *addr,
    pf_socklen_t len,
    char *host,
    size_t hostlen,
    char *serv,
    size_t servlen,
    int flags
) {
#ifdef _WIN32
    DWORD hostlen_ = hostlen;
    DWORD servlen_ = servlen;
    return getnameinfo(addr, len, host, hostlen_, serv, servlen_, flags);
#else
    return getnameinfo(addr, len, host, hostlen, serv, servlen, flags);
#endif
}

PF_API void pf_freeaddrinfo(pf_addrinfo_t *info) {
    if (info)
        freeaddrinfo(info);
}

PF_API const char *pf_inet_ntop(
    int af, const void *src, char *dst, size_t size
) {
#ifdef _WIN32
    return inet_ntop(af, src, dst, (DWORD)size);
#else
    return inet_ntop(af, src, dst, (socklen_t)size);
#endif
}

PF_API int pf_inet_pton(int af, const char *src, void *dst) {
    return inet_pton(af, src, dst);
}

#endif
