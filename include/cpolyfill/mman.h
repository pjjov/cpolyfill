/** cpolyfill - Filling the gaps between C standards and compilers

    Memory mappings: POSIX mmap and friends on every platform, and whole
    files mapped in one call.

        void *pf_mmap(void *addr, size_t len, int prot, int flags, int fd,
                      pf_off_t off)
                Maps `len` bytes of the file `fd` from offset `off`, or new
                zeroed memory with PF_MAP_ANONYMOUS (`fd` is then ignored,
                `off` must be 0). Returns PF_MAP_FAILED with errno on
                failure. `addr` is a hint, ignored on Windows.
                `prot`: PF_PROT_NONE, or any of PF_PROT_READ,
                PF_PROT_WRITE, PF_PROT_EXEC.
                `flags`: PF_MAP_SHARED (changes reach the file and other
                mappings of it) or PF_MAP_PRIVATE (copy on write), and
                optionally PF_MAP_ANONYMOUS. Only these flags are
                accepted, also on POSIX (their values are the library's).
                `off` must be a multiple of pf_map_granularity().
        int pf_munmap(void *addr, size_t len)
        int pf_mprotect(void *addr, size_t len, int prot)
        int pf_msync(void *addr, size_t len, int flags)
                PF_MS_SYNC or PF_MS_ASYNC, optionally PF_MS_INVALIDATE.
        int pf_mlock(const void *addr, size_t len)
        int pf_munlock(const void *addr, size_t len)
                0, or -1 with errno.
        size_t pf_page_size(void)
        size_t pf_map_granularity(void)
                The page size, and the alignment of mapping offsets: the
                page size on POSIX, 64 KiB on Windows.

    On Windows a mapping can only be unmapped as a whole (`addr` must be
    what pf_mmap returned, `len` is ignored), a file mapping cannot extend
    past the end of the file (POSIX allows it up to the end of the last
    page) and cannot be executable (the CRT opens files without execute
    access), and pf_msync with PF_MS_SYNC writes the pages but does not
    wait for the device (use pf_fsync on the descriptor for that).

    Whole files (0, or a negative PF_E* code):

        pf_mapped_file_t        data   the content (NULL if the file is
                                       empty)
                                size   its size in bytes
        int pf_map_file(pf_mapped_file_t *m, const char *path,
                        unsigned flags)
                Maps a whole file, read-only, or shared and writable with
                PF_MAP_FILE_WRITE. The file need not stay open.
        int pf_unmap_file(pf_mapped_file_t *m)
                Unmaps it and clears `m`.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_MMAN, only where the
    library found no <sys/mman.h> (Windows): `mmap`, `munmap`,
    `mprotect`, `msync`, `mlock`, `munlock`, `MAP_FAILED`, `PROT_*`,
    `MAP_SHARED`, `MAP_PRIVATE`, `MAP_ANONYMOUS`, `MAP_ANON`, `MS_*`.

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

#ifndef CPOLYFILL_MMAN_H
#define CPOLYFILL_MMAN_H

#include <cpolyfill/config.h>
#include <cpolyfill/io.h>
#include <cpolyfill/platform.h>

#include <stddef.h>

#define PF_PROT_NONE 0
#define PF_PROT_READ 1
#define PF_PROT_WRITE 2
#define PF_PROT_EXEC 4

#define PF_MAP_SHARED 0x01
#define PF_MAP_PRIVATE 0x02
#define PF_MAP_ANONYMOUS 0x20
#define PF_MAP_ANON PF_MAP_ANONYMOUS
#define PF_MAP_FAILED ((void *)-1)

#define PF_MS_ASYNC 1
#define PF_MS_INVALIDATE 2
#define PF_MS_SYNC 4

#define PF_MAP_FILE_WRITE 1u

typedef struct pf_mapped_file_t {
    void *data;
    size_t size;
} pf_mapped_file_t;

PF_BEGIN_DECLS

PF_API void *pf_mmap(
    void *addr, size_t len, int prot, int flags, int fd, pf_off_t off
);
PF_API int pf_munmap(void *addr, size_t len);
PF_API int pf_mprotect(void *addr, size_t len, int prot);
PF_API int pf_msync(void *addr, size_t len, int flags);
PF_API int pf_mlock(const void *addr, size_t len);
PF_API int pf_munlock(const void *addr, size_t len);
PF_API size_t pf_page_size(void);
PF_API size_t pf_map_granularity(void);

PF_API int pf_map_file(pf_mapped_file_t *m, const char *path, unsigned flags);
PF_API int pf_unmap_file(pf_mapped_file_t *m);

PF_END_DECLS

#endif /* CPOLYFILL_MMAN_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__MMAN_STD_NAMES)                                   \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_MMAN))  \
        || defined(PF_STD_NAMES_MMAN))
    #define PF__MMAN_STD_NAMES

    #if !PF_CONFIG_HAVE_SYS_MMAN_H
        #define PROT_NONE PF_PROT_NONE
        #define PROT_READ PF_PROT_READ
        #define PROT_WRITE PF_PROT_WRITE
        #define PROT_EXEC PF_PROT_EXEC
        #define MAP_SHARED PF_MAP_SHARED
        #define MAP_PRIVATE PF_MAP_PRIVATE
        #define MAP_ANONYMOUS PF_MAP_ANONYMOUS
        #define MAP_ANON PF_MAP_ANON
        #define MAP_FAILED PF_MAP_FAILED
        #define MS_ASYNC PF_MS_ASYNC
        #define MS_INVALIDATE PF_MS_INVALIDATE
        #define MS_SYNC PF_MS_SYNC
        #define mmap pf_mmap
        #define munmap pf_munmap
        #define mprotect pf_mprotect
        #define msync pf_msync
        #define mlock pf_mlock
        #define munlock pf_munlock
    #endif
#endif
