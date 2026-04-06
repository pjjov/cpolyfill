/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides Windows implementations of functions from <dirent.h>.

    SPDX-FileCopyrightText: 2026 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Predrag Jovanović

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

#ifndef POLYFILL_DLSYM
#define POLYFILL_DLSYM

#ifndef PF_API
    #define PF_API static inline
#endif

#ifndef _WIN32
    #include <dlfcn.h>
#else

    #define WIN32_LEAN_AND_MEAN
    #include "windows.h"
    #include <string.h>

    #define RTLD_LAZY 0x00001
    #define RTLD_NOW 0x00002
    #define RTLD_GLOBAL 0x00100
    #define RTLD_LOCAL 0

static char pf__dlerror_buf[256] = "";

PF_API void *dlopen(const char *path, int mode) {
    (void)mode;

    HMODULE lib = path ? LoadLibraryA(path) : GetModuleHandle(NULL);

    if (!lib) {
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            0,
            pf__dlerror_buf,
            sizeof(pf__dlerror_buf),
            NULL
        );
    }

    return (void *)lib;
}

PF_API void *dlsym(void *restrict lib, const char *restrict name) {
    FARPROC result = GetProcAddress((HMODULE)lib, name);

    if (!result) {
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            0,
            pf__dlerror_buf,
            sizeof(pf__dlerror_buf),
            NULL
        );
    }

    return (void *)result;
}

PF_API char *dlerror(void) {
    if (pf__dlerror_buf[0] == '\0')
        return NULL;
    return pf__dlerror_buf;
}

PF_API int dlclose(void *lib) {
    HMODULE handle = (HMODULE)lib;
    return !FreeLibrary(handle);
}

#endif
#endif
