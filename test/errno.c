/*
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
*/

#include "../include/pf_errno.h"
#include "check.h"
#include <errno.h>

/* clang-format off */
#define ERRNO_LIST(X) \
    X(EPERM) \
    X(ENOENT) \
    X(ESRCH) \
    X(EINTR) \
    X(EIO) \
    X(ENXIO) \
    X(E2BIG) \
    X(ENOEXEC) \
    X(EBADF) \
    X(ECHILD) \
    X(EAGAIN) \
    X(ENOMEM) \
    X(EACCES) \
    X(EFAULT) \
    X(ENOTBLK) \
    X(EBUSY) \
    X(EEXIST) \
    X(EXDEV) \
    X(ENODEV) \
    X(ENOTDIR) \
    X(EISDIR) \
    X(EINVAL) \
    X(ENFILE) \
    X(EMFILE) \
    X(ENOTTY) \
    X(ETXTBSY) \
    X(EFBIG) \
    X(ENOSPC) \
    X(ESPIPE) \
    X(EROFS) \
    X(EMLINK) \
    X(EPIPE) \
    X(EDOM) \
    X(ERANGE) \
    X(EDEADLK) \
    X(ENAMETOOLONG) \
    X(ENOLCK) \
    X(ENOSYS) \
    X(ENOTEMPTY) \
    X(ELOOP) \
    X(ENOMSG) \
    X(EIDRM) \
    X(ECHRNG) \
    X(EL2NSYNC) \
    X(EL3HLT) \
    X(EL3RST) \
    X(ELNRNG) \
    X(EUNATCH) \
    X(ENOCSI) \
    X(EL2HLT) \
    X(EBADE) \
    X(EBADR) \
    X(EXFULL) \
    X(ENOANO) \
    X(EBADRQC) \
    X(EBADSLT) \
    X(EBFONT) \
    X(ENOSTR) \
    X(ENODATA) \
    X(ETIME) \
    X(ENOSR) \
    X(ENONET) \
    X(ENOPKG) \
    X(EREMOTE) \
    X(ENOLINK) \
    X(EADV) \
    X(ESRMNT) \
    X(ECOMM) \
    X(EPROTO) \
    X(EMULTIHOP) \
    X(EDOTDOT) \
    X(EBADMSG) \
    X(EOVERFLOW) \
    X(ENOTUNIQ) \
    X(EBADFD) \
    X(EREMCHG) \
    X(ELIBACC) \
    X(ELIBBAD) \
    X(ELIBSCN) \
    X(ELIBMAX) \
    X(ELIBEXEC) \
    X(EILSEQ) \
    X(ERESTART) \
    X(ESTRPIPE) \
    X(EUSERS) \
    X(ENOTSOCK) \
    X(EDESTADDRREQ) \
    X(EMSGSIZE) \
    X(EPROTOTYPE) \
    X(ENOPROTOOPT) \
    X(EPROTONOSUPPORT) \
    X(ESOCKTNOSUPPORT) \
    X(EOPNOTSUPP) \
    X(EPFNOSUPPORT) \
    X(EAFNOSUPPORT) \
    X(EADDRINUSE) \
    X(EADDRNOTAVAIL) \
    X(ENETDOWN) \
    X(ENETUNREACH) \
    X(ENETRESET) \
    X(ECONNABORTED) \
    X(ECONNRESET) \
    X(ENOBUFS) \
    X(EISCONN) \
    X(ENOTCONN) \
    X(ESHUTDOWN) \
    X(ETOOMANYREFS) \
    X(ETIMEDOUT) \
    X(ECONNREFUSED) \
    X(EHOSTDOWN) \
    X(EHOSTUNREACH) \
    X(EALREADY) \
    X(EINPROGRESS) \
    X(ESTALE) \
    X(EUCLEAN) \
    X(ENOTNAM) \
    X(ENAVAIL) \
    X(EISNAM) \
    X(EREMOTEIO) \
    X(EDQUOT) \
    X(ENOMEDIUM) \
    X(EMEDIUMTYPE) \
    X(ECANCELED) \
    X(ENOKEY) \
    X(EKEYEXPIRED) \
    X(EKEYREVOKED) \
    X(EKEYREJECTED) \
    X(EOWNERDEAD) \
    X(ENOTRECOVERABLE) \
    X(ERFKILL) \
    X(EHWPOISON) \
    X(ENOTSUP) \
    /* end of list */
/* clang-format on */

static int test_errno_positive_unique(int seed, int rep) {
    static const int values[] = {
#define X(name) PF_##name,
        ERRNO_LIST(X)
#undef X
    };
    size_t i, j, n = sizeof values / sizeof values[0];

    CHECK(n > 100);
    for (i = 0; i < n; i++) {
        CHECK(values[i] > 0);
        for (j = i + 1; j < n; j++)
            /* ENOTSUP is the same error as EOPNOTSUPP. */
            CHECK(values[i] != values[j] || values[i] == PF_EOPNOTSUPP);
    }
    return 0;
}

/* The values documented to match Linux, which the header is modeled on. */
static int test_errno_known_values(int seed, int rep) {
    CHECK(PF_EPERM == 1);
    CHECK(PF_ENOENT == 2);
    CHECK(PF_EINTR == 4);
    CHECK(PF_EIO == 5);
    CHECK(PF_EBADF == 9);
    CHECK(PF_EAGAIN == 11);
    CHECK(PF_ENOMEM == 12);
    CHECK(PF_EACCES == 13);
    CHECK(PF_EEXIST == 17);
    CHECK(PF_ENOTDIR == 20);
    CHECK(PF_EISDIR == 21);
    CHECK(PF_EINVAL == 22);
    CHECK(PF_EPIPE == 32);
    CHECK(PF_EDOM == 33);
    CHECK(PF_ERANGE == 34);
    CHECK(PF_ENAMETOOLONG == 36);
    CHECK(PF_ENOSYS == 38);
    CHECK(PF_ENOTEMPTY == 39);
    CHECK(PF_EILSEQ == 84);
    CHECK(PF_EADDRINUSE == 98);
    CHECK(PF_ETIMEDOUT == 110);
    CHECK(PF_ECONNREFUSED == 111);
    CHECK(PF_ENOTSUP == 95);
    return 0;
}

/* The error codes must be usable in integer contexts and switches. */
static int test_errno_usable(int seed, int rep) {
    pf_errno_t e = PF_ENOENT;
    int hit = 0;

    switch (e) {
    case PF_EPERM:
        break;
    case PF_ENOENT:
        hit = 1;
        break;
    default:
        break;
    }
    CHECK(hit);
    CHECK(sizeof(pf_errno_t) == sizeof(int));
    return 0;
}

/* Compares with the platform. Codes are only guaranteed to match on Linux,
   elsewhere the differences are reported (PF_TEST_VERBOSE) but not fatal. */
static int test_errno_native(int seed, int rep) {
    int mismatches = 0;
    const char *report = getenv("PF_TEST_VERBOSE");

#define X(name)                                     \
    if (PF_##name != name) {                        \
        mismatches++;                               \
        if (report)                                 \
            fprintf(                                \
                stderr,                             \
                "# errno mismatch: %s: %d vs %d\n", \
                #name,                              \
                PF_##name,                          \
                name                                \
            );                                      \
    }
#define Y(name) X(name)
    /* Only names the platform defines. */
#ifdef EPERM
    X(EPERM)
#endif
#ifdef ENOENT
    X(ENOENT)
#endif
#ifdef ESRCH
    X(ESRCH)
#endif
#ifdef EINTR
    X(EINTR)
#endif
#ifdef EIO
    X(EIO)
#endif
#ifdef ENXIO
    X(ENXIO)
#endif
#ifdef E2BIG
    X(E2BIG)
#endif
#ifdef ENOEXEC
    X(ENOEXEC)
#endif
#ifdef EBADF
    X(EBADF)
#endif
#ifdef ECHILD
    X(ECHILD)
#endif
#ifdef EAGAIN
    X(EAGAIN)
#endif
#ifdef ENOMEM
    X(ENOMEM)
#endif
#ifdef EACCES
    X(EACCES)
#endif
#ifdef EFAULT
    X(EFAULT)
#endif
#ifdef ENOTBLK
    X(ENOTBLK)
#endif
#ifdef EBUSY
    X(EBUSY)
#endif
#ifdef EEXIST
    X(EEXIST)
#endif
#ifdef EXDEV
    X(EXDEV)
#endif
#ifdef ENODEV
    X(ENODEV)
#endif
#ifdef ENOTDIR
    X(ENOTDIR)
#endif
#ifdef EISDIR
    X(EISDIR)
#endif
#ifdef EINVAL
    X(EINVAL)
#endif
#ifdef ENFILE
    X(ENFILE)
#endif
#ifdef EMFILE
    X(EMFILE)
#endif
#ifdef ENOTTY
    X(ENOTTY)
#endif
#ifdef ETXTBSY
    X(ETXTBSY)
#endif
#ifdef EFBIG
    X(EFBIG)
#endif
#ifdef ENOSPC
    X(ENOSPC)
#endif
#ifdef ESPIPE
    X(ESPIPE)
#endif
#ifdef EROFS
    X(EROFS)
#endif
#ifdef EMLINK
    X(EMLINK)
#endif
#ifdef EPIPE
    X(EPIPE)
#endif
#ifdef EDOM
    X(EDOM)
#endif
#ifdef ERANGE
    X(ERANGE)
#endif
#ifdef EDEADLK
    X(EDEADLK)
#endif
#ifdef ENAMETOOLONG
    X(ENAMETOOLONG)
#endif
#ifdef ENOLCK
    X(ENOLCK)
#endif
#ifdef ENOSYS
    X(ENOSYS)
#endif
#ifdef ENOTEMPTY
    X(ENOTEMPTY)
#endif
#ifdef ELOOP
    X(ELOOP)
#endif
#ifdef ENOMSG
    X(ENOMSG)
#endif
#ifdef EIDRM
    X(EIDRM)
#endif
#ifdef ECHRNG
    X(ECHRNG)
#endif
#ifdef EL2NSYNC
    X(EL2NSYNC)
#endif
#ifdef EL3HLT
    X(EL3HLT)
#endif
#ifdef EL3RST
    X(EL3RST)
#endif
#ifdef ELNRNG
    X(ELNRNG)
#endif
#ifdef EUNATCH
    X(EUNATCH)
#endif
#ifdef ENOCSI
    X(ENOCSI)
#endif
#ifdef EL2HLT
    X(EL2HLT)
#endif
#ifdef EBADE
    X(EBADE)
#endif
#ifdef EBADR
    X(EBADR)
#endif
#ifdef EXFULL
    X(EXFULL)
#endif
#ifdef ENOANO
    X(ENOANO)
#endif
#ifdef EBADRQC
    X(EBADRQC)
#endif
#ifdef EBADSLT
    X(EBADSLT)
#endif
#ifdef EBFONT
    X(EBFONT)
#endif
#ifdef ENOSTR
    X(ENOSTR)
#endif
#ifdef ENODATA
    X(ENODATA)
#endif
#ifdef ETIME
    X(ETIME)
#endif
#ifdef ENOSR
    X(ENOSR)
#endif
#ifdef ENONET
    X(ENONET)
#endif
#ifdef ENOPKG
    X(ENOPKG)
#endif
#ifdef EREMOTE
    X(EREMOTE)
#endif
#ifdef ENOLINK
    X(ENOLINK)
#endif
#ifdef EADV
    X(EADV)
#endif
#ifdef ESRMNT
    X(ESRMNT)
#endif
#ifdef ECOMM
    X(ECOMM)
#endif
#ifdef EPROTO
    X(EPROTO)
#endif
#ifdef EMULTIHOP
    X(EMULTIHOP)
#endif
#ifdef EDOTDOT
    X(EDOTDOT)
#endif
#ifdef EBADMSG
    X(EBADMSG)
#endif
#ifdef EOVERFLOW
    X(EOVERFLOW)
#endif
#ifdef ENOTUNIQ
    X(ENOTUNIQ)
#endif
#ifdef EBADFD
    X(EBADFD)
#endif
#ifdef EREMCHG
    X(EREMCHG)
#endif
#ifdef ELIBACC
    X(ELIBACC)
#endif
#ifdef ELIBBAD
    X(ELIBBAD)
#endif
#ifdef ELIBSCN
    X(ELIBSCN)
#endif
#ifdef ELIBMAX
    X(ELIBMAX)
#endif
#ifdef ELIBEXEC
    X(ELIBEXEC)
#endif
#ifdef EILSEQ
    X(EILSEQ)
#endif
#ifdef ERESTART
    X(ERESTART)
#endif
#ifdef ESTRPIPE
    X(ESTRPIPE)
#endif
#ifdef EUSERS
    X(EUSERS)
#endif
#ifdef ENOTSOCK
    X(ENOTSOCK)
#endif
#ifdef EDESTADDRREQ
    X(EDESTADDRREQ)
#endif
#ifdef EMSGSIZE
    X(EMSGSIZE)
#endif
#ifdef EPROTOTYPE
    X(EPROTOTYPE)
#endif
#ifdef ENOPROTOOPT
    X(ENOPROTOOPT)
#endif
#ifdef EPROTONOSUPPORT
    X(EPROTONOSUPPORT)
#endif
#ifdef ESOCKTNOSUPPORT
    X(ESOCKTNOSUPPORT)
#endif
#ifdef EOPNOTSUPP
    X(EOPNOTSUPP)
#endif
#ifdef EPFNOSUPPORT
    X(EPFNOSUPPORT)
#endif
#ifdef EAFNOSUPPORT
    X(EAFNOSUPPORT)
#endif
#ifdef EADDRINUSE
    X(EADDRINUSE)
#endif
#ifdef EADDRNOTAVAIL
    X(EADDRNOTAVAIL)
#endif
#ifdef ENETDOWN
    X(ENETDOWN)
#endif
#ifdef ENETUNREACH
    X(ENETUNREACH)
#endif
#ifdef ENETRESET
    X(ENETRESET)
#endif
#ifdef ECONNABORTED
    X(ECONNABORTED)
#endif
#ifdef ECONNRESET
    X(ECONNRESET)
#endif
#ifdef ENOBUFS
    X(ENOBUFS)
#endif
#ifdef EISCONN
    X(EISCONN)
#endif
#ifdef ENOTCONN
    X(ENOTCONN)
#endif
#ifdef ESHUTDOWN
    X(ESHUTDOWN)
#endif
#ifdef ETOOMANYREFS
    X(ETOOMANYREFS)
#endif
#ifdef ETIMEDOUT
    X(ETIMEDOUT)
#endif
#ifdef ECONNREFUSED
    X(ECONNREFUSED)
#endif
#ifdef EHOSTDOWN
    X(EHOSTDOWN)
#endif
#ifdef EHOSTUNREACH
    X(EHOSTUNREACH)
#endif
#ifdef EALREADY
    X(EALREADY)
#endif
#ifdef EINPROGRESS
    X(EINPROGRESS)
#endif
#ifdef ESTALE
    X(ESTALE)
#endif
#ifdef EUCLEAN
    X(EUCLEAN)
#endif
#ifdef ENOTNAM
    X(ENOTNAM)
#endif
#ifdef ENAVAIL
    X(ENAVAIL)
#endif
#ifdef EISNAM
    X(EISNAM)
#endif
#ifdef EREMOTEIO
    X(EREMOTEIO)
#endif
#ifdef EDQUOT
    X(EDQUOT)
#endif
#ifdef ENOMEDIUM
    X(ENOMEDIUM)
#endif
#ifdef EMEDIUMTYPE
    X(EMEDIUMTYPE)
#endif
#ifdef ECANCELED
    X(ECANCELED)
#endif
#ifdef ENOKEY
    X(ENOKEY)
#endif
#ifdef EKEYEXPIRED
    X(EKEYEXPIRED)
#endif
#ifdef EKEYREVOKED
    X(EKEYREVOKED)
#endif
#ifdef EKEYREJECTED
    X(EKEYREJECTED)
#endif
#ifdef EOWNERDEAD
    X(EOWNERDEAD)
#endif
#ifdef ENOTRECOVERABLE
    X(ENOTRECOVERABLE)
#endif
#ifdef ERFKILL
    X(ERFKILL)
#endif
#ifdef EHWPOISON
    X(EHWPOISON)
#endif
#undef X
#undef Y

#if defined(__linux__)
    CHECK(mismatches == 0);
#else
    (void)mismatches;
#endif
    return 0;
}

/* ENOTSUP is 95 on Linux (the same as EOPNOTSUPP); 134 does not exist. */
static int test_errno_enotsup(int seed, int rep) {
#ifdef ENOTSUP
    CHECK(PF_ENOTSUP == ENOTSUP || PF_ENOTSUP == PF_EOPNOTSUPP);
    return 0;
#else
    return SKIP("ENOTSUP is not defined");
#endif
}

pf_test_t suite_errno[] = {
    { test_errno_positive_unique, "/errno/positive_unique" },
    { test_errno_known_values, "/errno/known_values" },
    { test_errno_usable, "/errno/usable" },
    { test_errno_native, "/errno/native" },
    { test_errno_enotsup, "/errno/enotsup" },
    { 0 },
};
