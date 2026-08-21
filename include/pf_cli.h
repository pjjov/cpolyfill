/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides functions for building command line interfaces.

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

#ifndef POLYFILL_CLI
#define POLYFILL_CLI

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
    #define PF_CLI_WINDOWS 1
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
};

struct pf_cli_spinner_opt {
    const char *title;
    const char *frames;
    pf_cli_spinner_fn *render;
};

struct pf_cli_spinner_t {
    pf_cli_t *cli;

    size_t frame;
    double startedAt;

    int active;
    int frameCount;

    struct pf_cli_spinner_opt options;
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
};

PF_API double pf_cli_now(void) {
    struct timespec ts;

    if (timespec_get(&ts, TIME_UTC) != TIME_UTC)
        return 0.0;

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

PF_API int pf__cli_is_terminal(FILE *handle) {
    if (!handle)
        return 0;

#if PF_CLI_WINDOWS
    return _isatty(_fileno(handle));
#else
    return isatty(fileno(handle));
#endif
}

PF_API int pf__cli_color_enabled(const pf_cli_t *c, FILE *handle) {
    if (!c)
        return 0;

    if (c->colorMode == PF_COLOR_ALWAYS)
        return 1;

    if (c->colorMode == PF_COLOR_NEVER)
        return 0;

    if (getenv("NO_COLOR"))
        return 0;

    return pf__cli_is_terminal(handle);
}

PF_API unsigned pf__cli_columns(const pf_cli_t *c, FILE *handle) {
    unsigned fallback = 80;

    const char *env = getenv("COLUMNS");

    if (env && *env) {
        char *end = NULL;
        unsigned long n = strtoul(env, &end, 10);

        if (end != env && n >= 20 && n <= 1000)
            return (unsigned)n;
    }

#if PF_CLI_WINDOWS

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

PF_API void pf__cli_init_stream(
    pf_cli_t *c, pf_cli_stream_t *stream, FILE *handle
) {
    if (!handle) {
        memset(stream, 0, sizeof(*stream));
        return;
    }

    stream->handle = handle;
    stream->colorEnabled = pf__cli_color_enabled(c, handle);
    stream->isTerminal = pf__cli_is_terminal(handle);
    stream->columns = pf__cli_columns(c, handle);
    stream->supportsAnsi = stream->colorEnabled;
    stream->supportsCursor = stream->supportsAnsi;
    stream->supportsHyperlinks = stream->supportsAnsi
        && !getenv("NO_HYPERLINKS");
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
    if (!c)
        return;

    struct pf_cli_opt fallback = { 0 };
    o = pf__cli_apply_defaults(o ? o : &fallback);

    c->definitionIndent = o->definitionIndent;
    c->silent = PF_FALSE;
    c->colorMode = o->colorMode;

    pf__cli_init_stream(c, &c->out, o->out);
    pf__cli_init_stream(c, &c->err, o->err);
    pf__cli_init_stream(c, &c->in, o->in);

    if (!c->out.handle)
        c->silent = PF_TRUE;
    c->isInteractive = pf__cli_is_interactive(c);
}

PF_API void pf_cli_color(pf_cli_t *c, int color) {
    if (!c || c->silent)
        return;

    if (!c->out.colorEnabled)
        return;

    const char *fmt = "\033[%dm";
    FILE *out = c->out.handle;

    if (color & PF_CLI_RESET)
        fprintf(out, fmt, 0);
    if (color & PF_CLI_BOLD)
        fprintf(out, fmt, 1);
    if (color & PF_CLI_DIM)
        fprintf(out, fmt, 2);
    if (color & PF__CLI_COLOR_MASK)
        fprintf(out, fmt, color & PF__CLI_COLOR_MASK);
}

PF_API void pf_cli_clearline(pf_cli_t *c, enum pf_clear_mode mode) {
    if (!c || c->silent || !c->out.supportsAnsi)
        return;
    if (mode < 0 || mode > 2)
        return;

    char seq[] = { '\r', '\033', '[', mode, 'K', '\0' };
    fputs(seq, c->out.handle);
}

PF_API void pf_cli_clear(pf_cli_t *c, enum pf_clear_mode mode) {
    if (!c || c->silent || !c->out.supportsAnsi)
        return;
    if (mode < 0 || mode > 3)
        return;

    char seq[] = { '\r', '\033', '[', mode, 'J', '\0' };
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

PF_API void pf_cli_position(pf_cli_t *c, int x, int y) {
    if (!c || c->silent || !c->out.supportsAnsi)
        return;

    if (x < 0 || y < 0)
        return;

    fprintf(c->out.handle, "\033[%d;%dH", x + 1, y + 1);
}

PF_API void pf_cli_vfprintf(
    pf_cli_t *c, pf_cli_stream_t *stream, const char *fmt, va_list args
) {
    if (!c || !stream || !fmt || c->silent)
        return;

    vfprintf(stream->handle, fmt, args);
}

PF_API void pf_cli_cprintf(pf_cli_t *c, int color, const char *fmt, ...) {
    pf_cli_color(c, color);

    va_list args;
    va_start(args, fmt);
    pf_cli_vfprintf(c, &c->out, fmt, args);
    va_end(args);

    pf_cli_color(c, PF_CLI_RESET);
}

PF_API void pf_cli_printf(pf_cli_t *c, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pf_cli_vfprintf(c, &c->out, fmt, args);
    va_end(args);
}

PF_API void pf_cli_errorf(pf_cli_t *c, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pf_cli_vfprintf(c, &c->err, fmt, args);
    va_end(args);
}

PF_API void pf_cli_fill(FILE *out, unsigned n, char c) {
    if (!out || n == 0)
        return;

    while (n--)
        fputc(' ', out);
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
        width = c->out.columns;

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

    columns = c->out.columns;

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

        pf__cli_wrap_at(c, indent + 4, indent + 4, columns, description);

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

        pf__cli_wrap_at(c, indent + 4, indent + 4, columns, description);

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
    if (!c || c->silent)
        return 0;

    FILE *out = c->out.handle;
    fputs(question ? question : "", out);
    fputs(": ", out);
    fflush(out);

    return pf_cli_readline(c, answer, answerSize);
}

PF_API int pf_cli_confirm(pf_cli_t *c, const char *question, int defaultValue) {
    if (!c || c->silent)
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

PF_API void pf__cli_spinner_default_render_cb(pf_cli_spinner_t *s) {
    pf_cli_t *c = s->cli;

    pf_cli_clearline(c, PF_CLEAR_ALL);

    fputc(s->options.frames[s->frame], c->out.handle);

    if (s->options.title)
        fprintf(c->out.handle, " %s", s->options.title);

    fflush(c->out.handle);

    s->frame = (s->frame + 1) % s->frameCount;
}

PF_API struct pf_cli_spinner_opt *pf__cli_spinner_apply_defaults(
    struct pf_cli_spinner_opt *opt
) {
    if (!opt->frames)
        opt->frames = "|/-\\";
    if (!opt->render)
        opt->render = pf__cli_spinner_default_render_cb;
    return opt;
}

PF_API void pf_cli_spinner_start(
    pf_cli_spinner_t *s, pf_cli_t *c, struct pf_cli_spinner_opt *options
) {
    struct pf_cli_spinner_opt fallback;
    options = pf__cli_spinner_apply_defaults(options ? options : &fallback);

    memset(s, 0, sizeof(*s));

    s->cli = c;
    s->startedAt = pf_cli_now();
    s->active = 1;
    s->options = *options;
    s->frameCount = strlen(s->options.frames);
}

PF_API void pf_cli_spinner_tick(pf_cli_spinner_t *s) {
    if (!s || !s->active || !s->cli)
        return;

    /*
     * Don't animate redirected output.
     */
    if (!s->cli->isInteractive)
        return;

    s->options.render(s);
}

PF_API void pf_cli_spinner_end(pf_cli_spinner_t *s, const char *message) {
    if (!s || !s->cli)
        return;

    if (s->cli->isInteractive)
        pf_cli_clearline(s->cli, PF_CLEAR_ALL);
    s->active = 0;
}

PF_API void pf__cli_progress_default_render_cb(
    pf_cli_progress_t *p, struct pf_cli_progress_frame *f
) {
    pf_cli_t *c = p->cli;
    FILE *out = c->out.handle;

    pf_cli_clear(c, PF_CLEAR_ALL);

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

PF_API struct pf_cli_progress_opt *pf__cli_progress_apply_defaults(
    pf_cli_t *c, struct pf_cli_progress_opt *o
) {
    if (o->width == 0)
        o->width = c->out.columns / 4;
    if (o->empty == '\0')
        o->empty = '-';
    if (o->fill == '\0')
        o->fill = '#';
    if (o->color == 0)
        o->color = PF_FG_GREEN;
    if (!o->render)
        o->render = pf__cli_progress_default_render_cb;
    return o;
}

PF_API void pf_cli_progress_start(
    pf_cli_progress_t *p,
    pf_cli_t *c,
    double maximum,
    struct pf_cli_progress_opt *options
) {
    struct pf_cli_progress_opt fallback = { 0 };
    options = pf__cli_progress_apply_defaults(c, options ? options : &fallback);

    memset(p, 0, sizeof(*p));

    p->cli = c;
    p->maximum = maximum > 0.0 ? maximum : 1.0;
    p->options = *options;

    p->startedAt = pf_cli_now();
    p->lastRender = p->startedAt;

    p->active = 1;
}

PF_API void pf_cli_progress_render(pf_cli_progress_t *p) {
    pf_cli_t *c;
    unsigned width;
    unsigned filled;
    double ratio;
    double elapsed;
    double rate;
    double eta;

    if (!p || !p->active || !p->cli)
        return;

    c = p->cli;

    if (c->silent || !c->isInteractive)
        return;

    width = p->options.width;

    ratio = p->value / p->maximum;

    if (ratio < 0.0)
        ratio = 0.0;

    if (ratio > 1.0)
        ratio = 1.0;

    filled = (unsigned)(ratio * width);

    elapsed = pf_cli_now() - p->startedAt;

    if (elapsed < 0.001)
        elapsed = 0.001;

    rate = p->value / elapsed;

    eta = rate > 0.0 ? (p->maximum - p->value) / rate : 0.0;

    struct pf_cli_progress_frame frame;
    frame.width = width;
    frame.filled = filled;
    frame.ratio = ratio;
    frame.elapsed = elapsed;
    frame.rate = rate;
    frame.eta = eta;
    p->options.render(p, &frame);
}

PF_API void pf_cli_progress_update(pf_cli_progress_t *p, double value) {
    if (!p)
        return;

    if (value < 0.0)
        value = 0.0;

    if (value > p->maximum)
        value = p->maximum;

    p->value = value;
    pf_cli_progress_render(p);
}

PF_API void pf_cli_progress_end(pf_cli_progress_t *p) {
    if (!p)
        return;

    p->value = p->maximum;
    pf_cli_progress_render(p);

    if (p->cli && !p->cli->silent && p->cli->isInteractive)
        fputc('\n', p->cli->out.handle);

    p->active = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_CLI */
