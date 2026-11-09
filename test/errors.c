/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Code that must not compile, checked by check_errors.py. Built with
   PF_ERROR_CASE=0 it must compile (with warnings as errors); with any
   other case it must fail. Valid in C and C++. */

#include <cpolyfill/macro.h>
#include <cpolyfill/stdc.h>
#include <cpolyfill/types.h>

#define PF_ERROR_CASES 9

struct item {
    int key;
    char name[8];
};

enum fruit { fruit_apple, fruit_pear, fruit_sentinel };

#if PF_ERROR_CASE == 1
PF_STATIC_ASSERT(sizeof(char) == 2);
#elif PF_ERROR_CASE == 2
PF_STATIC_ASSERT(sizeof(char) == 2, "with a message");
#endif

int main(void) {
    int n = 1;
    int arr[3] = { 0, 0, 0 };
    int *p = arr;
    struct item it = { 0, { 0 } };

#if PF_ERROR_CASE == 0
    PF_STATIC_ASSERT(sizeof(char) == 1);
    PF_STATIC_ASSERT(sizeof(char) == 1, "with a message");
    PF_ENUM_GUARD(fruit, 2);
    n = (int)PF_COUNTOF(arr);
    n = PF_CHECK_TYPE(int, n);
    p = &PF_CONTAINER_OF(&it.key, struct item, key)->key;
#elif PF_ERROR_CASE == 3
    /* Not a constant. */
    PF_STATIC_ASSERT(n == 1);
#elif PF_ERROR_CASE == 4
    PF_STATIC_ASSERT(0, "in a block");
#elif PF_ERROR_CASE == 5
    n = (int)PF_COUNTOF(p);
#elif PF_ERROR_CASE == 6
    (void)PF_CHECK_TYPE(long, n);
#elif PF_ERROR_CASE == 7
    (void)PF_CHECK_TYPE(int *, arr);
#elif PF_ERROR_CASE == 8
    PF_ENUM_GUARD(fruit, 3);
#elif PF_ERROR_CASE == 9
    p = &PF_CONTAINER_OF(&it.name, struct item, key)->key;
#endif

    (void)n, (void)arr, (void)p, (void)it;
    return 0;
}
