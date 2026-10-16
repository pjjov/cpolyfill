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

#include "../include/pf_dlsym.h"
#include "check.h"

/* A library and symbol that exist on every installation of the platform. */
#if defined(_WIN32)
    #define TEST_LIBS { "kernel32.dll", "KERNEL32.DLL", 0 }
    #define TEST_SYMBOL "GetCurrentProcessId"
#elif defined(__APPLE__)
    #define TEST_LIBS { "libSystem.B.dylib", "/usr/lib/libSystem.B.dylib", 0 }
    #define TEST_SYMBOL "strlen"
#else
    #define TEST_LIBS                                                          \
        { "libc.so.6", "libc.so", "libc.so.7", "libc.so.12", "libc.so.97", 0 }
    #define TEST_SYMBOL "strlen"
#endif

static void *open_system_library(void) {
    static const char *const names[] = TEST_LIBS;
    int i;

    for (i = 0; names[i]; i++) {
        void *lib = dlopen(names[i], RTLD_NOW);
        if (lib)
            return lib;
    }

#ifndef _WIN32
    /* The C library is also part of the running program (musl has no
       libc.so.6, for one). */
    return dlopen(NULL, RTLD_NOW);
#else
    return NULL;
#endif
}

static int test_dlsym_open_close(int seed, int rep) {
    void *lib = open_system_library();

    if (!lib)
        return SKIP("no system library found");

    CHECK(dlclose(lib) == 0);
    return 0;
}

static int test_dlsym_symbol(int seed, int rep) {
    void *lib = open_system_library();
    void *fn;

    if (!lib)
        return SKIP("no system library found");

    fn = dlsym(lib, TEST_SYMBOL);
    CHECK(fn != NULL);
    CHECK(dlclose(lib) == 0);
    return 0;
}

static int test_dlsym_call(int seed, int rep) {
#ifdef _WIN32
    typedef unsigned long (*pid_fn)(void);
    void *lib = open_system_library();
    pid_fn fn;
    void *sym;

    if (!lib)
        return SKIP("no system library found");

    sym = dlsym(lib, TEST_SYMBOL);
    CHECK(sym != NULL);
    memcpy(&fn, &sym, sizeof fn);
    CHECK(fn() > 0);
    dlclose(lib);
    return 0;
#else
    typedef size_t (*strlen_fn)(const char *);
    void *lib = open_system_library();
    strlen_fn fn;
    void *sym;

    if (!lib)
        return SKIP("no system library found");

    sym = dlsym(lib, TEST_SYMBOL);
    CHECK(sym != NULL);
    memcpy(&fn, &sym, sizeof fn);
    CHECK(fn("twelve chars") == 12);
    dlclose(lib);
    return 0;
#endif
}

static int test_dlsym_missing_library(int seed, int rep) {
    void *lib = dlopen("pf-no-such-library-xyz.so", RTLD_NOW);

    CHECK(lib == NULL);
    CHECK(dlerror() != NULL);
    return 0;
}

static int test_dlsym_missing_symbol(int seed, int rep) {
    void *lib = open_system_library();

    if (!lib)
        return SKIP("no system library found");

    CHECK(dlsym(lib, "pf_no_such_symbol_xyz") == NULL);
    CHECK(dlerror() != NULL);
    dlclose(lib);
    return 0;
}

static int test_dlsym_main_program(int seed, int rep) {
    /* A NULL path refers to the running program. */
    void *self = dlopen(NULL, RTLD_NOW);

    CHECK(self != NULL);
    CHECK(dlclose(self) == 0);
    return 0;
}

pf_test_t suite_dlsym[] = {
    { test_dlsym_open_close, "/dlsym/open_close" },
    { test_dlsym_symbol, "/dlsym/symbol" },
    { test_dlsym_call, "/dlsym/call" },
    { test_dlsym_missing_library, "/dlsym/missing_library" },
    { test_dlsym_missing_symbol, "/dlsym/missing_symbol" },
    { test_dlsym_main_program, "/dlsym/main_program" },
    { 0 },
};
