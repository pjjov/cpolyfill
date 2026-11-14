/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* Throws from another module, without the call site, for the exception
   suite. */

#define PF_EXCEPTION_MODULE 9
#define PF_EXCEPTION_NO_LOCATION
#include <cpolyfill/exception.h>

int exception_throw_noloc(pf_exception_stack_t *s, int code);

int exception_throw_noloc(pf_exception_stack_t *s, int code) {
    return PF_THROWF(s, code, "module %d", PF_EXCEPTION_MODULE);
}
