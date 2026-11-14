/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/cli.h>: terminal facts come from <cpolyfill/term.h>; this
    file decides what to write.
*/

#include "internal.h"

#include <cpolyfill/cli.h>
#include <cpolyfill/term.h>
#include <cpolyfill/time.h>

#if PF__HAVE_ARGPARSE
    #include <cpolyfill/argparse.h>
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <io.h>
    #define PF__FILENO _fileno
#else
    #define PF__FILENO fileno
#endif

#define PF__CLI_INTERVAL_MS 80u

/* ---- Streams ----------------------------------------------------------- */

static int pf__cli_fd(FILE *f) { return f ? PF__FILENO(f) : -1; }

static int pf__cli_columns(FILE *f) {
    const char *env = getenv("COLUMNS");
    int cols = 0;

    if (env && *env) {
        char *end;
        unsigned long n = strtoul(env, &end, 10);
        if (end != env && !*end && n >= 20 && n <= 1000)
            return (int)n;
    }
    if (f && pf_term_size(pf__cli_fd(f), &cols, NULL) == PF_OK && cols >= 20)
        return cols;
    return 80;
}

/* Colours for the mode: the terminal's, or with ALWAYS at least 256 (24-bit
   where COLORTERM says so) even for a file or pipe. */
static int pf__cli_colors(const pf_cli_t *c, FILE *f) {
    int n;
    if (!f || c->color_mode == PF_COLOR_NEVER)
        return 0;
    n = pf_term_colors(pf__cli_fd(f));
    if (c->color_mode == PF_COLOR_ALWAYS && n == 0) {
        const char *ct = getenv("COLORTERM");
        n = ct && (!strcmp(ct, "truecolor") || !strcmp(ct, "24bit")) ? 16777216
                                                                     : 256;
    }
    return n;
}

static void pf__cli_stream_colors(pf_cli_t *c, pf_cli_stream_t *s) {
    s->colors = pf__cli_colors(c, s->file);
    s->hyperlinks = s->cursor && s->colors > 0 && !getenv("NO_HYPERLINKS");
}

static void pf__cli_stream(pf_cli_t *c, pf_cli_stream_t *s, FILE *f) {
    memset(s, 0, sizeof(*s));
    s->file = f;
    s->columns = 80;
    if (!f)
        return;
    s->is_terminal = pf_term_is_terminal(pf__cli_fd(f));
    s->columns = pf__cli_columns(f);
    if (s->is_terminal) {
        const char *term = getenv("TERM");
        /* Windows consoles need VT processing turned on. */
        s->cursor = pf_term_enable_vt(pf__cli_fd(f)) == PF_OK
            && !(term && strcmp(term, "dumb") == 0);
    }
    pf__cli_stream_colors(c, s);
}

static void pf__cli_update_interactive(pf_cli_t *c) {
    if (c->interactive_mode == PF_INTERACTIVE_ALWAYS)
        c->interactive = true;
    else if (c->interactive_mode == PF_INTERACTIVE_NEVER)
        c->interactive = false;
    else
        c->interactive = c->in.is_terminal && c->out.is_terminal;
}

PF_API int pf_cli_init(pf_cli_t *c, const pf_cli_opts_t *o) {
    pf_cli_opts_t none;

    if (!c)
        return PF_EINVAL;
    if (!o) {
        memset(&none, 0, sizeof(none));
        o = &none;
    }
    memset(c, 0, sizeof(*c));
    c->color_mode = o->color_mode;
    c->interactive_mode = o->interactive_mode;
    c->definition_indent = o->definition_indent > 0
            && o->definition_indent <= 80
        ? o->definition_indent
        : 10;
    pf__cli_stream(c, &c->in, o->in || o->null_streams ? o->in : stdin);
    pf__cli_stream(c, &c->out, o->out || o->null_streams ? o->out : stdout);
    pf__cli_stream(c, &c->err, o->err || o->null_streams ? o->err : stderr);
    c->silent = !c->out.file;
    pf__cli_update_interactive(c);
#if PF_OS_WINDOWS
    if (c->out.is_terminal || c->err.is_terminal)
        (void)pf_term_set_utf8();
#endif
    return PF_OK;
}

PF_API void pf_cli_free(pf_cli_t *c) {
    if (c)
        c->pf__live = NULL;
}

PF_API void pf_cli_set_color_mode(pf_cli_t *c, pf_color_mode_t mode) {
    if (!c)
        return;
    c->color_mode = mode;
    pf__cli_stream_colors(c, &c->out);
    pf__cli_stream_colors(c, &c->err);
}

PF_API void pf_cli_set_interactive_mode(
    pf_cli_t *c, pf_interactive_mode_t mode
) {
    if (!c)
        return;
    c->interactive_mode = mode;
    pf__cli_update_interactive(c);
}

/* ---- Styles ------------------------------------------------------------ */

/* The nearest of the 16 colours: a bit per channel above the middle,
   bright when the strongest channel is high. */
static unsigned pf__cli_rgb_to_16(unsigned long rgb) {
    unsigned r = (unsigned)(rgb >> 16) & 0xFFu,
             g = (unsigned)(rgb >> 8) & 0xFFu;
    unsigned b = (unsigned)rgb & 0xFFu, max = r > g ? r : g;
    unsigned idx = (r >= 128 ? 1u : 0u) | (g >= 128 ? 2u : 0u)
        | (b >= 128 ? 4u : 0u);
    if (b > max)
        max = b;
    if (idx == 0 && max >= 64)
        return 8; /* dark grey */
    return idx + (max >= 192 && idx != 0 ? 8u : 0u);
}

static unsigned pf__cli_cube(unsigned v) {
    return v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40;
}

static unsigned pf__cli_rgb_to_256(unsigned long rgb) {
    return 16 + 36 * pf__cli_cube((unsigned)(rgb >> 16) & 0xFFu)
        + 6 * pf__cli_cube((unsigned)(rgb >> 8) & 0xFFu)
        + pf__cli_cube((unsigned)rgb & 0xFFu);
}

static unsigned long pf__cli_256_to_rgb(unsigned n) {
    static const unsigned char levels[6] = { 0, 95, 135, 175, 215, 255 };
    static const unsigned long basic[16] = {
        0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
        0x008080, 0xC0C0C0, 0x808080, 0xFF0000, 0x00FF00, 0xFFFF00,
        0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
    };
    if (n < 16)
        return basic[n];
    if (n >= 232) {
        unsigned long v = 8 + 10 * (n - 232);
        return v << 16 | v << 8 | v;
    }
    n -= 16;
    return (unsigned long)levels[n / 36] << 16
        | (unsigned long)levels[n / 6 % 6] << 8 | levels[n % 6];
}

/* Appends the SGR parameters of one colour (24 bits of value, 2 of kind),
   turned into what `colors` allows. */
static size_t pf__cli_sgr_color(
    char *buf, size_t at, unsigned long spec, int colors, bool bg
) {
    unsigned kind = (unsigned)(spec >> 24) & 3u;
    unsigned long v = spec & 0xFFFFFFul;
    int base = bg ? 40 : 30;

    if (kind == 3 && colors < 16777216) {
        if (colors >= 256) {
            kind = 2;
            v = pf__cli_rgb_to_256(v);
        } else {
            kind = 1;
            v = pf__cli_rgb_to_16(v);
        }
    }
    if (kind == 2 && colors < 256) {
        kind = 1;
        v = v < 16 ? v : pf__cli_rgb_to_16(pf__cli_256_to_rgb((unsigned)v));
    }
    switch (kind) {
    case 1:
        return at
            + (size_t)sprintf(
                   buf + at,
                   ";%d",
                   v < 8 ? base + (int)v : base + 60 + (int)v - 8
            );
    case 2:
        return at + (size_t)sprintf(buf + at, ";%d;5;%lu", base + 8, v);
    case 3:
        return at
            + (size_t)sprintf(
                   buf + at,
                   ";%d;2;%lu;%lu;%lu",
                   base + 8,
                   v >> 16,
                   (v >> 8) & 0xFFu,
                   v & 0xFFu
            );
    default:
        return at;
    }
}

static void pf__cli_style(pf_cli_stream_t *s, pf_cli_style_t style) {
    char buf[64];
    size_t n = 0;

    if (!s || !s->file || s->colors == 0 || style == 0)
        return;
    if (style & PF_CLI_RESET)
        n += (size_t)sprintf(buf + n, ";0");
    if (style & PF_CLI_BOLD)
        n += (size_t)sprintf(buf + n, ";1");
    if (style & PF_CLI_DIM)
        n += (size_t)sprintf(buf + n, ";2");
    if (style & PF_CLI_ITALIC)
        n += (size_t)sprintf(buf + n, ";3");
    if (style & PF_CLI_UNDERLINE)
        n += (size_t)sprintf(buf + n, ";4");
    n = pf__cli_sgr_color(
        buf, n, (unsigned long)(style & 0x3FFFFFFu), s->colors, false
    );
    n = pf__cli_sgr_color(
        buf, n, (unsigned long)((style >> 32) & 0x3FFFFFFu), s->colors, true
    );
    if (n > 0)
        fprintf(s->file, "\033[%sm", buf + 1);
}

PF_API void pf_cli_color(pf_cli_t *c, pf_cli_style_t style) {
    if (c && !c->silent)
        pf__cli_style(&c->out, style);
}

/* ---- Output ------------------------------------------------------------ */

/* Clears the line of an animated spinner or progress bar before text is
   written to out, or to a terminal err (sharing the screen). */
static void pf__cli_clear_live(pf_cli_t *c, const pf_cli_stream_t *s) {
    if (!c->pf__live || !c->out.file || !c->out.cursor)
        return;
    if (s == &c->out || s->is_terminal) {
        fputs("\r\033[2K", c->out.file);
        fflush(c->out.file);
    }
}

PF_API void pf_cli_vcprintf(
    pf_cli_t *c,
    pf_cli_stream_t *s,
    pf_cli_style_t style,
    const char *fmt,
    va_list ap
) {
    if (!c || !s || !s->file || !fmt || c->silent)
        return;
    pf__cli_clear_live(c, s);
    pf__cli_style(s, style);
    vfprintf(s->file, fmt, ap);
    if (style)
        pf__cli_style(s, PF_CLI_RESET);
}

#define PF__CLI_PRINT(c, s, style)                   \
    do {                                             \
        va_list ap;                                  \
        if (!(c))                                    \
            return;                                  \
        va_start(ap, fmt);                           \
        pf_cli_vcprintf((c), (s), (style), fmt, ap); \
        va_end(ap);                                  \
    } while (0)

PF_API void pf_cli_printf(pf_cli_t *c, const char *fmt, ...) {
    PF__CLI_PRINT(c, &c->out, 0);
}

PF_API void pf_cli_errorf(pf_cli_t *c, const char *fmt, ...) {
    PF__CLI_PRINT(c, &c->err, 0);
}

PF_API void pf_cli_cprintf(
    pf_cli_t *c, pf_cli_style_t style, const char *fmt, ...
) {
    PF__CLI_PRINT(c, &c->out, style);
}

PF_API void pf_cli_ecprintf(
    pf_cli_t *c, pf_cli_style_t style, const char *fmt, ...
) {
    PF__CLI_PRINT(c, &c->err, style);
}

PF_API void pf_cli_verbosef(pf_cli_t *c, int level, const char *fmt, ...) {
    if (c && c->verbosity < level)
        return;
    PF__CLI_PRINT(c, &c->out, 0);
}

static bool pf__cli_cursor(const pf_cli_t *c) {
    return c && !c->silent && c->out.file && c->out.cursor;
}

PF_API void pf_cli_clear_line(pf_cli_t *c, pf_cli_clear_t mode) {
    if (pf__cli_cursor(c) && (int)mode >= 0 && (int)mode <= 2)
        fprintf(c->out.file, "\r\033[%dK", (int)mode);
}

PF_API void pf_cli_clear(pf_cli_t *c, pf_cli_clear_t mode) {
    if (pf__cli_cursor(c) && (int)mode >= 0 && (int)mode <= 2)
        fprintf(c->out.file, "\033[%dJ", (int)mode);
}

PF_API void pf_cli_move(pf_cli_t *c, int dx, int dy) {
    if (!pf__cli_cursor(c))
        return;
    if (dx != 0)
        fprintf(
            c->out.file, "\033[%d%c", dx > 0 ? dx : -dx, dx > 0 ? 'C' : 'D'
        );
    if (dy != 0)
        fprintf(
            c->out.file, "\033[%d%c", dy > 0 ? dy : -dy, dy > 0 ? 'B' : 'A'
        );
}

PF_API void pf_cli_position(pf_cli_t *c, int x, int y) {
    if (pf__cli_cursor(c) && x >= 0 && y >= 0)
        fprintf(c->out.file, "\033[%d;%dH", y + 1, x + 1);
}

PF_API void pf_cli_link(pf_cli_t *c, const char *url, const char *text) {
    if (!c || c->silent || !c->out.file || !url || !text)
        return;
    pf__cli_clear_live(c, &c->out);
    if (c->out.hyperlinks)
        fprintf(c->out.file, "\033]8;;%s\033\\%s\033]8;;\033\\", url, text);
    else
        fputs(text, c->out.file);
}

PF_API void pf_cli_path_link(pf_cli_t *c, const char *path, unsigned line) {
    if (!c || c->silent || !c->out.file || !path)
        return;
    pf__cli_clear_live(c, &c->out);
    if (!c->out.hyperlinks) {
        fputs(path, c->out.file);
        return;
    }
    if (line > 0)
        fprintf(c->out.file, "\033]8;;file://%s#L%u\033\\", path, line);
    else
        fprintf(c->out.file, "\033]8;;file://%s\033\\", path);
    fprintf(c->out.file, "%s\033]8;;\033\\", path);
}

/* ---- Text width -------------------------------------------------------- */

/* Columns of one code point: a short list of blocks, not the Unicode
   width tables. */
static unsigned pf__cli_cp_width(long cp) {
    static const long zero[][2] = {
        { 0x0300, 0x036F }, { 0x1AB0, 0x1AFF }, { 0x1DC0, 0x1DFF },
        { 0x200B, 0x200F }, { 0x2060, 0x2064 }, { 0x20D0, 0x20FF },
        { 0xFE00, 0xFE0F }, { 0xFE20, 0xFE2F }, { 0xE0100, 0xE01EF },
    };
    static const long wide[][2] = {
        { 0x1100, 0x115F },   { 0x2E80, 0x303E },   { 0x3041, 0x33FF },
        { 0x3400, 0x4DBF },   { 0x4E00, 0x9FFF },   { 0xA000, 0xA4CF },
        { 0xAC00, 0xD7A3 },   { 0xF900, 0xFAFF },   { 0xFE30, 0xFE4F },
        { 0xFF00, 0xFF60 },   { 0xFFE0, 0xFFE6 },   { 0x1F300, 0x1F64F },
        { 0x1F900, 0x1F9FF }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD },
    };
    size_t k;

    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
        return 0;
    for (k = 0; k < sizeof(zero) / sizeof(zero[0]); k++)
        if (cp >= zero[k][0] && cp <= zero[k][1])
            return 0;
    for (k = 0; k < sizeof(wide) / sizeof(wide[0]); k++)
        if (cp >= wide[k][0] && cp <= wide[k][1])
            return 2;
    return 1;
}

/* Bytes of the escape sequence at `s` (CSI or OSC), or 0. */
static size_t pf__cli_escape(const char *s) {
    const char *p = s + 2;
    if (s[0] != '\033')
        return 0;
    if (s[1] == '[') {
        while (*p && !((unsigned char)*p >= '@' && (unsigned char)*p <= '~'))
            p++;
        return (size_t)(p - s) + (*p ? 1 : 0);
    }
    if (s[1] == ']') {
        /* ended by BEL or ESC \ */
        while (*p && *p != '\a' && !(p[0] == '\033' && p[1] == '\\'))
            p++;
        if (*p == '\a')
            return (size_t)(p - s) + 1;
        return (size_t)(p - s) + (*p ? 2 : 0);
    }
    return 0;
}

/* Columns of the first `n` bytes of `s`. */
static size_t pf__cli_width_n(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t width = 0, i = 0;

    while (i < n && p[i]) {
        size_t esc = pf__cli_escape(s + i), len;
        long cp;
        if (esc) {
            i += esc;
            continue;
        }
        len = pf__utf8_next(p + i, n - i, &cp);
        if (len == 0)
            break;
        width += cp < 0 ? 1 : pf__cli_cp_width(cp);
        i += len;
    }
    return width;
}

PF_API size_t pf_cli_text_width(const char *s) {
    return s ? pf__cli_width_n(s, strlen(s)) : 0;
}

/* The bytes of `s` that fit in `cols` columns, whole characters only. */
static size_t pf__cli_fit(const char *s, size_t cols) {
    const unsigned char *p = (const unsigned char *)s;
    size_t width = 0, i = 0, n = strlen(s);

    while (i < n) {
        size_t esc = pf__cli_escape(s + i), len, w;
        long cp;
        if (esc) {
            i += esc;
            continue;
        }
        len = pf__utf8_next(p + i, n - i, &cp);
        w = cp < 0 ? 1 : pf__cli_cp_width(cp);
        if (len == 0 || width + w > cols)
            break;
        width += w;
        i += len;
    }
    return i;
}

PF_API void pf_cli_fill(FILE *f, unsigned n, char ch) {
    if (!f)
        return;
    while (n--)
        fputc(ch, f);
}

/* Wraps `text` with the cursor already at column `first`. */
static void pf__cli_wrap_at(
    pf_cli_t *c, unsigned first, unsigned next, unsigned width, const char *text
) {
    FILE *out = c->out.file;
    unsigned indent, col;
    const char *p = text;

    if (width == 0)
        width = (unsigned)c->out.columns;
    if (width < 10)
        width = 10;
    if (first >= width)
        first = width - 1;
    if (next >= width)
        next = width - 1;
    indent = col = first;

    while (*p) {
        const char *word, *q;
        size_t w;

        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (*p == '\n') {
            fputc('\n', out);
            p++;
            indent = col = next;
            if (*p)
                pf_cli_fill(out, indent, ' ');
            continue;
        }
        word = q = p;
        while (*q && *q != ' ' && *q != '\t' && *q != '\n') {
            size_t esc = pf__cli_escape(q);
            q += esc ? esc : 1;
        }
        w = pf__cli_width_n(word, (size_t)(q - word));
        if (col == indent) {
            fwrite(word, 1, (size_t)(q - word), out);
        } else if (col + 1 + w <= width) {
            fputc(' ', out);
            col++;
            fwrite(word, 1, (size_t)(q - word), out);
        } else {
            fputc('\n', out);
            indent = col = next;
            pf_cli_fill(out, indent, ' ');
            fwrite(word, 1, (size_t)(q - word), out);
        }
        col += (unsigned)w;
        p = q;
    }
}

PF_API void pf_cli_wrap(
    pf_cli_t *c,
    unsigned first_indent,
    unsigned next_indent,
    unsigned width,
    const char *text
) {
    if (!c || c->silent || !c->out.file || !text)
        return;
    pf__cli_clear_live(c, &c->out);
    pf_cli_fill(c->out.file, first_indent, ' ');
    pf__cli_wrap_at(c, first_indent, next_indent, width, text);
}

/* ---- Help -------------------------------------------------------------- */

#define PF__CLI_NAME_COLUMN 30u

PF_API void pf_cli_help_definition(
    pf_cli_t *c, const char *name, const char *description
) {
    FILE *out;
    unsigned columns, indent, desc;
    size_t name_width;

    if (!c || c->silent || !(out = c->out.file))
        return;
    pf__cli_clear_live(c, &c->out);
    name = name ? name : "";
    description = description ? description : "";
    columns = c->out.columns < 10 ? 10u : (unsigned)c->out.columns;
    indent = (unsigned)c->definition_indent;
    desc = indent + PF__CLI_NAME_COLUMN;
    name_width = pf_cli_text_width(name);

    pf_cli_fill(out, indent, ' ');
    fputs(name, out);
    /* A long name, or no room beside it: the description goes below. */
    if (name_width >= PF__CLI_NAME_COLUMN || desc >= columns
        || columns - desc < 10) {
        fputc('\n', out);
        if (*description) {
            pf_cli_fill(out, indent + 4, ' ');
            pf__cli_wrap_at(c, indent + 4, indent + 4, columns, description);
            fputc('\n', out);
        }
        return;
    }
    pf_cli_fill(out, PF__CLI_NAME_COLUMN - (unsigned)name_width, ' ');
    pf__cli_wrap_at(c, desc, desc, columns, description);
    fputc('\n', out);
}

PF_API void pf_cli_help_definitions(
    pf_cli_t *c, const pf_cli_definition_t *defs
) {
    if (!c || !defs)
        return;
    for (; defs->name; defs++)
        pf_cli_help_definition(c, defs->name, defs->description);
}

PF_API void pf_cli_help_section(pf_cli_t *c, const char *title) {
    if (!c || c->silent || !c->out.file)
        return;
    pf__cli_clear_live(c, &c->out);
    fputc('\n', c->out.file);
    pf__cli_style(&c->out, PF_CLI_BOLD);
    fputs(title ? title : "", c->out.file);
    pf__cli_style(&c->out, PF_CLI_RESET);
    fputc('\n', c->out.file);
}

/* ---- Prompts ----------------------------------------------------------- */

/* `n` without an incomplete UTF-8 sequence at its end. */
static size_t pf__cli_whole(const char *s, size_t n) {
    size_t lead = n, need;
    unsigned char b;

    while (lead > 0 && n - lead < 4
           && ((unsigned char)s[lead - 1] & 0xC0u) == 0x80u)
        lead--;
    if (lead == 0)
        return n;
    b = (unsigned char)s[lead - 1];
    need = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
    return n - (lead - 1) < need ? lead - 1 : n;
}

PF_API int pf_cli_readline(pf_cli_t *c, char *buf, size_t size) {
    size_t n;
    int ch;

    if (!c || !c->in.file || !buf || size == 0 || size > INT_MAX)
        return PF_EINVAL;
    if (!fgets(buf, (int)size, c->in.file)) {
        buf[0] = '\0';
        return ferror(c->in.file) ? PF_EIO : PF_ENODATA;
    }
    n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') {
        buf[--n] = '\0';
        if (n > 0 && buf[n - 1] == '\r')
            buf[--n] = '\0';
        return PF_OK;
    }
    if (feof(c->in.file))
        return PF_OK; /* the last line, without a newline */
    /* Too long: keep whole characters, drop the rest of the line. */
    buf[pf__cli_whole(buf, n)] = '\0';
    while ((ch = fgetc(c->in.file)) != EOF && ch != '\n')
        ;
    return PF_ERANGE;
}

static bool pf__cli_can_prompt(const pf_cli_t *c) {
    return c && !c->silent && c->interactive && c->out.file && c->in.file;
}

PF_API int pf_cli_ask(
    pf_cli_t *c, const char *question, char *buf, size_t size
) {
    if (buf && size > 0)
        buf[0] = '\0';
    if (!pf__cli_can_prompt(c))
        return c ? PF_EAGAIN : PF_EINVAL;
    pf__cli_clear_live(c, &c->out);
    fprintf(c->out.file, "%s: ", question ? question : "");
    fflush(c->out.file);
    return pf_cli_readline(c, buf, size);
}

PF_API bool pf_cli_confirm(pf_cli_t *c, const char *question, bool def) {
    char answer[32];
    int rc;

    if (!pf__cli_can_prompt(c))
        return def;
    pf__cli_clear_live(c, &c->out);
    fprintf(
        c->out.file,
        "%s%s",
        question ? question : "",
        def ? " [Y/n]: " : " [y/N]: "
    );
    fflush(c->out.file);
    rc = pf_cli_readline(c, answer, sizeof(answer));
    if ((rc != PF_OK && rc != PF_ERANGE) || !answer[0])
        return def;
    return answer[0] == 'y' || answer[0] == 'Y' || answer[0] == '1';
}

PF_API int pf_cli_select(
    pf_cli_t *c,
    const char *question,
    const char *const *choices,
    size_t count,
    size_t def
) {
    char answer[128];
    size_t k;
    int rc;

    if (!c || !choices || count == 0 || count > INT_MAX)
        return PF_EINVAL;
    if (def >= count)
        def = 0;
    if (!pf__cli_can_prompt(c))
        return (int)def;
    pf__cli_clear_live(c, &c->out);
    fprintf(c->out.file, "%s:\n", question ? question : "Select");
    for (k = 0; k < count; k++)
        fprintf(
            c->out.file,
            "  %lu) %s%s\n",
            (unsigned long)(k + 1),
            choices[k] ? choices[k] : "",
            k == def ? " (default)" : ""
        );
    fprintf(c->out.file, "Choice [%lu]: ", (unsigned long)(def + 1));
    fflush(c->out.file);

    rc = pf_cli_readline(c, answer, sizeof(answer));
    if (rc != PF_OK)
        return rc == PF_ERANGE ? PF_EINVAL : rc;
    if (!answer[0])
        return (int)def;
    {
        char *end;
        unsigned long n = strtoul(answer, &end, 10);
        if (end != answer && !*end && n >= 1 && n <= count)
            return (int)(n - 1);
    }
    for (k = 0; k < count; k++)
        if (choices[k] && strcmp(answer, choices[k]) == 0)
            return (int)k;
    return PF_EINVAL;
}

/* ---- Animation --------------------------------------------------------- */

static bool pf__cli_can_animate(const pf_cli_t *c) {
    return pf__cli_cursor(c) && c->interactive_mode != PF_INTERACTIVE_NEVER;
}

static bool pf__cli_due(uint64_t *last, bool *drawn, unsigned interval_ms) {
    uint64_t now = pf_time_ns();
    if (*drawn && now - *last < (uint64_t)interval_ms * 1000000u)
        return false;
    *last = now;
    *drawn = true;
    return true;
}

static const char *const pf__cli_frames_line[] = { "-", "\\", "|", "/", NULL };
static const char *const pf__cli_frames_dots[] = {
    "\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8",
    "\xE2\xA0\xBC", "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7",
    "\xE2\xA0\x87", "\xE2\xA0\x8F", NULL,
};

PF_API const char *const *pf_cli_spinner_frames(int kind) {
    return kind == PF_CLI_SPINNER_DOTS ? pf__cli_frames_dots
                                       : pf__cli_frames_line;
}

static void pf__cli_spinner_draw(pf_cli_spinner_t *s) {
    pf_cli_t *c = s->cli;
    FILE *out = c->out.file;
    const char *frame = s->opts.frames[s->frame];

    fputs("\r\033[2K", out);
    fputs(frame, out);
    if (s->opts.title) {
        /* Never as wide as the terminal, so the line does not wrap. */
        size_t used = pf_cli_text_width(frame) + 2,
               cols = (size_t)c->out.columns;
        size_t room = cols > used ? cols - used : 0;
        fputc(' ', out);
        fwrite(s->opts.title, 1, pf__cli_fit(s->opts.title, room), out);
    }
    fflush(out);
}

PF_API int pf_cli_spinner_start(
    pf_cli_spinner_t *s, pf_cli_t *c, const pf_cli_spinner_opts_t *o
) {
    if (!s)
        return PF_EINVAL;
    memset(s, 0, sizeof(*s));
    if (o)
        s->opts = *o;
    if (!s->opts.frames)
        s->opts.frames = pf__cli_frames_line;
    if (!s->opts.interval_ms)
        s->opts.interval_ms = PF__CLI_INTERVAL_MS;
    while (s->opts.frames[s->frame_count])
        s->frame_count++;
    if (!c || s->frame_count == 0)
        return PF_EINVAL;
    s->cli = c;
    s->pf__started = pf_time_ns();
    s->active = true;
    c->pf__live = s;
    return PF_OK;
}

PF_API void pf_cli_spinner_set_title(
    pf_cli_spinner_t *s, const char *fmt, ...
) {
    va_list ap;
    if (!s || !fmt)
        return;
    va_start(ap, fmt);
    vsnprintf(s->pf__title, sizeof(s->pf__title), fmt, ap);
    va_end(ap);
    s->opts.title = s->pf__title;
}

PF_API void pf_cli_spinner_tick(pf_cli_spinner_t *s) {
    if (!s || !s->active || !pf__cli_can_animate(s->cli))
        return;
    if (!pf__cli_due(&s->pf__last_draw, &s->pf__drawn, s->opts.interval_ms))
        return;
    s->cli->pf__live = s;
    if (s->opts.render)
        s->opts.render(s);
    else
        pf__cli_spinner_draw(s);
    s->frame = (s->frame + 1) % s->frame_count;
}

static void pf__cli_spinner_end(
    pf_cli_spinner_t *s,
    const char *marker,
    pf_cli_style_t style,
    const char *message
) {
    pf_cli_t *c;

    if (!s || !s->cli)
        return;
    c = s->cli;
    if (c->pf__live == s)
        c->pf__live = NULL;
    if (s->active && !c->silent && c->out.file) {
        if (pf__cli_can_animate(c))
            fputs("\r\033[2K", c->out.file);
        if (message) {
            if (marker) {
                pf__cli_style(&c->out, style);
                fputs(marker, c->out.file);
                pf__cli_style(&c->out, PF_CLI_RESET);
                fputc(' ', c->out.file);
            }
            fputs(message, c->out.file);
            fputc('\n', c->out.file);
        }
        fflush(c->out.file);
    }
    s->active = false;
}

PF_API void pf_cli_spinner_stop(pf_cli_spinner_t *s, const char *message) {
    pf__cli_spinner_end(s, NULL, 0, message);
}

PF_API void pf_cli_spinner_finish(
    pf_cli_spinner_t *s, bool ok, const char *fmt, ...
) {
    char message[256];
    va_list ap;

    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(message, sizeof(message), fmt, ap);
        va_end(ap);
    }
    pf__cli_spinner_end(
        s,
        ok ? "[ok]" : "[fail]",
        ok ? PF_FG_GREEN : PF_FG_RED,
        fmt ? message : NULL
    );
}

static void pf__cli_progress_draw(
    pf_cli_progress_t *p, const pf_cli_progress_frame_t *f
) {
    pf_cli_t *c = p->cli;
    FILE *out = c->out.file;
    unsigned k;

    fputs("\r\033[2K", out);
    if (p->opts.title)
        fprintf(out, "%s ", p->opts.title);
    fputc('[', out);
    pf__cli_style(&c->out, p->opts.style);
    for (k = 0; k < f->width; k++)
        fputc(k < f->filled ? p->opts.fill : p->opts.empty, out);
    pf__cli_style(&c->out, PF_CLI_RESET);
    fputc(']', out);
    if (p->opts.show_percentage)
        fprintf(out, " %3u%%", (unsigned)(f->ratio * 100.0 + 0.5));
    if (p->opts.show_value)
        fprintf(out, " %.0f/%.0f", p->value, p->maximum);
    if (p->opts.show_rate)
        fprintf(out, " %.1f/s", f->rate);
    if (p->opts.show_eta && p->value < p->maximum)
        fprintf(
            out,
            " ETA %02u:%02u",
            (unsigned)(f->eta / 60.0),
            (unsigned)f->eta % 60
        );
    if (p->opts.show_elapsed)
        fprintf(
            out,
            " %02u:%02u",
            (unsigned)(f->elapsed / 60.0),
            (unsigned)f->elapsed % 60
        );
    fflush(out);
}

static void pf__cli_progress_paint(pf_cli_progress_t *p, bool force) {
    pf_cli_progress_frame_t f;
    double ratio, elapsed;

    if (!p || !p->active || !pf__cli_can_animate(p->cli))
        return;
    if (force) {
        p->pf__last_draw = pf_time_ns();
        p->pf__drawn = true;
    } else if (!pf__cli_due(
                   &p->pf__last_draw, &p->pf__drawn, p->opts.interval_ms
               )) {
        return;
    }
    ratio = p->value / p->maximum;
    ratio = ratio < 0.0 ? 0.0 : ratio > 1.0 ? 1.0 : ratio;
    elapsed = (double)(pf_time_ns() - p->pf__started) / 1e9;
    if (elapsed < 0.001)
        elapsed = 0.001;
    f.width = p->opts.width;
    /* 0.3 * 10 may come out as 2.999... (x87). */
    f.filled = (unsigned)(ratio * p->opts.width + 1e-9);
    if (f.filled > p->opts.width)
        f.filled = p->opts.width;
    f.ratio = ratio;
    f.elapsed = elapsed;
    f.rate = p->value / elapsed;
    f.eta = f.rate > 0.0 ? (p->maximum - p->value) / f.rate : 0.0;
    p->cli->pf__live = p;
    if (p->opts.render)
        p->opts.render(p, &f);
    else
        pf__cli_progress_draw(p, &f);
}

PF_API int pf_cli_progress_start(
    pf_cli_progress_t *p,
    pf_cli_t *c,
    double maximum,
    const pf_cli_progress_opts_t *o
) {
    if (!p)
        return PF_EINVAL;
    memset(p, 0, sizeof(*p));
    if (!c)
        return PF_EINVAL;
    if (o)
        p->opts = *o;
    if (!p->opts.width)
        p->opts.width = (unsigned)(c->out.columns / 4);
    if (!p->opts.fill)
        p->opts.fill = '#';
    if (!p->opts.empty)
        p->opts.empty = '-';
    if (!p->opts.style)
        p->opts.style = PF_FG_GREEN;
    if (!p->opts.interval_ms)
        p->opts.interval_ms = PF__CLI_INTERVAL_MS;
    p->cli = c;
    p->maximum = maximum > 0.0 ? maximum : 1.0;
    p->pf__started = pf_time_ns();
    p->active = true;
    c->pf__live = p;
    return PF_OK;
}

PF_API void pf_cli_progress_set_title(
    pf_cli_progress_t *p, const char *fmt, ...
) {
    va_list ap;
    if (!p || !fmt)
        return;
    va_start(ap, fmt);
    vsnprintf(p->pf__title, sizeof(p->pf__title), fmt, ap);
    va_end(ap);
    p->opts.title = p->pf__title;
}

static void pf__cli_progress_set(pf_cli_progress_t *p, double value) {
    p->value = value < 0.0 ? 0.0 : value > p->maximum ? p->maximum : value;
    pf__cli_progress_paint(p, false);
}

PF_API void pf_cli_progress_update(pf_cli_progress_t *p, double value) {
    if (p && p->cli)
        pf__cli_progress_set(p, value);
}

PF_API void pf_cli_progress_advance(pf_cli_progress_t *p, double delta) {
    if (p && p->cli)
        pf__cli_progress_set(p, p->value + delta);
}

PF_API void pf_cli_progress_render(pf_cli_progress_t *p) {
    pf__cli_progress_paint(p, true);
}

static void pf__cli_progress_end(pf_cli_progress_t *p, bool complete) {
    pf_cli_t *c;

    if (!p || !p->cli)
        return;
    c = p->cli;
    if (p->active && !c->silent && c->out.file) {
        if (complete) {
            p->value = p->maximum;
            pf__cli_progress_paint(p, true);
            if (pf__cli_can_animate(c))
                fputc('\n', c->out.file);
        } else if (pf__cli_can_animate(c)) {
            fputs("\r\033[2K", c->out.file);
        }
        fflush(c->out.file);
    }
    if (c->pf__live == p)
        c->pf__live = NULL;
    p->active = false;
}

PF_API void pf_cli_progress_finish(pf_cli_progress_t *p) {
    pf__cli_progress_end(p, true);
}

PF_API void pf_cli_progress_stop(pf_cli_progress_t *p) {
    pf__cli_progress_end(p, false);
}

/* ---- argparse ---------------------------------------------------------- */

#if PF__HAVE_ARGPARSE

/* The settings of the parser that change how errors look. */
static void pf__cli_sync(pf_cli_t *c, const pf_argparser_t *p) {
    if (p->color_set && p->color_mode != c->color_mode)
        pf_cli_set_color_mode(c, p->color_mode);
}

static void pf__cli_args_error(
    pf_argparser_t *p, const char *message, void *user
) {
    pf_cli_t *c = (pf_cli_t *)user;
    pf__cli_sync(c, p);
    if (p->err_prefix)
        pf_cli_ecprintf(c, PF_FG_RED | PF_CLI_BOLD, "%s", p->err_prefix);
    pf_cli_errorf(c, "%s\n", message);
}

PF_API void pf_cli_bind_args(pf_cli_t *c, pf_argparser_t *p) {
    if (!c || !p)
        return;
    p->out = c->out.file;
    p->err = c->err.file;
    p->verbosity = c->verbosity;
    pf_argparser_set_error_fn(p, pf__cli_args_error, c);
}

PF_API void pf_cli_apply_args(pf_cli_t *c, const pf_argparser_t *p) {
    if (!c || !p)
        return;
    c->verbosity = p->verbosity;
    c->silent = p->silent || !c->out.file;
    if (p->color_set)
        pf_cli_set_color_mode(c, p->color_mode);
    if (p->interactive_set)
        pf_cli_set_interactive_mode(c, p->interactive_mode);
}

static void pf__cli_help_option(
    pf_cli_t *c, char ch, const char *name, const char *arg, const char *text
) {
    char head[64];
    if (ch)
        snprintf(head, sizeof(head), "-%c, --%s%s", ch, name, arg);
    else
        snprintf(head, sizeof(head), "    --%s%s", name, arg);
    pf_cli_help_definition(c, head, text);
}

PF_API void pf_cli_help_options(
    pf_cli_t *c, const pf_argparser_t *p, unsigned mask
) {
    /* The order of pf__shorts: help, verbose, quiet, color, version,
       interactive (the PF_COMMON_* bits). */
    static const struct {
        unsigned bit;
        int index;
        const char *name, *arg, *text;
    } opts[] = {
        { PF_COMMON_HELP, 0, "help", "", "Shows this information." },
        { PF_COMMON_VERSION, 4, "version", "", "Shows version information." },
        { PF_COMMON_VERBOSE,
          1,
          "verbose",
          "",
          "Prints more messages. Repeat for more detail." },
        { PF_COMMON_QUIET,
          2,
          "quiet",
          "",
          "Disables output and error printing." },
        { PF_COMMON_COLOR,
          3,
          "color",
          "[=WHEN]",
          "Colors the output: 'auto', 'always' or 'never'." },
        { PF_COMMON_INTERACTIVE,
          5,
          "interactive",
          "[=WHEN]",
          "Allows prompts: 'auto', 'always' or 'never'." },
    };
    size_t k;

    if (!c || !p)
        return;
    for (k = 0; k < sizeof(opts) / sizeof(opts[0]); k++)
        if (mask & opts[k].bit)
            pf__cli_help_option(
                c,
                p->pf__shorts[opts[k].index],
                opts[k].name,
                opts[k].arg,
                opts[k].text
            );
}

#else

PF_API void pf_cli_bind_args(pf_cli_t *c, struct pf_argparser_t *p) {
    (void)c, (void)p;
}

PF_API void pf_cli_apply_args(pf_cli_t *c, const struct pf_argparser_t *p) {
    (void)c, (void)p;
}

PF_API void pf_cli_help_options(
    pf_cli_t *c, const struct pf_argparser_t *p, unsigned mask
) {
    (void)c, (void)p, (void)mask;
}

#endif
