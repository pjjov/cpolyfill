/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/argparse.h>: the driver splits argv into items (parameter,
    long option, one short option of a cluster) and calls the callback for
    each; the matchers look at the current item.
*/

#include "internal.h"

#include <cpolyfill/argparse.h>
#include <cpolyfill/threads.h>

#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <shellapi.h>
    #include <windows.h>
#endif

enum { PF__AP_NONE, PF__AP_PARAM, PF__AP_SHORT, PF__AP_LONG };

/* Indexes of pf__shorts, in the order of the PF_COMMON_* bits. */
enum {
    PF__AP_HELP,
    PF__AP_VERBOSE,
    PF__AP_QUIET,
    PF__AP_COLOR,
    PF__AP_VERSION,
    PF__AP_INTERACTIVE
};

#define PF__AP_WHAT 96 /* "option '--name'" buffers; names are cut there */

/* ---- Lifetime ---------------------------------------------------------- */

PF_API int pf_argparser_init(pf_argparser_t *p, int argc, char **argv) {
    if (!p || !argv || argc < 0)
        return PF_EINVAL;
    memset(p, 0, sizeof(*p));

#if PF_OS_WINDOWS
    /* main's arguments are in the ANSI code page: use UTF-8 ones. */
    if (argv == __argv) {
        int n;
        char **v;
        if (pf_args_utf8(&n, &v) == PF_OK) {
            argc = n;
            argv = v;
        }
    }
#endif

    p->paramv = (char **)allocate(NULL, ((size_t)argc + 1) * sizeof(char *));
    if (!p->paramv)
        return PF_ENOMEM;
    p->out = stdout;
    p->err = stderr;
    p->pf__argc = argc;
    p->pf__argv = argv;
    p->pf__shorts[PF__AP_HELP] = '?';
    p->pf__shorts[PF__AP_VERBOSE] = 'v';
    p->pf__shorts[PF__AP_QUIET] = 'q';
    p->pf__shorts[PF__AP_COLOR] = 0;
    p->pf__shorts[PF__AP_VERSION] = 'V';
    p->pf__shorts[PF__AP_INTERACTIVE] = 'i';
    return PF_OK;
}

PF_API void pf_argparser_free(pf_argparser_t *p) {
    if (!p || !p->paramv)
        return;
    deallocate(NULL, p->paramv, ((size_t)p->pf__argc + 1) * sizeof(char *));
    p->paramv = NULL;
    p->paramc = 0;
}

PF_API void pf_argparser_set_error_fn(
    pf_argparser_t *p, pf_argparser_error_fn fn, void *user
) {
    if (!p)
        return;
    p->pf__error_fn = fn;
    p->pf__error_user = user;
}

PF_API void pf_argparser_set_short(pf_argparser_t *p, unsigned common, char c) {
    int k;
    if (!p)
        return;
    for (k = 0; k < 6; k++)
        if (common & (1u << k))
            p->pf__shorts[k] = c;
}

/* ---- Errors ------------------------------------------------------------ */

PF_API void pf_argparser_verror(
    pf_argparser_t *p, const char *fmt, va_list ap
) {
    char msg[1024];

    if (!p || !fmt || p->help)
        return;
    p->failed = true;
    if (p->silent)
        return;
    vsnprintf(msg, sizeof(msg), fmt, ap);
    if (p->pf__error_fn) {
        p->pf__error_fn(p, msg, p->pf__error_user);
        return;
    }
    if (!p->err)
        return;
    if (p->err_prefix)
        fputs(p->err_prefix, p->err);
    fputs(msg, p->err);
    fputc('\n', p->err);
}

PF_API void pf_argparser_error(pf_argparser_t *p, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    pf_argparser_verror(p, fmt, ap);
    va_end(ap);
}

/* ---- Numbers ----------------------------------------------------------- */

/* Decimal, 0x hexadecimal or 0b binary, with an optional sign. 0,
   PF_EINVAL or PF_ERANGE. */
static int pf__ap_parse_long(const char *s, long *out) {
    const char *c = s;
    unsigned long acc = 0, limit;
    unsigned base = 10;
    bool neg = false, over = false;

    if (*c == '+' || *c == '-')
        neg = *c++ == '-';
    if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X')) {
        base = 16;
        c += 2;
    } else if (c[0] == '0' && (c[1] == 'b' || c[1] == 'B')) {
        base = 2;
        c += 2;
    }
    if (!*c)
        return PF_EINVAL;
    limit = neg ? (unsigned long)LONG_MAX + 1 : (unsigned long)LONG_MAX;
    for (; *c; c++) {
        unsigned d;
        if (*c >= '0' && *c <= '9')
            d = (unsigned)(*c - '0');
        else if (*c >= 'a' && *c <= 'f')
            d = (unsigned)(*c - 'a' + 10);
        else if (*c >= 'A' && *c <= 'F')
            d = (unsigned)(*c - 'A' + 10);
        else
            return PF_EINVAL;
        if (d >= base)
            return PF_EINVAL;
        if (over || acc > (limit - d) / base)
            over = true;
        else
            acc = acc * base + d;
    }
    if (over)
        return PF_ERANGE;
    if (!neg)
        *out = (long)acc;
    else if (acc == (unsigned long)LONG_MAX + 1)
        *out = LONG_MIN;
    else
        *out = -(long)acc;
    return PF_OK;
}

/* strtod with `.` as the decimal point whatever the locale: the string is
   rewritten with the locale's. 0, PF_EINVAL or PF_ERANGE. */
static int pf__ap_parse_double(const char *s, double *out) {
    const struct lconv *lc = localeconv();
    const char *dp = lc && lc->decimal_point && *lc->decimal_point
        ? lc->decimal_point
        : ".";
    char stack[128], *buf = stack, *end;
    size_t n = strlen(s), dpn = strlen(dp), size = 0, k;
    double v;

    if (n == 0 || s[0] == ' ' || (s[0] >= '\t' && s[0] <= '\r'))
        return PF_EINVAL;
    if (strcmp(dp, ".") != 0) {
        /* Only '.' is a decimal point here. */
        if (strstr(s, dp))
            return PF_EINVAL;
        size = n * dpn + 1;
        if (size > sizeof(stack) && !(buf = (char *)allocate(NULL, size)))
            return PF_ENOMEM;
        for (k = 0, n = 0; s[k]; k++) {
            if (s[k] == '.') {
                memcpy(buf + n, dp, dpn);
                n += dpn;
            } else {
                buf[n++] = s[k];
            }
        }
        buf[n] = '\0';
        s = buf;
    }
    v = strtod(s, &end);
    k = (size_t)(*end != '\0' || end == s);
    if (buf != stack)
        deallocate(NULL, buf, size);
    if (k || v != v)
        return PF_EINVAL;
    if (v > DBL_MAX || v < -DBL_MAX)
        return PF_ERANGE;
    *out = v;
    return PF_OK;
}

PF_API bool pf_arg_long(
    pf_argparser_t *p,
    const char *s,
    const char *what,
    long min,
    long max,
    long *out
) {
    long v = 0;
    int rc;

    if (!p || !s || !out)
        return false;
    if (!what)
        what = "value";
    rc = pf__ap_parse_long(s, &v);
    if (rc == PF_EINVAL) {
        pf_argparser_error(p, "invalid integer '%s' for %s", s, what);
        return false;
    }
    if (rc == PF_ERANGE || v < min || v > max) {
        if (min == LONG_MIN && max == LONG_MAX)
            pf_argparser_error(p, "%s is out of range: '%s'", what, s);
        else
            pf_argparser_error(
                p, "%s must be between %ld and %ld; got '%s'", what, min, max, s
            );
        return false;
    }
    *out = v;
    return true;
}

PF_API bool pf_arg_int(
    pf_argparser_t *p,
    const char *s,
    const char *what,
    int min,
    int max,
    int *out
) {
    long v;
    if (!out || !pf_arg_long(p, s, what, min, max, &v))
        return false;
    *out = (int)v;
    return true;
}

PF_API bool pf_arg_double(
    pf_argparser_t *p, const char *s, const char *what, double *out
) {
    double v = 0;
    int rc;

    if (!p || !s || !out)
        return false;
    if (!what)
        what = "value";
    rc = pf__ap_parse_double(s, &v);
    if (rc == PF_ERANGE) {
        pf_argparser_error(p, "%s is out of range: '%s'", what, s);
        return false;
    }
    if (rc != PF_OK) {
        pf_argparser_error(p, "invalid number '%s' for %s", s, what);
        return false;
    }
    *out = v;
    return true;
}

/* ---- Enumerations ------------------------------------------------------ */

PF_API bool pf_match_enum(
    const char *s, const pf_option_enum_t *values, int *out
) {
    int k;
    if (!s || !values || !out)
        return false;
    for (k = 0; values[k].name; k++) {
        if (strcmp(s, values[k].name) == 0) {
            *out = values[k].value;
            return true;
        }
    }
    return false;
}

/* The first `len` bytes of `s` against the names; reports the valid ones. */
static bool pf__ap_match_enum(
    pf_argparser_t *p,
    const pf_option_enum_t *e,
    const char *s,
    size_t len,
    const char *what,
    int *out
) {
    char list[192];
    size_t used = 0;
    int k;

    for (k = 0; e[k].name; k++) {
        if (strlen(e[k].name) == len && memcmp(s, e[k].name, len) == 0) {
            *out = e[k].value;
            return true;
        }
    }
    list[0] = '\0';
    for (k = 0; e[k].name; k++) {
        int w = snprintf(
            list + used,
            sizeof(list) - used,
            "%s%s",
            used ? ", " : "",
            e[k].name
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
        (int)(len > INT_MAX ? INT_MAX : len),
        s,
        what ? what : "argument",
        list
    );
    return false;
}

/* ---- The current item -------------------------------------------------- */

/* "option '--name'" or "option '-c'". */
static const char *pf__ap_what(const pf_argparser_t *p, char *buf) {
    if (p->pf__item.type == PF__AP_LONG)
        snprintf(
            buf,
            PF__AP_WHAT,
            "option '--%.*s'",
            (int)(p->pf__item.name_len > 64 ? 64 : p->pf__item.name_len),
            p->pf__item.name
        );
    else if (p->pf__item.type == PF__AP_SHORT)
        snprintf(buf, PF__AP_WHAT, "option '-%c'", p->pf__item.short_name);
    else
        snprintf(buf, PF__AP_WHAT, "argument");
    return buf;
}

static bool pf__ap_long_is(const pf_argparser_t *p, const char *name) {
    size_t n;
    if (p->pf__item.type != PF__AP_LONG || !name || !*name)
        return false;
    n = strlen(name);
    return n == p->pf__item.name_len && memcmp(name, p->pf__item.name, n) == 0;
}

/* "--no-<name>" */
static bool pf__ap_long_is_no(const pf_argparser_t *p, const char *name) {
    size_t n;
    if (p->pf__item.type != PF__AP_LONG || !name || !*name)
        return false;
    n = strlen(name);
    return p->pf__item.name_len == n + 3
        && memcmp(p->pf__item.name, "no-", 3) == 0
        && memcmp(p->pf__item.name + 3, name, n) == 0;
}

/* Does the item spell this option? A match marks it as recognised. */
static bool pf__ap_match(pf_argparser_t *p, const char *name, char c) {
    if (!p)
        return false;
    if (p->pf__item.type == PF__AP_LONG) {
        if (!pf__ap_long_is(p, name))
            return false;
    } else if (p->pf__item.type == PF__AP_SHORT) {
        if (!c || c != p->pf__item.short_name)
            return false;
    } else {
        return false;
    }
    p->pf__item.matched = true;
    return true;
}

/* The value of an option that takes one: after '=', the rest of a short
   cluster, or the next argument. NULL (reported) when there is none. */
static const char *pf__ap_value(pf_argparser_t *p) {
    char what[PF__AP_WHAT];

    if (p->pf__item.has_value)
        return p->pf__item.value;
    if (p->pf__item.type == PF__AP_SHORT && *p->pf__item.rest) {
        const char *r = p->pf__item.rest;
        p->pf__item.rest_used = true;
        p->pf__item.value = *r == '=' ? r + 1 : r;
        p->pf__item.has_value = true;
        return p->pf__item.value;
    }
    if (p->pf__index + 1 >= p->pf__argc || !p->pf__argv[p->pf__index + 1]) {
        pf_argparser_error(p, "%s requires a value", pf__ap_what(p, what));
        return NULL;
    }
    p->pf__item.value = p->pf__argv[++p->pf__index];
    p->pf__item.has_value = true;
    return p->pf__item.value;
}

/* The explicit value of a flag: after '=' (long or short), or the digits
   that follow a short count option. NULL when there is none. */
static const char *pf__ap_flag_value(pf_argparser_t *p, bool digits) {
    const char *r;

    if (p->pf__item.type == PF__AP_LONG)
        return p->pf__item.has_value ? p->pf__item.value : NULL;
    r = p->pf__item.rest;
    if (*r == '=') {
        p->pf__item.rest_used = true;
        return r + 1;
    }
    if (digits && *r >= '0' && *r <= '9') {
        p->pf__item.rest_used = true;
        return r;
    }
    return NULL;
}

/* ---- Options ----------------------------------------------------------- */

PF_API bool pf_option_is(pf_argparser_t *p) {
    return p
        && (p->pf__item.type == PF__AP_SHORT
            || p->pf__item.type == PF__AP_LONG);
}

PF_API bool pf_option_string(
    pf_argparser_t *p, const char *name, char c, char **out
) {
    const char *v;
    if (!out || !pf__ap_match(p, name, c) || !(v = pf__ap_value(p)))
        return false;
    *out = (char *)v;
    return true;
}

static bool pf__ap_option_long(
    pf_argparser_t *p, const char *name, char c, long min, long max, long *out
) {
    const char *s;
    char what[PF__AP_WHAT];
    if (!out || !pf__ap_match(p, name, c) || !(s = pf__ap_value(p)))
        return false;
    return pf_arg_long(p, s, pf__ap_what(p, what), min, max, out);
}

PF_API bool pf_option_long(
    pf_argparser_t *p, const char *name, char c, long *out
) {
    return pf__ap_option_long(p, name, c, LONG_MIN, LONG_MAX, out);
}

PF_API bool pf_option_long_range(
    pf_argparser_t *p, const char *name, char c, long min, long max, long *out
) {
    return pf__ap_option_long(p, name, c, min, max, out);
}

PF_API bool pf_option_int_range(
    pf_argparser_t *p, const char *name, char c, int min, int max, int *out
) {
    long v;
    if (!out || !pf__ap_option_long(p, name, c, min, max, &v))
        return false;
    *out = (int)v;
    return true;
}

PF_API bool pf_option_int(
    pf_argparser_t *p, const char *name, char c, int *out
) {
    return pf_option_int_range(p, name, c, INT_MIN, INT_MAX, out);
}

PF_API bool pf_option_double(
    pf_argparser_t *p, const char *name, char c, double *out
) {
    const char *s;
    char what[PF__AP_WHAT];
    if (!out || !pf__ap_match(p, name, c) || !(s = pf__ap_value(p)))
        return false;
    return pf_arg_double(p, s, pf__ap_what(p, what), out);
}

PF_API bool pf_option_float(
    pf_argparser_t *p, const char *name, char c, float *out
) {
    double v;
    if (!out || !pf_option_double(p, name, c, &v))
        return false;
    if (v > FLT_MAX || v < -FLT_MAX) {
        char what[PF__AP_WHAT];
        pf_argparser_error(p, "%s is out of range", pf__ap_what(p, what));
        return false;
    }
    *out = (float)v;
    return true;
}

PF_API bool pf_option_bool(
    pf_argparser_t *p, const char *name, char c, bool *out
) {
    const char *v;
    char what[PF__AP_WHAT];

    if (!out || !pf__ap_match(p, name, c))
        return false;
    if (!(v = pf__ap_flag_value(p, false))) {
        *out = true;
        return true;
    }
    if (!strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "yes")
        || !strcmp(v, "on")) {
        *out = true;
        return true;
    }
    if (!strcmp(v, "0") || !strcmp(v, "false") || !strcmp(v, "no")
        || !strcmp(v, "off")) {
        *out = false;
        return true;
    }
    pf_argparser_error(
        p, "invalid boolean '%s' for %s", v, pf__ap_what(p, what)
    );
    return false;
}

PF_API bool pf_option_toggle(
    pf_argparser_t *p, const char *name, char c, bool *out
) {
    if (!p || !out)
        return false;
    if (pf_option_bool(p, name, c, out))
        return true;
    if (p->pf__item.matched || !pf__ap_long_is_no(p, name))
        return false;
    p->pf__item.matched = true;
    if (p->pf__item.has_value) {
        pf_argparser_error(p, "option '--no-%s' does not take a value", name);
        return false;
    }
    *out = false;
    return true;
}

PF_API bool pf_option_count(
    pf_argparser_t *p, const char *name, char c, int *out
) {
    const char *v;
    char what[PF__AP_WHAT];
    long n;

    if (!out || !pf__ap_match(p, name, c))
        return false;
    if (!(v = pf__ap_flag_value(p, true))) {
        if (*out < INT_MAX)
            ++*out;
        return true;
    }
    if (!pf_arg_long(p, v, pf__ap_what(p, what), 0, INT_MAX, &n))
        return false;
    *out = (int)n;
    return true;
}

PF_API bool pf_option_enum(
    pf_argparser_t *p,
    const char *name,
    char c,
    const pf_option_enum_t *values,
    int *out
) {
    const char *v;
    char what[PF__AP_WHAT];
    if (!out || !values || !pf__ap_match(p, name, c) || !(v = pf__ap_value(p)))
        return false;
    return pf__ap_match_enum(
        p, values, v, strlen(v), pf__ap_what(p, what), out
    );
}

PF_API bool pf_option_flags(
    pf_argparser_t *p,
    const char *name,
    char c,
    const pf_option_enum_t *values,
    int *out
) {
    const char *v, *next;
    char what[PF__AP_WHAT];
    int acc = 0, one;

    if (!out || !values || !pf__ap_match(p, name, c) || !(v = pf__ap_value(p)))
        return false;
    pf__ap_what(p, what);
    for (;;) {
        size_t len;
        next = strpbrk(v, "|,");
        len = next ? (size_t)(next - v) : strlen(v);
        if (!pf__ap_match_enum(p, values, v, len, what, &one))
            return false;
        acc |= one;
        if (!next)
            break;
        v = next + 1;
    }
    *out |= acc;
    return true;
}

/* ---- Common options ---------------------------------------------------- */

PF_API bool pf_option_help(pf_argparser_t *p, bool *out) {
    bool v;
    if (!p || !pf_option_bool(p, "help", p->pf__shorts[PF__AP_HELP], &v))
        return false;
    p->help = v;
    if (out)
        *out = v;
    return true;
}

PF_API bool pf_option_version(pf_argparser_t *p, bool *out) {
    bool v;
    if (!p || !pf_option_bool(p, "version", p->pf__shorts[PF__AP_VERSION], &v))
        return false;
    p->version = v;
    if (out)
        *out = v;
    return true;
}

PF_API bool pf_option_verbose(pf_argparser_t *p, int *out) {
    int level;

    if (!p)
        return false;
    level = p->verbosity;
    if (pf__ap_long_is_no(p, "verbose") && !p->pf__item.has_value) {
        p->pf__item.matched = true;
        level = 0;
    } else if (!pf_option_count(
                   p, "verbose", p->pf__shorts[PF__AP_VERBOSE], &level
               )) {
        return false;
    }
    p->verbosity = level;
    if (out)
        *out = level;
    return true;
}

PF_API bool pf_option_quiet(pf_argparser_t *p, bool *out) {
    bool v;
    if (!p || !pf_option_toggle(p, "quiet", p->pf__shorts[PF__AP_QUIET], &v))
        return false;
    p->silent = v;
    if (out)
        *out = v;
    return true;
}

/* --name[=auto|always|never|yes|no], --no-name: the mode, or -1. */
static int pf__ap_mode(pf_argparser_t *p, const char *name, char c) {
    static const pf_option_enum_t values[] = {
        { "auto", 0 }, { "always", 1 }, { "never", 2 },
        { "yes", 1 },  { "no", 2 },     { 0, 0 },
    };
    char what[PF__AP_WHAT];
    const char *v;
    int mode;

    if (pf__ap_long_is_no(p, name) && !p->pf__item.has_value) {
        p->pf__item.matched = true;
        return 2;
    }
    if (!pf__ap_match(p, name, c))
        return -1;
    if (!(v = pf__ap_flag_value(p, false)))
        return 1;
    if (!pf__ap_match_enum(
            p, values, v, strlen(v), pf__ap_what(p, what), &mode
        ))
        return -1;
    return mode;
}

PF_API bool pf_option_color(pf_argparser_t *p, pf_color_mode_t *out) {
    static const pf_color_mode_t modes[] = { PF_COLOR_AUTO,
                                             PF_COLOR_ALWAYS,
                                             PF_COLOR_NEVER };
    int m;
    if (!p || (m = pf__ap_mode(p, "color", p->pf__shorts[PF__AP_COLOR])) < 0)
        return false;
    p->color_mode = modes[m];
    p->color_set = true;
    if (out)
        *out = p->color_mode;
    return true;
}

PF_API bool pf_option_interactive(
    pf_argparser_t *p, pf_interactive_mode_t *out
) {
    static const pf_interactive_mode_t modes[] = { PF_INTERACTIVE_AUTO,
                                                   PF_INTERACTIVE_ALWAYS,
                                                   PF_INTERACTIVE_NEVER };
    int m;
    if (!p
        || (m = pf__ap_mode(
                p, "interactive", p->pf__shorts[PF__AP_INTERACTIVE]
            ))
            < 0)
        return false;
    p->interactive_mode = modes[m];
    p->interactive_set = true;
    if (out)
        *out = p->interactive_mode;
    return true;
}

PF_API bool pf_option_common(pf_argparser_t *p, unsigned mask) {
    return ((mask & PF_COMMON_HELP) && pf_option_help(p, NULL))
        || ((mask & PF_COMMON_VERBOSE) && pf_option_verbose(p, NULL))
        || ((mask & PF_COMMON_QUIET) && pf_option_quiet(p, NULL))
        || ((mask & PF_COMMON_COLOR) && pf_option_color(p, NULL))
        || ((mask & PF_COMMON_VERSION) && pf_option_version(p, NULL))
        || ((mask & PF_COMMON_INTERACTIVE) && pf_option_interactive(p, NULL));
}

PF_API void pf_argparser_print_version(
    pf_argparser_t *p, const char *program, const char *version
) {
    if (!p || !p->out)
        return;
    fprintf(p->out, "%s %s\n", program ? program : "", version ? version : "");
}

static void pf__ap_help_line(
    pf_argparser_t *p,
    char c,
    const char *name,
    const char *arg,
    const char *text
) {
    char head[64];
    if (c)
        snprintf(head, sizeof(head), "-%c, --%s%s", c, name, arg);
    else
        snprintf(head, sizeof(head), "    --%s%s", name, arg);
    fprintf(p->out, "  %-24s %s\n", head, text);
}

PF_API void pf_argparser_help_options(pf_argparser_t *p, unsigned mask) {
    if (!p || !p->out)
        return;
    if (mask & PF_COMMON_HELP)
        pf__ap_help_line(
            p, p->pf__shorts[PF__AP_HELP], "help", "", "Shows this information."
        );
    if (mask & PF_COMMON_VERSION)
        pf__ap_help_line(
            p,
            p->pf__shorts[PF__AP_VERSION],
            "version",
            "",
            "Shows version information."
        );
    if (mask & PF_COMMON_VERBOSE)
        pf__ap_help_line(
            p,
            p->pf__shorts[PF__AP_VERBOSE],
            "verbose",
            "",
            "Prints more messages. Repeat for more detail."
        );
    if (mask & PF_COMMON_QUIET)
        pf__ap_help_line(
            p,
            p->pf__shorts[PF__AP_QUIET],
            "quiet",
            "",
            "Disables output and error printing."
        );
    if (mask & PF_COMMON_COLOR)
        pf__ap_help_line(
            p,
            p->pf__shorts[PF__AP_COLOR],
            "color",
            "[=WHEN]",
            "Colors the output: 'auto', 'always' or 'never'."
        );
    if (mask & PF_COMMON_INTERACTIVE)
        pf__ap_help_line(
            p,
            p->pf__shorts[PF__AP_INTERACTIVE],
            "interactive",
            "[=WHEN]",
            "Allows prompts: 'auto', 'always' or 'never'."
        );
}

/* ---- Parameters -------------------------------------------------------- */

PF_API bool pf_param_is(pf_argparser_t *p, int index) {
    if (!p || p->pf__item.type != PF__AP_PARAM)
        return false;
    return index < 0 || p->paramc == index + 1;
}

PF_API bool pf_param_equals(pf_argparser_t *p, const char *s) {
    return s && pf_param_is(p, -1) && strcmp(p->pf__item.value, s) == 0;
}

PF_API bool pf_param_match(
    pf_argparser_t *p, const char *const *values, char **out
) {
    int k;
    if (!values || !out || !pf_param_is(p, -1))
        return false;
    for (k = 0; values[k]; k++) {
        if (strcmp(p->pf__item.value, values[k]) == 0) {
            *out = (char *)p->pf__item.value;
            return true;
        }
    }
    return false;
}

PF_API void pf_param_consume(pf_argparser_t *p) {
    if (p && p->pf__item.type == PF__AP_PARAM)
        p->pf__item.consume = true;
}

PF_API bool pf_param_enum(
    pf_argparser_t *p,
    int index,
    const char *what,
    const pf_option_enum_t *values,
    int *out
) {
    const char *v;
    if (!values || !out || !pf_param_is(p, index))
        return false;
    v = p->pf__item.value;
    return pf__ap_match_enum(p, values, v, strlen(v), what, out);
}

PF_API bool pf_param_subcommand(
    pf_argparser_t *p,
    const char *what,
    const pf_option_enum_t *values,
    int *out
) {
    const char *v;
    int value;

    if (!p || !values || !out || p->pf__item.type != PF__AP_PARAM
        || p->pf__subcommand_seen)
        return false;
    p->pf__subcommand_seen = true;
    v = p->pf__item.value;
    if (!pf__ap_match_enum(
            p, values, v, strlen(v), what ? what : "subcommand", &value
        ))
        return false;
    *out = value;
    pf_param_consume(p);
    return true;
}

PF_API bool pf_param_expect(
    pf_argparser_t *p, int min, int max, const char *what
) {
    if (!p)
        return false;
    if (!what)
        what = "command";
    if (max == 0 && p->paramc > 0) {
        pf_argparser_error(p, "%s takes no arguments; got %d", what, p->paramc);
        return false;
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
        return false;
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
        return false;
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
        return false;
    }
    return true;
}

/* ---- Driver ------------------------------------------------------------ */

static void pf__ap_item(pf_argparser_t *p, int type) {
    memset(&p->pf__item, 0, sizeof(p->pf__item));
    p->pf__item.type = type;
    p->pf__item.rest = "";
}

/* Runs the callback for the item, then reports an option nothing took. */
static int pf__ap_dispatch(pf_argparser_t *p, pf_argparser_fn cb, void *user) {
    int rc = cb(p, user);

    if (!rc && !p->failed && !p->allow_unknown && !p->pf__item.matched
        && pf_option_is(p)) {
        char what[PF__AP_WHAT];
        pf__ap_what(p, what);
        /* "unknown option '--x'": `what` starts with "option " */
        pf_argparser_error(p, "unknown %s", what);
    }
    return rc;
}

static int pf__ap_param(
    pf_argparser_t *p, char *arg, pf_argparser_fn cb, void *user
) {
    int rc;
    p->paramv[p->paramc++] = arg;
    pf__ap_item(p, PF__AP_PARAM);
    p->pf__item.value = arg;
    rc = pf__ap_dispatch(p, cb, user);
    if (p->pf__item.consume)
        p->paramc--; /* the current parameter is always the last one */
    return rc;
}

static int pf__ap_long(
    pf_argparser_t *p, const char *arg, pf_argparser_fn cb, void *user
) {
    const char *eq = strchr(arg, '=');
    pf__ap_item(p, PF__AP_LONG);
    p->pf__item.name = arg;
    p->pf__item.name_len = eq ? (size_t)(eq - arg) : strlen(arg);
    p->pf__item.value = eq ? eq + 1 : NULL;
    p->pf__item.has_value = eq != NULL;
    return pf__ap_dispatch(p, cb, user);
}

static int pf__ap_cluster(
    pf_argparser_t *p, const char *arg, pf_argparser_fn cb, void *user
) {
    int rc = 0;
    while (!rc && !p->failed && *arg) {
        pf__ap_item(p, PF__AP_SHORT);
        p->pf__item.short_name = *arg++;
        p->pf__item.rest = arg;
        rc = pf__ap_dispatch(p, cb, user);
        if (p->pf__item.rest_used)
            break;
    }
    return rc;
}

PF_API int pf_argparser_run(pf_argparser_t *p, pf_argparser_fn cb, void *user) {
    int rc = 0;

    if (!p || !cb || !p->pf__argv || !p->paramv)
        return PF_EINVAL;
    p->paramc = 0;
    p->failed = false;
    p->pf__options_done = false;
    p->pf__subcommand_seen = false;
    pf__ap_item(p, PF__AP_NONE);

    for (p->pf__index = 1; p->pf__index < p->pf__argc; p->pf__index++) {
        char *arg = p->pf__argv[p->pf__index];

        if (rc || p->failed)
            break;
        if (!arg)
            continue;
        if (p->pf__options_done || arg[0] != '-' || arg[1] == '\0')
            rc = pf__ap_param(p, arg, cb, user);
        else if (strcmp(arg, "--") == 0)
            p->pf__options_done = true;
        else if (arg[1] == '-')
            rc = pf__ap_long(p, arg + 2, cb, user);
        else
            rc = pf__ap_cluster(p, arg + 1, cb, user);
    }
    pf__ap_item(p, PF__AP_NONE);

    if (rc && rc != PF_EINTR)
        return rc;
    return p->failed ? PF_EINVAL : PF_OK;
}

/* ---- Windows arguments ------------------------------------------------- */

#if PF_OS_WINDOWS

static pf_once_flag pf__args_once = PF_ONCE_FLAG_INIT;
static int pf__args_rc;
static int pf__args_argc;
static char **pf__args_argv;

/* One block: the pointers, then the strings (WTF-8, so file names with
   unpaired surrogates survive the way back through the path functions). */
static void pf__args_build(void) {
    int n, k;
    LPWSTR *w = CommandLineToArgvW(GetCommandLineW(), &n);
    size_t size, used;
    char *block;

    if (!w) {
        pf__args_rc = pf_error_last_win32();
        return;
    }
    size = ((size_t)n + 1) * sizeof(char *);
    for (k = 0; k < n; k++) {
        ptrdiff_t len = pf__utf16_to_wtf8(
            (const uint16_t *)w[k], PF_NTS, NULL, 0
        );
        if (len < 0) {
            pf__args_rc = (int)len;
            LocalFree(w);
            return;
        }
        size += (size_t)len + 1;
    }
    block = (char *)malloc(size);
    if (!block) {
        pf__args_rc = PF_ENOMEM;
        LocalFree(w);
        return;
    }
    pf__args_argv = (char **)block;
    used = ((size_t)n + 1) * sizeof(char *);
    for (k = 0; k < n; k++) {
        ptrdiff_t len = pf__utf16_to_wtf8(
            (const uint16_t *)w[k], PF_NTS, block + used, size - used
        );
        pf__args_argv[k] = block + used;
        used += (size_t)len + 1;
    }
    pf__args_argv[n] = NULL;
    pf__args_argc = n;
    LocalFree(w);
}

PF_API int pf_args_utf8(int *argc, char ***argv) {
    if (!argc || !argv)
        return PF_EINVAL;
    pf_call_once(&pf__args_once, pf__args_build);
    if (pf__args_rc)
        return pf__args_rc;
    *argc = pf__args_argc;
    *argv = pf__args_argv;
    return PF_OK;
}

#else

PF_API int pf_args_utf8(int *argc, char ***argv) {
    return argc && argv ? PF_OK : PF_EINVAL;
}

#endif
