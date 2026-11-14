/** cpolyfill - Filling the gaps between C standards and compilers

    Command line parsing driven by a callback. pf_argparser_run calls it
    once for every option and parameter; the callback tries matchers in
    turn, and the first one the argument belongs to stores its value and
    returns true. An option no matcher took is reported as unknown.

        static int on_arg(pf_argparser_t *p, void *user) {
            struct config *c = user;
            if (pf_option_common(p, PF_COMMON_ALL))
                return 0;
            if (pf_option_bool(p, "all", 'a', &c->all))
                return 0;
            if (pf_option_int_range(p, "jobs", 'j', 1, 64, &c->jobs))
                return 0;
            return 0;   // parameters are collected in p->paramv
        }

        pf_argparser_t p;
        pf_argparser_init(&p, argc, argv);
        rc = pf_argparser_run(&p, on_arg, &config);
        ... p.paramc, p.paramv, p.help, p.verbosity ...
        pf_argparser_free(&p);

    Syntax, as getopt_long:
    - `--name`, `--name=value`, `--name value` (for options taking one).
    - Short options cluster: `-abc` is `-a -b -c`. An option taking a
      value gets the rest of the cluster or else the next argument: `-n5`,
      `-n 5`, `-abVALUE`, and `-n=5`. A flag may take an explicit value
      after `=` (`-a=no`), a count digits (`-v2`).
    - `--` ends the options; `-` alone is a parameter.
    - A value option takes the next argument even if it starts with `-`.
    - Integers are decimal, or hexadecimal / binary with `0x` / `0b` (a
      leading zero does not mean octal); floating-point numbers always use
      `.`, whatever the locale.

    Parser (pf_argparser_t; set the public fields between init and run):

        int pf_argparser_init(pf_argparser_t *p, int argc, char **argv)
                0 or PF_E*. argv is never modified. On Windows, when
                `argv` is the C runtime's own (`__argv`, what main
                received), the UTF-8 arguments of pf_args_utf8 are parsed
                instead.
        void pf_argparser_free(pf_argparser_t *p)
        int pf_argparser_run(pf_argparser_t *p, pf_argparser_fn cb,
                             void *user)
                Calls `cb(p, user)` for each argument. 0, PF_EINVAL when
                errors were reported (p->failed), or what the callback
                returned; a callback returning PF_EINTR stops the run
                quietly (0, or PF_EINVAL after errors).
        void pf_argparser_error(pf_argparser_t *p, const char *fmt, ...)
        void pf_argparser_verror(pf_argparser_t *p, const char *fmt,
                                 va_list ap)
                Reports an error and fails the run. While --help was
                given nothing fails (the help wins); with --quiet it
                fails without printing.
        void pf_argparser_set_error_fn(pf_argparser_t *p,
                                       pf_argparser_error_fn fn,
                                       void *user)
                Errors go to `fn(p, message, user)` (the message without
                prefix or newline) instead of `err_prefix` + message to
                `err`. NULL restores that.
        void pf_argparser_set_short(pf_argparser_t *p, unsigned common,
                                    char c)
                The short name of the common options in `common`
                (PF_COMMON_*), 0 for none. Defaults: help '?', version 'V',
                verbose 'v', quiet 'q', interactive 'i', color none.

        Public fields:  out, err (FILE *, default stdout / stderr),
                        err_prefix, allow_unknown (no error for unknown
                        options); after the run: paramc, paramv (the
                        parameters, minus consumed ones), failed, and what
                        the common options set: help, version, silent
                        (--quiet), verbosity, color_mode + color_set,
                        interactive_mode + interactive_set.

    Options (true when the current argument was this option; on a bad
    value an error is reported, false returned and `*out` left alone):

        pf_option_string(p, name, c, char **out)
        pf_option_bool(p, name, c, bool *out)     --x, --x=yes|no|true|
                                                  false|on|off|1|0
        pf_option_toggle(p, name, c, bool *out)   also --no-x
        pf_option_count(p, name, c, int *out)     each -v adds one,
                                                  --x=N / -vN set it
        pf_option_int(p, name, c, int *out)
        pf_option_int_range(p, name, c, int min, int max, int *out)
        pf_option_long(p, name, c, long *out)
        pf_option_long_range(p, name, c, long min, long max, long *out)
        pf_option_double(p, name, c, double *out)
        pf_option_float(p, name, c, float *out)
        pf_option_enum(p, name, c, const pf_option_enum_t *values,
                       int *out)
        pf_option_flags(p, name, c, values, int *out)
                                                  --x=a|b,c, OR-ed in
        bool pf_option_is(p)                      the argument is an option
                `name` (long, NULL for none) and `c` (short, 0 for none).
                pf_option_enum_t is { name, value }, ended by a NULL name.

    Common options (results also in the parser's fields; `out` may be
    NULL):

        pf_option_help(p, bool *out)              --help, -?
        pf_option_version(p, bool *out)           --version, -V
        pf_option_verbose(p, int *out)            -v, -vv, --verbose[=N],
                                                  --no-verbose
        pf_option_quiet(p, bool *out)             --quiet, -q, --no-quiet
        pf_option_color(p, pf_color_mode_t *out)  --color[=auto|always|
                                                  never], --no-color
        pf_option_interactive(p, pf_interactive_mode_t *out)
                                                  -i, --interactive[=...],
                                                  --no-interactive
        bool pf_option_common(p, unsigned mask)   tries those in `mask`
                                                  (PF_COMMON_HELP, ...,
                                                  PF_COMMON_ALL)
        void pf_argparser_help_options(p, unsigned mask)
                Help lines for them on `out` ("  -v, --verbose   ...").
        void pf_argparser_print_version(p, const char *program,
                                        const char *version)
                "program version" on `out`.

    Parameters:

        bool pf_param_is(p, int index)           the argument is a
                                                  parameter (the index-th,
                                                  or any for -1)
        bool pf_param_equals(p, const char *s)
        bool pf_param_match(p, const char *const *values, char **out)
        bool pf_param_enum(p, int index, const char *what, values,
                           int *out)
        bool pf_param_subcommand(p, const char *what, values, int *out)
                The first parameter is a subcommand: looked up in `values`
                and removed from paramv, which then holds its arguments.
        void pf_param_consume(p)                  leave this parameter
                                                  out of paramv
        bool pf_param_expect(p, int min, int max, const char *what)
                After the run: checks paramc (`max` < 0: no limit).

    Values (for parameters, or anything else; `what` names the value in
    messages):

        bool pf_arg_long(p, const char *s, const char *what, long min,
                         long max, long *out)
        bool pf_arg_int(p, s, what, int min, int max, int *out)
        bool pf_arg_double(p, s, what, double *out)
        bool pf_match_enum(const char *s, values, int *out)
                Without reporting anything.

    Windows arguments:

        int pf_args_utf8(int *argc, char ***argv)
                Replaces `*argc` / `*argv` with the program's arguments in
                UTF-8 (from GetCommandLineW), kept until the program ends.
                Elsewhere it does nothing. 0 or PF_E*.

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

#ifndef CPOLYFILL_ARGPARSE_H
#define CPOLYFILL_ARGPARSE_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>
#include <cpolyfill/term.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

#define PF_COMMON_HELP 0x01u
#define PF_COMMON_VERBOSE 0x02u
#define PF_COMMON_QUIET 0x04u
#define PF_COMMON_COLOR 0x08u
#define PF_COMMON_VERSION 0x10u
#define PF_COMMON_INTERACTIVE 0x20u
#define PF_COMMON_ALL 0x3Fu

PF_BEGIN_DECLS

typedef struct pf_argparser_t pf_argparser_t;

typedef int (*pf_argparser_fn)(pf_argparser_t *p, void *user);
typedef void (*pf_argparser_error_fn)(
    pf_argparser_t *p, const char *message, void *user
);

typedef struct pf_option_enum_t {
    const char *name;
    int value;
} pf_option_enum_t;

struct pf_argparser_t {
    FILE *out;
    FILE *err;
    const char *err_prefix;
    bool allow_unknown;

    int paramc;
    char **paramv;

    bool failed;
    bool help;
    bool version;
    bool silent;
    int verbosity;
    pf_color_mode_t color_mode;
    bool color_set;
    pf_interactive_mode_t interactive_mode;
    bool interactive_set;

    /* Private. */
    int pf__argc;
    char **pf__argv;
    int pf__index;
    bool pf__options_done;
    bool pf__subcommand_seen;
    char pf__shorts[6];
    pf_argparser_error_fn pf__error_fn;
    void *pf__error_user;
    struct {
        int type;
        const char *name; /* long: after "--", `name_len` bytes */
        size_t name_len;
        char short_name;
        const char *rest; /* short: what follows in the cluster */
        const char *value;
        bool has_value;
        bool matched;
        bool consume;
        bool rest_used;
    } pf__item;
};

PF_API int pf_argparser_init(pf_argparser_t *p, int argc, char **argv);
PF_API void pf_argparser_free(pf_argparser_t *p);
PF_API int pf_argparser_run(pf_argparser_t *p, pf_argparser_fn cb, void *user);
PF_PRINTF(2, 3)
PF_API void pf_argparser_error(pf_argparser_t *p, const char *fmt, ...);
PF_PRINTF(2, 0)
PF_API void pf_argparser_verror(pf_argparser_t *p, const char *fmt, va_list ap);
PF_API void pf_argparser_set_error_fn(
    pf_argparser_t *p, pf_argparser_error_fn fn, void *user
);
PF_API void pf_argparser_set_short(pf_argparser_t *p, unsigned common, char c);

PF_API bool pf_option_is(pf_argparser_t *p);
PF_API bool pf_option_string(
    pf_argparser_t *p, const char *name, char c, char **out
);
PF_API bool pf_option_bool(
    pf_argparser_t *p, const char *name, char c, bool *out
);
PF_API bool pf_option_toggle(
    pf_argparser_t *p, const char *name, char c, bool *out
);
PF_API bool pf_option_count(
    pf_argparser_t *p, const char *name, char c, int *out
);
PF_API bool pf_option_int(
    pf_argparser_t *p, const char *name, char c, int *out
);
PF_API bool pf_option_int_range(
    pf_argparser_t *p, const char *name, char c, int min, int max, int *out
);
PF_API bool pf_option_long(
    pf_argparser_t *p, const char *name, char c, long *out
);
PF_API bool pf_option_long_range(
    pf_argparser_t *p, const char *name, char c, long min, long max, long *out
);
PF_API bool pf_option_double(
    pf_argparser_t *p, const char *name, char c, double *out
);
PF_API bool pf_option_float(
    pf_argparser_t *p, const char *name, char c, float *out
);
PF_API bool pf_option_enum(
    pf_argparser_t *p,
    const char *name,
    char c,
    const pf_option_enum_t *values,
    int *out
);
PF_API bool pf_option_flags(
    pf_argparser_t *p,
    const char *name,
    char c,
    const pf_option_enum_t *values,
    int *out
);

PF_API bool pf_option_help(pf_argparser_t *p, bool *out);
PF_API bool pf_option_version(pf_argparser_t *p, bool *out);
PF_API bool pf_option_verbose(pf_argparser_t *p, int *out);
PF_API bool pf_option_quiet(pf_argparser_t *p, bool *out);
PF_API bool pf_option_color(pf_argparser_t *p, pf_color_mode_t *out);
PF_API bool pf_option_interactive(
    pf_argparser_t *p, pf_interactive_mode_t *out
);
PF_API bool pf_option_common(pf_argparser_t *p, unsigned mask);
PF_API void pf_argparser_help_options(pf_argparser_t *p, unsigned mask);
PF_API void pf_argparser_print_version(
    pf_argparser_t *p, const char *program, const char *version
);

PF_API bool pf_param_is(pf_argparser_t *p, int index);
PF_API bool pf_param_equals(pf_argparser_t *p, const char *s);
PF_API bool pf_param_match(
    pf_argparser_t *p, const char *const *values, char **out
);
PF_API bool pf_param_enum(
    pf_argparser_t *p,
    int index,
    const char *what,
    const pf_option_enum_t *values,
    int *out
);
PF_API bool pf_param_subcommand(
    pf_argparser_t *p,
    const char *what,
    const pf_option_enum_t *values,
    int *out
);
PF_API void pf_param_consume(pf_argparser_t *p);
PF_API bool pf_param_expect(
    pf_argparser_t *p, int min, int max, const char *what
);

PF_API bool pf_arg_long(
    pf_argparser_t *p,
    const char *s,
    const char *what,
    long min,
    long max,
    long *out
);
PF_API bool pf_arg_int(
    pf_argparser_t *p,
    const char *s,
    const char *what,
    int min,
    int max,
    int *out
);
PF_API bool pf_arg_double(
    pf_argparser_t *p, const char *s, const char *what, double *out
);
PF_API bool pf_match_enum(
    const char *s, const pf_option_enum_t *values, int *out
);

PF_API int pf_args_utf8(int *argc, char ***argv);

PF_END_DECLS

#endif /* CPOLYFILL_ARGPARSE_H */
