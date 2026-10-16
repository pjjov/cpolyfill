/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides functions for building command line interfaces.

    Last-updated: October 2026
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

#ifndef POLYFILL_CLI
#define POLYFILL_CLI

#ifndef PF_API
    #define PF_API static inline
#endif

#if defined(_WIN32)
    #define PF_CLI_WINDOWS 1
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <io.h>
    #include <windows.h>
#else
    #define PF_CLI_POSIX 1
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PF_BOOL
    #define PF_BOOL
    #define PF_TRUE 1
    #define PF_FALSE 0
    #if defined(bool)
typedef bool pf_bool;
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef bool pf_bool;
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
typedef _Bool pf_bool;
    #else
typedef int pf_bool;
    #endif
#endif

enum pf_cli_color {
    PF_CLI_RESET = 0x2000,
    PF_CLI_DIM = 0x4000,
    PF_CLI_BOLD = 0x8000,
    PF_FG_BLACK = 30,
    PF_FG_RED = 31,
    PF_FG_GREEN = 32,
    PF_FG_YELLOW = 33,
    PF_FG_BLUE = 34,
    PF_FG_MAGENTA = 35,
    PF_FG_CYAN = 36,
    PF_FG_WHITE = 37,
    PF_BG_BLACK = 40,
    PF_BG_RED = 41,
    PF_BG_GREEN = 42,
    PF_BG_YELLOW = 43,
    PF_BG_BLUE = 44,
    PF_BG_MAGENTA = 45,
    PF_BG_CYAN = 46,
    PF_BG_WHITE = 47,

    PF__CLI_COLOR_MASK = 0xFF,
};

/* Shared with pf_argparse.h, which can be used without pf_cli.h. */
#ifndef PF_MODE_ENUMS
    #define PF_MODE_ENUMS
enum pf_color_mode_t {
    PF_COLOR_AUTO,
    PF_COLOR_ALWAYS,
    PF_COLOR_NEVER,
};

enum pf_interactive_mode_t {
    PF_INTERACTIVE_AUTO,
    PF_INTERACTIVE_ALWAYS,
    PF_INTERACTIVE_NEVER,
};
#endif

enum pf_clear_mode {
    PF_CLEAR_END,
    PF_CLEAR_START,
    PF_CLEAR_ALL,
};

typedef struct pf_cli_stream_t pf_cli_stream_t;
typedef struct pf_cli_spinner_t pf_cli_spinner_t;
typedef struct pf_cli_progress_t pf_cli_progress_t;
typedef struct pf_cli_t pf_cli_t;
struct pf_cli_progress_frame;

typedef void(pf_cli_spinner_fn)(pf_cli_spinner_t *s);
typedef void(pf_cli_progress_fn)(
    pf_cli_progress_t *p, struct pf_cli_progress_frame *frame
);

/* A name/description pair for help output; lists end with a NULL name. */
typedef struct pf_cli_definition_t {
    const char *name;
    const char *description;
} pf_cli_definition_t;

struct pf_cli_opt {
    FILE *out;
    FILE *err;
    FILE *in;

    enum pf_interactive_mode_t interactiveMode;
    enum pf_color_mode_t colorMode;
    int definitionIndent;
    pf_bool allowNullStreams;
};

struct pf_cli_stream_t {
    FILE *handle;
    pf_bool colorEnabled;
    pf_bool isTerminal;
    pf_bool supportsAnsi;
    pf_bool supportsCursor;
    pf_bool supportsHyperlinks;
    int columns;
};

struct pf_cli_t {
    pf_cli_stream_t in;
    pf_cli_stream_t out;
    pf_cli_stream_t err;

    int definitionIndent;
    enum pf_color_mode_t colorMode;
    enum pf_interactive_mode_t interactiveMode;

    pf_bool isInteractive;
    pf_bool silent;

    /* Verbosity level; 0 is normal, see pf_cli_verbosef(). */
    int verbosity;

    void *live; /* currently animated widget, if any */
};

struct pf_cli_spinner_opt {
    const char *title;
    const char *const *frames;
    pf_cli_spinner_fn *render;
};

struct pf_cli_spinner_t {
    pf_cli_t *cli;

    size_t frame;
    double startedAt;

    int active;
    int frameCount;

    struct pf_cli_spinner_opt options;

    char titleBuf[160];
};

struct pf_cli_progress_opt {
    const char *title;
    unsigned width;

    pf_bool showPercentage;
    pf_bool showValue;
    pf_bool showRate;
    pf_bool showEta;
    pf_bool showElapsed;
    pf_bool dynamic;

    char fill;
    char empty;
    enum pf_cli_color color;

    pf_cli_progress_fn *render;
};

struct pf_cli_progress_frame {
    unsigned width;
    unsigned filled;
    double ratio;
    double elapsed;
    double rate;
    double eta;
};

struct pf_cli_progress_t {
    pf_cli_t *cli;

    double value;
    double maximum;

    const char *title;

    double startedAt;
    double lastRender;

    struct pf_cli_progress_opt options;

    int active;

    char titleBuf[160];
};

/* ------------------------------------------------------------------------ */
/* Terminal detection                                                        */
/* ------------------------------------------------------------------------ */

PF_API double pf_cli_now(void) {
#ifdef PF_CLI_WINDOWS
    LARGE_INTEGER freq, counter;

    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0
        || !QueryPerformanceCounter(&counter))
        return 0.0;

    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;

    #ifdef CLOCK_MONOTONIC
    /* timespec_get needs C11, while this is available everywhere else. */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    #else
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC)
        return 0.0;
    #endif

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

PF_API int pf__cli_is_terminal(FILE *handle) {
    if (!handle)
        return 0;

#ifdef PF_CLI_WINDOWS
    return _isatty(_fileno(handle));
#else
    return isatty(fileno(handle));
#endif
}

/* Can escape sequences (cursor movement, line clearing) be used? */
PF_API int pf__cli_ansi_supported(FILE *handle) {
    if (!pf__cli_is_terminal(handle))
        return 0;

#ifdef PF_CLI_WINDOWS
    {
        DWORD mode = 0;
        HANDLE h = (HANDLE)_get_osfhandle(_fileno(handle));

        if (!GetConsoleMode(h, &mode))
            return 0;
        if (mode & 0x0004) /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
            return 1;
        return SetConsoleMode(h, mode | 0x0004) != 0;
    }
#else
    {
        const char *term = getenv("TERM");
        return term && *term && strcmp(term, "dumb") != 0;
    }
#endif
}

PF_API int pf__cli_color_enabled(const pf_cli_t *c, FILE *handle) {
    const char *noColor;

    if (!c)
        return 0;

    if (c->colorMode == PF_COLOR_ALWAYS)
        return 1;

    if (c->colorMode == PF_COLOR_NEVER)
        return 0;

    noColor = getenv("NO_COLOR");
    if (noColor && *noColor)
        return 0;

    return pf__cli_ansi_supported(handle);
}

PF_API unsigned pf__cli_columns(const pf_cli_t *c, FILE *handle) {
    unsigned fallback = 80;
    const char *env = getenv("COLUMNS");

    (void)c;

    if (env && *env) {
        char *end = NULL;
        unsigned long n = strtoul(env, &end, 10);

        if (end != env && n >= 20 && n <= 1000)
            return (unsigned)n;
    }

#ifdef PF_CLI_WINDOWS

    if (handle && pf__cli_is_terminal(handle)) {
        CONSOLE_SCREEN_BUFFER_INFO info;

        if (GetConsoleScreenBufferInfo(
                (HANDLE)_get_osfhandle(_fileno(handle)), &info
            )) {

            int width = (int)info.srWindow.Right - (int)info.srWindow.Left + 1;

            if (width >= 20)
                return (unsigned)width;
        }
    }

#else

    if (handle && pf__cli_is_terminal(handle)) {
        struct winsize ws;

        if (ioctl(fileno(handle), TIOCGWINSZ, &ws) == 0 && ws.ws_col >= 20) {
            return (unsigned)ws.ws_col;
        }
    }

#endif

    return fallback;
}

PF_API void pf__cli_update_stream_color(pf_cli_t *c, pf_cli_stream_t *stream) {
    if (!stream->handle)
        return;

    stream->colorEnabled = pf__cli_color_enabled(c, stream->handle) ? 1 : 0;
    stream->supportsHyperlinks = stream->supportsAnsi && stream->colorEnabled
        && !getenv("NO_HYPERLINKS");
}

PF_API void pf__cli_init_stream(
    pf_cli_t *c, pf_cli_stream_t *stream, FILE *handle
) {
    if (!handle) {
        memset(stream, 0, sizeof(*stream));
        return;
    }

    stream->handle = handle;
    stream->isTerminal = pf__cli_is_terminal(handle) ? 1 : 0;
    stream->columns = (int)pf__cli_columns(c, handle);

    /* Cursor control is independent of whether colors are wanted. */
    stream->supportsAnsi = pf__cli_ansi_supported(handle) ? 1 : 0;
    stream->supportsCursor = stream->supportsAnsi;

    pf__cli_update_stream_color(c, stream);
}

PF_API int pf__cli_is_interactive(const pf_cli_t *c) {
    if (!c)
        return 0;

    if (c->interactiveMode == PF_INTERACTIVE_ALWAYS)
        return 1;

    if (c->interactiveMode == PF_INTERACTIVE_NEVER)
        return 0;

    return c->out.isTerminal && c->in.isTerminal;
}

/* Is it appropriate to redraw a line in place on the output stream? */
PF_API int pf__cli_can_animate(const pf_cli_t *c) {
    if (!c || c->silent || !c->out.handle)
        return 0;
    if (c->interactiveMode == PF_INTERACTIVE_NEVER)
        return 0;
    return c->out.supportsCursor ? 1 : 0;
}

PF_API struct pf_cli_opt *pf__cli_apply_defaults(struct pf_cli_opt *o) {
    if (!o->out && !o->allowNullStreams)
        o->out = stdout;
    if (!o->in && !o->allowNullStreams)
        o->in = stdin;
    if (!o->err && !o->allowNullStreams)
        o->err = stderr;
    if (o->definitionIndent <= 0 || o->definitionIndent > 80)
        o->definitionIndent = 10;
    return o;
}

PF_API void pf_cli_init(pf_cli_t *c, struct pf_cli_opt *o) {
    struct pf_cli_opt fallback;

    if (!c)
        return;

    memset(c, 0, sizeof(*c));
    memset(&fallback, 0, sizeof(fallback));
    o = pf__cli_apply_defaults(o ? o : &fallback);

    c->definitionIndent = o->definitionIndent;
    c->silent = PF_FALSE;
    c->colorMode = o->colorMode;
    c->interactiveMode = o->interactiveMode;

    pf__cli_init_stream(c, &c->out, o->out);
    pf__cli_init_stream(c, &c->err, o->err);
    pf__cli_init_stream(c, &c->in, o->in);

    if (!c->out.handle)
        c->silent = PF_TRUE;
    c->isInteractive = pf__cli_is_interactive(c) ? 1 : 0;
}

PF_API void pf_cli_free(pf_cli_t *c) { (void)c; }

/* Changes the color mode at runtime (e.g. after parsing --color). */
PF_API void pf_cli_set_color_mode(pf_cli_t *c, enum pf_color_mode_t mode) {
    if (!c)
        return;

    c->colorMode = mode;
    pf__cli_update_stream_color(c, &c->out);
    pf__cli_update_stream_color(c, &c->err);
}

PF_API void pf_cli_set_interactive_mode(
    pf_cli_t *c, enum pf_interactive_mode_t mode
) {
    if (!c)
        return;

    c->interactiveMode = mode;
    c->isInteractive = pf__cli_is_interactive(c) ? 1 : 0;
}

/* ------------------------------------------------------------------------ */
/* Output primitives                                                         */
/* ------------------------------------------------------------------------ */

PF_API void pf__cli_stream_color(pf_cli_stream_t *s, int color) {
    const char *fmt = "\033[%dm";

    if (!s || !s->handle || !s->colorEnabled)
        return;

    if (color & PF_CLI_RESET)
        fprintf(s->handle, fmt, 0);
    if (color & PF_CLI_BOLD)
        fprintf(s->handle, fmt, 1);
    if (color & PF_CLI_DIM)
        fprintf(s->handle, fmt, 2);
    if (color & PF__CLI_COLOR_MASK)
        fprintf(s->handle, fmt, color & PF__CLI_COLOR_MASK);
}

PF_API void pf_cli_color(pf_cli_t *c, int color) {
    if (!c || c->silent)
        return;

    pf__cli_stream_color(&c->out, color);
}

PF_API void pf_cli_clearline(pf_cli_t *c, enum pf_clear_mode mode) {
    char seq[6];

    if (!c || c->silent || !c->out.supportsAnsi)
        return;
    if ((int)mode < 0 || (int)mode > 2)
        return;

    seq[0] = '\r';
    seq[1] = '\033';
    seq[2] = '[';
    seq[3] = (char)('0' + (int)mode);
    seq[4] = 'K';
    seq[5] = '\0';
    fputs(seq, c->out.handle);
}

PF_API void pf_cli_clear(pf_cli_t *c, enum pf_clear_mode mode) {
    char seq[6];

    if (!c || c->silent || !c->out.supportsAnsi)
        return;
    if ((int)mode < 0 || (int)mode > 3)
        return;

    seq[0] = '\r';
    seq[1] = '\033';
    seq[2] = '[';
    seq[3] = (char)('0' + (int)mode);
    seq[4] = 'J';
    seq[5] = '\0';
    fputs(seq, c->out.handle);
}

PF_API void pf__cli_move(pf_cli_t *c, int n, char pos, char neg) {
    fprintf(c->out.handle, "\033[%d%c", n > 0 ? n : -n, n > 0 ? pos : neg);
}

PF_API void pf_cli_move(pf_cli_t *c, int x, int y) {
    if (!c || c->silent || !c->out.supportsAnsi)
        return;

    if (x != 0)
        pf__cli_move(c, x, 'C', 'D');

    if (y != 0)
        pf__cli_move(c, y, 'A', 'B');
}

/* Moves the cursor to the zero-based column `x` and row `y`. */
PF_API void pf_cli_position(pf_cli_t *c, int x, int y) {
    if (!c || c->silent || !c->out.supportsAnsi)
        return;

    if (x < 0 || y < 0)
        return;

    fprintf(c->out.handle, "\033[%d;%dH", y + 1, x + 1);
}

/* Erases the animated widget line so that regular text can be printed. */
PF_API void pf__cli_clear_live(pf_cli_t *c, pf_cli_stream_t *stream) {
    if (!c->live || !c->out.handle || !c->out.supportsAnsi)
        return;

    if (stream == &c->out || stream->isTerminal)
        fputs("\r\033[2K", c->out.handle);
}

/*
 * Formatted output with an optional color. Thread-safe, and safe to call
 * while a spinner or progress bar is being animated.
 */
PF_API void pf_cli_vcprintf(
    pf_cli_t *c,
    pf_cli_stream_t *stream,
    int color,
    const char *fmt,
    va_list args
) {
    if (!c || !stream || !stream->handle || !fmt || c->silent)
        return;

    pf__cli_clear_live(c, stream);

    if (color)
        pf__cli_stream_color(stream, color);

    vfprintf(stream->handle, fmt, args);

    if (color)
        pf__cli_stream_color(stream, PF_CLI_RESET);
}

PF_API void pf_cli_vfprintf(
    pf_cli_t *c, pf_cli_stream_t *stream, const char *fmt, va_list args
) {
    pf_cli_vcprintf(c, stream, 0, fmt, args);
}

PF_API void pf_cli_cprintf(pf_cli_t *c, int color, const char *fmt, ...) {
    va_list args;

    if (!c)
        return;

    va_start(args, fmt);
    pf_cli_vcprintf(c, &c->out, color, fmt, args);
    va_end(args);
}

/* Like pf_cli_cprintf(), but writes to the error stream. */
PF_API void pf_cli_ecprintf(pf_cli_t *c, int color, const char *fmt, ...) {
    va_list args;

    if (!c)
        return;

    va_start(args, fmt);
    pf_cli_vcprintf(c, &c->err, color, fmt, args);
    va_end(args);
}

PF_API void pf_cli_printf(pf_cli_t *c, const char *fmt, ...) {
    va_list args;

    if (!c)
        return;

    va_start(args, fmt);
    pf_cli_vcprintf(c, &c->out, 0, fmt, args);
    va_end(args);
}

PF_API void pf_cli_errorf(pf_cli_t *c, const char *fmt, ...) {
    va_list args;

    if (!c)
        return;

    va_start(args, fmt);
    pf_cli_vcprintf(c, &c->err, 0, fmt, args);
    va_end(args);
}

/* Prints only when c->verbosity >= level (level 1 is "--verbose"). */
PF_API void pf_cli_verbosef(pf_cli_t *c, int level, const char *fmt, ...) {
    va_list args;

    if (!c || c->verbosity < level)
        return;

    va_start(args, fmt);
    pf_cli_vcprintf(c, &c->out, 0, fmt, args);
    va_end(args);
}

PF_API void pf_cli_fill(FILE *out, unsigned n, char c) {
    if (!out || n == 0)
        return;

    while (n--)
        fputc(c, out);
}

PF_API unsigned pf_cli_text_width(const char *s) {
    unsigned width = 0;

    if (!s)
        return 0;

    while (*s) {
        /*
         * Ignore ANSI CSI escape sequences:
         *
         *   ESC [ ... final-byte
         */
        if ((unsigned char)*s == 27 && s[1] == '[') {
            s += 2;

            while (*s) {
                unsigned char ch = (unsigned char)*s++;

                if (ch >= '@' && ch <= '~')
                    break;
            }

            continue;
        }

        /*
         * Newlines don't contribute to the width of a line.
         */
        if (*s == '\n' || *s == '\r') {
            ++s;
            continue;
        }

        ++width;
        ++s;
    }

    return width;
}

PF_API void pf__cli_wrap_at(
    pf_cli_t *c,
    unsigned firstIndent,
    unsigned nextIndent,
    unsigned width,
    const char *text
) {
    FILE *out;
    const char *p;
    unsigned indent;
    unsigned col;

    if (!c || c->silent || !text)
        return;

    out = c->out.handle;

    if (!out)
        return;

    if (width == 0)
        width = (unsigned)c->out.columns;

    if (width < 10)
        width = 10;

    /*
     * Keep indentation inside the requested line width.
     */
    if (firstIndent >= width)
        firstIndent = width - 1;

    if (nextIndent >= width)
        nextIndent = width - 1;

    indent = col = firstIndent;

    p = text;

    while (*p) {
        const char *word;
        const char *q;
        unsigned wordWidth = 0;
        size_t wordBytes;

        /*
         * Skip whitespace between words.
         */
        while (*p == ' ' || *p == '\t')
            ++p;

        if (!*p)
            break;

        /*
         * Explicit newline.
         */
        if (*p == '\n') {
            fputc('\n', out);
            ++p;

            indent = nextIndent;
            col = indent;

            if (*p)
                pf_cli_fill(out, indent, ' ');

            continue;
        }

        word = p;
        q = p;

        /*
         * Find the word and calculate its visible width.
         */
        while (*q && *q != ' ' && *q != '\t' && *q != '\n') {
            if ((unsigned char)*q == 27 && q[1] == '[') {
                /*
                 * Skip:
                 *
                 *   ESC [ ... final-byte
                 */
                q += 2;

                while (*q) {
                    unsigned char ch = (unsigned char)*q++;

                    if (ch >= '@' && ch <= '~')
                        break;
                }

                continue;
            }

            ++wordWidth;
            ++q;
        }

        wordBytes = (size_t)(q - word);

        /*
         * Word is the first item on this line.
         */
        if (col == indent) {
            fwrite(word, 1, wordBytes, out);
            col += wordWidth;
        }

        /*
         * Word fits after a separating space.
         */
        else if (col + 1 + wordWidth <= width) {
            fputc(' ', out);
            ++col;

            fwrite(word, 1, wordBytes, out);
            col += wordWidth;
        }

        /*
         * Word doesn't fit. Start a new line.
         */
        else {
            fputc('\n', out);

            indent = nextIndent;
            col = indent;

            pf_cli_fill(out, indent, ' ');

            fwrite(word, 1, wordBytes, out);
            col += wordWidth;
        }

        p = q;
    }
}

PF_API void pf_cli_wrap(
    pf_cli_t *c,
    unsigned firstIndent,
    unsigned nextIndent,
    unsigned width,
    const char *text
) {
    if (!c || !c->out.handle)
        return;

    pf_cli_fill(c->out.handle, firstIndent, ' ');
    pf__cli_wrap_at(c, firstIndent, nextIndent, width, text);
}

PF_API void pf_cli_help_definition(
    pf_cli_t *c, const char *name, const char *description
) {
    FILE *out;
    unsigned columns;
    unsigned indent;
    unsigned nameWidth;
    unsigned descIndent;

    if (!c || c->silent)
        return;

    out = c->out.handle;

    if (!out)
        return;

    name = name ? name : "";
    description = description ? description : "";

    columns = (unsigned)c->out.columns;

    if (columns < 10)
        columns = 10;

    indent = (unsigned)c->definitionIndent;
    nameWidth = pf_cli_text_width(name);

    /*
     * Long names get their own line.
     */
    if (nameWidth > 30) {
        pf_cli_fill(out, indent, ' ');
        fputs(name, out);
        fputc('\n', out);

        pf_cli_wrap(c, indent + 4, indent + 4, columns, description);

        fputc('\n', out);
        return;
    }

    /*
     * Descriptions normally begin at a fixed column:
     *
     *           --foo                  Description...
     *                                  continued...
     */
    descIndent = indent + 30;

    /*
     * Not enough room for the description beside the name.
     */
    if (descIndent >= columns || columns - descIndent < 10) {
        pf_cli_fill(out, indent, ' ');
        fputs(name, out);
        fputc('\n', out);

        pf_cli_wrap(c, indent + 4, indent + 4, columns, description);

        fputc('\n', out);
        return;
    }

    /*
     * Print the name and position the cursor at the description
     * column.
     */
    pf_cli_fill(out, indent, ' ');
    fputs(name, out);
    pf_cli_fill(out, 30 - nameWidth, ' ');

    /*
     * The cursor is now already at descIndent.
     *
     * Tell the wrapper not to emit firstIndent again.
     */
    pf__cli_wrap_at(c, descIndent, descIndent, columns, description);

    fputc('\n', out);
}

/* Prints a NULL-terminated list of definitions (options, subcommands...). */
PF_API void pf_cli_help_definitions(
    pf_cli_t *c, const pf_cli_definition_t *defs
) {
    if (!c || !defs)
        return;

    for (; defs->name; defs++)
        pf_cli_help_definition(c, defs->name, defs->description);
}

PF_API void pf_cli_help_section(pf_cli_t *c, const char *title) {
    if (!c || c->silent)
        return;

    FILE *out = c->out.handle;
    fputc('\n', out);

    pf_cli_color(c, PF_CLI_BOLD);
    fputs(title ? title : "", out);
    pf_cli_color(c, PF_CLI_RESET);

    fputc('\n', out);
}

PF_API void pf_cli_link(pf_cli_t *c, const char *url, const char *text) {
    if (!c || c->silent || !url || !text)
        return;

    FILE *out = c->out.handle;

    if (!c->out.supportsHyperlinks) {
        fputs(text, out);
        return;
    }

    const char *fmt = "\033]8;;%s\033\\%s\033]8;;\033\\";
    fprintf(out, fmt, url, text);
}

PF_API void pf_cli_path_link(pf_cli_t *c, const char *path, unsigned line) {
    if (!c || c->silent || !path)
        return;

    FILE *out = c->out.handle;

    if (!c->out.supportsHyperlinks) {
        fputs(path, out);
        return;
    }

    fputs("\033]8;;", out);

    const char *fmt = line > 0 ? "file://%s#L%u" : "file://%s";
    fprintf(out, fmt, path, line);

    fprintf(out, "\033\\%s\033]8;;\033\\", path);
}

PF_API int pf_cli_readline(pf_cli_t *c, char *buffer, size_t size) {
    if (!c || !c->in.handle || !buffer || size == 0)
        return 0;

    if (!fgets(buffer, (int)size, c->in.handle))
        return 0;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    return 1;
}

PF_API int pf_cli_ask(
    pf_cli_t *c, const char *question, char *answer, size_t answerSize
) {
    if (!c || c->silent || c->interactiveMode == PF_INTERACTIVE_NEVER)
        return 0;

    FILE *out = c->out.handle;
    fputs(question ? question : "", out);
    fputs(": ", out);
    fflush(out);

    return pf_cli_readline(c, answer, answerSize);
}

PF_API int pf_cli_confirm(pf_cli_t *c, const char *question, int defaultValue) {
    if (!c || c->silent || c->interactiveMode == PF_INTERACTIVE_NEVER)
        return defaultValue;

    char answer[32];
    FILE *out = c->out.handle;

    fputs(question ? question : "", out);
    fputs(defaultValue ? " [Y/n]: " : " [y/N]: ", out);
    fflush(out);

    if (!pf_cli_readline(c, answer, sizeof(answer)))
        return defaultValue;

    if (!answer[0])
        return defaultValue;

    return answer[0] == 'y' || answer[0] == 'Y' || answer[0] == '1';
}

PF_API int pf_cli_select(
    pf_cli_t *c,
    const char *question,
    const char **choices,
    size_t count,
    size_t defaultIndex
) {
    if (!c || c->silent || !choices || count == 0)
        return -1;

    if (defaultIndex >= count)
        defaultIndex = 0;

    if (c->interactiveMode == PF_INTERACTIVE_NEVER)
        return (int)defaultIndex;

    char answer[64];
    size_t i;
    FILE *out = c->out.handle;

    fputs(question ? question : "Select", out);
    fputs(":\n", out);

    for (i = 0; i < count; ++i) {
        fprintf(out, "  %lu) %s", (unsigned long)(i + 1), choices[i]);

        if (i == defaultIndex)
            fputs(" (default)", out);

        fputc('\n', out);
    }

    fprintf(out, "Choice [%lu]: ", (unsigned long)(defaultIndex + 1));
    fflush(out);

    if (!pf_cli_readline(c, answer, sizeof(answer)))
        return -1;

    if (!answer[0])
        return (int)defaultIndex;

    {
        char *end = NULL;
        unsigned long n = strtoul(answer, &end, 10);

        if (end != answer && n >= 1 && n <= count)
            return (int)(n - 1);
    }

    /*
     * Also permit entering the label directly.
     */
    for (i = 0; i < count; ++i)
        if (choices[i] && strcmp(answer, choices[i]) == 0)
            return (int)i;

    return -1;
}

/* ------------------------------------------------------------------------ */
/* Spinner                                                                   */
/* ------------------------------------------------------------------------ */

/* Ready-made frame lists for pf_cli_spinner_opt.frameList (UTF-8). */
static const char *const pf_cli_spinner_dots[] = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴",
                                                   "⠦", "⠧", "⠇", "⠏", NULL };
static const char *const pf_cli_spinner_line[] = { "-", "\\", "|", "/", NULL };

PF_API void pf__cli_spinner_default_render_cb(pf_cli_spinner_t *s) {
    pf_cli_t *c = s->cli;
    FILE *out = c->out.handle;
    int maxTitle = c->out.columns - 4;

    pf_cli_clearline(c, PF_CLEAR_ALL);

    fputs(s->options.frames[s->frame], out);

    if (s->options.title) {
        /* Keep the line shorter than the terminal so that it never wraps. */
        fprintf(out, " %.*s", maxTitle > 0 ? maxTitle : 0, s->options.title);
    }

    fflush(out);

    s->frame = (s->frame + 1) % (size_t)s->frameCount;
}

PF_API void pf__cli_spinner_apply_defaults(struct pf_cli_spinner_opt *opt) {
    if (!opt->frames)
        opt->frames = pf_cli_spinner_line;
    if (!opt->render)
        opt->render = pf__cli_spinner_default_render_cb;
}

PF_API int pf_cli_spinner_start(
    pf_cli_spinner_t *s, pf_cli_t *c, const struct pf_cli_spinner_opt *options
) {
    struct pf_cli_spinner_opt opt;

    if (!s)
        return -1;

    memset(&opt, 0, sizeof(opt));
    if (options)
        opt = *options;
    pf__cli_spinner_apply_defaults(&opt);

    memset(s, 0, sizeof(*s));
    s->cli = c;
    s->startedAt = pf_cli_now();
    s->options = opt;

    while (opt.frames[s->frameCount])
        s->frameCount++;

    if (!c || s->frameCount == 0)
        return -1;

    s->active = 1;
    return 0;
}

/* Thread-safe. Changes the text shown next to the spinner. */
PF_API void pf_cli_spinner_set_title(
    pf_cli_spinner_t *s, const char *fmt, ...
) {
    va_list args;

    if (!s || !s->cli || !fmt)
        return;

    va_start(args, fmt);
    vsnprintf(s->titleBuf, sizeof(s->titleBuf), fmt, args);
    va_end(args);
    s->options.title = s->titleBuf;
}

/* Draws one frame; for the manual (non-async) mode. */
PF_API void pf_cli_spinner_tick(pf_cli_spinner_t *s) {
    if (!s || !s->active || !s->cli)
        return;

    /* Don't animate redirected output. */
    if (pf__cli_can_animate(s->cli))
        s->options.render(s);
}

PF_API void pf__cli_spinner_stop(
    pf_cli_spinner_t *s,
    const char *marker,
    int markerColor,
    const char *message
) {
    pf_cli_t *c;

    if (!s || !s->cli)
        return;

    c = s->cli;

    if (c->live == s)
        c->live = NULL;

    if (s->active && !c->silent) {
        if (pf__cli_can_animate(c))
            pf_cli_clearline(c, PF_CLEAR_ALL);

        if (message && c->out.handle) {
            if (marker) {
                pf__cli_stream_color(&c->out, markerColor);
                fputs(marker, c->out.handle);
                pf__cli_stream_color(&c->out, PF_CLI_RESET);
                fputc(' ', c->out.handle);
            }
            fputs(message, c->out.handle);
            fputc('\n', c->out.handle);
        }

        if (c->out.handle)
            fflush(c->out.handle);
    }

    s->active = 0;
}

PF_API void pf_cli_spinner_stop(pf_cli_spinner_t *s, const char *message) {
    pf__cli_spinner_stop(s, NULL, 0, message);
}

PF_API void pf_cli_spinner_finish(
    pf_cli_spinner_t *s, pf_bool ok, const char *fmt, ...
) {
    char message[256];
    va_list args;

    message[0] = '\0';

    if (fmt) {
        va_start(args, fmt);
        vsnprintf(message, sizeof(message), fmt, args);
        va_end(args);
    }

    pf__cli_spinner_stop(
        s,
        ok ? "[ok]" : "[fail]",
        ok ? PF_FG_GREEN : PF_FG_RED,
        fmt ? message : NULL
    );
}

/* ------------------------------------------------------------------------ */
/* Progress bar                                                              */
/* ------------------------------------------------------------------------ */

PF_API void pf__cli_progress_default_render_cb(
    pf_cli_progress_t *p, struct pf_cli_progress_frame *f
) {
    pf_cli_t *c = p->cli;
    FILE *out = c->out.handle;

    pf_cli_clearline(c, PF_CLEAR_ALL);

    if (p->title)
        fprintf(out, "%s ", p->title);

    fputc('[', out);

    pf_cli_color(c, p->options.color);

    for (unsigned i = 0; i < f->width; ++i)
        fputc(i < f->filled ? p->options.fill : p->options.empty, out);

    pf_cli_color(c, PF_CLI_RESET);
    fputc(']', out);

    if (p->options.showPercentage)
        fprintf(out, " %3u%%", (unsigned)(f->ratio * 100.0 + 0.5));

    if (p->options.showValue)
        fprintf(out, " %.0f/%.0f", p->value, p->maximum);

    if (p->options.showRate)
        fprintf(out, " %.1f/s", f->rate);

    if (p->options.showEta && p->value < p->maximum) {
        fprintf(
            out,
            " ETA %02u:%02u",
            (unsigned)(f->eta / 60.0),
            (unsigned)f->eta % 60
        );
    }

    if (p->options.showElapsed) {
        fprintf(
            out,
            " %02u:%02u",
            (unsigned)(f->elapsed / 60.0),
            (unsigned)f->elapsed % 60
        );
    }

    fflush(out);
}

PF_API void pf__cli_progress_apply_defaults(
    pf_cli_t *c, struct pf_cli_progress_opt *o
) {
    if (o->width == 0)
        o->width = (unsigned)(c->out.columns / 4);
    if (o->empty == '\0')
        o->empty = '-';
    if (o->fill == '\0')
        o->fill = '#';
    if (o->color == 0)
        o->color = PF_FG_GREEN;
    if (!o->render)
        o->render = pf__cli_progress_default_render_cb;
}

PF_API void pf__cli_progress_render(pf_cli_progress_t *p) {
    pf_cli_t *c;
    struct pf_cli_progress_frame frame;
    double ratio, elapsed, rate;

    if (!p || !p->active || !p->cli)
        return;

    c = p->cli;

    if (!pf__cli_can_animate(c))
        return;

    ratio = p->value / p->maximum;

    if (ratio < 0.0)
        ratio = 0.0;

    if (ratio > 1.0)
        ratio = 1.0;

    elapsed = pf_cli_now() - p->startedAt;

    if (elapsed < 0.001)
        elapsed = 0.001;

    rate = p->value / elapsed;

    frame.width = p->options.width;
    frame.filled = (unsigned)(ratio * p->options.width);
    frame.ratio = ratio;
    frame.elapsed = elapsed;
    frame.rate = rate;
    frame.eta = rate > 0.0 ? (p->maximum - p->value) / rate : 0.0;

    p->lastRender = pf_cli_now();
    p->options.render(p, &frame);
}

PF_API int pf_cli_progress_start(
    pf_cli_progress_t *p,
    pf_cli_t *c,
    double maximum,
    const struct pf_cli_progress_opt *options
) {
    struct pf_cli_progress_opt opt;

    if (!p)
        return -1;

    memset(&opt, 0, sizeof(opt));
    if (options)
        opt = *options;

    memset(p, 0, sizeof(*p));
    p->cli = c;

    if (!c)
        return -1;

    pf__cli_progress_apply_defaults(c, &opt);

    p->maximum = maximum > 0.0 ? maximum : 1.0;
    p->options = opt;
    p->title = opt.title;

    p->startedAt = pf_cli_now();
    p->lastRender = p->startedAt;

    p->active = 1;

    return 0;
}

PF_API void pf_cli_progress_set_title(
    pf_cli_progress_t *p, const char *fmt, ...
) {
    va_list args;

    if (!p || !p->cli || !fmt)
        return;

    va_start(args, fmt);
    vsnprintf(p->titleBuf, sizeof(p->titleBuf), fmt, args);
    va_end(args);
    p->title = p->titleBuf;
}

PF_API void pf_cli_progress_render(pf_cli_progress_t *p) {
    if (!p || !p->cli)
        return;

    pf__cli_progress_render(p);
}

PF_API void pf_cli_progress_update(pf_cli_progress_t *p, double value) {
    if (!p || !p->cli)
        return;

    if (value < 0.0)
        value = 0.0;

    if (value > p->maximum)
        value = p->maximum;

    p->value = value;

    pf__cli_progress_render(p);
}

PF_API void pf_cli_progress_advance(pf_cli_progress_t *p, double delta) {
    double value;

    if (!p || !p->cli)
        return;

    value = p->value + delta;

    if (value < 0.0)
        value = 0.0;

    if (value > p->maximum)
        value = p->maximum;

    p->value = value;

    pf__cli_progress_render(p);
}

PF_API void pf__cli_progress_stop(pf_cli_progress_t *p, pf_bool complete) {
    pf_cli_t *c;

    if (!p || !p->cli)
        return;

    c = p->cli;

    if (c->live == p)
        c->live = NULL;

    if (p->active && !c->silent && c->out.handle) {
        if (complete) {
            p->value = p->maximum;
            pf__cli_progress_render(p);

            if (pf__cli_can_animate(c))
                fputc('\n', c->out.handle);
        } else if (pf__cli_can_animate(c)) {
            pf_cli_clearline(c, PF_CLEAR_ALL);
        }

        fflush(c->out.handle);
    }

    p->active = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_CLI */
