/** cpolyfill - Filling the gaps between C standards and compilers

    Portable error codes.

    Functions of this library return `int`: `PF_OK` (0) on success, or a
    negative `PF_E*` code on failure. Functions that stand in for a standard
    function (opt-in standard names) follow that function's convention
    instead, usually `-1` and `errno`.

    The codes have the same values on every platform. They are the negated
    Linux errno values, so on Linux `pf_error_from_errno(e) == -e` for every
    code in the list. Codes the system reports but that have no
    counterpart here become `PF_EUNKNOWN`.

        int pf_error_from_errno(int err);   errno value -> PF_E* code
        int pf_error_to_errno(int code);    PF_E* code -> errno value, or 0
                                            for PF_OK, or EINVAL if the
                                            system has no such errno value

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

#ifndef CPOLYFILL_ERRNO_H
#define CPOLYFILL_ERRNO_H

#include <cpolyfill/platform.h>

PF_BEGIN_DECLS

/* X(name, value, description) */
#define PF__ERROR_LIST(X)                                                   \
    X(EPERM, 1, "Operation not permitted")                                  \
    X(ENOENT, 2, "No such file or directory")                               \
    X(ESRCH, 3, "No such process")                                          \
    X(EINTR, 4, "Interrupted system call")                                  \
    X(EIO, 5, "Input/output error")                                         \
    X(ENXIO, 6, "No such device or address")                                \
    X(E2BIG, 7, "Argument list too long")                                   \
    X(ENOEXEC, 8, "Exec format error")                                      \
    X(EBADF, 9, "Bad file descriptor")                                      \
    X(ECHILD, 10, "No child processes")                                     \
    X(EAGAIN, 11, "Resource temporarily unavailable")                       \
    X(ENOMEM, 12, "Cannot allocate memory")                                 \
    X(EACCES, 13, "Permission denied")                                      \
    X(EFAULT, 14, "Bad address")                                            \
    X(EBUSY, 16, "Device or resource busy")                                 \
    X(EEXIST, 17, "File exists")                                            \
    X(EXDEV, 18, "Invalid cross-device link")                               \
    X(ENODEV, 19, "No such device")                                         \
    X(ENOTDIR, 20, "Not a directory")                                       \
    X(EISDIR, 21, "Is a directory")                                         \
    X(EINVAL, 22, "Invalid argument")                                       \
    X(ENFILE, 23, "Too many open files in system")                          \
    X(EMFILE, 24, "Too many open files")                                    \
    X(ENOTTY, 25, "Inappropriate ioctl for device")                         \
    X(ETXTBSY, 26, "Text file busy")                                        \
    X(EFBIG, 27, "File too large")                                          \
    X(ENOSPC, 28, "No space left on device")                                \
    X(ESPIPE, 29, "Illegal seek")                                           \
    X(EROFS, 30, "Read-only file system")                                   \
    X(EMLINK, 31, "Too many links")                                         \
    X(EPIPE, 32, "Broken pipe")                                             \
    X(EDOM, 33, "Numerical argument out of domain")                         \
    X(ERANGE, 34, "Numerical result out of range")                          \
    X(EDEADLK, 35, "Resource deadlock avoided")                             \
    X(ENAMETOOLONG, 36, "File name too long")                               \
    X(ENOLCK, 37, "No locks available")                                     \
    X(ENOSYS, 38, "Function not implemented")                               \
    X(ENOTEMPTY, 39, "Directory not empty")                                 \
    X(ELOOP, 40, "Too many levels of symbolic links")                       \
    X(ENOMSG, 42, "No message of desired type")                             \
    X(EIDRM, 43, "Identifier removed")                                      \
    X(ENODATA, 61, "No data available")                                     \
    X(ETIME, 62, "Timer expired")                                           \
    X(ENOLINK, 67, "Link has been severed")                                 \
    X(EPROTO, 71, "Protocol error")                                         \
    X(EBADMSG, 74, "Bad message")                                           \
    X(EOVERFLOW, 75, "Value too large for defined data type")               \
    X(EILSEQ, 84, "Invalid or incomplete multibyte or wide character")      \
    X(ENOTSOCK, 88, "Socket operation on non-socket")                       \
    X(EDESTADDRREQ, 89, "Destination address required")                     \
    X(EMSGSIZE, 90, "Message too long")                                     \
    X(EPROTOTYPE, 91, "Protocol wrong type for socket")                     \
    X(ENOPROTOOPT, 92, "Protocol not available")                            \
    X(EPROTONOSUPPORT, 93, "Protocol not supported")                        \
    X(ESOCKTNOSUPPORT, 94, "Socket type not supported")                     \
    X(EOPNOTSUPP, 95, "Operation not supported")                            \
    X(EPFNOSUPPORT, 96, "Protocol family not supported")                    \
    X(EAFNOSUPPORT, 97, "Address family not supported by protocol")         \
    X(EADDRINUSE, 98, "Address already in use")                             \
    X(EADDRNOTAVAIL, 99, "Cannot assign requested address")                 \
    X(ENETDOWN, 100, "Network is down")                                     \
    X(ENETUNREACH, 101, "Network is unreachable")                           \
    X(ENETRESET, 102, "Network dropped connection on reset")                \
    X(ECONNABORTED, 103, "Software caused connection abort")                \
    X(ECONNRESET, 104, "Connection reset by peer")                          \
    X(ENOBUFS, 105, "No buffer space available")                            \
    X(EISCONN, 106, "Transport endpoint is already connected")              \
    X(ENOTCONN, 107, "Transport endpoint is not connected")                 \
    X(ESHUTDOWN, 108, "Cannot send after transport endpoint shutdown")      \
    X(ETOOMANYREFS, 109, "Too many references")                             \
    X(ETIMEDOUT, 110, "Connection timed out")                               \
    X(ECONNREFUSED, 111, "Connection refused")                              \
    X(EHOSTDOWN, 112, "Host is down")                                       \
    X(EHOSTUNREACH, 113, "No route to host")                                \
    X(EALREADY, 114, "Operation already in progress")                       \
    X(EINPROGRESS, 115, "Operation now in progress")                        \
    X(ESTALE, 116, "Stale file handle")                                     \
    X(EDQUOT, 122, "Disk quota exceeded")                                   \
    X(ECANCELED, 125, "Operation canceled")                                 \
    X(EOWNERDEAD, 130, "Owner died")                                        \
    X(ENOTRECOVERABLE, 131, "State not recoverable")                        \
    X(EUNKNOWN, 4095, "Unknown error")

enum pf_error {
    PF_OK = 0,
#define PF__ERROR_ENUM(name, value, desc) PF_##name = -(value),
    PF__ERROR_LIST(PF__ERROR_ENUM)
#undef PF__ERROR_ENUM
    PF_EWOULDBLOCK = PF_EAGAIN,
    PF_ENOTSUP = PF_EOPNOTSUPP,
    PF_EDEADLOCK = PF_EDEADLK
};

PF_API int pf_error_from_errno(int err);
PF_API int pf_error_to_errno(int code);

PF_END_DECLS

#endif /* CPOLYFILL_ERRNO_H */
