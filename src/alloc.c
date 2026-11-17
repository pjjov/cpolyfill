/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#include "internal.h"

#include <stdlib.h>
#include <string.h>

#if PF_OS_WINDOWS
    #include <malloc.h>
#endif

/* An alignment malloc is known to guarantee: 16 on 64-bit and 8 on 32-bit
   systems. Understating it only costs a posix_memalign call. */
#define PF__MALLOC_ALIGN (2 * sizeof(void *))

static int pf__is_pow2(size_t x) { return x && !(x & (x - 1)); }

/*
   Windows has no `free` for over-aligned memory (`_aligned_free` is
   separate) and `allocator_fn` does not pass the alignment when freeing,
   so on Windows every block comes from the `_aligned_*` family.
   Elsewhere malloc is used, and posix_memalign for over-aligned blocks.
*/
void *pf__default_allocate(void *ptr, size_t old, size_t size, size_t zalign) {
    size_t align = zalign & ~(size_t)1;
    int zero = (int)(zalign & 1);
    void *res;

    if (align && !pf__is_pow2(align))
        return NULL;
    if (align < PF__MALLOC_ALIGN)
        align = PF__MALLOC_ALIGN;

    if (!ptr)
        old = 0;

    if (size == 0) {
        /* free, or `deallocate_all` (ptr == self) which is a no-op here */
        if (ptr) {
#if PF_OS_WINDOWS
            _aligned_free(ptr);
#else
            free(ptr);
#endif
        }
        return NULL;
    }

#if PF_OS_WINDOWS
    res = ptr ? _aligned_realloc(ptr, size, align)
              : _aligned_malloc(size, align);
#else
    if (align == PF__MALLOC_ALIGN) {
        if (!ptr)
            return zero ? calloc(1, size) : malloc(size);
        res = realloc(ptr, size);
    } else {
        /* realloc does not keep extended alignment, so always move */
        if (posix_memalign(&res, align, size) != 0)
            return NULL;
        if (ptr) {
            memcpy(res, ptr, old < size ? old : size);
            free(ptr);
        }
    }
#endif

    if (res && zero && size > old)
        memset((char *)res + old, 0, size - old);
    return res;
}
