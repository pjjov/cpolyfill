/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

/* The atomic operations through the library's backend, which compilers
   without atomics use, run by the atomic suite. */

#define PF__ATOMIC_FORCE_LOCKED
#define ATOMIC_OPS_NAME atomic_ops_lib
#include "atomic_ops.h"
