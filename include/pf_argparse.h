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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum pf_argparse_bool {
    PF_ARGPARSE_FALSE = 0,
    PF_ARGPARSE_TRUE,
};

enum pf_argparse_error {
    PF_ARGPARSE_OK = 0,
    PF_ARGPARSE_EINTR = -4,
    PF_ARGPARSE_EINVAL = -22,
};

enum pf_option_kind {
    PF_OPT_CALL = 0,
    PF_OPT_BOOL = 2,
    PF_OPT_STR = 3,
};

typedef int(pf_option_fn)(char **arg, void *data);

struct pf_option {
    const char *name;
    char shorthand;
    char kind;
    void *data;
};

struct pf_option_info {
    const char *aliases;
    const char *description;
};

struct pf_argparser {
    const char *name;
    const char *description;
    const char *epilog;
    const char *usage;
    const char *errorInfo;
    FILE *errorLog;
    struct pf_option *options;
    struct pf_option_info *infos;
    struct pf_argparser *parent;

    unsigned int stopAtFirst : 1;
    unsigned int silent : 1;
    unsigned int helpMode : 1;

    int argc;
    char **argv;
    void *user;
};

PF_API void pf__argparse_errorf(struct pf_argparser *p, const char *fmt, ...) {
    if (p->silent || p->helpMode)
        return;

    va_list args;
    va_start(args, fmt);
    vfprintf(p->errorLog, fmt, args);
    fputc('\n', p->errorLog);
    va_end(args);
}

PF_API int pf__argparse_distance(const char *a, const char *b) {
    int s = 0;
    while (*a && *b)
        s += *a++ != *b++;
    while (*a++)
        s++;
    while (*b++)
        s++;
    return s;
}

PF_API void pf__argparse_similar(struct pf_argparser *p, char *arg) {
    const struct pf_option *opt, *out;
    int min = 1e9, d;

    for (opt = p->options; opt->name; opt++) {
        d = pf__argparse_distance(arg, opt->name);

        if (d < min) {
            min = d;
            out = opt;
        }
    }

    pf__argparse_errorf(
        p,
        "%s: Unknown option '--%s'; did you mean '--%s'?",
        p->name,
        arg,
        out->name
    );
}

PF_API const struct pf_option *pf__argparse_long(
    struct pf_argparser *p, char *arg
) {
    const struct pf_option *opt;

    for (opt = p->options; opt->name; opt++)
        if (0 == strcmp(opt->name, arg))
            return opt;

    if (p->parent)
        return pf__argparse_long(p->parent, arg);

    pf__argparse_similar(p, arg);
    return NULL;
}

PF_API const struct pf_option *pf__argparse_short(
    struct pf_argparser *p, char *arg
) {
    const struct pf_option *opt, *out;

    for (; *arg; arg++) {
        out = NULL;

        for (opt = p->options; !out && opt->name; opt++)
            if (opt->shorthand == *arg)
                out = opt;

        if (!out || out->kind != PF_OPT_BOOL)
            break;

        *(char *)out->data = PF_ARGPARSE_TRUE;
    }

    if (arg[1] != '\0') {
        if (p->parent)
            return pf__argparse_short(p->parent, arg);

        pf__argparse_errorf(
            p,
            out && out->kind != PF_OPT_BOOL
                ? "%s: Option '%c' must be the last or specified separately.\n"
                : "%s: Unknown short option '%c'.",
            p->name,
            *arg
        );

        return NULL;
    }

    return out;
}

PF_API int pf__argparse_parse(
    struct pf_argparser *p, char *arg, const struct pf_option **opt
) {
    if (arg[0] != '-' || arg[0] == '\0') {
        p->argv[p->argc++] = arg;
        return PF_ARGPARSE_OK;
    }

    if (arg[1] == '-' && arg[2] == '\0')
        return PF_ARGPARSE_EINTR;
    else if (arg[1] == '-')
        *opt = pf__argparse_long(p, arg + 2);
    else
        *opt = pf__argparse_short(p, arg + 1);

    return *opt ? PF_ARGPARSE_OK : PF_ARGPARSE_EINVAL;
}

PF_API int pf__argparse_save(
    struct pf_argparser *p, char **argv, const struct pf_option *opt
) {
    switch (opt->kind) {
    case PF_OPT_STR:
        if (argv[1] == NULL) {
            pf__argparse_errorf(
                p, "Option '--%s' must be followed by an argument.", opt->name
            );

            return PF_ARGPARSE_EINVAL;
        }

        *(const char **)opt->data = argv[1];
        return 1;

    case PF_OPT_BOOL:
        *(char *)opt->data = PF_ARGPARSE_TRUE;
        return 0;

    case PF_OPT_CALL:
        if (!opt->data) {
            pf__argparse_errorf(
                p, "No handler specified for option '--%s'.", opt->name
            );

            return PF_ARGPARSE_EINVAL;
        }

        return ((pf_option_fn *)opt->data)(argv, p->user);

    default:
        return PF_ARGPARSE_EINVAL;
    }
}

PF_API int pf_argparse(struct pf_argparser *p, int argc, char **argv) {
    if (!p || !argv || argc == 0)
        return PF_ARGPARSE_EINVAL;

    if (!p->errorLog)
        p->errorLog = stderr;

    if (!p->name)
        p->name = argv[0];

    argc--;
    argv++;
    p->argc = 0;
    p->argv = argv;

    const struct pf_option *opt;
    int i, result = 0;

    for (i = 0; !result && i < argc; i++) {
        char *arg = argv[i];
        opt = NULL;

        if ((result = pf__argparse_parse(p, arg, &opt)))
            break;

        if (opt != NULL)
            result = pf__argparse_save(p, &argv[i], opt);
        else if (p->stopAtFirst)
            result = PF_ARGPARSE_EINTR;

        if (result > 0) {
            i += result;
            result = 0;
        }
    }

    if (result == PF_ARGPARSE_EINTR) {
        for (; i < argc; i++)
            p->argv[p->argc++] = argv[i];
        return PF_ARGPARSE_OK;
    }

    if (result) {
        fputs(p->errorInfo, p->errorLog);
        return result;
    }

    return p->argc;
}

PF_API int pf_arghelp(struct pf_argparser *p, FILE *out) {
    if (!p)
        return PF_ARGPARSE_EINVAL;

    if (p->silent)
        return PF_ARGPARSE_OK;

    if (!out)
        out = stdout;

    fprintf(
        out, p->usage ? p->usage : "usage: %s [OPTIONS] <arguments>\n", p->name
    );

    fputs(p->description ? p->description : "\n", out);
    struct pf_option_info *opt;

    for (opt = p->infos; opt->aliases; opt++) {
        int pad = 16 - strlen(opt->aliases);
        fprintf(out, "  %s", opt->aliases);

        if (opt->description) {
            for (int i = 0; i < pad; i++)
                fputc(' ', out);
            fprintf(out, " %s\n", opt->description);
        } else {
            fputc('\n', out);
        }
    }

    if (p->epilog)
        fputs(p->epilog, out);
    return PF_ARGPARSE_OK;
}

#endif
