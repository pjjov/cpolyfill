/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/argparse.h>
#include <cpolyfill/errno.h>

#include <float.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <string.h>

#include "check.h"
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
    bool all, other, flag, toggle;
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
    pf_argparser_t snap; /* parser state after the run */
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

static int callback(pf_argparser_t *p, void *user) {
    struct cfg *c = user;
    int level;

    c->calls++;
    if (c->stop_at && c->calls == c->stop_at)
        return PF_EINTR;
    if (c->fail_at && c->calls == c->fail_at)
        return PF_EINVAL;

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
    pf_argparser_t p;
    FILE *err = tmpfile(), *out = tmpfile();
    int i;

    PF_ASSERT_ALWAYS(err != NULL && out != NULL);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, argc, argv) == PF_OK);
    p.err = err;
    p.out = out;
    p.err_prefix = c->prefix;
    p.allow_unknown = c->allow_unknown ? true : false;

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
    (void)seed, (void)rep;
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };

    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    PF_ASSERT_ALWAYS(p.out == stdout && p.err == stderr);
    PF_ASSERT_ALWAYS(p.paramc == 0 && p.paramv != NULL);
    PF_ASSERT_ALWAYS(p.pf__argc == 1 && p.pf__argv == argv);
    pf_argparser_free(&p);
    PF_ASSERT_ALWAYS(p.paramv == NULL);
    pf_argparser_free(&p); /* twice is fine */

    PF_ASSERT_ALWAYS(pf_argparser_init(NULL, 1, argv) == PF_EINVAL);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, NULL) == PF_EINVAL);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, -1, argv) == PF_EINVAL);

    /* No arguments at all. */
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 0, argv) == PF_OK);
    PF_ASSERT_ALWAYS(
        pf_argparser_run(&p, callback, &(struct cfg) { 0 }) == PF_OK
    );
    pf_argparser_free(&p);
    pf_argparser_free(NULL);
    return 0;
}

static int test_argparse_run_invalid(int seed, int rep) {
    (void)seed, (void)rep;
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };
    struct cfg c;

    memset(&c, 0, sizeof c);
    PF_ASSERT_ALWAYS(pf_argparser_run(NULL, callback, &c) == PF_EINVAL);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    PF_ASSERT_ALWAYS(pf_argparser_run(&p, NULL, &c) == PF_EINVAL);
    pf_argparser_free(&p);
    PF_ASSERT_ALWAYS(
        pf_argparser_run(&p, callback, &c) == PF_EINVAL
    ); /* freed */
    return 0;
}

/* ---------------------------------------------------------------- */
/* Parameters                                                        */
/* ---------------------------------------------------------------- */

static int test_argparse_params(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    RUN(&c, "one", "two", "three");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS(c.paramc == 3);
    PF_ASSERT_ALWAYS_STR_EQ(c.params[0], "one");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[1], "two");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[2], "three");
    PF_ASSERT_ALWAYS(c.calls == 3);
    PF_ASSERT_ALWAYS(c.err[0] == '\0');
    return 0;
}

static int test_argparse_params_mixed(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    RUN(&c, "a", "--all", "b", "-n", "4", "c");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS(c.paramc == 3);
    PF_ASSERT_ALWAYS_STR_EQ(c.params[0], "a");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[1], "b");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[2], "c");
    PF_ASSERT_ALWAYS(c.all && c.count == 4);
    return 0;
}

static int test_argparse_double_dash(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    RUN(&c, "--all", "--", "--other", "-n", "x");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS(c.all && !c.other && c.count == 0);
    PF_ASSERT_ALWAYS(c.paramc == 3);
    PF_ASSERT_ALWAYS_STR_EQ(c.params[0], "--other");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[1], "-n");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[2], "x");
    return 0;
}

static int test_argparse_single_dash(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    RUN(&c, "-");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS(c.paramc == 1);
    PF_ASSERT_ALWAYS_STR_EQ(
        c.params[0], "-"
    ); /* conventionally standard input */
    return 0;
}

static int test_argparse_argv_untouched(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    char a1[] = "--name=value", a2[] = "-n5", a3[] = "--all";
    char *argv[] = { "prog", a1, a2, a3, NULL };

    parse(&c, 4, argv);
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(a1, "--name=value");
    PF_ASSERT_ALWAYS_STR_EQ(a2, "-n5");
    PF_ASSERT_ALWAYS_STR_EQ(a3, "--all");
    return 0;
}

/* ---------------------------------------------------------------- */
/* Options                                                           */
/* ---------------------------------------------------------------- */

static int test_argparse_long_values(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    RUN(&c, "--name=alice", "--count", "7", "--ratio=2.5");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS_STR_EQ(c.name, "alice");
    PF_ASSERT_ALWAYS(c.count == 7);
    PF_ASSERT_ALWAYS(c.ratio == 2.5);
    return 0;
}

static int test_argparse_short_values(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "-n5");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 5);

    memset(&c, 0, sizeof c);
    RUN(&c, "-n", "6");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 6);

    memset(&c, 0, sizeof c);
    RUN(&c, "-n=7");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 7);

    /* As getopt: an option taking a value gets the rest of the cluster,
       letters included, and flags before it stay flags. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-sbob");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS_STR_EQ(c.name, "bob");

    memset(&c, 0, sizeof c);
    RUN(&c, "-aosbob", "-n", "2");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.all && c.other && c.count == 2);
    PF_ASSERT_ALWAYS_STR_EQ(c.name, "bob");

    memset(&c, 0, sizeof c);
    RUN(&c, "-s");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "option '-s' requires a value") != NULL);

    /* After a flag, anything else is another option (v1 took "-b" as the
       value of -a here). */
    memset(&c, 0, sizeof c);
    RUN(&c, "-a-o");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "unknown option '--'") != NULL);

    /* A flag takes an explicit value only after '='. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-a=no", "-o=yes");
    PF_ASSERT_ALWAYS(c.rc == 0 && !c.all && c.other);

    memset(&c, 0, sizeof c);
    RUN(&c, "-s", "bob");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS_STR_EQ(c.name, "bob");

    memset(&c, 0, sizeof c);
    RUN(&c, "-s=eve");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS_STR_EQ(c.name, "eve");
    return 0;
}

static int test_argparse_short_cluster(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "-ao");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.all && c.other);

    /* A value takes the rest of the cluster. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-aon12");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.all && c.other && c.count == 12);

    /* Or the next argument. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-aon", "13");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.all && c.other && c.count == 13);
    return 0;
}

static int test_argparse_negative_values(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    /* The argument after an option that needs a value is that value. */
    RUN(&c, "--count", "-5", "--long", "-9000000");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS(c.count == -5);
    PF_ASSERT_ALWAYS(c.lng == -9000000);

    memset(&c, 0, sizeof c);
    RUN(&c, "-n-3");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == -3);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=-1.5e2");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.ratio == -150.0);
    return 0;
}

static int test_argparse_integer_forms(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--count=0x1F");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 31);

    /* Leading zeros are decimal, not octal. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--count=010");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 10);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=08");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 8);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=0b101", "--long=-0x10");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 5 && c.lng == -16);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=0XfF");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 255);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=0x");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=0b102");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count= 5");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--long=-9223372036854775808");
    PF_ASSERT_ALWAYS(
        sizeof(long) == 4 ? c.rc == PF_EINVAL : c.rc == 0 && c.lng == LONG_MIN
    );

    memset(&c, 0, sizeof c);
    RUN(&c, "--long=0x8000000000000000");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=+12");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == 12);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=2147483647");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.count == INT_MAX);

    /* Beyond int, within a 64-bit long. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--long=2147483648");
    if (sizeof(long) > 4)
        PF_ASSERT_ALWAYS(c.rc == 0 && c.lng == 2147483648L);
    else
        PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

static int test_argparse_integer_errors(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--count=abc");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "invalid integer 'abc'") != NULL);
    PF_ASSERT_ALWAYS(strstr(c.err, "option '--count'") != NULL);
    PF_ASSERT_ALWAYS(c.count == 0);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=12x");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--count=");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    /* Beyond int, and beyond long. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--count=99999999999999999999");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    /* With a 32-bit long, int's range is all of long's. */
    PF_ASSERT_ALWAYS(
        strstr(
            c.err,
            sizeof(long) > sizeof(int) ? "must be between" : "is out of range"
        )
        != NULL
    );

    memset(&c, 0, sizeof c);
    RUN(&c, "--long=99999999999999999999");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "out of range") != NULL);
    return 0;
}

static int test_argparse_ranges(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--level=1", "--ranged=-5");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.level == 1 && c.ranged == -5);

    memset(&c, 0, sizeof c);
    RUN(&c, "--level=10", "--ranged=5");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.level == 10 && c.ranged == 5);

    memset(&c, 0, sizeof c);
    RUN(&c, "--level=0");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "must be between 1 and 10") != NULL);
    PF_ASSERT_ALWAYS(strstr(c.err, "got '0'") != NULL);
    PF_ASSERT_ALWAYS(c.level == 0);

    memset(&c, 0, sizeof c);
    RUN(&c, "-L11");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "option '-L'") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ranged=6");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "between -5 and 5") != NULL);
    return 0;
}

static int test_argparse_missing_value(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--name");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "option '--name' requires a value") != NULL);
    PF_ASSERT_ALWAYS(c.name == NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--all", "-n");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "option '-n' requires a value") != NULL);
    return 0;
}

static int test_argparse_empty_value(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    RUN(&c, "--name=");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS(c.name != NULL);
    PF_ASSERT_ALWAYS_STR_EQ(c.name, "");
    return 0;
}

static int test_argparse_double(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--ratio=3.14159", "--real=0.5");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS(c.ratio > 3.14158 && c.ratio < 3.14160);
    PF_ASSERT_ALWAYS(c.real == 0.5f);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1e300");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.ratio == 1e300);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=abc");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "invalid number 'abc'") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1.5x");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=inf");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "out of range") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1e999");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    /* Representable as double, but not as float. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--real=1e300");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "out of range") != NULL);
    return 0;
}

static int test_argparse_bool(int seed, int rep) {
    (void)seed, (void)rep;
    static const char *const yes[] = { "1", "true", "yes", "on" };
    static const char *const no[] = { "0", "false", "no", "off" };
    size_t i;
    CFG(c);

    RUN(&c, "--all");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.all);

    memset(&c, 0, sizeof c);
    RUN(&c, "-a");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.all);

    for (i = 0; i < 4; i++) {
        char arg[32];
        char *argv[3];

        memset(&c, 0, sizeof c);
        snprintf(arg, sizeof arg, "--all=%s", yes[i]);
        argv[0] = "prog";
        argv[1] = arg;
        argv[2] = NULL;
        parse(&c, 2, argv);
        PF_ASSERT_ALWAYS(c.rc == 0 && c.all);

        memset(&c, 0, sizeof c);
        c.all = true;
        snprintf(arg, sizeof arg, "--all=%s", no[i]);
        parse(&c, 2, argv);
        PF_ASSERT_ALWAYS(c.rc == 0 && !c.all);
    }

    memset(&c, 0, sizeof c);
    RUN(&c, "--all=maybe");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "invalid boolean 'maybe'") != NULL);
    return 0;
}

static int test_argparse_toggle(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--flag");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.flag);

    RUN(&c, "--no-flag");
    PF_ASSERT_ALWAYS(c.rc == 0 && !c.flag);

    memset(&c, 0, sizeof c);
    RUN(&c, "--flag", "--no-flag");
    PF_ASSERT_ALWAYS(c.rc == 0 && !c.flag);

    memset(&c, 0, sizeof c);
    RUN(&c, "--no-flag=1");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "does not take a value") != NULL);

    /* A long name only: no short form. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--toggle", "--no-toggle", "--toggle");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.toggle);

    /* `--no-` alone is not an option. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--no-");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

static int test_argparse_count(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "-c");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 1);

    memset(&c, 0, sizeof c);
    RUN(&c, "-ccc");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 3);

    memset(&c, 0, sizeof c);
    RUN(&c, "-c", "--chatty", "-c");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 3);

    memset(&c, 0, sizeof c);
    RUN(&c, "--chatty=5");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 5);

    memset(&c, 0, sizeof c);
    RUN(&c, "-c4");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 4);

    memset(&c, 0, sizeof c);
    RUN(&c, "--chatty=-1");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--chatty=lots");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

static int test_argparse_enum(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--mode=slow");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.mode == MODE_SLOW);

    memset(&c, 0, sizeof c);
    RUN(&c, "-m", "auto");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.mode == MODE_AUTO);

    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=turbo");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "invalid value 'turbo'") != NULL);
    PF_ASSERT_ALWAYS(strstr(c.err, "option '--mode'") != NULL);
    PF_ASSERT_ALWAYS(
        strstr(c.err, "expected one of: fast, slow, auto") != NULL
    );
    PF_ASSERT_ALWAYS(c.mode == 0);

    /* Matches whole names only. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=fas");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=fastest");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    memset(&c, 0, sizeof c);
    RUN(&c, "--mode=");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

static int test_argparse_flags(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--flags=a");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.flags == FLAG_A);

    memset(&c, 0, sizeof c);
    RUN(&c, "--flags=a|c");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.flags == (FLAG_A | FLAG_C));

    memset(&c, 0, sizeof c);
    RUN(&c, "-F", "b,c");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.flags == (FLAG_B | FLAG_C));

    /* Repeating accumulates. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--flags=a", "--flags=b", "-F=c");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.flags == (FLAG_A | FLAG_B | FLAG_C));

    /* Nothing is written on error. */
    memset(&c, 0, sizeof c);
    c.flags = FLAG_C;
    RUN(&c, "--flags=a|x");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(c.flags == FLAG_C);
    PF_ASSERT_ALWAYS(strstr(c.err, "invalid value 'x'") != NULL);

    memset(&c, 0, sizeof c);
    RUN(&c, "--flags=a|");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

struct enum_run {
    const pf_option_enum_t *values;
    int out;
};

static int enum_cb(pf_argparser_t *p, void *user) {
    struct enum_run *r = (struct enum_run *)user;
    pf_option_enum(p, "thing", 0, r->values, &r->out);
    return 0;
}

static int test_argparse_long_enum_list(int seed, int rep) {
    (void)seed, (void)rep;
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
    pf_argparser_t p;
    char *argv[] = { "prog", "--thing=nope", NULL };
    FILE *err = tmpfile();
    char text[512];
    struct enum_run r;

    r.values = many;
    r.out = 0;
    PF_ASSERT_ALWAYS(err != NULL);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 2, argv) == PF_OK);
    p.err = err;
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, enum_cb, &r), PF_EINVAL);
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(
        strstr(text, "invalid value 'nope' for option '--thing'") != NULL
    );
    PF_ASSERT_ALWAYS(strstr(text, "alpha-alpha-alpha") != NULL);
    PF_ASSERT_ALWAYS(strstr(text, "...") != NULL);
    PF_ASSERT_ALWAYS(strstr(text, "juliet") == NULL);
    PF_ASSERT_ALWAYS(p.failed);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_match_enum(int seed, int rep) {
    (void)seed, (void)rep;
    int v = 0;

    PF_ASSERT_ALWAYS(pf_match_enum("slow", modes, &v) && v == MODE_SLOW);
    PF_ASSERT_ALWAYS(pf_match_enum("fast", modes, &v) && v == MODE_FAST);
    v = 99;
    PF_ASSERT_ALWAYS(
        !pf_match_enum("Slow", modes, &v) && v == 99
    ); /* case-sensitive */
    PF_ASSERT_ALWAYS(!pf_match_enum("", modes, &v));
    PF_ASSERT_ALWAYS(!pf_match_enum(NULL, modes, &v));
    PF_ASSERT_ALWAYS(!pf_match_enum("slow", NULL, &v));
    PF_ASSERT_ALWAYS(!pf_match_enum("slow", modes, NULL));
    return 0;
}

/* ---------------------------------------------------------------- */
/* Errors                                                            */
/* ---------------------------------------------------------------- */

static int test_argparse_unknown(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    RUN(&c, "--bogus");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "unknown option '--bogus'") != NULL);
    PF_ASSERT_ALWAYS(c.snap.failed);

    memset(&c, 0, sizeof c);
    RUN(&c, "-z");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "unknown option '-z'") != NULL);

    /* With a value it is still just unknown. */
    memset(&c, 0, sizeof c);
    RUN(&c, "--bogus=1");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "unknown option '--bogus'") != NULL);

    memset(&c, 0, sizeof c);
    c.allow_unknown = 1;
    RUN(&c, "--bogus", "-z", "--all");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS(c.all);
    PF_ASSERT_ALWAYS(c.err[0] == '\0');
    return 0;
}

static int test_argparse_failure_stops(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    /* Nothing after the failing item is looked at. */
    RUN(&c, "--bogus", "--all", "param");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(!c.all);
    PF_ASSERT_ALWAYS(c.calls == 1);
    PF_ASSERT_ALWAYS(c.paramc == 0);

    /* A failing short option stops its cluster too. */
    memset(&c, 0, sizeof c);
    RUN(&c, "-zao");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(!c.all && !c.other);
    return 0;
}

static int test_argparse_error_prefix(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    c.prefix = "prog: error: ";
    RUN(&c, "--bogus");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strncmp(c.err, "prog: error: unknown option", 27) == 0);
    PF_ASSERT_ALWAYS(c.err[strlen(c.err) - 1] == '\n');
    return 0;
}

static int test_argparse_error_function(int seed, int rep) {
    (void)seed, (void)rep;
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    char text[128];

    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    p.err = err;

    pf_argparser_error(&p, "bad %s #%d", "thing", 7);
    PF_ASSERT_ALWAYS(p.failed);
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS_STR_EQ(text, "bad thing #7\n");

    /* --quiet: still fails, but prints nothing. */
    p.failed = false;
    p.silent = true;
    pf_argparser_error(&p, "hidden");
    PF_ASSERT_ALWAYS(p.failed);
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS_STR_EQ(text, "bad thing #7\n");

    /* --help wins: neither failure nor output. */
    p.failed = false;
    p.silent = false;
    p.help = true;
    pf_argparser_error(&p, "ignored");
    PF_ASSERT_ALWAYS(!p.failed);
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS_STR_EQ(text, "bad thing #7\n");

    pf_argparser_error(NULL, "no parser");
    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_callback_control(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    /* EINTR stops parsing without an error. */
    c.stop_at = 2;
    RUN(&c, "--all", "--other", "--flag");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS(c.calls == 2);
    PF_ASSERT_ALWAYS(c.all && !c.flag);

    /* Any other return code is passed on. */
    memset(&c, 0, sizeof c);
    c.fail_at = 2;
    RUN(&c, "--all", "--other", "--flag");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(c.calls == 2);

    /* EINTR after errors were reported is still a failure. */
    memset(&c, 0, sizeof c);
    c.stop_at = 2;
    RUN(&c, "--bogus", "--all");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

static int test_argparse_rerun(int seed, int rep) {
    (void)seed, (void)rep;
    pf_argparser_t p;
    struct cfg c;
    char *argv[] = { "prog", "x", "--all", "y", NULL };
    FILE *err = tmpfile();

    memset(&c, 0, sizeof c);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 4, argv) == PF_OK);
    p.err = err;

    PF_ASSERT_ALWAYS(pf_argparser_run(&p, callback, &c) == PF_OK);
    PF_ASSERT_ALWAYS(p.paramc == 2);
    c.all = false;
    PF_ASSERT_ALWAYS(pf_argparser_run(&p, callback, &c) == PF_OK);
    PF_ASSERT_ALWAYS(p.paramc == 2); /* does not accumulate */
    PF_ASSERT_ALWAYS(c.all);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_null_arguments(int seed, int rep) {
    (void)seed, (void)rep;
    int i = 0;
    char *s = NULL;
    long l = 0;
    bool b = false;

    PF_ASSERT_ALWAYS(!pf_param_is(NULL, 0));
    PF_ASSERT_ALWAYS(!pf_option_is(NULL));
    PF_ASSERT_ALWAYS(!pf_param_equals(NULL, "x"));
    PF_ASSERT_ALWAYS(!pf_option_bool(NULL, "x", 0, &b));
    PF_ASSERT_ALWAYS(!pf_option_string(NULL, "x", 0, &s));
    PF_ASSERT_ALWAYS(!pf_option_int(NULL, "x", 0, &i));
    PF_ASSERT_ALWAYS(!pf_option_long(NULL, "x", 0, &l));
    PF_ASSERT_ALWAYS(!pf_option_toggle(NULL, "x", 0, &b));
    PF_ASSERT_ALWAYS(!pf_option_count(NULL, "x", 0, &i));
    PF_ASSERT_ALWAYS(!pf_option_common(NULL, PF_COMMON_ALL));
    PF_ASSERT_ALWAYS(!pf_option_help(NULL, &b));
    PF_ASSERT_ALWAYS(!pf_param_subcommand(NULL, "x", commands, &i));
    PF_ASSERT_ALWAYS(!pf_param_expect(NULL, 0, 0, "x"));
    pf_param_consume(NULL);
    pf_argparser_print_version(NULL, "a", "b");

    {
        pf_argparser_t p;
        char *argv[] = { "prog", NULL };
        long v;
        PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
        PF_ASSERT_ALWAYS(!pf_arg_long(&p, NULL, "x", 0, 1, &v));
        PF_ASSERT_ALWAYS(!pf_arg_long(&p, "1", "x", 0, 1, NULL));
        PF_ASSERT_ALWAYS(!pf_arg_long(NULL, "1", "x", 0, 1, &v));
        PF_ASSERT_ALWAYS(!p.failed); /* bad arguments are not user errors */
        pf_argparser_free(&p);
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Conversion helpers                                                */
/* ---------------------------------------------------------------- */

static int test_argparse_arg_long(int seed, int rep) {
    (void)seed, (void)rep;
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    long v = 0;
    int i = 0;
    double d = 0;
    char text[256];

    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    p.err = err;

    PF_ASSERT_ALWAYS(pf_arg_long(&p, "42", "answer", 0, 100, &v) && v == 42);
    PF_ASSERT_ALWAYS(
        pf_arg_long(&p, "-42", "answer", -100, 100, &v) && v == -42
    );
    PF_ASSERT_ALWAYS(
        pf_arg_long(&p, "0", NULL, LONG_MIN, LONG_MAX, &v) && v == 0
    );
    PF_ASSERT_ALWAYS(!p.failed);

    PF_ASSERT_ALWAYS(!pf_arg_long(&p, "101", "answer", 0, 100, &v));
    PF_ASSERT_ALWAYS(p.failed);
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(
        strstr(text, "answer must be between 0 and 100; got '101'") != NULL
    );

    p.failed = false;
    PF_ASSERT_ALWAYS(!pf_arg_long(&p, "zz", NULL, 0, 1, &v));
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(strstr(text, "invalid integer 'zz' for value") != NULL);

    PF_ASSERT_ALWAYS(pf_arg_int(&p, "7", "n", 0, 10, &i) && i == 7);
    PF_ASSERT_ALWAYS(!pf_arg_int(&p, "7", "n", 8, 10, &i) && i == 7);
    PF_ASSERT_ALWAYS(!pf_arg_int(&p, "7", "n", 0, 10, NULL));

    PF_ASSERT_ALWAYS(pf_arg_double(&p, "2.5", "x", &d) && d == 2.5);
    PF_ASSERT_ALWAYS(!pf_arg_double(&p, "", "x", &d));
    PF_ASSERT_ALWAYS(!pf_arg_double(&p, " ", "x", &d));
    PF_ASSERT_ALWAYS(!pf_arg_double(&p, NULL, "x", &d));

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

struct pcheck {
    int calls;
    int index_hits[4];
    int cmp_hit;
    int match_hit;
    int enum_hit;
    int enum_value;
};

static int param_cb(pf_argparser_t *p, void *user) {
    static const char *const names[] = { "red", "green", "blue", NULL };
    static const pf_option_enum_t colors[] = { { "red", 1 },
                                               { "blue", 2 },
                                               { 0, 0 } };
    struct pcheck *c = user;
    char *match;
    int i;

    c->calls++;
    for (i = 0; i < 4; i++)
        if (pf_param_is(p, i))
            c->index_hits[i]++;
    if (pf_param_equals(p, "special"))
        c->cmp_hit++;
    if (pf_param_match(p, names, &match))
        c->match_hit++;
    if (pf_param_enum(p, 0, "color", colors, &c->enum_value))
        c->enum_hit++;
    return 0;
}

static int test_argparse_param_helpers(int seed, int rep) {
    (void)seed, (void)rep;
    pf_argparser_t p;
    char *argv[] = { "prog", "red", "special", "green", "--opt", "x", NULL };
    struct pcheck c;
    FILE *err = tmpfile();

    memset(&c, 0, sizeof c);
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 6, argv) == PF_OK);
    p.err = err;
    p.allow_unknown = true;
    PF_ASSERT_ALWAYS(pf_argparser_run(&p, param_cb, &c) == PF_OK);

    /* Parameters: red, special, green, x. The option is not one. */
    PF_ASSERT_ALWAYS(c.calls == 5);
    PF_ASSERT_ALWAYS(c.index_hits[0] == 1 && c.index_hits[1] == 1);
    PF_ASSERT_ALWAYS(c.index_hits[2] == 1 && c.index_hits[3] == 1);
    PF_ASSERT_ALWAYS(c.cmp_hit == 1);
    PF_ASSERT_ALWAYS(c.match_hit == 2); /* red and green */
    PF_ASSERT_ALWAYS(c.enum_hit == 1);  /* only the first parameter */
    PF_ASSERT_ALWAYS(c.enum_value == 1);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int color_cb(pf_argparser_t *p, void *user) {
    static const pf_option_enum_t colors[] = { { "red", 1 }, { 0, 0 } };
    pf_param_enum(p, 0, "color", colors, (int *)user);
    return 0;
}

static int test_argparse_param_enum_error(int seed, int rep) {
    char *argv[] = { "prog", "purple", NULL };
    pf_argparser_t p;
    int out = 0;
    FILE *err = tmpfile();
    char text[128];
    (void)seed, (void)rep;

    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 2, argv) == PF_OK);
    p.err = err;
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, color_cb, &out), PF_EINVAL);
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(strstr(text, "invalid value 'purple' for color") != NULL);
    PF_ASSERT_ALWAYS_EQ(out, 0);
    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_subcommand(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    c.use_subcommand = 1;
    RUN(&c, "start", "alpha", "beta");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS(c.cmd == 1);
    PF_ASSERT_ALWAYS(
        c.paramc == 2
    ); /* the subcommand itself is not a parameter */
    PF_ASSERT_ALWAYS_STR_EQ(c.params[0], "alpha");
    PF_ASSERT_ALWAYS_STR_EQ(c.params[1], "beta");

    /* Options may come first. */
    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "--all", "stop", "-n", "3", "arg");
    PF_ASSERT_ALWAYS(c.rc == 0);
    PF_ASSERT_ALWAYS(c.cmd == 2 && c.all && c.count == 3);
    PF_ASSERT_ALWAYS(c.paramc == 1);
    PF_ASSERT_ALWAYS_STR_EQ(c.params[0], "arg");

    /* Only the first parameter is looked up. */
    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "start", "stop");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.cmd == 1);
    PF_ASSERT_ALWAYS(c.paramc == 1);
    PF_ASSERT_ALWAYS_STR_EQ(c.params[0], "stop");

    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "restart");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(
        strstr(c.err, "invalid value 'restart' for command") != NULL
    );
    PF_ASSERT_ALWAYS(strstr(c.err, "start, stop") != NULL);

    /* Nothing to look up. */
    memset(&c, 0, sizeof c);
    c.use_subcommand = 1;
    RUN(&c, "--all");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.cmd == 0 && c.all);
    return 0;
}

static int test_argparse_expect_params(int seed, int rep) {
    (void)seed, (void)rep;
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };
    FILE *err = tmpfile();
    char text[256];

    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    p.err = err;

    p.paramc = 2;
    PF_ASSERT_ALWAYS(pf_param_expect(&p, 1, 3, "cmd"));
    PF_ASSERT_ALWAYS(pf_param_expect(&p, 2, 2, "cmd"));
    PF_ASSERT_ALWAYS(pf_param_expect(&p, 0, -1, "cmd"));
    PF_ASSERT_ALWAYS(!p.failed);

    PF_ASSERT_ALWAYS(!pf_param_expect(&p, 0, 0, "cmd"));
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(strstr(text, "cmd takes no arguments; got 2") != NULL);

    p.failed = false;
    PF_ASSERT_ALWAYS(!pf_param_expect(&p, 1, 1, "sub 'x'"));
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(
        strstr(text, "sub 'x' requires exactly 1 argument; got 2") != NULL
    );

    PF_ASSERT_ALWAYS(!pf_param_expect(&p, 3, 3, "cmd"));
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(
        strstr(text, "requires exactly 3 arguments; got 2") != NULL
    );

    PF_ASSERT_ALWAYS(!pf_param_expect(&p, 3, -1, "cmd"));
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(
        strstr(text, "requires at least 3 arguments; got 2") != NULL
    );

    PF_ASSERT_ALWAYS(!pf_param_expect(&p, 0, 1, NULL));
    slurp(err, text, sizeof text);
    PF_ASSERT_ALWAYS(
        strstr(text, "command takes at most 1 argument; got 2") != NULL
    );
    PF_ASSERT_ALWAYS(p.failed);

    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_consume(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);

    c.use_subcommand = 1;
    RUN(&c, "start");
    PF_ASSERT_ALWAYS(c.paramc == 0);

    pf_param_consume(NULL);
    return 0;
}

/* ---------------------------------------------------------------- */
/* Common options                                                    */
/* ---------------------------------------------------------------- */

static int test_argparse_common_verbose(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "-v");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.verbosity == 1 && (c.snap.verbosity > 0)
    );

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-vvv");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.verbosity == 3);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--verbose=2");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.verbosity == 2);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-vv", "--no-verbose");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.verbosity == 0 && !(c.snap.verbosity > 0)
    );
    return 0;
}

static int test_argparse_common_quiet(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "-q");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.silent);

    RUN(&c, "--quiet", "--no-quiet");
    PF_ASSERT_ALWAYS(c.rc == 0 && !c.snap.silent);

    /* A silent parse still fails, but prints nothing. */
    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-q", "--bogus");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(c.err[0] == '\0');
    return 0;
}

static int test_argparse_common_help_version(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "--help");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.help);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-?");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.help);

    /* Asking for help mutes failures, so usage text can win. */
    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--help", "--bogus");
    PF_ASSERT_ALWAYS(c.rc == PF_OK);
    PF_ASSERT_ALWAYS(c.snap.help);
    PF_ASSERT_ALWAYS(c.err[0] == '\0');

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--version");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.version);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "-V");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.version);

    /* Only the selected options are recognised. */
    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_VERBOSE;
    RUN(&c, "--help");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(!c.snap.help);
    return 0;
}

static int test_argparse_common_color(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "--color");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.color_mode == PF_COLOR_ALWAYS && c.snap.color_set
    );

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=never");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.color_mode == PF_COLOR_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=auto");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.color_mode == PF_COLOR_AUTO && c.snap.color_set
    );

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=yes");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.color_mode == PF_COLOR_ALWAYS);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=no");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.color_mode == PF_COLOR_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color", "--no-color");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.snap.color_mode == PF_COLOR_NEVER);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--color=rainbow");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "invalid value 'rainbow'") != NULL);
    PF_ASSERT_ALWAYS(!c.snap.color_set);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--all");
    PF_ASSERT_ALWAYS(!c.snap.color_set && c.snap.color_mode == PF_COLOR_AUTO);
    return 0;
}

static int test_argparse_common_interactive(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    c.common = PF_COMMON_ALL;

    RUN(&c, "-i");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.interactive_mode == PF_INTERACTIVE_ALWAYS
    );
    PF_ASSERT_ALWAYS(c.snap.interactive_set);

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--interactive=never");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.interactive_mode == PF_INTERACTIVE_NEVER
    );

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--no-interactive");
    PF_ASSERT_ALWAYS(
        c.rc == 0 && c.snap.interactive_mode == PF_INTERACTIVE_NEVER
    );

    memset(&c, 0, sizeof c);
    c.common = PF_COMMON_ALL;
    RUN(&c, "--interactive=sometimes");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    return 0;
}

static int test_argparse_print_version(int seed, int rep) {
    (void)seed, (void)rep;
    CFG(c);
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };
    FILE *out = tmpfile();

    (void)c;
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    p.out = out;
    pf_argparser_print_version(&p, "tool", "1.2.3");
    pf_argparser_print_version(&p, NULL, NULL);
    slurp(out, c.out, sizeof c.out);
    PF_ASSERT_ALWAYS(strncmp(c.out, "tool 1.2.3\n", 11) == 0);
    PF_ASSERT_ALWAYS_STR_EQ(c.out + 11, " \n");

    p.out = NULL; /* nothing to write to */
    pf_argparser_print_version(&p, "tool", "1");

    pf_argparser_free(&p);
    fclose(out);
    return 0;
}

static int test_argparse_long_name(int seed, int rep) {
    (void)seed, (void)rep;
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
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    PF_ASSERT_ALWAYS(strstr(c.err, "unknown option") != NULL);
    return 0;
}

/* ---- Added in v2 ------------------------------------------------------- */

struct hook {
    int calls;
    char last[128];
};

static void error_hook(pf_argparser_t *p, const char *message, void *user) {
    struct hook *h = (struct hook *)user;
    (void)p;
    h->calls++;
    snprintf(h->last, sizeof(h->last), "%s", message);
}

static int test_argparse_error_hook(int seed, int rep) {
    pf_argparser_t p;
    char *argv[] = { "prog", "--bogus", NULL };
    FILE *err = tmpfile();
    char text[64];
    struct hook h;
    struct cfg c;
    (void)seed, (void)rep;

    memset(&h, 0, sizeof(h));
    memset(&c, 0, sizeof(c));
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 2, argv) == PF_OK);
    p.err = err;
    p.err_prefix = "prog: ";
    pf_argparser_set_error_fn(&p, error_hook, &h);
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, callback, &c), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(h.calls, 1);
    PF_ASSERT_ALWAYS_STR_EQ(h.last, "unknown option '--bogus'");
    slurp(err, text, sizeof(text));
    PF_ASSERT_ALWAYS_STR_EQ(text, ""); /* not printed */

    /* Without the hook: printed with the prefix again. */
    pf_argparser_set_error_fn(&p, NULL, NULL);
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, callback, &c), PF_EINVAL);
    slurp(err, text, sizeof(text));
    PF_ASSERT_ALWAYS_STR_EQ(text, "prog: unknown option '--bogus'\n");
    PF_ASSERT_ALWAYS_EQ(h.calls, 1);
    pf_argparser_set_error_fn(NULL, NULL, NULL);
    pf_argparser_free(&p);
    fclose(err);
    return 0;
}

static int test_argparse_set_short(int seed, int rep) {
    CFG(c);
    pf_argparser_t p;
    char *argv[] = { "prog", "-h", "-x", "-C", NULL };
    (void)seed, (void)rep;

    c.common = PF_COMMON_ALL;
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 4, argv) == PF_OK);
    pf_argparser_set_short(&p, PF_COMMON_HELP, 'h');
    pf_argparser_set_short(&p, PF_COMMON_VERBOSE | PF_COMMON_QUIET, 0);
    pf_argparser_set_short(&p, PF_COMMON_COLOR, 'C');
    pf_argparser_set_short(NULL, PF_COMMON_ALL, 'z');
    p.err = tmpfile();
    p.allow_unknown = true;
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, callback, &c), PF_OK);
    PF_ASSERT_ALWAYS(p.help);
    PF_ASSERT_ALWAYS(p.color_set && p.color_mode == PF_COLOR_ALWAYS);
    fclose(p.err);
    pf_argparser_free(&p);

    /* -v is no longer the verbose option. */
    memset(&c, 0, sizeof(c));
    c.common = PF_COMMON_ALL;
    {
        char *argv2[] = { "prog", "-v", NULL };
        PF_ASSERT_ALWAYS(pf_argparser_init(&p, 2, argv2) == PF_OK);
        pf_argparser_set_short(&p, PF_COMMON_VERBOSE, 0);
        p.err = tmpfile();
        PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, callback, &c), PF_EINVAL);
        PF_ASSERT_ALWAYS_EQ(p.verbosity, 0);
        fclose(p.err);
        pf_argparser_free(&p);
    }
    return 0;
}

static int test_argparse_help_options(int seed, int rep) {
    pf_argparser_t p;
    char *argv[] = { "prog", NULL };
    char text[1024];
    (void)seed, (void)rep;

    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 1, argv) == PF_OK);
    p.out = tmpfile();
    pf_argparser_help_options(&p, PF_COMMON_HELP | PF_COMMON_COLOR);
    slurp(p.out, text, sizeof(text));
    PF_ASSERT_ALWAYS_STR_EQ(
        text,
        "  -?, --help               Shows this information.\n"
        "      --color[=WHEN]       Colors the output: 'auto', 'always' or "
        "'never'.\n"
    );
    fclose(p.out);
    pf_argparser_help_options(NULL, PF_COMMON_ALL);
    pf_argparser_free(&p);
    return 0;
}

/* Long names are compared whole, however long. */
struct exact {
    const char *name;
    bool *on;
};

static int exact_cb(pf_argparser_t *p, void *user) {
    struct exact *e = (struct exact *)user;
    pf_option_bool(p, e->name, 0, e->on);
    return 0;
}

static int test_argparse_long_exact(int seed, int rep) {
    static char name[300];
    static char arg[310];
    pf_argparser_t p;
    char *argv[3];
    bool on = false;
    struct exact e;
    (void)seed, (void)rep;

    memset(name, 'y', sizeof(name) - 1);
    snprintf(arg, sizeof(arg), "--%s", name);
    argv[0] = "prog";
    argv[1] = arg;
    argv[2] = NULL;
    e.name = name;
    e.on = &on;
    PF_ASSERT_ALWAYS(pf_argparser_init(&p, 2, argv) == PF_OK);
    p.err = tmpfile();
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, exact_cb, &e), PF_OK);
    PF_ASSERT_ALWAYS(on);

    /* One character shorter: no match. */
    name[sizeof(name) - 2] = '\0';
    on = false;
    PF_ASSERT_ALWAYS_EQ(pf_argparser_run(&p, exact_cb, &e), PF_EINVAL);
    PF_ASSERT_ALWAYS(!on);
    fclose(p.err);
    pf_argparser_free(&p);
    return 0;
}

static int test_argparse_count_digits(int seed, int rep) {
    CFG(c);
    (void)seed, (void)rep;

    RUN(&c, "-c3");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 3);
    memset(&c, 0, sizeof c);
    RUN(&c, "-cca");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 2 && c.all);
    memset(&c, 0, sizeof c);
    RUN(&c, "-c=4", "-c");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.verbosity_out == 5);
    return 0;
}

/* Decimals use '.' whatever the locale. */
static int test_argparse_locale(int seed, int rep) {
    static const char *const locales[] = {
        "de_DE.UTF-8", "de_DE.utf8",          "fr_FR.UTF-8", "fr_FR.utf8",
        "ru_RU.UTF-8", "German_Germany.1252", NULL,
    };
    const char *old = setlocale(LC_NUMERIC, NULL);
    char saved[64];
    int k;
    CFG(c);
    (void)seed, (void)rep;

    snprintf(saved, sizeof(saved), "%s", old ? old : "C");
    for (k = 0; locales[k]; k++)
        if (setlocale(LC_NUMERIC, locales[k]))
            break;
    if (!locales[k] || strcmp(localeconv()->decimal_point, ",") != 0) {
        setlocale(LC_NUMERIC, saved);
        return SKIP("no locale with a decimal comma is installed");
    }
    RUN(&c, "--ratio=1.5", "--real=-2.25e1");
    PF_ASSERT_ALWAYS(c.rc == 0 && c.ratio == 1.5 && c.real == -22.5f);
    memset(&c, 0, sizeof c);
    RUN(&c, "--ratio=1,5");
    PF_ASSERT_ALWAYS(c.rc == PF_EINVAL);
    setlocale(LC_NUMERIC, saved);
    return 0;
}

static int test_argparse_args_utf8(int seed, int rep) {
    int argc = 2;
    char *list[] = { "prog", "x", NULL };
    char **argv = list;
    (void)seed, (void)rep;

    PF_ASSERT_ALWAYS_EQ(pf_args_utf8(NULL, &argv), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_args_utf8(&argc, NULL), PF_EINVAL);
    PF_ASSERT_ALWAYS_EQ(pf_args_utf8(&argc, &argv), PF_OK);
#if PF_OS_WINDOWS
    /* This program's own arguments. */
    PF_ASSERT_ALWAYS_GE(argc, 1);
    PF_ASSERT_ALWAYS_NOT_NULL(argv[0]);
    PF_ASSERT_ALWAYS_NULL(argv[argc]);
#else
    PF_ASSERT_ALWAYS(argc == 2 && argv == list);
#endif
    return 0;
}

pf_test_t suite_argparse[] = {
    { test_argparse_init, "init" },
    { test_argparse_run_invalid, "run_invalid" },
    { test_argparse_params, "params" },
    { test_argparse_params_mixed, "params_mixed" },
    { test_argparse_double_dash, "double_dash" },
    { test_argparse_single_dash, "single_dash" },
    { test_argparse_argv_untouched, "argv_untouched" },
    { test_argparse_long_values, "long_values" },
    { test_argparse_short_values, "short_values" },
    { test_argparse_short_cluster, "short_cluster" },
    { test_argparse_negative_values, "negative_values" },
    { test_argparse_integer_forms, "integer_forms" },
    { test_argparse_integer_errors, "integer_errors" },
    { test_argparse_ranges, "ranges" },
    { test_argparse_missing_value, "missing_value" },
    { test_argparse_empty_value, "empty_value" },
    { test_argparse_double, "double" },
    { test_argparse_bool, "bool" },
    { test_argparse_toggle, "toggle" },
    { test_argparse_count, "count" },
    { test_argparse_enum, "enum" },
    { test_argparse_flags, "flags" },
    { test_argparse_long_enum_list, "long_enum_list" },
    { test_argparse_match_enum, "match_enum" },
    { test_argparse_unknown, "unknown" },
    { test_argparse_failure_stops, "failure_stops" },
    { test_argparse_error_prefix, "error_prefix" },
    { test_argparse_error_function, "error_function" },
    { test_argparse_callback_control, "callback_control" },
    { test_argparse_rerun, "rerun" },
    { test_argparse_null_arguments, "null_arguments" },
    { test_argparse_arg_long, "arg_long" },
    { test_argparse_param_helpers, "param_helpers" },
    { test_argparse_param_enum_error, "param_enum_error" },
    { test_argparse_subcommand, "subcommand" },
    { test_argparse_expect_params, "expect_params" },
    { test_argparse_consume, "consume" },
    { test_argparse_common_verbose, "common_verbose" },
    { test_argparse_common_quiet, "common_quiet" },
    { test_argparse_common_help_version, "common_help_version" },
    { test_argparse_common_color, "common_color" },
    { test_argparse_common_interactive, "common_interactive" },
    { test_argparse_print_version, "print_version" },
    { test_argparse_long_name, "long_name" },
    { test_argparse_error_hook, "error_hook" },
    { test_argparse_set_short, "set_short" },
    { test_argparse_help_options, "help_options" },
    { test_argparse_long_exact, "long_exact" },
    { test_argparse_count_digits, "count_digits" },
    { test_argparse_locale, "locale" },
    { test_argparse_args_utf8, "args_utf8" },
    { 0 },
};
