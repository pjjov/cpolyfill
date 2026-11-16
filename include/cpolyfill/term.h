/** cpolyfill - Filling the gaps between C standards and compilers

    Terminals: detection, size, raw input, and colour support, the same
    on POSIX terminals and the Windows console.

        int pf_isatty(int fd)
                POSIX isatty: 1, or 0 with errno ENOTTY or EBADF. On
                Windows only a console counts (the CRT's _isatty is also
                true for NUL and printers).
        bool pf_term_is_terminal(int fd)
                pf_isatty, or on Windows also the pipes that mintty and
                MSYS2 / Cygwin terminals give their programs.
        int pf_term_size(int fd, int *cols, int *rows)
                The size of the terminal in characters: 0, or PF_ENOTTY
                (or another PF_E* code). Either pointer may be NULL.

    Input modes (0, or PF_ENOTTY / a PF_E* code):

        int pf_term_raw(int fd, unsigned flags, pf_term_state_t *saved)
                Raw input on a terminal: every key is read as soon as it is
                typed, without echo or line editing, and Ctrl+C and the
                like arrive as bytes, not signals, unless `flags` has
                PF_TERM_KEEP_SIGNALS. Output is not changed ("\n" still
                starts a new line). On Windows the console sends keys as
                VT escape sequences, as terminals do. The previous mode is
                stored in `saved` (may be NULL).
        int pf_term_restore(int fd, const pf_term_state_t *saved)
                Back to the mode pf_term_raw saved.
        int pf_term_echo(int fd, bool on)
                Shows or hides what is typed (for passwords), keeping line
                editing.
        pf_term_state_t         Opaque, a fixed size on every platform.

    Output:

        int pf_term_enable_vt(int fd)
                Makes the terminal interpret VT / ANSI escape sequences
                (colours, cursor movement). Windows consoles need it; the
                original mode is put back when the program exits.
                Elsewhere 0 for a terminal, else PF_ENOTTY.
        int pf_term_set_utf8(void)
                Makes the Windows console read and write UTF-8 (code page
                65001), until the program exits. 0 elsewhere.
        int pf_term_colors(int fd)
                How many colours to use writing to `fd`: 0 (no escape
                sequences), 16, 256 or 16777216. In order: NO_COLOR (set
                and not empty) gives 0; FORCE_COLOR gives 0 for "0" or
                "false", 256 for "2", 16777216 for "3", 16 otherwise;
                CLICOLOR_FORCE (not "0") skips the terminal check below;
                CLICOLOR=0 gives 0; a file or pipe gives 0; TERM=dumb
                gives 0; COLORTERM truecolor or 24bit, or a TERM ending in
                -direct, gives 16777216; a TERM with 256color gives 256;
                anything else 16. The Windows console is 16777216 once
                pf_term_enable_vt succeeds (which this calls), 0 if it
                fails.

    Modes chosen by the user, for <cpolyfill/argparse.h> and
    <cpolyfill/cli.h>:

        pf_color_mode_t         PF_COLOR_AUTO, PF_COLOR_ALWAYS,
                                PF_COLOR_NEVER
        pf_interactive_mode_t   PF_INTERACTIVE_AUTO, _ALWAYS, _NEVER

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_TERM, on Windows:
    `isatty`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_TERM_H
#define CPOLYFILL_TERM_H

#include <cpolyfill/platform.h>

#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF_TERM_KEEP_SIGNALS 0x1u

/* When to use colours or prompts, as chosen by the user (--color, see
   <cpolyfill/argparse.h>): AUTO decides by the terminal. */
typedef enum pf_color_mode_t {
    PF_COLOR_AUTO,
    PF_COLOR_ALWAYS,
    PF_COLOR_NEVER
} pf_color_mode_t;

typedef enum pf_interactive_mode_t {
    PF_INTERACTIVE_AUTO,
    PF_INTERACTIVE_ALWAYS,
    PF_INTERACTIVE_NEVER
} pf_interactive_mode_t;

typedef struct pf_term_state_t {
    union {
        unsigned char data[256];
        uint64_t align;
        void *ptr;
    } pf__u;
} pf_term_state_t;

PF_BEGIN_DECLS

PF_API int pf_isatty(int fd);
PF_API bool pf_term_is_terminal(int fd);
PF_API int pf_term_size(int fd, int *cols, int *rows);
PF_API int pf_term_raw(int fd, unsigned flags, pf_term_state_t *saved);
PF_API int pf_term_restore(int fd, const pf_term_state_t *saved);
PF_API int pf_term_echo(int fd, bool on);
PF_API int pf_term_enable_vt(int fd);
PF_API int pf_term_set_utf8(void);
PF_API int pf_term_colors(int fd);

PF_END_DECLS

#endif /* CPOLYFILL_TERM_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__TERM_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_TERM)) \
        || defined(PF_STD_NAMES_TERM))
    #define PF__TERM_STD_NAMES

    #if PF_OS_WINDOWS
        #undef isatty
        #define isatty pf_isatty
    #endif
#endif
