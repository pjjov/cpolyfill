/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025-2026 Предраг Јовановић

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "check.h"
#include "../include/pf_socket.h"
#include "util.h"
#include <string.h>

/* Everything runs against the loopback interface. Sockets may be
   unavailable in sandboxes, in which case the tests are skipped. */

static int net_up(void) {
    pf_sock_t s;

    if (pf_sock_init() != 0)
        return 0;
    s = pf_sock_open(AF_INET, SOCK_STREAM, 0);
    if (s == PF_INVALID_SOCKET) {
        pf_sock_exit();
        return 0;
    }
    pf_sock_close(s);
    return 1;
}

#define NEED_NET()                                \
    if (!net_up())                                \
    return SKIP("sockets are not available here")

/* Binds to 127.0.0.1 on a free port. */
static pf_sock_t listen_tcp(unsigned short *port) {
    struct sockaddr_in addr;
    pf_socklen_t len = sizeof addr;
    pf_sock_t s = pf_sock_open(AF_INET, SOCK_STREAM, 0);

    if (s == PF_INVALID_SOCKET)
        return s;

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (pf_sock_bind(s, (pf_sockaddr_t *)&addr, sizeof addr) != 0
        || pf_sock_listen(s, 4) != 0
        || pf_sock_getsockname(s, (pf_sockaddr_t *)&addr, &len) != 0) {
        pf_sock_close(s);
        return PF_INVALID_SOCKET;
    }
    *port = ntohs(addr.sin_port);
    return s;
}

static pf_sock_t connect_tcp(unsigned short port) {
    struct sockaddr_in addr;
    pf_sock_t s = pf_sock_open(AF_INET, SOCK_STREAM, 0);

    if (s == PF_INVALID_SOCKET)
        return s;

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (pf_sock_connect(s, (pf_sockaddr_t *)&addr, sizeof addr) != 0) {
        pf_sock_close(s);
        return PF_INVALID_SOCKET;
    }
    return s;
}

/* A connected pair: the client and the accepted end of the server. */
static int tcp_pair(pf_sock_t *client, pf_sock_t *server, pf_sock_t *listener) {
    unsigned short port;

    *listener = listen_tcp(&port);
    if (*listener == PF_INVALID_SOCKET)
        return -1;
    *client = connect_tcp(port);
    if (*client == PF_INVALID_SOCKET)
        return -1;
    *server = pf_sock_accept(*listener, NULL, NULL);
    return *server == PF_INVALID_SOCKET ? -1 : 0;
}

static int test_socket_init_exit(int seed, int rep) {
    CHECK(pf_sock_init() == 0);
    CHECK(pf_sock_init() == 0); /* reference counted on Windows */
    pf_sock_exit();
    pf_sock_exit();
    return 0;
}

static int test_socket_open_close(int seed, int rep) {
    pf_sock_t s;

    NEED_NET();
    s = pf_sock_open(AF_INET, SOCK_STREAM, 0);
    CHECK(s != PF_INVALID_SOCKET);
    CHECK(pf_sock_close(s) == 0);

    s = pf_sock_open(AF_INET, SOCK_DGRAM, 0);
    CHECK(s != PF_INVALID_SOCKET);
    CHECK(pf_sock_close(s) == 0);

    /* Closing the invalid socket is harmless. */
    CHECK(pf_sock_close(PF_INVALID_SOCKET) == 0);

    /* Invalid arguments fail. */
    CHECK(pf_sock_open(-1, -1, -1) == PF_INVALID_SOCKET);
    pf_sock_exit();
    return 0;
}

static int test_socket_tcp_echo(int seed, int rep) {
    pf_sock_t client, server, listener;
    char buf[32] = { 0 };

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);

    CHECK(pf_sock_send(client, "ping", 4, 0) == 4);
    CHECK(pf_sock_recv(server, buf, sizeof buf, 0) == 4);
    CHECK_STR(buf, "ping");

    CHECK(pf_sock_send(server, "pong!", 5, 0) == 5);
    CHECK(pf_sock_recv(client, buf, sizeof buf, 0) == 5);
    CHECK_MEM(buf, "pong!", 5);

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_tcp_large(int seed, int rep) {
    enum { SIZE = 8192 };
    pf_sock_t client, server, listener;
    unsigned char out[SIZE], in[SIZE];
    size_t got = 0, i;

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);
    for (i = 0; i < SIZE; i++)
        out[i] = (unsigned char)(i ^ seed);

    /* It fits the socket buffers, so one thread can do both ends. */
    CHECK(pf_sock_send(client, out, SIZE, 0) == SIZE);
    while (got < SIZE) {
        pf_sock_ssize_t n = pf_sock_recv(server, in + got, SIZE - got, 0);
        CHECK(n > 0);
        got += (size_t)n;
    }
    CHECK_MEM(in, out, SIZE);

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_peer_close(int seed, int rep) {
    pf_sock_t client, server, listener;
    char buf[8];

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);

    pf_sock_close(client);
    /* An orderly shutdown reads as zero bytes. */
    CHECK(pf_sock_recv(server, buf, sizeof buf, 0) == 0);

    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_names(int seed, int rep) {
    pf_sock_t client, server, listener;
    struct sockaddr_in local, peer;
    pf_socklen_t llen = sizeof local, plen = sizeof peer;
    char text[64];

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);

    CHECK(pf_sock_getsockname(client, (pf_sockaddr_t *)&local, &llen) == 0);
    CHECK(pf_sock_getpeername(client, (pf_sockaddr_t *)&peer, &plen) == 0);
    CHECK(local.sin_family == AF_INET && peer.sin_family == AF_INET);
    CHECK(local.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    CHECK(peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    CHECK(local.sin_port != peer.sin_port);

    /* The server sees the same connection from the other side. */
    llen = sizeof local;
    plen = sizeof peer;
    {
        struct sockaddr_in slocal, speer;
        pf_socklen_t sl = sizeof slocal, sp = sizeof speer;
        CHECK(pf_sock_getsockname(server, (pf_sockaddr_t *)&slocal, &sl) == 0);
        CHECK(pf_sock_getpeername(server, (pf_sockaddr_t *)&speer, &sp) == 0);
        CHECK(slocal.sin_port == peer.sin_port);
        CHECK(speer.sin_port == local.sin_port);
    }

    CHECK(pf_inet_ntop(AF_INET, &local.sin_addr, text, sizeof text) != NULL);
    CHECK_STR(text, "127.0.0.1");

    /* An unconnected socket has no peer. */
    pf_sock_close(client);
    client = pf_sock_open(AF_INET, SOCK_STREAM, 0);
    CHECK(client != PF_INVALID_SOCKET);
    plen = sizeof peer;
    CHECK(pf_sock_getpeername(client, (pf_sockaddr_t *)&peer, &plen) != 0);

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_connect_refused(int seed, int rep) {
    unsigned short port;
    pf_sock_t listener, client;

    NEED_NET();
    listener = listen_tcp(&port);
    CHECK(listener != PF_INVALID_SOCKET);
    pf_sock_close(listener); /* nobody listens at the port now */

    client = connect_tcp(port);
    CHECK(client == PF_INVALID_SOCKET);
    pf_sock_exit();
    return 0;
}

static int test_socket_udp(int seed, int rep) {
    pf_sock_t a, b;
    struct sockaddr_in addr, from;
    pf_socklen_t len = sizeof addr, flen = sizeof from;
    char buf[16] = { 0 };

    NEED_NET();
    a = pf_sock_open(AF_INET, SOCK_DGRAM, 0);
    b = pf_sock_open(AF_INET, SOCK_DGRAM, 0);
    CHECK(a != PF_INVALID_SOCKET && b != PF_INVALID_SOCKET);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(pf_sock_bind(a, (pf_sockaddr_t *)&addr, sizeof addr) == 0);
    CHECK(pf_sock_bind(b, (pf_sockaddr_t *)&addr, sizeof addr) == 0);
    CHECK(pf_sock_getsockname(a, (pf_sockaddr_t *)&addr, &len) == 0);

    CHECK(
        pf_sock_sendto(b, "datagram", 8, 0, (pf_sockaddr_t *)&addr, len) == 8
    );
    CHECK(
        pf_sock_recvfrom(a, buf, sizeof buf, 0, (pf_sockaddr_t *)&from, &flen)
        == 8
    );
    CHECK_STR(buf, "datagram");
    CHECK(from.sin_family == AF_INET);
    CHECK(from.sin_addr.s_addr == htonl(INADDR_LOOPBACK));

    pf_sock_close(a);
    pf_sock_close(b);
    pf_sock_exit();
    return 0;
}

static int test_socket_nonblocking(int seed, int rep) {
    pf_sock_t client, server, listener;
    char buf[8];

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);

    CHECK(pf_sock_set_nonblocking(server, 1) == 0);
    CHECK(pf_sock_recv(server, buf, sizeof buf, 0) < 0);
    CHECK(pf_sock_would_block());

    CHECK(pf_sock_send(client, "x", 1, 0) == 1);
    {
        /* The byte may take a moment to arrive. */
        int tries = 0;
        pf_sock_ssize_t n = -1;
        while (tries++ < 200 && (n = pf_sock_recv(server, buf, 8, 0)) < 0)
            test_sleep_ms(5);
        CHECK(n == 1);
    }

    CHECK(pf_sock_set_nonblocking(server, 0) == 0);
    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_poll(int seed, int rep) {
    pf_sock_t client, server, listener;
    pf_pollfd_t fd;

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);

    fd.fd = server;
    fd.events = POLLIN;
    fd.revents = 0;
    CHECK(pf_sock_poll(&fd, 1, 0) == 0); /* nothing yet */

    CHECK(pf_sock_send(client, "x", 1, 0) == 1);
    fd.revents = 0;
    CHECK(pf_sock_poll(&fd, 1, 2000) == 1);
    CHECK(fd.revents & POLLIN);

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_select(int seed, int rep) {
    pf_sock_t client, server, listener;
    pf_sockset_t set;
    pf_timeval_t tv;

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);

    FD_ZERO(&set);
    FD_SET(server, &set);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    CHECK(pf_sock_select((int)server + 1, &set, NULL, NULL, &tv) == 0);

    CHECK(pf_sock_send(client, "x", 1, 0) == 1);
    FD_ZERO(&set);
    FD_SET(server, &set);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    CHECK(pf_sock_select((int)server + 1, &set, NULL, NULL, &tv) == 1);
    CHECK(FD_ISSET(server, &set));

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_options(int seed, int rep) {
    pf_sock_t s;
    int value = -1;
    pf_socklen_t len = sizeof value;

    NEED_NET();
    s = pf_sock_open(AF_INET, SOCK_STREAM, 0);
    CHECK(s != PF_INVALID_SOCKET);

    CHECK(pf_sock_set_reuseaddr(s, 1) == 0);
    CHECK(
        pf_sock_getopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&value, &len) == 0
    );
    CHECK(value != 0);
    CHECK(pf_sock_set_reuseaddr(s, 0) == 0);
    len = sizeof value;
    CHECK(
        pf_sock_getopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&value, &len) == 0
    );
    CHECK(value == 0);

    CHECK(pf_sock_set_keepalive(s, 1) == 0);
    len = sizeof value;
    CHECK(
        pf_sock_getopt(s, SOL_SOCKET, SO_KEEPALIVE, (void *)&value, &len) == 0
    );
    CHECK(value != 0);

    CHECK(pf_sock_set_nodelay(s, 1) == 0);
    len = sizeof value;
    CHECK(
        pf_sock_getopt(s, IPPROTO_TCP, TCP_NODELAY, (void *)&value, &len) == 0
    );
    CHECK(value != 0);

    CHECK(pf_sock_set_linger(s, 1, 2) == 0);
    CHECK(pf_sock_set_recvtimeo(s, 1500) == 0);
    CHECK(pf_sock_set_sendtimeo(s, 1500) == 0);

    CHECK(pf_sock_set_recvbuf(s, 65536) == 0);
    len = sizeof value;
    CHECK(pf_sock_getopt(s, SOL_SOCKET, SO_RCVBUF, (void *)&value, &len) == 0);
    CHECK(value >= 4096);

    CHECK(pf_sock_set_sendbuf(s, 65536) == 0);
    len = sizeof value;
    CHECK(pf_sock_getopt(s, SOL_SOCKET, SO_SNDBUF, (void *)&value, &len) == 0);
    CHECK(value >= 4096);

    /* Setting through the generic function. */
    value = 1;
    CHECK(
        pf_sock_setopt(
            s, SOL_SOCKET, SO_REUSEADDR, (const void *)&value, sizeof value
        )
        == 0
    );

    pf_sock_close(s);
    pf_sock_exit();
    return 0;
}

static int test_socket_option_errors(int seed, int rep) {
    NEED_NET();
    /* Not a socket. */
    CHECK(pf_sock_set_reuseaddr(PF_INVALID_SOCKET, 1) != 0);
    CHECK(pf_sock_set_nodelay(PF_INVALID_SOCKET, 1) != 0);
    CHECK(pf_sock_set_nonblocking(PF_INVALID_SOCKET, 1) != 0);
    pf_sock_exit();
    return 0;
}

static int test_socket_recv_timeout(int seed, int rep) {
    pf_sock_t client, server, listener;
    char buf[4];
    double start;

    NEED_NET();
    CHECK(tcp_pair(&client, &server, &listener) == 0);
    CHECK(pf_sock_set_recvtimeo(server, 100) == 0);

    start = test_now();
    CHECK(pf_sock_recv(server, buf, sizeof buf, 0) < 0);
    CHECK(test_now() - start >= 0.05);
    CHECK(test_now() - start < 3.0);

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    pf_sock_exit();
    return 0;
}

static int test_socket_error_codes(int seed, int rep) {
    pf_sock_t s;

    NEED_NET();
    s = pf_sock_open(AF_INET, SOCK_STREAM, 0);
    CHECK(s != PF_INVALID_SOCKET);
    CHECK(pf_sock_set_nonblocking(s, 1) == 0);
    {
        char buf[1];
        /* Unconnected: fails with something other than "would block". */
        CHECK(pf_sock_recv(s, buf, 1, 0) < 0);
        CHECK(pf_sock_error() != 0);
        CHECK(!pf_sock_would_block());
    }
    pf_sock_close(s);
    pf_sock_exit();
    return 0;
}

static int test_socket_inet_conversion(int seed, int rep) {
    struct in_addr v4;
    struct in6_addr v6;
    char text[INET6_ADDRSTRLEN];

    CHECK(pf_inet_pton(AF_INET, "192.168.1.20", &v4) == 1);
    CHECK(v4.s_addr == htonl(0xC0A80114u));
    CHECK(pf_inet_ntop(AF_INET, &v4, text, sizeof text) != NULL);
    CHECK_STR(text, "192.168.1.20");

    CHECK(pf_inet_pton(AF_INET, "256.1.1.1", &v4) == 0);
    CHECK(pf_inet_pton(AF_INET, "not an address", &v4) == 0);
    CHECK(pf_inet_pton(AF_INET, "", &v4) == 0);

    CHECK(pf_inet_pton(AF_INET6, "::1", &v6) == 1);
    CHECK(pf_inet_ntop(AF_INET6, &v6, text, sizeof text) != NULL);
    CHECK_STR(text, "::1");
    CHECK(pf_inet_pton(AF_INET6, "2001:db8::ff00:42:8329", &v6) == 1);
    CHECK(pf_inet_ntop(AF_INET6, &v6, text, sizeof text) != NULL);
    CHECK_STR(text, "2001:db8::ff00:42:8329");
    CHECK(pf_inet_pton(AF_INET6, "1:2:3", &v6) == 0);

    /* The buffer must be large enough. */
    CHECK(pf_inet_pton(AF_INET, "255.255.255.255", &v4) == 1);
    CHECK(pf_inet_ntop(AF_INET, &v4, text, 8) == NULL);
    return 0;
}

static int test_socket_getaddrinfo(int seed, int rep) {
    pf_addrinfo_t hints, *res = NULL, *p;
    int found = 0;

    NEED_NET();
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;

    CHECK(pf_getaddrinfo("127.0.0.1", "8080", &hints, &res) == 0);
    CHECK(res != NULL);
    for (p = res; p; p = p->ai_next) {
        struct sockaddr_in *a = (struct sockaddr_in *)p->ai_addr;
        if (p->ai_family == AF_INET && a->sin_port == htons(8080)
            && a->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
            found = 1;
    }
    CHECK(found);

    {
        char host[64], serv[16];
        CHECK(
            pf_getnameinfo(
                res->ai_addr,
                (pf_socklen_t)res->ai_addrlen,
                host,
                sizeof host,
                serv,
                sizeof serv,
                NI_NUMERICHOST | NI_NUMERICSERV
            )
            == 0
        );
        CHECK_STR(host, "127.0.0.1");
        CHECK_STR(serv, "8080");
    }

    pf_freeaddrinfo(res);
    pf_freeaddrinfo(NULL);

    /* Names that cannot be resolved report an error. */
    res = NULL;
    CHECK(pf_getaddrinfo("999.999.999.999", NULL, &hints, &res) != 0);
    pf_sock_exit();
    return 0;
}

#ifndef _WIN32
static int test_socket_unix(int seed, int rep) {
    char path[TEST_PATH_MAX];
    struct sockaddr_un addr;
    pf_sock_t listener, client, server;
    char buf[8] = { 0 };

    NEED_NET();
    test_path(path, sizeof path, "sock");
    if (strlen(path) >= sizeof addr.sun_path) {
        pf_sock_exit();
        return SKIP("temporary path too long for a unix socket");
    }

    listener = pf_sock_open(AF_UNIX, SOCK_STREAM, 0);
    if (listener == PF_INVALID_SOCKET) {
        pf_sock_exit();
        return SKIP("unix sockets are not available here");
    }

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    CHECK(pf_sock_bind(listener, (pf_sockaddr_t *)&addr, sizeof addr) == 0);
    CHECK(pf_sock_listen(listener, 1) == 0);

    client = pf_sock_open_un(path);
    CHECK(client != PF_INVALID_SOCKET);
    server = pf_sock_accept(listener, NULL, NULL);
    CHECK(server != PF_INVALID_SOCKET);

    CHECK(pf_sock_send(client, "unix", 4, 0) == 4);
    CHECK(pf_sock_recv(server, buf, sizeof buf, 0) == 4);
    CHECK_STR(buf, "unix");

    pf_sock_close(client);
    pf_sock_close(server);
    pf_sock_close(listener);
    unlink(path);

    /* Nobody listens any more. */
    CHECK(pf_sock_open_un(path) == PF_INVALID_SOCKET);

    pf_sock_exit();
    return 0;
}

static int test_socket_unix_path_too_long(int seed, int rep) {
    char path[512];

    NEED_NET();
    memset(path, 'a', sizeof path - 1);
    path[sizeof path - 1] = '\0';
    CHECK(pf_sock_open_un(path) == PF_INVALID_SOCKET);
    pf_sock_exit();
    return 0;
}
#endif

pf_test_t suite_socket[] = {
    { test_socket_init_exit, "/socket/init_exit" },
    { test_socket_open_close, "/socket/open_close" },
    { test_socket_tcp_echo, "/socket/tcp_echo" },
    { test_socket_tcp_large, "/socket/tcp_large" },
    { test_socket_peer_close, "/socket/peer_close" },
    { test_socket_names, "/socket/names" },
    { test_socket_connect_refused, "/socket/connect_refused" },
    { test_socket_udp, "/socket/udp" },
    { test_socket_nonblocking, "/socket/nonblocking" },
    { test_socket_poll, "/socket/poll" },
    { test_socket_select, "/socket/select" },
    { test_socket_options, "/socket/options" },
    { test_socket_option_errors, "/socket/option_errors" },
    { test_socket_recv_timeout, "/socket/recv_timeout" },
    { test_socket_error_codes, "/socket/error_codes" },
    { test_socket_inet_conversion, "/socket/inet_conversion" },
    { test_socket_getaddrinfo, "/socket/getaddrinfo" },
#ifndef _WIN32
    { test_socket_unix, "/socket/unix" },
    { test_socket_unix_path_too_long, "/socket/unix_path_too_long" },
#endif
    { 0 },
};
