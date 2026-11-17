/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    pf_opendir and friends on FindFirstFileExW.
*/

#include "../internal.h"

#include <cpolyfill/dirent.h>

#include <errno.h>
#include <string.h>
#include <windows.h>

struct pf_dir_t {
    HANDLE handle; /* INVALID_HANDLE_VALUE when there are no entries */
    bool first;    /* `find` holds an entry not returned yet */
    uint16_t *pattern;
    size_t pattern_size;
    WIN32_FIND_DATAW find;
    struct pf_dirent ent;
};

/* Opens the search; 0 or a PF_E* code. */
static int pf__dir_start(pf_dir_t *d) {
    d->handle = FindFirstFileExW(
        (LPCWSTR)d->pattern,
        FindExInfoBasic,
        &d->find,
        FindExSearchNameMatch,
        NULL,
        FIND_FIRST_EX_LARGE_FETCH
    );
    d->first = d->handle != INVALID_HANDLE_VALUE;
    if (d->handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) /* empty root directory */
            return PF_OK;
        return pf_error_from_win32(err);
    }
    return PF_OK;
}

PF_API pf_dir_t *pf_opendir(const char *path) {
    pf__wstr w;
    pf_dir_t *d;
    size_t len;
    int rc;

    if ((rc = pf__wpath(&w, path)) != 0) {
        pf__errno_fail(rc);
        return NULL;
    }
    for (len = 0; w.ptr[len]; len++) { }

    d = (pf_dir_t *)zallocate(NULL, sizeof(*d));
    if (!d) {
        pf__wstr_free(&w);
        errno = ENOMEM;
        return NULL;
    }
    d->pattern_size = (len + 3) * sizeof(uint16_t);
    d->pattern = (uint16_t *)allocate(NULL, d->pattern_size);
    if (!d->pattern) {
        pf__wstr_free(&w);
        deallocate(NULL, d, sizeof(*d));
        errno = ENOMEM;
        return NULL;
    }
    memcpy(d->pattern, w.ptr, len * sizeof(uint16_t));
    pf__wstr_free(&w);
    if (len && d->pattern[len - 1] != '\\' && d->pattern[len - 1] != ':')
        d->pattern[len++] = '\\';
    d->pattern[len++] = '*';
    d->pattern[len] = 0;

    if ((rc = pf__dir_start(d)) != 0) {
        deallocate(NULL, d->pattern, d->pattern_size);
        deallocate(NULL, d, sizeof(*d));
        pf__errno_fail(rc == PF_EINVAL ? PF_ENOTDIR : rc);
        return NULL;
    }
    return d;
}

PF_API struct pf_dirent *pf_readdir(pf_dir_t *d) {
    ptrdiff_t n;
    DWORD attr;

    if (!d) {
        errno = EBADF;
        return NULL;
    }
    if (d->handle == INVALID_HANDLE_VALUE)
        return NULL;
    if (!d->first) {
        if (!FindNextFileW(d->handle, &d->find)) {
            if (GetLastError() == ERROR_NO_MORE_FILES)
                return NULL;
            pf__errno_win32();
            return NULL;
        }
    }
    d->first = false;

    n = pf__utf16_to_wtf8(
        (const uint16_t *)d->find.cFileName,
        PF_NTS,
        d->ent.d_name,
        sizeof(d->ent.d_name)
    );
    if (n < 0) {
        pf__errno_fail(n == PF_ERANGE ? PF_ENAMETOOLONG : (int)n);
        return NULL;
    }
    d->ent.d_ino = 0;
    attr = d->find.dwFileAttributes;
    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT)
        && (d->find.dwReserved0 == IO_REPARSE_TAG_SYMLINK
            || d->find.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT))
        d->ent.d_type = PF_DT_LNK;
    else if (attr & FILE_ATTRIBUTE_DIRECTORY)
        d->ent.d_type = PF_DT_DIR;
    else
        d->ent.d_type = PF_DT_REG;
    return &d->ent;
}

PF_API void pf_rewinddir(pf_dir_t *d) {
    if (!d)
        return;
    if (d->handle != INVALID_HANDLE_VALUE)
        FindClose(d->handle);
    pf__dir_start(d);
}

PF_API int pf_closedir(pf_dir_t *d) {
    if (!d) {
        errno = EBADF;
        return -1;
    }
    if (d->handle != INVALID_HANDLE_VALUE)
        FindClose(d->handle);
    deallocate(NULL, d->pattern, d->pattern_size);
    deallocate(NULL, d, sizeof(*d));
    return 0;
}
