/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/term.h>, on a pseudo-terminal where there is one. */

#include "../src/internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/term.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#if PF_OS_POSIX
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

#include "check.h"

/* A file that is not a terminal. */
static int open_file(char *path, size_t size) {
    char tmp[PF_PATH_MAX];
    PF_ASSERT_ALWAYS(pf_known_dir(PF_DIR_TEMP, tmp, sizeof(tmp)) > 0);
    PF_ASSERT_ALWAYS(pf_path_join(path, size, tmp, "pf-term-XXXXXX") > 0);
    return pf_mkstemp(path);
}

static int test_not_terminal(int seed, int i) {
    char path[PF_PATH_MAX];
    pf_term_state_t st;
    int fd = open_file(path, sizeof(path)), cols = -1;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_GE(fd, 0);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_isatty(fd), 0);
    PF_ASSERT_ALWAYS_EQ(errno, ENOTTY);
    PF_ASSERT_ALWAYS(!pf_term_is_terminal(fd));
    PF_ASSERT_ALWAYS_EQ(pf_term_size(fd, &cols, NULL), PF_ENOTTY);
    PF_ASSERT_ALWAYS_EQ(cols, -1);
    PF_ASSERT_ALWAYS_EQ(pf_term_raw(fd, 0, &st), PF_ENOTTY);
    PF_ASSERT_ALWAYS_EQ(pf_term_echo(fd, false), PF_ENOTTY);
    PF_ASSERT_ALWAYS_EQ(pf_term_enable_vt(fd), PF_ENOTTY);
    PF_ASSERT_ALWAYS_EQ(pf_term_set_utf8(), PF_OK);

    memset(&st, 0, sizeof(st));
    PF_ASSERT_ALWAYS_EQ(pf_term_restore(fd, &st), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_term_restore(fd, NULL), PF_EINVAL);

    PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
    PF_ASSERT_ALWAYS_EQ(pf_unlink(path), 0);
    errno = 0;
    PF_ASSERT_ALWAYS_EQ(pf_isatty(fd), 0);
    PF_ASSERT_ALWAYS_EQ(errno, EBADF);
    return 0;
}

#if PF_OS_POSIX && !PF_OS_EMSCRIPTEN && !PF_OS_WASI
    #define HAVE_PTY 1
/* A pseudo-terminal: the controlling side in `*master`, the terminal
   side returned. -1 if there is none. */
static int open_pty(int *master) {
    char *name;
    int m = posix_openpt(O_RDWR | O_NOCTTY), s;
    if (m < 0)
        return -1;
    if (grantpt(m) != 0 || unlockpt(m) != 0 || (name = ptsname(m)) == NULL) {
        close(m);
        return -1;
    }
    s = open(name, O_RDWR | O_NOCTTY);
    if (s < 0) {
        close(m);
        return -1;
    }
    *master = m;
    return s;
}
#else
    #define HAVE_PTY 0
#endif

static int test_pty(int seed, int i) {
#if HAVE_PTY
    struct termios before, now;
    struct winsize ws;
    pf_term_state_t st;
    int m, s = open_pty(&m), cols = 0, rows = 0;
    (void)seed, (void)i;

    if (s < 0)
        return SKIP("no pseudo-terminals");
    PF_ASSERT_ALWAYS_EQ(pf_isatty(s), 1);
    PF_ASSERT_ALWAYS(pf_term_is_terminal(s));
    PF_ASSERT_ALWAYS_EQ(pf_term_enable_vt(s), PF_OK);

    memset(&ws, 0, sizeof(ws));
    ws.ws_col = 123;
    ws.ws_row = 45;
    PF_ASSERT_ALWAYS_EQ(ioctl(m, TIOCSWINSZ, &ws), 0);
    PF_ASSERT_ALWAYS_EQ(pf_term_size(s, &cols, &rows), PF_OK);
    PF_ASSERT_ALWAYS_EQ(cols, 123);
    PF_ASSERT_ALWAYS_EQ(rows, 45);
    PF_ASSERT_ALWAYS_EQ(pf_term_size(s, NULL, NULL), PF_OK);

    PF_ASSERT_ALWAYS_EQ(tcgetattr(s, &before), 0);
    PF_ASSERT_ALWAYS(before.c_lflag & ICANON);

    /* raw: no line editing, echo or signals; output unchanged */
    PF_ASSERT_ALWAYS_EQ(pf_term_raw(s, 0, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(tcgetattr(s, &now), 0);
    PF_ASSERT_ALWAYS_EQ(now.c_lflag & (ICANON | ECHO | ISIG), 0);
    PF_ASSERT_ALWAYS_EQ(now.c_oflag, before.c_oflag);
    PF_ASSERT_ALWAYS_EQ(now.c_cc[VMIN], 1);
    {
        /* a key arrives at once, without a newline */
        char c = 0;
        PF_ASSERT_ALWAYS_EQ(write(m, "x", 1), 1);
        PF_ASSERT_ALWAYS_EQ(read(s, &c, 1), 1);
        PF_ASSERT_ALWAYS_EQ(c, 'x');
    }
    PF_ASSERT_ALWAYS_EQ(pf_term_restore(s, &st), PF_OK);
    PF_ASSERT_ALWAYS_EQ(tcgetattr(s, &now), 0);
    PF_ASSERT_ALWAYS_EQ(now.c_lflag, before.c_lflag);
    PF_ASSERT_ALWAYS_EQ(now.c_iflag, before.c_iflag);

    PF_ASSERT_ALWAYS_EQ(pf_term_raw(s, PF_TERM_KEEP_SIGNALS, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(tcgetattr(s, &now), 0);
    PF_ASSERT_ALWAYS(now.c_lflag & ISIG);
    PF_ASSERT_ALWAYS_EQ(pf_term_restore(s, &st), PF_OK);

    /* echo off keeps line editing */
    PF_ASSERT_ALWAYS_EQ(pf_term_echo(s, false), PF_OK);
    PF_ASSERT_ALWAYS_EQ(tcgetattr(s, &now), 0);
    PF_ASSERT_ALWAYS_EQ(now.c_lflag & ECHO, 0);
    PF_ASSERT_ALWAYS(now.c_lflag & ICANON);
    PF_ASSERT_ALWAYS_EQ(pf_term_echo(s, true), PF_OK);
    PF_ASSERT_ALWAYS_EQ(tcgetattr(s, &now), 0);
    PF_ASSERT_ALWAYS(now.c_lflag & ECHO);

    close(s);
    close(m);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

/* The colour environment, saved and cleared, then restored. */
static const char *const color_vars[] = {
    "NO_COLOR", "FORCE_COLOR", "CLICOLOR_FORCE",
    "CLICOLOR", "TERM",        "COLORTERM",
};
#define NVARS (sizeof(color_vars) / sizeof(color_vars[0]))

static void env_set(const char *name, const char *value) {
    if (value)
        PF_ASSERT_ALWAYS_EQ(pf_setenv(name, value, 1), 0);
    else
        PF_ASSERT_ALWAYS_EQ(pf_unsetenv(name), 0);
}

static int test_colors(int seed, int i) {
    char saved[NVARS][256];
    bool had[NVARS];
    char path[PF_PATH_MAX];
    int fd = open_file(path, sizeof(path)), tty = -1, m = -1;
    size_t k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_GE(fd, 0);
    for (k = 0; k < NVARS; k++) {
        had[k] = pf_getenv(color_vars[k], saved[k], sizeof(saved[k])) >= 0;
        env_set(color_vars[k], NULL);
    }
#if HAVE_PTY
    tty = open_pty(&m);
#endif

    /* a file */
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 0);
    env_set("FORCE_COLOR", "1");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 16);
    env_set("FORCE_COLOR", "2");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 256);
    env_set("FORCE_COLOR", "3");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 16777216);
    env_set("FORCE_COLOR", "0");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 0);
    env_set("NO_COLOR", "1");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 0);
#if !PF_OS_WINDOWS /* an empty value removes a variable on Windows */
    env_set("NO_COLOR", "");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 0);
    env_set("NO_COLOR", NULL);
    env_set("FORCE_COLOR", "");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 16);
    env_set("NO_COLOR", "");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 16);
#endif
    env_set("NO_COLOR", NULL);
    env_set("FORCE_COLOR", NULL);
    env_set("CLICOLOR_FORCE", "1");
    env_set("TERM", "xterm-256color");
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 256);
    env_set("CLICOLOR_FORCE", NULL);
    PF_ASSERT_ALWAYS_EQ(pf_term_colors(fd), 0);

    if (tty >= 0) {
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 256);
        env_set("TERM", "xterm");
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 16);
        env_set("COLORTERM", "truecolor");
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 16777216);
        env_set("COLORTERM", NULL);
        env_set("TERM", "xterm-direct");
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 16777216);
        env_set("TERM", "dumb");
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 0);
        env_set("TERM", "xterm");
        env_set("CLICOLOR", "0");
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 0);
        env_set("CLICOLOR", "1");
        PF_ASSERT_ALWAYS_EQ(pf_term_colors(tty), 16);
#if HAVE_PTY
        close(tty);
        close(m);
#endif
    }

    for (k = 0; k < NVARS; k++)
        env_set(color_vars[k], had[k] ? saved[k] : NULL);
    PF_ASSERT_ALWAYS_EQ(pf_close(fd), 0);
    PF_ASSERT_ALWAYS_EQ(pf_unlink(path), 0);
    return 0;
}

pf_test_t suite_term[] = {
    { test_not_terminal, "not_terminal" },
    { test_pty, "pty" },
    { test_colors, "colors" },
    { 0 },
};
