/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a simple CLI argument parsing.

    If "pf_cli.h" is included BEFORE this header, the parser can be bound to
    a pf_cli_t (see pf_argparser_set_cli()). Errors are then printed through
    the CLI (colors, --quiet) and the common options (--verbose, --quiet,
    --color, --interactive) are applied to it automatically.

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

#ifndef POLYFILL_ARGPARSE
#define POLYFILL_ARGPARSE

#ifndef PF_API
    #define PF_API static inline
#endif

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PF_PRINTF
    #if defined(__GNUC__) || defined(__clang__)
        #define PF_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
    #else
        #define PF_PRINTF(fmt, args)
    #endif
#endif

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

/* Shared with pf_cli.h, which can be used without pf_argparse.h. */
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

/*
 * Default short names of the common options. Define any of these to a
 * different character (or to 0 for "long form only") before including
 * this header.
 */
#ifndef PF_ARGPARSE_HELP_SHORT
    #define PF_ARGPARSE_HELP_SHORT '?'
#endif
#ifndef PF_ARGPARSE_VERBOSE_SHORT
    #define PF_ARGPARSE_VERBOSE_SHORT 'v'
#endif
#ifndef PF_ARGPARSE_QUIET_SHORT
    #define PF_ARGPARSE_QUIET_SHORT 'q'
#endif
#ifndef PF_ARGPARSE_VERSION_SHORT
    #define PF_ARGPARSE_VERSION_SHORT 'V'
#endif
#ifndef PF_ARGPARSE_COLOR_SHORT
    #define PF_ARGPARSE_COLOR_SHORT 0
#endif
#ifndef PF_ARGPARSE_INTERACTIVE_SHORT
    #define PF_ARGPARSE_INTERACTIVE_SHORT 'i'
#endif

struct pf_cli_t; /* defined by pf_cli.h; the tag matches its typedef */

/*
 * Return codes. A callback may return PF_ARGPARSE_EINTR to stop parsing;
 * if it has reported errors with pf_argparser_error(), pf_argparser_run()
 * then returns PF_ARGPARSE_EINVAL.
 */
enum pf_argparse_error {
    PF_ARGPARSE_OK = 0,
    PF_ARGPARSE_EINTR = -4,
    PF_ARGPARSE_ENOMEM = -12,
    PF_ARGPARSE_EINVAL = -22,
};

/* Bit mask for pf_option_common() and pf_argparser_help_options(). */
enum pf_common_option {
    PF_COMMON_HELP = 1 << 0,
    PF_COMMON_VERBOSE = 1 << 1,
    PF_COMMON_QUIET = 1 << 2,
    PF_COMMON_COLOR = 1 << 3,
    PF_COMMON_VERSION = 1 << 4,
    PF_COMMON_INTERACTIVE = 1 << 5,
    PF_COMMON_ALL = 0x3F,
};

enum pf__arg_type {
    PF__ARG_NONE,
    PF__ARG_PARAM,
    PF__ARG_SHORT,
    PF__ARG_LONG,
};

struct pf_argparser {
    FILE *out;
    FILE *err;
    const char *errPrefix;

    /* Optional; only used when pf_cli.h was included first. */
    struct pf_cli_t *cli;

    /* Positional parameters (subcommands consumed by the callback excluded) */
    int paramc;
    char **paramv;

    /* State set by the common options (see pf_option_common()). */
    pf_bool verbose; /* verbosity > 0 */
    pf_bool silent;  /* --quiet: no error printing */
    pf_bool help;    /* --help: also mutes further errors */
    pf_bool failed;
    pf_bool version;
    int verbosity;
    enum pf_color_mode_t colorMode;
    enum pf_interactive_mode_t interactiveMode;
    pf_bool colorSet;       /* --color was given */
    pf_bool interactiveSet; /* --interactive was given */

    /* Don't report options that the callback did not recognise. */
    pf_bool allowUnknown;

    /* private */

    pf_bool pauseOptions;
    pf_bool subcommandSeen;
    int index;
    int argc;
    char **argv;

    struct {
        enum pf__arg_type type;

        const char *name;
        const char *value;
        const char *param;
        char shorthand;
        pf_bool hasValue;
        pf_bool matched; /* some pf_option_* recognised this item */
        pf_bool consume; /* drop this parameter from paramv */
    } item;
};

typedef struct pf_argparser pf_argparser_t;

typedef int (*pf_argparser_fn)(struct pf_argparser *p, void *user);

typedef struct pf_option_enum_t {
    const char *name;
    int value;
} pf_option_enum_t;

/* ------------------------------------------------------------------------ */
/* Lifetime                                                                  */
/* ------------------------------------------------------------------------ */

PF_API int pf_argparser_init(struct pf_argparser *p, int argc, char *argv[]) {
    if (!p || !argv || argc < 0)
        return PF_ARGPARSE_EINVAL;

    memset(p, 0, sizeof(*p));

    /* At most `argc` parameters; never ask malloc for 0 bytes. */
    p->paramv = (char **)malloc(((size_t)argc + 1) * sizeof(*p->paramv));
    p->out = stdout;
    p->err = stderr;
    p->argc = argc;
    p->argv = argv;

    if (!p->paramv)
        return PF_ARGPARSE_ENOMEM;
    return PF_ARGPARSE_OK;
}

PF_API void pf_argparser_free(struct pf_argparser *p) {
    if (p && p->paramv) {
        free(p->paramv);
        p->paramv = NULL;
    }
}

/*
 * Binds the parser to a pf_cli_t. Without pf_cli.h this only stores the
 * pointer. With it, the parser takes over the stream handles and the
 * verbosity of the CLI, and prints errors through it.
 */
PF_API void pf_argparser_set_cli(struct pf_argparser *p, struct pf_cli_t *cli) {
    if (!p)
        return;

    p->cli = cli;

#ifdef POLYFILL_CLI
    if (cli) {
        if (cli->out.handle)
            p->out = cli->out.handle;
        if (cli->err.handle)
            p->err = cli->err.handle;
        p->verbosity = cli->verbosity;
        p->verbose = p->verbosity > 0;
    }
#endif
}

/* ------------------------------------------------------------------------ */
/* Errors                                                                    */
/* ------------------------------------------------------------------------ */

/*
 * Marks the parse as failed and prints the message. While --help is active
 * nothing fails (the help text wins); with --quiet the parse still fails,
 * it just doesn't print anything.
 */
PF_API void pf_argparser_verror(
    struct pf_argparser *p, const char *fmt, va_list args
) {
    if (!p || !fmt || p->help)
        return;

    p->failed = PF_TRUE;

    if (p->silent)
        return;

#ifdef POLYFILL_CLI
    if (p->cli) {
        if (p->errPrefix) {
            pf_cli_ecprintf(
                p->cli, PF_FG_RED | PF_CLI_BOLD, "%s", p->errPrefix
            );
        }
        pf_cli_vcprintf(p->cli, &p->cli->err, 0, fmt, args);
        pf_cli_errorf(p->cli, "\n");
        return;
    }
#endif

    if (!p->err)
        return;

    if (p->errPrefix)
        fputs(p->errPrefix, p->err);

    vfprintf(p->err, fmt, args);
    fputc('\n', p->err);
}

PF_API void pf_argparser_error(
    struct pf_argparser *p, const char *fmt, ...
) PF_PRINTF(2, 3);

PF_API void pf_argparser_error(struct pf_argparser *p, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    pf_argparser_verror(p, fmt, args);
    va_end(args);
}

/* ------------------------------------------------------------------------ */
/* Positional parameters                                                     */
/* ------------------------------------------------------------------------ */

/* Is the current item a parameter? With index >= 0: the index-th one. */
PF_API pf_bool pf_is_param(struct pf_argparser *p, int index) {
    if (!p || p->item.type != PF__ARG_PARAM)
        return PF_FALSE;
    return index < 0 || p->paramc == index + 1;
}

PF_API pf_bool pf_is_option(struct pf_argparser *p) {
    return p && (p->item.type == PF__ARG_SHORT || p->item.type == PF__ARG_LONG);
}

PF_API pf_bool pf_cmp_param(struct pf_argparser *p, const char *other) {
    if (!p || !other)
        return PF_FALSE;
    if (p->item.type != PF__ARG_PARAM)
        return PF_FALSE;
    return strcmp(p->item.param, other) == 0;
}

PF_API pf_bool
pf_match_param(struct pf_argparser *p, const char *const *values, char **out) {
    if (!p || !values || !out || p->item.type != PF__ARG_PARAM)
        return PF_FALSE;

    for (int i = 0; values[i]; i++) {
        if (strcmp(p->item.param, values[i]) == 0) {
            *out = (char *)p->item.param;
            return PF_TRUE;
        }
    }

    return PF_FALSE;
}

/*
 * Removes the current parameter from paramv/paramc. Useful for tokens the
 * callback handles itself, such as a subcommand name.
 */
PF_API void pf_argparser_consume(struct pf_argparser *p) {
    if (p && p->item.type == PF__ARG_PARAM)
        p->item.consume = PF_TRUE;
}

/*
 * Checks the number of collected parameters. `max < 0` means unlimited.
 * `what` names the thing being checked, e.g. "subcommand 'remap'".
 */
PF_API pf_bool pf_argparser_expect_params(
    struct pf_argparser *p, int min, int max, const char *what
) {
    if (!p)
        return PF_FALSE;
    if (!what)
        what = "command";

    if (max == 0 && p->paramc > 0) {
        pf_argparser_error(p, "%s takes no arguments; got %d", what, p->paramc);
        return PF_FALSE;
    }

    if (min == max && p->paramc != min) {
        pf_argparser_error(
            p,
            "%s requires exactly %d argument%s; got %d",
            what,
            min,
            min == 1 ? "" : "s",
            p->paramc
        );
        return PF_FALSE;
    }

    if (p->paramc < min) {
        pf_argparser_error(
            p,
            "%s requires at least %d argument%s; got %d",
            what,
            min,
            min == 1 ? "" : "s",
            p->paramc
        );
        return PF_FALSE;
    }

    if (max >= 0 && p->paramc > max) {
        pf_argparser_error(
            p,
            "%s takes at most %d argument%s; got %d",
            what,
            max,
            max == 1 ? "" : "s",
            p->paramc
        );
        return PF_FALSE;
    }

    return PF_TRUE;
}

/*
 * Splits `*cursor` at the next `delim` in place and returns the token
 * (possibly empty). Returns NULL once the string is used up:
 *
 *   char *s = "a+b+c", *tok;       // (writable copy)
 *   while ((tok = pf_next_token(&s, '+'))) ...
 */
PF_API char *pf_next_token(char **cursor, char delim) {
    char *start, *end;

    if (!cursor || !*cursor)
        return NULL;

    start = *cursor;
    end = strchr(start, delim);

    if (end) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = NULL;
    }

    return start;
}

/* ------------------------------------------------------------------------ */
/* Value conversion (usable for options and for positional parameters)       */
/* ------------------------------------------------------------------------ */

/*
 * Converts `str` to an integer in [min, max]. `what` is used in messages
 * ("control id" -> "control id must be between 0 and 65535; got '70000'").
 */
PF_API pf_bool pf_arg_long(
    struct pf_argparser *p,
    const char *str,
    const char *what,
    long min,
    long max,
    long *out
) {
    char *end;
    long v;

    if (!p || !str || !out)
        return PF_FALSE;
    if (!what)
        what = "value";

    errno = 0;
    v = strtol(str, &end, 0);

    if (end == str || *end) {
        pf_argparser_error(p, "invalid integer '%s' for %s", str, what);
        return PF_FALSE;
    }

    if (errno == ERANGE || v < min || v > max) {
        if (min == LONG_MIN && max == LONG_MAX) {
            pf_argparser_error(p, "%s is out of range: '%s'", what, str);
        } else {
            pf_argparser_error(
                p,
                "%s must be between %ld and %ld; got '%s'",
                what,
                min,
                max,
                str
            );
        }
        return PF_FALSE;
    }

    *out = v;
    return PF_TRUE;
}

PF_API pf_bool pf_arg_int(
    struct pf_argparser *p,
    const char *str,
    const char *what,
    int min,
    int max,
    int *out
) {
    long v;

    if (!out || !pf_arg_long(p, str, what, min, max, &v))
        return PF_FALSE;

    *out = (int)v;
    return PF_TRUE;
}

PF_API pf_bool pf_arg_double(
    struct pf_argparser *p, const char *str, const char *what, double *out
) {
    char *end;
    double v;

    if (!p || !str || !out)
        return PF_FALSE;
    if (!what)
        what = "value";

    errno = 0;
    v = strtod(str, &end);

    if (end == str || *end) {
        pf_argparser_error(p, "invalid number '%s' for %s", str, what);
        return PF_FALSE;
    }

    if (v > DBL_MAX || v < -DBL_MAX) {
        pf_argparser_error(p, "%s is out of range: '%s'", what, str);
        return PF_FALSE;
    }

    *out = v;
    return PF_TRUE;
}

PF_API pf_bool
pf_match_enum(const char *name, const pf_option_enum_t *values, int *out) {
    if (!name || !values || !out)
        return PF_FALSE;

    for (int i = 0; values[i].name; i++) {
        if (0 == strcmp(name, values[i].name)) {
            *out = values[i].value;
            return PF_TRUE;
        }
    }

    return PF_FALSE;
}

/* Exact match of the first `len` bytes of `arg`; reports the valid names. */
PF_API pf_bool pf__ap_match_enum(
    struct pf_argparser *p,
    const pf_option_enum_t *e,
    const char *arg,
    int len,
    const char *what,
    int *out
) {
    char list[192];
    size_t used = 0;

    for (int i = 0; e[i].name; i++) {
        if (strlen(e[i].name) == (size_t)len
            && 0 == strncmp(arg, e[i].name, (size_t)len)) {
            *out = e[i].value;
            return PF_TRUE;
        }
    }

    list[0] = '\0';
    for (int i = 0; e[i].name; i++) {
        int w = snprintf(
            list + used,
            sizeof(list) - used,
            "%s%s",
            used ? ", " : "",
            e[i].name
        );

        if (w < 0 || (size_t)w >= sizeof(list) - used) {
            list[used] = '\0';
            snprintf(list + used, sizeof(list) - used, ", ...");
            break;
        }
        used += (size_t)w;
    }

    pf_argparser_error(
        p,
        "invalid value '%.*s' for %s (expected one of: %s)",
        len,
        arg,
        what ? what : "argument",
        list
    );
    return PF_FALSE;
}

/* ------------------------------------------------------------------------ */
/* Option matching                                                           */
/* ------------------------------------------------------------------------ */

/* "--name" or "-c" */
PF_API const char *pf__ap_optname(
    const struct pf_argparser *p, char *buf, size_t size
) {
    if (p->item.type == PF__ARG_LONG)
        snprintf(buf, size, "--%s", p->item.name ? p->item.name : "");
    else if (p->item.type == PF__ARG_SHORT)
        snprintf(buf, size, "-%c", p->item.shorthand);
    else
        snprintf(buf, size, "argument");
    return buf;
}

/* "option '--name'" */
PF_API const char *pf__ap_what(
    const struct pf_argparser *p, char *buf, size_t size
) {
    char name[64];

    pf__ap_optname(p, name, sizeof(name));
    snprintf(buf, size, "option '%s'", name);
    return buf;
}

/*
 * Does the current item spell this option? Either of `name` and `shorthand`
 * may be NULL/0. A match marks the item as recognised, which is what stops
 * it from being reported as unknown.
 */
PF_API pf_bool
pf__ap_match_option(struct pf_argparser *p, const char *name, char shorthand) {
    if (!p)
        return PF_FALSE;

    if (p->item.type == PF__ARG_LONG) {
        if (!name || !*name || strcmp(name, p->item.name) != 0)
            return PF_FALSE;
    } else if (p->item.type == PF__ARG_SHORT) {
        if (!shorthand || shorthand != p->item.shorthand)
            return PF_FALSE;
    } else {
        return PF_FALSE;
    }

    p->item.matched = PF_TRUE;
    return PF_TRUE;
}

/*
 * Returns the option's value: the part after '=' (or glued to a short
 * option), otherwise the next command line argument, as with getopt.
 */
PF_API const char *pf__ap_match_value(
    struct pf_argparser *p, const char *name, char shorthand
) {
    char what[96];

    if (!pf__ap_match_option(p, name, shorthand))
        return NULL;

    if (!p->item.hasValue) {
        if (p->index + 1 >= p->argc || !p->argv[p->index + 1]) {
            pf_argparser_error(
                p, "%s requires a value", pf__ap_what(p, what, sizeof(what))
            );
            return NULL;
        }

        p->item.value = p->argv[++p->index];
        p->item.hasValue = PF_TRUE;
    }

    return p->item.value;
}

PF_API pf_bool pf__ap_option_long(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    long min,
    long max,
    long *out
) {
    const char *s;
    char what[96];

    if (!out || !(s = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    return pf_arg_long(p, s, pf__ap_what(p, what, sizeof(what)), min, max, out);
}

/* ------------------------------------------------------------------------ */
/* Typed options                                                             */
/* ------------------------------------------------------------------------ */

PF_API pf_bool pf_option_string(
    struct pf_argparser *p, const char *name, char shorthand, char **out
) {
    const char *val;

    if (!out || !(val = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    *out = (char *)val;
    return PF_TRUE;
}

PF_API pf_bool pf_option_long(
    struct pf_argparser *p, const char *name, char shorthand, long *out
) {
    return pf__ap_option_long(p, name, shorthand, LONG_MIN, LONG_MAX, out);
}

/* Like pf_option_long(), but the value must be within [min, max]. */
PF_API pf_bool pf_option_long_range(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    long min,
    long max,
    long *out
) {
    return pf__ap_option_long(p, name, shorthand, min, max, out);
}

PF_API pf_bool pf_option_int_range(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    int min,
    int max,
    int *out
) {
    long val;

    if (!out || !pf__ap_option_long(p, name, shorthand, min, max, &val))
        return PF_FALSE;

    *out = (int)val;
    return PF_TRUE;
}

PF_API pf_bool pf_option_int(
    struct pf_argparser *p, const char *name, char shorthand, int *out
) {
    return pf_option_int_range(p, name, shorthand, INT_MIN, INT_MAX, out);
}

PF_API pf_bool pf_option_double(
    struct pf_argparser *p, const char *name, char shorthand, double *out
) {
    const char *s;
    char what[96];

    if (!out || !(s = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    return pf_arg_double(p, s, pf__ap_what(p, what, sizeof(what)), out);
}

PF_API pf_bool pf_option_float(
    struct pf_argparser *p, const char *name, char shorthand, float *out
) {
    double val;

    if (!out || !pf_option_double(p, name, shorthand, &val))
        return PF_FALSE;

    if (val > FLT_MAX || val < -FLT_MAX) {
        char what[96];
        pf_argparser_error(
            p, "%s is out of range", pf__ap_what(p, what, sizeof(what))
        );
        return PF_FALSE;
    }

    *out = (float)val;
    return PF_TRUE;
}

/*
 * A flag: "--name" is true; "--name=<bool>" sets the value explicitly.
 * Accepted: 1/0, true/false, yes/no, on/off.
 */
PF_API pf_bool pf_option_bool(
    struct pf_argparser *p, const char *name, char shorthand, pf_bool *out
) {
    const char *v;
    char what[96];

    if (!out || !pf__ap_match_option(p, name, shorthand))
        return PF_FALSE;

    if (!p->item.hasValue) {
        *out = PF_TRUE;
        return PF_TRUE;
    }

    v = p->item.value;

    if (!strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "yes")
        || !strcmp(v, "on")) {
        *out = PF_TRUE;
        return PF_TRUE;
    }

    if (!strcmp(v, "0") || !strcmp(v, "false") || !strcmp(v, "no")
        || !strcmp(v, "off")) {
        *out = PF_FALSE;
        return PF_TRUE;
    }

    pf_argparser_error(
        p, "invalid boolean '%s' for %s", v, pf__ap_what(p, what, sizeof(what))
    );
    return PF_FALSE;
}

/* Like pf_option_bool(), and "--no-<name>" turns the flag off. */
PF_API pf_bool pf_option_toggle(
    struct pf_argparser *p, const char *name, char shorthand, pf_bool *out
) {
    if (!p || !out)
        return PF_FALSE;

    if (pf_option_bool(p, name, shorthand, out))
        return PF_TRUE;

    if (p->item.type == PF__ARG_LONG && name && !p->item.matched
        && strncmp(p->item.name, "no-", 3) == 0
        && strcmp(p->item.name + 3, name) == 0) {
        p->item.matched = PF_TRUE;

        if (p->item.hasValue) {
            pf_argparser_error(
                p, "option '--no-%s' does not take a value", name
            );
            return PF_FALSE;
        }

        *out = PF_FALSE;
        return PF_TRUE;
    }

    return PF_FALSE;
}

/*
 * A repeatable flag ("-vv"): every occurrence adds one. "--name=N" (or
 * "-v2") sets the level explicitly. `*out` must be initialised.
 */
PF_API pf_bool pf_option_count(
    struct pf_argparser *p, const char *name, char shorthand, int *out
) {
    long v;
    char what[96];

    if (!out || !pf__ap_match_option(p, name, shorthand))
        return PF_FALSE;

    if (!p->item.hasValue) {
        if (*out < INT_MAX)
            ++*out;
        return PF_TRUE;
    }

    if (!pf_arg_long(
            p, p->item.value, pf__ap_what(p, what, sizeof(what)), 0, INT_MAX, &v
        ))
        return PF_FALSE;

    *out = (int)v;
    return PF_TRUE;
}

PF_API pf_bool pf_option_enum(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    const pf_option_enum_t *values,
    int *out
) {
    const char *arg;
    char what[96];

    if (!out || !values || !(arg = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    return pf__ap_match_enum(
        p,
        values,
        arg,
        (int)strlen(arg),
        pf__ap_what(p, what, sizeof(what)),
        out
    );
}

/*
 * A set of flags: "--name=a|b|c" (',' works too). The result is OR-ed into
 * `*out`, so repeating the option accumulates. Nothing is written on error.
 */
PF_API pf_bool pf_option_flags(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    const pf_option_enum_t *values,
    int *out
) {
    const char *arg, *next;
    char what[96];
    int acc = 0, value, len;

    if (!out || !values || !(arg = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    pf__ap_what(p, what, sizeof(what));

    for (;;) {
        next = strpbrk(arg, "|,");
        len = next ? (int)(next - arg) : (int)strlen(arg);

        if (!pf__ap_match_enum(p, values, arg, len, what, &value))
            return PF_FALSE;

        acc |= value;

        if (!next)
            break;
        arg = next + 1;
    }

    *out |= acc;
    return PF_TRUE;
}

/* The first positional parameter, matched against `values`, e.g. "status". */
PF_API pf_bool pf_param_enum(
    struct pf_argparser *p,
    int index,
    const char *what,
    const pf_option_enum_t *values,
    int *out
) {
    if (!values || !out || !pf_is_param(p, index))
        return PF_FALSE;

    return pf__ap_match_enum(
        p, values, p->item.param, (int)strlen(p->item.param), what, out
    );
}

/*
 * The first positional parameter ever seen is a subcommand: it is looked up
 * in `values` and removed from paramv, so paramv/paramc hold only its
 * arguments. Returns TRUE only for that one parameter; an unknown name
 * reports an error. Options may precede the subcommand.
 */
PF_API pf_bool pf_param_subcommand(
    struct pf_argparser *p,
    const char *what,
    const pf_option_enum_t *values,
    int *out
) {
    int v;

    if (!p || !values || !out || p->item.type != PF__ARG_PARAM
        || p->subcommandSeen)
        return PF_FALSE;

    p->subcommandSeen = PF_TRUE;

    if (!pf__ap_match_enum(
            p,
            values,
            p->item.param,
            (int)strlen(p->item.param),
            what ? what : "subcommand",
            &v
        ))
        return PF_FALSE;

    *out = v;
    pf_argparser_consume(p);
    return PF_TRUE;
}

/* ------------------------------------------------------------------------ */
/* Common options                                                            */
/* ------------------------------------------------------------------------ */

/* --help, -?: sets p->help (which mutes further errors) and *out. */
PF_API pf_bool pf_option_help(struct pf_argparser *p, pf_bool *out) {
    pf_bool v;

    if (!p || !pf_option_bool(p, "help", PF_ARGPARSE_HELP_SHORT, &v))
        return PF_FALSE;

    p->help = v;
    if (out)
        *out = v;
    return PF_TRUE;
}

/* --version, -V: sets p->version and *out. See pf_argparser_print_version. */
PF_API pf_bool pf_option_version(struct pf_argparser *p, pf_bool *out) {
    pf_bool v;

    if (!p || !pf_option_bool(p, "version", PF_ARGPARSE_VERSION_SHORT, &v))
        return PF_FALSE;

    p->version = v;
    if (out)
        *out = v;
    return PF_TRUE;
}

/*
 * -v, -vv, --verbose, --verbose=N, --no-verbose. Updates p->verbosity,
 * p->verbose and, if bound, the verbosity of the CLI.
 */
PF_API pf_bool pf_option_verbose(struct pf_argparser *p, int *out) {
    int level;

    if (!p)
        return PF_FALSE;

    level = p->verbosity;

    if (p->item.type == PF__ARG_LONG && !p->item.hasValue
        && strcmp(p->item.name, "no-verbose") == 0) {
        p->item.matched = PF_TRUE;
        level = 0;
    } else if (!pf_option_count(
                   p, "verbose", PF_ARGPARSE_VERBOSE_SHORT, &level
               )) {
        return PF_FALSE;
    }

    p->verbosity = level;
    p->verbose = level > 0;

#ifdef POLYFILL_CLI
    if (p->cli)
        p->cli->verbosity = level;
#endif

    if (out)
        *out = level;
    return PF_TRUE;
}

/* --quiet, -q, --no-quiet: sets p->silent and silences the CLI. */
PF_API pf_bool pf_option_quiet(struct pf_argparser *p, pf_bool *out) {
    pf_bool v;

    if (!p || !pf_option_toggle(p, "quiet", PF_ARGPARSE_QUIET_SHORT, &v))
        return PF_FALSE;

    p->silent = v;

#ifdef POLYFILL_CLI
    if (p->cli)
        p->cli->silent = (v || !p->cli->out.handle) ? PF_TRUE : PF_FALSE;
#endif

    if (out)
        *out = v;
    return PF_TRUE;
}

/*
 * --color[=auto|always|never], --no-color. A bare --color means "always"
 * (as in ls and grep). NO_COLOR and terminal detection stay with the CLI.
 */
PF_API pf_bool
pf_option_color(struct pf_argparser *p, enum pf_color_mode_t *out) {
    static const pf_option_enum_t values[] = {
        { "auto", PF_COLOR_AUTO },   { "always", PF_COLOR_ALWAYS },
        { "never", PF_COLOR_NEVER }, { "yes", PF_COLOR_ALWAYS },
        { "no", PF_COLOR_NEVER },    { 0, 0 },
    };
    int mode;

    if (!p)
        return PF_FALSE;

    if (p->item.type == PF__ARG_LONG && !p->item.hasValue
        && strcmp(p->item.name, "no-color") == 0) {
        p->item.matched = PF_TRUE;
        mode = PF_COLOR_NEVER;
    } else if (!pf__ap_match_option(p, "color", PF_ARGPARSE_COLOR_SHORT)) {
        return PF_FALSE;
    } else if (!p->item.hasValue) {
        mode = PF_COLOR_ALWAYS;
    } else if (!pf__ap_match_enum(
                   p,
                   values,
                   p->item.value,
                   (int)strlen(p->item.value),
                   "option '--color'",
                   &mode
               )) {
        return PF_FALSE;
    }

    p->colorMode = (enum pf_color_mode_t)mode;
    p->colorSet = PF_TRUE;

#ifdef POLYFILL_CLI
    if (p->cli)
        pf_cli_set_color_mode(p->cli, p->colorMode);
#endif

    if (out)
        *out = p->colorMode;
    return PF_TRUE;
}

/*
 * --interactive[=auto|always|never], -i, --no-interactive. A bare flag
 * means "always".
 */
PF_API pf_bool
pf_option_interactive(struct pf_argparser *p, enum pf_interactive_mode_t *out) {
    static const pf_option_enum_t values[] = {
        { "auto", PF_INTERACTIVE_AUTO },   { "always", PF_INTERACTIVE_ALWAYS },
        { "never", PF_INTERACTIVE_NEVER }, { "yes", PF_INTERACTIVE_ALWAYS },
        { "no", PF_INTERACTIVE_NEVER },    { 0, 0 },
    };
    int mode;

    if (!p)
        return PF_FALSE;

    if (p->item.type == PF__ARG_LONG && !p->item.hasValue
        && strcmp(p->item.name, "no-interactive") == 0) {
        p->item.matched = PF_TRUE;
        mode = PF_INTERACTIVE_NEVER;
    } else if (!pf__ap_match_option(
                   p, "interactive", PF_ARGPARSE_INTERACTIVE_SHORT
               )) {
        return PF_FALSE;
    } else if (!p->item.hasValue) {
        mode = PF_INTERACTIVE_ALWAYS;
    } else if (!pf__ap_match_enum(
                   p,
                   values,
                   p->item.value,
                   (int)strlen(p->item.value),
                   "option '--interactive'",
                   &mode
               )) {
        return PF_FALSE;
    }

    p->interactiveMode = (enum pf_interactive_mode_t)mode;
    p->interactiveSet = PF_TRUE;

#ifdef POLYFILL_CLI
    if (p->cli)
        pf_cli_set_interactive_mode(p->cli, p->interactiveMode);
#endif

    if (out)
        *out = p->interactiveMode;
    return PF_TRUE;
}

/*
 * Tries every common option selected by `mask` (PF_COMMON_*). Call it first
 * in the callback; the results are in the parser's own fields.
 */
PF_API pf_bool pf_option_common(struct pf_argparser *p, unsigned mask) {
    return ((mask & PF_COMMON_HELP) && pf_option_help(p, NULL))
        || ((mask & PF_COMMON_VERBOSE) && pf_option_verbose(p, NULL))
        || ((mask & PF_COMMON_QUIET) && pf_option_quiet(p, NULL))
        || ((mask & PF_COMMON_COLOR) && pf_option_color(p, NULL))
        || ((mask & PF_COMMON_VERSION) && pf_option_version(p, NULL))
        || ((mask & PF_COMMON_INTERACTIVE) && pf_option_interactive(p, NULL));
}

/* Prints "<program> <version>" on the parser's output stream. */
PF_API void pf_argparser_print_version(
    struct pf_argparser *p, const char *program, const char *version
) {
    if (!p || !p->out)
        return;
    fprintf(p->out, "%s %s\n", program ? program : "", version ? version : "");
}

#ifdef POLYFILL_CLI

PF_API void pf__ap_help_option(
    struct pf_argparser *p,
    char shorthand,
    const char *name,
    const char *arg,
    const char *description
) {
    char buf[96];

    if (shorthand)
        snprintf(buf, sizeof(buf), "-%c, --%s%s", shorthand, name, arg);
    else
        snprintf(buf, sizeof(buf), "--%s%s", name, arg);

    pf_cli_help_definition(p->cli, buf, description);
}

/* Prints help entries for the common options selected by `mask`. */
PF_API void pf_argparser_help_options(struct pf_argparser *p, unsigned mask) {
    if (!p || !p->cli)
        return;

    if (mask & PF_COMMON_HELP) {
        pf__ap_help_option(
            p, PF_ARGPARSE_HELP_SHORT, "help", "", "Shows this information."
        );
    }
    if (mask & PF_COMMON_VERSION) {
        pf__ap_help_option(
            p,
            PF_ARGPARSE_VERSION_SHORT,
            "version",
            "",
            "Shows version information."
        );
    }
    if (mask & PF_COMMON_VERBOSE) {
        pf__ap_help_option(
            p,
            PF_ARGPARSE_VERBOSE_SHORT,
            "verbose",
            "",
            "Print more verbose messages. Repeat for more detail."
        );
    }
    if (mask & PF_COMMON_QUIET) {
        pf__ap_help_option(
            p,
            PF_ARGPARSE_QUIET_SHORT,
            "quiet",
            "",
            "Disables output and error printing."
        );
    }
    if (mask & PF_COMMON_COLOR) {
        pf__ap_help_option(
            p,
            PF_ARGPARSE_COLOR_SHORT,
            "color",
            "[=WHEN]",
            "Colors the output: 'auto', 'always' or 'never'."
        );
    }
    if (mask & PF_COMMON_INTERACTIVE) {
        pf__ap_help_option(
            p,
            PF_ARGPARSE_INTERACTIVE_SHORT,
            "interactive",
            "[=WHEN]",
            "Allows prompts: 'auto', 'always' or 'never'."
        );
    }
}

#endif /* POLYFILL_CLI */

/* ------------------------------------------------------------------------ */
/* Driver                                                                    */
/* ------------------------------------------------------------------------ */

/*
 * Runs the callback for the current item, then reports the item as an
 * unknown option if no pf_option_* function recognised it.
 */
PF_API int pf__ap_dispatch(
    struct pf_argparser *p, pf_argparser_fn cb, void *user
) {
    int rc;

    p->item.matched = PF_FALSE;
    rc = cb(p, user);

    if (!rc && !p->failed && !p->allowUnknown && !p->item.matched
        && (p->item.type == PF__ARG_SHORT || p->item.type == PF__ARG_LONG)) {
        char name[64];
        pf_argparser_error(
            p, "unknown option '%s'", pf__ap_optname(p, name, sizeof(name))
        );
    }

    return rc;
}

PF_API int pf__ap_emit_param(
    struct pf_argparser *p, char *arg, pf_argparser_fn cb, void *user
) {
    int rc;

    p->paramv[p->paramc++] = arg;
    p->item.type = PF__ARG_PARAM;
    p->item.name = NULL;
    p->item.value = NULL;
    p->item.hasValue = PF_FALSE;
    p->item.shorthand = 0;
    p->item.param = arg;
    p->item.consume = PF_FALSE;

    rc = pf__ap_dispatch(p, cb, user);

    if (p->item.consume)
        p->paramc--; /* the current parameter is always the last one */

    return rc;
}

PF_API int pf__ap_emit_long(
    struct pf_argparser *p, char *arg, pf_argparser_fn cb, void *user
) {
    /* argv is never modified: the name is copied out (over-long names can
       not match any option and are merely truncated in error messages). */
    char name[128];
    const char *eq = strchr(arg, '=');
    size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
    int rc;

    if (len >= sizeof(name))
        len = sizeof(name) - 1;
    memcpy(name, arg, len);
    name[len] = '\0';

    p->item.type = PF__ARG_LONG;
    p->item.param = NULL;
    p->item.shorthand = 0;
    p->item.name = name;
    p->item.value = eq ? eq + 1 : NULL;
    p->item.hasValue = eq ? PF_TRUE : PF_FALSE;

    rc = pf__ap_dispatch(p, cb, user);

    p->item.name = NULL; /* `name` dies with this frame */
    return rc;
}

/* Characters that may follow a short option as its value: "-j4", "-j=4". */
PF_API pf_bool pf__ap_isvalue(char chr) {
    pf_bool alpha = (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z');
    return !alpha && chr != '?' && chr != '\0';
}

PF_API int pf__ap_emit_short_cluster(
    struct pf_argparser *p, char *arg, pf_argparser_fn cb, void *user
) {
    int rc = PF_ARGPARSE_OK;

    while (!rc && !p->failed && *arg) {
        p->item.type = PF__ARG_SHORT;
        p->item.param = NULL;
        p->item.name = NULL;
        p->item.shorthand = *arg++;
        p->item.hasValue = PF_FALSE;
        p->item.value = NULL;

        /* An inline value takes up the rest of the cluster. */
        if (pf__ap_isvalue(*arg)) {
            p->item.value = *arg == '=' ? arg + 1 : arg;
            p->item.hasValue = PF_TRUE;
            arg += strlen(arg);
        }

        rc = pf__ap_dispatch(p, cb, user);
    }

    return rc;
}

PF_API int pf_argparser_run(
    struct pf_argparser *p, pf_argparser_fn cb, void *user
) {
    int rc = PF_ARGPARSE_OK;

    if (!p || !cb || !p->argv || !p->paramv)
        return PF_ARGPARSE_EINVAL;

    p->paramc = 0;
    p->failed = PF_FALSE;
    p->pauseOptions = PF_FALSE;
    p->subcommandSeen = PF_FALSE;
    memset(&p->item, 0, sizeof(p->item));

    for (p->index = 1; p->index < p->argc; ++p->index) {
        char *arg = p->argv[p->index];

        if (rc || p->failed)
            break;
        if (!arg)
            continue;

        if (p->pauseOptions || arg[0] != '-' || arg[1] == '\0')
            rc = pf__ap_emit_param(p, arg, cb, user);
        else if (strcmp(arg, "--") == 0)
            p->pauseOptions = PF_TRUE;
        else if (strncmp(arg, "--", 2) == 0)
            rc = pf__ap_emit_long(p, arg + 2, cb, user);
        else
            rc = pf__ap_emit_short_cluster(p, arg + 1, cb, user);
    }

    if (rc && rc != PF_ARGPARSE_EINTR)
        return rc;
    return p->failed ? PF_ARGPARSE_EINVAL : PF_ARGPARSE_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_ARGPARSE */
