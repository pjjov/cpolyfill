/** cpolyfill - Filling the gaps between C standards and compilers

    Library version.

    The macros give the version of the headers being compiled against,
    the functions the version of the library actually linked in.

        PF_VERSION_MAJOR, PF_VERSION_MINOR, PF_VERSION_PATCH
        PF_VERSION_STRING           "major.minor.patch"
        PF_VERSION_NUMBER           major * 10000 + minor * 100 + patch
        PF_VERSION_PREREQ(major, minor)
                                    The headers are at least that version.

        int pf_version(void);               linked version, as PF_VERSION_NUMBER
        const char *pf_version_string(void);   linked version, as a string

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_VERSION_H
#define CPOLYFILL_VERSION_H

#include <cpolyfill/platform.h>

#define PF_VERSION_NUMBER                                               \
    (PF_VERSION_MAJOR * 10000 + PF_VERSION_MINOR * 100 + PF_VERSION_PATCH)

#define PF_VERSION_PREREQ(major, minor)                       \
    (PF_VERSION_NUMBER >= (major) * 10000 + (minor) * 100)

PF_BEGIN_DECLS

PF_API int pf_version(void);
PF_API const char *pf_version_string(void);

PF_END_DECLS

#endif /* CPOLYFILL_VERSION_H */
