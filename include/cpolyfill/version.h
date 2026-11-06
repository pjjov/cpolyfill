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
**/

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
