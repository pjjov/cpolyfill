/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/pipe.h> */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/pipe.h>
#include <cpolyfill/process.h>
#include <cpolyfill/random.h>
#include <cpolyfill/time.h>

#include <stdio.h>
#include <string.h>
#if PF_OS_POSIX
    #include <fcntl.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

#include "check.h"
#include "files.h"

static struct timespec after_ms(long ms) {
    struct timespec now;
    PF_ASSERT_ALWAYS_EQ(pf_clock_gettime(PF_CLOCK_REALTIME, &now), 0);
    return pf_timespec_add(now, pf_timespec_from_ns((int64_t)ms * 1000000));
}

/* A name no other run uses. */
static const char *pipe_name(void) {
    static char buf[64];
    snprintf(
        buf,
        sizeof(buf),
        "cpolyfill-test-%lu-%08lx",
        (unsigned long)pf_getpid(),
        (unsigned long)pf_random_u32()
    );
    return buf;
}

static int test_anonymous(int seed, int i) {
    int fds[2];
    char buf[8];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_pipe(fds, 0), 0);
#if PF_OS_POSIX
    PF_ASSERT_ALWAYS(fcntl(fds[0], F_GETFD) & FD_CLOEXEC);
    PF_ASSERT_ALWAYS(fcntl(fds[1], F_GETFD) & FD_CLOEXEC);
    PF_ASSERT_ALWAYS_EQ(fcntl(fds[0], F_GETFL) & O_NONBLOCK, 0);
#endif
    PF_ASSERT_ALWAYS_EQ(pf_write(fds[1], "abc", 3), 3);
    PF_ASSERT_ALWAYS_EQ(pf_read(fds[0], buf, sizeof(buf)), 3);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "abc", 3);
    PF_ASSERT_ALWAYS_EQ(pf_close(fds[1]), 0);
    PF_ASSERT_ALWAYS_EQ(pf_read(fds[0], buf, sizeof(buf)), 0);
    PF_ASSERT_ALWAYS_EQ(pf_close(fds[0]), 0);

#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_EQ(pf_pipe(fds, PF_PIPE_NONBLOCK), PF_ENOSYS);
#else
    PF_ASSERT_ALWAYS_EQ(pf_pipe(fds, PF_PIPE_NONBLOCK), 0);
    PF_ASSERT_ALWAYS(fcntl(fds[0], F_GETFL) & O_NONBLOCK);
    PF_ASSERT_ALWAYS(fcntl(fds[1], F_GETFL) & O_NONBLOCK);
    PF_ASSERT_ALWAYS(fcntl(fds[0], F_GETFD) & FD_CLOEXEC);
    PF_ASSERT_ALWAYS_EQ(pf_read(fds[0], buf, sizeof(buf)), -1);
    pf_close(fds[0]);
    pf_close(fds[1]);
#endif
    PF_ASSERT_ALWAYS_EQ(pf_pipe(NULL, 0), PF_EINVAL);
    return 0;
}

static int test_named(int seed, int i) {
    const char *name = pipe_name();
    pf_pipe_server_t srv;
    pf_pipe_t c, s;
    char buf[16];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, name, NULL), PF_ENOENT);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, name), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, name, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s, NULL), 0);

    PF_ASSERT_ALWAYS_EQ(pf_pipe_write(&c, "request", 7), 7);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&s, buf, sizeof(buf)), 7);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "request", 7);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_write(&s, "reply", 5), 5);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&c, buf, sizeof(buf)), 5);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "reply", 5);

    /* The client leaves: the end, then a broken pipe. */
    PF_ASSERT_ALWAYS_EQ(pf_pipe_close(&c), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&s, buf, sizeof(buf)), 0);
    {
        pf_ssize_t r = 0;
        int k;
        for (k = 0; k < 4 && r >= 0; k++)
            r = pf_pipe_write(&s, "x", 1);
        PF_ASSERT_ALWAYS_EQ(r, PF_EPIPE);
    }
    PF_ASSERT_ALWAYS_EQ(pf_pipe_close(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_close(&s), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&s, buf, 1), PF_EBADF);

    PF_ASSERT_ALWAYS_EQ(pf_pipe_server_close(&srv), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, name, NULL), PF_ENOENT);
    return 0;
}

static int test_clients(int seed, int i) {
    const char *name = pipe_name();
    pf_pipe_server_t srv;
    pf_pipe_t c[3], s[3];
    int k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, name), 0);
    for (k = 0; k < 3; k++)
        PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c[k], name, NULL), 0);
    for (k = 0; k < 3; k++)
        PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s[k], NULL), 0);
    for (k = 0; k < 3; k++) {
        char b = (char)('a' + k);
        PF_ASSERT_ALWAYS_EQ(pf_pipe_write(&c[k], &b, 1), 1);
    }
    /* Each connection carries its own bytes, whichever order the server
       accepted them in. */
    {
        char seen[3] = { 0, 0, 0 };
        for (k = 0; k < 3; k++) {
            char b;
            PF_ASSERT_ALWAYS_EQ(pf_pipe_read(&s[k], &b, 1), 1);
            PF_ASSERT_ALWAYS(b >= 'a' && b <= 'c');
            seen[b - 'a'] = 1;
        }
        PF_ASSERT_ALWAYS(seen[0] && seen[1] && seen[2]);
    }
    /* Closing the server leaves the connections alone. */
    PF_ASSERT_ALWAYS_EQ(pf_pipe_server_close(&srv), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_write(&s[0], "!", 1), 1);
    for (k = 0; k < 3; k++) {
        pf_pipe_close(&c[k]);
        pf_pipe_close(&s[k]);
    }
    return 0;
}

static int test_in_use(int seed, int i) {
    const char *name = pipe_name();
    pf_pipe_server_t a, b;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&a, name), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&b, name), PF_EADDRINUSE);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_server_close(&a), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&b, name), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_server_close(&b), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_server_close(&b), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&a, ""), PF_EINVAL);
    return 0;
}

static int test_accept_timeout(int seed, int i) {
    const char *name = pipe_name();
    pf_pipe_server_t srv;
    pf_pipe_t c, s;
    struct timespec dl = after_ms(50);
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, name), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s, &dl), PF_ETIMEDOUT);
    {
        struct timespec now;
        pf_clock_gettime(PF_CLOCK_REALTIME, &now);
        PF_ASSERT_ALWAYS_GE(pf_timespec_cmp(now, dl), 0);
    }
    /* A client after a timed-out accept is still accepted. */
    dl = after_ms(5000);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, name, &dl), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s, &dl), 0);
    pf_pipe_close(&c);
    pf_pipe_close(&s);
    pf_pipe_server_close(&srv);
    return 0;
}

static int test_socket_file(int seed, int i) {
#if PF_OS_POSIX
    char dir[PF_PATH_MAX];
    const char *path;
    pf_pipe_server_t srv;
    pf_pipe_t c, s;
    struct stat st;
    (void)seed, (void)i;

    test_tmpdir(dir, sizeof(dir));
    path = test_path(0, dir, "p.sock");

    /* A socket file left behind by a server that is gone. */
    {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un a;
        memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        strcpy(a.sun_path, path);
        PF_ASSERT_ALWAYS_EQ(bind(fd, (struct sockaddr *)&a, sizeof(a)), 0);
        close(fd);
    }
    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, path), 0);
    PF_ASSERT_ALWAYS_EQ(stat(path, &st), 0);
    PF_ASSERT_ALWAYS(S_ISSOCK(st.st_mode));
    PF_ASSERT_ALWAYS_EQ(st.st_mode & 0777, 0600);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, path, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s, NULL), 0);
    pf_pipe_close(&c);
    pf_pipe_close(&s);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_server_close(&srv), 0);
    PF_ASSERT_ALWAYS(stat(path, &st) != 0);

    /* Not a socket: left alone. */
    test_write(path, "data");
    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, path), PF_EADDRINUSE);
    PF_ASSERT_ALWAYS_EQ(pf_remove_all(dir), 0);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("socket file paths");
#endif
}

static int test_stream(int seed, int i) {
    const char *name = pipe_name();
    pf_pipe_server_t srv;
    pf_pipe_t c, s;
    pf_stream_t w, r;
    char buf[16];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_pipe_listen(&srv, name), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_connect(&c, name, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_pipe_accept(&srv, &s, NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_from_pipe(&w, &c, true), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_from_pipe(&r, &s, true), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_puts(&w, "streamed"), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&w), 0);
    PF_ASSERT_ALWAYS_EQ(pf_stream_read_full(&r, buf, sizeof(buf)), 8);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "streamed", 8);
    PF_ASSERT_ALWAYS_EQ(pf_stream_close(&r), 0);
    pf_pipe_server_close(&srv);
    return 0;
}

pf_test_t suite_pipe[] = {
    { test_anonymous, "anonymous" },
    { test_named, "named" },
    { test_clients, "clients" },
    { test_in_use, "in_use" },
    { test_accept_timeout, "accept_timeout" },
    { test_socket_file, "socket_file" },
    { test_stream, "stream" },
    { 0 },
};
