/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/cli.h> on temporary files, and on a pseudo-terminal where
   there is one. */

#include "../src/internal.h"

#include <cpolyfill/cli.h>
#include <cpolyfill/errno.h>
#include <cpolyfill/stdlib.h>
#include <cpolyfill/time.h>
#if PF_TEST_ARGPARSE
    #include <cpolyfill/argparse.h>
#endif

#include <stdio.h>
#include <string.h>
#if PF_OS_POSIX
    #include <fcntl.h>
    #include <poll.h>
    #include <stdlib.h>
    #include <termios.h>
    #include <unistd.h>
#endif
#if PF_OS_WINDOWS
    #include <io.h>
#endif

#include "check.h"

/* ---- Harness ----------------------------------------------------------- */

/* The output must not depend on the environment running the tests. */
static void clean_env(void) {
    static const char *const unset[] = {
        "NO_COLOR",  "FORCE_COLOR",   "CLICOLOR", "CLICOLOR_FORCE",
        "COLORTERM", "NO_HYPERLINKS", NULL,
    };
    int k;
    for (k = 0; unset[k]; k++)
        pf_unsetenv(unset[k]);
    pf_setenv("COLUMNS", "80", 1);
    pf_setenv("TERM", "xterm", 1);
}

struct rig {
    pf_cli_t c;
    FILE *in, *out, *err;
};

/* A CLI on temporary files, reading `input`. */
static void rig_open(
    struct rig *r,
    const char *input,
    pf_color_mode_t color,
    pf_interactive_mode_t interactive
) {
    pf_cli_opts_t o;

    clean_env();
    r->in = tmpfile();
    r->out = tmpfile();
    r->err = tmpfile();
    PF_ASSERT_ALWAYS(r->in && r->out && r->err);
    if (input) {
        fputs(input, r->in);
        rewind(r->in);
    }
    memset(&o, 0, sizeof(o));
    o.in = r->in;
    o.out = r->out;
    o.err = r->err;
    o.color_mode = color;
    o.interactive_mode = interactive;
    o.definition_indent = 2;
    PF_ASSERT_ALWAYS_EQ(pf_cli_init(&r->c, &o), PF_OK);
}

static void rig_close(struct rig *r) {
    pf_cli_free(&r->c);
    fclose(r->in);
    fclose(r->out);
    fclose(r->err);
}

/* What was written to `f` since the last call. */
static const char *take(FILE *f) {
    static char buf[4096];
    size_t n;
    fflush(f);
    rewind(f);
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    rewind(f);
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_EQ(_chsize_s(_fileno(f), 0), 0);
#else
    PF_ASSERT_ALWAYS_EQ(ftruncate(fileno(f), 0), 0);
#endif
    return buf;
}

#define HAS(text, part)                                                    \
    PF_ASSERT_ALWAYS_MSG(strstr((text), (part)) != NULL, "in: %s", (text))

/* ---- Setup ------------------------------------------------------------- */

static int test_init(int seed, int i) {
    pf_cli_t c;
    pf_cli_opts_t o;
    (void)seed, (void)i;

    clean_env();
    PF_ASSERT_ALWAYS_EQ(pf_cli_init(NULL, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_init(&c, NULL), PF_OK);
    PF_ASSERT_ALWAYS(c.in.file == stdin && c.out.file == stdout);
    PF_ASSERT_ALWAYS(c.err.file == stderr);
    PF_ASSERT_ALWAYS_EQ(c.definition_indent, 10);
    PF_ASSERT_ALWAYS(!c.silent);
    pf_cli_free(&c);
    pf_cli_free(NULL);

    /* Streams left out stay out. */
    memset(&o, 0, sizeof(o));
    o.null_streams = true;
    PF_ASSERT_ALWAYS_EQ(pf_cli_init(&c, &o), PF_OK);
    PF_ASSERT_ALWAYS(!c.in.file && !c.out.file && !c.err.file);
    PF_ASSERT_ALWAYS(c.silent && !c.interactive);
    pf_cli_printf(&c, "nothing");
    pf_cli_errorf(&c, "nothing");
    pf_cli_help_definition(&c, "x", "y");
    pf_cli_help_section(&c, "x");
    PF_ASSERT_ALWAYS(pf_cli_confirm(&c, "?", true));
    PF_ASSERT_ALWAYS_EQ(
        pf_cli_select(&c, "?", (const char *[]) { "a" }, 1, 0), 0
    );
    pf_cli_free(&c);
    return 0;
}

static int test_files(int seed, int i) {
    struct rig r;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS(!r.c.out.is_terminal && !r.c.out.cursor);
    PF_ASSERT_ALWAYS_EQ(r.c.out.colors, 0);
    PF_ASSERT_ALWAYS(!r.c.out.hyperlinks);
    PF_ASSERT_ALWAYS_EQ(r.c.out.columns, 80);
    PF_ASSERT_ALWAYS(!r.c.interactive);

    /* ALWAYS colours even a file: 256, or 24-bit with COLORTERM. */
    pf_cli_set_color_mode(&r.c, PF_COLOR_ALWAYS);
    PF_ASSERT_ALWAYS_EQ(r.c.out.colors, 256);
    PF_ASSERT_ALWAYS_EQ(r.c.err.colors, 256);
    pf_setenv("COLORTERM", "truecolor", 1);
    pf_cli_set_color_mode(&r.c, PF_COLOR_ALWAYS);
    PF_ASSERT_ALWAYS_EQ(r.c.out.colors, 16777216);
    pf_cli_set_color_mode(&r.c, PF_COLOR_NEVER);
    PF_ASSERT_ALWAYS_EQ(r.c.out.colors, 0);

    pf_cli_set_interactive_mode(&r.c, PF_INTERACTIVE_ALWAYS);
    PF_ASSERT_ALWAYS(r.c.interactive);
    pf_cli_set_interactive_mode(&r.c, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS(!r.c.interactive);
    pf_cli_set_color_mode(NULL, PF_COLOR_NEVER);
    pf_cli_set_interactive_mode(NULL, PF_INTERACTIVE_NEVER);
    rig_close(&r);

    /* COLUMNS within 20..1000 only. */
    clean_env();
    pf_setenv("COLUMNS", "132", 1);
    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    rig_close(&r); /* rig_open resets COLUMNS to 80 */
    {
        pf_cli_t c;
        pf_cli_opts_t o;
        FILE *f = tmpfile();
        memset(&o, 0, sizeof(o));
        o.out = f;
        pf_setenv("COLUMNS", "132", 1);
        pf_cli_init(&c, &o);
        PF_ASSERT_ALWAYS_EQ(c.out.columns, 132);
        pf_setenv("COLUMNS", "5", 1);
        pf_cli_init(&c, &o);
        PF_ASSERT_ALWAYS_EQ(c.out.columns, 80);
        fclose(f);
    }
    return 0;
}

/* ---- Output ------------------------------------------------------------ */

static int test_printf(int seed, int i) {
    struct rig r;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    pf_cli_printf(&r.c, "a %d\n", 1);
    pf_cli_cprintf(&r.c, PF_FG_RED, "red\n"); /* no colours on a file */
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "a 1\nred\n");
    pf_cli_errorf(&r.c, "e %s\n", "x");
    pf_cli_ecprintf(&r.c, PF_CLI_BOLD, "b\n");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.err), "e x\nb\n");

    pf_cli_verbosef(&r.c, 1, "hidden\n");
    r.c.verbosity = 2;
    pf_cli_verbosef(&r.c, 1, "one\n");
    pf_cli_verbosef(&r.c, 2, "two\n");
    pf_cli_verbosef(&r.c, 3, "three\n");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "one\ntwo\n");

    r.c.silent = true;
    pf_cli_printf(&r.c, "x");
    pf_cli_errorf(&r.c, "x");
    pf_cli_wrap(&r.c, 0, 0, 0, "x");
    pf_cli_link(&r.c, "u", "t");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.err), "");

    /* No cursor on a file: nothing at all. */
    r.c.silent = false;
    pf_cli_clear_line(&r.c, PF_CLEAR_ALL);
    pf_cli_clear(&r.c, PF_CLEAR_ALL);
    pf_cli_move(&r.c, 2, -3);
    pf_cli_position(&r.c, 1, 1);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");

    pf_cli_printf(NULL, "x");
    pf_cli_cprintf(NULL, 0, "x");
    pf_cli_verbosef(NULL, 0, "x");
    rig_close(&r);
    return 0;
}

static int test_styles(int seed, int i) {
    struct rig r;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_ALWAYS, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS_EQ(r.c.out.colors, 256);
    pf_cli_cprintf(&r.c, PF_FG_RED | PF_CLI_BOLD, "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[1;31mx\033[0m");
    pf_cli_cprintf(
        &r.c, PF_FG_BRIGHT_CYAN | PF_BG_BLUE | PF_CLI_UNDERLINE, "x"
    );
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[4;96;44mx\033[0m");
    pf_cli_cprintf(&r.c, PF_CLI_DIM | PF_CLI_ITALIC | PF_BG_BRIGHT_BLACK, "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[2;3;100mx\033[0m");
    pf_cli_cprintf(&r.c, PF_FG_256(208) | PF_BG_256(17), "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[38;5;208;48;5;17mx\033[0m");
    /* RGB turned into the palette. */
    pf_cli_cprintf(&r.c, PF_FG_RGB(255, 0, 0) | PF_BG_RGB(0, 0, 0), "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[38;5;196;48;5;16mx\033[0m");
    pf_cli_color(&r.c, PF_FG_GREEN);
    pf_cli_color(&r.c, PF_CLI_RESET);
    pf_cli_color(&r.c, 0);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[32m\033[0m");

    /* 24-bit as it is. */
    r.c.out.colors = 16777216;
    pf_cli_cprintf(&r.c, PF_FG_RGB(1, 2, 3) | PF_BG_RGB(250, 128, 0), "x");
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out), "\033[38;2;1;2;3;48;2;250;128;0mx\033[0m"
    );

    /* 16 colours: the nearest basic or bright one. */
    r.c.out.colors = 16;
    pf_cli_cprintf(&r.c, PF_FG_RGB(255, 0, 0), "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[91mx\033[0m");
    pf_cli_cprintf(&r.c, PF_FG_RGB(128, 128, 128) | PF_BG_RGB(0, 0, 100), "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[37;100mx\033[0m");
    pf_cli_cprintf(&r.c, PF_FG_256(4) | PF_BG_256(196), "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[34;101mx\033[0m");
    pf_cli_cprintf(&r.c, PF_FG_256(232) | PF_BG_256(255), "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[30;107mx\033[0m");

    /* None: not even attributes. */
    r.c.out.colors = 0;
    pf_cli_cprintf(&r.c, PF_FG_RED | PF_CLI_BOLD, "x");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "x");
    rig_close(&r);
    return 0;
}

static int test_links(int seed, int i) {
    struct rig r;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_ALWAYS, PF_INTERACTIVE_AUTO);
    pf_cli_link(&r.c, "https://x.org", "site");
    pf_cli_path_link(&r.c, "/tmp/f.c", 12);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "site/tmp/f.c");
    r.c.out.hyperlinks = true;
    pf_cli_link(&r.c, "https://x.org", "site");
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out), "\033]8;;https://x.org\033\\site\033]8;;\033\\"
    );
    pf_cli_path_link(&r.c, "/tmp/f.c", 12);
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out), "\033]8;;file:///tmp/f.c#L12\033\\/tmp/f.c\033]8;;\033\\"
    );
    pf_cli_path_link(&r.c, "/a", 0);
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out), "\033]8;;file:///a\033\\/a\033]8;;\033\\"
    );
    pf_cli_link(&r.c, NULL, "x");
    pf_cli_path_link(&r.c, NULL, 0);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");
    rig_close(&r);
    return 0;
}

/* ---- Text -------------------------------------------------------------- */

static int test_text_width(int seed, int i) {
    (void)seed, (void)i;
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width(NULL), 0);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width(""), 0);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("abc"), 3);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("a\nb"), 2);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("\033[1;31mab\033[0m"), 2);
    PF_ASSERT_ALWAYS_EQ(
        pf_cli_text_width("\033]8;;https://x\033\\t\033]8;;\033\\"), 1
    );
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("\033]0;title\a."), 1);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("h\xC3\xA9llo"), 5); /* é */
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("e\xCC\x81"), 1);    /* e + ́ */
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("\xE6\x97\xA5\xE6\x9C\xAC"), 4);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("\xEA\xB0\x80"), 2);     /* 가 */
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("\xF0\x9F\x98\x80"), 2); /* 😀 */
    PF_ASSERT_ALWAYS_EQ(
        pf_cli_text_width(
            "a\xE2\x80\x8B"
            "b"
        ),
        2
    );                                                         /* ZWSP */
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width("\xE2\xA0\x8B"), 1); /* braille */
    PF_ASSERT_ALWAYS_EQ(
        pf_cli_text_width(
            "\xFF"
            "a"
        ),
        2
    ); /* invalid */
    return 0;
}

static int test_wrap(int seed, int i) {
    struct rig r;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    pf_cli_wrap(&r.c, 2, 4, 20, "the quick brown fox jumps over the lazy dog");
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out),
        "  the quick brown\n"
        "    fox jumps over\n"
        "    the lazy dog"
    );
    /* Explicit newlines, runs of spaces, a word longer than the line. */
    pf_cli_wrap(&r.c, 0, 2, 12, "one  two\nthree abcdefghijklmnop end");
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out), "one two\n  three\n  abcdefghijklmnop\n  end"
    );
    /* Escapes take no room; wide characters two columns each. */
    pf_cli_wrap(&r.c, 0, 0, 10, "\033[1mabcd\033[0m efgh ij");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[1mabcd\033[0m efgh\nij");
    pf_cli_wrap(
        &r.c,
        0,
        0,
        10,
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xE6\x97\xA5\xE6\x9C\xAC "
        "\xE6\x97\xA5"
    );
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out),
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\n\xE6\x97\xA5\xE6\x9C\xAC "
        "\xE6\x97\xA5"
    );
    /* 0: the terminal's width (80 here). */
    pf_cli_wrap(&r.c, 0, 0, 0, "short");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "short");
    pf_cli_wrap(NULL, 0, 0, 0, "x");
    pf_cli_wrap(&r.c, 0, 0, 0, NULL);
    pf_cli_fill(NULL, 3, 'x');
    pf_cli_fill(r.out, 3, 'x');
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "xxx");
    rig_close(&r);
    return 0;
}

static int test_help(int seed, int i) {
    static const pf_cli_definition_t defs[] = {
        { "--one", "First." },
        { "--two", "Second." },
        { NULL, NULL },
    };
    struct rig r;
    char expect[256];
    const char *out;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    pf_cli_help_definition(&r.c, "-a, --all", "Everything.");
    snprintf(expect, sizeof(expect), "  %-30sEverything.\n", "-a, --all");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), expect);

    /* Aligned by columns, not bytes. */
    pf_cli_help_definition(&r.c, "--na\xC3\xAFve", "D.");
    snprintf(expect, sizeof(expect), "  --na\xC3\xAFve%23sD.\n", "");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), expect);

    /* Long descriptions wrap under the description column. */
    pf_cli_help_definition(
        &r.c,
        "--x",
        "aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm"
    );
    out = take(r.out);
    HAS(out, "\n                                ");
    PF_ASSERT_ALWAYS_EQ(strchr(out, '\n') - out, 76);

    /* A long name: the description below it. */
    pf_cli_help_definition(&r.c, "--a-very-long-option-name=<VALUE>", "Long.");
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out), "  --a-very-long-option-name=<VALUE>\n      Long.\n"
    );
    pf_cli_help_definition(&r.c, NULL, NULL);
    snprintf(expect, sizeof(expect), "  %30s\n", "");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), expect);

    pf_cli_help_definitions(&r.c, defs);
    out = take(r.out);
    HAS(out, "--one");
    HAS(out, "Second.\n");
    pf_cli_help_definitions(&r.c, NULL);

    pf_cli_help_section(&r.c, "Options:");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\nOptions:\n");
    pf_cli_set_color_mode(&r.c, PF_COLOR_ALWAYS);
    pf_cli_help_section(&r.c, "Options:");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\n\033[1mOptions:\033[0m\n");

    /* Narrow: no room beside the name. */
    r.c.out.columns = 30;
    pf_cli_help_definition(&r.c, "--x", "Text.");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "  --x\n      Text.\n");
    rig_close(&r);
    return 0;
}

/* ---- Prompts ----------------------------------------------------------- */

static int test_readline(int seed, int i) {
    struct rig r;
    char buf[16];
    (void)seed, (void)i;

    rig_open(
        &r,
        "one\ntwo\r\n0123456789abcdefghij\naaaaaaaaaaaaaa\xC3\xA9xy\nlast",
        PF_COLOR_AUTO,
        PF_INTERACTIVE_AUTO
    );
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "one");
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "two");
    /* Too long: cut, and the rest of the line is gone. */
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_ERANGE);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "0123456789abcde");
    /* Not in the middle of a character. */
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_ERANGE);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "aaaaaaaaaaaaaa");
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "last");
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_ENODATA);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "");
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(NULL, buf, sizeof(buf)), PF_EINVAL);
    rig_close(&r);
    return 0;
}

/* AUTO on files: no prompt is shown, nothing is read. */
static int test_prompts_not_interactive(int seed, int i) {
    static const char *const choices[] = { "red", "green" };
    struct rig r;
    char buf[16];
    (void)seed, (void)i;

    rig_open(&r, "yes\n", PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS_EQ(pf_cli_ask(&r.c, "Name", buf, sizeof(buf)), PF_EAGAIN);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "");
    PF_ASSERT_ALWAYS(!pf_cli_confirm(&r.c, "Sure?", false));
    PF_ASSERT_ALWAYS(pf_cli_confirm(&r.c, "Sure?", true));
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "Pick", choices, 2, 1), 1);
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "Pick", choices, 2, 9), 0);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");
    /* The input is still there. */
    PF_ASSERT_ALWAYS_EQ(pf_cli_readline(&r.c, buf, sizeof(buf)), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "yes");
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "Pick", NULL, 2, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "Pick", choices, 0, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_ask(NULL, "x", buf, sizeof(buf)), PF_EINVAL);
    rig_close(&r);
    return 0;
}

static int test_prompts(int seed, int i) {
    static const char *const choices[] = { "red", "green", "blue" };
    struct rig r;
    char buf[16];
    (void)seed, (void)i;

    rig_open(
        &r, "Ann\ny\n\nno\n2\nblue\n\n7\n", PF_COLOR_AUTO, PF_INTERACTIVE_ALWAYS
    );
    PF_ASSERT_ALWAYS(r.c.interactive);
    PF_ASSERT_ALWAYS_EQ(pf_cli_ask(&r.c, "Name", buf, sizeof(buf)), PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "Ann");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "Name: ");

    PF_ASSERT_ALWAYS(pf_cli_confirm(&r.c, "Sure?", false));
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "Sure? [y/N]: ");
    PF_ASSERT_ALWAYS(pf_cli_confirm(&r.c, "Sure?", true)); /* empty */
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "Sure? [Y/n]: ");
    PF_ASSERT_ALWAYS(!pf_cli_confirm(&r.c, "Sure?", true));
    take(r.out);

    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "Color", choices, 3, 0), 1);
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.out),
        "Color:\n  1) red (default)\n  2) green\n  3) blue\nChoice [1]: "
    );
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, NULL, choices, 3, 0), 2);
    HAS(take(r.out), "Select:\n");
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "C", choices, 3, 2), 2);
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "C", choices, 3, 0), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_select(&r.c, "C", choices, 3, 0), PF_ENODATA);
    PF_ASSERT_ALWAYS(pf_cli_confirm(&r.c, "End?", true));
    PF_ASSERT_ALWAYS_EQ(pf_cli_ask(&r.c, "End", buf, sizeof(buf)), PF_ENODATA);
    rig_close(&r);
    return 0;
}

/* ---- Spinner and progress on files ------------------------------------- */

static int test_spinner_redirected(int seed, int i) {
    static const char *const empty[] = { NULL };
    pf_cli_spinner_opts_t o;
    pf_cli_spinner_t s;
    struct rig r;
    int k;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &r.c, NULL), PF_OK);
    pf_cli_spinner_set_title(&s, "working %d", 1);
    PF_ASSERT_ALWAYS_STR_EQ(s.opts.title, "working 1");
    for (k = 0; k < 5; k++)
        pf_cli_spinner_tick(&s);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), ""); /* not animated */
    pf_cli_printf(&r.c, "text\n");            /* no clearing either */
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "text\n");
    pf_cli_spinner_finish(&s, true, "done %s", "now");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "[ok] done now\n");
    PF_ASSERT_ALWAYS(!s.active);
    pf_cli_spinner_finish(&s, true, "again"); /* no longer active */
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");

    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &r.c, NULL), PF_OK);
    pf_cli_spinner_finish(&s, false, "bad");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "[fail] bad\n");
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &r.c, NULL), PF_OK);
    pf_cli_spinner_stop(&s, NULL);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &r.c, NULL), PF_OK);
    pf_cli_spinner_stop(&s, "plain");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "plain\n");

    /* A coloured marker. */
    pf_cli_set_color_mode(&r.c, PF_COLOR_ALWAYS);
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &r.c, NULL), PF_OK);
    pf_cli_spinner_finish(&s, true, "ok");
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "\033[32m[ok]\033[0m ok\n");

    memset(&o, 0, sizeof(o));
    o.frames = empty;
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &r.c, &o), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, NULL, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(NULL, &r.c, NULL), PF_EINVAL);
    pf_cli_spinner_tick(NULL);
    pf_cli_spinner_stop(NULL, "x");
    rig_close(&r);
    return 0;
}

static int test_spinner_frames(int seed, int i) {
    const char *const *line = pf_cli_spinner_frames(PF_CLI_SPINNER_LINE);
    const char *const *dots = pf_cli_spinner_frames(PF_CLI_SPINNER_DOTS);
    int k;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_STR_EQ(line[0], "-");
    PF_ASSERT_ALWAYS_STR_EQ(line[3], "/");
    PF_ASSERT_ALWAYS_NULL(line[4]);
    for (k = 0; dots[k]; k++)
        PF_ASSERT_ALWAYS_EQ(pf_cli_text_width(dots[k]), 1);
    PF_ASSERT_ALWAYS_EQ(k, 10);
    PF_ASSERT_ALWAYS(pf_cli_spinner_frames(99) == line);
    return 0;
}

static int test_progress_redirected(int seed, int i) {
    pf_cli_progress_t p;
    struct rig r;
    (void)seed, (void)i;

    rig_open(&r, NULL, PF_COLOR_AUTO, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, &r.c, 100, NULL), PF_OK);
    PF_ASSERT_ALWAYS_EQ(p.opts.width, 20); /* a quarter of 80 */
    PF_ASSERT_ALWAYS(p.opts.fill == '#' && p.opts.empty == '-');
    PF_ASSERT_ALWAYS(p.opts.style == PF_FG_GREEN && p.opts.interval_ms == 80);
    pf_cli_progress_set_title(&p, "copy %d", 2);
    PF_ASSERT_ALWAYS_STR_EQ(p.opts.title, "copy 2");
    pf_cli_progress_update(&p, 150);
    PF_ASSERT_ALWAYS(p.value == 100);
    pf_cli_progress_advance(&p, -300);
    PF_ASSERT_ALWAYS(p.value == 0);
    pf_cli_progress_advance(&p, 30);
    PF_ASSERT_ALWAYS(p.value == 30);
    pf_cli_progress_render(&p);
    pf_cli_progress_finish(&p);
    PF_ASSERT_ALWAYS(!p.active && p.value == 100);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.out), "");

    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, &r.c, 0, NULL), PF_OK);
    PF_ASSERT_ALWAYS(p.maximum == 1.0);
    pf_cli_progress_stop(&p);
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, NULL, 1, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(NULL, &r.c, 1, NULL), PF_EINVAL);
    pf_cli_progress_update(NULL, 1);
    pf_cli_progress_finish(NULL);
    rig_close(&r);
    return 0;
}

/* ---- On a pseudo-terminal ---------------------------------------------- */

#if PF_OS_POSIX && !PF_OS_EMSCRIPTEN
    #define HAVE_PTY 1

struct pty {
    int master;
    FILE *term;
    pf_cli_t c;
};

/* A CLI on a pseudo-terminal 60 columns wide. -1 if there is none. */
static int pty_open(struct pty *t, pf_color_mode_t color) {
    struct termios tio;
    pf_cli_opts_t o;
    char *name;
    int s;

    clean_env();
    pf_setenv("COLUMNS", "60", 1);
    t->master = posix_openpt(O_RDWR | O_NOCTTY);
    if (t->master < 0)
        return -1;
    if (grantpt(t->master) != 0 || unlockpt(t->master) != 0
        || !(name = ptsname(t->master))
        || (s = open(name, O_RDWR | O_NOCTTY)) < 0) {
        close(t->master);
        return -1;
    }
    /* No translation of the output. */
    if (tcgetattr(s, &tio) == 0) {
        tio.c_oflag &= ~(tcflag_t)OPOST;
        tio.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
        tcsetattr(s, TCSANOW, &tio);
    }
    t->term = fdopen(s, "w+");
    PF_ASSERT_ALWAYS_NOT_NULL(t->term);
    setvbuf(t->term, NULL, _IOFBF, 8192);
    memset(&o, 0, sizeof(o));
    o.in = o.out = o.err = t->term;
    o.color_mode = color;
    o.definition_indent = 2;
    PF_ASSERT_ALWAYS_EQ(pf_cli_init(&t->c, &o), PF_OK);
    return 0;
}

static void pty_close(struct pty *t) {
    fclose(t->term);
    close(t->master);
}

/* What reached the terminal so far. */
static const char *pty_take(struct pty *t) {
    static char buf[4096];
    size_t used = 0;
    struct pollfd pfd;

    fflush(t->term);
    pfd.fd = t->master;
    pfd.events = POLLIN;
    while (used + 1 < sizeof(buf) && poll(&pfd, 1, 50) > 0) {
        ssize_t n = read(t->master, buf + used, sizeof(buf) - 1 - used);
        if (n <= 0)
            break;
        used += (size_t)n;
    }
    buf[used] = '\0';
    return buf;
}

    #define NEED_PTY(t, color)                                     \
        do {                                                       \
            if (pty_open(&(t), (color)) != 0)                      \
                return SKIP("pseudo-terminals are not available"); \
        } while (0)
#else
    #define HAVE_PTY 0
#endif

static int test_pty_detection(int seed, int i) {
#if HAVE_PTY
    struct pty t;
    (void)seed, (void)i;

    NEED_PTY(t, PF_COLOR_AUTO);
    PF_ASSERT_ALWAYS(t.c.out.is_terminal && t.c.in.is_terminal);
    PF_ASSERT_ALWAYS(t.c.out.cursor && t.c.out.hyperlinks);
    PF_ASSERT_ALWAYS_EQ(t.c.out.colors, 16); /* TERM=xterm */
    PF_ASSERT_ALWAYS_EQ(t.c.out.columns, 60);
    PF_ASSERT_ALWAYS(t.c.interactive);

    pf_setenv("NO_COLOR", "1", 1);
    pf_cli_set_color_mode(&t.c, PF_COLOR_AUTO);
    PF_ASSERT_ALWAYS_EQ(t.c.out.colors, 0);
    PF_ASSERT_ALWAYS(t.c.out.cursor && !t.c.out.hyperlinks);
    pf_setenv("NO_HYPERLINKS", "1", 1);
    pf_unsetenv("NO_COLOR");
    pf_cli_set_color_mode(&t.c, PF_COLOR_AUTO);
    PF_ASSERT_ALWAYS(t.c.out.colors == 16 && !t.c.out.hyperlinks);
    pf_unsetenv("NO_HYPERLINKS");

    pf_setenv("TERM", "xterm-256color", 1);
    pf_cli_set_color_mode(&t.c, PF_COLOR_AUTO);
    PF_ASSERT_ALWAYS_EQ(t.c.out.colors, 256);

    /* A dumb terminal: no escape sequences at all. */
    pf_setenv("TERM", "dumb", 1);
    {
        pf_cli_opts_t o;
        memset(&o, 0, sizeof(o));
        o.in = o.out = o.err = t.term;
        pf_cli_init(&t.c, &o);
    }
    PF_ASSERT_ALWAYS(t.c.out.is_terminal && !t.c.out.cursor);
    PF_ASSERT_ALWAYS_EQ(t.c.out.colors, 0);
    pty_close(&t);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

static int test_pty_cursor(int seed, int i) {
#if HAVE_PTY
    struct pty t;
    (void)seed, (void)i;

    NEED_PTY(t, PF_COLOR_NEVER);
    pf_cli_clear_line(&t.c, PF_CLEAR_ALL);
    pf_cli_clear(&t.c, PF_CLEAR_END);
    pf_cli_move(&t.c, 3, -2);
    pf_cli_move(&t.c, -1, 4);
    pf_cli_position(&t.c, 0, 9);
    pf_cli_position(&t.c, -1, 0);
    pf_cli_clear_line(&t.c, (pf_cli_clear_t)7);
    PF_ASSERT_ALWAYS_STR_EQ(
        pty_take(&t), "\r\033[2K\033[0J\033[3C\033[2A\033[1D\033[4B\033[10;1H"
    );
    pty_close(&t);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

static int test_pty_spinner(int seed, int i) {
#if HAVE_PTY
    pf_cli_spinner_opts_t o;
    pf_cli_spinner_t s;
    struct pty t;
    char title[200];
    const char *out;
    (void)seed, (void)i;

    NEED_PTY(t, PF_COLOR_NEVER);
    memset(&o, 0, sizeof(o));
    o.title = "work";
    o.interval_ms = 30;
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &t.c, &o), PF_OK);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s); /* too soon: not drawn */
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2K- work");
    pf_sleep_ns(40000000);
    pf_cli_spinner_tick(&s);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2K\\ work");

    /* Text printed meanwhile clears the line first. */
    pf_cli_printf(&t.c, "note\n");
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2Knote\n");

    pf_cli_spinner_finish(&s, true, "done");
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2K[ok] done\n");
    pf_cli_printf(&t.c, "after\n"); /* nothing live any more */
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "after\n");

    /* A long title is cut to the line, by columns. */
    memset(title, 'x', sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
    title[0] = '\xE6';
    title[1] = '\x97';
    title[2] = '\xA5'; /* a wide character first */
    o.title = title;
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &t.c, &o), PF_OK);
    pf_cli_spinner_tick(&s);
    out = pty_take(&t);
    PF_ASSERT_ALWAYS_EQ(pf_cli_text_width(out), 59);
    pf_cli_spinner_stop(&s, NULL);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2K");

    /* With interactive mode NEVER nothing is animated. */
    pf_cli_set_interactive_mode(&t.c, PF_INTERACTIVE_NEVER);
    o.title = NULL;
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &t.c, &o), PF_OK);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_stop(&s, NULL);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "");
    pty_close(&t);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

#if HAVE_PTY
static int custom_spins;
static void custom_spinner(pf_cli_spinner_t *s) {
    custom_spins++;
    fprintf(s->cli->out.file, "<%s>", s->opts.frames[s->frame]);
}
#endif

static int test_pty_spinner_custom(int seed, int i) {
#if HAVE_PTY
    static const char *const frames[] = { "a", "b", NULL };
    pf_cli_spinner_opts_t o;
    pf_cli_spinner_t s;
    struct pty t;
    (void)seed, (void)i;

    NEED_PTY(t, PF_COLOR_NEVER);
    memset(&o, 0, sizeof(o));
    o.frames = frames;
    o.render = custom_spinner;
    o.interval_ms = 1;
    custom_spins = 0;
    PF_ASSERT_ALWAYS_EQ(pf_cli_spinner_start(&s, &t.c, &o), PF_OK);
    pf_cli_spinner_tick(&s);
    pf_sleep_ns(3000000);
    pf_cli_spinner_tick(&s);
    pf_sleep_ns(3000000);
    pf_cli_spinner_tick(&s);
    PF_ASSERT_ALWAYS_EQ(custom_spins, 3);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "<a><b><a>");
    pf_cli_spinner_stop(&s, NULL);
    pty_close(&t);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

static int test_pty_progress(int seed, int i) {
#if HAVE_PTY
    pf_cli_progress_opts_t o;
    pf_cli_progress_t p;
    struct pty t;
    const char *out;
    (void)seed, (void)i;

    NEED_PTY(t, PF_COLOR_NEVER);
    memset(&o, 0, sizeof(o));
    o.title = "copy";
    o.width = 10;
    o.show_percentage = true;
    o.show_value = true;
    o.interval_ms = 1000;
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, &t.c, 10, &o), PF_OK);
    pf_cli_progress_update(&p, 3);
    pf_cli_progress_update(&p, 4); /* within the interval: not drawn */
    PF_ASSERT_ALWAYS_STR_EQ(
        pty_take(&t), "\r\033[2Kcopy [###-------]  30% 3/10"
    );
    pf_cli_progress_render(&p); /* now */
    PF_ASSERT_ALWAYS_STR_EQ(
        pty_take(&t), "\r\033[2Kcopy [####------]  40% 4/10"
    );
    pf_cli_printf(&t.c, "log\n");
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2Klog\n");
    pf_cli_progress_finish(&p);
    PF_ASSERT_ALWAYS_STR_EQ(
        pty_take(&t), "\r\033[2Kcopy [##########] 100% 10/10\n"
    );

    /* Cancelled: the line is cleared. */
    o.show_eta = o.show_rate = o.show_elapsed = true;
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, &t.c, 10, &o), PF_OK);
    pf_cli_progress_update(&p, 5);
    out = pty_take(&t);
    HAS(out, "[#####-----]  50% 5/10 ");
    HAS(out, "/s ETA ");
    pf_cli_progress_stop(&p);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2K");

    /* The bar in its colour. */
    pf_cli_set_color_mode(&t.c, PF_COLOR_ALWAYS);
    memset(&o, 0, sizeof(o));
    o.width = 4;
    o.style = PF_FG_CYAN;
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, &t.c, 4, &o), PF_OK);
    pf_cli_progress_update(&p, 1);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\r\033[2K[\033[36m#---\033[0m]");
    pf_cli_progress_stop(&p);
    pty_take(&t);
    pty_close(&t);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

#if HAVE_PTY
static unsigned custom_filled;
static void custom_progress(
    pf_cli_progress_t *p, const pf_cli_progress_frame_t *f
) {
    (void)p;
    custom_filled = f->filled;
}
#endif

static int test_pty_progress_custom(int seed, int i) {
#if HAVE_PTY
    pf_cli_progress_opts_t o;
    pf_cli_progress_t p;
    struct pty t;
    (void)seed, (void)i;

    NEED_PTY(t, PF_COLOR_NEVER);
    memset(&o, 0, sizeof(o));
    o.width = 8;
    o.render = custom_progress;
    PF_ASSERT_ALWAYS_EQ(pf_cli_progress_start(&p, &t.c, 2, &o), PF_OK);
    pf_cli_progress_update(&p, 1);
    PF_ASSERT_ALWAYS_EQ(custom_filled, 4);
    pf_cli_progress_finish(&p);
    PF_ASSERT_ALWAYS_EQ(custom_filled, 8);
    PF_ASSERT_ALWAYS_STR_EQ(pty_take(&t), "\n");
    pty_close(&t);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("no pseudo-terminals");
#endif
}

/* ---- argparse ---------------------------------------------------------- */

#if PF_TEST_ARGPARSE
static int args_cb(pf_argparser_t *p, void *user) {
    (void)user;
    pf_option_common(p, PF_COMMON_ALL);
    return 0;
}

static int run_args(struct rig *r, pf_argparser_t *p, char **argv, int argc) {
    PF_ASSERT_ALWAYS_EQ(pf_argparser_init(p, argc, argv), PF_OK);
    p->err_prefix = "prog: ";
    pf_cli_bind_args(&r->c, p);
    PF_ASSERT_ALWAYS(p->out == r->out && p->err == r->err);
    return pf_argparser_run(p, args_cb, NULL);
}
#endif

static int test_args(int seed, int i) {
#if PF_TEST_ARGPARSE
    struct rig r;
    pf_argparser_t p;
    char *bad[] = { "prog", "--bogus", NULL };
    char *nocolor[] = { "prog", "--no-color", "--bogus", NULL };
    char *opts[] = { "prog", "-vv", "--color=always", "-i", NULL };
    char *quiet[] = { "prog", "-q", "--bogus", NULL };
    (void)seed, (void)i;

    /* Errors through the CLI, the prefix in bold red. */
    rig_open(&r, NULL, PF_COLOR_ALWAYS, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS_EQ(run_args(&r, &p, bad, 2), PF_EINVAL);
    PF_ASSERT_ALWAYS_STR_EQ(
        take(r.err), "\033[1;31mprog: \033[0munknown option '--bogus'\n"
    );
    pf_argparser_free(&p);

    /* --no-color before the error applies to it. */
    PF_ASSERT_ALWAYS_EQ(run_args(&r, &p, nocolor, 3), PF_EINVAL);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.err), "prog: unknown option '--bogus'\n");
    pf_argparser_free(&p);
    rig_close(&r);

    /* --quiet: the error is not printed. */
    rig_open(&r, NULL, PF_COLOR_NEVER, PF_INTERACTIVE_AUTO);
    PF_ASSERT_ALWAYS_EQ(run_args(&r, &p, quiet, 3), PF_EINVAL);
    PF_ASSERT_ALWAYS_STR_EQ(take(r.err), "");
    pf_cli_apply_args(&r.c, &p);
    PF_ASSERT_ALWAYS(r.c.silent);
    pf_argparser_free(&p);

    /* After the run the options reach the CLI. */
    r.c.verbosity = 1;
    PF_ASSERT_ALWAYS_EQ(run_args(&r, &p, opts, 4), PF_OK);
    PF_ASSERT_ALWAYS_EQ(p.verbosity, 3); /* from the CLI's 1 */
    pf_cli_apply_args(&r.c, &p);
    PF_ASSERT_ALWAYS_EQ(r.c.verbosity, 3);
    PF_ASSERT_ALWAYS(!r.c.silent);
    PF_ASSERT_ALWAYS(r.c.color_mode == PF_COLOR_ALWAYS && r.c.out.colors > 0);
    PF_ASSERT_ALWAYS(r.c.interactive_mode == PF_INTERACTIVE_ALWAYS);
    PF_ASSERT_ALWAYS(r.c.interactive);

    /* Help lines for the common options. */
    pf_cli_set_color_mode(&r.c, PF_COLOR_NEVER);
    pf_cli_help_options(&r.c, &p, PF_COMMON_HELP | PF_COMMON_COLOR);
    {
        char expect[256];
        snprintf(
            expect,
            sizeof(expect),
            "  %-30sShows this information.\n  %-30sColors the output: "
            "'auto', 'always' or 'never'.\n",
            "-?, --help",
            "    --color[=WHEN]"
        );
        PF_ASSERT_ALWAYS_STR_EQ(take(r.out), expect);
    }
    pf_cli_bind_args(NULL, &p);
    pf_cli_apply_args(&r.c, NULL);
    pf_cli_help_options(NULL, &p, PF_COMMON_ALL);
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
#else
    (void)seed, (void)i;
    return SKIP("built without argparse");
#endif
}

pf_test_t suite_cli[] = {
    { test_init, "init" },
    { test_files, "files" },
    { test_printf, "printf" },
    { test_styles, "styles" },
    { test_links, "links" },
    { test_text_width, "text_width" },
    { test_wrap, "wrap" },
    { test_help, "help" },
    { test_readline, "readline" },
    { test_prompts_not_interactive, "prompts_not_interactive" },
    { test_prompts, "prompts" },
    { test_spinner_redirected, "spinner_redirected" },
    { test_spinner_frames, "spinner_frames" },
    { test_progress_redirected, "progress_redirected" },
    { test_pty_detection, "pty_detection" },
    { test_pty_cursor, "pty_cursor" },
    { test_pty_spinner, "pty_spinner" },
    { test_pty_spinner_custom, "pty_spinner_custom" },
    { test_pty_progress, "pty_progress" },
    { test_pty_progress_custom, "pty_progress_custom" },
    { test_args, "args" },
    { 0 },
};
