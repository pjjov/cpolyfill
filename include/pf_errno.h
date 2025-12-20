/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides enumeration constants for POSIX error codes.
    While actual numeric values aren't standardized, most systems
    define values identical to the ones below.

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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

#ifndef POLYFILL_ERRNO
#define POLYFILL_ERRNO

typedef int pf_errno_t;

enum pf_errno {
    PF_EPERM = 1, /* Operation not permitted */
    PF_ENOENT = 2, /* No such file or directory */
    PF_ESRCH = 3, /* No such process */
    PF_EINTR = 4, /* Interrupted system call */
    PF_EIO = 5, /* Input/output error */
    PF_ENXIO = 6, /* No such device or address */
    PF_E2BIG = 7, /* Argument list too long */
    PF_ENOEXEC = 8, /* Exec format error */
    PF_EBADF = 9, /* Bad file descriptor */
    PF_ECHILD = 10, /* No child processes */
    PF_EAGAIN = 11, /* Resource temporarily unavailable */
    PF_ENOMEM = 12, /* Cannot allocate memory */
    PF_EACCES = 13, /* Permission denied */
    PF_EFAULT = 14, /* Bad address */
    PF_ENOTBLK = 15, /* Block device required */
    PF_EBUSY = 16, /* Device or resource busy */
    PF_EEXIST = 17, /* File exists */
    PF_EXDEV = 18, /* Invalid cross-device link */
    PF_ENODEV = 19, /* No such device */
    PF_ENOTDIR = 20, /* Not a directory */
    PF_EISDIR = 21, /* Is a directory */
    PF_EINVAL = 22, /* Invalid argument */
    PF_ENFILE = 23, /* Too many open files in system */
    PF_EMFILE = 24, /* Too many open files */
    PF_ENOTTY = 25, /* Inappropriate ioctl for device */
    PF_ETXTBSY = 26, /* Text file busy */
    PF_EFBIG = 27, /* File too large */
    PF_ENOSPC = 28, /* No space left on device */
    PF_ESPIPE = 29, /* Illegal seek */
    PF_EROFS = 30, /* Read-only file system */
    PF_EMLINK = 31, /* Too many links */
    PF_EPIPE = 32, /* Broken pipe */
    PF_EDOM = 33, /* Numerical argument out of domain */
    PF_ERANGE = 34, /* Numerical result out of range */
    PF_EDEADLK = 35, /* Resource deadlock avoided */
    PF_ENAMETOOLONG = 36, /* File name too long */
    PF_ENOLCK = 37, /* No locks available */
    PF_ENOSYS = 38, /* Function not implemented */
    PF_ENOTEMPTY = 39, /* Directory not empty */
    PF_ELOOP = 40, /* Too many levels of symbolic links */
    PF_ENOMSG = 42, /* No message of desired type */
    PF_EIDRM = 43, /* Identifier removed */
    PF_ECHRNG = 44, /* Channel number out of range */
    PF_EL2NSYNC = 45, /* Level 2 not synchronized */
    PF_EL3HLT = 46, /* Level 3 halted */
    PF_EL3RST = 47, /* Level 3 reset */
    PF_ELNRNG = 48, /* Link number out of range */
    PF_EUNATCH = 49, /* Protocol driver not attached */
    PF_ENOCSI = 50, /* No CSI structure available */
    PF_EL2HLT = 51, /* Level 2 halted */
    PF_EBADE = 52, /* Invalid exchange */
    PF_EBADR = 53, /* Invalid request descriptor */
    PF_EXFULL = 54, /* Exchange full */
    PF_ENOANO = 55, /* No anode */
    PF_EBADRQC = 56, /* Invalid request code */
    PF_EBADSLT = 57, /* Invalid slot */
    PF_EBFONT = 59, /* Bad font file format */
    PF_ENOSTR = 60, /* Device not a stream */
    PF_ENODATA = 61, /* No data available */
    PF_ETIME = 62, /* Timer expired */
    PF_ENOSR = 63, /* Out of streams resources */
    PF_ENONET = 64, /* Machine is not on the network */
    PF_ENOPKG = 65, /* Package not installed */
    PF_EREMOTE = 66, /* Object is remote */
    PF_ENOLINK = 67, /* Link has been severed */
    PF_EADV = 68, /* Advertise error */
    PF_ESRMNT = 69, /* Srmount error */
    PF_ECOMM = 70, /* Communication error on send */
    PF_EPROTO = 71, /* Protocol error */
    PF_EMULTIHOP = 72, /* Multihop attempted */
    PF_EDOTDOT = 73, /* RFS specific error */
    PF_EBADMSG = 74, /* Bad message */
    PF_EOVERFLOW = 75, /* Value too large for defined data type */
    PF_ENOTUNIQ = 76, /* Name not unique on network */
    PF_EBADFD = 77, /* File descriptor in bad state */
    PF_EREMCHG = 78, /* Remote address changed */
    PF_ELIBACC = 79, /* Can not access a needed shared library */
    PF_ELIBBAD = 80, /* Accessing a corrupted shared library */
    PF_ELIBSCN = 81, /* .lib section in a.out corrupted */
    PF_ELIBMAX = 82, /* Attempting to link in too many shared libraries */
    PF_ELIBEXEC = 83, /* Cannot exec a shared library directly */
    PF_EILSEQ = 84, /* Invalid or incomplete multibyte or wide character */
    PF_ERESTART = 85, /* Interrupted system call should be restarted */
    PF_ESTRPIPE = 86, /* Streams pipe error */
    PF_EUSERS = 87, /* Too many users */
    PF_ENOTSOCK = 88, /* Socket operation on non-socket */
    PF_EDESTADDRREQ = 89, /* Destination address required */
    PF_EMSGSIZE = 90, /* Message too long */
    PF_EPROTOTYPE = 91, /* Protocol wrong type for socket */
    PF_ENOPROTOOPT = 92, /* Protocol not available */
    PF_EPROTONOSUPPORT = 93, /* Protocol not supported */
    PF_ESOCKTNOSUPPORT = 94, /* Socket type not supported */
    PF_EOPNOTSUPP = 95, /* Operation not supported */
    PF_EPFNOSUPPORT = 96, /* Protocol family not supported */
    PF_EAFNOSUPPORT = 97, /* Address family not supported by protocol */
    PF_EADDRINUSE = 98, /* Address already in use */
    PF_EADDRNOTAVAIL = 99, /* Cannot assign requested address */
    PF_ENETDOWN = 100, /* Network is down */
    PF_ENETUNREACH = 101, /* Network is unreachable */
    PF_ENETRESET = 102, /* Network dropped connection on reset */
    PF_ECONNABORTED = 103, /* Software caused connection abort */
    PF_ECONNRESET = 104, /* Connection reset by peer */
    PF_ENOBUFS = 105, /* No buffer space available */
    PF_EISCONN = 106, /* Transport endpoint is already connected */
    PF_ENOTCONN = 107, /* Transport endpoint is not connected */
    PF_ESHUTDOWN = 108, /* Cannot send after transport endpoint shutdown */
    PF_ETOOMANYREFS = 109, /* Too many references: cannot splice */
    PF_ETIMEDOUT = 110, /* Connection timed out */
    PF_ECONNREFUSED = 111, /* Connection refused */
    PF_EHOSTDOWN = 112, /* Host is down */
    PF_EHOSTUNREACH = 113, /* No route to host */
    PF_EALREADY = 114, /* Operation already in progress */
    PF_EINPROGRESS = 115, /* Operation now in progress */
    PF_ESTALE = 116, /* Stale file handle */
    PF_EUCLEAN = 117, /* Structure needs cleaning */
    PF_ENOTNAM = 118, /* Not a Xenix named type file */
    PF_ENAVAIL = 119, /* No Xenix semaphores available */
    PF_EISNAM = 120, /* Is a named type file */
    PF_EREMOTEIO = 121, /* Remote I/O error */
    PF_EDQUOT = 122, /* Disk quota exceeded */
    PF_ENOMEDIUM = 123, /* No medium found */
    PF_EMEDIUMTYPE = 124, /* Wrong medium type */
    PF_ECANCELED = 125, /* Operation canceled */
    PF_ENOKEY = 126, /* Required key not available */
    PF_EKEYEXPIRED = 127, /* Key has expired */
    PF_EKEYREVOKED = 128, /* Key has been revoked */
    PF_EKEYREJECTED = 129, /* Key was rejected by service */
    PF_EOWNERDEAD = 130, /* Owner died */
    PF_ENOTRECOVERABLE = 131, /* State not recoverable */
    PF_ERFKILL = 132, /* Operation not possible due to RF-kill */
    PF_EHWPOISON = 133, /* Memory page has hardware error */
    PF_ENOTSUP = 134, /* Not supported parameter or option */
};

#endif
