/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides implementations of the <stdatomic.h> interface.
    The implementations are best-effort, leaving many quirks of the interface
    unimplemented for some systems and compilers. This includes:

    - ATOMIC_VAR_INIT and ATOMIC_FLAG_INIT are not well supported and should be
      replaced with `atomic_init` and `pf_atomic_flag_init`.
    - Only integer atomic types are supported (no `_Atomic(struct example)`).
    - `_Atomic` must be used with parentheses, keyword version is not supported.
    - `memory_order` is ignored in threads fallback as of time of writing.
    - The threads fallback needs `typeof` (or `__typeof__` on GNU compilers).
    - Objects in the threads fallback must be initialized with `atomic_init`
      (or `pf_atomic_flag_init`) before use unless a zeroed `mtx_t` is valid
      on the platform. Arguments to the macros may be evaluated more than once.
    - `kill_dependency`, `atomic_signal_fence` and `atomic_thread_fence` are not
      implemented on non-GNU platforms. Functions are replaced as no-op.

    If threads are available (<pf_threads.h>) the fallback implementation will
    lock atomic operations for all types. On Windows, some types will have true
    atomic operations (without locking) depending on availability.

    If threads are not available, PF_NO_ATOMICS will be defined and all
    functions will be replaced with no-ops to allow clean compilations.

    Last-updated: October 2026
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

#ifndef POLYFILL_ATOMIC
#define POLYFILL_ATOMIC

#ifndef PF_API
    #define PF_API static inline
#endif

#ifndef PF_ATOMIC_NONE
    #if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
        #define PF_ATOMIC_STD
    #elif defined(__GNUC__)
        #define PF_ATOMIC_GNUC
    #elif defined(_WIN32)
        #define PF_ATOMIC_WIN32
    #else
        #define PF_ATOMIC_NONE
    #endif
#endif

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
    #define PF_ATOMIC_HAS_THREADS
#elif defined(POLYFILL_THREADS)
    #define PF_ATOMIC_HAS_THREADS
#endif

#if (defined(PF_ATOMIC_NONE) || defined(PF_ATOMIC_WIN32)) \
    && !defined(PF_ATOMIC_HAS_THREADS)
    #define PF_NO_ATOMICS
#endif

#ifdef PF_ATOMIC_STD
    #include <stdatomic.h>
#else

    /* Common definitions for every non-standard implementation. These must come
       before the implementations, which refer to `_Bool` and `memory_order`. */

    #include <stddef.h>
    #include <stdint.h>
    #include <wchar.h>

    #if defined(__has_include)
        #if __has_include(<uchar.h>)
            #include <uchar.h>
        #elif !defined(__cplusplus)
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
        #endif
    #elif !defined(__APPLE__)
        #include <uchar.h>
    #endif

    #if defined(__cplusplus)
typedef bool _Bool;
    #elif (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) \
        && !defined(__GNUC__)
typedef int _Bool;
    #endif

    #if defined(__cplusplus)
        #ifndef __cpp_char8_t
typedef unsigned char char8_t;
        #endif
    #elif !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
typedef uint8_t char8_t;
    #endif

    #ifdef PF_ATOMIC_GNUC
enum memory_order {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
};
    #else
enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};
    #endif
typedef enum memory_order memory_order;

#endif

#ifdef PF_ATOMIC_STD
    /* Nothing more to do. */
#elif defined(PF_ATOMIC_GNUC)

    #define _Atomic(...) __VA_ARGS__

    /* Lock-free macros are 0 (never), 1 (sometimes) or 2 (always). */
    #define PF__GNUC_LOCK_FREE(type)                         \
        (__atomic_always_lock_free(sizeof(type), 0) ? 2 : 0)

    #define ATOMIC_BOOL_LOCK_FREE PF__GNUC_LOCK_FREE(_Bool)
    #define ATOMIC_CHAR_LOCK_FREE PF__GNUC_LOCK_FREE(char)
    #define ATOMIC_CHAR8_T_LOCK_FREE PF__GNUC_LOCK_FREE(char8_t)
    #define ATOMIC_CHAR16_T_LOCK_FREE PF__GNUC_LOCK_FREE(char16_t)
    #define ATOMIC_CHAR32_T_LOCK_FREE PF__GNUC_LOCK_FREE(char32_t)
    #define ATOMIC_WCHAR_T_LOCK_FREE PF__GNUC_LOCK_FREE(wchar_t)
    #define ATOMIC_SHORT_LOCK_FREE PF__GNUC_LOCK_FREE(short)
    #define ATOMIC_INT_LOCK_FREE PF__GNUC_LOCK_FREE(int)
    #define ATOMIC_LONG_LOCK_FREE PF__GNUC_LOCK_FREE(long)
    #define ATOMIC_LLONG_LOCK_FREE PF__GNUC_LOCK_FREE(long long)
    #define ATOMIC_POINTER_LOCK_FREE PF__GNUC_LOCK_FREE(void *)

    /* `obj` is a pointer to the atomic object, not the object itself. */
    #define atomic_is_lock_free(obj)                 \
        __atomic_is_lock_free(sizeof(*(obj)), (obj))

    #define atomic_store(obj, desired)                           \
        __atomic_store_n((obj), (desired), memory_order_seq_cst)
    #define atomic_store_explicit(obj, desired, order) \
        __atomic_store_n((obj), (desired), (order))

    #define atomic_load(obj) __atomic_load_n((obj), memory_order_seq_cst)
    #define atomic_load_explicit(obj, order) __atomic_load_n((obj), (order))

    #define atomic_exchange(obj, desired)                           \
        __atomic_exchange_n((obj), (desired), memory_order_seq_cst)
    #define atomic_exchange_explicit(obj, desired, order) \
        __atomic_exchange_n((obj), (desired), (order))

    #define atomic_compare_exchange_strong(o, expect, desire) \
        __atomic_compare_exchange_n(                          \
            (o),                                              \
            (expect),                                         \
            (desire),                                         \
            0,                                                \
            memory_order_seq_cst,                             \
            memory_order_seq_cst                              \
        )

    #define atomic_compare_exchange_weak(o, expect, desire) \
        __atomic_compare_exchange_n(                        \
            (o),                                            \
            (expect),                                       \
            (desire),                                       \
            1,                                              \
            memory_order_seq_cst,                           \
            memory_order_seq_cst                            \
        )

    #define atomic_compare_exchange_strong_explicit(                          \
        o, expect, desire, ok, fail                                           \
    )                                                                         \
        __atomic_compare_exchange_n((o), (expect), (desire), 0, (ok), (fail))

    #define atomic_compare_exchange_weak_explicit(o, expect, desire, ok, fail) \
        __atomic_compare_exchange_n((o), (expect), (desire), 1, (ok), (fail))

    #define atomic_fetch_add(obj, arg)                         \
        __atomic_fetch_add((obj), (arg), memory_order_seq_cst)
    #define atomic_fetch_add_explicit(obj, arg, order) \
        __atomic_fetch_add((obj), (arg), (order))

    #define atomic_fetch_sub(obj, arg)                         \
        __atomic_fetch_sub((obj), (arg), memory_order_seq_cst)
    #define atomic_fetch_sub_explicit(obj, arg, order) \
        __atomic_fetch_sub((obj), (arg), (order))

    #define atomic_fetch_or(obj, arg)                         \
        __atomic_fetch_or((obj), (arg), memory_order_seq_cst)
    #define atomic_fetch_or_explicit(obj, arg, order) \
        __atomic_fetch_or((obj), (arg), (order))

    #define atomic_fetch_xor(obj, arg)                         \
        __atomic_fetch_xor((obj), (arg), memory_order_seq_cst)
    #define atomic_fetch_xor_explicit(obj, arg, order) \
        __atomic_fetch_xor((obj), (arg), (order))

    #define atomic_fetch_and(obj, arg)                         \
        __atomic_fetch_and((obj), (arg), memory_order_seq_cst)
    #define atomic_fetch_and_explicit(obj, arg, order) \
        __atomic_fetch_and((obj), (arg), (order))

typedef struct {
    char value;
} atomic_flag;

    #define atomic_flag_test_and_set(obj)                          \
        __atomic_test_and_set(&(obj)->value, memory_order_seq_cst)
    #define atomic_flag_test_and_set_explicit(obj, order) \
        __atomic_test_and_set(&(obj)->value, (order))
    #define atomic_flag_clear(obj)                          \
        __atomic_clear(&(obj)->value, memory_order_seq_cst)
    #define atomic_flag_clear_explicit(obj, order) \
        __atomic_clear(&(obj)->value, (order))

    #define PF_HAS_ATOMIC_FLAG_INIT
PF_API void pf_atomic_flag_init(volatile atomic_flag *obj) {
    atomic_flag_clear(obj);
}

    #define atomic_init(obj, desired) (*(obj) = (desired))
    #define ATOMIC_VAR_INIT(...) __VA_ARGS__
    #define ATOMIC_FLAG_INIT { 0 }

    #define kill_dependency(y) (y)
    #define atomic_thread_fence __atomic_thread_fence
    #define atomic_signal_fence __atomic_signal_fence

#elif defined(PF_ATOMIC_HAS_THREADS)

    #if !defined(POLYFILL_THREADS)
        #include <threads.h>
    #endif

    #include <limits.h>
    #include <string.h>

    #ifdef PF_ATOMIC_WIN32
        #include <windows.h>
        #if defined(_MSC_VER)
            #include <intrin.h>
        #endif
    #endif

    #if defined(__GNUC__) || defined(__clang__)
        #define PF__TYPEOF __typeof__
    #else
        #define PF__TYPEOF typeof
    #endif

/* Values are stored zero-extended and truncated to the size of the atomic type
   (the "raw bits" of the object), so wrap-around, comparisons and exchanges
   behave as they do on the real type. */
typedef struct pf_atomic_t {
    mtx_t lock;
    uintmax_t value;
} pf_atomic_t;

    #define _Atomic(TYPE)     \
        union {               \
            pf_atomic_t base; \
            TYPE type;        \
        }

PF_API void pf_atomic_init_lock(mtx_t *lock) {
    (void)mtx_init(lock, mtx_plain);
}

PF_API void pf_atomic_lock(mtx_t *lock) { (void)mtx_lock(lock); }

PF_API void pf_atomic_unlock(mtx_t *lock) { (void)mtx_unlock(lock); }

PF_API uintmax_t pf_atomic_mask(size_t size) {
    if (size >= sizeof(uintmax_t))
        return UINTMAX_MAX;
    return (((uintmax_t)1) << (size * 8)) - 1;
}

/* Copy between an object of `size` bytes and a uintmax_t, in either byte
   order. */
PF_API uintmax_t pf_atomic_unpack(const void *src, size_t size) {
    const unsigned one = 1;
    uintmax_t value = 0;
    unsigned char *bytes = (unsigned char *)&value;

    if (*(const unsigned char *)&one)
        memcpy(bytes, src, size);
    else
        memcpy(bytes + sizeof(value) - size, src, size);
    return value;
}

PF_API void pf_atomic_pack(void *dst, uintmax_t value, size_t size) {
    const unsigned one = 1;
    const unsigned char *bytes = (const unsigned char *)&value;

    if (*(const unsigned char *)&one)
        memcpy(dst, bytes, size);
    else
        memcpy(dst, bytes + sizeof(value) - size, size);
}

/* On Windows, objects of size 1, 2, 4 and 8 are *only* ever accessed through
   interlocked operations. Mixing them with mutex-protected operations on the
   same object would not be atomic, so the choice depends on size alone. */
PF_API _Bool pf_atomic_lock_free(size_t size) {
    #ifdef PF_ATOMIC_WIN32
    return size == 1 || size == 2 || size == 4 || size == 8;
    #else
    (void)size;
    return 0;
    #endif
}

/* Returns the previous (truncated) value. */
PF_API uintmax_t pf_atomic_w32_cmpxchg(
    pf_atomic_t *atomic, size_t size, uintmax_t expected, uintmax_t desired
) {
    #ifdef PF_ATOMIC_WIN32
        #if defined(_MSC_VER)
    switch (size) {
    case 1:
        return (unsigned char)_InterlockedCompareExchange8(
            (char volatile *)&atomic->value, (char)desired, (char)expected
        );
    case 2:
        return (unsigned short)_InterlockedCompareExchange16(
            (short volatile *)&atomic->value, (short)desired, (short)expected
        );
    case 4:
        return (uint32_t)_InterlockedCompareExchange(
            (long volatile *)&atomic->value, (long)desired, (long)expected
        );
    case 8:
        return (uint64_t)_InterlockedCompareExchange64(
            (__int64 volatile *)&atomic->value,
            (__int64)desired,
            (__int64)expected
        );
    default:
        break;
    }
        #else
    /* GNU-compatible compilers on Windows lack some of the intrinsics. */
    switch (size) {
    case 1:
        return __sync_val_compare_and_swap(
            (volatile uint8_t *)&atomic->value,
            (uint8_t)expected,
            (uint8_t)desired
        );
    case 2:
        return __sync_val_compare_and_swap(
            (volatile uint16_t *)&atomic->value,
            (uint16_t)expected,
            (uint16_t)desired
        );
    case 4:
        return __sync_val_compare_and_swap(
            (volatile uint32_t *)&atomic->value,
            (uint32_t)expected,
            (uint32_t)desired
        );
    case 8:
        return __sync_val_compare_and_swap(
            (volatile uint64_t *)&atomic->value,
            (uint64_t)expected,
            (uint64_t)desired
        );
    default:
        break;
    }
        #endif
    #else
    (void)atomic;
    (void)size;
    (void)expected;
    (void)desired;
    #endif
    return 0;
}

PF_API void pf_atomic_init(
    pf_atomic_t *atomic, size_t size, uintmax_t desired
) {
    atomic->value = desired & pf_atomic_mask(size);
    if (!pf_atomic_lock_free(size))
        pf_atomic_init_lock(&atomic->lock);
}

PF_API uintmax_t pf_atomic_load(pf_atomic_t *atomic, size_t size) {
    uintmax_t out;

    if (pf_atomic_lock_free(size))
        return pf_atomic_w32_cmpxchg(atomic, size, 0, 0);

    pf_atomic_lock(&atomic->lock);
    out = atomic->value & pf_atomic_mask(size);
    pf_atomic_unlock(&atomic->lock);
    return out;
}

enum {
    PF__ATOMIC_OP_ADD,
    PF__ATOMIC_OP_SUB,
    PF__ATOMIC_OP_AND,
    PF__ATOMIC_OP_OR,
    PF__ATOMIC_OP_XOR,
    PF__ATOMIC_OP_XCHG
};

PF_API uintmax_t pf_atomic_apply(int op, uintmax_t old, uintmax_t arg) {
    switch (op) {
    case PF__ATOMIC_OP_ADD:
        return old + arg;
    case PF__ATOMIC_OP_SUB:
        return old - arg;
    case PF__ATOMIC_OP_AND:
        return old & arg;
    case PF__ATOMIC_OP_OR:
        return old | arg;
    case PF__ATOMIC_OP_XOR:
        return old ^ arg;
    default:
        return arg;
    }
}

/* Read-modify-write; returns the previous value. */
PF_API uintmax_t
pf_atomic_rmw(pf_atomic_t *atomic, size_t size, int op, uintmax_t arg) {
    uintmax_t mask = pf_atomic_mask(size);
    uintmax_t prev;

    if (pf_atomic_lock_free(size)) {
        uintmax_t seen = pf_atomic_w32_cmpxchg(atomic, size, 0, 0);
        do {
            prev = seen;
            seen = pf_atomic_w32_cmpxchg(
                atomic, size, prev, pf_atomic_apply(op, prev, arg) & mask
            );
        } while (seen != prev);
        return prev;
    }

    pf_atomic_lock(&atomic->lock);
    prev = atomic->value & mask;
    atomic->value = pf_atomic_apply(op, prev, arg) & mask;
    pf_atomic_unlock(&atomic->lock);
    return prev;
}

    #define PF__IMPL_ATOMIC_OP(NAME, OP)                    \
        PF_API uintmax_t pf_atomic_##NAME(                  \
            pf_atomic_t *atomic, size_t size, uintmax_t arg \
        ) {                                                 \
            return pf_atomic_rmw(atomic, size, OP, arg);    \
        }

PF__IMPL_ATOMIC_OP(fetch_add, PF__ATOMIC_OP_ADD)
PF__IMPL_ATOMIC_OP(fetch_sub, PF__ATOMIC_OP_SUB)
PF__IMPL_ATOMIC_OP(fetch_and, PF__ATOMIC_OP_AND)
PF__IMPL_ATOMIC_OP(fetch_or, PF__ATOMIC_OP_OR)
PF__IMPL_ATOMIC_OP(fetch_xor, PF__ATOMIC_OP_XOR)
PF__IMPL_ATOMIC_OP(exchange, PF__ATOMIC_OP_XCHG)

PF_API void pf_atomic_store(pf_atomic_t *atomic, size_t size, uintmax_t value) {
    (void)pf_atomic_rmw(atomic, size, PF__ATOMIC_OP_XCHG, value);
}

/* `expected` points to an object of `size` bytes. */
PF_API _Bool pf_atomic_cas(
    pf_atomic_t *atomic, size_t size, void *expected, uintmax_t desired
) {
    uintmax_t mask = pf_atomic_mask(size);
    uintmax_t want = pf_atomic_unpack(expected, size);
    uintmax_t prev;
    _Bool ok;

    desired &= mask;

    if (pf_atomic_lock_free(size)) {
        prev = pf_atomic_w32_cmpxchg(atomic, size, want, desired);
        ok = prev == want;
    } else {
        pf_atomic_lock(&atomic->lock);
        prev = atomic->value & mask;
        ok = prev == want;
        if (ok)
            atomic->value = desired;
        pf_atomic_unlock(&atomic->lock);
    }

    if (!ok)
        pf_atomic_pack(expected, prev, size);
    return ok;
}

    #define PF__ATOMIC_TYPE(obj) PF__TYPEOF((obj)->type)
    #define PF__ATOMIC_ARG(obj, value)             \
        ((uintmax_t)(PF__ATOMIC_TYPE(obj))(value))
    /* The statement expression avoids "value computed is not used" warnings
       when the result of a fetch operation is discarded. */
    #if defined(__GNUC__) || defined(__clang__)
        #define PF__ATOMIC_UNWRAP(obj, FUNC, ...)                  \
            __extension__({                                        \
                PF__ATOMIC_TYPE(obj)                               \
                pf__result_ = (PF__ATOMIC_TYPE(obj))FUNC(          \
                    &(obj)->base, sizeof((obj)->type), __VA_ARGS__ \
                );                                                 \
                pf__result_;                                       \
            })
    #else
        #define PF__ATOMIC_UNWRAP(obj, FUNC, ...)                      \
            ((PF__ATOMIC_TYPE(obj))                                    \
                 FUNC(&(obj)->base, sizeof((obj)->type), __VA_ARGS__))
    #endif

    #define atomic_init(obj, desired)                                       \
        pf_atomic_init(                                                     \
            &(obj)->base, sizeof((obj)->type), PF__ATOMIC_ARG(obj, desired) \
        )
    #define atomic_load(obj)                                    \
        ((PF__ATOMIC_TYPE(obj))                                 \
             pf_atomic_load(&(obj)->base, sizeof((obj)->type)))
    #define atomic_store(obj, value)                                      \
        pf_atomic_store(                                                  \
            &(obj)->base, sizeof((obj)->type), PF__ATOMIC_ARG(obj, value) \
        )
    #define atomic_fetch_add(obj, other) \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_fetch_add, PF__ATOMIC_ARG(obj, other))
    #define atomic_fetch_sub(obj, other) \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_fetch_sub, PF__ATOMIC_ARG(obj, other))
    #define atomic_fetch_or(obj, other)                                        \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_fetch_or, PF__ATOMIC_ARG(obj, other))
    #define atomic_fetch_xor(obj, other) \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_fetch_xor, PF__ATOMIC_ARG(obj, other))
    #define atomic_fetch_and(obj, other) \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_fetch_and, PF__ATOMIC_ARG(obj, other))
    #define atomic_exchange(obj, desired) \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_exchange, PF__ATOMIC_ARG(obj, desired))
    #define atomic_compare_exchange_weak(obj, expected, desired) \
        pf_atomic_cas(                                           \
            &(obj)->base,                                        \
            sizeof((obj)->type),                                 \
            (expected),                                          \
            PF__ATOMIC_ARG(obj, desired)                         \
        )
    #define atomic_compare_exchange_strong(obj, expected, desired) \
        atomic_compare_exchange_weak(obj, expected, desired)

    #ifdef PF_ATOMIC_WIN32
        #define PF_ATOMIC_LOCK_FREE(type)                                 \
            ((sizeof(type) == 1 || sizeof(type) == 2 || sizeof(type) == 4 \
              || sizeof(type) == 8)                                       \
                 ? 2                                                      \
                 : 0)
    #else
        #define PF_ATOMIC_LOCK_FREE(type) 0
    #endif

    #define ATOMIC_BOOL_LOCK_FREE PF_ATOMIC_LOCK_FREE(_Bool)
    #define ATOMIC_CHAR_LOCK_FREE PF_ATOMIC_LOCK_FREE(char)
    #define ATOMIC_CHAR8_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(char8_t)
    #define ATOMIC_CHAR16_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(char16_t)
    #define ATOMIC_CHAR32_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(char32_t)
    #define ATOMIC_WCHAR_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(wchar_t)
    #define ATOMIC_SHORT_LOCK_FREE PF_ATOMIC_LOCK_FREE(short)
    #define ATOMIC_INT_LOCK_FREE PF_ATOMIC_LOCK_FREE(int)
    #define ATOMIC_LONG_LOCK_FREE PF_ATOMIC_LOCK_FREE(long)
    #define ATOMIC_LLONG_LOCK_FREE PF_ATOMIC_LOCK_FREE(long long)
    #define ATOMIC_POINTER_LOCK_FREE PF_ATOMIC_LOCK_FREE(void *)

    #define atomic_is_lock_free(obj)                     \
        (PF_ATOMIC_LOCK_FREE(PF__ATOMIC_TYPE(obj)) != 0)

    /* The value is kept, but the lock still needs atomic_init (see above). */
    #define ATOMIC_VAR_INIT(value)      \
        {                               \
            .base = {.value = (value) } \
        }

    #define atomic_load_explicit(obj, order) atomic_load(obj)
    #define atomic_store_explicit(obj, value, order) atomic_store(obj, value)
    #define atomic_fetch_add_explicit(obj, other, order) \
        atomic_fetch_add(obj, other)
    #define atomic_fetch_sub_explicit(obj, other, order) \
        atomic_fetch_sub(obj, other)
    #define atomic_fetch_or_explicit(obj, other, order) \
        atomic_fetch_or(obj, other)
    #define atomic_fetch_xor_explicit(obj, other, order) \
        atomic_fetch_xor(obj, other)
    #define atomic_fetch_and_explicit(obj, other, order) \
        atomic_fetch_and(obj, other)
    #define atomic_exchange_explicit(obj, desired, order) \
        atomic_exchange(obj, desired)
    #define atomic_compare_exchange_weak_explicit(           \
        obj, expected, desired, success, failure             \
    )                                                        \
        atomic_compare_exchange_weak(obj, expected, desired)
    #define atomic_compare_exchange_strong_explicit(           \
        obj, expected, desired, success, failure               \
    )                                                          \
        atomic_compare_exchange_strong(obj, expected, desired)

typedef struct {
    mtx_t lock;
    _Bool value;
} atomic_flag;

    #define ATOMIC_FLAG_INIT { 0 }
    #define PF_HAS_ATOMIC_FLAG_INIT

PF_API void pf_atomic_flag_init(volatile atomic_flag *obj) {
    atomic_flag *flag = (atomic_flag *)obj;
    pf_atomic_init_lock(&flag->lock);
    flag->value = 0;
}

PF_API _Bool atomic_flag_test_and_set(volatile atomic_flag *obj) {
    atomic_flag *flag = (atomic_flag *)obj;
    _Bool prev;
    pf_atomic_lock(&flag->lock);
    prev = flag->value;
    flag->value = 1;
    pf_atomic_unlock(&flag->lock);
    return prev;
}

PF_API _Bool atomic_flag_test_and_set_explicit(
    volatile atomic_flag *obj, memory_order order
) {
    (void)order;
    return atomic_flag_test_and_set(obj);
}

PF_API void atomic_flag_clear(volatile atomic_flag *obj) {
    atomic_flag *flag = (atomic_flag *)obj;
    pf_atomic_lock(&flag->lock);
    flag->value = 0;
    pf_atomic_unlock(&flag->lock);
}

PF_API void atomic_flag_clear_explicit(
    volatile atomic_flag *obj, memory_order order
) {
    (void)order;
    atomic_flag_clear(obj);
}

    #define kill_dependency(y) (y)
    #define atomic_thread_fence(order) ((void)(order))
    #define atomic_signal_fence(order) ((void)(order))
#else

    /* Fallback implementation with no threads. These macros allow for code to
   compile cleanly, but the user should still check for PF_NO_ATOMICS macro.
*/

    #define _Atomic(...) __VA_ARGS__

    #define ATOMIC_BOOL_LOCK_FREE -1
    #define ATOMIC_CHAR_LOCK_FREE -1
    #define ATOMIC_CHAR8_T_LOCK_FREE -1
    #define ATOMIC_CHAR16_T_LOCK_FREE -1
    #define ATOMIC_CHAR32_T_LOCK_FREE -1
    #define ATOMIC_WCHAR_T_LOCK_FREE -1
    #define ATOMIC_SHORT_LOCK_FREE -1
    #define ATOMIC_INT_LOCK_FREE -1
    #define ATOMIC_LONG_LOCK_FREE -1
    #define ATOMIC_LLONG_LOCK_FREE -1
    #define ATOMIC_POINTER_LOCK_FREE -1
    #define atomic_is_lock_free(type) -1

    #define atomic_load(...) 0
    #define atomic_store(...)
    #define atomic_fetch_add(...) 0
    #define atomic_fetch_sub(...) 0
    #define atomic_fetch_or(...) 0
    #define atomic_fetch_xor(...) 0
    #define atomic_fetch_and(...) 0
    #define atomic_exchange(...) 0
    #define atomic_compare_exchange_weak(...) 0
    #define atomic_compare_exchange_strong(...) 0
    #define atomic_load_explicit(...) 0
    #define atomic_store_explicit(...)
    #define atomic_fetch_add_explicit(...) 0
    #define atomic_fetch_sub_explicit(...) 0
    #define atomic_fetch_or_explicit(...) 0
    #define atomic_fetch_xor_explicit(...) 0
    #define atomic_fetch_and_explicit(...) 0
    #define atomic_exchange_explicit(...) 0
    #define atomic_compare_exchange_weak_explicit(...) 0
    #define atomic_compare_exchange_strong_explicit(...) 0
    #define atomic_flag_test_and_set(...) 0
    #define atomic_flag_test_and_set_explicit(...) 0
    #define atomic_flag_clear(...)
    #define atomic_flag_clear_explicit(...)
    #define atomic_init(...)
    #define kill_dependency(y) (y)
    #define atomic_thread_fence(...)
    #define atomic_signal_fence(...)

    #define ATOMIC_FLAG_INIT { 0 }
    #define ATOMIC_VAR_INIT(value) (value)
typedef struct {
    char unused;
} atomic_flag;

#endif

#ifndef PF_HAS_ATOMIC_FLAG_INIT
    #ifdef PF_ATOMIC_STD
PF_API void pf_atomic_flag_init(volatile atomic_flag *obj) {
    atomic_flag_clear(obj);
}
    #else
PF_API void pf_atomic_flag_init(volatile atomic_flag *obj) { (void)obj; }
    #endif
#endif

#ifndef PF_ATOMIC_STD

typedef _Atomic(_Bool) atomic_bool;
typedef _Atomic(char) atomic_char;
typedef _Atomic(signed char) atomic_schar;
typedef _Atomic(unsigned char) atomic_uchar;
typedef _Atomic(short) atomic_short;
typedef _Atomic(unsigned short) atomic_ushort;
typedef _Atomic(int) atomic_int;
typedef _Atomic(unsigned int) atomic_uint;
typedef _Atomic(long) atomic_long;
typedef _Atomic(unsigned long) atomic_ulong;
typedef _Atomic(long long) atomic_llong;
typedef _Atomic(unsigned long long) atomic_ullong;
typedef _Atomic(unsigned char) atomic_char8_t;
typedef _Atomic(uint_least16_t) atomic_char16_t;
typedef _Atomic(uint_least32_t) atomic_char32_t;
typedef _Atomic(wchar_t) atomic_wchar_t;
typedef _Atomic(int_least8_t) atomic_int_least8_t;
typedef _Atomic(uint_least8_t) atomic_uint_least8_t;
typedef _Atomic(int_least16_t) atomic_int_least16_t;
typedef _Atomic(uint_least16_t) atomic_uint_least16_t;
typedef _Atomic(int_least32_t) atomic_int_least32_t;
typedef _Atomic(uint_least32_t) atomic_uint_least32_t;
typedef _Atomic(int_least64_t) atomic_int_least64_t;
typedef _Atomic(uint_least64_t) atomic_uint_least64_t;
typedef _Atomic(int_fast8_t) atomic_int_fast8_t;
typedef _Atomic(uint_fast8_t) atomic_uint_fast8_t;
typedef _Atomic(int_fast16_t) atomic_int_fast16_t;
typedef _Atomic(uint_fast16_t) atomic_uint_fast16_t;
typedef _Atomic(int_fast32_t) atomic_int_fast32_t;
typedef _Atomic(uint_fast32_t) atomic_uint_fast32_t;
typedef _Atomic(int_fast64_t) atomic_int_fast64_t;
typedef _Atomic(uint_fast64_t) atomic_uint_fast64_t;
typedef _Atomic(intptr_t) atomic_intptr_t;
typedef _Atomic(uintptr_t) atomic_uintptr_t;
typedef _Atomic(size_t) atomic_size_t;
typedef _Atomic(ptrdiff_t) atomic_ptrdiff_t;
typedef _Atomic(intmax_t) atomic_intmax_t;
typedef _Atomic(uintmax_t) atomic_uintmax_t;

#endif

#endif