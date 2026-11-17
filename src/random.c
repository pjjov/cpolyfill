/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/random.h>: the system's secure generator, and xoshiro256**.
*/

#include "internal.h"

#include <cpolyfill/random.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <bcrypt.h>
    #include <windows.h>
    #ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG
        #define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
    #endif
    #define PF__RANDOM_BACKEND_WIN32 1
#elif PF_OS_LINUX || PF_OS_ANDROID
    #include <fcntl.h>
    #include <unistd.h>
    #if PF__HAVE_GETRANDOM
        #include <sys/random.h>
    #else
        #include <sys/syscall.h>
    #endif
    #define PF__RANDOM_BACKEND_LINUX 1
#elif PF_OS_APPLE || PF_OS_BSD
    #include <stdlib.h>
    #define PF__RANDOM_BACKEND_ARC4 1
#elif PF_OS_EMSCRIPTEN || PF_OS_WASI
    #include <unistd.h>
    #define PF__RANDOM_BACKEND_GETENTROPY 1
#else
    #include <fcntl.h>
    #include <unistd.h>
    #define PF__RANDOM_BACKEND_URANDOM 1
#endif

#ifndef PF__HAVE_GETRANDOM
    #define PF__HAVE_GETRANDOM 0
#endif

#if PF__RANDOM_BACKEND_LINUX || PF__RANDOM_BACKEND_URANDOM

static int pf__urandom(unsigned char *p, size_t n) {
    int fd;
    #ifdef O_CLOEXEC
    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    #else
    fd = open("/dev/urandom", O_RDONLY);
    #endif
    if (fd < 0)
        return pf_error_from_errno(errno);
    while (n > 0) {
        ssize_t got = read(fd, p, n);
        if (got < 0) {
            int err = errno;
            if (err == EINTR)
                continue;
            close(fd);
            return pf_error_from_errno(err);
        }
        if (got == 0) {
            close(fd);
            return PF_EIO;
        }
        p += got;
        n -= (size_t)got;
    }
    close(fd);
    return PF_OK;
}

#endif

PF_API int pf_getrandom(void *buf, size_t n) {
    unsigned char *p = (unsigned char *)buf;

    if (n == 0)
        return PF_OK;
    if (!buf)
        return PF_EINVAL;

#if PF__RANDOM_BACKEND_WIN32
    while (n > 0) {
        ULONG chunk = n > 0x40000000u ? 0x40000000u : (ULONG)n;
        NTSTATUS st = BCryptGenRandom(
            NULL, p, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG
        );
        if (st < 0)
            return PF_EIO;
        p += chunk;
        n -= chunk;
    }
    return PF_OK;
#elif PF__RANDOM_BACKEND_LINUX
    while (n > 0) {
        long got;
    #if PF__HAVE_GETRANDOM
        got = (long)getrandom(p, n, 0);
    #elif defined(SYS_getrandom)
        got = syscall(SYS_getrandom, p, n, 0);
    #else
        got = -1;
        errno = ENOSYS;
    #endif
        if (got < 0) {
            if (errno == EINTR)
                continue;
            if (errno == ENOSYS || errno == EPERM)
                return pf__urandom(p, n); /* old kernel, seccomp */
            return pf_error_from_errno(errno);
        }
        p += got;
        n -= (size_t)got;
    }
    return PF_OK;
#elif PF__RANDOM_BACKEND_ARC4
    arc4random_buf(p, n);
    return PF_OK;
#elif PF__RANDOM_BACKEND_GETENTROPY
    while (n > 0) {
        size_t chunk = n > 256 ? 256 : n;
        if (getentropy(p, chunk) != 0)
            return pf_error_from_errno(errno);
        p += chunk;
        n -= chunk;
    }
    return PF_OK;
#else
    return pf__urandom(p, n);
#endif
}

/* The arc4random contract: random bytes or no return. */
static void pf__random_or_abort(void *buf, size_t n) {
    if (pf_getrandom(buf, n) != PF_OK)
        abort();
}

PF_API uint32_t pf_random_u32(void) {
    uint32_t v;
    pf__random_or_abort(&v, sizeof(v));
    return v;
}

PF_API uint64_t pf_random_u64(void) {
    uint64_t v;
    pf__random_or_abort(&v, sizeof(v));
    return v;
}

PF_API uint32_t pf_random_uniform(uint32_t bound) {
    uint32_t min, r;

    if (bound < 2)
        return 0;
    /* Reject the values below 2^32 % bound, so every residue is equally
       likely. */
    min = (uint32_t)(0u - bound) % bound;
    do
        r = pf_random_u32();
    while (r < min);
    return r % bound;
}

PF_API int pf_getentropy(void *buf, size_t n) {
    if (n > 256)
        return pf__errno_fail(PF_EINVAL);
    if (pf_getrandom(buf, n) != PF_OK)
        return pf__errno_fail(PF_EIO);
    return 0;
}

PF_API uint32_t pf_arc4random(void) { return pf_random_u32(); }

PF_API void pf_arc4random_buf(void *buf, size_t n) {
    pf__random_or_abort(buf, n);
}

PF_API uint32_t pf_arc4random_uniform(uint32_t bound) {
    return pf_random_uniform(bound);
}

/* ---- xoshiro256** ------------------------------------------------------ */

static uint64_t pf__splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
}

PF_API void pf_rng_seed(pf_rng_t *rng, uint64_t seed) {
    int i;
    /* splitmix64 never yields four zeros in a row. */
    for (i = 0; i < 4; i++)
        rng->s[i] = pf__splitmix64(&seed);
}

PF_API int pf_rng_seed_random(pf_rng_t *rng) {
    pf_rng_t t;
    int rc;

    do {
        rc = pf_getrandom(t.s, sizeof(t.s));
        if (rc)
            return rc;
    } while ((t.s[0] | t.s[1] | t.s[2] | t.s[3]) == 0);
    *rng = t;
    return PF_OK;
}

static uint64_t pf__rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

PF_API uint64_t pf_rng_u64(pf_rng_t *rng) {
    uint64_t *s = rng->s;
    uint64_t result = pf__rotl64(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = pf__rotl64(s[3], 45);
    return result;
}

PF_API uint32_t pf_rng_u32(pf_rng_t *rng) {
    return (uint32_t)(pf_rng_u64(rng) >> 32);
}

PF_API uint64_t pf_rng_uniform(pf_rng_t *rng, uint64_t bound) {
    uint64_t min, r;

    if (bound < 2)
        return 0;
    min = (0u - bound) % bound;
    do
        r = pf_rng_u64(rng);
    while (r < min);
    return r % bound;
}

PF_API double pf_rng_double(pf_rng_t *rng) {
    return (double)(pf_rng_u64(rng) >> 11) * (1.0 / 9007199254740992.0);
}
