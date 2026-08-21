/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides cross-platform daemon/background-process
    helper functions for Windows and POSIX systems.

    ```c
        pf_daemon_t daemon;
        pf_daemon_options_t options;

        pf_daemon_options_t_init(&options);

        options.workingDirectory = "/";
        options.lockPath = "/run/mydaemon.lock";
        options.pidPath = "/run/mydaemon.pid";
        options.daemonize = 1;
        options.closeFds = 1;
        options.redirectStdio = 1;

        int rc = pf_daemon_start(&daemon, &options);

        if (rc == PF_DAEMON_PARENT)
            return 0;       // parent after daemonization

        if (rc != PF_DAEMON_OK)
            return 1;

        ...

        pf_daemon_stop(&daemon);
    ```

    Notes:

    - pf_daemon_start() NEVER calls exit().
    - daemonization is optional.
    - singleton locking is independent from daemonization.
    - POSIX uses an advisory lock on a persistent lock file.
    - Windows uses a named mutex.
    - PID files are optional and are separate from lock files.
    - state is stored in the caller-owned pf_daemon_t object.

    Last-updated: August 2026
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
**/

#ifndef POLYFILL_DAEMON
#define POLYFILL_DAEMON

#ifndef PF_API
    #define PF_API static inline
#endif

#ifndef PF_DAEMON_API
    #define PF_DAEMON_API static
#endif

#include <stddef.h>
#include <stdint.h>

enum pf_daemon_error {
    PF_DAEMON_OK = 0,

    /* Not an error: returned to the original parent after fork. */
    PF_DAEMON_PARENT = 1,

    PF_DAEMON_EINVAL = 2,
    PF_DAEMON_EFORK = 3,
    PF_DAEMON_ESETSID = 4,
    PF_DAEMON_ECHDIR = 5,
    PF_DAEMON_EIO = 6,
    PF_DAEMON_ELOCKED = 7,
    PF_DAEMON_EPERM = 8,
    PF_DAEMON_ENOMEM = 9,
    PF_DAEMON_ETIMEDOUT = 10,
    PF_DAEMON_ESIGNAL = 11,
    PF_DAEMON_ESTDIO = 12,
    PF_DAEMON_EUMASK = 13,
    PF_DAEMON_EPIDFILE = 14,
    PF_DAEMON_ELOCKFILE = 15,
    PF_DAEMON_EUNKNOWN = 255
};

enum {
    /*
     * Close all file descriptors except those listed in
     * preserveFds.
     */
    PF_DAEMON_DEFAULT_CLOSE_FDS = 1,

    /*
     * Redirect stdin/stdout/stderr to the platform null device.
     */
    PF_DAEMON_DEFAULT_REDIRECT_STDIO = 1,

    /*
     * Ignore SIGHUP/SIGCHLD during daemonization.
     * Disabled by default because signal handling is global process state.
     */
    PF_DAEMON_DEFAULT_IGNORE_SIGNALS = 0
};

typedef struct pf_daemon_error_info_t {
    int code;

    /*
     * POSIX:
     *     errno value at the time of failure.
     *
     * Windows:
     *     GetLastError() value.
     */
    uint32_t osError;
} pf_daemon_error_info_t;

typedef struct pf_daemon_options_t {
    /*
     * If non-NULL, daemonize changes to this directory.
     *
     * POSIX: chdir()
     * Windows: SetCurrentDirectoryA()
     */
    const char *workingDirectory;

    /*
     * POSIX:
     *     Complete lock-file path.
     *
     * Windows:
     *     Named mutex name.
     *
     * NULL disables singleton locking.
     */
    const char *lockPath;

    /*
     * Optional PID file.
     *
     * NULL disables PID-file support.
     */
    const char *pidPath;

    /*
     * Perform Unix-style daemonization.
     *
     * POSIX: double fork + setsid().
     *
     * Windows: no-op. Windows does not have a direct equivalent of
     * Unix daemonization; use a Windows service for service semantics.
     */
    int daemonize;

    /*
     * Close inherited file descriptors on POSIX.
     *
     * Windows: ignored.
     */
    int closeFds;

    /*
     * Redirect stdin/stdout/stderr to /dev/null on POSIX,
     * NUL on Windows.
     */
    int redirectStdio;

    /*
     * Change process umask on POSIX.
     *
     * Set changeUmask = 0 to preserve the existing umask.
     */
    int changeUmask;

    /*
     * POSIX umask value.
     *
     * Typical values are 022 or 027.
     */
    unsigned int umaskValue;

    /*
     * Do not modify these file descriptors when closeFds is enabled.
     *
     * The array is borrowed only during pf_daemon_start().
     */
    const int *preserveFds;
    size_t preserveFdCount;

    /*
     * If non-zero, daemonize may return PF_DAEMON_PARENT.
     *
     * This exists mainly for explicit callers that want to distinguish
     * parent/child behavior.
     *
     * pf_daemon_start() never calls exit().
     */
    int returnParent;

    /*
     * Singleton lock acquisition timeout in milliseconds.
     *
     * 0 = do not wait; fail immediately if already locked.
     *
     * UINT32_MAX = wait indefinitely.
     */
    uint32_t lockTimeoutMs;

    /*
     * Windows mutex namespace.
     *
     * NULL means "Local".
     *
     * Set to "Global" for a global Windows namespace.
     *
     * POSIX: ignored.
     */
    const char *windowsMutexNamespace;

    /*
     * Windows: whether to use UTF-8 interpretation for strings.
     *
     * This implementation expects UTF-8 and converts to UTF-16.
     */
    int windowsUtf8;
} pf_daemon_options_t;

typedef struct pf_daemon_t {
#ifdef _WIN32
    void *mutex;
#else
    int lockFd;
#endif

    int started;
    int ownsLock;
    int pidFileCreated;

    char lockPath[4096];
    char pidPath[4096];

    pf_daemon_error_info_t error;
} pf_daemon_t;

#ifdef _WIN32

    #include <windows.h>

#else

    #include <errno.h>
    #include <fcntl.h>
    #include <limits.h>
    #include <signal.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <time.h>
    #include <unistd.h>

    #ifdef __linux__
        #include <sys/socket.h>
        #include <sys/un.h>
    #endif

#endif

PF_API void pf_daemon_options_t_init(pf_daemon_options_t *options) {
    if (!options)
        return;

    memset(options, 0, sizeof(*options));

    options->daemonize = 0;
    options->closeFds = PF_DAEMON_DEFAULT_CLOSE_FDS;
    options->redirectStdio = PF_DAEMON_DEFAULT_REDIRECT_STDIO;
    options->changeUmask = 0;
    options->umaskValue = 022;
    options->returnParent = 1;
    options->lockTimeoutMs = 0;
#ifdef _WIN32
    options->windowsMutexNamespace = NULL;
    options->windowsUtf8 = 1;
#endif
}

PF_API void pf_daemon_init(pf_daemon_t *daemon) {
    if (!daemon)
        return;

    memset(daemon, 0, sizeof(*daemon));

#ifndef _WIN32
    daemon->lockFd = -1;
#endif

    daemon->error.code = PF_DAEMON_OK;
    daemon->error.osError = 0;
}

PF_API void pf_daemon_set_error(
    pf_daemon_t *daemon, int code, uint32_t osError
) {
    if (!daemon)
        return;

    daemon->error.code = code;
    daemon->error.osError = osError;
}

PF_API int pf_daemon_last_error(
    const pf_daemon_t *daemon, pf_daemon_error_info_t *info
) {
    if (!daemon || !info)
        return PF_DAEMON_EINVAL;

    *info = daemon->error;
    return PF_DAEMON_OK;
}

PF_API int pf_daemon_is_running(const pf_daemon_t *daemon) {
    return daemon && daemon->started && daemon->ownsLock;
}

#ifndef _WIN32

PF_API int pf_daemon_is_preserved_fd(
    int fd, const pf_daemon_options_t *options
) {
    size_t i;

    if (!options)
        return 0;

    for (i = 0; i < options->preserveFdCount; ++i) {
        if (options->preserveFds[i] == fd)
            return 1;
    }

    return 0;
}

PF_API int pf_daemon_closeFds(const pf_daemon_options_t *options) {
    long max_fd;
    int fd;

    max_fd = sysconf(_SC_OPEN_MAX);

    if (max_fd < 0) {
        /*
         * POSIX guarantees sysconf(_SC_OPEN_MAX) can fail.
         * Use a conservative fallback rather than looping forever.
         */
        max_fd = 1024;
    }

    for (fd = (int)max_fd; fd >= 0; --fd) {
        if (pf_daemon_is_preserved_fd(fd, options))
            continue;

        close(fd);
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_redirectStdio(
    pf_daemon_t *daemon, const pf_daemon_options_t *options
) {
    int fd;

    (void)options;

    fd = open("/dev/null", O_RDWR);

    if (fd < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_ESTDIO, (uint32_t)errno);
        return PF_DAEMON_ESTDIO;
    }

    if (dup2(fd, STDIN_FILENO) < 0 || dup2(fd, STDOUT_FILENO) < 0
        || dup2(fd, STDERR_FILENO) < 0) {

        int saved_errno = errno;

        if (fd > STDERR_FILENO)
            close(fd);

        pf_daemon_set_error(daemon, PF_DAEMON_ESTDIO, (uint32_t)saved_errno);

        return PF_DAEMON_ESTDIO;
    }

    if (fd > STDERR_FILENO)
        close(fd);

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_ignore_signals(pf_daemon_t *daemon) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_ESIGNAL, (uint32_t)errno);

        return PF_DAEMON_ESIGNAL;
    }

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_ESIGNAL, (uint32_t)errno);

        return PF_DAEMON_ESIGNAL;
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_daemonize(
    pf_daemon_t *daemon, const pf_daemon_options_t *options
) {
    pid_t pid;

    if (!daemon || !options)
        return PF_DAEMON_EINVAL;

    if (!options->daemonize)
        return PF_DAEMON_OK;

    /*
     * First fork.
     *
     * The parent receives PF_DAEMON_PARENT instead of being terminated
     * by the library.
     */
    pid = fork();

    if (pid < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_EFORK, (uint32_t)errno);

        return PF_DAEMON_EFORK;
    }

    if (pid > 0)
        return PF_DAEMON_PARENT;

    /*
     * Create a new session.
     */
    if (setsid() < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_ESETSID, (uint32_t)errno);

        return PF_DAEMON_ESETSID;
    }

    /*
     * Ignore terminal/session signals only when explicitly requested.
     */
    if (options->returnParent == 0) {
        /*
         * No special behavior here. This field controls parent handling;
         * signal handling is intentionally separate.
         */
    }

    /*
     * Second fork prevents the daemon from reacquiring a controlling
     * terminal.
     */
    pid = fork();

    if (pid < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_EFORK, (uint32_t)errno);

        return PF_DAEMON_EFORK;
    }

    if (pid > 0)
        return PF_DAEMON_PARENT;

    /*
     * Set a neutral umask only if requested.
     */
    if (options->changeUmask)
        umask((mode_t)options->umaskValue);

    if (options->workingDirectory) {
        if (chdir(options->workingDirectory) < 0) {
            pf_daemon_set_error(daemon, PF_DAEMON_ECHDIR, (uint32_t)errno);

            return PF_DAEMON_ECHDIR;
        }
    }

    if (options->closeFds) {
        int rc = pf_daemon_closeFds(options);

        if (rc != PF_DAEMON_OK)
            return rc;
    }

    /*
     * If closeFds was used, stdin/stdout/stderr are normally gone.
     * Redirect them back to the null device when requested.
     */
    if (options->redirectStdio) {
        int rc = pf_daemon_redirectStdio(daemon, options);

        if (rc != PF_DAEMON_OK)
            return rc;
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_build_lockPath(pf_daemon_t *daemon, const char *lockPath) {
    int n;

    if (!daemon || !lockPath || !lockPath[0])
        return PF_DAEMON_EINVAL;

    n = snprintf(daemon->lockPath, sizeof(daemon->lockPath), "%s", lockPath);

    if (n < 0 || (size_t)n >= sizeof(daemon->lockPath)) {
        pf_daemon_set_error(daemon, PF_DAEMON_ELOCKFILE, ENAMETOOLONG);

        return PF_DAEMON_ELOCKFILE;
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_default_lockPath(
    pf_daemon_t *daemon, const char *lock_name
) {
    const char *runtime;
    int n;

    if (!daemon || !lock_name || !lock_name[0])
        return PF_DAEMON_EINVAL;

    runtime = getenv("XDG_RUNTIME_DIR");

    if (!runtime || !runtime[0])
        runtime = "/tmp";

    n = snprintf(
        daemon->lockPath, sizeof(daemon->lockPath), "%s/%s", runtime, lock_name
    );

    if (n < 0 || (size_t)n >= sizeof(daemon->lockPath)) {
        pf_daemon_set_error(daemon, PF_DAEMON_ELOCKFILE, ENAMETOOLONG);

        return PF_DAEMON_ELOCKFILE;
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_lock_posix(
    pf_daemon_t *daemon, const char *path, uint32_t timeout_ms
) {
    int fd;
    int saved_errno;
    uint64_t elapsed = 0;

    if (!daemon || !path || !path[0])
        return PF_DAEMON_EINVAL;

    fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0640);

    if (fd < 0) {
        saved_errno = errno;

        pf_daemon_set_error(daemon, PF_DAEMON_ELOCKFILE, (uint32_t)saved_errno);

        return PF_DAEMON_ELOCKFILE;
    }

    for (;;) {
        if (lockf(fd, F_TLOCK, 0) == 0)
            break;

        saved_errno = errno;

        /*
         * EACCES/EAGAIN means another process owns the lock.
         * Other errors are genuine lock failures.
         */
        if (saved_errno != EACCES && saved_errno != EAGAIN
            && saved_errno != EINTR) {

            close(fd);

            pf_daemon_set_error(daemon, PF_DAEMON_EIO, (uint32_t)saved_errno);

            return PF_DAEMON_EIO;
        }

        if (timeout_ms == 0) {
            close(fd);

            pf_daemon_set_error(
                daemon, PF_DAEMON_ELOCKED, (uint32_t)saved_errno
            );

            return PF_DAEMON_ELOCKED;
        }

        if (timeout_ms != UINT32_MAX && elapsed >= timeout_ms) {

            close(fd);

            pf_daemon_set_error(
                daemon, PF_DAEMON_ETIMEDOUT, (uint32_t)saved_errno
            );

            return PF_DAEMON_ETIMEDOUT;
        }

        {
            struct timespec ts;

            ts.tv_sec = 0;
            ts.tv_nsec = 10000000L; /* 10 ms */

            while (nanosleep(&ts, &ts) < 0) {
                if (errno != EINTR)
                    break;
            }
        }

        if (timeout_ms != UINT32_MAX)
            elapsed += 10;
    }

    daemon->lockFd = fd;
    daemon->ownsLock = 1;

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_write_pid_file(pf_daemon_t *daemon, const char *path) {
    int fd;
    int saved_errno;
    int n;

    if (!daemon || !path || !path[0])
        return PF_DAEMON_EINVAL;

    n = snprintf(daemon->pidPath, sizeof(daemon->pidPath), "%s", path);

    if (n < 0 || (size_t)n >= sizeof(daemon->pidPath)) {
        pf_daemon_set_error(daemon, PF_DAEMON_EPIDFILE, ENAMETOOLONG);

        return PF_DAEMON_EPIDFILE;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);

    if (fd < 0) {
        saved_errno = errno;

        pf_daemon_set_error(daemon, PF_DAEMON_EPIDFILE, (uint32_t)saved_errno);

        return PF_DAEMON_EPIDFILE;
    }

    {
        char buffer[64];

        n = snprintf(buffer, sizeof(buffer), "%ld\n", (long)getpid());

        if (n < 0 || write(fd, buffer, (size_t)n) != (ssize_t)n) {

            saved_errno = errno;

            close(fd);
            unlink(path);

            pf_daemon_set_error(
                daemon, PF_DAEMON_EPIDFILE, (uint32_t)saved_errno
            );

            return PF_DAEMON_EPIDFILE;
        }
    }

    close(fd);

    daemon->pidFileCreated = 1;

    return PF_DAEMON_OK;
}

    #ifdef __linux__

PF_API int pf_daemon_systemd_notify(pf_daemon_t *daemon, const char *message) {
    const char *socket_path;
    struct sockaddr_un addr;
    int fd;
    size_t path_len;
    ssize_t result;

    if (!daemon || !message)
        return PF_DAEMON_EINVAL;

    socket_path = getenv("NOTIFY_SOCKET");

    if (!socket_path || !socket_path[0])
        return PF_DAEMON_OK;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    /*
     * systemd supports abstract namespace sockets beginning with '@'.
     */
    if (socket_path[0] == '@') {
        addr.sun_path[0] = '\0';

        path_len = strlen(socket_path + 1);

        if (path_len >= sizeof(addr.sun_path))
            return PF_DAEMON_EINVAL;

        memcpy(addr.sun_path + 1, socket_path + 1, path_len);
    } else {
        path_len = strlen(socket_path);

        if (path_len >= sizeof(addr.sun_path))
            return PF_DAEMON_EINVAL;

        memcpy(addr.sun_path, socket_path, path_len + 1);
    }

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);

    if (fd < 0) {
        pf_daemon_set_error(daemon, PF_DAEMON_EIO, (uint32_t)errno);

        return PF_DAEMON_EIO;
    }

    if (socket_path[0] == '@')
        path_len += 1;

    result = sendto(
        fd,
        message,
        strlen(message),
        0,
        (struct sockaddr *)&addr,
        (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_len)
    );

    if (result < 0) {
        int saved_errno = errno;

        close(fd);

        pf_daemon_set_error(daemon, PF_DAEMON_EIO, (uint32_t)saved_errno);

        return PF_DAEMON_EIO;
    }

    close(fd);

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_systemd_ready(pf_daemon_t *daemon) {
    return pf_daemon_systemd_notify(daemon, "READY=1");
}

PF_API int pf_daemon_systemd_stopping(pf_daemon_t *daemon) {
    return pf_daemon_systemd_notify(daemon, "STOPPING=1");
}

    #endif /* __linux__ */

#else /* _WIN32 */

PF_API int pf_daemon_utf8_to_wide(
    pf_daemon_t *daemon, const char *input, wchar_t *output, int output_count
) {
    int result;

    if (!input || !output || output_count <= 0)
        return PF_DAEMON_EINVAL;

    result = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, output_count
    );

    if (result == 0) {
        DWORD error = GetLastError();

        pf_daemon_set_error(daemon, PF_DAEMON_EINVAL, (uint32_t)error);

        return PF_DAEMON_EINVAL;
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_set_workingDirectory(
    pf_daemon_t *daemon, const char *path
) {
    wchar_t wide_path[32768];

    if (!path || !path[0])
        return PF_DAEMON_OK;

    if (pf_daemon_utf8_to_wide(
            daemon,
            path,
            wide_path,
            (int)(sizeof(wide_path) / sizeof(wide_path[0]))
        )
        != PF_DAEMON_OK)
        return PF_DAEMON_ECHDIR;

    if (!SetCurrentDirectoryW(wide_path)) {
        DWORD error = GetLastError();

        pf_daemon_set_error(daemon, PF_DAEMON_ECHDIR, (uint32_t)error);

        return PF_DAEMON_ECHDIR;
    }

    return PF_DAEMON_OK;
}

PF_API int pf_daemon_lock_windows(
    pf_daemon_t *daemon,
    const char *name,
    const char *namespace_name,
    uint32_t timeout_ms
) {
    wchar_t wide_name[32768];
    wchar_t full_name[32768];
    const char *ns;
    HANDLE mutex;
    DWORD wait_result;

    if (!daemon || !name || !name[0])
        return PF_DAEMON_EINVAL;

    /*
     * Windows named-object names cannot contain a backslash except as
     * part of the Local\ / Global\ namespace prefix.
     */
    if (strchr(name, '\\')) {
        pf_daemon_set_error(daemon, PF_DAEMON_EINVAL, ERROR_INVALID_NAME);

        return PF_DAEMON_EINVAL;
    }

    ns = namespace_name;

    if (!ns || !ns[0])
        ns = "Local";

    if (strcmp(ns, "Local") != 0 && strcmp(ns, "Global") != 0) {

        pf_daemon_set_error(daemon, PF_DAEMON_EINVAL, ERROR_INVALID_NAME);

        return PF_DAEMON_EINVAL;
    }

    {
        int n;

        n = snprintf(
            daemon->lockPath, sizeof(daemon->lockPath), "%s\\%s", ns, name
        );

        if (n < 0 || (size_t)n >= sizeof(daemon->lockPath)) {

            pf_daemon_set_error(
                daemon, PF_DAEMON_ELOCKFILE, ERROR_FILENAME_EXCED_RANGE
            );

            return PF_DAEMON_ELOCKFILE;
        }
    }

    if (pf_daemon_utf8_to_wide(
            daemon,
            daemon->lockPath,
            full_name,
            (int)(sizeof(full_name) / sizeof(full_name[0]))
        )
        != PF_DAEMON_OK)
        return PF_DAEMON_EINVAL;

    /*
     * Use FALSE for initial ownership.
     *
     * We explicitly wait for ownership below. This avoids the ambiguity
     * documented by Microsoft around CreateMutex(..., TRUE, ...).
     */
    mutex = CreateMutexW(NULL, FALSE, full_name);

    if (!mutex) {
        DWORD error = GetLastError();

        pf_daemon_set_error(daemon, PF_DAEMON_EIO, (uint32_t)error);

        return PF_DAEMON_EIO;
    }

    /*
     * Existing named mutex.
     */
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (timeout_ms == 0) {
            CloseHandle(mutex);

            pf_daemon_set_error(
                daemon, PF_DAEMON_ELOCKED, ERROR_ALREADY_EXISTS
            );

            return PF_DAEMON_ELOCKED;
        }
    }

    if (timeout_ms == UINT32_MAX)
        wait_result = WaitForSingleObject(mutex, INFINITE);
    else
        wait_result = WaitForSingleObject(mutex, (DWORD)timeout_ms);

    if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {

        /*
         * WAIT_ABANDONED means the previous owner died without releasing
         * the mutex. Ownership is transferred to this caller.
         */
        daemon->mutex = mutex;
        daemon->ownsLock = 1;

        return PF_DAEMON_OK;
    }

    if (wait_result == WAIT_TIMEOUT) {
        CloseHandle(mutex);

        pf_daemon_set_error(daemon, PF_DAEMON_ETIMEDOUT, WAIT_TIMEOUT);

        return PF_DAEMON_ETIMEDOUT;
    }

    {
        DWORD error = GetLastError();

        CloseHandle(mutex);

        pf_daemon_set_error(daemon, PF_DAEMON_EIO, (uint32_t)error);

        return PF_DAEMON_EIO;
    }
}

PF_API int pf_daemon_write_pid_file(pf_daemon_t *daemon, const char *path) {
    HANDLE file;
    wchar_t wide_path[32768];
    DWORD written;
    char buffer[64];
    int n;

    if (!daemon || !path || !path[0])
        return PF_DAEMON_EINVAL;

    n = snprintf(daemon->pidPath, sizeof(daemon->pidPath), "%s", path);

    if (n < 0 || (size_t)n >= sizeof(daemon->pidPath)) {
        pf_daemon_set_error(
            daemon, PF_DAEMON_EPIDFILE, ERROR_FILENAME_EXCED_RANGE
        );

        return PF_DAEMON_EPIDFILE;
    }

    if (pf_daemon_utf8_to_wide(
            daemon,
            path,
            wide_path,
            (int)(sizeof(wide_path) / sizeof(wide_path[0]))
        )
        != PF_DAEMON_OK)
        return PF_DAEMON_EPIDFILE;

    file = CreateFileW(
        wide_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();

        pf_daemon_set_error(daemon, PF_DAEMON_EPIDFILE, (uint32_t)error);

        return PF_DAEMON_EPIDFILE;
    }

    n = snprintf(
        buffer, sizeof(buffer), "%lu\r\n", (unsigned long)GetCurrentProcessId()
    );

    if (n < 0 || !WriteFile(file, buffer, (DWORD)n, &written, NULL)
        || written != (DWORD)n) {

        DWORD error = GetLastError();

        CloseHandle(file);

        DeleteFileW(wide_path);

        pf_daemon_set_error(daemon, PF_DAEMON_EPIDFILE, (uint32_t)error);

        return PF_DAEMON_EPIDFILE;
    }

    CloseHandle(file);

    daemon->pidFileCreated = 1;

    return PF_DAEMON_OK;
}

#endif /* _WIN32 */

PF_API int pf_daemon_start(
    pf_daemon_t *daemon, const pf_daemon_options_t *options
) {
    int rc;

    if (!daemon || !options) {
        return PF_DAEMON_EINVAL;
    }

    pf_daemon_init(daemon);

#ifndef _WIN32

    /*
     * Daemonization happens before acquiring the singleton lock.
     * Therefore the lock belongs to the final daemon process.
     */
    rc = pf_daemon_daemonize(daemon, options);

    if (rc != PF_DAEMON_OK)
        return rc;

    if (options->workingDirectory && !options->daemonize) {

        if (chdir(options->workingDirectory) < 0) {
            pf_daemon_set_error(daemon, PF_DAEMON_ECHDIR, (uint32_t)errno);

            return PF_DAEMON_ECHDIR;
        }
    }

    /*
     * Signal handling is deliberately opt-in.
     *
     * The public options structure does not expose this switch yet;
     * applications can call pf_daemon_ignore_signals() explicitly after
     * successful daemonization.
     */

    if (options->lockPath) {
        rc = pf_daemon_build_lockPath(daemon, options->lockPath);

        if (rc != PF_DAEMON_OK)
            return rc;

        rc = pf_daemon_lock_posix(
            daemon, daemon->lockPath, options->lockTimeoutMs
        );

        if (rc != PF_DAEMON_OK)
            return rc;
    }

#else

    /*
     * Windows does not perform Unix daemonization.
     *
     * The working directory option is still honored.
     */
    if (options->workingDirectory) {
        rc = pf_daemon_set_workingDirectory(daemon, options->workingDirectory);

        if (rc != PF_DAEMON_OK)
            return rc;
    }

    if (options->lockPath) {
        rc = pf_daemon_lock_windows(
            daemon,
            options->lockPath,
            options->windowsMutexNamespace,
            options->lockTimeoutMs
        );

        if (rc != PF_DAEMON_OK)
            return rc;
    }

#endif

    /*
     * PID file is intentionally separate from the singleton lock.
     */
    if (options->pidPath) {
        rc = pf_daemon_write_pid_file(daemon, options->pidPath);

        if (rc != PF_DAEMON_OK) {
            /*
             * Do not leave the singleton lock held if PID-file creation
             * failed.
             */
            pf_daemon_stop(daemon);
            return rc;
        }
    }

    daemon->started = 1;

    return PF_DAEMON_OK;
}

PF_API void pf_daemon_stop(pf_daemon_t *daemon) {
    if (!daemon)
        return;

#ifdef _WIN32

    if (daemon->mutex) {
        if (daemon->ownsLock)
            ReleaseMutex((HANDLE)daemon->mutex);

        CloseHandle((HANDLE)daemon->mutex);

        daemon->mutex = NULL;
        daemon->ownsLock = 0;
    }

    if (daemon->pidFileCreated && daemon->pidPath[0]) {

        wchar_t wide_path[32768];

        if (pf_daemon_utf8_to_wide(
                daemon,
                daemon->pidPath,
                wide_path,
                (int)(sizeof(wide_path) / sizeof(wide_path[0]))
            )
            == PF_DAEMON_OK) {

            DeleteFileW(wide_path);
        }

        daemon->pidFileCreated = 0;
    }

#else

    /*
     * IMPORTANT:
     *
     * Do not unlink the lock file.
     *
     * The lock is associated with the descriptor/inode and is released
     * when the descriptor is closed. Leaving the file in place prevents
     * pathname races between competing processes.
     */
    if (daemon->lockFd >= 0) {
        close(daemon->lockFd);
        daemon->lockFd = -1;
    }

    daemon->ownsLock = 0;

    if (daemon->pidFileCreated && daemon->pidPath[0]) {

        unlink(daemon->pidPath);
        daemon->pidFileCreated = 0;
    }

#endif

    daemon->started = 0;
}

#endif /* POLYFILL_DAEMON */
