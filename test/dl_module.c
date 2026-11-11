/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* A shared module that the dlfcn suite loads (its path comes from the
   PF_TEST_DL_MODULE environment variable). */

#include <cpolyfill/platform.h>

#if PF_OS_WINDOWS
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT int pf_test_dl_value = 42;

EXPORT int pf_test_dl_add(int a, int b) { return a + b; }
