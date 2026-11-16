/** cpolyfill - Filling the gaps between C standards and compilers

    Building command line interfaces: styled output that adapts to the
    terminal (or file, or pipe), help text layout, links, prompts, and
    spinners and progress bars that redraw a line in place. One thread
    owns a pf_cli_t.

        pf_cli_t cli;
        pf_cli_init(&cli, NULL);              // stdin, stdout, stderr
        pf_cli_cprintf(&cli, PF_FG_GREEN | PF_CLI_BOLD, "done\n");

    Setup:

        int pf_cli_init(pf_cli_t *c, const pf_cli_opts_t *o)
                `o` may be NULL. Detects each stream (pf_term_is_terminal,
                pf_term_colors, pf_term_size; COLUMNS overrides the width).
                On Windows a console output is switched to UTF-8
                (pf_term_set_utf8). 0 or PF_EINVAL.
        pf_cli_opts_t           in, out, err  NULL: stdin, stdout, stderr
                                null_streams  keep NULL streams NULL (no
                                              output, no input)
                                color_mode, interactive_mode  (term.h)
                                definition_indent  help indentation
                                              (default 10)
        void pf_cli_free(pf_cli_t *c)
        void pf_cli_set_color_mode(pf_cli_t *c, pf_color_mode_t mode)
        void pf_cli_set_interactive_mode(pf_cli_t *c,
                                         pf_interactive_mode_t mode)

        Fields: in, out, err (pf_cli_stream_t: file, is_terminal, cursor
        (escape sequences work), hyperlinks, colors (0, 16, 256 or
        16777216), columns), interactive (prompts are shown: ALWAYS, or
        AUTO with in and out terminals), silent (no output), verbosity.

    Styles (pf_cli_style_t, OR-ed together; 0 is none):

        PF_CLI_RESET, PF_CLI_BOLD, PF_CLI_DIM, PF_CLI_ITALIC,
        PF_CLI_UNDERLINE
        PF_FG_BLACK ... PF_FG_WHITE, PF_FG_BRIGHT_BLACK ... _BRIGHT_WHITE
        PF_BG_BLACK ... PF_BG_WHITE, PF_BG_BRIGHT_BLACK ... _BRIGHT_WHITE
        PF_FG_256(n), PF_BG_256(n)              the 256-colour palette
        PF_FG_RGB(r, g, b), PF_BG_RGB(r, g, b)  24-bit colour
                A colour the stream cannot show is turned into the
                nearest one it can (RGB to 256 to 16); with no colours
                nothing is written.

    Output (nothing when silent; an animated spinner or progress line is
    cleared first):

        void pf_cli_printf(c, fmt, ...)         to out
        void pf_cli_errorf(c, fmt, ...)         to err
        void pf_cli_cprintf(c, style, fmt, ...) to out, styled
        void pf_cli_ecprintf(c, style, fmt, ...) to err, styled
        void pf_cli_verbosef(c, int level, fmt, ...)
                To out when c->verbosity >= level.
        void pf_cli_vcprintf(c, pf_cli_stream_t *s, style, fmt, va_list)
        void pf_cli_color(c, style)             starts a style on out
        void pf_cli_clear_line(c, pf_cli_clear_t mode)
        void pf_cli_clear(c, pf_cli_clear_t mode)
                PF_CLEAR_END, PF_CLEAR_START, PF_CLEAR_ALL (of the line /
                screen).
        void pf_cli_move(c, int dx, int dy)
        void pf_cli_position(c, int x, int y)   zero-based
        void pf_cli_link(c, const char *url, const char *text)
        void pf_cli_path_link(c, const char *path, unsigned line)
                Hyperlinks where the terminal has them (not with
                NO_HYPERLINKS), else just the text.

    Text:

        size_t pf_cli_text_width(const char *s)
                Terminal columns: escape sequences take none, common
                East Asian and emoji blocks two, combining marks and
                zero-width characters none, other characters one. A simple
                approximation, not the Unicode width tables.
        void pf_cli_fill(FILE *f, unsigned n, char ch)
        void pf_cli_wrap(c, unsigned first_indent, unsigned next_indent,
                         unsigned width, const char *text)
                Word-wraps `text` to `width` columns (0: the terminal's).
        void pf_cli_help_section(c, const char *title)
        void pf_cli_help_definition(c, const char *name,
                                    const char *description)
        void pf_cli_help_definitions(c, const pf_cli_definition_t *defs)
                { name, description } pairs, ended by a NULL name.

    Prompts (only when c->interactive; otherwise nothing is printed or
    read):

        int pf_cli_readline(c, char *buf, size_t size)
                A line from `in`, without its newline: 0, PF_ERANGE (cut
                at a character boundary, the rest of the line discarded),
                PF_ENODATA (end of input) or PF_EIO.
        int pf_cli_ask(c, const char *question, char *buf, size_t size)
                As pf_cli_readline after printing "question: ";
                PF_EAGAIN when not interactive.
        bool pf_cli_confirm(c, const char *question, bool def)
                "question [Y/n]: "; `def` when not interactive, at the end
                of input or for an empty answer.
        int pf_cli_select(c, const char *question,
                          const char *const *choices, size_t count,
                          size_t def)
                A numbered list; the answer is a number or a choice. The
                index, `def` when not interactive or for an empty answer,
                or PF_EINVAL / PF_ENODATA.

    Spinner and progress bar (drawn only where the cursor can be moved,
    and at most every `interval_ms`, default 80):

        int pf_cli_spinner_start(pf_cli_spinner_t *s, pf_cli_t *c,
                                 const pf_cli_spinner_opts_t *o)
                o: title, frames (NULL-terminated; NULL:
                pf_cli_spinner_frames(PF_CLI_SPINNER_LINE)), render
                (custom drawing), interval_ms.
        void pf_cli_spinner_set_title(s, fmt, ...)
        void pf_cli_spinner_tick(s)             call it often
        void pf_cli_spinner_stop(s, const char *message)
        void pf_cli_spinner_finish(s, bool ok, fmt, ...)
                Ends with "[ok] message" / "[fail] message".
        const char *const *pf_cli_spinner_frames(int kind)
                PF_CLI_SPINNER_LINE (- \ | /), PF_CLI_SPINNER_DOTS
                (braille).

        int pf_cli_progress_start(pf_cli_progress_t *p, pf_cli_t *c,
                                  double maximum,
                                  const pf_cli_progress_opts_t *o)
                o: title, width (default a quarter of the terminal), fill,
                empty ('#', '-'), style (PF_FG_GREEN), show_percentage,
                show_value, show_rate, show_eta, show_elapsed, render,
                interval_ms.
        void pf_cli_progress_set_title(p, fmt, ...)
        void pf_cli_progress_update(p, double value)
        void pf_cli_progress_advance(p, double delta)
        void pf_cli_progress_render(p)          now, whatever the interval
        void pf_cli_progress_finish(p)          full, then a newline
        void pf_cli_progress_stop(p)            cancelled: line cleared

    With <cpolyfill/argparse.h> (when the library has it):

        void pf_cli_bind_args(pf_cli_t *c, pf_argparser_t *p)
                The parser writes to the CLI's streams and reports errors
                through it (err_prefix in bold red), applying --color and
                --quiet given so far before each one.
        void pf_cli_apply_args(pf_cli_t *c, const pf_argparser_t *p)
                After the run: verbosity, --quiet, --color and
                --interactive into the CLI.
        void pf_cli_help_options(pf_cli_t *c, const pf_argparser_t *p,
                                 unsigned mask)
                The common options (PF_COMMON_*) as help definitions.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_CLI_H
#define CPOLYFILL_CLI_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/term.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

/* Bits 0-23 the foreground value, 24-25 its kind (1 basic 0-15, 2 the
   256 palette, 3 RGB); 26-30 attributes; 32-57 the background, likewise. */
typedef uint64_t pf_cli_style_t;

#define PF__CLI_COLOR(kind, v)                                           \
    (((pf_cli_style_t)(kind) << 24) | ((pf_cli_style_t)(v) & 0xFFFFFFu))
#define PF__CLI_BG(fg) ((pf_cli_style_t)(fg) << 32)
#define PF__CLI_RGB(r, g, b)                                               \
    ((((unsigned long)(r) & 0xFFu) << 16)                                  \
     | (((unsigned long)(g) & 0xFFu) << 8) | ((unsigned long)(b) & 0xFFu))

#define PF_CLI_RESET ((pf_cli_style_t)1 << 26)
#define PF_CLI_BOLD ((pf_cli_style_t)1 << 27)
#define PF_CLI_DIM ((pf_cli_style_t)1 << 28)
#define PF_CLI_ITALIC ((pf_cli_style_t)1 << 29)
#define PF_CLI_UNDERLINE ((pf_cli_style_t)1 << 30)

#define PF_FG_BLACK PF__CLI_COLOR(1, 0)
#define PF_FG_RED PF__CLI_COLOR(1, 1)
#define PF_FG_GREEN PF__CLI_COLOR(1, 2)
#define PF_FG_YELLOW PF__CLI_COLOR(1, 3)
#define PF_FG_BLUE PF__CLI_COLOR(1, 4)
#define PF_FG_MAGENTA PF__CLI_COLOR(1, 5)
#define PF_FG_CYAN PF__CLI_COLOR(1, 6)
#define PF_FG_WHITE PF__CLI_COLOR(1, 7)
#define PF_FG_BRIGHT_BLACK PF__CLI_COLOR(1, 8)
#define PF_FG_BRIGHT_RED PF__CLI_COLOR(1, 9)
#define PF_FG_BRIGHT_GREEN PF__CLI_COLOR(1, 10)
#define PF_FG_BRIGHT_YELLOW PF__CLI_COLOR(1, 11)
#define PF_FG_BRIGHT_BLUE PF__CLI_COLOR(1, 12)
#define PF_FG_BRIGHT_MAGENTA PF__CLI_COLOR(1, 13)
#define PF_FG_BRIGHT_CYAN PF__CLI_COLOR(1, 14)
#define PF_FG_BRIGHT_WHITE PF__CLI_COLOR(1, 15)
#define PF_FG_256(n) PF__CLI_COLOR(2, (unsigned)(n) & 0xFFu)
#define PF_FG_RGB(r, g, b) PF__CLI_COLOR(3, PF__CLI_RGB(r, g, b))

#define PF_BG_BLACK PF__CLI_BG(PF_FG_BLACK)
#define PF_BG_RED PF__CLI_BG(PF_FG_RED)
#define PF_BG_GREEN PF__CLI_BG(PF_FG_GREEN)
#define PF_BG_YELLOW PF__CLI_BG(PF_FG_YELLOW)
#define PF_BG_BLUE PF__CLI_BG(PF_FG_BLUE)
#define PF_BG_MAGENTA PF__CLI_BG(PF_FG_MAGENTA)
#define PF_BG_CYAN PF__CLI_BG(PF_FG_CYAN)
#define PF_BG_WHITE PF__CLI_BG(PF_FG_WHITE)
#define PF_BG_BRIGHT_BLACK PF__CLI_BG(PF_FG_BRIGHT_BLACK)
#define PF_BG_BRIGHT_RED PF__CLI_BG(PF_FG_BRIGHT_RED)
#define PF_BG_BRIGHT_GREEN PF__CLI_BG(PF_FG_BRIGHT_GREEN)
#define PF_BG_BRIGHT_YELLOW PF__CLI_BG(PF_FG_BRIGHT_YELLOW)
#define PF_BG_BRIGHT_BLUE PF__CLI_BG(PF_FG_BRIGHT_BLUE)
#define PF_BG_BRIGHT_MAGENTA PF__CLI_BG(PF_FG_BRIGHT_MAGENTA)
#define PF_BG_BRIGHT_CYAN PF__CLI_BG(PF_FG_BRIGHT_CYAN)
#define PF_BG_BRIGHT_WHITE PF__CLI_BG(PF_FG_BRIGHT_WHITE)
#define PF_BG_256(n) PF__CLI_BG(PF_FG_256(n))
#define PF_BG_RGB(r, g, b) PF__CLI_BG(PF_FG_RGB(r, g, b))

#define PF_CLI_SPINNER_LINE 0
#define PF_CLI_SPINNER_DOTS 1

PF_BEGIN_DECLS

typedef enum pf_cli_clear_t {
    PF_CLEAR_END,
    PF_CLEAR_START,
    PF_CLEAR_ALL
} pf_cli_clear_t;

typedef struct pf_cli_stream_t {
    FILE *file;
    bool is_terminal;
    bool cursor;
    bool hyperlinks;
    int colors;
    int columns;
} pf_cli_stream_t;

typedef struct pf_cli_opts_t {
    FILE *in;
    FILE *out;
    FILE *err;
    bool null_streams;
    pf_color_mode_t color_mode;
    pf_interactive_mode_t interactive_mode;
    int definition_indent;
} pf_cli_opts_t;

typedef struct pf_cli_t {
    pf_cli_stream_t in;
    pf_cli_stream_t out;
    pf_cli_stream_t err;
    pf_color_mode_t color_mode;
    pf_interactive_mode_t interactive_mode;
    bool interactive;
    bool silent;
    int verbosity;
    int definition_indent;
    /* Private. */
    void *pf__live;
} pf_cli_t;

typedef struct pf_cli_definition_t {
    const char *name;
    const char *description;
} pf_cli_definition_t;

typedef struct pf_cli_spinner_t pf_cli_spinner_t;
typedef struct pf_cli_progress_t pf_cli_progress_t;

typedef struct pf_cli_spinner_opts_t {
    const char *title;
    const char *const *frames;
    void (*render)(pf_cli_spinner_t *s);
    unsigned interval_ms;
} pf_cli_spinner_opts_t;

struct pf_cli_spinner_t {
    pf_cli_t *cli;
    pf_cli_spinner_opts_t opts;
    size_t frame;
    size_t frame_count;
    bool active;
    /* Private. */
    uint64_t pf__started;
    uint64_t pf__last_draw;
    bool pf__drawn;
    char pf__title[160];
};

typedef struct pf_cli_progress_frame_t {
    unsigned width;
    unsigned filled;
    double ratio;
    double elapsed;
    double rate;
    double eta;
} pf_cli_progress_frame_t;

typedef struct pf_cli_progress_opts_t {
    const char *title;
    unsigned width;
    char fill;
    char empty;
    pf_cli_style_t style;
    bool show_percentage;
    bool show_value;
    bool show_rate;
    bool show_eta;
    bool show_elapsed;
    void (*render)(pf_cli_progress_t *p, const pf_cli_progress_frame_t *f);
    unsigned interval_ms;
} pf_cli_progress_opts_t;

struct pf_cli_progress_t {
    pf_cli_t *cli;
    pf_cli_progress_opts_t opts;
    double value;
    double maximum;
    bool active;
    /* Private. */
    uint64_t pf__started;
    uint64_t pf__last_draw;
    bool pf__drawn;
    char pf__title[160];
};

PF_API int pf_cli_init(pf_cli_t *c, const pf_cli_opts_t *o);
PF_API void pf_cli_free(pf_cli_t *c);
PF_API void pf_cli_set_color_mode(pf_cli_t *c, pf_color_mode_t mode);
PF_API void pf_cli_set_interactive_mode(
    pf_cli_t *c, pf_interactive_mode_t mode
);

PF_PRINTF(2, 3) PF_API void pf_cli_printf(pf_cli_t *c, const char *fmt, ...);
PF_PRINTF(2, 3) PF_API void pf_cli_errorf(pf_cli_t *c, const char *fmt, ...);
PF_PRINTF(3, 4)
PF_API void pf_cli_cprintf(
    pf_cli_t *c, pf_cli_style_t style, const char *fmt, ...
);
PF_PRINTF(3, 4)
PF_API void pf_cli_ecprintf(
    pf_cli_t *c, pf_cli_style_t style, const char *fmt, ...
);
PF_PRINTF(3, 4)
PF_API void pf_cli_verbosef(pf_cli_t *c, int level, const char *fmt, ...);
PF_PRINTF(4, 0)
PF_API void pf_cli_vcprintf(
    pf_cli_t *c,
    pf_cli_stream_t *s,
    pf_cli_style_t style,
    const char *fmt,
    va_list ap
);
PF_API void pf_cli_color(pf_cli_t *c, pf_cli_style_t style);
PF_API void pf_cli_clear_line(pf_cli_t *c, pf_cli_clear_t mode);
PF_API void pf_cli_clear(pf_cli_t *c, pf_cli_clear_t mode);
PF_API void pf_cli_move(pf_cli_t *c, int dx, int dy);
PF_API void pf_cli_position(pf_cli_t *c, int x, int y);
PF_API void pf_cli_link(pf_cli_t *c, const char *url, const char *text);
PF_API void pf_cli_path_link(pf_cli_t *c, const char *path, unsigned line);

PF_API size_t pf_cli_text_width(const char *s);
PF_API void pf_cli_fill(FILE *f, unsigned n, char ch);
PF_API void pf_cli_wrap(
    pf_cli_t *c,
    unsigned first_indent,
    unsigned next_indent,
    unsigned width,
    const char *text
);
PF_API void pf_cli_help_section(pf_cli_t *c, const char *title);
PF_API void pf_cli_help_definition(
    pf_cli_t *c, const char *name, const char *description
);
PF_API void pf_cli_help_definitions(
    pf_cli_t *c, const pf_cli_definition_t *defs
);

PF_API int pf_cli_readline(pf_cli_t *c, char *buf, size_t size);
PF_API int pf_cli_ask(
    pf_cli_t *c, const char *question, char *buf, size_t size
);
PF_API bool pf_cli_confirm(pf_cli_t *c, const char *question, bool def);
PF_API int pf_cli_select(
    pf_cli_t *c,
    const char *question,
    const char *const *choices,
    size_t count,
    size_t def
);

PF_API const char *const *pf_cli_spinner_frames(int kind);
PF_API int pf_cli_spinner_start(
    pf_cli_spinner_t *s, pf_cli_t *c, const pf_cli_spinner_opts_t *o
);
PF_PRINTF(2, 3)
PF_API void pf_cli_spinner_set_title(pf_cli_spinner_t *s, const char *fmt, ...);
PF_API void pf_cli_spinner_tick(pf_cli_spinner_t *s);
PF_API void pf_cli_spinner_stop(pf_cli_spinner_t *s, const char *message);
PF_PRINTF(3, 4)
PF_API void pf_cli_spinner_finish(
    pf_cli_spinner_t *s, bool ok, const char *fmt, ...
);

PF_API int pf_cli_progress_start(
    pf_cli_progress_t *p,
    pf_cli_t *c,
    double maximum,
    const pf_cli_progress_opts_t *o
);
PF_PRINTF(2, 3)
PF_API void pf_cli_progress_set_title(
    pf_cli_progress_t *p, const char *fmt, ...
);
PF_API void pf_cli_progress_update(pf_cli_progress_t *p, double value);
PF_API void pf_cli_progress_advance(pf_cli_progress_t *p, double delta);
PF_API void pf_cli_progress_render(pf_cli_progress_t *p);
PF_API void pf_cli_progress_finish(pf_cli_progress_t *p);
PF_API void pf_cli_progress_stop(pf_cli_progress_t *p);

struct pf_argparser_t;
PF_API void pf_cli_bind_args(pf_cli_t *c, struct pf_argparser_t *p);
PF_API void pf_cli_apply_args(pf_cli_t *c, const struct pf_argparser_t *p);
PF_API void pf_cli_help_options(
    pf_cli_t *c, const struct pf_argparser_t *p, unsigned mask
);

PF_END_DECLS

#endif /* CPOLYFILL_CLI_H */
