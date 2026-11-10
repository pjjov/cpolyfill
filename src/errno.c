/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* System errno value -> PF_E* code. The first entry for a code is its
   canonical errno value; aliases (EWOULDBLOCK, ENOTSUP, EDEADLOCK) come
   last, for systems where they differ from the canonical value. */
static const struct {
    int sys;
    int code;
} pf__errno_table[] = {
#ifdef EPERM
    { EPERM, PF_EPERM },
#endif
#ifdef ENOENT
    { ENOENT, PF_ENOENT },
#endif
#ifdef ESRCH
    { ESRCH, PF_ESRCH },
#endif
#ifdef EINTR
    { EINTR, PF_EINTR },
#endif
#ifdef EIO
    { EIO, PF_EIO },
#endif
#ifdef ENXIO
    { ENXIO, PF_ENXIO },
#endif
#ifdef E2BIG
    { E2BIG, PF_E2BIG },
#endif
#ifdef ENOEXEC
    { ENOEXEC, PF_ENOEXEC },
#endif
#ifdef EBADF
    { EBADF, PF_EBADF },
#endif
#ifdef ECHILD
    { ECHILD, PF_ECHILD },
#endif
#ifdef EAGAIN
    { EAGAIN, PF_EAGAIN },
#endif
#ifdef ENOMEM
    { ENOMEM, PF_ENOMEM },
#endif
#ifdef EACCES
    { EACCES, PF_EACCES },
#endif
#ifdef EFAULT
    { EFAULT, PF_EFAULT },
#endif
#ifdef EBUSY
    { EBUSY, PF_EBUSY },
#endif
#ifdef EEXIST
    { EEXIST, PF_EEXIST },
#endif
#ifdef EXDEV
    { EXDEV, PF_EXDEV },
#endif
#ifdef ENODEV
    { ENODEV, PF_ENODEV },
#endif
#ifdef ENOTDIR
    { ENOTDIR, PF_ENOTDIR },
#endif
#ifdef EISDIR
    { EISDIR, PF_EISDIR },
#endif
#ifdef EINVAL
    { EINVAL, PF_EINVAL },
#endif
#ifdef ENFILE
    { ENFILE, PF_ENFILE },
#endif
#ifdef EMFILE
    { EMFILE, PF_EMFILE },
#endif
#ifdef ENOTTY
    { ENOTTY, PF_ENOTTY },
#endif
#ifdef ETXTBSY
    { ETXTBSY, PF_ETXTBSY },
#endif
#ifdef EFBIG
    { EFBIG, PF_EFBIG },
#endif
#ifdef ENOSPC
    { ENOSPC, PF_ENOSPC },
#endif
#ifdef ESPIPE
    { ESPIPE, PF_ESPIPE },
#endif
#ifdef EROFS
    { EROFS, PF_EROFS },
#endif
#ifdef EMLINK
    { EMLINK, PF_EMLINK },
#endif
#ifdef EPIPE
    { EPIPE, PF_EPIPE },
#endif
#ifdef EDOM
    { EDOM, PF_EDOM },
#endif
#ifdef ERANGE
    { ERANGE, PF_ERANGE },
#endif
#ifdef EDEADLK
    { EDEADLK, PF_EDEADLK },
#endif
#ifdef ENAMETOOLONG
    { ENAMETOOLONG, PF_ENAMETOOLONG },
#endif
#ifdef ENOLCK
    { ENOLCK, PF_ENOLCK },
#endif
#ifdef ENOSYS
    { ENOSYS, PF_ENOSYS },
#endif
#ifdef ENOTEMPTY
    { ENOTEMPTY, PF_ENOTEMPTY },
#endif
#ifdef ELOOP
    { ELOOP, PF_ELOOP },
#endif
#ifdef ENOMSG
    { ENOMSG, PF_ENOMSG },
#endif
#ifdef EIDRM
    { EIDRM, PF_EIDRM },
#endif
#ifdef ENODATA
    { ENODATA, PF_ENODATA },
#endif
#ifdef ETIME
    { ETIME, PF_ETIME },
#endif
#ifdef ENOLINK
    { ENOLINK, PF_ENOLINK },
#endif
#ifdef EPROTO
    { EPROTO, PF_EPROTO },
#endif
#ifdef EBADMSG
    { EBADMSG, PF_EBADMSG },
#endif
#ifdef EOVERFLOW
    { EOVERFLOW, PF_EOVERFLOW },
#endif
#ifdef EILSEQ
    { EILSEQ, PF_EILSEQ },
#endif
#ifdef ENOTSOCK
    { ENOTSOCK, PF_ENOTSOCK },
#endif
#ifdef EDESTADDRREQ
    { EDESTADDRREQ, PF_EDESTADDRREQ },
#endif
#ifdef EMSGSIZE
    { EMSGSIZE, PF_EMSGSIZE },
#endif
#ifdef EPROTOTYPE
    { EPROTOTYPE, PF_EPROTOTYPE },
#endif
#ifdef ENOPROTOOPT
    { ENOPROTOOPT, PF_ENOPROTOOPT },
#endif
#ifdef EPROTONOSUPPORT
    { EPROTONOSUPPORT, PF_EPROTONOSUPPORT },
#endif
#ifdef ESOCKTNOSUPPORT
    { ESOCKTNOSUPPORT, PF_ESOCKTNOSUPPORT },
#endif
#ifdef EOPNOTSUPP
    { EOPNOTSUPP, PF_EOPNOTSUPP },
#endif
#ifdef EPFNOSUPPORT
    { EPFNOSUPPORT, PF_EPFNOSUPPORT },
#endif
#ifdef EAFNOSUPPORT
    { EAFNOSUPPORT, PF_EAFNOSUPPORT },
#endif
#ifdef EADDRINUSE
    { EADDRINUSE, PF_EADDRINUSE },
#endif
#ifdef EADDRNOTAVAIL
    { EADDRNOTAVAIL, PF_EADDRNOTAVAIL },
#endif
#ifdef ENETDOWN
    { ENETDOWN, PF_ENETDOWN },
#endif
#ifdef ENETUNREACH
    { ENETUNREACH, PF_ENETUNREACH },
#endif
#ifdef ENETRESET
    { ENETRESET, PF_ENETRESET },
#endif
#ifdef ECONNABORTED
    { ECONNABORTED, PF_ECONNABORTED },
#endif
#ifdef ECONNRESET
    { ECONNRESET, PF_ECONNRESET },
#endif
#ifdef ENOBUFS
    { ENOBUFS, PF_ENOBUFS },
#endif
#ifdef EISCONN
    { EISCONN, PF_EISCONN },
#endif
#ifdef ENOTCONN
    { ENOTCONN, PF_ENOTCONN },
#endif
#ifdef ESHUTDOWN
    { ESHUTDOWN, PF_ESHUTDOWN },
#endif
#ifdef ETOOMANYREFS
    { ETOOMANYREFS, PF_ETOOMANYREFS },
#endif
#ifdef ETIMEDOUT
    { ETIMEDOUT, PF_ETIMEDOUT },
#endif
#ifdef ECONNREFUSED
    { ECONNREFUSED, PF_ECONNREFUSED },
#endif
#ifdef EHOSTDOWN
    { EHOSTDOWN, PF_EHOSTDOWN },
#endif
#ifdef EHOSTUNREACH
    { EHOSTUNREACH, PF_EHOSTUNREACH },
#endif
#ifdef EALREADY
    { EALREADY, PF_EALREADY },
#endif
#ifdef EINPROGRESS
    { EINPROGRESS, PF_EINPROGRESS },
#endif
#ifdef ESTALE
    { ESTALE, PF_ESTALE },
#endif
#ifdef EDQUOT
    { EDQUOT, PF_EDQUOT },
#endif
#ifdef ECANCELED
    { ECANCELED, PF_ECANCELED },
#endif
#ifdef EOWNERDEAD
    { EOWNERDEAD, PF_EOWNERDEAD },
#endif
#ifdef ENOTRECOVERABLE
    { ENOTRECOVERABLE, PF_ENOTRECOVERABLE },
#endif
#ifdef EWOULDBLOCK
    { EWOULDBLOCK, PF_EAGAIN },
#endif
#ifdef ENOTSUP
    { ENOTSUP, PF_EOPNOTSUPP },
#endif
#ifdef EDEADLOCK
    { EDEADLOCK, PF_EDEADLK },
#endif
};

#define PF__ERRNO_COUNT (sizeof(pf__errno_table) / sizeof(pf__errno_table[0]))

PF_API int pf_error_from_errno(int err) {
    size_t i;

    if (err == 0)
        return PF_OK;
    for (i = 0; i < PF__ERRNO_COUNT; i++) {
        if (pf__errno_table[i].sys == err)
            return pf__errno_table[i].code;
    }
    return PF_EUNKNOWN;
}

PF_API int pf_error_to_errno(int code) {
    size_t i;

    if (code == PF_OK)
        return 0;
    for (i = 0; i < PF__ERRNO_COUNT; i++) {
        if (pf__errno_table[i].code == code)
            return pf__errno_table[i].sys;
    }
    return EINVAL;
}

/* ---- Descriptions ----------------------------------------------------- */

static const struct {
    int code;
    const char *name;
    const char *desc;
} pf__error_info[] = {
#define PF__ERROR_INFO(name, value, desc) { -(value), #name, desc },
    PF__ERROR_LIST(PF__ERROR_INFO)
#undef PF__ERROR_INFO
};

#define PF__INFO_COUNT (sizeof(pf__error_info) / sizeof(pf__error_info[0]))

static size_t pf__error_index(int code) {
    size_t i;
    for (i = 0; i < PF__INFO_COUNT; i++) {
        if (pf__error_info[i].code == code)
            return i;
    }
    return PF__INFO_COUNT;
}

PF_API const char *pf_strerror(int code) {
    size_t i;

    if (code == PF_OK)
        return "Success";
    i = pf__error_index(code);
    return i < PF__INFO_COUNT ? pf__error_info[i].desc : "Unknown error";
}

PF_API const char *pf_error_name(int code) {
    size_t i;

    if (code == PF_OK)
        return "OK";
    i = pf__error_index(code);
    return i < PF__INFO_COUNT ? pf__error_info[i].name : "EUNKNOWN";
}

PF_API int pf_strerror_r(int errnum, char *buf, size_t size) {
    char tmp[256];
    const char *msg;
    size_t len;

    if (!buf || size == 0)
        return EINVAL;

#if PF_OS_WINDOWS
    if (strerror_s(tmp, sizeof(tmp), errnum) != 0)
        snprintf(tmp, sizeof(tmp), "Unknown error %d", errnum);
    msg = tmp;
#elif PF__STRERROR_R_GNU
    msg = strerror_r(errnum, tmp, sizeof(tmp));
#else
    if (strerror_r(errnum, tmp, sizeof(tmp)) != 0)
        snprintf(tmp, sizeof(tmp), "Unknown error %d", errnum);
    msg = tmp;
#endif

    len = strlen(msg);
    if (len >= size) {
        memcpy(buf, msg, size - 1);
        buf[size - 1] = '\0';
        return ERANGE;
    }
    memcpy(buf, msg, len + 1);
    return 0;
}
