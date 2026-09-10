/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides Windows implementations of functions from <dirent.h>.

    Last-updated: August 2026
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

#ifndef POLYFILL_DIRENT
#define POLYFILL_DIRENT

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

enum pf_dirent_error {
    PF_DE_ENOENT = -2,
    PF_DE_EIO = -5,
    PF_DE_EBADF = -9,
    PF_DE_ENOMEM = -12,
    PF_DE_EINVAL = -22,
    PF_DE_ENAMETOOLONG = -36,
};

#ifndef _WIN32
    #include <dirent.h>
#else

    #define WIN32_LEAN_AND_MEAN
    #include "windows.h"

    #ifndef PF_MALLOC
        #include <stdlib.h>
        #define PF_MALLOC malloc
        #define PF_REALLOC realloc
        #define PF_FREE free
    #endif

    #include <string.h>

    #define NAME_MAX MAX_PATH

PF_API void pf_dirent_swap_char(char *string, char old, char c) {
    while (NULL != (string = strchr(string, old))) {
        *string = c;
    }
}

struct dirent {
    ino_t d_ino;
    char d_name[MAX_PATH + 1];
};

typedef struct DIR {
    long int pos;
    HANDLE handle;
    WIN32_FIND_DATA find;
    struct dirent *ent;
    char path[];
} DIR;

PF_API DIR *opendir(const char *path) {
    errno = 0;
    if (!path) {
        errno = PF_DE_ENOENT;
        return NULL;
    }

    size_t length = strnlen(path, MAX_PATH - 1);
    if (length == 0) {
        errno = PF_DE_ENOENT;
        return NULL;
    }
    if (length == MAX_PATH - 1) {
        errno = PF_DE_ENAMETOOLONG;
        return NULL;
    }

    DIR *dir = PF_MALLOC(sizeof(DIR) + length + 3);
    if (!dir) {
        errno = PF_DE_ENOMEM;
        return NULL;
    }

    memcpy(dir->path, path, length);
    #ifndef PF_KEEP_SLASHES
    pf_dirent_swap_char(dir->path, '/', '\\');
    #endif
    if (dir->path[length - 1] != '\\')
        dir->path[length++] = '\\';
    dir->path[length++] = '*';
    dir->path[length] = '\0';

    dir->handle = FindFirstFile(dir->path, &dir->find);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        errno = GetLastError() == ERROR_FILE_NOT_FOUND ? PF_DE_ENOENT
                                                       : PF_DE_EIO;
        PF_FREE(dir);
        return NULL;
    }

    dir->ent = NULL;
    dir->pos = 0;
    return dir;
}

PF_API int closedir(DIR *dir) {
    if (!dir) {
        errno = PF_DE_EBADF;
        return -1;
    }

    if (dir->ent)
        PF_FREE(dir->ent);

    if (dir->handle != INVALID_HANDLE_VALUE) {
        if (!FindClose(dir->handle)) {
            errno = PF_DE_EIO;
            PF_FREE(dir);
            return -1;
        }
    }

    PF_FREE(dir);
    return 0;
}

PF_API int readdir_r(DIR *dir, struct dirent *ent, struct dirent **result) {
    errno = 0;
    if (!dir || !ent || !result || dir->handle == INVALID_HANDLE_VALUE) {
        *result = NULL;
        return !dir ? PF_DE_EBADF : PF_DE_EINVAL;
    }

    if (dir->pos > 0) {
        if (!FindNextFile(dir->handle, &dir->find)) {
            if (GetLastError() == ERROR_NO_MORE_FILES) {
                *result = NULL;
                return 0;
            }

            return PF_DE_EINVAL;
        }
    }

    ent->d_ino = 0;
    strncpy(ent->d_name, dir->find.cFilename, MAX_PATH);
    ent->d_name[MAX_PATH] = '\0';

    #ifndef PF_KEEP_BACKSLASHES
    pf_dirent_swap_char(ent->d_name, '\\', '/');
    #endif
    *result = ent;
    dir->pos++;
    return 0;
}

PF_API struct dirent *readdir(DIR *dir) {
    if (!dir) {
        errno = PF_DE_EBADF;
        return NULL;
    }

    if (!dir->ent) {
        dir->ent = PF_MALLOC(sizeof(struct dirent));
        if (!dir->ent) {
            errno = PF_DE_ENOMEM;
            return NULL;
        }
    }

    struct dirent *result;
    errno = readdir_r(dir, dir->ent, &result);
    return result;
}

PF_API void seekdir(DIR *dir, long int pos) {
    if (dir->pos > pos) {
        if (dir->handle != INVALID_HANDLE_VALUE)
            FindClose(dir->handle);

        dir->handle = FindFirstFile(dir->path, &dir->find);
        dir->pos = 0;
        if (dir->handle == INVALID_HANDLE_VALUE)
            return;
    }

    while (dir->pos < pos) {
        if (!FindNextFile(dir->handle, &dir->find))
            break;
        dir->pos++;
    }
}

PF_API void rewinddir(DIR *dir) { return seekdir(dir, 0); }
PF_API long int telldir(DIR *dir) { return dir->pos; }

#endif

/* Called once per directory entry. Return non-zero to stop iteration early. */
typedef int(pf_iterdir_fn)(const struct dirent *entry, void *user_data);

PF_API int pf_iterdir(const char *path, pf_iterdir_fn *callback, void *user) {
    if (!callback) {
        errno = PF_DE_EINVAL;
        return -1;
    }

    DIR *dir = opendir(path);
    if (!dir)
        return -1;

    int result = 0;
    int had_error = 0;
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            had_error = (errno != 0);
            break;
        }

        result = callback(entry, user);
        if (result != 0)
            break;
    }

    closedir(dir);

    return had_error ? -1 : result;
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_DIRENT */
