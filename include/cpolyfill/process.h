/** cpolyfill - Filling the gaps between C standards and compilers

    Processes: identifiers, starting programs with their standard streams
    connected to the caller, waiting for them, and the path of the running
    program.

    Stand-ins for POSIX functions:

        pf_pid_t                pid_t; `unsigned long` (DWORD) on Windows.
        pf_pid_t pf_getpid(void)
        pf_pid_t pf_getppid(void)
                The parent's id on Windows is that of the process that
                started this one (which may have ended since).
        FILE *pf_popen(const char *command, const char *mode)
        int pf_pclose(FILE *f)
                The command line runs in the shell (/bin/sh, cmd.exe on
                Windows) with UTF-8 everywhere.

    Starting programs (0, or a PF_E* code):

        int pf_spawn(pf_process_t *p, const char *const argv[],
                     const pf_spawn_opts_t *opts)
                Starts `argv[0]` with the arguments `argv` (NULL-terminated).
                Without PF_SPAWN_SEARCH_PATH `argv[0]` is a path (relative
                to the current directory); with it, a name without a
                directory is looked up in PATH (on Windows, also with
                ".exe"). PF_ENOENT if the program does not exist, PF_EACCES
                if it cannot be run. On Windows the arguments are quoted so
                the program's CommandLineToArgvW / CRT sees them unchanged;
                batch files (.bat, .cmd) are refused with PF_EINVAL, since
                cmd.exe cannot receive arguments safely.
        pf_spawn_opts_t         All zero means: inherit everything.
                                cwd     directory to start in, NULL: ours
                                env     "NAME=value" strings, NULL-
                                        terminated; NULL: our environment
                                stdio[3]  for stdin, stdout, stderr:
                                        .mode  PF_STDIO_INHERIT (the
                                               caller's), PF_STDIO_PIPE (a
                                               pipe to the caller, see
                                               below), PF_STDIO_NULL (the
                                               null device), PF_STDIO_FD
                                        .fd    with PF_STDIO_FD: a
                                               descriptor of the caller
                                flags   PF_SPAWN_SEARCH_PATH
                                        PF_SPAWN_STDERR_TO_STDOUT  stderr
                                            goes where stdout goes
                                        PF_SPAWN_NEW_GROUP  a new process
                                            group, so Ctrl+C in the
                                            terminal does not reach it
                                        PF_SPAWN_HIDE_WINDOW  Windows: no
                                            console window for a console
                                            program
        pf_process_t            pid     the process id
                                in      stream writing to its stdin
                                out     stream reading its stdout
                                err     stream reading its stderr
                                The streams exist for PF_STDIO_PIPE only
                                (`vt == NULL` otherwise); they belong to the
                                process (pf_stream_close one to close the
                                pipe early, such as stdin to signal the end
                                of the input).
        int pf_process_wait(pf_process_t *p, const struct timespec *deadline,
                            pf_exit_status_t *status)
                Waits until the process ends: 0, or PF_ETIMEDOUT once the
                absolute UTC `deadline` (NULL: none) passes. May be called
                again after it ended (the same status).
        int pf_process_try_wait(pf_process_t *p, pf_exit_status_t *status)
                Without waiting: 0 if it ended, PF_EAGAIN if not.
        pf_exit_status_t        exited  true if it ended by itself
                                code    its exit code (if `exited`)
                                signal  the signal that ended it (POSIX,
                                        if not `exited`)
        int pf_process_kill(pf_process_t *p, bool force)
                Asks the process to end (SIGTERM), or ends it at once
                (SIGKILL, with `force`). Windows has no way of asking: both
                end it (TerminateProcess, exit code 1). 0 also if it has
                already ended.
        int pf_process_communicate(pf_process_t *p, pf_stream_t *in,
                                   pf_stream_t *out, pf_stream_t *err)
                Copies `in` to the process's stdin pipe, then closes it,
                and its stdout and stderr pipes into `out` and `err`, all
                at once (no deadlock however much is written), until both
                pipes reach their end. A NULL `in` just closes stdin; a
                NULL `out` / `err` discards the output. Pipes that were not
                requested are skipped. The process may still be running
                when it returns: wait for it next. A process that stops
                reading its stdin early is not an error.
        int pf_process_free(pf_process_t *p)
                Closes the pipes and handles. A process that is still
                running goes on; on POSIX one that was never waited for
                stays a zombie until this process ends.

        int pf_process_run(const char *const argv[],
                           const pf_spawn_opts_t *opts, pf_stream_t *in,
                           pf_stream_t *out, pf_stream_t *err,
                           pf_exit_status_t *status)
                pf_spawn, pf_process_communicate and pf_process_wait in one
                call: stdin, stdout and stderr are pipes for each non-NULL
                stream and as in `opts` (or inherited) otherwise. `status`
                may be NULL.

    The program:

        ptrdiff_t pf_exe_path(char *buf, size_t size)
                The absolute path of the running executable: its length,
                PF_ERANGE, or PF_ENOSYS where the system does not say
                (OpenBSD). With `buf == NULL` only the length.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_PROCESS, on Windows:
    `getpid`, `getppid`, `popen`, `pclose`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_PROCESS_H
#define CPOLYFILL_PROCESS_H

#include <cpolyfill/io.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#if !PF_OS_WINDOWS
    #include <sys/types.h>
#endif
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#if PF_OS_WINDOWS
typedef unsigned long pf_pid_t;
#else
typedef pid_t pf_pid_t;
#endif

#define PF_STDIO_INHERIT 0
#define PF_STDIO_PIPE 1
#define PF_STDIO_NULL 2
#define PF_STDIO_FD 3

#define PF_SPAWN_SEARCH_PATH 0x1u
#define PF_SPAWN_STDERR_TO_STDOUT 0x2u
#define PF_SPAWN_NEW_GROUP 0x4u
#define PF_SPAWN_HIDE_WINDOW 0x8u

typedef struct pf_spawn_stdio_t {
    int mode;
    int fd;
} pf_spawn_stdio_t;

typedef struct pf_spawn_opts_t {
    const char *cwd;
    const char *const *env;
    pf_spawn_stdio_t stdio[3];
    unsigned flags;
} pf_spawn_opts_t;

typedef struct pf_exit_status_t {
    bool exited;
    int code;
    int signal;
} pf_exit_status_t;

typedef struct pf_process_t {
    pf_pid_t pid;
    pf_stream_t in;
    pf_stream_t out;
    pf_stream_t err;
    /* Private. */
    intptr_t pf__handle; /* HANDLE (Windows), pidfd (Linux) or -1 */
    bool pf__done;
    pf_exit_status_t pf__status;
} pf_process_t;

PF_BEGIN_DECLS

PF_API pf_pid_t pf_getpid(void);
PF_API pf_pid_t pf_getppid(void);
PF_API FILE *pf_popen(const char *command, const char *mode);
PF_API int pf_pclose(FILE *f);

PF_API int pf_spawn(
    pf_process_t *p, const char *const argv[], const pf_spawn_opts_t *opts
);
PF_API int pf_process_wait(
    pf_process_t *p, const struct timespec *deadline, pf_exit_status_t *status
);
PF_API int pf_process_try_wait(pf_process_t *p, pf_exit_status_t *status);
PF_API int pf_process_kill(pf_process_t *p, bool force);
PF_API int pf_process_communicate(
    pf_process_t *p, pf_stream_t *in, pf_stream_t *out, pf_stream_t *err
);
PF_API int pf_process_free(pf_process_t *p);
PF_API int pf_process_run(
    const char *const argv[],
    const pf_spawn_opts_t *opts,
    pf_stream_t *in,
    pf_stream_t *out,
    pf_stream_t *err,
    pf_exit_status_t *status
);

PF_API ptrdiff_t pf_exe_path(char *buf, size_t size);

PF_END_DECLS

#endif /* CPOLYFILL_PROCESS_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__PROCESS_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_PROCESS)) \
        || defined(PF_STD_NAMES_PROCESS))
    #define PF__PROCESS_STD_NAMES

    #if PF_OS_WINDOWS
        #undef getpid
        #define getpid pf_getpid
        #undef getppid
        #define getppid pf_getppid
        #undef popen
        #define popen pf_popen
        #undef pclose
        #define pclose pf_pclose
    #endif
#endif
