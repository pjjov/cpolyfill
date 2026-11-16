/** cpolyfill - Filling the gaps between C standards and compilers

    Starting a background service: detaching from the terminal (POSIX),
    a single-instance lock, a PID file, and systemd notifications.

        pf_daemon_opts_t o = { 0 };
        pf_daemon_t d;
        int rc;

        o.flags = PF_DAEMON_DETACH;
        o.lock_path = "/run/mydaemon.lock";
        o.pid_path = "/run/mydaemon.pid";
        rc = pf_daemon_start(&d, &o);
        if (rc == PF_DAEMON_PARENT)
            return 0;               // the daemon is up
        if (rc != 0)
            return 1;               // in the parent or the daemon
        pf_daemon_notify_ready();
        ... serve ...
        pf_daemon_stop(&d);

    Functions (0 or a PF_E* code):

        int pf_daemon_start(pf_daemon_t *d, const pf_daemon_opts_t *o)
                In order: detaches (with PF_DAEMON_DETACH), sets the umask
                and working directory, takes the lock, writes the PID file.
                With PF_DAEMON_DETACH the calling process forks twice; the
                original process waits until the daemon has finished
                starting and returns PF_DAEMON_PARENT (1), or the daemon's
                error (the daemon gets it too and should exit). The middle
                process exits by itself. Nothing else calls exit().
                Errors: PF_EBUSY (another instance holds the lock),
                PF_ETIMEDOUT (it held it past lock_timeout_ms), or what the
                system reported.
        pf_daemon_opts_t        All zero: no detaching, lock or PID file.
                                flags     PF_DAEMON_DETACH  double fork and
                                              setsid, then close inherited
                                              descriptors and point stdin,
                                              stdout and stderr to the null
                                              device (POSIX; ignored on
                                              Windows, where services are
                                              the equivalent)
                                          PF_DAEMON_KEEP_FDS  do not close
                                              inherited descriptors
                                          PF_DAEMON_KEEP_STDIO  keep
                                              stdin, stdout and stderr
                                          PF_DAEMON_SET_UMASK  apply
                                              `umask` (POSIX)
                                cwd       directory to change to, or NULL
                                lock_path lock file (never deleted), or
                                          NULL: locked with pf_lock_file,
                                          so a second open conflicts even
                                          in the same process
                                pid_path  PID file, written atomically, or
                                          NULL
                                umask     with PF_DAEMON_SET_UMASK
                                keep_fds, keep_fd_count  descriptors not
                                          to close
                                lock_timeout_ms  0: fail at once;
                                          UINT32_MAX: wait as long as it
                                          takes
        void pf_daemon_stop(pf_daemon_t *d)
                Releases the lock and removes the PID file, if it still
                holds this process's id.
        bool pf_daemon_is_running(const pf_daemon_t *d)
        int pf_daemon_ignore_signals(void)
                SIGHUP and SIGCHLD ignored (POSIX; 0 on Windows).
        int pf_daemon_notify(const char *state)
                Sends `state` ("READY=1", "STATUS=...", "WATCHDOG=1") to
                systemd's NOTIFY_SOCKET; 0 without one, or off Linux.
        int pf_daemon_notify_ready(void)
        int pf_daemon_notify_stopping(void)
        ptrdiff_t pf_daemon_default_lock_path(const char *name, char *buf,
                                              size_t size)
                `name` in the runtime directory (XDG_RUNTIME_DIR, ...),
                else the temporary one: the length, or PF_ERANGE /
                PF_E* (as pf_known_dir).

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_DAEMON_H
#define CPOLYFILL_DAEMON_H

#include <cpolyfill/platform.h>

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF_DAEMON_PARENT 1

#define PF_DAEMON_DETACH 0x1u
#define PF_DAEMON_KEEP_FDS 0x2u
#define PF_DAEMON_KEEP_STDIO 0x4u
#define PF_DAEMON_SET_UMASK 0x8u

PF_BEGIN_DECLS

typedef struct pf_daemon_opts_t {
    unsigned flags;
    const char *cwd;
    const char *lock_path;
    const char *pid_path;
    unsigned umask;
    const int *keep_fds;
    size_t keep_fd_count;
    uint32_t lock_timeout_ms;
} pf_daemon_opts_t;

typedef struct pf_daemon_t {
    /* Private. */
    bool pf__started;
    int pf__lock_fd;
    char *pf__pid_path;
    long pf__pid;
} pf_daemon_t;

PF_API int pf_daemon_start(pf_daemon_t *d, const pf_daemon_opts_t *o);
PF_API void pf_daemon_stop(pf_daemon_t *d);
PF_API bool pf_daemon_is_running(const pf_daemon_t *d);
PF_API int pf_daemon_ignore_signals(void);
PF_API int pf_daemon_notify(const char *state);
PF_API int pf_daemon_notify_ready(void);
PF_API int pf_daemon_notify_stopping(void);
PF_API ptrdiff_t
pf_daemon_default_lock_path(const char *name, char *buf, size_t size);

PF_END_DECLS

#endif /* CPOLYFILL_DAEMON_H */
