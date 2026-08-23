/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides implementations of the <stdatomic.h> interface.
    The implementations are best-effort, leaving many quirks of the interface
    unimplemented for some systems and compilers. This includes:

    - ATOMIC_VAR_INIT and ATOMIC_FLAG_INIT are not well supported and should be
      replaced with `atomic_init` and `pf_atomic_flag_init`.
    - Only integer atomic types are supported (no `_Atomic(struct example)`).
    - `_Atomic` must be used with parentheses, keyword version is not supported.
    - `memory_order` is ignored in threads fallback as of time of writing.
    - `kill_dependency`, `atomic_signal_fence` and `atomic_thread_fence` are not
      implemented on non-GNU platforms. Functions are replaced as no-op.

    If threads are available (<pf_threads.h>) the fallback implementation will
    lock atomic operations for all types. On Windows, some types will have true
    atomic operations (without locking) depending on availability.

    If threads are not available, PF_NO_ATOMICS will be defined and all
    functions will be replaced with no-ops to allow clean compilations.

    Last-updated: August 2026
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
#elif defined(PF_ATOMIC_GNUC)

    #include <stddef.h>
    #include <stdint.h>

    #define _Atomic(...) __VA_ARGS__

    #define ATOMIC_BOOL_LOCK_FREE (__atomic_always_lock_free(sizeof(_Bool), 0))
    #define ATOMIC_CHAR_LOCK_FREE (__atomic_always_lock_free(sizeof(char), 0))
    #define ATOMIC_CHAR16_T_LOCK_FREE                    \
        (__atomic_always_lock_free(sizeof(char16_t), 0))
    #define ATOMIC_CHAR32_T_LOCK_FREE                    \
        (__atomic_always_lock_free(sizeof(char32_t), 0))
    #define ATOMIC_WCHAR_T_LOCK_FREE                    \
        (__atomic_always_lock_free(sizeof(wchar_t), 0))
    #define ATOMIC_SHORT_LOCK_FREE (__atomic_always_lock_free(sizeof(short), 0))
    #define ATOMIC_INT_LOCK_FREE (__atomic_always_lock_free(sizeof(int), 0))
    #define ATOMIC_LONG_LOCK_FREE (__atomic_always_lock_free(sizeof(long), 0))
    #define ATOMIC_LLONG_LOCK_FREE                        \
        (__atomic_always_lock_free(sizeof(long long), 0))
    #define ATOMIC_POINTER_LOCK_FREE                   \
        (__atomic_always_lock_free(sizeof(void *), 0))
    #define ATOMIC_CHAR8_T_LOCK_FREE                    \
        (__atomic_always_lock_free(sizeof(char8_t), 0))

    #define atomic_is_lock_free(obj) __atomic_is_lock_free(sizeof(obj), 0)

    #define atomic_store(obj, desired)                       \
        __atomic_store_n(obj, desired, memory_order_seq_cst)
    #define atomic_store_explicit __atomic_store_n

    #define atomic_load(obj) __atomic_load_n(obj, memory_order_seq_cst)
    #define atomic_load_explicit __atomic_load_n

    #define atomic_exchange(obj, desired)                       \
        __atomic_exchange_n(obj, desired, memory_order_seq_cst)
    #define atomic_exchange_explicit(obj, desired, order) \
        __atomic_exchange_n(obj, desired, order)

    #define atomic_compare_exchange_strong(o, expect, desire)                \
        __atomic_compare_exchange_n(                                         \
            o, expect, desire, 0, memory_order_seq_cst, memory_order_seq_cst \
        )

    #define atomic_compare_exchange_weak(o, expect, desire)                  \
        __atomic_compare_exchange_n(                                         \
            o, expect, desire, 1, memory_order_seq_cst, memory_order_seq_cst \
        )

    #define atomic_compare_exchange_strong_explicit(                \
        o, expect, desire, ok, fail                                 \
    )                                                               \
        __atomic_compare_exchange_n(o, expect, desire, 0, ok, fail)

    #define atomic_compare_exchange_weak_explicit(o, expect, desire, ok, fail) \
        __atomic_compare_exchange_n(o, expect, desire, 1, ok, fail)

    #define atomic_fetch_add(obj, arg)                     \
        __atomic_fetch_add(obj, arg, memory_order_seq_cst)
    #define atomic_fetch_add_explicit(obj, arg, order) \
        __atomic_fetch_add_n(obj, arg, order)

    #define atomic_fetch_sub(obj, arg)                     \
        __atomic_fetch_sub(obj, arg, memory_order_seq_cst)
    #define atomic_fetch_sub_explicit(obj, arg, order) \
        __atomic_fetch_sub_n(obj, arg, order)

    #define atomic_fetch_or(obj, arg)                     \
        __atomic_fetch_or(obj, arg, memory_order_seq_cst)
    #define atomic_fetch_or_explicit(obj, arg, order) \
        __atomic_fetch_or_n(obj, arg, order)

    #define atomic_fetch_xor(obj, arg)                     \
        __atomic_fetch_xor(obj, arg, memory_order_seq_cst)
    #define atomic_fetch_xor_explicit(obj, arg, order) \
        __atomic_fetch_xor_n(obj, arg, order)

    #define atomic_fetch_and(obj, arg)                     \
        __atomic_fetch_and(obj, arg, memory_order_seq_cst)
    #define atomic_fetch_and_explicit(obj, arg, order) \
        __atomic_fetch_and_n(obj, arg, order)

typedef struct {
    char value;
} atomic_flag;

enum memory_order {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST,
};

    #define atomic_flag_test_and_set(obj)                \
        __atomic_test_and_set(obj, memory_order_seq_cst)
    #define atomic_flag_test_and_set_explicit __atomic_test_and_set
    #define atomic_flag_clear(obj) __atomic_clear(obj, memory_order_seq_cst)
    #define atomic_flag_clear_explicit __atomic_clear

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
    #include <stdint.h>

typedef struct pf_atomic_t {
    mtx_t lock;
    intmax_t value;
} pf_atomic_t;

    #define _Atomic(TYPE)     \
        union {               \
            pf_atomic_t base; \
            TYPE type;        \
        }

static inline void pf_atomic_init(pf_atomic_t *atomic, intmax_t desired);
static inline void pf_atomic_init_lock(mtx_t *lock);

static inline void pf_atomic_lock(mtx_t *lock);

static inline void pf_atomic_unlock(mtx_t *lock);

    #ifdef PF_ATOMIC_WIN32
        #define PF__IMPL_WIN32(FUNC, ...)                               \
            switch (size) {                                             \
            case sizeof(int8_t):                                        \
                return FUNC##8((CHAR *)&atomic->value, __VA_ARGS__);    \
            case sizeof(int16_t):                                       \
                return FUNC##16((SHORT *)&atomic->value, __VA_ARGS__);  \
            case sizeof(int32_t):                                       \
                return FUNC((LONG *)&atomic->value, __VA_ARGS__);       \
            case sizeof(int64_t):                                       \
                return FUNC##64((LONG64 *)&atomic->value, __VA_ARGS__); \
            default:                                                    \
                break;                                                  \
            }
    #else
        #define PF__IMPL_WIN32(FUNC, ...) (void)size
    #endif

    #define PF__IMPL_WIN32_add (void)size
    #define PF__IMPL_WIN32_sub (void)size
    #define PF__IMPL_WIN32_or PF__IMPL_WIN32(InterlockedOr, other)
    #define PF__IMPL_WIN32_xor PF__IMPL_WIN32(InterlockedXor, other)
    #define PF__IMPL_WIN32_and PF__IMPL_WIN32(InterlockedAnd, other)
    #define PF__IMPL_WIN32_xchg PF__IMPL_WIN32(InterlockedExchange, desired)
    #define PF__IMPL_WIN32_cas                                        \
        PF__IMPL_WIN32(InterlockedCompareExchange, expected, desired)

    #define PF__IMPL_ATOMIC_FETCH_OP(NAME, OPERATION, ...) \
        PF_API intmax_t pf_atomic_##NAME(                  \
            pf_atomic_t *atomic, size_t size, __VA_ARGS__  \
        ) {                                                \
            PF__IMPL_WIN32_##NAME;                         \
            intmax_t prev;                                 \
            pf_atomic_lock(&atomic->lock);                 \
            prev = atomic->value;                          \
            atomic->value = OPERATION;                     \
            pf_atomic_unlock(&atomic->lock);               \
            return prev;                                   \
        }

PF__IMPL_ATOMIC_FETCH_OP(add, prev + other, intmax_t other);
PF__IMPL_ATOMIC_FETCH_OP(sub, prev - other, intmax_t other);
PF__IMPL_ATOMIC_FETCH_OP(and, prev &other, intmax_t other);
PF__IMPL_ATOMIC_FETCH_OP(or, prev | other, intmax_t other);
PF__IMPL_ATOMIC_FETCH_OP(xor, prev ^ other, intmax_t other);
PF__IMPL_ATOMIC_FETCH_OP(xchg, desired, intmax_t desired);

PF_API intmax_t pf_atomic_load(pf_atomic_t *atomic) {
    intmax_t out;
    pf_atomic_lock(&atomic->lock);
    out = atomic->value;
    pf_atomic_unlock(&atomic->lock);
    return out;
}

PF_API void pf_atomic_store(pf_atomic_t *atomic, intmax_t value) {
    pf_atomic_lock(&atomic->lock);
    atomic->value = value;
    pf_atomic_unlock(&atomic->lock);
}

PF_API _Bool pf_atomic_cas_w(
    pf_atomic_t *atomic, size_t size, intmax_t *expected, intmax_t desired
) {
    _Bool result;

    pf_atomic_lock(&atomic->lock);
    result = atomic->value == *expected;
    *expected = atomic->value;

    if (result)
        atomic->value = desired;
    pf_atomic_unlock(&atomic->lock);
    return result;
}

PF_API _Bool pf_atomic_cas_s(
    pf_atomic_t *atomic, size_t size, intmax_t *expected, intmax_t desired
) {
    return pf_atomic_cas_w(atomic, size, expected, desired);
}

    #define PF__ATOMIC_UNWRAP(obj, FUNC, ...)                      \
        ((typeof((obj)->type))                                     \
             FUNC(&(obj)->base, sizeof((obj)->type), __VA_ARGS__))

    #define atomic_init(obj, desired) (pf_atomic_init(&(obj)->base, (desired)))
    #define atomic_load(obj) ((typeof((obj)->type))pf_atomic_load(&(obj)->base))
    #define atomic_store(obj, value) pf_atomic_store(&(obj)->base, (value))
    #define atomic_fetch_add(obj, other)               \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_add, (other))
    #define atomic_fetch_sub(obj, other)               \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_sub, (other))
    #define atomic_fetch_or(obj, other)               \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_or, (other))
    #define atomic_fetch_xor(obj, other)               \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_xor, (other))
    #define atomic_fetch_and(obj, other)               \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_and, (other))
    #define atomic_exchange(obj, desired)                 \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_xchg, (desired))
    #define atomic_compare_exchange_weak(obj, expected, desired)       \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_cas_w, (expected), (desired))
    #define atomic_compare_exchange_strong(obj, expected, desired)     \
        PF__ATOMIC_UNWRAP(obj, pf_atomic_cas_s, (expected), (desired))

    #define PF__ATOMIC_WIN32_TYPE(max)                                 \
        ((max) == INT8_MAX || (max) == INT16_MAX || (max) == INT32_MAX \
         || (max) == INT64_MAX)

    #ifdef PF_ATOMIC_WIN32
        #define PF_ATOMIC_LOCK_FREE(max) (PF__ATOMIC_WIN32_TYPE(max) ? 1 : 0)
    #else
        #define PF_ATOMIC_LOCK_FREE(max) 0
    #endif

    #define ATOMIC_BOOL_LOCK_FREE PF_ATOMIC_LOCK_FREE(1)
    #define ATOMIC_CHAR_LOCK_FREE PF_ATOMIC_LOCK_FREE(CHAR_MAX)
    #define ATOMIC_CHAR16_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(INT16_MAX)
    #define ATOMIC_CHAR32_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(INT32_MAX)
    #define ATOMIC_WCHAR_T_LOCK_FREE PF_ATOMIC_LOCK_FREE(WCHAR_MAX)
    #define ATOMIC_SHORT_LOCK_FREE PF_ATOMIC_LOCK_FREE(SHRT_MAX)
    #define ATOMIC_INT_LOCK_FREE PF_ATOMIC_LOCK_FREE(INT_MAX)
    #define ATOMIC_LONG_LOCK_FREE PF_ATOMIC_LOCK_FREE(LONG_MAX)
    #define ATOMIC_LLONG_LOCK_FREE PF_ATOMIC_LOCK_FREE(LLONG_MAX)
    #define ATOMIC_POINTER_LOCK_FREE PF_ATOMIC_LOCK_FREE(INTPTR_MAX)

    #define atomic_is_lock_free(obj) 0

    /* Use atomic_init instead of this macro. */
    #define ATOMIC_VAR_INIT(value) { 0 }

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
        obj, expected, desired, order                        \
    )                                                        \
        atomic_compare_exchange_weak(obj, expected, desired)
    #define atomic_compare_exchange_strong_explicit(           \
        obj, expected, desired, order                          \
    )                                                          \
        atomic_compare_exchange_strong(obj, expected, desired)

typedef struct {
    mtx_t lock;
    _Bool value;
} atomic_flag;

    #define ATOMIC_FLAG_INIT { 0 }
    #define PF_HAS_ATOMIC_FLAG_INIT

PF_API void pf_atomic_flag_init(volatile atomic_flag *obj) {
    pf_atomic_init_lock(&obj->lock);
    obj->value = 0;
}

PF_API _Bool atomic_flag_test_and_set(volatile atomic_flag *obj) {
    pf_atomic_lock(&obj->lock);
    _Bool prev = obj->value;
    obj->value = 1;
    pf_atomic_unlock(&obj->lock);
    return prev;
}

PF_API _Bool atomic_flag_test_and_set_explicit(
    volatile atomic_flag *obj, memory_order order
) {
    return atomic_flag_test_and_set(obj);
}

PF_API void atomic_flag_clear(volatile atomic_flag *obj) {
    pf_atomic_lock(&obj->lock);
    obj->value = 0;
    pf_atomic_unlock(&obj->lock);
}

PF_API void atomic_flag_clear_explicit(
    volatile atomic_flag *obj, memory_order order
) {
    return atomic_flag_clear(obj);
}

    #define kill_dependency(y)
    #define atomic_thread_fence(order)
    #define atomic_signal_fence(order)
#else

    /* Fallback implementation with no threads. These macros allow for code to
   compile cleanly, but the user should still check for PF_NO_ATOMICS macro.
*/

    #define ATOMIC_BOOL_LOCK_FREE -1
    #define ATOMIC_CHAR_LOCK_FREE -1
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
    #define kill_dependency(...)
    #define atomic_thread_fence(...)
    #define atomic_signal_fence(...)

    #define ATOMIC_FLAG_INIT { 0 }
    #define ATOMIC_VAR_INIT(value) { 0 }
typedef struct {
    char unused;
} atomic_flag;

#endif

#ifndef PF_HAS_ATOMIC_FLAG_INIT
PF_API void pf_atomic_flag_init(volatile atomic_flag *obj) { (void)obj; }
#endif

#ifndef PF_ATOMIC_STD
    #include <stddef.h>
    #include <stdint.h>
    #include <uchar.h>
    #include <wchar.h>

    #if defined(__cplusplus) || defined(bool)
typedef bool _Bool;
    #elif !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
typedef int _Bool;
    #endif

    #if __STDC_VERSION__ < 202311L
typedef uint8_t char8_t;
    #endif

    #ifndef PF_ATOMIC_GNUC
enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};
    #endif

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
