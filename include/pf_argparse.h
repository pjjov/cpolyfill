/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a simple CLI argument parsing.

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum pf_argparse_error {
    PF_ARGPARSE_OK = 0,
    PF_ARGPARSE_EINTR = -4,
    PF_ARGPARSE_ENOMEM = -12,
    PF_ARGPARSE_EINVAL = -22,
};

struct pf_argparser {
    FILE *out;
    FILE *err;

    int paramc;
    char **paramv;

    pf_bool verbose;
    pf_bool silent;
    pf_bool help;
    pf_bool failed;

    /* private */

    pf_bool pauseOptions;
    int index;
    int argc;
    char **argv;

    struct {
        enum {
            PF__ARG_NONE,
            PF__ARG_PARAM,
            PF__ARG_SHORT,
            PF__ARG_LONG,
        } type;

        const char *name;
        const char *value;
        const char *param;
        char shorthand;
        pf_bool hasValue;
    } item;
};

typedef int (*pf_argparser_fn)(struct pf_argparser *p, void *user);

typedef struct pf_option_enum_t {
    const char *name;
    int value;
} pf_option_enum_t;

PF_API int pf_argparser_init(struct pf_argparser *p, int argc, char *argv[]) {
    if (!p || !argv)
        return PF_ARGPARSE_EINVAL;

    memset(p, 0, sizeof(*p));

    p->paramv = malloc(argc * sizeof(*p->paramv));
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

PF_API int pf_is_param(struct pf_argparser *p) {
    return p && p->item.type == PF__ARG_PARAM;
}

PF_API int pf_cmp_param(struct pf_argparser *p, const char *other) {
    if (!p || !other)
        return PF_FALSE;
    if (p->item.type != PF__ARG_PARAM)
        return PF_FALSE;
    return strcmp(p->item.param, other) == 0;
}

PF_API void pf_argparser_error(struct pf_argparser *p, const char *fmt, ...) {
    if (!p || !fmt || p->silent || p->help)
        return;

    p->failed = PF_TRUE;

    va_list args;
    va_start(args, fmt);
    vfprintf(p->err, fmt, args);
    va_end(args);
}

PF_API int pf__ap_match_option(
    struct pf_argparser *p, const char *name, char shorthand
) {
    if (!p || !name)
        return PF_FALSE;

    if (p->item.type == PF__ARG_LONG) {
        if (name && strcmp(name, p->item.name) == 0)
            return PF_TRUE;
    }

    if (p->item.type == PF__ARG_SHORT) {
        if (shorthand && shorthand == p->item.shorthand)
            return PF_TRUE;
    }

    return PF_FALSE;
}

PF_API const char *pf__ap_match_value(
    struct pf_argparser *p, const char *name, char shorthand
) {
    if (!pf__ap_match_option(p, name, shorthand))
        return NULL;

    if (!p->item.hasValue) {
        pf_argparser_error(
            p, "option '%s' requires a value\n", name ? name : ""
        );
        return NULL;
    }

    return p->item.value;
}

PF_API int pf__ap_match_long(
    struct pf_argparser *p, const char *name, char shorthand, long *out
) {
    const char *s;
    char *end;

    if (!(s = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    errno = 0;
    long v = strtol(s, &end, 0);

    if (errno || *end) {
        pf_argparser_error(p, "invalid integer '%s'\n", p->item.value);
        return PF_FALSE;
    }

    *out = v;
    return PF_TRUE;
}

PF_API int pf__ap_match_double(
    struct pf_argparser *p, const char *name, char shorthand, double *out
) {
    const char *s;
    char *end;

    if (!(s = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    errno = 0;
    double v = strtod(s, &end);

    if (errno || *end) {
        pf_argparser_error(p, "invalid number '%s'\n", p->item.value);
        return PF_FALSE;
    }

    *out = v;
    return PF_TRUE;
}

PF_API int pf_option_string(
    struct pf_argparser *p, const char *name, char shorthand, char **out
) {
    const char *val;

    if (!(val = pf__ap_match_value(p, name, shorthand)))
        return PF_FALSE;

    *out = (char *)val;
    return PF_TRUE;
}

PF_API int pf_option_long(
    struct pf_argparser *p, const char *name, char shorthand, long *out
) {
    return out && pf__ap_match_long(p, name, shorthand, out);
}

PF_API int pf_option_int(
    struct pf_argparser *p, const char *name, char shorthand, int *out
) {
    long val;
    if (!out || !pf__ap_match_long(p, name, shorthand, &val))
        return PF_FALSE;

    *out = val;
    return PF_TRUE;
}

PF_API int pf_option_double(
    struct pf_argparser *p, const char *name, char shorthand, double *out
) {
    return out && pf__ap_match_double(p, name, shorthand, out);
}

PF_API int pf_option_float(
    struct pf_argparser *p, const char *name, char shorthand, double *out
) {
    double val;
    if (!out || !pf__ap_match_double(p, name, shorthand, &val))
        return PF_FALSE;

    *out = val;
    return PF_TRUE;
}

PF_API int pf_option_bool(
    struct pf_argparser *p, const char *name, char shorthand, pf_bool *out
) {
    if (!out || !pf__ap_match_option(p, name, shorthand))
        return PF_FALSE;

    if (!p->item.hasValue) {
        *out = PF_TRUE;
        return PF_TRUE;
    }

    if (!strcmp(p->item.value, "1") || !strcmp(p->item.value, "true")) {
        *out = PF_TRUE;
        return PF_TRUE;
    }

    if (!strcmp(p->item.value, "0") || !strcmp(p->item.value, "false")) {
        *out = PF_FALSE;
        return PF_TRUE;
    }

    pf_argparser_error(p, "invalid boolean '%s'\n", p->item.value);
    return PF_FALSE;
}

PF_API int pf_option_toggle(
    struct pf_argparser *p, const char *name, char shorthand, pf_bool *out
) {
    if (p->item.type == PF__ARG_LONG) {
        if (strcmp(p->item.name, name) == 0) {
            if (!p->item.hasValue) {
                *out = PF_TRUE;
                return PF_TRUE;
            }

            return pf_option_bool(p, name, shorthand, out);
        }

        if (strncmp(p->item.name, "no-", 3) == 0
            && strcmp(p->item.name + 3, name) == 0) {
            *out = PF_FALSE;
            return PF_TRUE;
        }
    }

    if (p->item.type == PF__ARG_SHORT && shorthand
        && p->item.shorthand == shorthand) {
        *out = PF_TRUE;
        return PF_TRUE;
    }

    return PF_FALSE;
}

PF_API int pf__ap_match_enum(
    struct pf_argparser *p,
    pf_option_enum_t *e,
    const char *arg,
    int len,
    int *out
) {
    for (int i = 0; e[i].name; i++) {
        if (0 == strncmp(arg, e[i].name, len)) {
            *out = e[i].value;
            return PF_TRUE;
        }
    }

    pf_argparser_error(p, "unknown enum constant '%.*s'\n", len, arg);
    return PF_FALSE;
}

PF_API int pf_option_enum(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    pf_option_enum_t *values,
    int *out
) {
    char *arg;

    if (!out || !values || !pf_option_string(p, name, shorthand, &arg))
        return PF_FALSE;

    return pf__ap_match_enum(p, values, arg, strlen(arg), out);
}

PF_API int pf_option_flags(
    struct pf_argparser *p,
    const char *name,
    char shorthand,
    pf_option_enum_t *values,
    int *out
) {
    char *arg, *next;
    int len, value;

    if (!out || !values || !pf_option_string(p, name, shorthand, &arg))
        return PF_FALSE;

    for (; arg; arg = next + 1) {
        next = strchr(arg, '|');
        len = next ? next - arg : strlen(arg);
        if (pf__ap_match_enum(p, values, arg, len, &value))
            return PF_FALSE;
        *out |= value;
    }

    return PF_TRUE;
}

PF_API int pf__ap_emit_param(
    struct pf_argparser *p, const char *arg, pf_argparser_fn cb, void *user
) {
    p->paramv[p->paramc++] = (char *)arg;
    p->item.type = PF__ARG_PARAM;
    p->item.param = arg;
    return cb(p, user);
}

PF_API int pf__ap_emit_long(
    struct pf_argparser *p, char *arg, pf_argparser_fn cb, void *user
) {
    char *eq = strchr(arg, '=');

    p->item.type = PF__ARG_LONG;

    if (eq) {
        *eq = 0;
        p->item.name = arg;
        p->item.value = eq + 1;
        p->item.hasValue = PF_TRUE;
    } else {
        p->item.name = arg;
        p->item.value = NULL;
        p->item.hasValue = PF_FALSE;
    }

    return cb(p, user);
}

PF_API int pf__ap_isalpha(char chr) {
    return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z');
}

PF_API int pf__ap_emit_short_cluster(
    struct pf_argparser *p, char *arg, pf_argparser_fn cb, void *user
) {
    int rc = PF_ARGPARSE_OK;

    while (!rc && *arg) {
        p->item.type = PF__ARG_SHORT;
        p->item.shorthand = *arg++;

        p->item.hasValue = PF_FALSE;
        p->item.value = NULL;

        if (*arg) {
            if (*arg == '=') {
                p->item.value = arg + 1;
                p->item.hasValue = PF_TRUE;
                arg += strlen(arg);
            } else if (!pf__ap_isalpha((unsigned char)*arg)) {
                p->item.value = arg;
                p->item.hasValue = PF_TRUE;
                arg += strlen(arg);
            }
        }

        rc = cb(p, user);

        if (p->item.hasValue)
            break;
    }

    return rc;
}

PF_API int pf_argparser_run(
    struct pf_argparser *p, pf_argparser_fn cb, void *user
) {
    int rc = PF_ARGPARSE_OK;

    for (int i = 1; i < p->argc; ++i) {
        if (rc || p->failed)
            break;

        char *arg = p->argv[i];

        if (p->pauseOptions || arg[0] != '-' || arg[1] == '\0')
            rc = pf__ap_emit_param(p, arg, cb, user);
        else if (strcmp(arg, "--") == 0)
            p->pauseOptions = PF_TRUE;
        else if (strncmp(arg, "--", 2) == 0)
            rc = pf__ap_emit_long(p, arg + 2, cb, user);
        else
            rc = pf__ap_emit_short_cluster(p, arg + 1, cb, user);
    }

    if (rc)
        return rc;
    return p->failed ? PF_ARGPARSE_EINVAL : PF_ARGPARSE_OK;
}

#endif
