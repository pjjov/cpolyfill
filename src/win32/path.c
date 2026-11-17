/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    Paths for the Windows W APIs.
*/

#include "../internal.h"

#include <errno.h>
#include <string.h>
#include <windows.h>

/* Paths of this many UTF-16 units or more get the \\?\ prefix:
   CreateDirectoryW's limit (MAX_PATH less room for an 8.3 name) is the
   lowest of the classic APIs. */
#define PF__WPATH_LONG 248

static bool pf__has_prefix(const uint16_t *w) {
    return w[0] == '\\' && w[1] == '\\' && (w[2] == '?' || w[2] == '.')
        && w[3] == '\\';
}

int pf__wpath(pf__wstr *out, const char *path) {
    uint16_t *full;
    size_t len, size, skip, plen;
    DWORD n;
    int rc;

    if (!path)
        return PF_EINVAL;
    if (!*path)
        return PF_ENOENT;
    rc = pf__wstr_from_wtf8(out, path);
    if (rc)
        return rc;
    for (len = 0; out->ptr[len]; len++)
        if (out->ptr[len] == '/')
            out->ptr[len] = '\\';
    if (len < PF__WPATH_LONG || pf__has_prefix(out->ptr))
        return PF_OK;

    /* \\?\ turns off the path processing of Win32, so the path must be
       absolute and normalized first. */
    n = GetFullPathNameW((LPCWSTR)out->ptr, 0, NULL, NULL);
    if (n == 0) {
        rc = pf_error_last_win32();
        pf__wstr_free(out);
        return rc;
    }
    size = ((size_t)n + 8) * sizeof(uint16_t);
    full = (uint16_t *)allocate(NULL, size);
    if (!full) {
        pf__wstr_free(out);
        return PF_ENOMEM;
    }
    /* "\\server\share" becomes "\\?\UNC\server\share". */
    plen = 4;
    memcpy(full, L"\\\\?\\", 4 * sizeof(uint16_t));
    n = GetFullPathNameW((LPCWSTR)out->ptr, n, (LPWSTR)full + 8, NULL);
    pf__wstr_free(out);
    if (n == 0) {
        deallocate(NULL, full, size);
        return pf_error_last_win32();
    }
    skip = 8;
    if (full[8] == '\\' && full[9] == '\\') {
        memcpy(full + 4, L"UNC", 3 * sizeof(uint16_t));
        plen = 7;
        skip = 9; /* keep one of the two backslashes */
    }
    memmove(full + plen, full + skip, ((size_t)n - (skip - 8) + 1) * 2);
    out->ptr = full;
    out->heap_size = size;
    return PF_OK;
}

ptrdiff_t pf__path_from_wide(
    const uint16_t *w, size_t len, char *buf, size_t size
) {
    ptrdiff_t n;

    if (len == PF_NTS)
        for (len = 0; w[len]; len++) { }

    if (len >= 8 && w[0] == '\\' && w[1] == '\\' && w[2] == '?' && w[3] == '\\'
        && (w[4] | 0x20) == 'u' && (w[5] | 0x20) == 'n' && (w[6] | 0x20) == 'c'
        && w[7] == '\\') {
        /* \\?\UNC\server\share -> \\server\share */
        n = pf__utf16_to_wtf8(w + 8, len - 8, NULL, 0);
        if (n < 0)
            return n;
        if (!buf)
            return n + 2;
        if (size < 2)
            return PF_ERANGE;
        buf[0] = buf[1] = '\\';
        n = pf__utf16_to_wtf8(w + 8, len - 8, buf + 2, size - 2);
        return n < 0 ? n : n + 2;
    }
    if (len >= 6 && pf__has_prefix(w) && w[2] == '?' && w[5] == ':') {
        w += 4; /* \\?\C:\ -> C:\ */
        len -= 4;
    }
    return pf__utf16_to_wtf8(w, len, buf, buf ? size : 0);
}

int pf__errno_win32(void) {
    errno = pf_error_to_errno(pf_error_last_win32());
    return -1;
}
