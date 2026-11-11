/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* <cpolyfill/dlfcn.h>, on the module built from dl_module.c. */

#include "../src/internal.h"

#include <cpolyfill/dlfcn.h>
#include <cpolyfill/fs.h>

#include <stdlib.h>
#include <string.h>

#include "check.h"

typedef int (*add_fn)(int, int);

static add_fn as_add(void *p) {
    add_fn f;
    memcpy(&f, &p, sizeof(f));
    return f;
}

static int test_module(int seed, int i) {
    const char *path = getenv("PF_TEST_DL_MODULE");
    pf_dl_info_t info;
    void *h, *sym;
    int *value;
    size_t len;
    (void)seed, (void)i;

    if (!path)
        return SKIP("PF_TEST_DL_MODULE is not set (run through meson test)");
    /* Whatever happened before (the sanitizers' own lookups) */
    (void)pf_dlerror();

    h = pf_dlopen(path, PF_RTLD_NOW | PF_RTLD_LOCAL);
    PF_ASSERT_ALWAYS_MSG(h != NULL, "%s", pf_dlerror());
    sym = pf_dlsym(h, "pf_test_dl_add");
    PF_ASSERT_ALWAYS(sym != NULL);
    PF_ASSERT_ALWAYS_EQ(as_add(sym)(2, 3), 5);
    value = (int *)pf_dlsym(h, "pf_test_dl_value");
    PF_ASSERT_ALWAYS(value != NULL);
    PF_ASSERT_ALWAYS_EQ(*value, 42);
    PF_ASSERT_ALWAYS(pf_dlerror() == NULL);

    /* a missing symbol, reported once */
    PF_ASSERT_ALWAYS(pf_dlsym(h, "pf_test_dl_missing") == NULL);
    PF_ASSERT_ALWAYS(pf_dlerror() != NULL);
    PF_ASSERT_ALWAYS(pf_dlerror() == NULL);
    PF_ASSERT_ALWAYS(pf_dlsym(h, NULL) == NULL);
    PF_ASSERT_ALWAYS(pf_dlerror() != NULL);

    /* the library holding an address */
    PF_ASSERT_ALWAYS(pf_dladdr(sym, &info) != 0);
    PF_ASSERT_ALWAYS(info.dli_fbase != NULL);
    PF_ASSERT_ALWAYS_STR_EQ(
        pf_path_basename(info.dli_fname, &len), pf_path_basename(path, NULL)
    );
    if (info.dli_sname) {
        PF_ASSERT_ALWAYS_STR_EQ(info.dli_sname, "pf_test_dl_add");
        PF_ASSERT_ALWAYS(info.dli_saddr == sym);
    }

    /* the default search finds symbols of global libraries */
    {
        void *g = pf_dlopen(path, PF_RTLD_NOW | PF_RTLD_GLOBAL);
        PF_ASSERT_ALWAYS(g != NULL);
        PF_ASSERT_ALWAYS(pf_dlsym(PF_RTLD_DEFAULT, "pf_test_dl_add") == sym);
        PF_ASSERT_ALWAYS_EQ(pf_dlclose(g), 0);
    }
    PF_ASSERT_ALWAYS_EQ(pf_dlclose(h), 0);
    return 0;
}

static int test_errors(int seed, int i) {
    void *self;
    (void)seed, (void)i;

    PF_ASSERT_ALWAYS(pf_dlopen("/no/such/library" PF_DL_EXT, PF_RTLD_LAZY) == NULL);
    PF_ASSERT_ALWAYS(pf_dlerror() != NULL);
    PF_ASSERT_ALWAYS(pf_dlerror() == NULL);
    PF_ASSERT_ALWAYS(pf_dlclose(NULL) != 0);
    PF_ASSERT_ALWAYS(pf_dlerror() != NULL);

    self = pf_dlopen(NULL, PF_RTLD_LAZY);
    PF_ASSERT_ALWAYS(self != NULL);
    PF_ASSERT_ALWAYS_EQ(pf_dlclose(self), 0);
    return 0;
}

static int test_ext(int seed, int i) {
    (void)seed, (void)i;
#if PF_OS_WINDOWS
    PF_ASSERT_ALWAYS_STR_EQ(PF_DL_EXT, ".dll");
#elif PF_OS_APPLE
    PF_ASSERT_ALWAYS_STR_EQ(PF_DL_EXT, ".dylib");
#else
    PF_ASSERT_ALWAYS_STR_EQ(PF_DL_EXT, ".so");
#endif
    return 0;
}

pf_test_t suite_dlfcn[] = {
    { test_module, "module" },
    { test_errors, "errors" },
    { test_ext, "ext" },
    { 0 },
};
