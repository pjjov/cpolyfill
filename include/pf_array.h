/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a minimal resizable array implementation.
    While lacking in type safety and features, it is good enough for
    most use cases, especially when already using other polyfills.

    Macro function reference:
    - PF_ARRAY(TYPE)
    - PF_ARRAY_INIT(array, capacity)
    - PF_ARRAY_FREE(array)
    - PF_ARRAY_GET(array, index)
    - PF_ARRAY_SLOT(array, index)
    - PF_ARRAY_PUSH(array, item, count)
    - PF_ARRAY_INCR(array, count)
    - PF_ARRAY_POP(array, out, count)
    - PF_ARRAY_RESERVE(array, count)
    - PF_ARRAY_RESIZE(array, capacity)
    - PF_ARRAY_SETLEN(array, length)

    SPDX-FileCopyrightText: 2026 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2026 Predrag Jovanović

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

#ifndef POLYFILL_ARRAY
#define POLYFILL_ARRAY

#include <stddef.h>
#include <string.h>

enum pf_array_error {
    PF_ARRAY_OK = 0,
    PF_ARRAY_EINTR = -4,
    PF_ARRAY_ENOMEM = -12,
    PF_ARRAY_EINVAL = -22,
};

#define PF_ARRAY(TYPE)        \
    union {                   \
        struct pf_array base; \
        TYPE *type;           \
    }

struct pf_array {
    size_t capacity;
    size_t length;
    void *items;
#ifdef PF_ARRAY_USE_ALLOCATOR_T
    allocator_t *alloc;
#elif defined(PF_ARRAY_CUSTOM_ALLOC)
    PF_ARRAY_CUSTOM_ALLOC
#endif
};

#define PF_ARRAY_INIT(array, capacity)                                        \
    pf__array_init(PF__ARRAY_BASE(array), (capacity) * PF__ARRAY_SIZE(array))

#define PF_ARRAY_FREE(array) pf__array_free(PF__ARRAY_BASE(array))

#define PF_ARRAY_GET(array, index)                             \
    ((PF__ARRAY_TYPE(array))pf__array_get(                     \
        PF__ARRAY_BASE(array), (index) * PF__ARRAY_SIZE(array) \
    ))

#define PF_ARRAY_SLOT(array, index)                            \
    ((PF__ARRAY_TYPE(array))pf__array_slot(                    \
        PF__ARRAY_BASE(array), (index) * PF__ARRAY_SIZE(array) \
    ))

#define PF_ARRAY_PUSH(array, item, count)                              \
    pf__array_push(                                                    \
        PF__ARRAY_BASE(array), (item), (count) * PF__ARRAY_SIZE(array) \
    )

#define PF_ARRAY_INCR(array, count)                            \
    ((PF__ARRAY_TYPE(array))pf__array_incr(                    \
        PF__ARRAY_BASE(array), (count) * PF__ARRAY_SIZE(array) \
    ))

#define PF_ARRAY_POP(array, out, count)                               \
    pf__array_push(                                                   \
        PF__ARRAY_BASE(array), (out), (count) * PF__ARRAY_SIZE(array) \
    )

#define PF_ARRAY_RESERVE(array, count)                                        \
    pf__array_reserve(PF__ARRAY_BASE(array), (count) * PF__ARRAY_SIZE(array))

#define PF_ARRAY_RESIZE(array, capacity) \
    pf__array_resize(PF__ARRAY_BASE(array), (capacity) * PF__ARRAY_SIZE(array))

#define PF_ARRAY_SETLEN(array, length)                                        \
    pf__array_setlen(PF__ARRAY_BASE(array), (length) * PF__ARRAY_SIZE(array))

#if !defined(PF_ARRAY_CUSTOM_ALLOC) && !defined(PF_ARRAY_USE_ALLOCATOR_T)
    #include <stdlib.h>
#endif

#ifndef POLYFILL_MACRO

    #define PF_COUNTOF(array)                                \
        ((sizeof(array) / sizeof(0 [array]))                 \
         / ((size_t)(!(sizeof(array) % sizeof(0 [array])))))

    #define PF_OFFSET(ptr, offset) ((void *)(&((char *)(ptr))[offset]))

#endif

#ifdef PF_ARRAY_USE_ALLOCATOR_T
    #define PF_ARRAY_MALLOC(array, size) allocate((array)->alloc, (size))
    #define PF_ARRAY_REALLOC(array, ptr, size)                       \
        reallocate((array)->alloc, (ptr), (array)->capacity, (size))
    #define PF_ARRAY_DEALLOC(array, ptr)                     \
        deallocate((array)->alloc, (ptr), (array)->capacity)
#elif !defined(PF_ARRAY_CUSTOM_ALLOC)
    #define PF_ARRAY_MALLOC(array, size) malloc((size))
    #define PF_ARRAY_REALLOC(array, ptr, size) realloc((ptr), (size))
    #define PF_ARRAY_DEALLOC(array, ptr) free((ptr))
#endif

#ifndef PF_ARRAY_GROW
    #define PF_ARRAY_GROW(old, req) (((old) + (req)) * 2)
#endif

#define PF__ARRAY_SIZE(array) sizeof(*(array).type)
#define PF__ARRAY_TYPE(array) typeof((array).type)
#define PF__ARRAY_BASE(array) (&(array).base)

static inline int pf__array_init(struct pf_array *a, size_t capacity) {
    if (!a || capacity == 0)
        return PF_ARRAY_EINVAL;

    void *buf = PF_ARRAY_MALLOC(a, capacity);

    if (!buf)
        return PF_ARRAY_ENOMEM;

    a->items = buf;
    a->length = 0;
    a->capacity = 0;

#ifdef PF_ARRAY_USE_ALLOCATOR_T
    a->alloc = PF_ARRAY_DEFAULT_ALLOCATOR;
#endif

    return PF_ARRAY_OK;
}

static inline void pf__array_free(struct pf_array *a) {
    if (a)
        PF_ARRAY_DEALLOC(a, a->items);
}

static inline void *pf__array_get(struct pf_array *a, size_t index) {
    if (!a || index >= a->length)
        return NULL;

    return PF_OFFSET(a->items, index);
}

static inline void *pf__array_slot(struct pf_array *a, size_t index) {
    if (!a || index >= a->capacity)
        return NULL;

    return PF_OFFSET(a->items, index);
}

static inline int pf__array_setlen(struct pf_array *a, size_t length) {
    if (!a || length > a->capacity)
        return PF_ARRAY_EINVAL;

    a->length = length;
    return PF_ARRAY_OK;
}

static inline int pf__array_resize(struct pf_array *a, size_t capacity) {
    if (!a || capacity == 0)
        return PF_ARRAY_EINVAL;

    void *buf = PF_ARRAY_REALLOC(a, a->items, capacity);

    if (!buf)
        return PF_ARRAY_ENOMEM;

    a->capacity = capacity;
    a->items = buf;
    if (a->length > capacity)
        a->length = capacity;
    return PF_ARRAY_OK;
}

static inline int pf__array_reserve(struct pf_array *a, size_t size) {
    if (!a || size == 0)
        return PF_ARRAY_EINVAL;

    if (a->length + size <= a->capacity)
        return PF_ARRAY_OK;

    size_t capacity = PF_ARRAY_GROW(a->length, size);
    return pf__array_resize(a, capacity);
}

static inline int pf__array_push(struct pf_array *a, void *item, size_t size) {
    if (!a || size == 0)
        return PF_ARRAY_EINVAL;
    if (pf__array_reserve(a, size))
        return PF_ARRAY_ENOMEM;

    memcpy(PF_OFFSET(a->items, a->length), item, size);
    a->length += size;
    return PF_ARRAY_OK;
}

static inline int pf__array_pop(struct pf_array *a, void *out, size_t size) {
    if (!a || size == 0 || a->length < size)
        return PF_ARRAY_EINVAL;

    a->length -= size;

    if (out)
        memcpy(out, PF_OFFSET(a->items, a->length), size);
    return PF_ARRAY_OK;
}

static inline void *pf__array_incr(struct pf_array *a, size_t size) {
    if (!a || size == 0)
        return NULL;
    if (pf__array_reserve(a, size))
        return NULL;

    void *out = PF_OFFSET(a->items, a->length);
    a->length += size;
    return out;
}

#endif
