/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The portable part of <cpolyfill/io.h>: whole files and streams, on the
    backends (posix/io.c, win32/io.c).
*/

#include "internal.h"

#include <cpolyfill/fs.h>
#include <cpolyfill/io.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- Whole files ------------------------------------------------------ */

PF_API int pf_read_file(
    const char *path, void **data, size_t *size, struct allocator_t *alloc
) {
    size_t cap, len = 0;
    pf_stat_t st;
    char *buf, *p;
    int fd, rc = PF_OK;

    if (!path || !data || !size)
        return PF_EINVAL;
    *data = NULL;
    *size = 0;
    fd = pf_open(path, PF_O_RDONLY | PF_O_CLOEXEC, 0);
    if (fd < 0)
        return pf_error_from_errno(errno);

    /* The size is a hint: it may be 0 (/proc, pipes) or change. */
    cap = 4096;
    if (pf_fstat(fd, &st) != 0)
        st.size = 0;
    else if (st.type == PF_FILE_DIR) {
        pf_close(fd);
        return PF_EISDIR;
    }
    if (st.size > 0) {
        if (st.size >= SIZE_MAX) {
            pf_close(fd);
            return PF_EFBIG;
        }
        cap = (size_t)st.size + 1;
    }
    buf = (char *)allocate(alloc, cap);
    if (!buf) {
        pf_close(fd);
        return PF_ENOMEM;
    }

    for (;;) {
        pf_ssize_t n;
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            if (ncap <= cap) {
                rc = PF_EFBIG;
                break;
            }
            p = (char *)reallocate(alloc, buf, cap, ncap);
            if (!p) {
                rc = PF_ENOMEM;
                break;
            }
            buf = p;
            cap = ncap;
        }
        n = pf_read(fd, buf + len, cap - 1 - len);
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            rc = pf_error_from_errno(errno);
            break;
        }
        len += (size_t)n;
    }
    pf_close(fd);

    /* Exactly `len + 1` bytes, so pf_read_file_free knows the size. */
    if (rc == 0 && cap != len + 1) {
        p = (char *)reallocate(alloc, buf, cap, len + 1);
        if (!p)
            rc = PF_ENOMEM;
        else {
            buf = p;
            cap = len + 1;
        }
    }
    if (rc) {
        deallocate(alloc, buf, cap);
        return rc;
    }
    buf[len] = 0;
    *data = buf;
    *size = len;
    return PF_OK;
}

PF_API void pf_read_file_free(
    void *data, size_t size, struct allocator_t *alloc
) {
    if (data)
        deallocate(alloc, data, size + 1);
}

static int pf__write_fd(int fd, const void *data, size_t size) {
    const char *p = (const char *)data;

    while (size) {
        pf_ssize_t n = pf_write(fd, p, size);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return pf_error_from_errno(errno);
        }
        if (n == 0)
            return PF_EIO;
        p += n;
        size -= (size_t)n;
    }
    return PF_OK;
}

PF_API int pf_write_file(
    const char *path, const void *data, size_t size, unsigned flags
) {
    static const char suffix[] = ".tmp-XXXXXX";
    size_t len;
    pf_stat_t st;
    char *tmp;
    int fd, rc;

    if (!path || (!data && size) || (flags & ~PF_WRITE_ATOMIC))
        return PF_EINVAL;
    if (!(flags & PF_WRITE_ATOMIC)) {
        fd = pf_open(
            path, PF_O_WRONLY | PF_O_CREAT | PF_O_TRUNC | PF_O_CLOEXEC, 0666
        );
        if (fd < 0)
            return pf_error_from_errno(errno);
        rc = pf__write_fd(fd, data, size);
        if (pf_close(fd) != 0 && rc == 0)
            rc = pf_error_from_errno(errno);
        return rc;
    }

    /* A temporary file next to `path` (rename only works within a file
       system), created like the file itself would be (umask applies). */
    len = strlen(path);
    tmp = (char *)allocate(NULL, len + sizeof(suffix));
    if (!tmp)
        return PF_ENOMEM;
    memcpy(tmp, path, len);
    memcpy(tmp + len, suffix, sizeof(suffix));
    fd = pf__mktemp(tmp, false, 0666);
    if (fd < 0) {
        rc = pf_error_from_errno(errno);
        deallocate(NULL, tmp, len + sizeof(suffix));
        return rc;
    }

    rc = pf__write_fd(fd, data, size);
    if (rc == 0 && pf_fsync(fd) != 0)
        rc = pf_error_from_errno(errno);
    if (pf_close(fd) != 0 && rc == 0)
        rc = pf_error_from_errno(errno);
    /* Keep the permissions of the file being replaced. */
    if (rc == 0 && pf_stat(path, &st) == 0 && st.type == PF_FILE_REGULAR
        && pf_chmod(tmp, (pf_mode_t)st.perm) != 0)
        rc = pf_error_from_errno(errno);
    if (rc == 0 && pf_rename(tmp, path) != 0)
        rc = pf_error_from_errno(errno);
    if (rc)
        pf_unlink(tmp);
#if !PF_OS_WINDOWS
    else {
        /* The rename itself reaches the device with the directory. */
        size_t dlen;
        const char *dir = pf_path_dirname(path, &dlen);
        char *d = (char *)allocate(NULL, dlen + 1);
        if (d) {
            int dfd;
            memcpy(d, dir, dlen);
            d[dlen] = 0;
            dfd = pf_open(d, PF_O_RDONLY | PF_O_CLOEXEC, 0);
            if (dfd >= 0) {
                pf_fsync(dfd);
                pf_close(dfd);
            }
            deallocate(NULL, d, dlen + 1);
        }
    }
#endif
    deallocate(NULL, tmp, len + sizeof(suffix));
    return rc;
}

/* ---- Streams ---------------------------------------------------------- */

PF_API int pf_stream_init(
    pf_stream_t *s, const struct pf_stream_vt *vt, void *ctx
) {
    if (!s || !vt)
        return PF_EINVAL;
    s->vt = vt;
    s->ctx = ctx;
    return PF_OK;
}

PF_API pf_ssize_t pf_stream_read(pf_stream_t *s, void *buf, size_t n) {
    if (!s || !s->vt || (!buf && n))
        return PF_EINVAL;
    if (!s->vt->read)
        return PF_EBADF;
    if (n == 0)
        return 0;
    return s->vt->read(s->ctx, buf, n);
}

PF_API pf_ssize_t pf_stream_read_full(pf_stream_t *s, void *buf, size_t n) {
    size_t done = 0;

    if (n > (size_t)PTRDIFF_MAX)
        return PF_EINVAL;
    while (done < n) {
        pf_ssize_t got = pf_stream_read(s, (char *)buf + done, n - done);
        if (got < 0)
            return got;
        if (got == 0)
            break;
        done += (size_t)got;
    }
    return (pf_ssize_t)done;
}

PF_API pf_ssize_t pf_stream_write(pf_stream_t *s, const void *buf, size_t n) {
    if (!s || !s->vt || (!buf && n))
        return PF_EINVAL;
    if (!s->vt->write)
        return PF_EBADF;
    if (n == 0)
        return 0;
    return s->vt->write(s->ctx, buf, n);
}

PF_API int pf_stream_write_all(pf_stream_t *s, const void *buf, size_t n) {
    const char *p = (const char *)buf;

    while (n) {
        pf_ssize_t w = pf_stream_write(s, p, n);
        if (w < 0)
            return (int)w;
        if (w == 0)
            return PF_EIO;
        p += w;
        n -= (size_t)w;
    }
    return s && s->vt ? PF_OK : PF_EINVAL;
}

PF_API int64_t pf_stream_seek(pf_stream_t *s, int64_t off, int whence) {
    if (!s || !s->vt)
        return PF_EINVAL;
    if (!s->vt->seek)
        return PF_ESPIPE;
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
        return PF_EINVAL;
    return s->vt->seek(s->ctx, off, whence);
}

PF_API int64_t pf_stream_tell(pf_stream_t *s) {
    return pf_stream_seek(s, 0, SEEK_CUR);
}

PF_API int pf_stream_flush(pf_stream_t *s) {
    if (!s || !s->vt)
        return PF_EINVAL;
    return s->vt->flush ? s->vt->flush(s->ctx) : PF_OK;
}

PF_API int pf_stream_close(pf_stream_t *s) {
    int rc;

    if (!s || !s->vt)
        return PF_EINVAL;
    rc = s->vt->close ? s->vt->close(s->ctx) : PF_OK;
    s->vt = NULL;
    s->ctx = NULL;
    return rc;
}

PF_API int pf_stream_putc(pf_stream_t *s, int c) {
    unsigned char b = (unsigned char)c;
    return pf_stream_write_all(s, &b, 1);
}

PF_API int pf_stream_puts(pf_stream_t *s, const char *str) {
    if (!str)
        return PF_EINVAL;
    return pf_stream_write_all(s, str, strlen(str));
}

PF_API int pf_stream_vprintf(pf_stream_t *s, const char *fmt, va_list ap) {
    char small[256], *buf = small;
    va_list copy;
    int n, rc;

    if (!fmt)
        return PF_EINVAL;
    va_copy(copy, ap);
    n = vsnprintf(small, sizeof(small), fmt, copy);
    va_end(copy);
    if (n < 0)
        return PF_EILSEQ;
    if ((size_t)n >= sizeof(small)) {
        buf = (char *)allocate(NULL, (size_t)n + 1);
        if (!buf)
            return PF_ENOMEM;
        vsnprintf(buf, (size_t)n + 1, fmt, ap);
    }
    rc = pf_stream_write_all(s, buf, (size_t)n);
    if (buf != small)
        deallocate(NULL, buf, (size_t)n + 1);
    return rc ? rc : n;
}

PF_API int pf_stream_printf(pf_stream_t *s, const char *fmt, ...) {
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = pf_stream_vprintf(s, fmt, ap);
    va_end(ap);
    return n;
}

PF_API int64_t pf_stream_copy(pf_stream_t *dst, pf_stream_t *src) {
    char buf[8192];
    int64_t total = 0;

    for (;;) {
        pf_ssize_t n = pf_stream_read(src, buf, sizeof(buf));
        int rc;
        if (n < 0)
            return n;
        if (n == 0)
            return total;
        if ((rc = pf_stream_write_all(dst, buf, (size_t)n)) != 0)
            return rc;
        total += n;
    }
}

/* stdio files: `ctx` is the FILE. */

static pf_ssize_t pf__file_read(void *ctx, void *buf, size_t n) {
    FILE *f = (FILE *)ctx;
    size_t got;

    if (n > (size_t)PTRDIFF_MAX)
        n = (size_t)PTRDIFF_MAX;
    errno = 0;
    got = fread(buf, 1, n, f);
    if (got == 0 && ferror(f)) {
        clearerr(f);
        return errno ? pf_error_from_errno(errno) : PF_EIO;
    }
    return (pf_ssize_t)got;
}

static pf_ssize_t pf__file_write(void *ctx, const void *buf, size_t n) {
    FILE *f = (FILE *)ctx;
    size_t put;

    if (n > (size_t)PTRDIFF_MAX)
        n = (size_t)PTRDIFF_MAX;
    errno = 0;
    put = fwrite(buf, 1, n, f);
    if (put == 0 && ferror(f)) {
        clearerr(f);
        return errno ? pf_error_from_errno(errno) : PF_EIO;
    }
    return (pf_ssize_t)put;
}

static int64_t pf__file_seek(void *ctx, int64_t off, int whence) {
    FILE *f = (FILE *)ctx;
    pf_off_t pos;

    if (pf_fseeko(f, off, whence) != 0)
        return pf_error_from_errno(errno);
    pos = pf_ftello(f);
    return pos < 0 ? pf_error_from_errno(errno) : pos;
}

static int pf__file_flush(void *ctx) {
    return fflush((FILE *)ctx) == 0 ? PF_OK : pf_error_from_errno(errno);
}

static int pf__file_close(void *ctx) {
    return fclose((FILE *)ctx) == 0 ? PF_OK : pf_error_from_errno(errno);
}

static const struct pf_stream_vt pf__file_vt = {
    pf__file_read, pf__file_write, pf__file_seek, pf__file_flush,
    pf__file_flush, /* not owned: closing flushes */
};

static const struct pf_stream_vt pf__file_own_vt = {
    pf__file_read, pf__file_write, pf__file_seek, pf__file_flush,
    pf__file_close,
};

PF_API int pf_stream_from_file(pf_stream_t *s, FILE *f, bool own) {
    if (!f)
        return PF_EINVAL;
    return pf_stream_init(s, own ? &pf__file_own_vt : &pf__file_vt, f);
}

PF_API int pf_stream_fopen(pf_stream_t *s, const char *path, const char *mode) {
    FILE *f;

    if (!s || !path || !mode)
        return PF_EINVAL;
    f = pf_fopen(path, mode);
    if (!f)
        return pf_error_from_errno(errno);
    return pf_stream_from_file(s, f, true);
}

/* File descriptors: `ctx` holds the descriptor. */

#define PF__FD(ctx) ((int)(intptr_t)(ctx))

static pf_ssize_t pf__fd_read(void *ctx, void *buf, size_t n) {
    for (;;) {
        pf_ssize_t got = pf_read(PF__FD(ctx), buf, n);
        if (got >= 0)
            return got;
        if (errno != EINTR)
            return pf_error_from_errno(errno);
    }
}

static pf_ssize_t pf__fd_write(void *ctx, const void *buf, size_t n) {
    for (;;) {
        pf_ssize_t put = pf_write(PF__FD(ctx), buf, n);
        if (put >= 0)
            return put;
        if (errno != EINTR)
            return pf_error_from_errno(errno);
    }
}

static int64_t pf__fd_seek(void *ctx, int64_t off, int whence) {
    pf_off_t pos = pf_lseek(PF__FD(ctx), off, whence);
    return pos < 0 ? pf_error_from_errno(errno) : pos;
}

static int pf__fd_close(void *ctx) {
    return pf_close(PF__FD(ctx)) == 0 ? PF_OK : pf_error_from_errno(errno);
}

static const struct pf_stream_vt pf__fd_vt = {
    pf__fd_read, pf__fd_write, pf__fd_seek, NULL, NULL,
};

static const struct pf_stream_vt pf__fd_own_vt = {
    pf__fd_read, pf__fd_write, pf__fd_seek, NULL, pf__fd_close,
};

PF_API int pf_stream_from_fd(pf_stream_t *s, int fd, bool own) {
    if (fd < 0)
        return PF_EBADF;
    return pf_stream_init(
        s, own ? &pf__fd_own_vt : &pf__fd_vt, (void *)(intptr_t)fd
    );
}

/* Memory: a fixed buffer, or one that grows (pf_stream_buffer). */

typedef struct pf__mem {
    char *data;
    size_t size; /* the content: [0, size) */
    size_t cap;  /* allocated (growing) or the buffer's size (fixed) */
    size_t pos;
    bool writable, grows;
    struct allocator_t *alloc;
} pf__mem;

static pf_ssize_t pf__mem_read(void *ctx, void *buf, size_t n) {
    pf__mem *m = (pf__mem *)ctx;
    size_t left = m->pos < m->size ? m->size - m->pos : 0;

    if (n > left)
        n = left;
    if (n > (size_t)PTRDIFF_MAX)
        n = (size_t)PTRDIFF_MAX;
    memcpy(buf, m->data + m->pos, n);
    m->pos += n;
    return (pf_ssize_t)n;
}

static pf_ssize_t pf__mem_write(void *ctx, const void *buf, size_t n) {
    pf__mem *m = (pf__mem *)ctx;

    if (!m->writable)
        return PF_EBADF;
    if (n > (size_t)PTRDIFF_MAX)
        n = (size_t)PTRDIFF_MAX;
    if (m->grows) {
        /* Room for the content and a NUL after it. */
        if (m->pos > SIZE_MAX - 1 - n)
            return PF_EFBIG;
        if (m->pos + n + 1 > m->cap) {
            size_t ncap = m->cap ? m->cap : 64;
            char *p;
            while (ncap < m->pos + n + 1)
                ncap = ncap > SIZE_MAX / 2 ? m->pos + n + 1 : ncap * 2;
            p = (char *)reallocate(m->alloc, m->data, m->cap, ncap);
            if (!p)
                return PF_ENOMEM;
            m->data = p;
            m->cap = ncap;
        }
    } else {
        if (m->pos >= m->cap)
            return PF_ENOSPC;
        if (n > m->cap - m->pos)
            n = m->cap - m->pos;
    }
    if (m->pos > m->size)
        memset(m->data + m->size, 0, m->pos - m->size); /* the gap */
    memcpy(m->data + m->pos, buf, n);
    m->pos += n;
    if (m->pos > m->size)
        m->size = m->pos;
    if (m->grows)
        m->data[m->size] = 0;
    return (pf_ssize_t)n;
}

static int64_t pf__mem_seek(void *ctx, int64_t off, int whence) {
    pf__mem *m = (pf__mem *)ctx;
    int64_t base = whence == SEEK_SET ? 0
                 : whence == SEEK_CUR ? (int64_t)m->pos
                                      : (int64_t)m->size;

    if ((off < 0 && base + off < 0) || (off > 0 && base > INT64_MAX - off))
        return PF_EINVAL;
    off += base;
    if (!m->grows && (uint64_t)off > m->cap)
        return PF_EINVAL;
    if ((uint64_t)off > SIZE_MAX)
        return PF_EOVERFLOW;
    m->pos = (size_t)off;
    return off;
}

static int pf__mem_close(void *ctx) {
    pf__mem *m = (pf__mem *)ctx;
    struct allocator_t *alloc = m->alloc;

    if (m->grows && m->data)
        deallocate(alloc, m->data, m->cap);
    deallocate(alloc, m, sizeof(*m));
    return PF_OK;
}

static const struct pf_stream_vt pf__mem_vt = {
    pf__mem_read, pf__mem_write, pf__mem_seek, NULL, pf__mem_close,
};

/* A distinct table, so pf_stream_buffer_data can recognise its streams. */
static const struct pf_stream_vt pf__buffer_vt = {
    pf__mem_read, pf__mem_write, pf__mem_seek, NULL, pf__mem_close,
};

static int pf__mem_new(
    pf_stream_t *s, const struct pf_stream_vt *vt, struct allocator_t *alloc,
    pf__mem **out
) {
    pf__mem *m;

    if (!s)
        return PF_EINVAL;
    m = (pf__mem *)zallocate(alloc, sizeof(*m));
    if (!m)
        return PF_ENOMEM;
    m->alloc = alloc;
    *out = m;
    return pf_stream_init(s, vt, m);
}

PF_API int pf_stream_memory(
    pf_stream_t *s,
    void *buf,
    size_t size,
    bool writable,
    struct allocator_t *alloc
) {
    pf__mem *m;
    int rc;

    if (!buf && size)
        return PF_EINVAL;
    if ((rc = pf__mem_new(s, &pf__mem_vt, alloc, &m)) != 0)
        return rc;
    m->data = (char *)buf;
    m->size = m->cap = size;
    m->writable = writable;
    return PF_OK;
}

PF_API int pf_stream_buffer(pf_stream_t *s, struct allocator_t *alloc) {
    pf__mem *m;
    int rc;

    if ((rc = pf__mem_new(s, &pf__buffer_vt, alloc, &m)) != 0)
        return rc;
    m->writable = m->grows = true;
    return PF_OK;
}

PF_API const void *pf_stream_buffer_data(const pf_stream_t *s, size_t *size) {
    const pf__mem *m;

    if (!s || s->vt != &pf__buffer_vt)
        return NULL;
    m = (const pf__mem *)s->ctx;
    if (size)
        *size = m->size;
    return m->data ? (const void *)m->data : (const void *)"";
}

/* The null stream. */

static pf_ssize_t pf__null_read(void *ctx, void *buf, size_t n) {
    (void)ctx, (void)buf, (void)n;
    return 0;
}

static pf_ssize_t pf__null_write(void *ctx, const void *buf, size_t n) {
    (void)ctx, (void)buf;
    return (pf_ssize_t)(n > (size_t)PTRDIFF_MAX ? (size_t)PTRDIFF_MAX : n);
}

static int64_t pf__null_seek(void *ctx, int64_t off, int whence) {
    (void)ctx, (void)off, (void)whence;
    return 0;
}

static const struct pf_stream_vt pf__null_vt = {
    pf__null_read, pf__null_write, pf__null_seek, NULL, NULL,
};

PF_API int pf_stream_null(pf_stream_t *s) {
    return pf_stream_init(s, &pf__null_vt, NULL);
}
