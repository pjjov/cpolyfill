/** cpolyfill - Filling the gaps between C standards and compilers

    Pipes: anonymous pipes between file descriptors, and named pipes that
    connect processes on the same machine.

    Anonymous pipes:

        int pf_pipe(int fds[2], unsigned flags)
                A pipe: `fds[0]` is the read end, `fds[1]` the write end,
                both close-on-exec (pass them to a child with pf_spawn's
                PF_STDIO_FD). On Windows a CRT pipe in binary mode whose
                handles are not inherited. `flags`: PF_PIPE_NONBLOCK, both
                ends do not block (PF_ENOSYS on Windows). 0 or a PF_E*
                code.

    Named pipes are connections, like local stream sockets: a server
    listens on a name and accepts clients, each connection carries bytes
    both ways. They are Windows named pipes (\\.\pipe\<name>) and AF_UNIX
    stream sockets elsewhere. On POSIX a `name` without '/' is a file in
    $XDG_RUNTIME_DIR, else $TMPDIR, else /tmp; one with '/' is the path of
    the socket file. The socket file is only accessible to the owner
    (0600); a Windows pipe to the owner and SYSTEM, and refuses remote
    clients.

        pf_pipe_server_t        A listening name.
        pf_pipe_t               One connection.

        int pf_pipe_listen(pf_pipe_server_t *srv, const char *name)
                PF_EADDRINUSE if a server already listens on `name`. On
                POSIX a socket file left behind by a server that is gone is
                replaced.
        int pf_pipe_accept(pf_pipe_server_t *srv, pf_pipe_t *p,
                           const struct timespec *deadline)
                Waits for a client: 0, or PF_ETIMEDOUT once the absolute UTC
                `deadline` (NULL: none) passes.
        int pf_pipe_server_close(pf_pipe_server_t *srv)
                Stops listening (and removes the socket file). Connections
                already accepted go on.
        int pf_pipe_connect(pf_pipe_t *p, const char *name,
                            const struct timespec *deadline)
                Connects to the server on `name`: PF_ENOENT if there is
                none; while the server is busy, waits until `deadline`
                (NULL: none), then PF_ETIMEDOUT.
        pf_ssize_t pf_pipe_read(pf_pipe_t *p, void *buf, size_t n)
                The bytes read, 0 once the other side has closed.
        pf_ssize_t pf_pipe_write(pf_pipe_t *p, const void *buf, size_t n)
                The bytes written; PF_EPIPE once the other side has closed
                (no SIGPIPE). On Windows a call moves at most UINT32_MAX
                bytes.
        int pf_pipe_close(pf_pipe_t *p)
        int pf_stream_from_pipe(pf_stream_t *s, pf_pipe_t *p, bool own)
                A pf_stream_t (<cpolyfill/io.h>) on the connection; `p`
                must stay where it is while the stream is used. Closed
                with the stream if `own`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_PIPE_H
#define CPOLYFILL_PIPE_H

#include <cpolyfill/io.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF_PIPE_NONBLOCK 0x1u

typedef struct pf_pipe_server_t {
    /* Private. */
    void *pf__impl;
} pf_pipe_server_t;

typedef struct pf_pipe_t {
    /* Private. */
    intptr_t pf__h;        /* HANDLE (Windows) or socket descriptor, -1 */
    unsigned pf__overlapped; /* Windows: opened for overlapped I/O */
} pf_pipe_t;

PF_BEGIN_DECLS

PF_API int pf_pipe(int fds[2], unsigned flags);

PF_API int pf_pipe_listen(pf_pipe_server_t *srv, const char *name);
PF_API int pf_pipe_accept(
    pf_pipe_server_t *srv, pf_pipe_t *p, const struct timespec *deadline
);
PF_API int pf_pipe_server_close(pf_pipe_server_t *srv);
PF_API int pf_pipe_connect(
    pf_pipe_t *p, const char *name, const struct timespec *deadline
);
PF_API pf_ssize_t pf_pipe_read(pf_pipe_t *p, void *buf, size_t n);
PF_API pf_ssize_t pf_pipe_write(pf_pipe_t *p, const void *buf, size_t n);
PF_API int pf_pipe_close(pf_pipe_t *p);
PF_API int pf_stream_from_pipe(pf_stream_t *s, pf_pipe_t *p, bool own);

PF_END_DECLS

#endif /* CPOLYFILL_PIPE_H */
