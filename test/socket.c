/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/socket.h> */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/random.h>
#include <cpolyfill/socket.h>
#include <cpolyfill/time.h>

#include <stdio.h>
#include <string.h>
#if PF_OS_POSIX
    #include <fcntl.h>
#endif

#include "check.h"
#include "files.h"

/* The absolute UTC time `ms` milliseconds from now. */
static struct timespec after_ms(long ms) {
    struct timespec now;
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_REALTIME, &now), 0);
    return pf_timespec_add(now, pf_timespec_from_ns((int64_t)ms * 1000000));
}

static int64_t now_ms(void) {
    struct timespec now;
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_MONOTONIC, &now), 0);
    return pf_timespec_to_ns(now) / 1000000;
}

static const char *fmt(const pf_sockaddr_t *a) {
    static char buf[200];
    PF_ASSERT_ALWAYS_GE(pf_sockaddr_format(a, buf, sizeof(buf)), 0);
    return buf;
}

/* Whether the descriptor is close-on-exec (true off POSIX). */
static bool cloexec(pf_socket_t s) {
#if PF_OS_POSIX
    int fl = fcntl(s, F_GETFD);
    return fl >= 0 && (fl & FD_CLOEXEC);
#else
    (void)s;
    return true;
#endif
}

/* A listening TCP socket on the loopback address and a connected pair. */
static void tcp_pair(pf_socket_t *lst, pf_socket_t *c, pf_socket_t *a) {
    pf_sockaddr_t addr;
    char port[16];
    struct timespec dl = after_ms(5000);

    PF_ASSERT_ALWAYS_EQ(pf_tcp_listen(lst, "127.0.0.1", "0", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(*lst, &addr), 0);
    snprintf(port, sizeof(port), "%d", pf_sockaddr_port(&addr));
    PF_ASSERT_ALWAYS_EQ(pf_tcp_connect(c, "127.0.0.1", port, &dl), 0);
    PF_ASSERT_ALWAYS_EQ(pf_accept(*lst, a, NULL, 0), 0);
}

static int test_addresses(int seed, int i) {
    pf_sockaddr_t a, b;
    char buf[8];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&a, "10.1.2.3", 80), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_family(&a), AF_INET);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_port(&a), 80);
    PF_ASSERT_ALWAYS_EQ(a.len, (pf_socklen_t)sizeof(struct sockaddr_in));
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&a), "10.1.2.3:80");
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_format(&a, NULL, 0), 11);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_format(&a, buf, sizeof(buf)), PF_ERANGE);

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&b, "::1", 8080), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_family(&b), AF_INET6);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&b), "[::1]:8080");
    PF_ASSERT_ALWAYS(!pf_sockaddr_equal(&a, &b));
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_loopback(&a, AF_INET6, 8080), 0);
    PF_ASSERT_ALWAYS(pf_sockaddr_equal(&a, &b));
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_set_port(&a, 1), 0);
    PF_ASSERT_ALWAYS(!pf_sockaddr_equal(&a, &b));

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&a, "fe80::1%1", 5), 0);
    PF_ASSERT_ALWAYS_EQ(a.u.in6.sin6_scope_id, 1u);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&a), "[fe80::1%1]:5");

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_any(&a, AF_INET, 0), 0);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&a), "0.0.0.0:0");
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_loopback(&a, AF_INET, 7), 0);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&a), "127.0.0.1:7");
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_any(&a, AF_UNIX, 0), PF_EAFNOSUPPORT);

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&a, "localhost", 1), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&a, "1.2.3.4.5", 1), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&a, "1.2.3.4", 65536), PF_EINVAL);

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_unix(&a, "/tmp/x.sock"), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_family(&a), AF_UNIX);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&a), "/tmp/x.sock");
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_port(&a), PF_EAFNOSUPPORT);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_unix(&b, "/tmp/x.sock"), 0);
    PF_ASSERT_ALWAYS(pf_sockaddr_equal(&a, &b));
    {
        char longp[300];
        memset(longp, 'a', sizeof(longp) - 1);
        longp[sizeof(longp) - 1] = 0;
        PF_ASSERT_ALWAYS_EQ(pf_sockaddr_unix(&a, longp), PF_ENAMETOOLONG);
    }
#if PF_OS_LINUX
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_unix(&a, "@abstract"), 0);
    PF_ASSERT_ALWAYS_EQ(a.u.un.sun_path[0], 0);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&a), "@abstract");
#endif
    return 0;
}

static int test_resolve(int seed, int i) {
    pf_sockaddr_t out[8];
    char host[64], serv[16];
    int n, k;
    (void)seed, (void)i;

    n = pf_resolve("localhost", "80", AF_UNSPEC, SOCK_STREAM, 0, out, 8);
    PF_ASSERT_ALWAYS_GE(n, 1);
    for (k = 0; k < n; k++)
        PF_ASSERT_ALWAYS_EQ(pf_sockaddr_port(&out[k]), 80);

    n = pf_resolve("127.0.0.1", NULL, AF_INET, 0, PF_RESOLVE_NUMERIC, out, 8);
    PF_ASSERT_ALWAYS_EQ(n, 1); /* one, though once per socket type */
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&out[0]), "127.0.0.1:0");

    n = pf_resolve(NULL, "99", AF_INET, SOCK_STREAM, PF_RESOLVE_PASSIVE, out, 8);
    PF_ASSERT_ALWAYS_EQ(n, 1);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&out[0]), "0.0.0.0:99");

    PF_ASSERT_ALWAYS_EQ(
        pf_resolve("example", NULL, AF_INET, 0, PF_RESOLVE_NUMERIC, out, 8),
        PF_ENOENT
    );
    n = pf_resolve("nothing.invalid", NULL, AF_UNSPEC, 0, 0, out, 8);
    PF_ASSERT_ALWAYS_MSG(
        n == PF_ENOENT || n == PF_EAGAIN, "%s", pf_error_name(n)
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_resolve("x", "no-such-service-name", AF_INET, 0, 0, out, 8),
        PF_EINVAL
    );
    PF_ASSERT_ALWAYS_EQ(
        pf_resolve("127.0.0.1", NULL, AF_INET, 0, 0, out, 0), PF_EINVAL
    );

    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&out[0], "127.0.0.1", 443), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf_getnameinfo(
            &out[0], host, sizeof(host), serv, sizeof(serv),
            NI_NUMERICHOST | NI_NUMERICSERV
        ),
        0
    );
    PF_ASSERT_ALWAYS_STR_EQ(host, "127.0.0.1");
    PF_ASSERT_ALWAYS_STR_EQ(serv, "443");

    {
        unsigned char b4[4];
        char text[INET_ADDRSTRLEN];
        PF_ASSERT_ALWAYS_EQ(pf_inet_pton(AF_INET, "1.2.3.4", b4), 1);
        PF_ASSERT_ALWAYS_EQ(b4[3], 4);
        PF_ASSERT_ALWAYS_EQ(pf_inet_pton(AF_INET, "nope", b4), 0);
        PF_ASSERT_ALWAYS(pf_inet_ntop(AF_INET, b4, text, sizeof(text)) == text);
        PF_ASSERT_ALWAYS_STR_EQ(text, "1.2.3.4");
        PF_ASSERT_ALWAYS(pf_inet_ntop(AF_INET, b4, text, 4) == NULL);
    }
    return 0;
}

static int test_tcp(int seed, int i) {
    pf_socket_t lst, c, a;
    pf_sockaddr_t mine, peer;
    char buf[32];
    (void)seed, (void)i;

    tcp_pair(&lst, &c, &a);
    PF_ASSERT_ALWAYS(cloexec(lst) && cloexec(c) && cloexec(a));
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(c, &mine), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getpeername(a, &peer), 0);
    PF_ASSERT_ALWAYS(pf_sockaddr_equal(&mine, &peer));

    PF_ASSERT_ALWAYS_EQ(pf_send(c, "hello", 5, 0), 5);
    PF_ASSERT_ALWAYS_EQ(pf_recv(a, buf, sizeof(buf), 0), 5);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "hello", 5);
    PF_ASSERT_ALWAYS_EQ(pf_send(a, "back", 4, 0), 4);
    PF_ASSERT_ALWAYS_EQ(pf_recv(c, buf, sizeof(buf), 0), 4);

    PF_ASSERT_ALWAYS_EQ(pf_shutdown(c, PF_SHUT_WR), 0);
    PF_ASSERT_ALWAYS_EQ(pf_recv(a, buf, sizeof(buf), 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_shutdown(c, 7), PF_EINVAL);

    PF_ASSERT_ALWAYS_EQ(pf_socket_close(a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_close(c), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_close(lst), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_close(PF_SOCKET_INVALID), 0);
    return 0;
}

static int test_tcp_dual(int seed, int i) {
    pf_socket_t lst, c, a;
    pf_sockaddr_t addr;
    char port[16];
    (void)seed, (void)i;

    /* Every address: IPv4 clients reach it too. */
    PF_ASSERT_ALWAYS_EQ(pf_tcp_listen(&lst, NULL, "0", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(lst, &addr), 0);
    snprintf(port, sizeof(port), "%d", pf_sockaddr_port(&addr));
    PF_ASSERT_ALWAYS_EQ(pf_tcp_connect(&c, "127.0.0.1", port, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_accept(lst, &a, &addr, 0), 0);
    pf_socket_close(a);
    pf_socket_close(c);
    pf_socket_close(lst);
    return 0;
}

static int test_refused(int seed, int i) {
    pf_socket_t lst, c;
    pf_sockaddr_t addr;
    (void)seed, (void)i;

    /* A port nobody listens on any more. */
    PF_ASSERT_ALWAYS_EQ(pf_tcp_listen(&lst, "127.0.0.1", "0", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(lst, &addr), 0);
    pf_socket_close(lst);

    PF_ASSERT_ALWAYS_EQ(pf_socket(&c, AF_INET, SOCK_STREAM, 0, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_connect(c, &addr, NULL), PF_ECONNREFUSED);
    pf_socket_close(c);

    {
        struct timespec dl = after_ms(5000);
        PF_ASSERT_ALWAYS_EQ(pf_socket(&c, AF_INET, SOCK_STREAM, 0, 0), 0);
        PF_ASSERT_ALWAYS_EQ(pf_connect(c, &addr, &dl), PF_ECONNREFUSED);
        pf_socket_close(c);
    }
    return 0;
}

static int test_connect_timeout(int seed, int i) {
    pf_socket_t c;
    pf_sockaddr_t addr;
    struct timespec dl = after_ms(200);
    int64_t start = now_ms(), took;
    int rc;
    (void)seed, (void)i;

    /* A non-routable address: the SYN goes nowhere. */
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_ip(&addr, "10.255.255.1", 9), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket(&c, AF_INET, SOCK_STREAM, 0, 0), 0);
    rc = pf_connect(c, &addr, &dl);
    took = now_ms() - start;
    pf_socket_close(c);
    if (rc == PF_ENETUNREACH || rc == PF_EHOSTUNREACH)
        return SKIP("no route");
    PF_ASSERT_ALWAYS_MSG(rc == PF_ETIMEDOUT, "%s", pf_error_name(rc));
    PF_ASSERT_ALWAYS_GE(took, 150);
    PF_ASSERT_ALWAYS_LT(took, 3000);
    return 0;
}

static int test_nonblocking(int seed, int i) {
    pf_socket_t lst, c, a;
    pf_sockaddr_t addr;
    char buf[4];
    int rc;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_tcp_listen(&lst, "127.0.0.1", "0", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(lst, &addr), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_nonblocking(lst, true), 0);
    PF_ASSERT_ALWAYS_EQ(pf_accept(lst, &a, NULL, 0), PF_EAGAIN);

    PF_ASSERT_ALWAYS_EQ(
        pf_socket(&c, AF_INET, SOCK_STREAM, 0, PF_SOCK_NONBLOCK), 0
    );
    rc = pf_connect(c, &addr, NULL);
    PF_ASSERT_ALWAYS_MSG(rc == 0 || rc == PF_EINPROGRESS, "%s", pf_error_name(rc));
    {
        struct timespec dl = after_ms(5000);
        rc = pf_socket_wait(c, POLLOUT, &dl);
        PF_ASSERT_ALWAYS_GT(rc, 0);
        PF_ASSERT_ALWAYS(rc & POLLOUT);
    }
    PF_ASSERT_ALWAYS_EQ(pf_socket_error(c), 0);
    PF_ASSERT_ALWAYS_EQ(pf_recv(c, buf, sizeof(buf), 0), PF_EAGAIN);

    /* The accepted socket blocks, though the listening one does not. */
    {
        struct timespec dl = after_ms(5000);
        PF_ASSERT_ALWAYS_GT(pf_socket_wait(lst, POLLIN, &dl), 0);
    }
    PF_ASSERT_ALWAYS_EQ(pf_accept(lst, &a, NULL, 0), 0);
#if PF_OS_POSIX
    PF_ASSERT_ALWAYS_EQ(fcntl(a, F_GETFL) & O_NONBLOCK, 0);
#endif
    pf_socket_close(a);
    pf_socket_close(c);
    pf_socket_close(lst);
    return 0;
}

static int test_udp(int seed, int i) {
    pf_socket_t x, y;
    pf_sockaddr_t ax, ay, from;
    char buf[16];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_socket(&x, AF_INET, SOCK_DGRAM, 0, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket(&y, AF_INET, SOCK_DGRAM, 0, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_sockaddr_loopback(&ax, AF_INET, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_bind(x, &ax), 0);
    PF_ASSERT_ALWAYS_EQ(pf_bind(y, &ax), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(x, &ax), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(y, &ay), 0);

    PF_ASSERT_ALWAYS_EQ(pf_sendto(x, "ping", 4, 0, &ay), 4);
    PF_ASSERT_ALWAYS_EQ(pf_recvfrom(y, buf, sizeof(buf), 0, &from), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "ping", 4);
    PF_ASSERT_ALWAYS(pf_sockaddr_equal(&from, &ax));
    pf_socket_close(x);
    pf_socket_close(y);
    return 0;
}

static int test_unix(int seed, int i) {
    char dir[PF_PATH_MAX];
    const char *path;
    pf_socket_t lst, c, a;
    pf_sockaddr_t addr;
    char buf[8];
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    path = test_path(0, dir, "s");
    PF_ASSERT_ALWAYS_EQ(pf_unix_connect(&c, path), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_unix_listen(&lst, path, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_unix_listen(&a, path, 0), PF_EADDRINUSE);
    PF_ASSERT_ALWAYS_EQ(pf_unix_connect(&c, path), 0);
    PF_ASSERT_ALWAYS_EQ(pf_accept(lst, &a, NULL, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_getsockname(lst, &addr), 0);
    PF_ASSERT_ALWAYS_STR_EQ(fmt(&addr), path);
    PF_ASSERT_ALWAYS_EQ(pf_send(a, "unix", 4, 0), 4);
    PF_ASSERT_ALWAYS_EQ(pf_recv(c, buf, sizeof(buf), 0), 4);
    pf_socket_close(a);
    pf_socket_close(c);
    pf_socket_close(lst);

#if PF_OS_LINUX
    {
        char name[64];
        snprintf(name, sizeof(name), "@cpolyfill-test-%lu", (unsigned long)pf_random_u32());
        PF_ASSERT_ALWAYS_EQ(pf_unix_listen(&lst, name, 0), 0);
        PF_ASSERT_ALWAYS_EQ(pf_unix_connect(&c, name), 0);
        PF_ASSERT_ALWAYS_EQ(pf_accept(lst, &a, NULL, 0), 0);
        pf_socket_close(a);
        pf_socket_close(c);
        pf_socket_close(lst);
    }
#endif
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
}

static int test_socketpair(int seed, int i) {
    pf_socket_t s[2];
    char buf[8];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_socketpair(s, AF_UNIX, SOCK_STREAM, 0), 0);
    PF_ASSERT_ALWAYS(cloexec(s[0]) && cloexec(s[1]));
    PF_ASSERT_ALWAYS_EQ(pf_send(s[0], "ab", 2, 0), 2);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[1], buf, sizeof(buf), 0), 2);
    PF_ASSERT_ALWAYS_EQ(pf_send(s[1], "cde", 3, 0), 3);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[0], buf, sizeof(buf), 0), 3);
    pf_socket_close(s[0]);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[1], buf, sizeof(buf), 0), 0);
    pf_socket_close(s[1]);

    PF_ASSERT_ALWAYS_EQ(
        pf_socketpair(s, AF_UNIX, SOCK_STREAM, PF_SOCK_NONBLOCK), 0
    );
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[0], buf, sizeof(buf), 0), PF_EAGAIN);
    pf_socket_close(s[0]);
    pf_socket_close(s[1]);
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_EQ(pf_socketpair(s, AF_UNIX, SOCK_DGRAM, 0), PF_ENOSYS);
#endif
    return 0;
}

static int test_epipe(int seed, int i) {
    pf_socket_t s[2];
    char big[65536];
    int k;
    pf_ssize_t r = 0;
    (void)seed, (void)i;

    /* With SIGPIPE at its default, a signal would end the test. */
    memset(big, 'x', sizeof(big));
    PF_ASSERT_ALWAYS_EQ(pf_socketpair(s, AF_UNIX, SOCK_STREAM, 0), 0);
    pf_socket_close(s[1]);
    for (k = 0; k < 4 && r >= 0; k++)
        r = pf_send(s[0], big, sizeof(big), 0);
    PF_ASSERT_ALWAYS_MSG(
        r == PF_EPIPE || r == PF_ECONNRESET, "%s", pf_error_name((int)r)
    );
    pf_socket_close(s[0]);
    return 0;
}

static int test_poll(int seed, int i) {
    pf_socket_t s[2];
    pf_pollfd_t p[2];
    struct timespec dl;
    int64_t start;
    char c;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_socketpair(s, AF_UNIX, SOCK_STREAM, 0), 0);
    memset(p, 0, sizeof(p));
    p[0].fd = s[0];
    p[0].events = POLLIN;
    p[1].fd = s[1];
    p[1].events = POLLIN;

    start = now_ms();
    dl = after_ms(60);
    PF_ASSERT_ALWAYS_EQ(pf_poll(p, 2, &dl), 0);
    PF_ASSERT_ALWAYS_GE(now_ms() - start, 50);

    PF_ASSERT_ALWAYS_EQ(pf_send(s[0], "z", 1, 0), 1);
    PF_ASSERT_ALWAYS_EQ(pf_poll(p, 2, NULL), 1);
    PF_ASSERT_ALWAYS_EQ(p[0].revents, 0);
    PF_ASSERT_ALWAYS(p[1].revents & POLLIN);

    dl = after_ms(0);
    PF_ASSERT_ALWAYS(pf_socket_wait(s[1], POLLIN, &dl) & POLLIN);
    PF_ASSERT_ALWAYS_EQ(pf_recv(s[1], &c, 1, 0), 1);
    PF_ASSERT_ALWAYS_EQ(pf_socket_wait(s[1], POLLIN, &dl), PF_ETIMEDOUT);
    pf_socket_close(s[0]);
    pf_socket_close(s[1]);
    return 0;
}

static int getint(pf_socket_t s, int level, int name) {
    int v = 0;
    pf_socklen_t len = (pf_socklen_t)sizeof(v);
    PF_ASSERT_ALWAYS_EQ(pf_getsockopt(s, level, name, &v, &len), 0);
    return v;
}

static int test_options(int seed, int i) {
    pf_socket_t s;
    int rc;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_socket(&s, AF_INET6, SOCK_STREAM, 0, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_nodelay(s, true), 0);
    PF_ASSERT_ALWAYS(getint(s, IPPROTO_TCP, TCP_NODELAY) != 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_nodelay(s, false), 0);
    PF_ASSERT_ALWAYS_EQ(getint(s, IPPROTO_TCP, TCP_NODELAY), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_keepalive(s, true), 0);
    PF_ASSERT_ALWAYS(getint(s, SOL_SOCKET, SO_KEEPALIVE) != 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_reuseaddr(s, true), 0);
#if PF_OS_POSIX
    PF_ASSERT_ALWAYS(getint(s, SOL_SOCKET, SO_REUSEADDR) != 0);
#endif
    rc = pf_socket_set_reuseport(s, true);
    PF_ASSERT_ALWAYS(rc == 0 || rc == PF_ENOSYS);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_ipv6only(s, true), 0);
    PF_ASSERT_ALWAYS(getint(s, IPPROTO_IPV6, IPV6_V6ONLY) != 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_linger(s, true, 3), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_linger(s, true, -1), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_recv_buffer(s, 65536), 0);
    PF_ASSERT_ALWAYS_GE(getint(s, SOL_SOCKET, SO_RCVBUF), 65536);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_send_buffer(s, 65536), 0);
    PF_ASSERT_ALWAYS_EQ(pf_socket_set_send_timeout(s, 1500), 0);
    pf_socket_close(s);

    {
        pf_socket_t p[2];
        char c;
        int64_t start;
        PF_ASSERT_ALWAYS_EQ(pf_socketpair(p, AF_UNIX, SOCK_STREAM, 0), 0);
        PF_ASSERT_ALWAYS_EQ(pf_socket_set_recv_timeout(p[0], 50), 0);
        start = now_ms();
        PF_ASSERT_ALWAYS_EQ(pf_recv(p[0], &c, 1, 0), PF_EAGAIN);
        PF_ASSERT_ALWAYS_GE(now_ms() - start, 40);
        pf_socket_close(p[0]);
        pf_socket_close(p[1]);
    }
    return 0;
}

static int test_stream(int seed, int i) {
    pf_socket_t s[2];
    pf_stream_t w, r;
    char buf[32];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_socketpair(s, AF_UNIX, SOCK_STREAM, 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_from_socket(&w, s[0], true), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_from_socket(&r, s[1], true), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_printf(&w, "n=%d", 42), 4);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&w), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read_full(&r, buf, sizeof(buf)), 4);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "n=42", 4);
    PF_ASSERT_ALWAYS_EQ(pf_stream_seek(&r, 0, SEEK_SET), PF_ESPIPE);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&r), 0);
    PF_ASSERT_ALWAYS_EQ(
        pf_stream_from_socket(&r, PF_SOCKET_INVALID, false), PF_EBADF
    );
    return 0;
}

pf_test_t suite_socket[] = {
    { test_addresses, "addresses" },
    { test_resolve, "resolve" },
    { test_tcp, "tcp" },
    { test_tcp_dual, "tcp_dual" },
    { test_refused, "refused" },
    { test_connect_timeout, "connect_timeout" },
    { test_nonblocking, "nonblocking" },
    { test_udp, "udp" },
    { test_unix, "unix" },
    { test_socketpair, "socketpair" },
    { test_epipe, "epipe" },
    { test_poll, "poll" },
    { test_options, "options" },
    { test_stream, "stream" },
    { 0 },
};
