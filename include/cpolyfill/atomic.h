/** cpolyfill - Filling the gaps between C standards and compilers

    C11 atomics for every compiler, and waiting on an atomic value.

        PF_ATOMIC(T)            An atomic T. Without <stdatomic.h> it has
                                the size of T, so a plain initializer works:
                                `PF_ATOMIC(int) n = 0;`
        pf_memory_order         PF_MEMORY_ORDER_RELAXED, _CONSUME, _ACQUIRE,
                                _RELEASE, _ACQ_REL, _SEQ_CST
        pf_atomic_flag_t        PF_ATOMIC_FLAG_INIT

        PF_ATOMIC_INIT(obj, value)              (not atomic)
        PF_ATOMIC_LOAD(obj)
        PF_ATOMIC_STORE(obj, value)
        PF_ATOMIC_EXCHANGE(obj, value)
        PF_ATOMIC_COMPARE_EXCHANGE_STRONG(obj, expected, desired)
        PF_ATOMIC_COMPARE_EXCHANGE_WEAK(obj, expected, desired)
        PF_ATOMIC_FETCH_ADD / _SUB / _OR / _XOR / _AND(obj, arg)
        PF_ATOMIC_FLAG_TEST_AND_SET(flag)
        PF_ATOMIC_FLAG_CLEAR(flag)
                The C11 operations on a pointer to an atomic object, each
                also as `..._EXPLICIT` with memory order arguments last.
                FETCH_ADD / _SUB scale by the pointed-to size for atomic
                pointers.
        PF_ATOMIC_THREAD_FENCE(order)
        PF_ATOMIC_SIGNAL_FENCE(order)
        PF_KILL_DEPENDENCY(y)
        PF_ATOMIC_IS_LOCK_FREE(obj)
        PF_ATOMIC_BOOL_LOCK_FREE, _CHAR_, _CHAR16_T_, _CHAR32_T_,
        _WCHAR_T_, _SHORT_, _INT_, _LONG_, _LLONG_, _POINTER_LOCK_FREE
                0 never, 1 sometimes, 2 always lock-free.

        PF_HAS_ATOMICS          1 if the macros above are available.

    The implementation depends on the compiler including the header:

        C11 with <stdatomic.h>  the standard header
        C++                     std::atomic (PF_ATOMIC(T) is
                                std::atomic<T>, with the same layout in
                                practice)
        GCC, Clang before C11   the __atomic builtins; types that are not
                                lock-free need libatomic, which the
                                cpolyfill dependency links when found
        MSVC C                  Interlocked intrinsics for 1, 2, 4 and 8
                                bytes, the library's lock table for other
                                sizes; needs __typeof__ (Visual Studio 2022
                                17.9)
        others with typeof      calls into the library

    In the last two, the macros may evaluate `obj` more than once, every
    operation is sequentially consistent, and the weak compare-exchange
    never fails spuriously.

    Waiting on an atomic, like C++20 atomic::wait (process-private):

        int pf_atomic_wait(const volatile void *addr, uint32_t expected,
                           const struct timespec *timeout)
                Blocks while the 32-bit atomic at `addr` holds `expected`,
                until woken or until the relative `timeout` (NULL: none)
                passes. Returns 0 if woken or the value differs (wakeups
                may be spurious: check the value again), PF_ETIMEDOUT, or
                PF_EDEADLK without threads when nothing could wake it.
        void pf_atomic_wake_one(const volatile void *addr)
        void pf_atomic_wake_all(const volatile void *addr)
                Wake one or all threads waiting on `addr`. Change the value
                first.

    Standard names, with PF_STD_NAMES or PF_STD_NAMES_ATOMIC, when the
    compiler has no <stdatomic.h>: `_Atomic(T)`, `memory_order` and its
    constants, `atomic_flag`, `ATOMIC_FLAG_INIT`, the `atomic_*`
    operations, `kill_dependency`, the `ATOMIC_*_LOCK_FREE` macros and the
    `atomic_int`... typedefs (C only). In C++ the std:: names are brought
    in with using-declarations. `ATOMIC_VAR_INIT` is not provided (removed
    in C23): use a plain initializer.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_ATOMIC_H
#define CPOLYFILL_ATOMIC_H

#include <cpolyfill/platform.h>
#include <cpolyfill/time.h>

#include <stddef.h>
#include <stdint.h>

/* ---- Choice of implementation ----------------------------------------- */

#define PF__ATOMIC_STD 0
#define PF__ATOMIC_CXX 0
#define PF__ATOMIC_GNU 0
#define PF__ATOMIC_MSVC 0
#define PF__ATOMIC_LIB 0

#if defined(__cplusplus)
    #undef PF__ATOMIC_CXX
    #define PF__ATOMIC_CXX 1
#elif defined(PF__ATOMIC_FORCE_LOCKED) /* internal, for the tests */
    #include <cpolyfill/stdc.h>
    #undef PF__ATOMIC_LIB
    #define PF__ATOMIC_LIB 1
#elif PF_STD_C11 && !defined(__STDC_NO_ATOMICS__)
    #undef PF__ATOMIC_STD
    #define PF__ATOMIC_STD 1
#elif PF_CC_GNU
    #undef PF__ATOMIC_GNU
    #define PF__ATOMIC_GNU 1
#elif PF_CC_MSVC
    #undef PF__ATOMIC_MSVC
    #define PF__ATOMIC_MSVC 1
#else
    #include <cpolyfill/stdc.h>
    #if PF_HAS_TYPEOF
        #undef PF__ATOMIC_LIB
        #define PF__ATOMIC_LIB 1
    #endif
#endif

#if PF__ATOMIC_STD || PF__ATOMIC_CXX || PF__ATOMIC_GNU || PF__ATOMIC_MSVC \
    || PF__ATOMIC_LIB
    #define PF_HAS_ATOMICS 1
#else
    #define PF_HAS_ATOMICS 0
#endif

#if PF__ATOMIC_CXX
    #include <atomic>
#elif PF__ATOMIC_STD
    #include <stdatomic.h>
#elif PF__ATOMIC_MSVC
    #include <intrin.h>
    #include <string.h>
#endif

#if !defined(__cplusplus)
    #include <stdbool.h>
#endif

/* ---- Library functions ------------------------------------------------ */

PF_BEGIN_DECLS

PF_API int pf_atomic_wait(
    const volatile void *addr, uint32_t expected, const struct timespec *timeout
);
PF_API void pf_atomic_wake_one(const volatile void *addr);
PF_API void pf_atomic_wake_all(const volatile void *addr);

/* Backend of the MSVC and library implementations. Values travel by
   pointer, so every type works: lock-free where the library's compiler
   can, else under a striped lock table. The fetch operations take integers
   and pointers of 1, 2, 4 or 8 bytes only. */
enum {
    PF__ATOMIC_OP_ADD,
    PF__ATOMIC_OP_SUB,
    PF__ATOMIC_OP_OR,
    PF__ATOMIC_OP_XOR,
    PF__ATOMIC_OP_AND
};

PF_API void *pf__atomic_load(const volatile void *obj, size_t size, void *ret);
PF_API void pf__atomic_store(volatile void *obj, size_t size, const void *val);
PF_API void *pf__atomic_exchange(
    volatile void *obj, size_t size, const void *val, void *ret
);
PF_API bool pf__atomic_cas(
    volatile void *obj, size_t size, void *expected, const void *desired
);
PF_API void *pf__atomic_fetch(
    volatile void *obj, size_t size, int op, unsigned long long arg, void *ret
);
PF_API bool pf__atomic_is_lock_free(size_t size);
PF_API void pf__atomic_fence(void);

PF_END_DECLS

/* GNU: atomic pointers step by the size of what they point to. GCC's own
   <stdatomic.h> does not scale them (GCC bug 64843), Clang's does. */
#if PF_CC_GNU
    #define PF__ATOMIC_GNU_STEP(obj, arg)                                     \
        ((arg)                                                                \
         * (__builtin_classify_type(*(obj)) == 5                              \
                ? (ptrdiff_t)((uintptr_t)((__typeof__(*(obj)))(uintptr_t)4096 \
                                          + 1)                                \
                              - 4096u)                                        \
                : (ptrdiff_t)1))
#endif

/* ---- C11 <stdatomic.h> ------------------------------------------------ */

#if PF__ATOMIC_STD

    #if PF_CC_GCC
        #define PF__ATOMIC_STD_STEP(obj, arg) PF__ATOMIC_GNU_STEP(obj, arg)
    #else
        #define PF__ATOMIC_STD_STEP(obj, arg) (arg)
    #endif

    #define PF_ATOMIC(T) _Atomic(T)
typedef memory_order pf_memory_order;
    #define PF_MEMORY_ORDER_RELAXED memory_order_relaxed
    #define PF_MEMORY_ORDER_CONSUME memory_order_consume
    #define PF_MEMORY_ORDER_ACQUIRE memory_order_acquire
    #define PF_MEMORY_ORDER_RELEASE memory_order_release
    #define PF_MEMORY_ORDER_ACQ_REL memory_order_acq_rel
    #define PF_MEMORY_ORDER_SEQ_CST memory_order_seq_cst
typedef atomic_flag pf_atomic_flag_t;
    #define PF_ATOMIC_FLAG_INIT ATOMIC_FLAG_INIT

    #define PF_ATOMIC_INIT(obj, v) atomic_init(obj, v)
    #define PF_ATOMIC_LOAD(obj) atomic_load(obj)
    #define PF_ATOMIC_STORE(obj, v) atomic_store(obj, v)
    #define PF_ATOMIC_EXCHANGE(obj, v) atomic_exchange(obj, v)
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG(obj, e, d) \
        atomic_compare_exchange_strong(obj, e, d)
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK(obj, e, d) \
        atomic_compare_exchange_weak(obj, e, d)
    #define PF_ATOMIC_FETCH_ADD(obj, v)                    \
        atomic_fetch_add(obj, PF__ATOMIC_STD_STEP(obj, v))
    #define PF_ATOMIC_FETCH_SUB(obj, v)                    \
        atomic_fetch_sub(obj, PF__ATOMIC_STD_STEP(obj, v))
    #define PF_ATOMIC_FETCH_OR(obj, v) atomic_fetch_or(obj, v)
    #define PF_ATOMIC_FETCH_XOR(obj, v) atomic_fetch_xor(obj, v)
    #define PF_ATOMIC_FETCH_AND(obj, v) atomic_fetch_and(obj, v)
    #define PF_ATOMIC_FLAG_TEST_AND_SET(f) atomic_flag_test_and_set(f)
    #define PF_ATOMIC_FLAG_CLEAR(f) atomic_flag_clear(f)

    #define PF_ATOMIC_LOAD_EXPLICIT(obj, mo) atomic_load_explicit(obj, mo)
    #define PF_ATOMIC_STORE_EXPLICIT(obj, v, mo) \
        atomic_store_explicit(obj, v, mo)
    #define PF_ATOMIC_EXCHANGE_EXPLICIT(obj, v, mo) \
        atomic_exchange_explicit(obj, v, mo)
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(obj, e, d, s, f) \
        atomic_compare_exchange_strong_explicit(obj, e, d, s, f)
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT(obj, e, d, s, f) \
        atomic_compare_exchange_weak_explicit(obj, e, d, s, f)
    #define PF_ATOMIC_FETCH_ADD_EXPLICIT(obj, v, mo)                    \
        atomic_fetch_add_explicit(obj, PF__ATOMIC_STD_STEP(obj, v), mo)
    #define PF_ATOMIC_FETCH_SUB_EXPLICIT(obj, v, mo)                    \
        atomic_fetch_sub_explicit(obj, PF__ATOMIC_STD_STEP(obj, v), mo)
    #define PF_ATOMIC_FETCH_OR_EXPLICIT(obj, v, mo) \
        atomic_fetch_or_explicit(obj, v, mo)
    #define PF_ATOMIC_FETCH_XOR_EXPLICIT(obj, v, mo) \
        atomic_fetch_xor_explicit(obj, v, mo)
    #define PF_ATOMIC_FETCH_AND_EXPLICIT(obj, v, mo) \
        atomic_fetch_and_explicit(obj, v, mo)
    #define PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT(f, mo) \
        atomic_flag_test_and_set_explicit(f, mo)
    #define PF_ATOMIC_FLAG_CLEAR_EXPLICIT(f, mo) \
        atomic_flag_clear_explicit(f, mo)

    #define PF_ATOMIC_THREAD_FENCE(mo) atomic_thread_fence(mo)
    #define PF_ATOMIC_SIGNAL_FENCE(mo) atomic_signal_fence(mo)
    #define PF_KILL_DEPENDENCY(y) kill_dependency(y)
    #define PF_ATOMIC_IS_LOCK_FREE(obj) atomic_is_lock_free(obj)

    #define PF_ATOMIC_BOOL_LOCK_FREE ATOMIC_BOOL_LOCK_FREE
    #define PF_ATOMIC_CHAR_LOCK_FREE ATOMIC_CHAR_LOCK_FREE
    #define PF_ATOMIC_CHAR16_T_LOCK_FREE ATOMIC_CHAR16_T_LOCK_FREE
    #define PF_ATOMIC_CHAR32_T_LOCK_FREE ATOMIC_CHAR32_T_LOCK_FREE
    #define PF_ATOMIC_WCHAR_T_LOCK_FREE ATOMIC_WCHAR_T_LOCK_FREE
    #define PF_ATOMIC_SHORT_LOCK_FREE ATOMIC_SHORT_LOCK_FREE
    #define PF_ATOMIC_INT_LOCK_FREE ATOMIC_INT_LOCK_FREE
    #define PF_ATOMIC_LONG_LOCK_FREE ATOMIC_LONG_LOCK_FREE
    #define PF_ATOMIC_LLONG_LOCK_FREE ATOMIC_LLONG_LOCK_FREE
    #define PF_ATOMIC_POINTER_LOCK_FREE ATOMIC_POINTER_LOCK_FREE

    /* ---- C++ std::atomic -------------------------------------------------- */

#elif PF__ATOMIC_CXX

    #define PF_ATOMIC(T) std::atomic<T>
typedef std::memory_order pf_memory_order;
    #define PF_MEMORY_ORDER_RELAXED std::memory_order_relaxed
    #define PF_MEMORY_ORDER_CONSUME std::memory_order_consume
    #define PF_MEMORY_ORDER_ACQUIRE std::memory_order_acquire
    #define PF_MEMORY_ORDER_RELEASE std::memory_order_release
    #define PF_MEMORY_ORDER_ACQ_REL std::memory_order_acq_rel
    #define PF_MEMORY_ORDER_SEQ_CST std::memory_order_seq_cst
typedef std::atomic_flag pf_atomic_flag_t;
    #define PF_ATOMIC_FLAG_INIT ATOMIC_FLAG_INIT

    #define PF_ATOMIC_INIT(obj, v)                     \
        ((obj)->store((v), std::memory_order_relaxed))
    #define PF_ATOMIC_LOAD(obj) std::atomic_load(obj)
    #define PF_ATOMIC_STORE(obj, v) std::atomic_store(obj, v)
    #define PF_ATOMIC_EXCHANGE(obj, v) std::atomic_exchange(obj, v)
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG(obj, e, d) \
        std::atomic_compare_exchange_strong(obj, e, d)
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK(obj, e, d) \
        std::atomic_compare_exchange_weak(obj, e, d)
    #define PF_ATOMIC_FETCH_ADD(obj, v) std::atomic_fetch_add(obj, v)
    #define PF_ATOMIC_FETCH_SUB(obj, v) std::atomic_fetch_sub(obj, v)
    #define PF_ATOMIC_FETCH_OR(obj, v) std::atomic_fetch_or(obj, v)
    #define PF_ATOMIC_FETCH_XOR(obj, v) std::atomic_fetch_xor(obj, v)
    #define PF_ATOMIC_FETCH_AND(obj, v) std::atomic_fetch_and(obj, v)
    #define PF_ATOMIC_FLAG_TEST_AND_SET(f) std::atomic_flag_test_and_set(f)
    #define PF_ATOMIC_FLAG_CLEAR(f) std::atomic_flag_clear(f)

    #define PF_ATOMIC_LOAD_EXPLICIT(obj, mo) std::atomic_load_explicit(obj, mo)
    #define PF_ATOMIC_STORE_EXPLICIT(obj, v, mo) \
        std::atomic_store_explicit(obj, v, mo)
    #define PF_ATOMIC_EXCHANGE_EXPLICIT(obj, v, mo) \
        std::atomic_exchange_explicit(obj, v, mo)
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(obj, e, d, s, f) \
        std::atomic_compare_exchange_strong_explicit(obj, e, d, s, f)
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT(obj, e, d, s, f) \
        std::atomic_compare_exchange_weak_explicit(obj, e, d, s, f)
    #define PF_ATOMIC_FETCH_ADD_EXPLICIT(obj, v, mo) \
        std::atomic_fetch_add_explicit(obj, v, mo)
    #define PF_ATOMIC_FETCH_SUB_EXPLICIT(obj, v, mo) \
        std::atomic_fetch_sub_explicit(obj, v, mo)
    #define PF_ATOMIC_FETCH_OR_EXPLICIT(obj, v, mo) \
        std::atomic_fetch_or_explicit(obj, v, mo)
    #define PF_ATOMIC_FETCH_XOR_EXPLICIT(obj, v, mo) \
        std::atomic_fetch_xor_explicit(obj, v, mo)
    #define PF_ATOMIC_FETCH_AND_EXPLICIT(obj, v, mo) \
        std::atomic_fetch_and_explicit(obj, v, mo)
    #define PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT(f, mo) \
        std::atomic_flag_test_and_set_explicit(f, mo)
    #define PF_ATOMIC_FLAG_CLEAR_EXPLICIT(f, mo) \
        std::atomic_flag_clear_explicit(f, mo)

    #define PF_ATOMIC_THREAD_FENCE(mo) std::atomic_thread_fence(mo)
    #define PF_ATOMIC_SIGNAL_FENCE(mo) std::atomic_signal_fence(mo)
    #define PF_KILL_DEPENDENCY(y) std::kill_dependency(y)
    #define PF_ATOMIC_IS_LOCK_FREE(obj) std::atomic_is_lock_free(obj)

    #define PF_ATOMIC_BOOL_LOCK_FREE ATOMIC_BOOL_LOCK_FREE
    #define PF_ATOMIC_CHAR_LOCK_FREE ATOMIC_CHAR_LOCK_FREE
    #define PF_ATOMIC_CHAR16_T_LOCK_FREE ATOMIC_CHAR16_T_LOCK_FREE
    #define PF_ATOMIC_CHAR32_T_LOCK_FREE ATOMIC_CHAR32_T_LOCK_FREE
    #define PF_ATOMIC_WCHAR_T_LOCK_FREE ATOMIC_WCHAR_T_LOCK_FREE
    #define PF_ATOMIC_SHORT_LOCK_FREE ATOMIC_SHORT_LOCK_FREE
    #define PF_ATOMIC_INT_LOCK_FREE ATOMIC_INT_LOCK_FREE
    #define PF_ATOMIC_LONG_LOCK_FREE ATOMIC_LONG_LOCK_FREE
    #define PF_ATOMIC_LLONG_LOCK_FREE ATOMIC_LLONG_LOCK_FREE
    #define PF_ATOMIC_POINTER_LOCK_FREE ATOMIC_POINTER_LOCK_FREE

    /* ---- GNU __atomic builtins -------------------------------------------- */

#elif PF__ATOMIC_GNU

    /* The alignment C11 gives _Atomic(T): the size, for the sizes the
   hardware can operate on. */
    #define PF__ATOMIC_ALIGN(T)                                \
        ((sizeof(T) & (sizeof(T) - 1)) == 0 && sizeof(T) <= 16 \
             ? sizeof(T)                                       \
             : __alignof__(T))
    #define PF_ATOMIC(T)                                            \
        __typeof__(T) __attribute__((aligned(PF__ATOMIC_ALIGN(T))))

typedef int pf_memory_order;
    #define PF_MEMORY_ORDER_RELAXED __ATOMIC_RELAXED
    #define PF_MEMORY_ORDER_CONSUME __ATOMIC_CONSUME
    #define PF_MEMORY_ORDER_ACQUIRE __ATOMIC_ACQUIRE
    #define PF_MEMORY_ORDER_RELEASE __ATOMIC_RELEASE
    #define PF_MEMORY_ORDER_ACQ_REL __ATOMIC_ACQ_REL
    #define PF_MEMORY_ORDER_SEQ_CST __ATOMIC_SEQ_CST

typedef struct pf_atomic_flag_t {
    unsigned char pf__value;
} pf_atomic_flag_t;
    #define PF_ATOMIC_FLAG_INIT { 0 }

    #define PF_ATOMIC_INIT(obj, v) ((void)(*(obj) = (v)))
    #define PF_ATOMIC_LOAD_EXPLICIT(obj, mo)    \
        __extension__({                         \
            __typeof__(*(obj)) pf__r;           \
            __atomic_load((obj), &pf__r, (mo)); \
            pf__r;                              \
        })
    #define PF_ATOMIC_STORE_EXPLICIT(obj, v, mo) \
        __extension__({                          \
            __typeof__(*(obj)) pf__v = (v);      \
            __atomic_store((obj), &pf__v, (mo)); \
        })
    #define PF_ATOMIC_EXCHANGE_EXPLICIT(obj, v, mo)         \
        __extension__({                                     \
            __typeof__(*(obj)) pf__v = (v), pf__r;          \
            __atomic_exchange((obj), &pf__v, &pf__r, (mo)); \
            pf__r;                                          \
        })
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(obj, e, d, s, f) \
        __extension__({                                                 \
            __typeof__(*(obj)) pf__d = (d);                             \
            __atomic_compare_exchange((obj), (e), &pf__d, 0, (s), (f)); \
        })
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT(obj, e, d, s, f)   \
        __extension__({                                                 \
            __typeof__(*(obj)) pf__d = (d);                             \
            __atomic_compare_exchange((obj), (e), &pf__d, 1, (s), (f)); \
        })
    #define PF_ATOMIC_FETCH_ADD_EXPLICIT(obj, v, mo)                 \
        __atomic_fetch_add((obj), PF__ATOMIC_GNU_STEP(obj, v), (mo))
    #define PF_ATOMIC_FETCH_SUB_EXPLICIT(obj, v, mo)                 \
        __atomic_fetch_sub((obj), PF__ATOMIC_GNU_STEP(obj, v), (mo))
    #define PF_ATOMIC_FETCH_OR_EXPLICIT(obj, v, mo) \
        __atomic_fetch_or((obj), (v), (mo))
    #define PF_ATOMIC_FETCH_XOR_EXPLICIT(obj, v, mo) \
        __atomic_fetch_xor((obj), (v), (mo))
    #define PF_ATOMIC_FETCH_AND_EXPLICIT(obj, v, mo) \
        __atomic_fetch_and((obj), (v), (mo))
    #define PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT(f, mo) \
        __atomic_test_and_set(&(f)->pf__value, (mo))
    #define PF_ATOMIC_FLAG_CLEAR_EXPLICIT(f, mo) \
        __atomic_clear(&(f)->pf__value, (mo))

    #define PF_ATOMIC_THREAD_FENCE(mo) __atomic_thread_fence(mo)
    #define PF_ATOMIC_SIGNAL_FENCE(mo) __atomic_signal_fence(mo)
    #define PF_KILL_DEPENDENCY(y) (y)
    #define PF_ATOMIC_IS_LOCK_FREE(obj)              \
        __atomic_is_lock_free(sizeof(*(obj)), (obj))

    #define PF_ATOMIC_BOOL_LOCK_FREE __GCC_ATOMIC_BOOL_LOCK_FREE
    #define PF_ATOMIC_CHAR_LOCK_FREE __GCC_ATOMIC_CHAR_LOCK_FREE
    #define PF_ATOMIC_CHAR16_T_LOCK_FREE __GCC_ATOMIC_CHAR16_T_LOCK_FREE
    #define PF_ATOMIC_CHAR32_T_LOCK_FREE __GCC_ATOMIC_CHAR32_T_LOCK_FREE
    #define PF_ATOMIC_WCHAR_T_LOCK_FREE __GCC_ATOMIC_WCHAR_T_LOCK_FREE
    #define PF_ATOMIC_SHORT_LOCK_FREE __GCC_ATOMIC_SHORT_LOCK_FREE
    #define PF_ATOMIC_INT_LOCK_FREE __GCC_ATOMIC_INT_LOCK_FREE
    #define PF_ATOMIC_LONG_LOCK_FREE __GCC_ATOMIC_LONG_LOCK_FREE
    #define PF_ATOMIC_LLONG_LOCK_FREE __GCC_ATOMIC_LLONG_LOCK_FREE
    #define PF_ATOMIC_POINTER_LOCK_FREE __GCC_ATOMIC_POINTER_LOCK_FREE

    /* ---- Integers through functions: MSVC intrinsics or the library ------- */

#elif PF__ATOMIC_MSVC || PF__ATOMIC_LIB

    #if PF__ATOMIC_MSVC

/* Objects of 1, 2, 4 or 8 bytes use compare-exchange loops on the
   intrinsics, which every Windows architecture has; others go to the
   library. Full barriers, so every order is sequentially consistent. */
PF_INLINE unsigned long long pf__atomic_msvc_cas_raw(
    volatile void *p,
    size_t size,
    unsigned long long expect,
    unsigned long long desire
) {
    switch (size) {
    case 1:
        return (unsigned char)_InterlockedCompareExchange8(
            (volatile char *)p, (char)desire, (char)expect
        );
    case 2:
        return (unsigned short)_InterlockedCompareExchange16(
            (volatile short *)p, (short)desire, (short)expect
        );
    case 4:
        return (unsigned long)_InterlockedCompareExchange(
            (volatile long *)p, (long)desire, (long)expect
        );
    default:
        return (unsigned long long)_InterlockedCompareExchange64(
            (volatile long long *)p, (long long)desire, (long long)expect
        );
    }
}

PF_INLINE bool pf__atomic_msvc_is_lock_free(size_t size) {
    return size == 1 || size == 2 || size == 4 || size == 8;
}

/* Windows is little-endian: the value is in the low bytes. */
PF_INLINE unsigned long long pf__atomic_msvc_get(const void *p, size_t size) {
    unsigned long long v = 0;
    memcpy(&v, p, size);
    return v;
}

PF_INLINE void *pf__atomic_msvc_load(
    const volatile void *obj, size_t size, void *ret
) {
    unsigned long long v;
    if (!pf__atomic_msvc_is_lock_free(size))
        return pf__atomic_load(obj, size, ret);
    v = pf__atomic_msvc_cas_raw((volatile void *)obj, size, 0, 0);
    memcpy(ret, &v, size);
    return ret;
}

PF_INLINE void *pf__atomic_msvc_exchange(
    volatile void *obj, size_t size, const void *val, void *ret
) {
    unsigned long long old, seen, v;
    if (!pf__atomic_msvc_is_lock_free(size))
        return pf__atomic_exchange(obj, size, val, ret);
    v = pf__atomic_msvc_get(val, size);
    old = pf__atomic_msvc_cas_raw(obj, size, 0, 0);
    while ((seen = pf__atomic_msvc_cas_raw(obj, size, old, v)) != old)
        old = seen;
    memcpy(ret, &old, size);
    return ret;
}

PF_INLINE void pf__atomic_msvc_store(
    volatile void *obj, size_t size, const void *val
) {
    unsigned long long ignored;
    if (!pf__atomic_msvc_is_lock_free(size))
        pf__atomic_store(obj, size, val);
    else
        (void)pf__atomic_msvc_exchange(obj, size, val, &ignored);
}

PF_INLINE bool pf__atomic_msvc_cas(
    volatile void *obj, size_t size, void *expected, const void *desired
) {
    unsigned long long e, seen;
    if (!pf__atomic_msvc_is_lock_free(size))
        return pf__atomic_cas(obj, size, expected, desired);
    e = pf__atomic_msvc_get(expected, size);
    seen = pf__atomic_msvc_cas_raw(
        obj, size, e, pf__atomic_msvc_get(desired, size)
    );
    if (seen == e)
        return true;
    memcpy(expected, &seen, size);
    return false;
}

PF_INLINE void *pf__atomic_msvc_fetch(
    volatile void *obj, size_t size, int op, unsigned long long arg, void *ret
) {
    unsigned long long mask = size >= 8 ? ~0ULL : (1ULL << (size * 8)) - 1;
    unsigned long long old = pf__atomic_msvc_cas_raw(obj, size, 0, 0), v, seen;
    for (;;) {
        switch (op) {
        case PF__ATOMIC_OP_ADD:
            v = old + arg;
            break;
        case PF__ATOMIC_OP_SUB:
            v = old - arg;
            break;
        case PF__ATOMIC_OP_OR:
            v = old | arg;
            break;
        case PF__ATOMIC_OP_XOR:
            v = old ^ arg;
            break;
        default:
            v = old & arg;
            break;
        }
        seen = pf__atomic_msvc_cas_raw(obj, size, old, v & mask);
        if (seen == old)
            break;
        old = seen;
    }
    memcpy(ret, &old, size);
    return ret;
}

PF_INLINE void pf__atomic_msvc_fence(void) {
        #if defined(_M_ARM64) || defined(_M_ARM) || defined(_M_ARM64EC)
    __dmb(0xB); /* ISH */
        #else
    _mm_mfence();
        #endif
}

        #define PF__ATOMIC_FN(name) pf__atomic_msvc_##name
        #define PF__ATOMIC_ALWAYS_LOCK_FREE 2
        #define PF__ATOMIC_TYPEOF(x) __typeof__(x)
    #else
        #define PF__ATOMIC_FN(name) pf__atomic_##name
        #define PF__ATOMIC_ALWAYS_LOCK_FREE 1
        #define PF__ATOMIC_TYPEOF(x) PF_TYPEOF(x)
    #endif

    #define PF_ATOMIC(T) PF__ATOMIC_TYPEOF(T)

typedef int pf_memory_order;
    #define PF_MEMORY_ORDER_RELAXED 0
    #define PF_MEMORY_ORDER_CONSUME 1
    #define PF_MEMORY_ORDER_ACQUIRE 2
    #define PF_MEMORY_ORDER_RELEASE 3
    #define PF_MEMORY_ORDER_ACQ_REL 4
    #define PF_MEMORY_ORDER_SEQ_CST 5

typedef struct pf_atomic_flag_t {
    unsigned char pf__value;
} pf_atomic_flag_t;
    #define PF_ATOMIC_FLAG_INIT { 0 }

    /* A temporary of the object's type holding `v`, as a pointer. An array
       compound literal, so a structure value can initialize it. */
    #define PF__ATOMIC_VAL(obj, v) ((PF__ATOMIC_TYPEOF(*(obj))[1]) { (v) })
    /* An uninitialized one, for results (the char member takes the zero, so
       any type works without brace warnings). */
    #define PF__ATOMIC_TMP(obj)                \
        (&(union {                             \
              char pf__c;                      \
              PF__ATOMIC_TYPEOF(*(obj)) pf__v; \
          }) { 0 }                             \
              .pf__v)
    #define PF__ATOMIC_DEREF(obj, p) (*(PF__ATOMIC_TYPEOF(*(obj)) *)(p))
    /* Fetch arguments travel as unsigned long long; the arithmetic is
       modulo the size of the object, so signs and pointer steps work. */
    #define PF__ATOMIC_STEP(obj, arg)                                    \
        ((unsigned long long)(arg)                                       \
         * (unsigned long long)((uintptr_t)((PF__ATOMIC_TYPEOF(*(obj)))( \
                                                uintptr_t                \
                                            )4096                        \
                                            + 1)                         \
                                - 4096u))

    #define PF_ATOMIC_INIT(obj, v) ((void)(*(obj) = (v)))
    #define PF_ATOMIC_LOAD_EXPLICIT(obj, mo)                                 \
        ((void)(mo),                                                         \
         PF__ATOMIC_DEREF(                                                   \
             obj,                                                            \
             PF__ATOMIC_FN(load)((obj), sizeof(*(obj)), PF__ATOMIC_TMP(obj)) \
         ))
    #define PF_ATOMIC_STORE_EXPLICIT(obj, v, mo)                              \
        ((void)(mo),                                                          \
         PF__ATOMIC_FN(store)((obj), sizeof(*(obj)), PF__ATOMIC_VAL(obj, v)))
    #define PF_ATOMIC_EXCHANGE_EXPLICIT(obj, v, mo) \
        ((void)(mo),                                \
         PF__ATOMIC_DEREF(                          \
             obj,                                   \
             PF__ATOMIC_FN(exchange)(               \
                 (obj),                             \
                 sizeof(*(obj)),                    \
                 PF__ATOMIC_VAL(obj, v),            \
                 PF__ATOMIC_TMP(obj)                \
             )                                      \
         ))
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(obj, e, d, s, f) \
        ((void)(s),                                                     \
         (void)(f),                                                     \
         PF__ATOMIC_FN(cas)(                                            \
             (obj), sizeof(*(obj)), (e), PF__ATOMIC_VAL(obj, d)         \
         ))
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT(obj, e, d, s, f) \
        PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(obj, e, d, s, f)
    #define PF__ATOMIC_FETCH(obj, op, arg, mo)                           \
        ((void)(mo),                                                     \
         PF__ATOMIC_DEREF(                                               \
             obj,                                                        \
             PF__ATOMIC_FN(fetch)(                                       \
                 (obj), sizeof(*(obj)), (op), (arg), PF__ATOMIC_TMP(obj) \
             )                                                           \
         ))
    #define PF_ATOMIC_FETCH_ADD_EXPLICIT(obj, v, mo)                          \
        PF__ATOMIC_FETCH(obj, PF__ATOMIC_OP_ADD, PF__ATOMIC_STEP(obj, v), mo)
    #define PF_ATOMIC_FETCH_SUB_EXPLICIT(obj, v, mo)                          \
        PF__ATOMIC_FETCH(obj, PF__ATOMIC_OP_SUB, PF__ATOMIC_STEP(obj, v), mo)
    #define PF_ATOMIC_FETCH_OR_EXPLICIT(obj, v, mo)                          \
        PF__ATOMIC_FETCH(obj, PF__ATOMIC_OP_OR, (unsigned long long)(v), mo)
    #define PF_ATOMIC_FETCH_XOR_EXPLICIT(obj, v, mo)                          \
        PF__ATOMIC_FETCH(obj, PF__ATOMIC_OP_XOR, (unsigned long long)(v), mo)
    #define PF_ATOMIC_FETCH_AND_EXPLICIT(obj, v, mo)                          \
        PF__ATOMIC_FETCH(obj, PF__ATOMIC_OP_AND, (unsigned long long)(v), mo)
    #define PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT(f, mo) \
        ((void)(mo),                                    \
         *(unsigned char *)PF__ATOMIC_FN(exchange)(     \
             &(f)->pf__value,                           \
             1,                                         \
             (unsigned char[1]) { 1 },                  \
             (unsigned char[1]) { 0 }                   \
         ) != 0)
    #define PF_ATOMIC_FLAG_CLEAR_EXPLICIT(f, mo)                             \
        ((void)(mo),                                                         \
         PF__ATOMIC_FN(store)(&(f)->pf__value, 1, (unsigned char[1]) { 0 }))

    #define PF_ATOMIC_THREAD_FENCE(mo) ((void)(mo), PF__ATOMIC_FN(fence)())
    #define PF_ATOMIC_SIGNAL_FENCE(mo) ((void)(mo), PF__ATOMIC_FN(fence)())
    #define PF_KILL_DEPENDENCY(y) (y)
    #define PF_ATOMIC_IS_LOCK_FREE(obj)             \
        PF__ATOMIC_FN(is_lock_free)(sizeof(*(obj)))

    #define PF_ATOMIC_BOOL_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_CHAR_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_CHAR16_T_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_CHAR32_T_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_WCHAR_T_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_SHORT_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_INT_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_LONG_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_LLONG_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE
    #define PF_ATOMIC_POINTER_LOCK_FREE PF__ATOMIC_ALWAYS_LOCK_FREE

#endif

/* The sequentially consistent forms. */
#if PF_HAS_ATOMICS && !PF__ATOMIC_STD && !PF__ATOMIC_CXX
    #define PF_ATOMIC_LOAD(obj)                               \
        PF_ATOMIC_LOAD_EXPLICIT(obj, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_STORE(obj, v)                               \
        PF_ATOMIC_STORE_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_EXCHANGE(obj, v)                               \
        PF_ATOMIC_EXCHANGE_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_COMPARE_EXCHANGE_STRONG(obj, e, d)                \
        PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT(                     \
            obj, e, d, PF_MEMORY_ORDER_SEQ_CST, PF_MEMORY_ORDER_SEQ_CST \
        )
    #define PF_ATOMIC_COMPARE_EXCHANGE_WEAK(obj, e, d)                  \
        PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT(                       \
            obj, e, d, PF_MEMORY_ORDER_SEQ_CST, PF_MEMORY_ORDER_SEQ_CST \
        )
    #define PF_ATOMIC_FETCH_ADD(obj, v)                               \
        PF_ATOMIC_FETCH_ADD_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_FETCH_SUB(obj, v)                               \
        PF_ATOMIC_FETCH_SUB_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_FETCH_OR(obj, v)                               \
        PF_ATOMIC_FETCH_OR_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_FETCH_XOR(obj, v)                               \
        PF_ATOMIC_FETCH_XOR_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_FETCH_AND(obj, v)                               \
        PF_ATOMIC_FETCH_AND_EXPLICIT(obj, v, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_FLAG_TEST_AND_SET(f)                               \
        PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT(f, PF_MEMORY_ORDER_SEQ_CST)
    #define PF_ATOMIC_FLAG_CLEAR(f)                               \
        PF_ATOMIC_FLAG_CLEAR_EXPLICIT(f, PF_MEMORY_ORDER_SEQ_CST)
#endif

#endif /* CPOLYFILL_ATOMIC_H */

/* ---- Standard names --------------------------------------------------- */

#if !defined(PF__ATOMIC_STD_NAMES) && PF_HAS_ATOMICS && !PF__ATOMIC_STD \
    && ((defined(PF_STD_NAMES) && !defined(PF_NO_STD_NAMES_ATOMIC))     \
        || defined(PF_STD_NAMES_ATOMIC))
#define PF__ATOMIC_STD_NAMES

#if PF__ATOMIC_CXX
    #if PF_CPP > 202002L && defined(__has_include)
        #if __has_include(<stdatomic.h>)
            #include <stdatomic.h>
            #define PF__ATOMIC_CXX_STDATOMIC
        #endif
    #endif
    #ifndef PF__ATOMIC_CXX_STDATOMIC
        #ifndef _Atomic
            #define _Atomic(T) std::atomic<T>
        #endif
using std::atomic_compare_exchange_strong;
using std::atomic_compare_exchange_strong_explicit;
using std::atomic_compare_exchange_weak;
using std::atomic_compare_exchange_weak_explicit;
using std::atomic_exchange;
using std::atomic_exchange_explicit;
using std::atomic_fetch_add;
using std::atomic_fetch_add_explicit;
using std::atomic_fetch_and;
using std::atomic_fetch_and_explicit;
using std::atomic_fetch_or;
using std::atomic_fetch_or_explicit;
using std::atomic_fetch_sub;
using std::atomic_fetch_sub_explicit;
using std::atomic_fetch_xor;
using std::atomic_fetch_xor_explicit;
using std::atomic_flag;
using std::atomic_flag_clear;
using std::atomic_flag_clear_explicit;
using std::atomic_flag_test_and_set;
using std::atomic_flag_test_and_set_explicit;
using std::atomic_is_lock_free;
using std::atomic_load;
using std::atomic_load_explicit;
using std::atomic_signal_fence;
using std::atomic_store;
using std::atomic_store_explicit;
using std::atomic_thread_fence;
using std::kill_dependency;
using std::memory_order;
using std::memory_order_acq_rel;
using std::memory_order_acquire;
using std::memory_order_consume;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::memory_order_seq_cst;
    #endif
#else
    #define _Atomic(T) PF_ATOMIC(T)
typedef pf_memory_order memory_order;
    #define memory_order_relaxed PF_MEMORY_ORDER_RELAXED
    #define memory_order_consume PF_MEMORY_ORDER_CONSUME
    #define memory_order_acquire PF_MEMORY_ORDER_ACQUIRE
    #define memory_order_release PF_MEMORY_ORDER_RELEASE
    #define memory_order_acq_rel PF_MEMORY_ORDER_ACQ_REL
    #define memory_order_seq_cst PF_MEMORY_ORDER_SEQ_CST
typedef pf_atomic_flag_t atomic_flag;
    #define ATOMIC_FLAG_INIT PF_ATOMIC_FLAG_INIT

    #define ATOMIC_BOOL_LOCK_FREE PF_ATOMIC_BOOL_LOCK_FREE
    #define ATOMIC_CHAR_LOCK_FREE PF_ATOMIC_CHAR_LOCK_FREE
    #define ATOMIC_CHAR16_T_LOCK_FREE PF_ATOMIC_CHAR16_T_LOCK_FREE
    #define ATOMIC_CHAR32_T_LOCK_FREE PF_ATOMIC_CHAR32_T_LOCK_FREE
    #define ATOMIC_WCHAR_T_LOCK_FREE PF_ATOMIC_WCHAR_T_LOCK_FREE
    #define ATOMIC_SHORT_LOCK_FREE PF_ATOMIC_SHORT_LOCK_FREE
    #define ATOMIC_INT_LOCK_FREE PF_ATOMIC_INT_LOCK_FREE
    #define ATOMIC_LONG_LOCK_FREE PF_ATOMIC_LONG_LOCK_FREE
    #define ATOMIC_LLONG_LOCK_FREE PF_ATOMIC_LLONG_LOCK_FREE
    #define ATOMIC_POINTER_LOCK_FREE PF_ATOMIC_POINTER_LOCK_FREE

    #define atomic_init PF_ATOMIC_INIT
    #define atomic_load PF_ATOMIC_LOAD
    #define atomic_load_explicit PF_ATOMIC_LOAD_EXPLICIT
    #define atomic_store PF_ATOMIC_STORE
    #define atomic_store_explicit PF_ATOMIC_STORE_EXPLICIT
    #define atomic_exchange PF_ATOMIC_EXCHANGE
    #define atomic_exchange_explicit PF_ATOMIC_EXCHANGE_EXPLICIT
    #define atomic_compare_exchange_strong PF_ATOMIC_COMPARE_EXCHANGE_STRONG
    #define atomic_compare_exchange_strong_explicit \
        PF_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT
    #define atomic_compare_exchange_weak PF_ATOMIC_COMPARE_EXCHANGE_WEAK
    #define atomic_compare_exchange_weak_explicit \
        PF_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT
    #define atomic_fetch_add PF_ATOMIC_FETCH_ADD
    #define atomic_fetch_add_explicit PF_ATOMIC_FETCH_ADD_EXPLICIT
    #define atomic_fetch_sub PF_ATOMIC_FETCH_SUB
    #define atomic_fetch_sub_explicit PF_ATOMIC_FETCH_SUB_EXPLICIT
    #define atomic_fetch_or PF_ATOMIC_FETCH_OR
    #define atomic_fetch_or_explicit PF_ATOMIC_FETCH_OR_EXPLICIT
    #define atomic_fetch_xor PF_ATOMIC_FETCH_XOR
    #define atomic_fetch_xor_explicit PF_ATOMIC_FETCH_XOR_EXPLICIT
    #define atomic_fetch_and PF_ATOMIC_FETCH_AND
    #define atomic_fetch_and_explicit PF_ATOMIC_FETCH_AND_EXPLICIT
    #define atomic_flag_test_and_set PF_ATOMIC_FLAG_TEST_AND_SET
    #define atomic_flag_test_and_set_explicit \
        PF_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT
    #define atomic_flag_clear PF_ATOMIC_FLAG_CLEAR
    #define atomic_flag_clear_explicit PF_ATOMIC_FLAG_CLEAR_EXPLICIT
    #define atomic_thread_fence PF_ATOMIC_THREAD_FENCE
    #define atomic_signal_fence PF_ATOMIC_SIGNAL_FENCE
    #define atomic_is_lock_free PF_ATOMIC_IS_LOCK_FREE
    #define kill_dependency PF_KILL_DEPENDENCY

typedef PF_ATOMIC(_Bool) atomic_bool;
typedef PF_ATOMIC(char) atomic_char;
typedef PF_ATOMIC(signed char) atomic_schar;
typedef PF_ATOMIC(unsigned char) atomic_uchar;
typedef PF_ATOMIC(short) atomic_short;
typedef PF_ATOMIC(unsigned short) atomic_ushort;
typedef PF_ATOMIC(int) atomic_int;
typedef PF_ATOMIC(unsigned int) atomic_uint;
typedef PF_ATOMIC(long) atomic_long;
typedef PF_ATOMIC(unsigned long) atomic_ulong;
typedef PF_ATOMIC(long long) atomic_llong;
typedef PF_ATOMIC(unsigned long long) atomic_ullong;
typedef PF_ATOMIC(uint_least16_t) atomic_char16_t;
typedef PF_ATOMIC(uint_least32_t) atomic_char32_t;
typedef PF_ATOMIC(wchar_t) atomic_wchar_t;
typedef PF_ATOMIC(int_least8_t) atomic_int_least8_t;
typedef PF_ATOMIC(uint_least8_t) atomic_uint_least8_t;
typedef PF_ATOMIC(int_least16_t) atomic_int_least16_t;
typedef PF_ATOMIC(uint_least16_t) atomic_uint_least16_t;
typedef PF_ATOMIC(int_least32_t) atomic_int_least32_t;
typedef PF_ATOMIC(uint_least32_t) atomic_uint_least32_t;
typedef PF_ATOMIC(int_least64_t) atomic_int_least64_t;
typedef PF_ATOMIC(uint_least64_t) atomic_uint_least64_t;
typedef PF_ATOMIC(int_fast8_t) atomic_int_fast8_t;
typedef PF_ATOMIC(uint_fast8_t) atomic_uint_fast8_t;
typedef PF_ATOMIC(int_fast16_t) atomic_int_fast16_t;
typedef PF_ATOMIC(uint_fast16_t) atomic_uint_fast16_t;
typedef PF_ATOMIC(int_fast32_t) atomic_int_fast32_t;
typedef PF_ATOMIC(uint_fast32_t) atomic_uint_fast32_t;
typedef PF_ATOMIC(int_fast64_t) atomic_int_fast64_t;
typedef PF_ATOMIC(uint_fast64_t) atomic_uint_fast64_t;
typedef PF_ATOMIC(intptr_t) atomic_intptr_t;
typedef PF_ATOMIC(uintptr_t) atomic_uintptr_t;
typedef PF_ATOMIC(size_t) atomic_size_t;
typedef PF_ATOMIC(ptrdiff_t) atomic_ptrdiff_t;
typedef PF_ATOMIC(intmax_t) atomic_intmax_t;
typedef PF_ATOMIC(uintmax_t) atomic_uintmax_t;
#endif
#endif
