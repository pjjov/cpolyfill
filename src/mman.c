/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/mman.h>: <sys/mman.h>, or file mapping objects on Windows.
*/

#include "internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>
#include <cpolyfill/mman.h>

#include <errno.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <io.h>
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
        #define MAP_ANONYMOUS MAP_ANON
    #endif
#endif

#define PF__PROT_ALL (PF_PROT_READ | PF_PROT_WRITE | PF_PROT_EXEC)
#define PF__MAP_ALL (PF_MAP_SHARED | PF_MAP_PRIVATE | PF_MAP_ANONYMOUS)

/* Checks the arguments common to both backends; 0 or a PF_E* code. */
static int pf__map_check(size_t len, int prot, int flags, pf_off_t off) {
    int kind = flags & (PF_MAP_SHARED | PF_MAP_PRIVATE);

    if (len == 0 || (prot & ~PF__PROT_ALL) || (flags & ~PF__MAP_ALL)
        || (kind != PF_MAP_SHARED && kind != PF_MAP_PRIVATE) || off < 0
        || (pf_off_t)((uint64_t)off % pf_map_granularity()) != 0
        || ((flags & PF_MAP_ANONYMOUS) && off != 0))
        return PF_EINVAL;
    return PF_OK;
}

#if PF_OS_WINDOWS

static SYSTEM_INFO pf__sysinfo(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si;
}

PF_API size_t pf_page_size(void) { return pf__sysinfo().dwPageSize; }

PF_API size_t pf_map_granularity(void) {
    return pf__sysinfo().dwAllocationGranularity;
}

/* The page protection for `prot`; `cow` for copy-on-write views. */
static DWORD pf__page_prot(int prot, bool cow) {
    bool w = (prot & PF_PROT_WRITE) != 0, x = (prot & PF_PROT_EXEC) != 0;

    if (!(prot & PF__PROT_ALL))
        return PAGE_NOACCESS;
    if (x)
        return w ? (cow ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READWRITE)
                 : ((prot & PF_PROT_READ) ? PAGE_EXECUTE_READ : PAGE_EXECUTE);
    if (w)
        return cow ? PAGE_WRITECOPY : PAGE_READWRITE;
    return PAGE_READONLY;
}

PF_API void *pf_mmap(
    void *addr, size_t len, int prot, int flags, int fd, pf_off_t off
) {
    bool anon = (flags & PF_MAP_ANONYMOUS) != 0;
    bool cow = !anon && (flags & PF_MAP_PRIVATE);
    bool exec = (prot & PF_PROT_EXEC) != 0;
    HANDLE file = INVALID_HANDLE_VALUE, mapping;
    DWORD protect, access, old;
    uint64_t max = 0;
    void *view;
    int rc;

    (void)addr;
    if ((rc = pf__map_check(len, prot, flags, off)) != 0) {
        pf__errno_fail(rc);
        return PF_MAP_FAILED;
    }
    /* The mapping gets the most access the view can have, so pf_mprotect
       can widen what pf_mmap was asked for. */
    if (anon) {
        protect = exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
        access = FILE_MAP_WRITE | (exec ? FILE_MAP_EXECUTE : 0);
        max = len; /* backed by the paging file: the size is needed */
    } else {
        file = (HANDLE)_get_osfhandle(fd);
        if (file == INVALID_HANDLE_VALUE) {
            errno = EBADF;
            return PF_MAP_FAILED;
        }
        if (cow) {
            protect = exec ? PAGE_EXECUTE_WRITECOPY : PAGE_WRITECOPY;
            access = FILE_MAP_COPY;
        } else if (prot & PF_PROT_WRITE) {
            protect = exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
            access = FILE_MAP_WRITE;
        } else {
            protect = exec ? PAGE_EXECUTE_READ : PAGE_READONLY;
            access = FILE_MAP_READ;
        }
        if (exec)
            access |= FILE_MAP_EXECUTE;
    }

    mapping = CreateFileMappingW(
        file, NULL, protect, (DWORD)(max >> 32), (DWORD)max, NULL
    );
    if (!mapping) {
        pf__errno_win32();
        return PF_MAP_FAILED;
    }
    view = MapViewOfFile(
        mapping, access, (DWORD)((uint64_t)off >> 32), (DWORD)off, len
    );
    if (!view) {
        pf__errno_win32();
        CloseHandle(mapping);
        return PF_MAP_FAILED;
    }
    CloseHandle(mapping); /* the view keeps it */

    if (!VirtualProtect(view, len, pf__page_prot(prot, cow), &old)) {
        pf__errno_win32();
        UnmapViewOfFile(view);
        return PF_MAP_FAILED;
    }
    return view;
}

PF_API int pf_munmap(void *addr, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;

    (void)len;
    if (!VirtualQuery(addr, &mbi, sizeof(mbi)) || mbi.AllocationBase != addr
        || mbi.Type != MEM_MAPPED)
        return pf__errno_fail(PF_EINVAL);
    return UnmapViewOfFile(addr) ? 0 : pf__errno_win32();
}

PF_API int pf_mprotect(void *addr, size_t len, int prot) {
    MEMORY_BASIC_INFORMATION mbi;
    DWORD old;
    bool cow;

    if ((prot & ~PF__PROT_ALL) || !VirtualQuery(addr, &mbi, sizeof(mbi)))
        return pf__errno_fail(PF_EINVAL);
    cow = mbi.AllocationProtect == PAGE_WRITECOPY
        || mbi.AllocationProtect == PAGE_EXECUTE_WRITECOPY;
    return VirtualProtect(addr, len, pf__page_prot(prot, cow), &old)
        ? 0
        : pf__errno_win32();
}

PF_API int pf_msync(void *addr, size_t len, int flags) {
    if (flags & ~(PF_MS_ASYNC | PF_MS_SYNC | PF_MS_INVALIDATE))
        return pf__errno_fail(PF_EINVAL);
    return FlushViewOfFile(addr, len) ? 0 : pf__errno_win32();
}

PF_API int pf_mlock(const void *addr, size_t len) {
    return VirtualLock((LPVOID)addr, len) ? 0 : pf__errno_win32();
}

PF_API int pf_munlock(const void *addr, size_t len) {
    return VirtualUnlock((LPVOID)addr, len) ? 0 : pf__errno_win32();
}

#else /* POSIX */

PF_API size_t pf_page_size(void) {
    long n = sysconf(_SC_PAGESIZE);
    return n > 0 ? (size_t)n : 4096;
}

PF_API size_t pf_map_granularity(void) { return pf_page_size(); }

static int pf__native_prot(int prot) {
    return ((prot & PF_PROT_READ) ? PROT_READ : 0)
        | ((prot & PF_PROT_WRITE) ? PROT_WRITE : 0)
        | ((prot & PF_PROT_EXEC) ? PROT_EXEC : 0);
}

PF_API void *pf_mmap(
    void *addr, size_t len, int prot, int flags, int fd, pf_off_t off
) {
    int native = (flags & PF_MAP_SHARED) ? MAP_SHARED : MAP_PRIVATE;
    void *p;
    int rc;

    if ((rc = pf__map_check(len, prot, flags, off)) != 0) {
        pf__errno_fail(rc);
        return PF_MAP_FAILED;
    }
    if ((off_t)off != off) {
        errno = EOVERFLOW;
        return PF_MAP_FAILED;
    }
    if (flags & PF_MAP_ANONYMOUS) {
        native |= MAP_ANONYMOUS;
        fd = -1;
    }
    p = mmap(addr, len, pf__native_prot(prot), native, fd, (off_t)off);
    return p == MAP_FAILED ? PF_MAP_FAILED : p;
}

PF_API int pf_munmap(void *addr, size_t len) { return munmap(addr, len); }

PF_API int pf_mprotect(void *addr, size_t len, int prot) {
    if (prot & ~PF__PROT_ALL)
        return pf__errno_fail(PF_EINVAL);
    return mprotect(addr, len, pf__native_prot(prot));
}

PF_API int pf_msync(void *addr, size_t len, int flags) {
    int native = (flags & PF_MS_SYNC) ? MS_SYNC : MS_ASYNC;

    if (flags & ~(PF_MS_ASYNC | PF_MS_SYNC | PF_MS_INVALIDATE))
        return pf__errno_fail(PF_EINVAL);
    if (flags & PF_MS_INVALIDATE)
        native |= MS_INVALIDATE;
    return msync(addr, len, native);
}

PF_API int pf_mlock(const void *addr, size_t len) { return mlock(addr, len); }

PF_API int pf_munlock(const void *addr, size_t len) {
    return munlock(addr, len);
}

#endif

/* ---- Whole files ------------------------------------------------------ */

PF_API int pf_map_file(pf_mapped_file_t *m, const char *path, unsigned flags) {
    bool write = (flags & PF_MAP_FILE_WRITE) != 0;
    pf_stat_t st;
    void *p;
    int fd, rc;

    if (!m || !path || (flags & ~PF_MAP_FILE_WRITE))
        return PF_EINVAL;
    m->data = NULL;
    m->size = 0;
    fd = pf_open(path, (write ? PF_O_RDWR : PF_O_RDONLY) | PF_O_CLOEXEC, 0);
    if (fd < 0)
        return pf_error_from_errno(errno);
    if ((rc = pf_fstat(fd, &st)) != 0) {
        pf_close(fd);
        return rc;
    }
    if (st.type != PF_FILE_REGULAR) {
        pf_close(fd);
        return st.type == PF_FILE_DIR ? PF_EISDIR : PF_EINVAL;
    }
    if (st.size == 0) {
        pf_close(fd);
        return PF_OK;
    }
    if (st.size > SIZE_MAX) {
        pf_close(fd);
        return PF_EFBIG;
    }
    p = pf_mmap(
        NULL,
        (size_t)st.size,
        PF_PROT_READ | (write ? PF_PROT_WRITE : 0),
        PF_MAP_SHARED,
        fd,
        0
    );
    rc = p == PF_MAP_FAILED ? pf_error_from_errno(errno) : PF_OK;
    pf_close(fd);
    if (rc)
        return rc;
    m->data = p;
    m->size = (size_t)st.size;
    return PF_OK;
}

PF_API int pf_unmap_file(pf_mapped_file_t *m) {
    if (!m)
        return PF_EINVAL;
    if (m->data && pf_munmap(m->data, m->size) != 0)
        return pf_error_from_errno(errno);
    m->data = NULL;
    m->size = 0;
    return PF_OK;
}
