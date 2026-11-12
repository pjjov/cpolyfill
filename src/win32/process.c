/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/process.h> on Windows: CreateProcessW with an explicit list
    of inherited handles, pipes through CRT descriptors, a thread per pipe
    to communicate.
*/

#include "../internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/process.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <tlhelp32.h>

PF_API pf_pid_t pf_getpid(void) { return GetCurrentProcessId(); }

PF_API pf_pid_t pf_getppid(void) {
    PROCESSENTRY32W e;
    DWORD self = GetCurrentProcessId(), parent = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    e.dwSize = sizeof(e);
    if (Process32FirstW(snap, &e)) {
        do {
            if (e.th32ProcessID == self) {
                parent = e.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snap, &e));
    }
    CloseHandle(snap);
    return parent;
}

PF_API FILE *pf_popen(const char *command, const char *mode) {
    pf__wstr wcmd, wmode;
    FILE *f = NULL;
    int rc;

    if (!command || !mode) {
        errno = EINVAL;
        return NULL;
    }
    rc = pf__wstr_from_utf8(&wcmd, command);
    if (rc) {
        pf__errno_fail(rc);
        return NULL;
    }
    rc = pf__wstr_from_utf8(&wmode, mode);
    if (rc == 0) {
        f = _wpopen((const wchar_t *)wcmd.ptr, (const wchar_t *)wmode.ptr);
        pf__wstr_free(&wmode);
    } else {
        pf__errno_fail(rc);
    }
    pf__wstr_free(&wcmd);
    return f;
}

PF_API int pf_pclose(FILE *f) { return _pclose(f); }

/* ---- Command lines and environment blocks ------------------------------ */

/* A growing UTF-8 string. */
typedef struct pf__buf {
    char *p;
    size_t len, cap;
} pf__buf;

static bool pf__buf_add(pf__buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        char *grown;
        while (cap < b->len + n + 1)
            cap *= 2;
        grown = (char *)realloc(b->p, cap);
        if (!grown)
            return false;
        b->p = grown;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
    return true;
}

static bool pf__buf_rep(pf__buf *b, char c, size_t n) {
    while (n--)
        if (!pf__buf_add(b, &c, 1))
            return false;
    return true;
}

/* One argument, quoted so CommandLineToArgvW and the CRT give it back
   unchanged: backslashes are literal unless they precede a quote. */
static bool pf__quote_arg(pf__buf *b, const char *arg) {
    const char *s;
    size_t slashes = 0;

    if (*arg && !strpbrk(arg, " \t\n\v\""))
        return pf__buf_add(b, arg, strlen(arg));
    if (!pf__buf_add(b, "\"", 1))
        return false;
    for (s = arg; *s; s++) {
        if (*s == '\\') {
            slashes++;
            continue;
        }
        if (*s == '"') {
            if (!pf__buf_rep(b, '\\', slashes * 2 + 1) || !pf__buf_add(b, s, 1))
                return false;
        } else if (!pf__buf_rep(b, '\\', slashes) || !pf__buf_add(b, s, 1)) {
            return false;
        }
        slashes = 0;
    }
    return pf__buf_rep(b, '\\', slashes * 2) && pf__buf_add(b, "\"", 1);
}

/* The UTF-16 environment block for "NAME=value" strings, or NULL. */
static wchar_t *pf__env_block(const char *const *env, int *rc) {
    size_t units = 1, k;
    wchar_t *block, *w;

    for (k = 0; env[k]; k++) {
        ptrdiff_t n = pf_utf8_to_utf16(env[k], PF_NTS, NULL, 0, 0);
        if (n < 0) {
            *rc = (int)n;
            return NULL;
        }
        units += (size_t)n + 1;
    }
    block = (wchar_t *)malloc(units * sizeof(wchar_t));
    if (!block) {
        *rc = PF_ENOMEM;
        return NULL;
    }
    w = block;
    for (k = 0; env[k]; k++) {
        ptrdiff_t n = pf_utf8_to_utf16(
            env[k], PF_NTS, (uint16_t *)w, units - (size_t)(w - block), 0
        );
        w += n + 1;
    }
    *w = L'\0';
    if (k == 0) /* an empty block needs two NULs */
        block[0] = L'\0';
    *rc = PF_OK;
    return block;
}

static bool pf__has_sep(const char *s) {
    return strchr(s, '/') || strchr(s, '\\') || strchr(s, ':');
}

static bool pf__ends_with_ci(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && _stricmp(s + n - m, suffix) == 0;
}

static bool pf__is_file(const char *path) {
    pf_stat_t st;
    return pf_stat(path, &st) == 0 && st.type != PF_FILE_DIR;
}

/* The program for `name`: a path in new memory (malloc), or NULL. */
static char *pf__resolve(const char *name, bool search, int *rc) {
    *rc = PF_OK;
    if (!search || pf__has_sep(name)) {
        char *copy = _strdup(name);
        if (!copy)
            *rc = PF_ENOMEM;
        return copy;
    }
    {
        char path[PF_PATH_MAX], cand[PF_PATH_MAX];
        ptrdiff_t n = pf_getenv("PATH", path, sizeof(path));
        bool ext = strchr(name, '.') != NULL;
        char *p, *end;

        if (n < 0)
            path[0] = '\0';
        for (p = path;; p = end + 1) {
            end = strchr(p, ';');
            if (end)
                *end = '\0';
            if (*p) {
                if (pf_path_join(cand, sizeof(cand), p, name) > 0) {
                    if (ext && pf__is_file(cand))
                        return _strdup(cand);
                    if (!ext && strlen(cand) + 5 < sizeof(cand)) {
                        strcat(cand, ".exe");
                        if (pf__is_file(cand))
                            return _strdup(cand);
                    }
                }
            }
            if (!end)
                break;
        }
        *rc = PF_ENOENT;
        return NULL;
    }
}

/* ---- Spawning ---------------------------------------------------------- */

static void pf__process_clear(pf_process_t *p) {
    memset(p, 0, sizeof(*p));
    p->pf__handle = (intptr_t)-1;
}

static HANDLE pf__proc(const pf_process_t *p) {
    return (HANDLE)p->pf__handle;
}

/* An inheritable duplicate of `h`, or NULL. */
static HANDLE pf__inheritable(HANDLE h) {
    HANDLE dup = NULL;
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return NULL;
    if (!DuplicateHandle(
            GetCurrentProcess(), h, GetCurrentProcess(), &dup, 0, TRUE,
            DUPLICATE_SAME_ACCESS
        ))
        return NULL;
    return dup;
}

PF_API int pf_spawn(
    pf_process_t *p, const char *const argv[], const pf_spawn_opts_t *opts
) {
    static const pf_spawn_opts_t none;
    static const DWORD std_ids[3] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE,
                                      STD_ERROR_HANDLE };
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    HANDLE child[3] = { NULL, NULL, NULL }, mine[3] = { NULL, NULL, NULL };
    HANDLE list[3];
    DWORD nlist = 0, flags;
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = NULL;
    SIZE_T attrs_size = 0;
    pf__buf cmd = { NULL, 0, 0 };
    pf__wstr wpath, wcmd, wcwd;
    bool have_path = false, have_cmd = false, have_cwd = false;
    wchar_t *env = NULL;
    char *path = NULL;
    int rc = PF_OK, k;

    if (!p)
        return PF_EINVAL;
    pf__process_clear(p);
    if (!argv || !argv[0] || !*argv[0])
        return PF_EINVAL;
    if (!opts)
        opts = &none;
    if (opts->flags
        & ~(PF_SPAWN_SEARCH_PATH | PF_SPAWN_STDERR_TO_STDOUT
            | PF_SPAWN_NEW_GROUP | PF_SPAWN_HIDE_WINDOW))
        return PF_EINVAL;
    for (k = 0; k < 3; k++)
        if (opts->stdio[k].mode < PF_STDIO_INHERIT
            || opts->stdio[k].mode > PF_STDIO_FD
            || (opts->stdio[k].mode == PF_STDIO_FD && opts->stdio[k].fd < 0))
            return PF_EINVAL;

    path = pf__resolve(argv[0], (opts->flags & PF_SPAWN_SEARCH_PATH) != 0, &rc);
    if (!path)
        return rc;
    /* cmd.exe would parse the arguments of a batch file again. */
    if (pf__ends_with_ci(path, ".bat") || pf__ends_with_ci(path, ".cmd")) {
        rc = PF_EINVAL;
        goto out;
    }
    for (k = 0; argv[k]; k++)
        if ((k > 0 && !pf__buf_add(&cmd, " ", 1))
            || !pf__quote_arg(&cmd, argv[k])) {
            rc = PF_ENOMEM;
            goto out;
        }
    if (cmd.len >= 32767) {
        rc = PF_E2BIG;
        goto out;
    }

    if ((rc = pf__wpath(&wpath, path)) != PF_OK)
        goto out;
    have_path = true;
    if ((rc = pf__wstr_from_utf8(&wcmd, cmd.p)) != PF_OK)
        goto out;
    have_cmd = true;
    if (opts->cwd) {
        if ((rc = pf__wpath(&wcwd, opts->cwd)) != PF_OK)
            goto out;
        have_cwd = true;
    }
    if (opts->env && !(env = pf__env_block(opts->env, &rc)))
        goto out;

    /* The child's standard handles, all inheritable copies we close. */
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    for (k = 0; k < 3; k++) {
        switch (opts->stdio[k].mode) {
        case PF_STDIO_PIPE: {
            HANDLE r, w;
            if (!CreatePipe(&r, &w, &sa, 0)) {
                rc = pf_error_last_win32();
                goto out;
            }
            child[k] = k == 0 ? r : w;
            mine[k] = k == 0 ? w : r;
            SetHandleInformation(mine[k], HANDLE_FLAG_INHERIT, 0);
            break;
        }
        case PF_STDIO_NULL:
            child[k] = CreateFileW(
                L"NUL", GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL
            );
            if (child[k] == INVALID_HANDLE_VALUE) {
                child[k] = NULL;
                rc = pf_error_last_win32();
                goto out;
            }
            break;
        case PF_STDIO_FD:
            child[k] = pf__inheritable((HANDLE)_get_osfhandle(opts->stdio[k].fd));
            if (!child[k]) {
                rc = PF_EBADF;
                goto out;
            }
            break;
        default:
            child[k] = pf__inheritable(GetStdHandle(std_ids[k]));
            break;
        }
    }
    if (opts->flags & PF_SPAWN_STDERR_TO_STDOUT) {
        if (child[2])
            CloseHandle(child[2]);
        child[2] = pf__inheritable(child[1]);
    }
    for (k = 0; k < 3; k++)
        if (child[k])
            list[nlist++] = child[k];

    /* Only these handles are inherited, not every inheritable one. */
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrs_size);
    attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrs_size);
    if (!attrs) {
        rc = PF_ENOMEM;
        goto out;
    }
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrs_size)) {
        free(attrs);
        attrs = NULL;
        rc = pf_error_last_win32();
        goto out;
    }
    if (nlist > 0
        && !UpdateProcThreadAttribute(
            attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list,
            nlist * sizeof(HANDLE), NULL, NULL
        )) {
        rc = pf_error_last_win32();
        goto out;
    }

    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = child[0];
    si.StartupInfo.hStdOutput = child[1];
    si.StartupInfo.hStdError = child[2];
    si.lpAttributeList = attrs;
    flags = CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT;
    if (opts->flags & PF_SPAWN_NEW_GROUP)
        flags |= CREATE_NEW_PROCESS_GROUP;
    if (opts->flags & PF_SPAWN_HIDE_WINDOW)
        flags |= CREATE_NO_WINDOW;

    if (!CreateProcessW(
            (const wchar_t *)wpath.ptr, (wchar_t *)wcmd.ptr, NULL, NULL,
            nlist > 0, flags, env, have_cwd ? (const wchar_t *)wcwd.ptr : NULL,
            &si.StartupInfo, &pi
        )) {
        rc = pf_error_last_win32();
        goto out;
    }
    CloseHandle(pi.hThread);
    p->pid = pi.dwProcessId;
    p->pf__handle = (intptr_t)pi.hProcess;

    for (k = 0; k < 3; k++) {
        pf_stream_t *s = k == 0 ? &p->in : k == 1 ? &p->out : &p->err;
        int fd;
        if (!mine[k])
            continue;
        fd = _open_osfhandle(
            (intptr_t)mine[k], (k == 0 ? _O_WRONLY : _O_RDONLY) | _O_BINARY
        );
        if (fd < 0)
            continue; /* mine[k] is closed below */
        mine[k] = NULL; /* now owned by the descriptor */
        pf_stream_from_fd(s, fd, true);
    }

out:
    for (k = 0; k < 3; k++) {
        if (child[k])
            CloseHandle(child[k]);
        if (mine[k])
            CloseHandle(mine[k]);
    }
    if (attrs) {
        DeleteProcThreadAttributeList(attrs);
        free(attrs);
    }
    if (have_path)
        pf__wstr_free(&wpath);
    if (have_cmd)
        pf__wstr_free(&wcmd);
    if (have_cwd)
        pf__wstr_free(&wcwd);
    free(env);
    free(cmd.p);
    free(path);
    return rc;
}

/* ---- Waiting ----------------------------------------------------------- */

static int pf__finish(pf_process_t *p, pf_exit_status_t *status) {
    if (!p->pf__done) {
        DWORD code = 0;
        if (!GetExitCodeProcess(pf__proc(p), &code))
            return pf_error_last_win32();
        p->pf__done = true;
        p->pf__status.exited = true;
        p->pf__status.code = (int)code;
        p->pf__status.signal = 0;
    }
    if (status)
        *status = p->pf__status;
    return PF_OK;
}

PF_API int pf_process_try_wait(pf_process_t *p, pf_exit_status_t *status) {
    DWORD rc;
    if (!p || p->pf__handle == (intptr_t)-1)
        return PF_EINVAL;
    if (p->pf__done)
        return pf__finish(p, status);
    rc = WaitForSingleObject(pf__proc(p), 0);
    if (rc == WAIT_OBJECT_0)
        return pf__finish(p, status);
    return rc == WAIT_TIMEOUT ? PF_EAGAIN : pf_error_last_win32();
}

PF_API int pf_process_wait(
    pf_process_t *p, const struct timespec *deadline, pf_exit_status_t *status
) {
    if (!p || p->pf__handle == (intptr_t)-1)
        return PF_EINVAL;
    for (;;) {
        DWORD ms = INFINITE, rc;
        if (p->pf__done)
            return pf__finish(p, status);
        if (deadline) {
            struct timespec now, left;
            if (pf_clock_gettime(PF_CLOCK_REALTIME, &now) != 0)
                return PF_EINVAL;
            left = pf_timespec_sub(*deadline, now);
            if (left.tv_sec < 0)
                ms = 0;
            else if (left.tv_sec >= 86400)
                ms = 86400000; /* checked again after a day */
            else
                ms = (DWORD)(left.tv_sec * 1000
                             + (left.tv_nsec + 999999) / 1000000);
        }
        rc = WaitForSingleObject(pf__proc(p), ms);
        if (rc == WAIT_OBJECT_0)
            return pf__finish(p, status);
        if (rc != WAIT_TIMEOUT)
            return pf_error_last_win32();
        if (ms == 0)
            return PF_ETIMEDOUT;
    }
}

PF_API int pf_process_kill(pf_process_t *p, bool force) {
    (void)force;
    if (!p || p->pf__handle == (intptr_t)-1)
        return PF_EINVAL;
    if (p->pf__done)
        return PF_OK;
    if (!TerminateProcess(pf__proc(p), 1)) {
        /* Access is denied once it has ended. */
        if (WaitForSingleObject(pf__proc(p), 0) == WAIT_OBJECT_0)
            return PF_OK;
        return pf_error_last_win32();
    }
    return PF_OK;
}

PF_API int pf_process_free(pf_process_t *p) {
    if (!p)
        return PF_EINVAL;
    if (p->in.vt)
        pf_stream_close(&p->in);
    if (p->out.vt)
        pf_stream_close(&p->out);
    if (p->err.vt)
        pf_stream_close(&p->err);
    if (p->pf__handle != (intptr_t)-1)
        CloseHandle(pf__proc(p));
    p->pf__handle = (intptr_t)-1;
    return PF_OK;
}

/* ---- Communicating ------------------------------------------------------ */

/* One direction: copies `src` to `dst` (NULL: discard), then closes the
   pipe end, which is `src` for output and `dst` for input. */
typedef struct pf__pump {
    pf_stream_t *src, *dst;
    pf_stream_t *pipe;
    bool to_child;
    int rc;
} pf__pump;

static int pf__pump_run(void *arg) {
    pf__pump *pu = (pf__pump *)arg;
    unsigned char buf[16384];

    pu->rc = PF_OK;
    if (pu->src) {
        for (;;) {
            pf_ssize_t got = pf_stream_read(pu->src, buf, sizeof(buf));
            if (got <= 0) {
                /* A pipe whose writer went away reads as the end. */
                if (got < 0 && !(!pu->to_child && got == PF_EPIPE))
                    pu->rc = (int)got;
                break;
            }
            if (pu->dst) {
                int rc = pf_stream_write_all(pu->dst, buf, (size_t)got);
                if (rc != PF_OK) {
                    /* The process stopped reading its stdin: not an error. */
                    if (!(pu->to_child && (rc == PF_EPIPE || rc == PF_EINVAL)))
                        pu->rc = rc;
                    if (pu->to_child)
                        break;
                    pu->dst = NULL; /* keep draining the process's output */
                }
            }
        }
    }
    pf_stream_close(pu->pipe);
    return 0;
}

PF_API int pf_process_communicate(
    pf_process_t *p, pf_stream_t *in, pf_stream_t *out, pf_stream_t *err
) {
    pf__pump pumps[3];
    int n = 0, k, rc = PF_OK;

    if (!p)
        return PF_EINVAL;
    if (p->in.vt) {
        if (in) {
            pumps[n].src = in;
            pumps[n].dst = &p->in;
            pumps[n].pipe = &p->in;
            pumps[n].to_child = true;
            n++;
        } else {
            pf_stream_close(&p->in);
        }
    }
    if (p->out.vt) {
        pumps[n].src = &p->out;
        pumps[n].dst = out;
        pumps[n].pipe = &p->out;
        pumps[n].to_child = false;
        n++;
    }
    if (p->err.vt) {
        pumps[n].src = &p->err;
        pumps[n].dst = err;
        pumps[n].pipe = &p->err;
        pumps[n].to_child = false;
        n++;
    }

#if PF_CONFIG_THREADS
    {
        pf_thrd_t threads[2];
        bool started[2] = { false, false };
        /* The first pumps get threads, the last runs here. */
        for (k = 0; k + 1 < n; k++)
            started[k] =
                pf_thrd_create(&threads[k], pf__pump_run, &pumps[k])
                == pf_thrd_success;
        if (n > 0)
            pf__pump_run(&pumps[n - 1]);
        for (k = 0; k + 1 < n; k++) {
            if (started[k])
                pf_thrd_join(threads[k], NULL);
            else
                pf__pump_run(&pumps[k]);
        }
    }
#else
    /* One at a time: a process filling one pipe while we wait on another
       would block, so only one pipe is allowed. */
    if (n > 1)
        return PF_ENOSYS;
    if (n == 1)
        pf__pump_run(&pumps[0]);
#endif
    for (k = 0; k < n; k++)
        if (rc == PF_OK)
            rc = pumps[k].rc;
    return rc;
}
