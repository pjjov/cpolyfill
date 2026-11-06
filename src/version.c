/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <cpolyfill/version.h>

PF_API int pf_version(void) { return PF_VERSION_NUMBER; }

PF_API const char *pf_version_string(void) { return PF_VERSION_STRING; }
