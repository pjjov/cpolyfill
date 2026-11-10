/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "../internal.h"

#include <winsock2.h>
#include <windows.h>

PF_API int pf_error_from_win32(unsigned long err) {
    switch (err) {
    case ERROR_SUCCESS:
        return PF_OK;

    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_MOD_NOT_FOUND:
    case ERROR_PROC_NOT_FOUND:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
        return PF_ENOENT;

    case ERROR_ACCESS_DENIED:
    case ERROR_ELEVATION_REQUIRED:
    case ERROR_CANT_ACCESS_FILE:
        return PF_EACCES;
    case ERROR_PRIVILEGE_NOT_HELD:
        return PF_EPERM;
    case ERROR_WRITE_PROTECT:
        return PF_EROFS;

    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return PF_EEXIST;
    case ERROR_DIR_NOT_EMPTY:
        return PF_ENOTEMPTY;
    case ERROR_DIRECTORY:
        return PF_ENOTDIR;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BUFFER_OVERFLOW:
        return PF_ENAMETOOLONG;
    case ERROR_CANT_RESOLVE_FILENAME:
        return PF_ELOOP;
    case ERROR_NOT_SAME_DEVICE:
        return PF_EXDEV;
    case ERROR_TOO_MANY_LINKS:
        return PF_EMLINK;
    case ERROR_FILE_TOO_LARGE:
        return PF_EFBIG;
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
        return PF_ENOSPC;
    case ERROR_TOO_MANY_OPEN_FILES:
        return PF_EMFILE;

    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_COMMITMENT_LIMIT:
        return PF_ENOMEM;
    case ERROR_NOACCESS:
        return PF_EFAULT;
    case ERROR_INVALID_HANDLE:
        return PF_EBADF;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_INVALID_FLAGS:
    case ERROR_NEGATIVE_SEEK:
        return PF_EINVAL;
    case ERROR_INSUFFICIENT_BUFFER:
        return PF_ERANGE;
    case ERROR_NO_UNICODE_TRANSLATION:
        return PF_EILSEQ;

    case ERROR_BUSY:
    case ERROR_PIPE_BUSY:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return PF_EBUSY;
    case ERROR_BROKEN_PIPE:
    case ERROR_NO_DATA:
    case ERROR_PIPE_NOT_CONNECTED:
        return PF_EPIPE;
    case ERROR_POSSIBLE_DEADLOCK:
        return PF_EDEADLK;
    case ERROR_TIMEOUT:
    case ERROR_SEM_TIMEOUT:
    case WAIT_TIMEOUT:
        return PF_ETIMEDOUT;
    case ERROR_OPERATION_ABORTED:
    case ERROR_CANCELLED:
        return PF_ECANCELED;

    case ERROR_NOT_SUPPORTED:
        return PF_ENOTSUP;
    case ERROR_CALL_NOT_IMPLEMENTED:
    case ERROR_INVALID_FUNCTION:
        return PF_ENOSYS;
    case ERROR_BAD_EXE_FORMAT:
        return PF_ENOEXEC;

    case ERROR_SEEK:
    case ERROR_CRC:
    case ERROR_GEN_FAILURE:
    case ERROR_IO_DEVICE:
        return PF_EIO;

    default:
        return PF_EUNKNOWN;
    }
}

PF_API int pf_error_from_wsa(int err) {
    switch (err) {
    case 0:
        return PF_OK;
    case WSAEINTR:
        return PF_EINTR;
    case WSAEBADF:
    case WSA_INVALID_HANDLE:
        return PF_EBADF;
    case WSAEACCES:
        return PF_EACCES;
    case WSAEFAULT:
        return PF_EFAULT;
    case WSAEINVAL:
    case WSA_INVALID_PARAMETER:
        return PF_EINVAL;
    case WSAEMFILE:
        return PF_EMFILE;
    case WSAEWOULDBLOCK:
    case WSAEPROCLIM:
        return PF_EAGAIN;
    case WSAEINPROGRESS:
        return PF_EINPROGRESS;
    case WSAEALREADY:
        return PF_EALREADY;
    case WSAENOTSOCK:
        return PF_ENOTSOCK;
    case WSAEDESTADDRREQ:
        return PF_EDESTADDRREQ;
    case WSAEMSGSIZE:
        return PF_EMSGSIZE;
    case WSAEPROTOTYPE:
        return PF_EPROTOTYPE;
    case WSAENOPROTOOPT:
        return PF_ENOPROTOOPT;
    case WSAEPROTONOSUPPORT:
        return PF_EPROTONOSUPPORT;
    case WSAESOCKTNOSUPPORT:
        return PF_ESOCKTNOSUPPORT;
    case WSAEOPNOTSUPP:
        return PF_EOPNOTSUPP;
    case WSAEPFNOSUPPORT:
        return PF_EPFNOSUPPORT;
    case WSAEAFNOSUPPORT:
        return PF_EAFNOSUPPORT;
    case WSAEADDRINUSE:
        return PF_EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return PF_EADDRNOTAVAIL;
    case WSAENETDOWN:
        return PF_ENETDOWN;
    case WSAENETUNREACH:
        return PF_ENETUNREACH;
    case WSAENETRESET:
        return PF_ENETRESET;
    case WSAECONNABORTED:
        return PF_ECONNABORTED;
    case WSAECONNRESET:
        return PF_ECONNRESET;
    case WSAENOBUFS:
        return PF_ENOBUFS;
    case WSAEISCONN:
        return PF_EISCONN;
    case WSAENOTCONN:
        return PF_ENOTCONN;
    case WSAESHUTDOWN:
        return PF_ESHUTDOWN;
    case WSAETOOMANYREFS:
        return PF_ETOOMANYREFS;
    case WSAETIMEDOUT:
        return PF_ETIMEDOUT;
    case WSAECONNREFUSED:
        return PF_ECONNREFUSED;
    case WSAELOOP:
        return PF_ELOOP;
    case WSAENAMETOOLONG:
        return PF_ENAMETOOLONG;
    case WSAEHOSTDOWN:
        return PF_EHOSTDOWN;
    case WSAEHOSTUNREACH:
        return PF_EHOSTUNREACH;
    case WSAENOTEMPTY:
        return PF_ENOTEMPTY;
    case WSAEDQUOT:
        return PF_EDQUOT;
    case WSAESTALE:
        return PF_ESTALE;
    case WSA_NOT_ENOUGH_MEMORY:
        return PF_ENOMEM;
    case WSA_OPERATION_ABORTED:
    case WSAECANCELLED:
        return PF_ECANCELED;
    default:
        return PF_EUNKNOWN;
    }
}

PF_API int pf_error_last_win32(void) { return pf_error_from_win32(GetLastError()); }
