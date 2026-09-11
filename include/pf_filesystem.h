/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides cross-platform filesystem operations.
    For Windows, `mkdir`, `rmdir`, `chdir` and `getcwd` are implemented.

    Function reference:
    - pf_homedir
    - pf_datadir
    - pf_confdir
    - pf_statedir
    - pf_cachedir
    - pf_runtimedir
    - pf_userdir

    Last-updated: September 2026
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

#ifndef POLYFILL_FILESYSTEM
#define POLYFILL_FILESYSTEM

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define PF_DIR_WIN32
#else
    #define PF_DIR_POSIX
#endif

#ifdef PF_DIR_POSIX
    #include <pwd.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(PF_DIR_WIN32)
    #include <knownfolders.h>
    #include <lm.h>
    #include <shlobj.h>
    #include <windows.h>
#endif

#ifndef PF_MALLOC
    #define PF_MALLOC malloc
    #define PF_REALLOC realloc
    #define PF_FREE free
#endif

#ifndef pf_static_assert
    #if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) \
        || defined(static_assert)
        #include <assert.h>
        #define pf_static_assert static_assert
    #else
        #define pf_static_assert(expr, msg)
    #endif
#endif

enum pf_filesystem_error {
    PF_FS_ENOENT = -2,
    PF_FS_EIO = -5,
    PF_FS_ENOMEM = -12,
    PF_FS_EEXIST = -17,
    PF_FS_EINVAL = -22,
    PF_FS_ERANGE = -34,
    PF_FS_ENOTEMPTY = -39,
};

enum pf_userdir_kind {
    PF_USERDIR_DESKTOP,
    PF_USERDIR_DOCUMENTS,
    PF_USERDIR_DOWNLOAD,
    PF_USERDIR_MUSIC,
    PF_USERDIR_PICTURES,
    PF_USERDIR_PROJECTS,
    PF_USERDIR_PUBLICSHARE,
    PF_USERDIR_TEMPLATES,
    PF_USERDIR_VIDEOS,
    PF__USERDIR_COUNT = 9,
};

PF_API int pf__fs_copy(char *buf, size_t size, const char *src) {
    size_t len;

    if (!src)
        return -PF_FS_EINVAL;

    len = strlen(src);

    if (len + 1 > size)
        return -PF_FS_ERANGE;

    if (!buf)
        return -PF_FS_EINVAL;

    memcpy(buf, src, len + 1);
    return (int)len;
}

PF_API int pf__fs_copy_join(
    char *buf, size_t size, const char *dir, const char *leaf
) {
    size_t dlen = strlen(dir);
    size_t llen = strlen(leaf);
    int need_sep = (dlen > 0 && dir[dlen - 1] != '/'
#ifdef PF_DIR_WIN32
                  && dir[dlen - 1] != '\\'
#endif
  );
    size_t total = dlen + (need_sep ? 1 : 0) + llen;

    if (total + 1 > size)
        return -PF_FS_ERANGE;
    if (!buf)
        return -PF_FS_EINVAL;

    memcpy(buf, dir, dlen);
    if (need_sep) {
#ifdef PF_DIR_WIN32
        buf[dlen] = '\\';
#else
        buf[dlen] = '/';
#endif
        dlen++;
    }
    memcpy(buf + dlen, leaf, llen + 1);
    return (int)total;
}

#ifdef PF_DIR_WIN32

PF_API int mkdir(const char *path, int mode) {
    (void)mode;
    errno = 0;
    if (!path) {
        errno = PF_FS_ENOENT;
        return -1;
    }

    if (!CreateDirectoryA(path, NULL)) {
        DWORD err = GetLastError();
        switch (err) {
        case ERROR_ALREADY_EXISTS:
            errno = PF_FS_EEXIST;
            break;
        case ERROR_PATH_NOT_FOUND:
            errno = PF_FS_ENOENT;
            break;
        default:
            errno = PF_FS_EIO;
            break;
        }
        return -1;
    }

    return 0;
}

PF_API int rmdir(const char *path) {
    errno = 0;
    if (!path) {
        errno = PF_FS_ENOENT;
        return -1;
    }

    if (!RemoveDirectoryA(path)) {
        DWORD err = GetLastError();
        switch (err) {
        case ERROR_DIR_NOT_EMPTY:
            errno = PF_FS_ENOTEMPTY;
            break;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            errno = PF_FS_ENOENT;
            break;
        default:
            errno = PF_FS_EIO;
            break;
        }
        return -1;
    }

    return 0;
}

PF_API int chdir(const char *path) {
    errno = 0;
    if (!path) {
        errno = PF_FS_ENOENT;
        return -1;
    }

    if (!SetCurrentDirectoryA(path)) {
        DWORD err = GetLastError();
        errno = (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
            ? PF_FS_ENOENT
            : PF_FS_EIO;
        return -1;
    }

    return 0;
}

PF_API char *getcwd(char *buf, size_t size) {
    errno = 0;

    DWORD needed = GetCurrentDirectoryA(0, NULL);
    if (needed == 0) {
        errno = PF_FS_EIO;
        return NULL;
    }

    int allocated = 0;
    if (!buf) {
        /* Allocate a buffer big enough when buf is NULL. */
        size = size ? size : (size_t)needed;
        buf = PF_MALLOC(size);
        if (!buf) {
            errno = PF_FS_ENOMEM;
            return NULL;
        }
        allocated = 1;
    } else if (size < (size_t)needed) {
        errno = PF_FS_ERANGE;
        return NULL;
    }

    DWORD written = GetCurrentDirectoryA((DWORD)size, buf);
    if (written == 0 || written > size) {
        if (allocated)
            PF_FREE(buf);
        errno = PF_FS_EIO;
        return NULL;
    }

    return buf;
}

#endif

#ifdef PF_DIR_POSIX
PF_API int pf_homedir_unix(const char *username, char *buf, size_t size) {
    if (!username || !*username) {
        const char *home = getenv("HOME");
        if (home && *home)
            return pf__fs_copy(buf, size, home);

        /* HOME not set: fall back to passwd db for the real uid */
        struct passwd pwbuf;
        struct passwd *pw = NULL;
        char stackbuf[4096];
        int rc = getpwuid_r(getuid(), &pwbuf, stackbuf, sizeof(stackbuf), &pw);
        if (rc != 0 || !pw)
            return -PF_FS_ENOENT;
        return pf__fs_copy(buf, size, pw->pw_dir);
    }

    struct passwd pwbuf;
    struct passwd *pw = NULL;
    char stackbuf[4096];
    int rc = getpwnam_r(username, &pwbuf, stackbuf, sizeof(stackbuf), &pw);
    if (rc != 0 || !pw)
        return -PF_FS_ENOENT;
    return pf__fs_copy(buf, size, pw->pw_dir);
}
#endif

#ifdef PF_DIR_WIN32
PF_API int pf_homedir_windows(const char *username, char *buf, size_t size) {
    if (!username || !*username) {
        /* Active user's profile directory */
        wchar_t wpath[MAX_PATH];
        DWORD wsize = MAX_PATH;

        HANDLE tok = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
            /* Fall back to %USERPROFILE% */
            char envbuf[MAX_PATH];
            DWORD n = GetEnvironmentVariableA(
                "USERPROFILE", envbuf, sizeof(envbuf)
            );
            if (n == 0 || n >= sizeof(envbuf))
                return -PF_FS_ENOENT;
            return pf__fs_copy(buf, size, envbuf);
        }

        BOOL ok = GetUserProfileDirectoryW(tok, wpath, &wsize);
        CloseHandle(tok);
        if (!ok)
            return -PF_FS_ENOENT;

        char narrow[MAX_PATH * 2];
        int n = WideCharToMultiByte(
            CP_UTF8, 0, wpath, -1, narrow, sizeof(narrow), NULL, NULL
        );
        if (n <= 0)
            return -PF_FS_ENOENT;
        return pf__fs_copy(buf, size, narrow);
    }

    /*
   * Windows has no cheap per-user API keyed by name that doesn't
   * require elevated/network lookups. Try NetUserGetInfo (level 3,
   * usri3_home_dir) first; fall back to the conventional
   * %SystemDrive%\Users\<username> layout if that's empty/unset,
   * which matches how a shell would expand ~username on a typical
   * local machine.
   */
    wchar_t wuser[256];
    if (MultiByteToWideChar(CP_UTF8, 0, username, -1, wuser, 256) <= 0)
        return -PF_FS_EINVAL;

    USER_INFO_3 *info = NULL;
    NET_API_STATUS st = NetUserGetInfo(NULL, wuser, 3, (LPBYTE *)&info);
    if (st == NERR_Success && info) {
        if (info->usri3_home_dir && info->usri3_home_dir[0]) {
            char narrow[MAX_PATH * 2];
            int n = WideCharToMultiByte(
                CP_UTF8,
                0,
                info->usri3_home_dir,
                -1,
                narrow,
                sizeof(narrow),
                NULL,
                NULL
            );
            NetApiBufferFree(info);
            if (n <= 0)
                return -PF_FS_ENOENT;
            return pf__fs_copy(buf, size, narrow);
        }
        NetApiBufferFree(info);
    }

    char sysdrive[16];
    DWORD n = GetEnvironmentVariableA(
        "SystemDrive", sysdrive, sizeof(sysdrive)
    );
    if (n == 0 || n >= sizeof(sysdrive))
        strcpy(sysdrive, "C:");

    char guess[MAX_PATH];
    snprintf(guess, sizeof(guess), "%s\\Users\\%s", sysdrive, username);
    return pf__fs_copy(buf, size, guess);
}
#endif

PF_API int pf_homedir(const char *username, char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    return pf_homedir_unix(username, buf, size);
#else
    return pf_homedir_windows(username, buf, size);
#endif
}

#ifdef PF_DIR_POSIX

/* env var with fallback of "$HOME/<default_leaf>" */
PF_API int pf_xdg_base(
    char *buf, size_t size, const char *env_name, const char *default_leaf
) {
    const char *env = getenv(env_name);
    if (env && *env == '/')
        return pf__fs_copy(buf, size, env);

    char home[4096];
    int hr = pf_homedir_unix(NULL, home, sizeof(home));
    if (hr < 0)
        return hr;

    return pf__fs_copy_join(buf, size, home, default_leaf);
}

#endif

#ifdef PF_DIR_WIN32

PF_API int pf_known_folder(char *buf, size_t size, REFKNOWNFOLDERID id) {
    PWSTR wpath = NULL;
    HRESULT hr = SHGetKnownFolderPath(id, 0, NULL, &wpath);
    if (FAILED(hr) || !wpath)
        return -PF_FS_ENOENT;

    char narrow[MAX_PATH * 2];
    int n = WideCharToMultiByte(
        CP_UTF8, 0, wpath, -1, narrow, sizeof(narrow), NULL, NULL
    );
    CoTaskMemFree(wpath);
    if (n <= 0)
        return -PF_FS_ENOENT;

    return pf__fs_copy(buf, size, narrow);
}

#endif

int pf_datadir(char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    return pf_xdg_base(buf, size, "XDG_DATA_HOME", ".local/share");
#else
    return pf_known_folder(buf, size, &FOLDERID_LocalAppData);
#endif
}

int pf_confdir(char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    return pf_xdg_base(buf, size, "XDG_CONFIG_HOME", ".config");
#else
    return pf_known_folder(buf, size, &FOLDERID_RoamingAppData);
#endif
}

int pf_statedir(char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    return pf_xdg_base(buf, size, "XDG_STATE_HOME", ".local/state");
#else
    /* No direct Windows analogue; local app data is the closest fit
   * for "persists across reboots but is machine/user local". */
    return pf_known_folder(buf, size, &FOLDERID_LocalAppData);
#endif
}

int pf_cachedir(char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    return pf_xdg_base(buf, size, "XDG_CACHE_HOME", ".cache");
#else
    /* Prefer LocalLow-style temp/cache location: LocalAppData\...\Temp
   * has no single KNOWNFOLDERID; use the standard temp path. */
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp))
        return -PF_FS_ENOENT;
    /* strip trailing backslash for consistency with other dirs */
    if (n > 0 && tmp[n - 1] == '\\')
        tmp[n - 1] = '\0';
    return pf__fs_copy(buf, size, tmp);
#endif
}

int pf_runtimedir(char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    const char *env = getenv("XDG_RUNTIME_DIR");
    if (env && *env == '/')
        return pf__fs_copy(buf, size, env);

    /* No spec-mandated fallback; use the conventional per-uid dir
   * under /run/user if it exists, else /tmp. */
    char guess[128];
    snprintf(guess, sizeof(guess), "/run/user/%ld", (long)getuid());
    if (access(guess, F_OK) == 0)
        return pf__fs_copy(buf, size, guess);

    return pf__fs_copy(buf, size, "/tmp");
#else
    char tmp[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tmp), tmp);
    if (n == 0 || n >= sizeof(tmp))
        return -PF_FS_ENOENT;
    if (n > 0 && tmp[n - 1] == '\\')
        tmp[n - 1] = '\0';
    return pf__fs_copy(buf, size, tmp);
#endif
}

#ifdef PF_DIR_POSIX

/*
 * Parses ~/.config/user-dirs.dirs looking for `key`. Lines look like:
 *   XDG_DESKTOP_DIR="$HOME/Desktop"
 * Only the "$HOME/..." and absolute-path forms are handled, which
 * covers everything xdg-user-dirs-update actually generates.
 */
static int pf_parse_user_dirs_dirs(const char *key, char *out, size_t outsize) {
    char confpath[4096];
    int cr = pf_confdir(confpath, sizeof(confpath));
    if (cr < 0)
        return cr;

    char filepath[4200];
    if (pf__fs_copy_join(filepath, sizeof(filepath), confpath, "user-dirs.dirs")
        < 0)
        return -PF_FS_ERANGE;

    FILE *f = fopen(filepath, "r");
    if (!f)
        return -PF_FS_ENOENT;

    char line[4096];
    int found = -PF_FS_ENOENT;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        size_t klen = strlen(key);
        if (strncmp(p, key, klen) != 0)
            continue;
        p += klen;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p != '=')
            continue;
        p++;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p != '"')
            continue;
        p++;

        char value[4096];
        size_t vlen = 0;
        int is_home_relative = 0;

        if (strncmp(p, "$HOME", 5) == 0) {
            is_home_relative = 1;
            p += 5;
            if (*p == '/')
                p++;
        }

        while (*p && *p != '"' && vlen + 1 < sizeof(value)) {
            value[vlen++] = *p++;
        }
        value[vlen] = '\0';

        if (is_home_relative) {
            char home[4096];
            int hr = pf_homedir_unix(NULL, home, sizeof(home));
            if (hr < 0) {
                found = hr;
                break;
            }
            found = pf__fs_copy_join(out, outsize, home, value);
        } else {
            found = pf__fs_copy(out, outsize, value);
        }
        break;
    }

    fclose(f);
    return found;
}

static int pf_userdir_unix(int kind, char *buf, size_t size) {
    static_assert(
        PF__USERDIR_COUNT == 9, "Update below tables if enum changed."
    );

    /* xdg-user-dirs config keys, in pf_userdir_kind order */
    static const char *pf_xdg_user_key[] = {
        "XDG_DESKTOP_DIR",     "XDG_DOCUMENTS_DIR",
        "XDG_DOWNLOAD_DIR",    "XDG_MUSIC_DIR",
        "XDG_PICTURES_DIR",    NULL, /* PROJECTS has no xdg-user-dirs entry */
        "XDG_PUBLICSHARE_DIR", "XDG_TEMPLATES_DIR",
        "XDG_VIDEOS_DIR",
    };

    /* Default leaf names (English, matches xdg-user-dirs-update defaults) */
    static const char *pf_default_leaf[] = {
        "Desktop",  "Documents", "Downloads", "Music",  "Pictures",
        "Projects", "Public",    "Templates", "Videos",
    };

    if (kind < 0 || (size_t)kind >= PF__USERDIR_COUNT)
        return -PF_FS_EINVAL;

    const char *key = pf_xdg_user_key[kind];

    if (key) {
        int r = pf_parse_user_dirs_dirs(key, buf, size);
        if (r >= 0 || r == -PF_FS_ERANGE)
            return r;
        /* -PF_FS_ENOENT (no config file / key missing): fall through to default */
    }

    char home[4096];
    int hr = pf_homedir_unix(NULL, home, sizeof(home));
    if (hr < 0)
        return hr;

    return pf__fs_copy_join(buf, size, home, pf_default_leaf[kind]);
}

#endif /* PF_DIR_POSIX */

#ifdef PF_DIR_WIN32

static int pf_userdir_windows(int kind, char *buf, size_t size) {
    switch (kind) {
    case PF_USERDIR_DESKTOP:
        return pf_known_folder(buf, size, &FOLDERID_Desktop);
    case PF_USERDIR_DOCUMENTS:
        return pf_known_folder(buf, size, &FOLDERID_Documents);
    case PF_USERDIR_DOWNLOAD:
        return pf_known_folder(buf, size, &FOLDERID_Downloads);
    case PF_USERDIR_MUSIC:
        return pf_known_folder(buf, size, &FOLDERID_Music);
    case PF_USERDIR_PICTURES:
        return pf_known_folder(buf, size, &FOLDERID_Pictures);
    case PF_USERDIR_PUBLICSHARE:
        return pf_known_folder(buf, size, &FOLDERID_Public);
    case PF_USERDIR_TEMPLATES:
        return pf_known_folder(buf, size, &FOLDERID_Templates);
    case PF_USERDIR_VIDEOS:
        return pf_known_folder(buf, size, &FOLDERID_Videos);
    case PF_USERDIR_PROJECTS: {
        /* No KNOWNFOLDERID for "Projects"; conventional location is
     * <Documents>\Projects, matching how Visual Studio et al.
     * default their workspace root. */
        char docs[4096];
        int dr = pf_known_folder(docs, sizeof(docs), &FOLDERID_Documents);
        if (dr < 0)
            return dr;
        return pf__fs_copy_join(buf, size, docs, "Projects");
    }
    default:
        return -PF_FS_EINVAL;
    }
}

#endif /* PF_DIR_WIN32 */

int pf_userdir(int kind, char *buf, size_t size) {
#ifdef PF_DIR_POSIX
    return pf_userdir_unix(kind, buf, size);
#else
    return pf_userdir_windows(kind, buf, size);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* POLYFILL_FILESYSTEM */
