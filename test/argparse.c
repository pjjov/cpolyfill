/*
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
*/

#include "check.h"
#include "../include/pf_argparse.h"
#include <float.h>
#include <limits.h>
#include <stddef.h>

/* ---------------------------------------------------------------- */
/* Harness                                                           */
/* ---------------------------------------------------------------- */

enum { MODE_FAST = 1, MODE_SLOW = 2, MODE_AUTO = 3 };
enum { FLAG_A = 1, FLAG_B = 2, FLAG_C = 4 };

static const pf_option_enum_t modes[] = {
    { "fast", MODE_FAST },
    { "slow", MODE_SLOW },
    { "auto", MODE_AUTO },
    { 0, 0 },
};

static const pf_option_enum_t flag_names[] = {
    { "a", FLAG_A },
    { "b", FLAG_B },
    { "c", FLAG_C },
    { 0, 0 },
};

static const pf_option_enum_t commands[] = {
    { "start", 1 },
    { "stop", 2 },
    { 0, 0 },
};

struct cfg {
    /* Inputs. */
    unsigned common; /* mask passed to pf_option_common */
    int stop_at;     /* callback returns EINTR on this call (1-based) */
    int fail_at;     /* callback returns EINVAL on this call */
    int use_subcommand;
    int allow_unknown;
    const char *prefix;

    /* Values set by options. */
    pf_bool all, other, flag, toggle;
    int count, level, mode, flags, cmd, enum_param;
    long lng, ranged;
    char *name;
    double ratio;
    float real;
    int verbosity_out;

    /* Results. */
    int rc;
    int calls;
    int paramc;
    char *params[16];
    struct pf_argparser snap; /* parser state after the run */
    char err[1024];
    char out[256];
};

static void slurp(FILE *f, char *buf, size_t size) {
    size_t n;
    fflush(f);
    rewind(f);
    n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
}

static int callback(struct pf_argparser *p, void *user) {
    struct cfg *c = user;
    int level;

    c->calls++;
    if (c->stop_at && c->calls == c->stop_at)
        return PF_ARGPARSE_EINTR;
    if (c->fail_at && c->calls == c->fail_at)
        return PF_ARGPARSE_EINVAL;

    if (c->common && pf_option_common(p, c->common))
        return 0;

    if (c->use_subcommand
        && pf_param_subcommand(p, "command", commands, &c->cmd))
        return 0;

    if (pf_option_bool(p, "all", 'a', &c->all))
        return 0;
    if (pf_option_bool(p, "other", 'o', &c->other))
        return 0;
    if (pf_option_toggle(p, "flag", 'f', &c->flag))
        return 0;
    if (pf_option_toggle(p, "toggle", 0, &c->toggle))
        return 0;
    if (pf_option_int(p, "count", 'n', &c->count))
        return 0;
    if (pf_option_int_range(p, "level", 'L', 1, 10, &c->level))
        return 0;
    if (pf_option_long(p, "long", 'l', &c->lng))
        return 0;
    if (pf_option_long_range(p, "ranged", 0, -5, 5, &c->ranged))
        return 0;
    if (pf_option_string(p, "name", 's', &c->name))
        return 0;
    if (pf_option_double(p, "ratio", 'r', &c->ratio))
        return 0;
    if (pf_option_float(p, "real", 0, &c->real))
        return 0;
    if (pf_option_enum(p, "mode", 'm', modes, &c->mode))
        return 0;
    if (pf_option_flags(p, "flags", 'F', flag_names, &c->flags))
        return 0;
    level = c->verbosity_out;
    if (pf_option_count(p, "chatty", 'c', &level)) {
        c->verbosity_out = level;
        return 0;
    }

    return 0;
}

static int parse(struct cfg *c, int argc, char **argv) {
    struct pf_argparser p;
    FILE *err = tmpfile(), *out = tmpfile();
    int i;

    CHECK(err != NULL && out != NULL);
    CHECK(pf_argparser_init(&p, argc, argv) == PF_ARGPARSE_OK);
    p.err = err;
    p.out = out;
    p.errPrefix = c->prefix;
    p.allowUnknown = c->allow_unknown ? PF_TRUE : PF_FALSE;

    c->calls = 0;
    c->rc = pf_argparser_run(&p, callback, c);

    c->paramc = p.paramc;
    for (i = 0; i < p.paramc && i < 16; i++)
        c->params[i] = p.paramv[i];
    c->snap = p;
    c->snap.paramv = NULL;
    slurp(err, c->err, sizeof c->err);
    slurp(out, c->out, sizeof c->out);

    pf_argparser_free(&p);
    fclose(err);
    fclose(out);
    return c->rc;
}

#define RUN(c, ...)                                                 \
    do {                                                            \
        char *argv_[] = { "prog", __VA_ARGS__, NULL };              \
        parse((c), (int)(sizeof argv_ / sizeof *argv_) - 1, argv_); \
    } while (0)

#define CFG(name)                 \
    struct cfg name;              \
    memset(&name, 0, sizeof name)

/* ---------------------------------------------------------------- */
/* Lifetime                                                          */
/* ---------------------------------------------------------------- */

static int test_argparse_init(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };

    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    CHECK(p.out == stdout && p.err == stderr);
    CHECK(p.paramc == 0 && p.paramv != NULL);
    CHECK(p.argc == 1 && p.argv == argv);
    pf_argparser_free(&p);
    CHECK(p.paramv == NULL);
    pf_argparser_free(&p); /* twice is fine */

    CHECK(pf_argparser_init(NULL, 1, argv) == PF_ARGPARSE_EINVAL);
    CHECK(pf_argparser_init(&p, 1, NULL) == PF_ARGPARSE_EINVAL);
    CHECK(pf_argparser_init(&p, -1, argv) == PF_ARGPARSE_EINVAL);

    /* No arguments at all. */
    CHECK(pf_argparser_init(&p, 0, argv) == PF_ARGPARSE_OK);
    CHECK(
        pf_argparser_run(&p, callback, &(struct cfg) { 0 }) == PF_ARGPARSE_OK
    );
    pf_argparser_free(&p);
    pf_argparser_free(NULL);
    return 0;
}

static int test_argparse_run_invalid(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    struct cfg c;

    memset(&c, 0, sizeof c);
    CHECK(pf_argparser_run(NULL, callback, &c) == PF_ARGPARSE_EINVAL);
    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    CHECK(pf_argparser_run(&p, NULL, &c) == PF_ARGPARSE_EINVAL);
    pf_argparser_free(&p);
    CHECK(pf_argparser_run(&p, callback, &c) == PF_ARGPARSE_EINVAL); /* freed */
    return 0;
}

/* ---------------------------------------------------------------- */
/* Parameters                                                        */
/* ---------------------------------------------------------------- */

static int test_argparse_params(int seed, int rep) {
    CFG(c);
    RUN(&c, "one", "two", "three");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK(c.paramc == 3);
    CHECK_STR(c.params[0], "one");
    CHECK_STR(c.params[1], "two");
    CHECK_STR(c.params[2], "three");
    CHECK(c.calls == 3);
    CHECK(c.err[0] == '\0');
    return 0;
}

static int test_argparse_params_mixed(int seed, int rep) {
    CFG(c);
    RUN(&c, "a", "--all", "b", "-n", "4", "c");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK(c.paramc == 3);
    CHECK_STR(c.params[0], "a");
    CHECK_STR(c.params[1], "b");
    CHECK_STR(c.params[2], "c");
    CHECK(c.all && c.count == 4);
    return 0;
}

static int test_argparse_double_dash(int seed, int rep) {
    CFG(c);
    RUN(&c, "--all", "--", "--other", "-n", "x");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK(c.all && !c.other && c.count == 0);
    CHECK(c.paramc == 3);
    CHECK_STR(c.params[0], "--other");
    CHECK_STR(c.params[1], "-n");
    CHECK_STR(c.params[2], "x");
    return 0;
}

static int test_argparse_single_dash(int seed, int rep) {
    CFG(c);
    RUN(&c, "-");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK(c.paramc == 1);
    CHECK_STR(c.params[0], "-"); /* conventionally standard input */
    return 0;
}

static int test_argparse_argv_untouched(int seed, int rep) {
    CFG(c);
    char a1[] = "--name=value", a2[] = "-n5", a3[] = "--all";
    char *argv[] = { "prog", a1, a2, a3, NULL };

    parse(&c, 4, argv);
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK_STR(a1, "--name=value");
    CHECK_STR(a2, "-n5");
    CHECK_STR(a3, "--all");
    return 0;
}

/* ---------------------------------------------------------------- */
/* Options                                                           */
/* ---------------------------------------------------------------- */

static int test_argparse_long_values(int seed, int rep) {
    CFG(c);
    RUN(&c, "--name=alice", "--count", "7", "--ratio=2.5");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK_STR(c.name, "alice");
    CHECK(c.count == 7);
    CHECK(c.ratio == 2.5);
    return 0;
}

static int test_argparse_short_values(int seed, int rep) {
    CFG(c);

    RUN(&c, "-n5");
    CHECK(c.rc == 0 && c.count == 5);

    memset(&c, 0, sizeof c);
    RUN(&c, "-n", "6");
    CHECK(c.rc == 0 && c.count == 6);

    memset(&c, 0, sizeof c);
    RUN(&c, "-n=7");
    CHECK(c.rc == 0 && c.count == 7);

    /* Letters after an option are more options, never a value. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-sbob");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "option '-s' requires a value") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "-s", "bob");
    CHECK(c.rc == 0);
    CHECK_STR(c.name, "bob");

    memset(&c, 0, sizeof c);
    RUN(&c, "-s=eve");
    CHECK(c.rc == 0);
    CHECK_STR(c.name, "eve");
    return 0;
}

static int test_argparse_short_cluster(int seed, int rep) {
    CFG(c);

    RUN(&c, "-ao");
    CHECK(c.rc == 0 && c.all && c.other);

    /* A value takes the rest of the cluster. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-aon12");
    CHECK(c.rc == 0 && c.all && c.other && c.count == 12);

    /* Or the next argument. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-aon", "13");
    CHECK(c.rc == 0 && c.all && c.other && c.count == 13);
    return 0;
}

static int test_argparse_negative_values(int seed, int rep) {
    CFG(c);

    /* The argument after an option that needs a value is that value. */
    RUN(&c, "--count", "-5", "--long", "-9000000");
    CHECK(c.rc == 0);
    CHECK(c.count == -5);
    CHECK(c.lng == -9000000);

    memset(&c, 0, sizeof c);
    RUN(&c, "-n-3");
    CHECK(c.rc == 0 && c.count == -3);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=-1.5e2");
    CHECK(c.rc == 0 && c.ratio == -150.0);
    return 0;
}

static int test_argparse_integer_forms(int seed, int rep) {
    CFG(c);

    RUN(&c, "--count=0x1F");
    CHECK(c.rc == 0 && c.count == 31);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=010");
    CHECK(c.rc == 0 && c.count == 8);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=+12");
    CHECK(c.rc == 0 && c.count == 12);

    memset(&c, 0, sizeof c);
    RUN(&c, "--long=2147483648", "--count=2147483647");
    CHECK(c.rc == 0 && c.count == INT_MAX);
    CHECK(c.lng == 2147483648L || sizeof(long) == 4);
    return 0;
}

static int test_argparse_integer_errors(int seed, int rep) {
    CFG(c);

    RUN(&c, "--count=abc");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "invalid integer 'abc'") != NULL);
    CHECK(strstr(c.err, "option '--count'") != NULL);
    CHECK(c.count == 0);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=12x");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);

    /* Beyond int, and beyond long. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--count=99999999999999999999");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "must be between") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--long=99999999999999999999");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "out of range") != NULL);
    return 0;
}

static int test_argparse_ranges(int seed, int rep) {
    CFG(c);

    RUN(&c, "--level=1", "--ranged=-5");
    CHECK(c.rc == 0 && c.level == 1 && c.ranged == -5);

    memset(&c, 0, sizeof c);
    RUN(&c, "--level=10", "--ranged=5");
    CHECK(c.rc == 0 && c.level == 10 && c.ranged == 5);

    memset(&c, 0, sizeof c);
    RUN(&c, "--level=0");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "must be between 1 and 10") != NULL);
    CHECK(strstr(c.err, "got '0'") != NULL);
    CHECK(c.level == 0);

    memset(&c, 0, sizeof c);
    RUN(&c, "-L11");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "option '-L'") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ranged=6");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "between -5 and 5") != NULL);
    return 0;
}

static int test_argparse_missing_value(int seed, int rep) {
    CFG(c);

    RUN(&c, "--name");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "option '--name' requires a value") != NULL);
    CHECK(c.name == NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--all", "-n");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "option '-n' requires a value") != NULL);
    return 0;
}

static int test_argparse_empty_value(int seed, int rep) {
    CFG(c);
    RUN(&c, "--name=");
    CHECK(c.rc == 0);
    CHECK(c.name != NULL);
    CHECK_STR(c.name, "");
    return 0;
}

static int test_argparse_double(int seed, int rep) {
    CFG(c);

    RUN(&c, "--ratio=3.14159", "--real=0.5");
    CHECK(c.rc == 0);
    CHECK(c.ratio > 3.14158 && c.ratio < 3.14160);
    CHECK(c.real == 0.5f);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1e300");
    CHECK(c.rc == 0 && c.ratio == 1e300);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=abc");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "invalid number 'abc'") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1.5x");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=inf");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "out of range") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1e999");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);

    /* Representable as double, but not as float. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--real=1e300");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "out of range") != NULL);
    return 0;
}

static int test_argparse_bool(int seed, int rep) {
    static const char *const yes[] = { "1", "true", "yes", "on" };
    static const char *const no[] = { "0", "false", "no", "off" };
    size_t i;
    CFG(c);

    RUN(&c, "--all");
    CHECK(c.rc == 0 && c.all);

    memset(&c, 0, sizeof c);
    RUN(&c, "-a");
    CHECK(c.rc == 0 && c.all);

    for (i = 0; i < 4; i++) {
        char arg[32];
        char *argv[3];

        memset(&c, 0, sizeof c);
        snprintf(arg, sizeof arg, "--all=%s", yes[i]);
        argv[0] = "prog";
        argv[1] = arg;
        argv[2] = NULL;
        parse(&c, 2, argv);
        CHECK(c.rc == 0 && c.all);

        memset(&c, 0, sizeof c);
        c.all = PF_TRUE;
        snprintf(arg, sizeof arg, "--all=%s", no[i]);
        parse(&c, 2, argv);
        CHECK(c.rc == 0 && !c.all);
    }

    memset(&c, 0, sizeof c);
    RUN(&c, "--all=maybe");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "invalid boolean 'maybe'") != NULL);
    return 0;
}

static int test_argparse_toggle(int seed, int rep) {
    CFG(c);

    RUN(&c, "--flag");
    CHECK(c.rc == 0 && c.flag);

    RUN(&c, "--no-flag");
    CHECK(c.rc == 0 && !c.flag);

    memset(&c, 0, sizeof c);
    RUN(&c, "--flag", "--no-flag");
    CHECK(c.rc == 0 && !c.flag);

    memset(&c, 0, sizeof c);
    RUN(&c, "--no-flag=1");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "does not take a value") != NULL);

    /* A long name only: no short form. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--toggle", "--no-toggle", "--toggle");
    CHECK(c.rc == 0 && c.toggle);

    /* `--no-` alone is not an option. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--no-");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    return 0;
}

static int test_argparse_count(int seed, int rep) {
    CFG(c);

    RUN(&c, "-c");
    CHECK(c.rc == 0 && c.verbosity_out == 1);

    memset(&c, 0, sizeof c);
    RUN(&c, "-ccc");
    CHECK(c.rc == 0 && c.verbosity_out == 3);

    memset(&c, 0, sizeof c);
    RUN(&c, "-c", "--chatty", "-c");
    CHECK(c.rc == 0 && c.verbosity_out == 3);

    memset(&c, 0, sizeof c);
    RUN(&c, "--chatty=5");
    CHECK(c.rc == 0 && c.verbosity_out == 5);

    memset(&c, 0, sizeof c);
    RUN(&c, "-c4");
    CHECK(c.rc == 0 && c.verbosity_out == 4);

    memset(&c, 0, sizeof c);
    RUN(&c, "--chatty=-1");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--chatty=lots");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    return 0;
}

static int test_argparse_enum(int seed, int rep) {
    CFG(c);

    RUN(&c, "--mode=slow");
    CHECK(c.rc == 0 && c.mode == MODE_SLOW);

    memset(&c, 0, sizeof c);
    RUN(&c, "-m", "auto");
    CHECK(c.rc == 0 && c.mode == MODE_AUTO);

    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=turbo");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "invalid value 'turbo'") != NULL);
    CHECK(strstr(c.err, "option '--mode'") != NULL);
    CHECK(strstr(c.err, "expected one of: fast, slow, auto") != NULL);
    CHECK(c.mode == 0);

    /* Matches whole names only. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=fas");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=fastest");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    return 0;
}

static int test_argparse_flags(int seed, int rep) {
    CFG(c);

    RUN(&c, "--flags=a");
    CHECK(c.rc == 0 && c.flags == FLAG_A);

    memset(&c, 0, sizeof c);
    RUN(&c, "--flags=a|c");
    CHECK(c.rc == 0 && c.flags == (FLAG_A | FLAG_C));

    memset(&c, 0, sizeof c);
    RUN(&c, "-F", "b,c");
    CHECK(c.rc == 0 && c.flags == (FLAG_B | FLAG_C));

    /* Repeating accumulates. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--flags=a", "--flags=b", "-F=c");
    CHECK(c.rc == 0 && c.flags == (FLAG_A | FLAG_B | FLAG_C));

    /* Nothing is written on error. */
    memset(&c, 0, sizeof c);
    c.flags = FLAG_C;
    RUN(&c, "--flags=a|x");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(c.flags == FLAG_C);
    CHECK(strstr(c.err, "invalid value 'x'") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--flags=a|");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    return 0;
}

static int test_argparse_long_enum_list(int seed, int rep) {
    /* The list of valid values is cut when it does not fit the message. */
    static const pf_option_enum_t many[] = {
        { "alpha-alpha-alpha", 1 },
        { "bravo-bravo-bravo", 2 },
        { "charlie-charlie-char", 3 },
        { "delta-delta-delta-d", 4 },
        { "echo-echo-echo-echo", 5 },
        { "foxtrot-foxtrot-fox", 6 },
        { "golf-golf-golf-golf", 7 },
        { "hotel-hotel-hotel-h", 8 },
        { "india-india-india-i", 9 },
        { "juliet-juliet-julie", 10 },
        { 0, 0 },
    };
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    char text[512];
    int out = 0;

    CHECK(err != NULL);
    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    p.err = err;
    CHECK(!pf__ap_match_enum(&p, many, "nope", 4, "thing", &out));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "invalid value 'nope' for thing") != NULL);
    CHECK(strstr(text, "alpha-alpha-alpha") != NULL);
    CHECK(strstr(text, "...") != NULL);
    CHECK(strstr(text, "juliet") == NULL);
    CHECK(p.failed);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_match_enum(int seed, int rep) {
    int v = 0;

    CHECK(pf_match_enum("slow", modes, &v) && v == MODE_SLOW);
    CHECK(pf_match_enum("fast", modes, &v) && v == MODE_FAST);
    v = 99;
    CHECK(!pf_match_enum("Slow", modes, &v) && v == 99); /* case-sensitive */
    CHECK(!pf_match_enum("", modes, &v));
    CHECK(!pf_match_enum(NULL, modes, &v));
    CHECK(!pf_match_enum("slow", NULL, &v));
    CHECK(!pf_match_enum("slow", modes, NULL));
    return 0;
}

/* ---------------------------------------------------------------- */
/* Errors                                                            */
/* ---------------------------------------------------------------- */

static int test_argparse_unknown(int seed, int rep) {
    CFG(c);

    RUN(&c, "--bogus");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "unknown option '--bogus'") != NULL);
    CHECK(c.snap.failed);

    memset(&c, 0, sizeof c);
    RUN(&c, "-z");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "unknown option '-z'") != NULL);

    /* With a value it is still just unknown. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--bogus=1");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "unknown option '--bogus'") != NULL);

    memset(&c, 0, sizeof c);
    c.allow_unknown = 1;
    RUN(&c, "--bogus", "-z", "--all");
    CHECK(c.rc == 0);
    CHECK(c.all);
    CHECK(c.err[0] == '\0');
    return 0;
}

static int test_argparse_failure_stops(int seed, int rep) {
    CFG(c);

    /* Nothing after the failing item is looked at. */
    RUN(&c, "--bogus", "--all", "param");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(!c.all);
    CHECK(c.calls == 1);
    CHECK(c.paramc == 0);

    /* A failing short option stops its cluster too. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-zao");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(!c.all && !c.other);
    return 0;
}

static int test_argparse_error_prefix(int seed, int rep) {
    CFG(c);

    c.prefix = "prog: error: ";
    RUN(&c, "--bogus");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strncmp(c.err, "prog: error: unknown option", 27) == 0);
    CHECK(c.err[strlen(c.err) - 1] == '\n');
    return 0;
}

static int test_argparse_error_function(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    char text[128];

    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    p.err = err;

    pf_argparser_error(&p, "bad %s #%d", "thing", 7);
    CHECK(p.failed);
    slurp(err, text, sizeof text);
    CHECK_STR(text, "bad thing #7\n");

    /* --quiet: still fails, but prints nothing. */
    p.failed = PF_FALSE;
    p.silent = PF_TRUE;
    pf_argparser_error(&p, "hidden");
    CHECK(p.failed);
    slurp(err, text, sizeof text);
    CHECK_STR(text, "bad thing #7\n");

    /* --help wins: neither failure nor output. */
    p.failed = PF_FALSE;
    p.silent = PF_FALSE;
    p.help = PF_TRUE;
    pf_argparser_error(&p, "ignored");
    CHECK(!p.failed);
    slurp(err, text, sizeof text);
    CHECK_STR(text, "bad thing #7\n");

    pf_argparser_error(NULL, "no parser");
    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_callback_control(int seed, int rep) {
    CFG(c);

    /* EINTR stops parsing without an error. */
    c.stop_at = 2;
    RUN(&c, "--all", "--other", "--flag");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK(c.calls == 2);
    CHECK(c.all && !c.flag);

    /* Any other return code is passed on. */
    memset(&c, 0, sizeof c);
    c.fail_at = 2;
    RUN(&c, "--all", "--other", "--flag");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(c.calls == 2);

    /* EINTR after errors were reported is still a failure. */
    memset(&c, 0, sizeof c);
    c.stop_at = 2;
    RUN(&c, "--bogus", "--all");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    return 0;
}

static int test_argparse_rerun(int seed, int rep) {
    struct pf_argparser p;
    struct cfg c;
    char *argv[] = { "prog", "x", "--all", "y", NULL };
    FILE *err = tmpfile();

    memset(&c, 0, sizeof c);
    CHECK(pf_argparser_init(&p, 4, argv) == PF_ARGPARSE_OK);
    p.err = err;

    CHECK(pf_argparser_run(&p, callback, &c) == PF_ARGPARSE_OK);
    CHECK(p.paramc == 2);
    c.all = PF_FALSE;
    CHECK(pf_argparser_run(&p, callback, &c) == PF_ARGPARSE_OK);
    CHECK(p.paramc == 2); /* does not accumulate */
    CHECK(c.all);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_null_arguments(int seed, int rep) {
    int i = 0;
    char *s = NULL;
    long l = 0;
    pf_bool b = PF_FALSE;

    CHECK(!pf_is_param(NULL, 0));
    CHECK(!pf_is_option(NULL));
    CHECK(!pf_cmp_param(NULL, "x"));
    CHECK(!pf_option_bool(NULL, "x", 0, &b));
    CHECK(!pf_option_string(NULL, "x", 0, &s));
    CHECK(!pf_option_int(NULL, "x", 0, &i));
    CHECK(!pf_option_long(NULL, "x", 0, &l));
    CHECK(!pf_option_toggle(NULL, "x", 0, &b));
    CHECK(!pf_option_count(NULL, "x", 0, &i));
    CHECK(!pf_option_common(NULL, PF_COMMON_ALL));
    CHECK(!pf_option_help(NULL, &b));
    CHECK(!pf_param_subcommand(NULL, "x", commands, &i));
    CHECK(!pf_argparser_expect_params(NULL, 0, 0, "x"));
    CHECK(pf_next_token(NULL, '+') == NULL);
    pf_argparser_consume(NULL);
    pf_argparser_print_version(NULL, "a", "b");

    {
        struct pf_argparser p;
        char *argv[] = { "prog", NULL };
        long v;
        CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
        CHECK(!pf_arg_long(&p, NULL, "x", 0, 1, &v));
        CHECK(!pf_arg_long(&p, "1", "x", 0, 1, NULL));
        CHECK(!pf_arg_long(NULL, "1", "x", 0, 1, &v));
        CHECK(!p.failed); /* bad arguments are not user errors */
        pf_argparser_free(&p);
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Conversion helpers                                                */
/* ---------------------------------------------------------------- */

static int test_argparse_arg_long(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    long v = 0;
    int i = 0;
    double d = 0;
    char text[256];

    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    p.err = err;

    CHECK(pf_arg_long(&p, "42", "answer", 0, 100, &v) && v == 42);
    CHECK(pf_arg_long(&p, "-42", "answer", -100, 100, &v) && v == -42);
    CHECK(pf_arg_long(&p, "0", NULL, LONG_MIN, LONG_MAX, &v) && v == 0);
    CHECK(!p.failed);

    CHECK(!pf_arg_long(&p, "101", "answer", 0, 100, &v));
    CHECK(p.failed);
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "answer must be between 0 and 100; got '101'") != NULL);

    p.failed = PF_FALSE;
    CHECK(!pf_arg_long(&p, "zz", NULL, 0, 1, &v));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "invalid integer 'zz' for value") != NULL);

    CHECK(pf_arg_int(&p, "7", "n", 0, 10, &i) && i == 7);
    CHECK(!pf_arg_int(&p, "7", "n", 8, 10, &i) && i == 7);
    CHECK(!pf_arg_int(&p, "7", "n", 0, 10, NULL));

    CHECK(pf_arg_double(&p, "2.5", "x", &d) && d == 2.5);
    CHECK(!pf_arg_double(&p, "", "x", &d));
    CHECK(!pf_arg_double(&p, " ", "x", &d));
    CHECK(!pf_arg_double(&p, NULL, "x", &d));

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_next_token(int seed, int rep) {
    char text[] = "a+b++c";
    char *cursor = text, *tok;
    static const char *const want[] = { "a", "b", "", "c" };
    int i = 0;

    while ((tok = pf_next_token(&cursor, '+')) != NULL) {
        CHECK(i < 4);
        CHECK_STR(tok, want[i]);
        i++;
    }
    CHECK(i == 4);
    CHECK(cursor == NULL);
    CHECK(pf_next_token(&cursor, '+') == NULL);

    {
        char one[] = "single";
        char *c1 = one;
        CHECK_STR(pf_next_token(&c1, ','), "single");
        CHECK(c1 == NULL);
    }
    {
        char trailing[] = "x,";
        char *c2 = trailing;
        CHECK_STR(pf_next_token(&c2, ','), "x");
        CHECK_STR(pf_next_token(&c2, ','), ""); /* the empty last token */
        CHECK(pf_next_token(&c2, ',') == NULL);
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Positional parameters                                             */
/* ---------------------------------------------------------------- */

struct pcheck {
    int calls;
    int index_hits[4];
    int cmp_hit;
    int match_hit;
    int enum_hit;
    int enum_value;
};

static int param_cb(struct pf_argparser *p, void *user) {
    static const char *const names[] = { "red", "green", "blue", NULL };
    static const pf_option_enum_t colors[] = { { "red", 1 },
                                               { "blue", 2 },
                                               { 0, 0 } };
    struct pcheck *c = user;
    char *match;
    int i;

    c->calls++;
    for (i = 0; i < 4; i++)
        if (pf_is_param(p, i))
            c->index_hits[i]++;
    if (pf_cmp_param(p, "special"))
        c->cmp_hit++;
    if (pf_match_param(p, names, &match))
        c->match_hit++;
    if (pf_param_enum(p, 0, "color", colors, &c->enum_value))
        c->enum_hit++;
    return 0;
}

static int test_argparse_param_helpers(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", "red", "special", "green", "--opt", "x", NULL };
    struct pcheck c;
    FILE *err = tmpfile();

    memset(&c, 0, sizeof c);
    CHECK(pf_argparser_init(&p, 6, argv) == PF_ARGPARSE_OK);
    p.err = err;
    p.allowUnknown = PF_TRUE;
    CHECK(pf_argparser_run(&p, param_cb, &c) == PF_ARGPARSE_OK);

    /* Parameters: red, special, green, x. The option is not one. */
    CHECK(c.calls == 5);
    CHECK(c.index_hits[0] == 1 && c.index_hits[1] == 1);
    CHECK(c.index_hits[2] == 1 && c.index_hits[3] == 1);
    CHECK(c.cmp_hit == 1);
    CHECK(c.match_hit == 2); /* red and green */
    CHECK(c.enum_hit == 1);  /* only the first parameter */
    CHECK(c.enum_value == 1);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_param_enum_error(int seed, int rep) {
    CFG(c);
    char *argv[] = { "prog", "purple", NULL };
    struct pf_argparser p;
    static const pf_option_enum_t colors[] = { { "red", 1 }, { 0, 0 } };
    int out = 0;
    FILE *err = tmpfile();
    char text[128];

    (void)c;
    CHECK(pf_argparser_init(&p, 2, argv) == PF_ARGPARSE_OK);
    p.err = err;
    p.paramc = 1;
    p.paramv[0] = argv[1];
    p.item.type = PF__ARG_PARAM;
    p.item.param = argv[1];
    CHECK(!pf_param_enum(&p, 0, "color", colors, &out));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "invalid value 'purple' for color") != NULL);
    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_subcommand(int seed, int rep) {
    CFG(c);

    c.use_subcommand = 1;
    RUN(&c, "start", "alpha", "beta");
    CHECK(c.rc == 0);
    CHECK(c.cmd == 1);
    CHECK(c.paramc == 2); /* the subcommand itself is not a parameter */
    CHECK_STR(c.params[0], "alpha");
    CHECK_STR(c.params[1], "beta");

    /* Options may come first. */
    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "--all", "stop", "-n", "3", "arg");
    CHECK(c.rc == 0);
    CHECK(c.cmd == 2 && c.all && c.count == 3);
    CHECK(c.paramc == 1);
    CHECK_STR(c.params[0], "arg");

    /* Only the first parameter is looked up. */
    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "start", "stop");
    CHECK(c.rc == 0 && c.cmd == 1);
    CHECK(c.paramc == 1);
    CHECK_STR(c.params[0], "stop");

    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "restart");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "invalid value 'restart' for command") != NULL);
    CHECK(strstr(c.err, "start, stop") != NULL);

    /* Nothing to look up. */
    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "--all");
    CHECK(c.rc == 0 && c.cmd == 0 && c.all);
    return 0;
}

static int test_argparse_expect_params(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    char text[256];

    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    p.err = err;

    p.paramc = 2;
    CHECK(pf_argparser_expect_params(&p, 1, 3, "cmd"));
    CHECK(pf_argparser_expect_params(&p, 2, 2, "cmd"));
    CHECK(pf_argparser_expect_params(&p, 0, -1, "cmd"));
    CHECK(!p.failed);

    CHECK(!pf_argparser_expect_params(&p, 0, 0, "cmd"));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "cmd takes no arguments; got 2") != NULL);

    p.failed = PF_FALSE;
    CHECK(!pf_argparser_expect_params(&p, 1, 1, "sub 'x'"));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "sub 'x' requires exactly 1 argument; got 2") != NULL);

    CHECK(!pf_argparser_expect_params(&p, 3, 3, "cmd"));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "requires exactly 3 arguments; got 2") != NULL);

    CHECK(!pf_argparser_expect_params(&p, 3, -1, "cmd"));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "requires at least 3 arguments; got 2") != NULL);

    CHECK(!pf_argparser_expect_params(&p, 0, 1, NULL));
    slurp(err, text, sizeof text);
    CHECK(strstr(text, "command takes at most 1 argument; got 2") != NULL);
    CHECK(p.failed);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_consume(int seed, int rep) {
    CFG(c);

    c.use_subcommand = 1;
    RUN(&c, "start");
    CHECK(c.paramc == 0);

    pf_argparser_consume(NULL);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Common options                                                    */
/* ---------------------------------------------------------------- */

static int test_argparse_common_verbose(int seed, int rep) {
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "-v");
    CHECK(c.rc == 0 && c.snap.verbosity == 1 && c.snap.verbose);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-vvv");
    CHECK(c.rc == 0 && c.snap.verbosity == 3);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--verbose=2");
    CHECK(c.rc == 0 && c.snap.verbosity == 2);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-vv", "--no-verbose");
    CHECK(c.rc == 0 && c.snap.verbosity == 0 && !c.snap.verbose);
    return 0;
}

static int test_argparse_common_quiet(int seed, int rep) {
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "-q");
    CHECK(c.rc == 0 && c.snap.silent);

    RUN(&c, "--quiet", "--no-quiet");
    CHECK(c.rc == 0 && !c.snap.silent);

    /* A silent parse still fails, but prints nothing. */
    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-q", "--bogus");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(c.err[0] == '\0');
    return 0;
}

static int test_argparse_common_help_version(int seed, int rep) {
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "--help");
    CHECK(c.rc == 0 && c.snap.help);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-?");
    CHECK(c.rc == 0 && c.snap.help);

    /* Asking for help mutes failures, so usage text can win. */
    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--help", "--bogus");
    CHECK(c.rc == PF_ARGPARSE_OK);
    CHECK(c.snap.help);
    CHECK(c.err[0] == '\0');

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--version");
    CHECK(c.rc == 0 && c.snap.version);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-V");
    CHECK(c.rc == 0 && c.snap.version);

    /* Only the selected options are recognised. */
    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_VERBOSE;
    RUN(&c, "--help");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(!c.snap.help);
    return 0;
}

static int test_argparse_common_color(int seed, int rep) {
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "--color");
    CHECK(c.rc == 0 && c.snap.colorMode == PF_COLOR_ALWAYS && c.snap.colorSet);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=never");
    CHECK(c.rc == 0 && c.snap.colorMode == PF_COLOR_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=auto");
    CHECK(c.rc == 0 && c.snap.colorMode == PF_COLOR_AUTO && c.snap.colorSet);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=yes");
    CHECK(c.rc == 0 && c.snap.colorMode == PF_COLOR_ALWAYS);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=no");
    CHECK(c.rc == 0 && c.snap.colorMode == PF_COLOR_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color", "--no-color");
    CHECK(c.rc == 0 && c.snap.colorMode == PF_COLOR_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=rainbow");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "invalid value 'rainbow'") != NULL);
    CHECK(!c.snap.colorSet);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--all");
    CHECK(!c.snap.colorSet && c.snap.colorMode == PF_COLOR_AUTO);
    return 0;
}

static int test_argparse_common_interactive(int seed, int rep) {
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "-i");
    CHECK(c.rc == 0 && c.snap.interactiveMode == PF_INTERACTIVE_ALWAYS);
    CHECK(c.snap.interactiveSet);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--interactive=never");
    CHECK(c.rc == 0 && c.snap.interactiveMode == PF_INTERACTIVE_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--no-interactive");
    CHECK(c.rc == 0 && c.snap.interactiveMode == PF_INTERACTIVE_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--interactive=sometimes");
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    return 0;
}

static int test_argparse_print_version(int seed, int rep) {
    CFG(c);
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };
    FILE *out = tmpfile();

    (void)c;
    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    p.out = out;
    pf_argparser_print_version(&p, "tool", "1.2.3");
    pf_argparser_print_version(&p, NULL, NULL);
    slurp(out, c.out, sizeof c.out);
    CHECK(strncmp(c.out, "tool 1.2.3\n", 11) == 0);
    CHECK_STR(c.out + 11, " \n");

    p.out = NULL; /* nothing to write to */
    pf_argparser_print_version(&p, "tool", "1");

    pf_argparser_free(&p);
    fclose(out);
    return 0;
}

static int test_argparse_without_cli(int seed, int rep) {
    struct pf_argparser p;
    char *argv[] = { "prog", NULL };

    CHECK(pf_argparser_init(&p, 1, argv) == PF_ARGPARSE_OK);
    pf_argparser_set_cli(&p, NULL);
    CHECK(p.cli == NULL);
    pf_argparser_set_cli(NULL, NULL);
    pf_argparser_free(&p);
    return 0;
}

static int test_argparse_long_name(int seed, int rep) {
    /* Names longer than the internal copy are cut, and match nothing. */
    CFG(c);
    char arg[400];
    char *argv[3];

    memset(arg, 'x', sizeof arg - 1);
    arg[0] = arg[1] = '-';
    arg[sizeof arg - 1] = '\0';
    argv[0] = "prog";
    argv[1] = arg;
    argv[2] = NULL;

    parse(&c, 2, argv);
    CHECK(c.rc == PF_ARGPARSE_EINVAL);
    CHECK(strstr(c.err, "unknown option") != NULL);
    return 0;
}

pf_test_t suite_argparse[] = {
    { test_argparse_init, "/argparse/init" },
    { test_argparse_run_invalid, "/argparse/run_invalid" },
    { test_argparse_params, "/argparse/params" },
    { test_argparse_params_mixed, "/argparse/params_mixed" },
    { test_argparse_double_dash, "/argparse/double_dash" },
    { test_argparse_single_dash, "/argparse/single_dash" },
    { test_argparse_argv_untouched, "/argparse/argv_untouched" },
    { test_argparse_long_values, "/argparse/long_values" },
    { test_argparse_short_values, "/argparse/short_values" },
    { test_argparse_short_cluster, "/argparse/short_cluster" },
    { test_argparse_negative_values, "/argparse/negative_values" },
    { test_argparse_integer_forms, "/argparse/integer_forms" },
    { test_argparse_integer_errors, "/argparse/integer_errors" },
    { test_argparse_ranges, "/argparse/ranges" },
    { test_argparse_missing_value, "/argparse/missing_value" },
    { test_argparse_empty_value, "/argparse/empty_value" },
    { test_argparse_double, "/argparse/double" },
    { test_argparse_bool, "/argparse/bool" },
    { test_argparse_toggle, "/argparse/toggle" },
    { test_argparse_count, "/argparse/count" },
    { test_argparse_enum, "/argparse/enum" },
    { test_argparse_flags, "/argparse/flags" },
    { test_argparse_long_enum_list, "/argparse/long_enum_list" },
    { test_argparse_match_enum, "/argparse/match_enum" },
    { test_argparse_unknown, "/argparse/unknown" },
    { test_argparse_failure_stops, "/argparse/failure_stops" },
    { test_argparse_error_prefix, "/argparse/error_prefix" },
    { test_argparse_error_function, "/argparse/error_function" },
    { test_argparse_callback_control, "/argparse/callback_control" },
    { test_argparse_rerun, "/argparse/rerun" },
    { test_argparse_null_arguments, "/argparse/null_arguments" },
    { test_argparse_arg_long, "/argparse/arg_long" },
    { test_argparse_next_token, "/argparse/next_token" },
    { test_argparse_param_helpers, "/argparse/param_helpers" },
    { test_argparse_param_enum_error, "/argparse/param_enum_error" },
    { test_argparse_subcommand, "/argparse/subcommand" },
    { test_argparse_expect_params, "/argparse/expect_params" },
    { test_argparse_consume, "/argparse/consume" },
    { test_argparse_common_verbose, "/argparse/common_verbose" },
    { test_argparse_common_quiet, "/argparse/common_quiet" },
    { test_argparse_common_help_version, "/argparse/common_help_version" },
    { test_argparse_common_color, "/argparse/common_color" },
    { test_argparse_common_interactive, "/argparse/common_interactive" },
    { test_argparse_print_version, "/argparse/print_version" },
    { test_argparse_without_cli, "/argparse/without_cli" },
    { test_argparse_long_name, "/argparse/long_name" },
    { 0 },
};
