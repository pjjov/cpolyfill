/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include <cpolyfill/macro.h>
#include <cpolyfill/string.h>

#include <errno.h>
#include <limits.h>
#if PF_CONFIG_HAVE_STRINGS_H
    #include <strings.h>
#endif

#include "check.h"

static int test_dup(int seed, int i) {
    char *p;
    (void)seed, (void)i;

    p = pf_strdup("hello");
    PF_ASSERT_ALWAYS_STR_EQ(p, "hello");
    free(p);
    p = pf_strdup("");
    PF_ASSERT_ALWAYS_STR_EQ(p, "");
    free(p);

    p = pf_strndup("hello", 3);
    PF_ASSERT_ALWAYS_STR_EQ(p, "hel");
    free(p);
    p = pf_strndup("hi", 10);
    PF_ASSERT_ALWAYS_STR_EQ(p, "hi");
    free(p);
    p = pf_strndup("hi", 0);
    PF_ASSERT_ALWAYS_STR_EQ(p, "");
    free(p);
    {
        /* not NUL-terminated within n */
        const char raw[3] = { 'a', 'b', 'c' };
        p = pf_strndup(raw, 3);
        PF_ASSERT_ALWAYS_STR_EQ(p, "abc");
        free(p);
    }
    return 0;
}

static int test_strnlen(int seed, int i) {
    const char raw[4] = { 'a', 'b', 'c', 'd' };
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_strnlen("hello", 10), 5);
    PF_ASSERT_ALWAYS_EQ(pf_strnlen("hello", 3), 3);
    PF_ASSERT_ALWAYS_EQ(pf_strnlen("", 3), 0);
    PF_ASSERT_ALWAYS_EQ(pf_strnlen("abc", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_strnlen(raw, 4), 4);
    PF_ASSERT_ALWAYS_EQ(pf_strnlen(NULL, 4), 0);
    PF_ASSERT_ALWAYS_EQ(pf_strnlen("abc", (size_t)-1), 3);
    return 0;
}

static int test_strlcpy(int seed, int i) {
    char buf[8];
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_strlcpy(buf, "abc", sizeof(buf)), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abc");
    PF_ASSERT_ALWAYS_EQ(pf_strlcpy(buf, "abcdefghij", sizeof(buf)), 10);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abcdefg");
    PF_ASSERT_ALWAYS_EQ(pf_strlcpy(buf, "1234567", sizeof(buf)), 7);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "1234567");
    buf[0] = 'x';
    PF_ASSERT_ALWAYS_EQ(pf_strlcpy(buf, "abc", 0), 3);
    PF_ASSERT_ALWAYS_EQ(buf[0], 'x');
    PF_ASSERT_ALWAYS_EQ(pf_strlcpy(buf, "abc", 1), 3);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "");

    strcpy(buf, "ab");
    PF_ASSERT_ALWAYS_EQ(pf_strlcat(buf, "cd", sizeof(buf)), 4);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abcd");
    PF_ASSERT_ALWAYS_EQ(pf_strlcat(buf, "efghij", sizeof(buf)), 10);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abcdefg");
    PF_ASSERT_ALWAYS_EQ(pf_strlcat(buf, "x", sizeof(buf)), 8);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abcdefg");
    /* no NUL within `size`: nothing appended, returns size + strlen */
    PF_ASSERT_ALWAYS_EQ(pf_strlcat(buf, "xyz", 3), 6);
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abcdefg");
    PF_ASSERT_ALWAYS_EQ(pf_strlcat(buf, "xyz", 0), 3);
#if PF_CONFIG_HAVE_STRLCPY && PF_CONFIG_HAVE_STRLCAT
    {
        char a[6], b[6];
        static const char *const srcs[] = {
            "", "a", "hello", "hello!", "long string"
        };
        size_t k, size;
        for (k = 0; k < PF_COUNTOF(srcs); k++) {
            for (size = 0; size <= sizeof(a); size++) {
                memset(a, '#', sizeof(a));
                memset(b, '#', sizeof(b));
                PF_ASSERT_ALWAYS_EQ(
                    pf_strlcpy(a, srcs[k], size), strlcpy(b, srcs[k], size)
                );
                PF_ASSERT_ALWAYS_MEM_EQ(a, b, sizeof(a));
                PF_ASSERT_ALWAYS_EQ(
                    pf_strlcat(a, srcs[k], size), strlcat(b, srcs[k], size)
                );
                PF_ASSERT_ALWAYS_MEM_EQ(a, b, sizeof(a));
            }
        }
    }
#endif
    return 0;
}

static int test_casecmp(int seed, int i) {
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS_EQ(pf_strcasecmp("Hello", "hELLO"), 0);
    PF_ASSERT_ALWAYS_EQ(pf_strcasecmp("", ""), 0);
    PF_ASSERT_ALWAYS(pf_strcasecmp("abc", "ABD") < 0);
    PF_ASSERT_ALWAYS(pf_strcasecmp("abc", "AB") > 0);
    PF_ASSERT_ALWAYS(pf_strcasecmp("ab", "ABC") < 0);
    /* '_' (0x5F) sits between 'Z' and 'a': folding to lower case puts it
       before letters, as POSIX strcasecmp does */
    PF_ASSERT_ALWAYS(pf_strcasecmp("_", "A") < 0);
    /* bytes above 127 compare unsigned and are not folded */
    PF_ASSERT_ALWAYS(pf_strcasecmp("\xC3\xA9", "a") > 0);
    PF_ASSERT_ALWAYS(pf_strcasecmp("\xC3\x89", "\xC3\xA9") != 0);

    PF_ASSERT_ALWAYS_EQ(pf_strncasecmp("HelloX", "hELLOy", 5), 0);
    PF_ASSERT_ALWAYS(pf_strncasecmp("HelloX", "hELLOy", 6) < 0);
    PF_ASSERT_ALWAYS_EQ(pf_strncasecmp("a", "b", 0), 0);
    PF_ASSERT_ALWAYS_EQ(pf_strncasecmp("ab", "AB", 10), 0);
#if PF_CONFIG_HAVE_STRCASECMP
    {
    #define sign(x) (((x) > 0) - ((x) < 0))
        static const char *const w[] = { "",  "a",  "A",   "_",   "[",
                                         "z", "Z{", "abc", "ABD", "\xFF" };
        size_t x, y;
        for (x = 0; x < PF_COUNTOF(w); x++)
            for (y = 0; y < PF_COUNTOF(w); y++)
                PF_ASSERT_ALWAYS_EQ(
                    sign(pf_strcasecmp(w[x], w[y])),
                    sign(strcasecmp(w[x], w[y]))
                );
    }
#endif
    return 0;
}

static int test_tokens(int seed, int i) {
    char buf[32], *save = NULL, *tok, *p;
    (void)seed, (void)i;

    strcpy(buf, ",,a,b;;c,");
    tok = pf_strtok_r(buf, ",;", &save);
    PF_ASSERT_ALWAYS_STR_EQ(tok, "a");
    tok = pf_strtok_r(NULL, ",;", &save);
    PF_ASSERT_ALWAYS_STR_EQ(tok, "b");
    tok = pf_strtok_r(NULL, ",;", &save);
    PF_ASSERT_ALWAYS_STR_EQ(tok, "c");
    PF_ASSERT_ALWAYS(pf_strtok_r(NULL, ",;", &save) == NULL);
    PF_ASSERT_ALWAYS(pf_strtok_r(NULL, ",;", &save) == NULL);
    strcpy(buf, ",,,");
    PF_ASSERT_ALWAYS(pf_strtok_r(buf, ",", &save) == NULL);

    /* strsep returns empty tokens */
    strcpy(buf, "a,,b");
    p = buf;
    PF_ASSERT_ALWAYS_STR_EQ(pf_strsep(&p, ","), "a");
    PF_ASSERT_ALWAYS_STR_EQ(pf_strsep(&p, ","), "");
    PF_ASSERT_ALWAYS_STR_EQ(pf_strsep(&p, ","), "b");
    PF_ASSERT_ALWAYS(p == NULL);
    PF_ASSERT_ALWAYS(pf_strsep(&p, ",") == NULL);
    return 0;
}

static int test_search(int seed, int i) {
    static const char hay[] = "abcabcabd";
    const char *s = "hello";
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(pf_strchrnul(s, 'l') == s + 2);
    PF_ASSERT_ALWAYS(pf_strchrnul(s, 'z') == s + 5);
    PF_ASSERT_ALWAYS(pf_strchrnul(s, '\0') == s + 5);
    PF_ASSERT_ALWAYS(pf_strchrnul("\xE9", 0xE9)[0] == '\xE9');

    PF_ASSERT_ALWAYS(pf_memmem(hay, 9, "abd", 3) == hay + 6);
    PF_ASSERT_ALWAYS(pf_memmem(hay, 9, "abc", 3) == hay);
    PF_ASSERT_ALWAYS(pf_memmem(hay, 9, "abe", 3) == NULL);
    PF_ASSERT_ALWAYS(pf_memmem(hay, 9, "", 0) == hay);
    PF_ASSERT_ALWAYS(pf_memmem(hay, 0, "", 0) == hay);
    PF_ASSERT_ALWAYS(pf_memmem(hay, 2, "abc", 3) == NULL);
    PF_ASSERT_ALWAYS(pf_memmem(hay, 9, hay, 9) == hay);
    {
        static const char bin[] = { 'x', 0, 'y', 0, 'z' };
        PF_ASSERT_ALWAYS(pf_memmem(bin, 5, "\0z", 2) == bin + 3);
    }

    PF_ASSERT_ALWAYS(pf_memrchr(hay, 'a', 9) == hay + 6);
    PF_ASSERT_ALWAYS(pf_memrchr(hay, 'd', 9) == hay + 8);
    PF_ASSERT_ALWAYS(pf_memrchr(hay, 'd', 8) == NULL);
    PF_ASSERT_ALWAYS(pf_memrchr(hay, 'a', 0) == NULL);
    PF_ASSERT_ALWAYS(pf_memrchr("\xFF", -1, 1) != NULL);
    return 0;
}

static int test_copy(int seed, int i) {
    char buf[16];
    char *end;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS((char *)pf_mempcpy(buf, "abc", 3) == buf + 3);
    PF_ASSERT_ALWAYS((char *)pf_mempcpy(buf, "abc", 0) == buf);

    end = pf_stpcpy(buf, "abc");
    PF_ASSERT_ALWAYS(end == buf + 3 && *end == '\0');
    end = pf_stpcpy(end, "de");
    PF_ASSERT_ALWAYS_STR_EQ(buf, "abcde");
    PF_ASSERT_ALWAYS(end == buf + 5);

    memset(buf, '#', sizeof(buf));
    end = pf_stpncpy(buf, "ab", 5);
    PF_ASSERT_ALWAYS(end == buf + 2);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "ab\0\0\0#", 6);
    end = pf_stpncpy(buf, "abcdef", 3);
    PF_ASSERT_ALWAYS(end == buf + 3);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "abc\0", 4);

    memset(buf, '#', sizeof(buf));
    end = (char *)pf_memccpy(buf, "abc:def", ':', 7);
    PF_ASSERT_ALWAYS(end == buf + 4);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "abc:#", 5);
    PF_ASSERT_ALWAYS(pf_memccpy(buf, "abcdef", 'z', 6) == NULL);
    PF_ASSERT_ALWAYS_MEM_EQ(buf, "abcdef#", 7);
    PF_ASSERT_ALWAYS(pf_memccpy(buf, "abc", 'a', 0) == NULL);
    PF_ASSERT_ALWAYS(
        pf_memccpy(
            buf,
            "\xFF"
            "a",
            -1,
            2
        )
        == buf + 1
    );
    return 0;
}

static int test_memset_explicit(int seed, int i) {
    unsigned char secret[64];
    size_t k;
    (void)seed, (void)i;

    memset(secret, 0x5A, sizeof(secret));
    PF_ASSERT_ALWAYS(pf_memset_explicit(secret, 0, sizeof(secret)) == secret);
    for (k = 0; k < sizeof(secret); k++)
        PF_ASSERT_ALWAYS_EQ(secret[k], 0);
    pf_memset_explicit(secret, 0xA5, 10);
    PF_ASSERT_ALWAYS(secret[9] == 0xA5 && secret[10] == 0);
    PF_ASSERT_ALWAYS(pf_memset_explicit(secret, 1, 0) == secret);
    PF_ASSERT_ALWAYS_EQ(secret[0], 0xA5);
    return 0;
}

static int test_asprintf(int seed, int i) {
    char *s = (char *)1;
    char big[1000];
    int n;
    (void)seed, (void)i;

    n = pf_asprintf(&s, "%d-%s", 42, "x");
    PF_ASSERT_ALWAYS_EQ(n, 4);
    PF_ASSERT_ALWAYS_STR_EQ(s, "42-x");
    free(s);

    n = pf_asprintf(&s, "%s", "");
    PF_ASSERT_ALWAYS_EQ(n, 0);
    PF_ASSERT_ALWAYS_STR_EQ(s, "");
    free(s);

    /* longer than the internal first try */
    memset(big, 'q', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    n = pf_asprintf(&s, "<%s>", big);
    PF_ASSERT_ALWAYS_EQ(n, 1001);
    PF_ASSERT_ALWAYS(s[0] == '<' && s[1000] == '>' && s[1001] == '\0');
    PF_ASSERT_ALWAYS_EQ(strspn(s + 1, "q"), 999);
    free(s);
    return 0;
}

pf_test_t suite_string[] = {
    { test_dup, "dup" },           { test_strnlen, "strnlen" },
    { test_strlcpy, "strlcpy" },   { test_casecmp, "casecmp" },
    { test_tokens, "tokens" },     { test_search, "search" },
    { test_copy, "copy" },         { test_memset_explicit, "memset_explicit" },
    { test_asprintf, "asprintf" }, { 0 },
};
