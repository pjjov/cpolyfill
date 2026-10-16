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

/* Pseudo terminal functions are hidden by default on glibc. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#include "check.h"
#include "../include/pf_cli.h"
#include "../include/pf_argparse.h"
#include "util.h"

#ifndef _WIN32
    #include <fcntl.h>
    #include <poll.h>
    #include <termios.h>
#endif

/* ---------------------------------------------------------------- */
/* Harness                                                           */
/* ---------------------------------------------------------------- */

/* A cli writing to temporary files, reading from a string. */
struct rig {
    pf_cli_t cli;
    FILE *out, *err, *in;
};

static void rig_env(void) {
    /* Make the output independent of the environment running the tests. */
    test_setenv("COLUMNS", "80");
    test_setenv("NO_COLOR", NULL);
    test_setenv("NO_HYPERLINKS", NULL);
    test_setenv("TERM", "xterm");
}

static int rig_open(
    struct rig *r, const char *input, enum pf_color_mode_t color
) {
    struct pf_cli_opt o;

    rig_env();
    r->out = tmpfile();
    r->err = tmpfile();
    r->in = tmpfile();
    if (!r->out || !r->err || !r->in)
        return -1;
    if (input) {
        fputs(input, r->in);
        rewind(r->in);
    }

    memset(&o, 0, sizeof o);
    o.out = r->out;
    o.err = r->err;
    o.in = r->in;
    o.colorMode = color;
    pf_cli_init(&r->cli, &o);
    return 0;
}

static void rig_close(struct rig *r) {
    pf_cli_free(&r->cli);
    fclose(r->out);
    fclose(r->err);
    fclose(r->in);
}

static void slurp(FILE *f, char *buf, size_t size) {
    size_t n;
    fflush(f);
    rewind(f);
    n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    /* Start over for the next round. */
    rewind(f);
#ifdef _WIN32
    _chsize_s(_fileno(f), 0);
#else
    if (ftruncate(fileno(f), 0) != 0)
        buf[0] = buf[0];
#endif
}

static void rig_out(struct rig *r, char *buf, size_t size) {
    slurp(r->out, buf, size);
}

static void rig_err(struct rig *r, char *buf, size_t size) {
    slurp(r->err, buf, size);
}

/* ---------------------------------------------------------------- */
/* Initialization                                                    */
/* ---------------------------------------------------------------- */

static int test_cli_init(int seed, int rep) {
    struct rig r;

    CHECK(rig_open(&r, NULL, PF_COLOR_AUTO) == 0);

    CHECK(r.cli.out.handle == r.out);
    CHECK(r.cli.err.handle == r.err);
    CHECK(r.cli.in.handle == r.in);
    CHECK(!r.cli.silent);
    CHECK(r.cli.verbosity == 0);
    CHECK(r.cli.definitionIndent == 10);
    CHECK(r.cli.live == NULL);

    /* Files are no terminals: no colors, no escape sequences. */
    CHECK(!r.cli.out.isTerminal && !r.cli.out.supportsAnsi);
    CHECK(!r.cli.out.supportsCursor && !r.cli.out.supportsHyperlinks);
    CHECK(!r.cli.out.colorEnabled && !r.cli.err.colorEnabled);
    CHECK(!r.cli.isInteractive);
    CHECK(r.cli.out.columns == 80);

    rig_close(&r);
    return 0;
}

static int test_cli_init_defaults(int seed, int rep) {
    pf_cli_t c;

    pf_cli_init(&c, NULL);
    CHECK(c.out.handle == stdout);
    CHECK(c.err.handle == stderr);
    CHECK(c.in.handle == stdin);
    CHECK(c.definitionIndent == 10);
    pf_cli_free(&c);

    pf_cli_init(NULL, NULL); /* must not crash */
    return 0;
}

static int test_cli_init_options(int seed, int rep) {
    struct pf_cli_opt o;
    pf_cli_t c;

    memset(&o, 0, sizeof o);
    o.definitionIndent = 4;
    pf_cli_init(&c, &o);
    CHECK(c.definitionIndent == 4);

    /* Out of range values fall back to the default. */
    o.definitionIndent = 200;
    pf_cli_init(&c, &o);
    CHECK(c.definitionIndent == 10);
    o.definitionIndent = -3;
    pf_cli_init(&c, &o);
    CHECK(c.definitionIndent == 10);
    return 0;
}

static int test_cli_null_streams(int seed, int rep) {
    struct pf_cli_opt o;
    pf_cli_t c;

    memset(&o, 0, sizeof o);
    o.allowNullStreams = PF_TRUE;
    pf_cli_init(&c, &o);

    CHECK(c.out.handle == NULL && c.err.handle == NULL && c.in.handle == NULL);
    CHECK(c.silent); /* nowhere to print to */

    /* Everything is a no-op. */
    pf_cli_printf(&c, "x");
    pf_cli_errorf(&c, "x");
    pf_cli_cprintf(&c, PF_FG_RED, "x");
    pf_cli_help_definition(&c, "a", "b");
    pf_cli_help_section(&c, "t");
    pf_cli_link(&c, "http://x", "x");
    {
        char buf[8];
        CHECK(pf_cli_readline(&c, buf, sizeof buf) == 0);
        CHECK(pf_cli_confirm(&c, "q", 1) == 1);
        CHECK(pf_cli_confirm(&c, "q", 0) == 0);
    }
    return 0;
}

static int test_cli_colors_modes(int seed, int rep) {
    struct rig r;

    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    CHECK(r.cli.out.colorEnabled && r.cli.err.colorEnabled);
    pf_cli_set_color_mode(&r.cli, PF_COLOR_NEVER);
    CHECK(!r.cli.out.colorEnabled && !r.cli.err.colorEnabled);
    CHECK(r.cli.colorMode == PF_COLOR_NEVER);
    pf_cli_set_color_mode(&r.cli, PF_COLOR_ALWAYS);
    CHECK(r.cli.out.colorEnabled && r.cli.err.colorEnabled);
    /* Not a terminal, so no hyperlinks even with color. */
    CHECK(!r.cli.out.supportsHyperlinks);
    rig_close(&r);

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(!r.cli.out.colorEnabled);
    rig_close(&r);

    pf_cli_set_color_mode(NULL, PF_COLOR_ALWAYS);
    return 0;
}

static int test_cli_interactive_modes(int seed, int rep) {
    struct rig r;

    CHECK(rig_open(&r, NULL, PF_COLOR_AUTO) == 0);
    CHECK(!r.cli.isInteractive);
    pf_cli_set_interactive_mode(&r.cli, PF_INTERACTIVE_ALWAYS);
    CHECK(r.cli.isInteractive);
    pf_cli_set_interactive_mode(&r.cli, PF_INTERACTIVE_NEVER);
    CHECK(!r.cli.isInteractive);
    pf_cli_set_interactive_mode(&r.cli, PF_INTERACTIVE_AUTO);
    CHECK(!r.cli.isInteractive); /* files again */
    rig_close(&r);

    pf_cli_set_interactive_mode(NULL, PF_INTERACTIVE_ALWAYS);
    return 0;
}

static int test_cli_columns(int seed, int rep) {
    struct pf_cli_opt o;
    pf_cli_t c;
    FILE *f = tmpfile();

    memset(&o, 0, sizeof o);
    o.out = o.err = f;

    CHECK(test_setenv("COLUMNS", "132") == 0);
    pf_cli_init(&c, &o);
    CHECK(c.out.columns == 132);

    /* Unreasonable values are ignored. */
    CHECK(test_setenv("COLUMNS", "5") == 0);
    pf_cli_init(&c, &o);
    CHECK(c.out.columns == 80);
    CHECK(test_setenv("COLUMNS", "99999") == 0);
    pf_cli_init(&c, &o);
    CHECK(c.out.columns == 80);
    CHECK(test_setenv("COLUMNS", "abc") == 0);
    pf_cli_init(&c, &o);
    CHECK(c.out.columns == 80);
    CHECK(test_setenv("COLUMNS", NULL) == 0);
    pf_cli_init(&c, &o);
    CHECK(c.out.columns == 80);

    fclose(f);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Output                                                            */
/* ---------------------------------------------------------------- */

static int test_cli_printf(int seed, int rep) {
    struct rig r;
    char out[128], err[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    pf_cli_printf(&r.cli, "hello %s %d\n", "world", 42);
    pf_cli_errorf(&r.cli, "oops %c\n", 'x');
    rig_out(&r, out, sizeof out);
    rig_err(&r, err, sizeof err);
    CHECK_STR(out, "hello world 42\n");
    CHECK_STR(err, "oops x\n");
    rig_close(&r);
    return 0;
}

static int test_cli_silent(int seed, int rep) {
    struct rig r;
    char out[64], err[64];

    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    r.cli.silent = PF_TRUE;

    pf_cli_printf(&r.cli, "a");
    pf_cli_errorf(&r.cli, "b");
    pf_cli_cprintf(&r.cli, PF_FG_RED, "c");
    pf_cli_ecprintf(&r.cli, PF_FG_RED, "d");
    pf_cli_help_definition(&r.cli, "e", "f");
    pf_cli_help_section(&r.cli, "g");
    pf_cli_color(&r.cli, PF_CLI_BOLD);
    pf_cli_link(&r.cli, "http://x", "h");

    rig_out(&r, out, sizeof out);
    rig_err(&r, err, sizeof err);
    CHECK_STR(out, "");
    CHECK_STR(err, "");
    rig_close(&r);
    return 0;
}

static int test_cli_color_output(int seed, int rep) {
    struct rig r;
    char out[256], err[256];

    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);

    pf_cli_cprintf(&r.cli, PF_FG_RED, "red");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[31mred\033[0m");

    pf_cli_cprintf(&r.cli, PF_FG_GREEN | PF_CLI_BOLD, "x%d", 1);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[1m\033[32mx1\033[0m");

    pf_cli_cprintf(&r.cli, PF_CLI_DIM | PF_BG_BLUE, "d");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[2m\033[44md\033[0m");

    pf_cli_cprintf(&r.cli, 0, "plain");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "plain");

    pf_cli_ecprintf(&r.cli, PF_FG_YELLOW, "warn");
    rig_err(&r, err, sizeof err);
    CHECK_STR(err, "\033[33mwarn\033[0m");

    pf_cli_color(&r.cli, PF_CLI_BOLD);
    pf_cli_printf(&r.cli, "b");
    pf_cli_color(&r.cli, PF_CLI_RESET);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[1mb\033[0m");
    rig_close(&r);
    return 0;
}

static int test_cli_color_disabled(int seed, int rep) {
    struct rig r;
    char out[64];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    pf_cli_cprintf(&r.cli, PF_FG_RED | PF_CLI_BOLD, "text");
    pf_cli_color(&r.cli, PF_FG_BLUE);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "text");
    rig_close(&r);
    return 0;
}

static int test_cli_no_color_env(int seed, int rep) {
    struct pf_cli_opt o;
    pf_cli_t c;
    FILE *f = tmpfile();

    memset(&o, 0, sizeof o);
    o.out = o.err = f;
    o.colorMode = PF_COLOR_AUTO;

    CHECK(test_setenv("NO_COLOR", "1") == 0);
    pf_cli_init(&c, &o);
    CHECK(!c.out.colorEnabled);

    /* An explicit request beats the environment. */
    o.colorMode = PF_COLOR_ALWAYS;
    pf_cli_init(&c, &o);
    CHECK(c.out.colorEnabled);

    test_setenv("NO_COLOR", NULL);
    fclose(f);
    return 0;
}

static int test_cli_verbosef(int seed, int rep) {
    struct rig r;
    char out[64];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    pf_cli_verbosef(&r.cli, 1, "one\n");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");

    r.cli.verbosity = 1;
    pf_cli_verbosef(&r.cli, 1, "one %d\n", 1);
    pf_cli_verbosef(&r.cli, 2, "two\n");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "one 1\n");

    r.cli.verbosity = 3;
    pf_cli_verbosef(&r.cli, 2, "two\n");
    pf_cli_verbosef(&r.cli, 3, "three\n");
    pf_cli_verbosef(&r.cli, 4, "four\n");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "two\nthree\n");

    /* Level 0 prints always. */
    r.cli.verbosity = 0;
    pf_cli_verbosef(&r.cli, 0, "zero\n");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "zero\n");
    rig_close(&r);
    return 0;
}

static int test_cli_null_cli(int seed, int rep) {
    char buf[8];

    /* None of these may crash. */
    pf_cli_printf(NULL, "x");
    pf_cli_errorf(NULL, "x");
    pf_cli_cprintf(NULL, PF_FG_RED, "x");
    pf_cli_ecprintf(NULL, PF_FG_RED, "x");
    pf_cli_verbosef(NULL, 1, "x");
    pf_cli_color(NULL, PF_CLI_BOLD);
    pf_cli_clearline(NULL, PF_CLEAR_ALL);
    pf_cli_clear(NULL, PF_CLEAR_ALL);
    pf_cli_move(NULL, 1, 1);
    pf_cli_position(NULL, 1, 1);
    pf_cli_wrap(NULL, 0, 0, 80, "x");
    pf_cli_help_definition(NULL, "a", "b");
    pf_cli_help_definitions(NULL, NULL);
    pf_cli_help_section(NULL, "x");
    pf_cli_link(NULL, "u", "t");
    pf_cli_path_link(NULL, "p", 1);
    pf_cli_free(NULL);

    CHECK(pf_cli_readline(NULL, buf, sizeof buf) == 0);
    CHECK(pf_cli_ask(NULL, "q", buf, sizeof buf) == 0);
    CHECK(pf_cli_confirm(NULL, "q", 1) == 1);
    CHECK(pf_cli_select(NULL, "q", NULL, 0, 0) == -1);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Text layout                                                       */
/* ---------------------------------------------------------------- */

static int test_cli_text_width(int seed, int rep) {
    CHECK(pf_cli_text_width(NULL) == 0);
    CHECK(pf_cli_text_width("") == 0);
    CHECK(pf_cli_text_width("hello") == 5);
    CHECK(pf_cli_text_width("a b c") == 5);

    /* Colors take no space. */
    CHECK(pf_cli_text_width("\033[31mred\033[0m") == 3);
    CHECK(pf_cli_text_width("\033[1;31;44mx\033[0m") == 1);
    CHECK(pf_cli_text_width("a\033[2Kb") == 2);
    CHECK(pf_cli_text_width("\033[31m") == 0);

    /* Neither do line breaks. */
    CHECK(pf_cli_text_width("ab\ncd\r\n") == 4);

    /* A dangling escape does not run past the string. */
    CHECK(pf_cli_text_width("ab\033[") == 2);
    CHECK(pf_cli_text_width("ab\033[31") == 2);
    return 0;
}

static int test_cli_fill(int seed, int rep) {
    FILE *f = tmpfile();
    char buf[32];

    pf_cli_fill(f, 3, '-');
    pf_cli_fill(f, 0, 'x');
    pf_cli_fill(f, 2, ' ');
    pf_cli_fill(NULL, 5, 'x');
    slurp(f, buf, sizeof buf);
    CHECK_STR(buf, "---  ");
    fclose(f);
    return 0;
}

static int test_cli_wrap(int seed, int rep) {
    struct rig r;
    char out[256];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    pf_cli_wrap(&r.cli, 0, 0, 10, "aaa bbb ccc ddd");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "aaa bbb\nccc ddd");

    pf_cli_wrap(&r.cli, 2, 4, 10, "aaa bbb ccc ddd");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "  aaa bbb\n    ccc\n    ddd");

    /* Fits on one line. */
    pf_cli_wrap(&r.cli, 0, 0, 80, "short text");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "short text");

    /* Exactly the width. */
    pf_cli_wrap(&r.cli, 0, 0, 10, "aaaa bbbbb");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "aaaa bbbbb");
    pf_cli_wrap(&r.cli, 0, 0, 10, "aaaa bbbbbb");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "aaaa\nbbbbbb");

    rig_close(&r);
    return 0;
}

static int test_cli_wrap_edge_cases(int seed, int rep) {
    struct rig r;
    char out[256];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    /* A word longer than the line gets a line to itself. */
    pf_cli_wrap(&r.cli, 0, 0, 10, "hi supercalifragilistic yo");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "hi\nsupercalifragilistic\nyo");

    /* Runs of blanks collapse. */
    pf_cli_wrap(&r.cli, 0, 0, 40, "a   b\t\tc");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "a b c");

    /* Explicit line breaks are kept, and indent the next line. */
    pf_cli_wrap(&r.cli, 0, 3, 40, "one\ntwo");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "one\n   two");

    /* Empty and blank input print nothing. */
    pf_cli_wrap(&r.cli, 0, 0, 40, "");
    pf_cli_wrap(&r.cli, 0, 0, 40, "   ");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");

    /* Widths below 10 are raised to 10. */
    pf_cli_wrap(&r.cli, 0, 0, 3, "aaaa bbbb cc");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "aaaa bbbb\ncc");

    pf_cli_wrap(&r.cli, 0, 0, 40, NULL);
    rig_close(&r);
    return 0;
}

static int test_cli_wrap_colors(int seed, int rep) {
    struct rig r;
    char out[256];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    /* Escape sequences do not count towards the width. */
    pf_cli_wrap(&r.cli, 0, 0, 10, "\033[31maaaa\033[0m \033[1mbbbbb\033[0m");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[31maaaa\033[0m \033[1mbbbbb\033[0m");
    rig_close(&r);
    return 0;
}

static int test_cli_help_definition(int seed, int rep) {
    struct rig r;
    char out[512], want[512];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    /* The name at the indent, the description 30 columns on. */
    pf_cli_help_definition(&r.cli, "--foo", "Does foo.");
    rig_out(&r, out, sizeof out);
    snprintf(want, sizeof want, "%10s%-30s%s\n", "", "--foo", "Does foo.");
    CHECK_STR(out, want);
    CHECK(strlen(out) == 10 + 30 + 9 + 1);

    /* Empty and missing parts. */
    pf_cli_help_definition(&r.cli, "-x", "");
    rig_out(&r, out, sizeof out);
    snprintf(want, sizeof want, "%10s%-30s\n", "", "-x");
    CHECK_STR(out, want);

    pf_cli_help_definition(&r.cli, NULL, NULL);
    rig_out(&r, out, sizeof out);
    snprintf(want, sizeof want, "%10s%-30s\n", "", "");
    CHECK_STR(out, want);

    rig_close(&r);
    return 0;
}

static int test_cli_help_definition_wrapping(int seed, int rep) {
    struct rig r;
    char out[1024];
    char *nl;

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    pf_cli_help_definition(
        &r.cli,
        "--long-option",
        "This description is deliberately long enough that it has to be "
        "wrapped onto a second line of the help output."
    );
    rig_out(&r, out, sizeof out);

    /* No line is wider than the terminal, continuation lines are aligned. */
    nl = strchr(out, '\n');
    CHECK(nl != NULL);
    CHECK((size_t)(nl - out) <= 80);
    CHECK(strncmp(nl + 1, "                                        ", 40) == 0);
    CHECK(nl[41] != ' ');
    CHECK(strchr(nl + 1, '\n') != NULL);
    rig_close(&r);
    return 0;
}

static int test_cli_help_definition_long_name(int seed, int rep) {
    struct rig r;
    char out[512], want[512];
    const char *name = "--a-very-long-option-name=<VALUE>";

    CHECK(strlen(name) > 30);
    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    /* Long names get a line, and the description is indented below. */
    pf_cli_help_definition(&r.cli, name, "Description.");
    rig_out(&r, out, sizeof out);
    snprintf(
        want, sizeof want, "%10s%s\n%14s%s\n", "", name, "", "Description."
    );
    CHECK(strncmp(out, want, strlen(want) - 0) == 0);
    rig_close(&r);
    return 0;
}

static int test_cli_help_definition_indent(int seed, int rep) {
    struct pf_cli_opt o;
    pf_cli_t c;
    FILE *f = tmpfile();
    char out[256], want[256];

    rig_env();
    memset(&o, 0, sizeof o);
    o.out = o.err = f;
    o.definitionIndent = 2;
    pf_cli_init(&c, &o);

    pf_cli_help_definition(&c, "-h", "Help.");
    slurp(f, out, sizeof out);
    snprintf(want, sizeof want, "%2s%-30s%s\n", "", "-h", "Help.");
    CHECK_STR(out, want);
    fclose(f);
    return 0;
}

static int test_cli_help_definition_narrow(int seed, int rep) {
    struct pf_cli_opt o;
    pf_cli_t c;
    FILE *f = tmpfile();
    char out[256];

    rig_env();
    CHECK(test_setenv("COLUMNS", "30") == 0);
    memset(&o, 0, sizeof o);
    o.out = o.err = f;
    pf_cli_init(&c, &o);
    CHECK(c.out.columns == 30);

    /* No room beside the name: the description goes below it. */
    pf_cli_help_definition(&c, "--opt", "Some description text.");
    slurp(f, out, sizeof out);
    CHECK(strncmp(out, "          --opt\n", 16) == 0);
    CHECK(strstr(out, "\n              Some") != NULL);
    fclose(f);
    return 0;
}

static int test_cli_help_definitions(int seed, int rep) {
    static const pf_cli_definition_t defs[] = {
        { "--one", "First." },
        { "--two", "Second." },
        { "--three", "Third." },
        { 0, 0 },
    };
    struct rig r;
    char out[512];
    int lines = 0;
    const char *p;

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    pf_cli_help_definitions(&r.cli, defs);
    pf_cli_help_definitions(&r.cli, NULL);
    rig_out(&r, out, sizeof out);

    for (p = out; (p = strchr(p, '\n')) != NULL; p++)
        lines++;
    CHECK(lines == 3);
    CHECK(strstr(out, "--one") < strstr(out, "--two"));
    CHECK(strstr(out, "--two") < strstr(out, "--three"));
    CHECK(strstr(out, "Third.") != NULL);
    rig_close(&r);
    return 0;
}

static int test_cli_help_section(int seed, int rep) {
    struct rig r;
    char out[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    pf_cli_help_section(&r.cli, "Options:");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\nOptions:\n");
    pf_cli_help_section(&r.cli, NULL);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\n\n");
    rig_close(&r);

    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    pf_cli_help_section(&r.cli, "Usage:");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\n\033[1mUsage:\033[0m\n");
    rig_close(&r);
    return 0;
}

static int test_cli_links(int seed, int rep) {
    struct rig r;
    char out[128];

    /* Without terminal support, only the text is shown. */
    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    pf_cli_link(&r.cli, "https://example.com", "example");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "example");

    pf_cli_path_link(&r.cli, "/tmp/file.c", 12);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "/tmp/file.c");

    pf_cli_link(&r.cli, NULL, "text");
    pf_cli_link(&r.cli, "url", NULL);
    pf_cli_path_link(&r.cli, NULL, 0);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");
    rig_close(&r);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Input                                                             */
/* ---------------------------------------------------------------- */

static int test_cli_readline(int seed, int rep) {
    struct rig r;
    char buf[16];

    CHECK(
        rig_open(&r, "first\nsecond\r\n\nlast without newline", PF_COLOR_NEVER)
        == 0
    );

    CHECK(pf_cli_readline(&r.cli, buf, sizeof buf) == 1);
    CHECK_STR(buf, "first");
    CHECK(pf_cli_readline(&r.cli, buf, sizeof buf) == 1);
    CHECK_STR(buf, "second"); /* CRLF handled */
    CHECK(pf_cli_readline(&r.cli, buf, sizeof buf) == 1);
    CHECK_STR(buf, "");
    CHECK(pf_cli_readline(&r.cli, buf, sizeof buf) == 1);
    CHECK_STR(buf, "last without ne"); /* cut to the size of the buffer */
    CHECK(pf_cli_readline(&r.cli, buf, sizeof buf) == 1); /* the rest */
    CHECK_STR(buf, "wline");
    CHECK(pf_cli_readline(&r.cli, buf, sizeof buf) == 0); /* end of input */

    CHECK(pf_cli_readline(&r.cli, NULL, 5) == 0);
    CHECK(pf_cli_readline(&r.cli, buf, 0) == 0);
    rig_close(&r);
    return 0;
}

static int test_cli_ask(int seed, int rep) {
    struct rig r;
    char answer[32], out[64];

    CHECK(rig_open(&r, "Ada\n", PF_COLOR_NEVER) == 0);
    CHECK(pf_cli_ask(&r.cli, "Name", answer, sizeof answer) == 1);
    CHECK_STR(answer, "Ada");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "Name: ");

    /* Nothing left to read. */
    CHECK(pf_cli_ask(&r.cli, "Name", answer, sizeof answer) == 0);

    /* No prompt in non-interactive mode. */
    pf_cli_set_interactive_mode(&r.cli, PF_INTERACTIVE_NEVER);
    CHECK(pf_cli_ask(&r.cli, "Name", answer, sizeof answer) == 0);
    rig_close(&r);
    return 0;
}

static int test_cli_confirm(int seed, int rep) {
    struct rig r;
    char out[64];

    CHECK(rig_open(&r, "y\nY\n1\nn\nN\nyes\nno\nx\n\n", PF_COLOR_NEVER) == 0);
    CHECK(pf_cli_confirm(&r.cli, "Go?", 0) == 1);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "Go? [y/N]: ");

    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 1); /* Y */
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "Go? [Y/n]: ");

    CHECK(pf_cli_confirm(&r.cli, "Go?", 0) == 1); /* 1 */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 0); /* n */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 0); /* N */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 0) == 1); /* yes */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 0); /* no */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 0); /* anything else is no */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 1); /* empty: the default */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 0) == 0); /* end of input: default */
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 1);
    rig_close(&r);
    return 0;
}

static int test_cli_confirm_never(int seed, int rep) {
    struct rig r;
    char out[64];

    CHECK(rig_open(&r, "y\n", PF_COLOR_NEVER) == 0);
    pf_cli_set_interactive_mode(&r.cli, PF_INTERACTIVE_NEVER);
    CHECK(pf_cli_confirm(&r.cli, "Go?", 0) == 0);
    CHECK(pf_cli_confirm(&r.cli, "Go?", 1) == 1);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, ""); /* it did not even ask */
    rig_close(&r);
    return 0;
}

static int test_cli_select(int seed, int rep) {
    static const char *choices[] = { "red", "green", "blue" };
    struct rig r;
    char out[256];

    CHECK(rig_open(&r, "2\n\nblue\n7\n0\nteal\n", PF_COLOR_NEVER) == 0);

    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 1) == 1);
    rig_out(&r, out, sizeof out);
    CHECK_STR(
        out, "Pick:\n  1) red\n  2) green (default)\n  3) blue\nChoice [2]: "
    );

    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 2) == 2);  /* default */
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 0) == 2);  /* by name */
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 0) == -1); /* 7 */
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 0) == -1); /* 0 */
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 0) == -1); /* teal */
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 0) == -1); /* end */

    CHECK(pf_cli_select(&r.cli, "Pick", NULL, 3, 0) == -1);
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 0, 0) == -1);

    /* A default out of range is the first choice. */
    rig_out(&r, out, sizeof out);
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 9) == -1);
    rig_out(&r, out, sizeof out);
    CHECK(strstr(out, "1) red (default)") != NULL);

    pf_cli_set_interactive_mode(&r.cli, PF_INTERACTIVE_NEVER);
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 2) == 2);
    CHECK(pf_cli_select(&r.cli, "Pick", choices, 3, 9) == 0);
    rig_close(&r);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Spinner and progress, without a terminal                          */
/* ---------------------------------------------------------------- */

static int test_cli_spinner_redirected(int seed, int rep) {
    struct rig r;
    pf_cli_spinner_t s;
    struct pf_cli_spinner_opt o;
    char out[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    memset(&o, 0, sizeof o);
    o.title = "Working";
    CHECK(pf_cli_spinner_start(&s, &r.cli, &o) == 0);
    CHECK(s.active);

    /* No frames are drawn into files. */
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");

    pf_cli_spinner_set_title(&s, "Step %d", 2);
    CHECK_STR(s.options.title, "Step 2");

    pf_cli_spinner_finish(&s, PF_TRUE, "Done in %d steps", 2);
    CHECK(!s.active);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "[ok] Done in 2 steps\n");
    rig_close(&r);
    return 0;
}

static int test_cli_spinner_finish_variants(int seed, int rep) {
    struct rig r;
    pf_cli_spinner_t s;
    char out[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_finish(&s, PF_FALSE, "It broke");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "[fail] It broke\n");

    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_stop(&s, "stopped");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "stopped\n");

    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_stop(&s, NULL);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");

    /* Finishing twice prints once. */
    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_finish(&s, PF_TRUE, "once");
    pf_cli_spinner_finish(&s, PF_TRUE, "twice");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "[ok] once\n");
    rig_close(&r);
    return 0;
}

static int test_cli_spinner_colored_marker(int seed, int rep) {
    struct rig r;
    pf_cli_spinner_t s;
    char out[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_finish(&s, PF_TRUE, "fine");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[32m[ok]\033[0m fine\n");

    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_finish(&s, PF_FALSE, "bad");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "\033[31m[fail]\033[0m bad\n");
    rig_close(&r);
    return 0;
}

static int test_cli_spinner_silent(int seed, int rep) {
    struct rig r;
    pf_cli_spinner_t s;
    char out[64];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    r.cli.silent = PF_TRUE;
    CHECK(pf_cli_spinner_start(&s, &r.cli, NULL) == 0);
    pf_cli_spinner_finish(&s, PF_TRUE, "quiet");
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");
    rig_close(&r);
    return 0;
}

static int test_cli_spinner_invalid(int seed, int rep) {
    struct rig r;
    pf_cli_spinner_t s;
    struct pf_cli_spinner_opt o;
    static const char *const none[] = { NULL };

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(pf_cli_spinner_start(NULL, &r.cli, NULL) == -1);
    CHECK(pf_cli_spinner_start(&s, NULL, NULL) == -1);

    memset(&o, 0, sizeof o);
    o.frames = none; /* no frames at all */
    CHECK(pf_cli_spinner_start(&s, &r.cli, &o) == -1);

    pf_cli_spinner_tick(NULL);
    pf_cli_spinner_set_title(NULL, "x");
    pf_cli_spinner_stop(NULL, "x");
    pf_cli_spinner_finish(NULL, PF_TRUE, "x");
    rig_close(&r);
    return 0;
}

static int test_cli_spinner_frames(int seed, int rep) {
    CHECK(pf_cli_spinner_line[0] != NULL);
    CHECK(pf_cli_spinner_line[4] == NULL);
    CHECK(pf_cli_spinner_dots[0] != NULL);
    CHECK(pf_cli_spinner_dots[10] == NULL);
    return 0;
}

static int test_cli_progress_redirected(int seed, int rep) {
    struct rig r;
    pf_cli_progress_t p;
    struct pf_cli_progress_opt o;
    char out[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    memset(&o, 0, sizeof o);
    o.title = "Download";
    CHECK(pf_cli_progress_start(&p, &r.cli, 200.0, &o) == 0);
    CHECK(p.active);
    CHECK(p.maximum == 200.0);
    CHECK(p.value == 0.0);
    CHECK(p.options.fill == '#' && p.options.empty == '-');
    CHECK(p.options.width == 20); /* a quarter of the line */

    pf_cli_progress_update(&p, 50);
    CHECK(p.value == 50.0);
    pf_cli_progress_advance(&p, 25);
    CHECK(p.value == 75.0);
    pf_cli_progress_advance(&p, -100);
    CHECK(p.value == 0.0); /* clamped */
    pf_cli_progress_update(&p, 1000);
    CHECK(p.value == 200.0);
    pf_cli_progress_update(&p, -5);
    CHECK(p.value == 0.0);

    pf_cli_progress_set_title(&p, "Item %d", 3);
    CHECK_STR(p.title, "Item 3");

    /* Nothing is drawn for redirected output. */
    pf_cli_progress_render(&p);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");

    pf__cli_progress_stop(&p, PF_TRUE);
    CHECK(!p.active);
    CHECK(p.value == p.maximum);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");
    rig_close(&r);
    return 0;
}

static int test_cli_progress_options(int seed, int rep) {
    struct rig r;
    pf_cli_progress_t p;
    struct pf_cli_progress_opt o;

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);

    memset(&o, 0, sizeof o);
    o.width = 33;
    o.fill = '=';
    o.empty = '.';
    o.color = PF_FG_BLUE;
    CHECK(pf_cli_progress_start(&p, &r.cli, 10.0, &o) == 0);
    CHECK(p.options.width == 33);
    CHECK(p.options.fill == '=' && p.options.empty == '.');
    CHECK(p.options.color == PF_FG_BLUE);

    /* A nonsensical maximum becomes 1. */
    CHECK(pf_cli_progress_start(&p, &r.cli, 0.0, NULL) == 0);
    CHECK(p.maximum == 1.0);
    CHECK(pf_cli_progress_start(&p, &r.cli, -4.0, NULL) == 0);
    CHECK(p.maximum == 1.0);

    CHECK(pf_cli_progress_start(NULL, &r.cli, 1.0, NULL) == -1);
    CHECK(pf_cli_progress_start(&p, NULL, 1.0, NULL) == -1);

    pf_cli_progress_update(NULL, 1);
    pf_cli_progress_advance(NULL, 1);
    pf_cli_progress_render(NULL);
    pf_cli_progress_set_title(NULL, "x");
    pf__cli_progress_stop(NULL, PF_TRUE);
    rig_close(&r);
    return 0;
}

static int test_cli_now(int seed, int rep) {
    double a = pf_cli_now(), b;

    CHECK(a > 0.0);
    test_sleep_ms(20);
    b = pf_cli_now();
    CHECK(b >= a + 0.01);
    CHECK(b < a + 5.0);
    return 0;
}

/* ---------------------------------------------------------------- */
/* argparse integration                                              */
/* ---------------------------------------------------------------- */

struct cli_args {
    int unused;
};

static int common_cb(struct pf_argparser *p, void *user) {
    (void)user;
    pf_option_common(p, PF_COMMON_ALL);
    return 0;
}

static int run_with_cli(
    struct rig *r, struct pf_argparser *p, char **argv, int argc
) {
    CHECK(pf_argparser_init(p, argc, argv) == PF_ARGPARSE_OK);
    pf_argparser_set_cli(p, &r->cli);
    return pf_argparser_run(p, common_cb, NULL);
}

static int test_cli_argparse_binding(int seed, int rep) {
    struct rig r;
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    r.cli.verbosity = 2;

    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    pf_argparser_set_cli(&p, &r.cli);
    CHECK(p.cli == &r.cli);
    CHECK(p.out == r.out && p.err == r.err);
    CHECK(p.verbosity == 2 && p.verbose);
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
}

static int test_cli_argparse_options_apply(int seed, int rep) {
    struct rig r;
    struct pf_argparser p;
    char *argv[] = {
        "prog", "-vv", "--color=always", "--interactive=always", NULL
    };

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(!r.cli.out.colorEnabled && !r.cli.isInteractive);

    CHECK(run_with_cli(&r, &p, argv, 4) == PF_ARGPARSE_OK);

    /* The common options configure the cli directly. */
    CHECK(r.cli.verbosity == 2);
    CHECK(r.cli.out.colorEnabled);
    CHECK(r.cli.colorMode == PF_COLOR_ALWAYS);
    CHECK(r.cli.interactiveMode == PF_INTERACTIVE_ALWAYS);
    CHECK(r.cli.isInteractive);
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
}

static int test_cli_argparse_quiet(int seed, int rep) {
    struct rig r;
    struct pf_argparser p;
    char *argv[] = { "prog", "--quiet", NULL };
    char *back[] = { "prog", "--quiet", "--no-quiet", NULL };

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(run_with_cli(&r, &p, argv, 2) == PF_ARGPARSE_OK);
    CHECK(r.cli.silent);
    pf_argparser_free(&p);

    CHECK(run_with_cli(&r, &p, back, 3) == PF_ARGPARSE_OK);
    CHECK(!r.cli.silent);
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
}

static int test_cli_argparse_errors_through_cli(int seed, int rep) {
    struct rig r;
    struct pf_argparser p;
    char *argv[] = { "prog", "--bogus", NULL };
    char err[128], out[128];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(run_with_cli(&r, &p, argv, 2) == PF_ARGPARSE_EINVAL);
    rig_err(&r, err, sizeof err);
    rig_out(&r, out, sizeof out);
    CHECK_STR(err, "unknown option '--bogus'\n");
    CHECK_STR(out, "");
    pf_argparser_free(&p);

    /* With color, the prefix is highlighted. */
    rig_close(&r);
    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    CHECK(pf_argparser_init(&p, 2, argv) == PF_ARGPARSE_OK);
    pf_argparser_set_cli(&p, &r.cli);
    p.errPrefix = "error: ";
    CHECK(pf_argparser_run(&p, common_cb, NULL) == PF_ARGPARSE_EINVAL);
    rig_err(&r, err, sizeof err);
    CHECK(strstr(err, "\033[1m\033[31merror: \033[0m") != NULL);
    CHECK(strstr(err, "unknown option '--bogus'\n") != NULL);
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
}

static int test_cli_argparse_quiet_errors(int seed, int rep) {
    struct rig r;
    struct pf_argparser p;
    char *argv[] = { "prog", "-q", "--bogus", NULL };
    char err[64];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(run_with_cli(&r, &p, argv, 3) == PF_ARGPARSE_EINVAL);
    rig_err(&r, err, sizeof err);
    CHECK_STR(err, "");
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
}

static int test_cli_argparse_help_options(int seed, int rep) {
    struct rig r;
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    char out[1024];

    CHECK(rig_open(&r, NULL, PF_COLOR_NEVER) == 0);
    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    pf_argparser_set_cli(&p, &r.cli);

    pf_argparser_help_options(&p, PF_COMMON_HELP | PF_COMMON_VERBOSE);
    rig_out(&r, out, sizeof out);
    CHECK(strstr(out, "-?, --help") != NULL);
    CHECK(strstr(out, "-v, --verbose") != NULL);
    CHECK(strstr(out, "--quiet") == NULL);

    pf_argparser_help_options(&p, PF_COMMON_ALL);
    rig_out(&r, out, sizeof out);
    CHECK(strstr(out, "-V, --version") != NULL);
    CHECK(strstr(out, "-q, --quiet") != NULL);
    CHECK(strstr(out, "--color[=WHEN]") != NULL);
    CHECK(strstr(out, "-i, --interactive[=WHEN]") != NULL);
    CHECK(strstr(out, "-c, --color") == NULL); /* long form only */

    pf_argparser_help_options(&p, 0);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");

    pf_argparser_help_options(NULL, PF_COMMON_ALL);
    pf_argparser_free(&p);
    rig_close(&r);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Pseudo terminals, for what only a terminal gets to see            */
/* ---------------------------------------------------------------- */

#ifndef _WIN32

struct pty {
    int master;
    FILE *slave;
};

static int pty_open(struct pty *t) {
    char *name;
    int slave_fd;
    struct termios tio;

    t->master = posix_openpt(O_RDWR | O_NOCTTY);
    if (t->master < 0)
        return -1;
    if (grantpt(t->master) != 0 || unlockpt(t->master) != 0)
        return -1;
    name = ptsname(t->master);
    if (!name)
        return -1;
    slave_fd = open(name, O_RDWR | O_NOCTTY);
    if (slave_fd < 0)
        return -1;

    /* Raw mode: no translation of the output. */
    if (tcgetattr(slave_fd, &tio) == 0) {
        tio.c_oflag &= ~(tcflag_t)OPOST;
        tio.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
        tcsetattr(slave_fd, TCSANOW, &tio);
    }

    t->slave = fdopen(slave_fd, "w+");
    if (!t->slave)
        return -1;
    setvbuf(t->slave, NULL, _IOFBF, 8192);
    return 0;
}

static void pty_close(struct pty *t) {
    fclose(t->slave);
    close(t->master);
}

/* Everything written so far. */
static void pty_read(struct pty *t, char *buf, size_t size) {
    size_t used = 0;
    struct pollfd pfd;

    fflush(t->slave);
    pfd.fd = t->master;
    pfd.events = POLLIN;
    while (used + 1 < size && poll(&pfd, 1, 50) > 0) {
        ssize_t n = read(t->master, buf + used, size - 1 - used);
        if (n <= 0)
            break;
        used += (size_t)n;
    }
    buf[used] = '\0';
}

static int cli_on_pty(struct pty *t, pf_cli_t *c, enum pf_color_mode_t color) {
    struct pf_cli_opt o;

    rig_env();
    if (test_setenv("COLUMNS", "60") != 0)
        return -1;
    if (pty_open(t) != 0)
        return -1;
    memset(&o, 0, sizeof o);
    o.out = o.err = o.in = t->slave;
    o.colorMode = color;
    pf_cli_init(c, &o);
    return 0;
}

    #define NEED_PTY(t, c, color)                              \
        if (cli_on_pty(&(t), &(c), (color)) != 0)              \
        return SKIP("pseudo terminals are not available here")

static int test_cli_pty_detection(int seed, int rep) {
    struct pty t;
    pf_cli_t c;

    NEED_PTY(t, c, PF_COLOR_AUTO);

    CHECK(c.out.isTerminal && c.err.isTerminal && c.in.isTerminal);
    CHECK(c.out.supportsAnsi && c.out.supportsCursor);
    CHECK(c.out.colorEnabled);
    CHECK(c.out.supportsHyperlinks);
    CHECK(c.out.columns == 60);
    CHECK(c.isInteractive);

    /* NO_COLOR is respected on terminals. */
    CHECK(test_setenv("NO_COLOR", "1") == 0);
    pf_cli_set_color_mode(&c, PF_COLOR_AUTO);
    CHECK(!c.out.colorEnabled && !c.out.supportsHyperlinks);
    CHECK(c.out.supportsAnsi); /* cursor control is independent of color */
    pf_cli_set_color_mode(&c, PF_COLOR_ALWAYS);
    CHECK(c.out.colorEnabled);
    CHECK(test_setenv("NO_COLOR", NULL) == 0);

    /* A dumb terminal gets no escape sequences at all. */
    CHECK(test_setenv("TERM", "dumb") == 0);
    pf_cli_init(
        &c,
        &(struct pf_cli_opt) { .out = t.slave, .err = t.slave, .in = t.slave }
    );
    CHECK(c.out.isTerminal && !c.out.supportsAnsi && !c.out.supportsCursor);
    CHECK(!c.out.colorEnabled);

    pty_close(&t);
    return 0;
}

static int test_cli_pty_cursor(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    char out[128];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    pf_cli_clearline(&c, PF_CLEAR_END);
    pf_cli_clearline(&c, PF_CLEAR_START);
    pf_cli_clearline(&c, PF_CLEAR_ALL);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[0K\r\033[1K\r\033[2K");

    pf_cli_clear(&c, PF_CLEAR_END);
    pf_cli_clear(&c, PF_CLEAR_ALL);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[0J\r\033[2J");

    pf_cli_move(&c, 3, 0);
    pf_cli_move(&c, -2, 0);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\033[3C\033[2D");

    pf_cli_move(&c, 0, 4);
    pf_cli_move(&c, 0, -1);
    pf_cli_move(&c, 0, 0);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\033[4A\033[1B");

    pf_cli_position(&c, 0, 0);
    pf_cli_position(&c, 9, 4);
    pf_cli_position(&c, -1, 4); /* off-screen is refused */
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\033[1;1H\033[5;10H");

    pty_close(&t);
    return 0;
}

static int test_cli_pty_cursor_redirected(int seed, int rep) {
    struct rig r;
    char out[64];

    /* The same calls print nothing to files. */
    CHECK(rig_open(&r, NULL, PF_COLOR_ALWAYS) == 0);
    pf_cli_clearline(&r.cli, PF_CLEAR_ALL);
    pf_cli_clear(&r.cli, PF_CLEAR_ALL);
    pf_cli_move(&r.cli, 2, 2);
    pf_cli_position(&r.cli, 1, 1);
    rig_out(&r, out, sizeof out);
    CHECK_STR(out, "");
    rig_close(&r);
    return 0;
}

static int test_cli_pty_links(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    char out[256];

    NEED_PTY(t, c, PF_COLOR_AUTO);

    pf_cli_link(&c, "https://example.com", "example");
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\033]8;;https://example.com\033\\example\033]8;;\033\\");

    pf_cli_path_link(&c, "/tmp/a.c", 7);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\033]8;;file:///tmp/a.c#L7\033\\/tmp/a.c\033]8;;\033\\");

    pf_cli_path_link(&c, "/tmp/a.c", 0);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\033]8;;file:///tmp/a.c\033\\/tmp/a.c\033]8;;\033\\");

    /* Can be turned off. */
    CHECK(test_setenv("NO_HYPERLINKS", "1") == 0);
    pf_cli_set_color_mode(&c, PF_COLOR_AUTO);
    pf_cli_link(&c, "https://example.com", "example");
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "example");
    CHECK(test_setenv("NO_HYPERLINKS", NULL) == 0);

    pty_close(&t);
    return 0;
}

static int test_cli_pty_spinner(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    pf_cli_spinner_t s;
    struct pf_cli_spinner_opt o;
    char out[512];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    memset(&o, 0, sizeof o);
    o.title = "Loading";
    CHECK(pf_cli_spinner_start(&s, &c, &o) == 0);

    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s); /* wraps around */
    pty_read(&t, out, sizeof out);
    CHECK_STR(
        out,
        "\r\033[2K- Loading"
        "\r\033[2K\\ Loading"
        "\r\033[2K| Loading"
        "\r\033[2K/ Loading"
        "\r\033[2K- Loading"
    );

    pf_cli_spinner_set_title(&s, "Almost");
    pf_cli_spinner_tick(&s);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2K\\ Almost");

    pf_cli_spinner_finish(&s, PF_TRUE, "Complete");
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2K[ok] Complete\n");

    /* Stopped spinners are quiet. */
    pf_cli_spinner_tick(&s);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "");

    pty_close(&t);
    return 0;
}

static int test_cli_pty_spinner_custom(int seed, int rep) {
    static const char *const frames[] = { "a", "b", NULL };
    struct pty t;
    pf_cli_t c;
    pf_cli_spinner_t s;
    struct pf_cli_spinner_opt o;
    char out[256];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    memset(&o, 0, sizeof o);
    o.frames = frames;
    CHECK(pf_cli_spinner_start(&s, &c, &o) == 0);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s);
    pf_cli_spinner_tick(&s);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2Ka\r\033[2Kb\r\033[2Ka");
    pf_cli_spinner_stop(&s, NULL);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2K");

    pty_close(&t);
    return 0;
}

static int test_cli_pty_spinner_long_title(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    pf_cli_spinner_t s;
    struct pf_cli_spinner_opt o;
    char out[512], title[200];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    memset(title, 'T', sizeof title - 1);
    title[sizeof title - 1] = '\0';
    memset(&o, 0, sizeof o);
    o.title = title;
    CHECK(pf_cli_spinner_start(&s, &c, &o) == 0);
    pf_cli_spinner_tick(&s);
    pty_read(&t, out, sizeof out);

    /* The line never wraps: frame, a space and the title, within 60. */
    CHECK(pf_cli_text_width(out) <= 60);
    CHECK(pf_cli_text_width(out) >= 50);

    pty_close(&t);
    return 0;
}

static int test_cli_pty_progress(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    pf_cli_progress_t p;
    struct pf_cli_progress_opt o;
    char out[512];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    memset(&o, 0, sizeof o);
    o.width = 10;
    o.title = "Copy";
    o.showPercentage = PF_TRUE;
    o.showValue = PF_TRUE;
    CHECK(pf_cli_progress_start(&p, &c, 100.0, &o) == 0);

    pf_cli_progress_update(&p, 50);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2KCopy [#####-----]  50% 50/100");

    pf_cli_progress_update(&p, 100);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2KCopy [##########] 100% 100/100");

    pf_cli_progress_update(&p, 0);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2KCopy [----------]   0% 0/100");

    pf_cli_progress_update(&p, 33);
    pty_read(&t, out, sizeof out);
    CHECK(strstr(out, "[###-------]") != NULL);
    CHECK(strstr(out, " 33%") != NULL);

    /* Completing prints the full bar and moves on to a new line. */
    pf__cli_progress_stop(&p, PF_TRUE);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2KCopy [##########] 100% 100/100\n");

    pty_close(&t);
    return 0;
}

static int test_cli_pty_progress_cancel(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    pf_cli_progress_t p;
    char out[256];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    CHECK(pf_cli_progress_start(&p, &c, 10.0, NULL) == 0);
    pf_cli_progress_update(&p, 5);
    pty_read(&t, out, sizeof out);
    CHECK(strstr(out, "#") != NULL);

    /* Cancelling erases the bar. */
    pf__cli_progress_stop(&p, PF_FALSE);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2K");
    CHECK(!p.active);

    pty_close(&t);
    return 0;
}

static int test_cli_pty_progress_colors(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    pf_cli_progress_t p;
    struct pf_cli_progress_opt o;
    char out[256];

    NEED_PTY(t, c, PF_COLOR_ALWAYS);

    memset(&o, 0, sizeof o);
    o.width = 4;
    CHECK(pf_cli_progress_start(&p, &c, 4.0, &o) == 0);
    pf_cli_progress_update(&p, 2);
    pty_read(&t, out, sizeof out);
    CHECK_STR(out, "\r\033[2K[\033[32m##--\033[0m]");

    pty_close(&t);
    return 0;
}

static int test_cli_pty_progress_custom_render(int seed, int rep) {
    static struct pf_cli_progress_frame last;
    struct pty t;
    pf_cli_t c;
    pf_cli_progress_t p;
    struct pf_cli_progress_opt o;
    char out[64];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    memset(&o, 0, sizeof o);
    o.width = 8;
    o.render = NULL;
    CHECK(pf_cli_progress_start(&p, &c, 8.0, &o) == 0);

    pf_cli_progress_update(&p, 2);
    pty_read(&t, out, sizeof out);
    CHECK(strstr(out, "[##------]") != NULL);
    (void)last;

    pty_close(&t);
    return 0;
}

static int test_cli_pty_live_text(int seed, int rep) {
    struct pty t;
    pf_cli_t c;
    pf_cli_spinner_t s;
    char out[256];

    NEED_PTY(t, c, PF_COLOR_NEVER);

    CHECK(pf_cli_spinner_start(&s, &c, NULL) == 0);
    pf_cli_spinner_tick(&s);
    pty_read(&t, out, sizeof out);

    /* Regular output goes on a clean line when nothing is animated. */
    pf_cli_printf(&c, "message\n");
    pty_read(&t, out, sizeof out);
    CHECK(strstr(out, "message\n") != NULL);

    pty_close(&t);
    return 0;
}

#endif /* !_WIN32 */

pf_test_t suite_cli[] = {
    { test_cli_init, "/cli/init" },
    { test_cli_init_defaults, "/cli/init_defaults" },
    { test_cli_init_options, "/cli/init_options" },
    { test_cli_null_streams, "/cli/null_streams" },
    { test_cli_colors_modes, "/cli/color_modes" },
    { test_cli_interactive_modes, "/cli/interactive_modes" },
    { test_cli_columns, "/cli/columns" },
    { test_cli_printf, "/cli/printf" },
    { test_cli_silent, "/cli/silent" },
    { test_cli_color_output, "/cli/color_output" },
    { test_cli_color_disabled, "/cli/color_disabled" },
    { test_cli_no_color_env, "/cli/no_color_env" },
    { test_cli_verbosef, "/cli/verbosef" },
    { test_cli_null_cli, "/cli/null_cli" },
    { test_cli_text_width, "/cli/text_width" },
    { test_cli_fill, "/cli/fill" },
    { test_cli_wrap, "/cli/wrap" },
    { test_cli_wrap_edge_cases, "/cli/wrap_edge_cases" },
    { test_cli_wrap_colors, "/cli/wrap_colors" },
    { test_cli_help_definition, "/cli/help_definition" },
    { test_cli_help_definition_wrapping, "/cli/help_definition_wrapping" },
    { test_cli_help_definition_long_name, "/cli/help_definition_long_name" },
    { test_cli_help_definition_indent, "/cli/help_definition_indent" },
    { test_cli_help_definition_narrow, "/cli/help_definition_narrow" },
    { test_cli_help_definitions, "/cli/help_definitions" },
    { test_cli_help_section, "/cli/help_section" },
    { test_cli_links, "/cli/links" },
    { test_cli_readline, "/cli/readline" },
    { test_cli_ask, "/cli/ask" },
    { test_cli_confirm, "/cli/confirm" },
    { test_cli_confirm_never, "/cli/confirm_never" },
    { test_cli_select, "/cli/select" },
    { test_cli_spinner_redirected, "/cli/spinner_redirected" },
    { test_cli_spinner_finish_variants, "/cli/spinner_finish_variants" },
    { test_cli_spinner_colored_marker, "/cli/spinner_colored_marker" },
    { test_cli_spinner_silent, "/cli/spinner_silent" },
    { test_cli_spinner_invalid, "/cli/spinner_invalid" },
    { test_cli_spinner_frames, "/cli/spinner_frames" },
    { test_cli_progress_redirected, "/cli/progress_redirected" },
    { test_cli_progress_options, "/cli/progress_options" },
    { test_cli_now, "/cli/now" },
    { test_cli_argparse_binding, "/cli/argparse_binding" },
    { test_cli_argparse_options_apply, "/cli/argparse_options_apply" },
    { test_cli_argparse_quiet, "/cli/argparse_quiet" },
    { test_cli_argparse_errors_through_cli, "/cli/argparse_errors" },
    { test_cli_argparse_quiet_errors, "/cli/argparse_quiet_errors" },
    { test_cli_argparse_help_options, "/cli/argparse_help_options" },
#ifndef _WIN32
    { test_cli_pty_detection, "/cli/pty_detection" },
    { test_cli_pty_cursor, "/cli/pty_cursor" },
    { test_cli_pty_cursor_redirected, "/cli/pty_cursor_redirected" },
    { test_cli_pty_links, "/cli/pty_links" },
    { test_cli_pty_spinner, "/cli/pty_spinner" },
    { test_cli_pty_spinner_custom, "/cli/pty_spinner_custom" },
    { test_cli_pty_spinner_long_title, "/cli/pty_spinner_long_title" },
    { test_cli_pty_progress, "/cli/pty_progress" },
    { test_cli_pty_progress_cancel, "/cli/pty_progress_cancel" },
    { test_cli_pty_progress_colors, "/cli/pty_progress_colors" },
    { test_cli_pty_progress_custom_render, "/cli/pty_progress_render" },
    { test_cli_pty_live_text, "/cli/pty_live_text" },
#endif
    { 0 },
};
