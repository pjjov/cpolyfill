/** cpolyfill - Filling the gaps between C standards and compilers

    Random numbers: the operating system's cryptographically secure
    generator, and a fast generator for everything else.

    From the operating system (getrandom on Linux, arc4random_buf on macOS
    and the BSDs, BCryptGenRandom on Windows, getentropy on Emscripten and
    WASI, else /dev/urandom):

        int pf_getrandom(void *buf, size_t n)
                Fills `buf` with `n` random bytes: 0, or a PF_E* code
                (nothing is ever partly filled). Blocks only early at boot,
                until the system's generator is seeded.
        uint32_t pf_random_u32(void)
        uint64_t pf_random_u64(void)
        uint32_t pf_random_uniform(uint32_t bound)
                A random number, below `bound` (without bias) for uniform;
                0 if `bound` is below 2. Like arc4random, these abort the
                program if the system cannot give random bytes.

    Stand-ins for the BSD functions:

        int pf_getentropy(void *buf, size_t n)
                0, or -1 with errno EINVAL if `n` is above 256, EIO on
                failure.
        uint32_t pf_arc4random(void)
        void pf_arc4random_buf(void *buf, size_t n)
        uint32_t pf_arc4random_uniform(uint32_t bound)
                Never fail (abort the program instead).

    A fast generator, not suitable for anything secret (xoshiro256**, a
    period of 2^256 - 1). Every pf_rng_t is independent; one must not be
    used by two threads at once.

        pf_rng_t                The state (four uint64_t, never all zero).
        void pf_rng_seed(pf_rng_t *rng, uint64_t seed)
                The same seed always gives the same sequence, on every
                platform.
        int pf_rng_seed_random(pf_rng_t *rng)
                A seed from pf_getrandom: 0 or its error.
        uint32_t pf_rng_u32(pf_rng_t *rng)
        uint64_t pf_rng_u64(pf_rng_t *rng)
        uint64_t pf_rng_uniform(pf_rng_t *rng, uint64_t bound)
                Below `bound`, without bias; 0 if `bound` is below 2.
        double pf_rng_double(pf_rng_t *rng)
                In [0, 1), with 53 random bits.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_RANDOM, where the
    library found them missing when it was built: `getentropy`,
    `arc4random`, `arc4random_buf`, `arc4random_uniform`.

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

#ifndef CPOLYFILL_RANDOM_H
#define CPOLYFILL_RANDOM_H

#include <cpolyfill/config.h>
#include <cpolyfill/platform.h>

#include <stddef.h>
#include <stdint.h>

typedef struct pf_rng_t {
    uint64_t s[4];
} pf_rng_t;

PF_BEGIN_DECLS

PF_API int pf_getrandom(void *buf, size_t n);
PF_API uint32_t pf_random_u32(void);
PF_API uint64_t pf_random_u64(void);
PF_API uint32_t pf_random_uniform(uint32_t bound);

PF_API int pf_getentropy(void *buf, size_t n);
PF_API uint32_t pf_arc4random(void);
PF_API void pf_arc4random_buf(void *buf, size_t n);
PF_API uint32_t pf_arc4random_uniform(uint32_t bound);

PF_API void pf_rng_seed(pf_rng_t *rng, uint64_t seed);
PF_API int pf_rng_seed_random(pf_rng_t *rng);
PF_API uint32_t pf_rng_u32(pf_rng_t *rng);
PF_API uint64_t pf_rng_u64(pf_rng_t *rng);
PF_API uint64_t pf_rng_uniform(pf_rng_t *rng, uint64_t bound);
PF_API double pf_rng_double(pf_rng_t *rng);

PF_END_DECLS

#endif /* CPOLYFILL_RANDOM_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__RANDOM_STD_NAMES)                                  \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_RANDOM)) \
        || defined(PF_STD_NAMES_RANDOM))
    #define PF__RANDOM_STD_NAMES

    #ifdef PF__FORCE_STD_NAMES_POLYFILL
        #define PF__RANDOM_MAP(have) 1
    #else
        #define PF__RANDOM_MAP(have) (!(have))
    #endif

    #if PF__RANDOM_MAP(PF_CONFIG_HAVE_GETENTROPY)
        #undef getentropy
        #define getentropy pf_getentropy
    #endif
    #if PF__RANDOM_MAP(PF_CONFIG_HAVE_ARC4RANDOM)
        #undef arc4random
        #define arc4random pf_arc4random
    #endif
    #if PF__RANDOM_MAP(PF_CONFIG_HAVE_ARC4RANDOM_BUF)
        #undef arc4random_buf
        #define arc4random_buf pf_arc4random_buf
    #endif
    #if PF__RANDOM_MAP(PF_CONFIG_HAVE_ARC4RANDOM_UNIFORM)
        #undef arc4random_uniform
        #define arc4random_uniform pf_arc4random_uniform
    #endif
#endif
