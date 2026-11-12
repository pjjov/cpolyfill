/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/term.h>: termios and TIOCGWINSZ, or the console API on
    Windows.
*/

#include "internal.h"

#include <cpolyfill/stdc.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/term.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <io.h>
    #include <windows.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
    #ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
        #define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
    #endif
#else
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

/* An environment variable into `buf`: true if set (truncated values count
   as unset). */
static bool pf__env(const char *name, char *buf, size_t size) {
    return pf_getenv(name, buf, size) >= 0;
}

#if PF_OS_WINDOWS

static HANDLE pf__term_handle(int fd) {
    return (HANDLE)_get_osfhandle(fd);
}

/* The console mode of `fd`: 0, or PF_EBADF / PF_ENOTTY. */
static int pf__console_mode(int fd, HANDLE *h, DWORD *mode) {
    *h = pf__term_handle(fd);
    if (*h == INVALID_HANDLE_VALUE)
        return PF_EBADF;
    if (!GetConsoleMode(*h, mode))
        return PF_ENOTTY;
    return PF_OK;
}

PF_API int pf_isatty(int fd) {
    HANDLE h;
    DWORD mode;
    int rc = pf__console_mode(fd, &h, &mode);
    if (rc)
        return pf__errno_fail(rc), 0;
    return 1;
}

/* mintty, MSYS2 and Cygwin connect programs to named pipes called
   \msys-<hash>-ptyN-to-master or \cygwin-<hash>-ptyN-from-master. */
static bool pf__is_mintty(HANDLE h) {
    struct {
        DWORD len;
        WCHAR name[MAX_PATH];
    } info;
    char name[MAX_PATH * 3];
    ptrdiff_t n;

    if (GetFileType(h) != FILE_TYPE_PIPE)
        return false;
    memset(&info, 0, sizeof(info));
    if (!GetFileInformationByHandleEx(h, FileNameInfo, &info, sizeof(info)))
        return false;
    n = pf_utf16_to_utf8(
        (const uint16_t *)info.name, info.len / sizeof(WCHAR), name,
        sizeof(name), PF_UNICODE_REPLACE
    );
    if (n < 0)
        return false;
    return (strncmp(name, "\\msys-", 6) == 0
            || strncmp(name, "\\cygwin-", 8) == 0)
        && strstr(name, "-pty") != NULL && strstr(name, "-master") != NULL;
}

PF_API bool pf_term_is_terminal(int fd) {
    HANDLE h;
    DWORD mode;
    int rc = pf__console_mode(fd, &h, &mode);
    if (rc == PF_OK)
        return true;
    return rc == PF_ENOTTY && pf__is_mintty(h);
}

PF_API int pf_term_size(int fd, int *cols, int *rows) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE h;
    DWORD mode;
    int rc = pf__console_mode(fd, &h, &mode);
    bool ok;

    if (rc)
        return rc;
    ok = GetConsoleScreenBufferInfo(h, &info) != 0;
    if (!ok) {
        /* An input handle: ask the console's screen buffer. */
        HANDLE out = CreateFileW(
            L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL
        );
        if (out != INVALID_HANDLE_VALUE) {
            ok = GetConsoleScreenBufferInfo(out, &info) != 0;
            CloseHandle(out);
        }
    }
    if (!ok)
        return pf_error_last_win32();
    if (cols)
        *cols = info.srWindow.Right - info.srWindow.Left + 1;
    if (rows)
        *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return PF_OK;
}

/* The saved state: the console mode. */
    #define PF__TERM_MAGIC 0x50465457u /* "PFTW" */

typedef struct pf__term_saved {
    uint32_t magic;
    DWORD mode;
} pf__term_saved;

PF_API int pf_term_raw(int fd, unsigned flags, pf_term_state_t *saved) {
    HANDLE h;
    DWORD mode, raw;
    int rc = pf__console_mode(fd, &h, &mode);

    if (rc)
        return rc;
    if (saved) {
        pf__term_saved s;
        memset(saved, 0, sizeof(*saved));
        s.magic = PF__TERM_MAGIC;
        s.mode = mode;
        memcpy(saved->pf__u.data, &s, sizeof(s));
    }
    raw = mode & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    if (!(flags & PF_TERM_KEEP_SIGNALS))
        raw &= ~(DWORD)ENABLE_PROCESSED_INPUT;
    if (!SetConsoleMode(h, raw | ENABLE_VIRTUAL_TERMINAL_INPUT)
        && !SetConsoleMode(h, raw))
        return pf_error_last_win32();
    return PF_OK;
}

PF_API int pf_term_restore(int fd, const pf_term_state_t *saved) {
    pf__term_saved s;
    HANDLE h;
    DWORD mode;
    int rc;

    if (!saved)
        return PF_EINVAL;
    memcpy(&s, saved->pf__u.data, sizeof(s));
    if (s.magic != PF__TERM_MAGIC)
        return PF_EINVAL;
    rc = pf__console_mode(fd, &h, &mode);
    if (rc)
        return rc;
    return SetConsoleMode(h, s.mode) ? PF_OK : pf_error_last_win32();
}

PF_API int pf_term_echo(int fd, bool on) {
    HANDLE h;
    DWORD mode;
    int rc = pf__console_mode(fd, &h, &mode);

    if (rc)
        return rc;
    /* Echo needs line input. */
    mode = on ? mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT
              : mode & ~(DWORD)ENABLE_ECHO_INPUT;
    return SetConsoleMode(h, mode) ? PF_OK : pf_error_last_win32();
}

/* What pf_term_enable_vt and pf_term_set_utf8 changed, put back at exit. */
static struct {
    volatile uint32_t lock;
    bool registered;
    int count;
    HANDLE handle[8];
    DWORD mode[8];
    bool cp_saved;
    UINT out_cp, in_cp;
} pf__term_restore_list;

static void pf__term_at_exit(void) {
    int k;
    pf__spin_lock(&pf__term_restore_list.lock);
    for (k = 0; k < pf__term_restore_list.count; k++)
        SetConsoleMode(
            pf__term_restore_list.handle[k], pf__term_restore_list.mode[k]
        );
    pf__term_restore_list.count = 0;
    if (pf__term_restore_list.cp_saved) {
        SetConsoleOutputCP(pf__term_restore_list.out_cp);
        SetConsoleCP(pf__term_restore_list.in_cp);
        pf__term_restore_list.cp_saved = false;
    }
    pf__spin_unlock(&pf__term_restore_list.lock);
}

/* Call with the lock held. */
static void pf__term_register(void) {
    if (!pf__term_restore_list.registered) {
        pf__term_restore_list.registered = true;
        atexit(pf__term_at_exit);
    }
}

PF_API int pf_term_enable_vt(int fd) {
    HANDLE h;
    DWORD mode;
    int rc = pf__console_mode(fd, &h, &mode), k;

    if (rc)
        return rc;
    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        return PF_OK;
    if (!SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return PF_ENOTSUP; /* before Windows 10 */
    pf__spin_lock(&pf__term_restore_list.lock);
    for (k = 0; k < pf__term_restore_list.count; k++)
        if (pf__term_restore_list.handle[k] == h)
            break;
    if (k == pf__term_restore_list.count && k < 8) {
        pf__term_restore_list.handle[k] = h;
        pf__term_restore_list.mode[k] = mode;
        pf__term_restore_list.count++;
        pf__term_register();
    }
    pf__spin_unlock(&pf__term_restore_list.lock);
    return PF_OK;
}

PF_API int pf_term_set_utf8(void) {
    UINT out = GetConsoleOutputCP(), in = GetConsoleCP();

    if (out == 0 && in == 0)
        return PF_OK; /* no console */
    if (out == CP_UTF8 && in == CP_UTF8)
        return PF_OK;
    pf__spin_lock(&pf__term_restore_list.lock);
    if (!pf__term_restore_list.cp_saved) {
        pf__term_restore_list.cp_saved = true;
        pf__term_restore_list.out_cp = out;
        pf__term_restore_list.in_cp = in;
        pf__term_register();
    }
    pf__spin_unlock(&pf__term_restore_list.lock);
    if (!SetConsoleOutputCP(CP_UTF8) || !SetConsoleCP(CP_UTF8))
        return pf_error_last_win32();
    return PF_OK;
}

#else /* POSIX */

PF_API int pf_isatty(int fd) { return isatty(fd); }

PF_API bool pf_term_is_terminal(int fd) { return isatty(fd) != 0; }

/* tcgetattr: 0, or PF_ENOTTY / PF_EBADF. */
static int pf__tcget(int fd, struct termios *t) {
    if (tcgetattr(fd, t) != 0)
        return pf_error_from_errno(errno == EINVAL ? ENOTTY : errno);
    return PF_OK;
}

static int pf__tcset(int fd, const struct termios *t) {
    while (tcsetattr(fd, TCSANOW, t) != 0)
        if (errno != EINTR)
            return pf_error_from_errno(errno);
    return PF_OK;
}

PF_API int pf_term_size(int fd, int *cols, int *rows) {
    struct winsize ws;

    if (!isatty(fd))
        return pf_error_from_errno(errno);
    if (ioctl(fd, TIOCGWINSZ, &ws) != 0)
        return pf_error_from_errno(errno);
    if (cols)
        *cols = ws.ws_col;
    if (rows)
        *rows = ws.ws_row;
    return PF_OK;
}

PF_STATIC_ASSERT(
    sizeof(struct termios) + 8 <= sizeof(((pf_term_state_t *)0)->pf__u.data),
    "pf_term_state_t too small for struct termios"
);

    #define PF__TERM_MAGIC 0x50465450u /* "PFTP" */

PF_API int pf_term_raw(int fd, unsigned flags, pf_term_state_t *saved) {
    struct termios t, raw;
    int rc = pf__tcget(fd, &t);
    uint32_t magic = PF__TERM_MAGIC;

    if (rc)
        return rc;
    if (saved) {
        memset(saved, 0, sizeof(*saved));
        memcpy(saved->pf__u.data, &magic, sizeof(magic));
        memcpy(saved->pf__u.data + 8, &t, sizeof(t));
    }
    raw = t;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | IEXTEN);
    if (!(flags & PF_TERM_KEEP_SIGNALS))
        raw.c_lflag &= ~(tcflag_t)ISIG;
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    return pf__tcset(fd, &raw);
}

PF_API int pf_term_restore(int fd, const pf_term_state_t *saved) {
    struct termios t;
    uint32_t magic;

    if (!saved)
        return PF_EINVAL;
    memcpy(&magic, saved->pf__u.data, sizeof(magic));
    if (magic != PF__TERM_MAGIC)
        return PF_EINVAL;
    memcpy(&t, saved->pf__u.data + 8, sizeof(t));
    return pf__tcset(fd, &t);
}

PF_API int pf_term_echo(int fd, bool on) {
    struct termios t;
    int rc = pf__tcget(fd, &t);

    if (rc)
        return rc;
    if (on)
        t.c_lflag |= ECHO;
    else
        t.c_lflag &= ~(tcflag_t)ECHO;
    return pf__tcset(fd, &t);
}

PF_API int pf_term_enable_vt(int fd) {
    return isatty(fd) ? PF_OK : pf_error_from_errno(errno);
}

PF_API int pf_term_set_utf8(void) { return PF_OK; }

#endif

PF_API int pf_term_colors(int fd) {
    char v[64], term[128];
    bool forced = false;

    if (pf__env("NO_COLOR", v, sizeof(v)) && v[0])
        return 0;
    if (pf__env("FORCE_COLOR", v, sizeof(v))) {
        if (strcmp(v, "0") == 0 || strcmp(v, "false") == 0)
            return 0;
        if (strcmp(v, "2") == 0)
            return 256;
        if (strcmp(v, "3") == 0)
            return 16777216;
        return 16;
    }
    if (pf__env("CLICOLOR_FORCE", v, sizeof(v)) && strcmp(v, "0") != 0)
        forced = true;
    else if (pf__env("CLICOLOR", v, sizeof(v)) && strcmp(v, "0") == 0)
        return 0;

    if (!forced && !pf_term_is_terminal(fd))
        return 0;
#if PF_OS_WINDOWS
    /* The console itself (not mintty, which sets TERM). */
    if (!pf__env("TERM", term, sizeof(term))) {
        if (pf_term_enable_vt(fd) == PF_OK)
            return 16777216;
        return forced ? 16 : 0;
    }
#endif
    if (!pf__env("TERM", term, sizeof(term)))
        term[0] = '\0';
    if (strcmp(term, "dumb") == 0)
        return forced ? 16 : 0;
    if (pf__env("COLORTERM", v, sizeof(v))
        && (strcmp(v, "truecolor") == 0 || strcmp(v, "24bit") == 0))
        return 16777216;
    {
        size_t n = strlen(term);
        if (n >= 7 && strcmp(term + n - 7, "-direct") == 0)
            return 16777216;
    }
    if (strstr(term, "256color"))
        return 256;
    return 16;
}
