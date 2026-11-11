/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/threads.h> on Win32.

    A pf_thrd_t points to a control block. Each thread that the library
    knows has one, found through a thread-local pointer; one FLS slot holds
    it too, and its callback (run when the thread exits) runs the tss
    destructors and releases the block. Blocks are reference counted: the
    thread itself and, for threads created here, the joining or detaching
    side.
*/

#include "../internal.h"

#include <cpolyfill/stdc.h>
#include <cpolyfill/threads.h>
#include <cpolyfill/time.h>

#include <errno.h>
#include <limits.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

PF_STATIC_ASSERT(sizeof(SRWLOCK) == sizeof(void *), "SRWLOCK");
PF_STATIC_ASSERT(sizeof(CONDITION_VARIABLE) == sizeof(void *), "CV");
PF_STATIC_ASSERT(sizeof(INIT_ONCE) == sizeof(void *), "INIT_ONCE");
PF_STATIC_ASSERT(sizeof(DWORD) == sizeof(unsigned long), "DWORD");

#define SRW(m) ((PSRWLOCK)(void *)&(m)->pf__srw)
#define MTX_CV(m) ((PCONDITION_VARIABLE)(void *)&(m)->pf__cv)
#define CND_CV(c) ((PCONDITION_VARIABLE)(void *)&(c)->pf__cv)

struct pf__thrd {
    HANDLE handle;
    DWORD id;
    volatile LONG refs;
    int res;
    int foreign; /* not created by the library: cannot be joined */
    pf_thrd_start_t func;
    void *arg;
    char *name;
};

/* ---- Global state ------------------------------------------------------ */

struct tss_entry {
    DWORD key;
    pf_tss_dtor_t dtor;
};

static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
static DWORD fls_slot = FLS_OUT_OF_INDEXES;
static SRWLOCK tss_lock = SRWLOCK_INIT;
static struct tss_entry *tss_dtors;
static size_t tss_count, tss_cap;

static PF_THREAD_LOCAL struct pf__thrd *self_block;

static void release(struct pf__thrd *t) {
    if (InterlockedDecrement(&t->refs) == 0) {
        if (t->handle)
            CloseHandle(t->handle);
        free(t->name);
        free(t);
    }
}

/* Runs the destructors of the calling thread's tss values. */
static void run_tss_dtors(void) {
    int pass;
    for (pass = 0; pass < PF_TSS_DTOR_ITERATIONS; pass++) {
        struct tss_entry *copy;
        size_t n, i;
        int any = 0;

        AcquireSRWLockShared(&tss_lock);
        n = tss_count;
        copy = n ? (struct tss_entry *)malloc(n * sizeof(*copy)) : NULL;
        if (copy)
            memcpy(copy, tss_dtors, n * sizeof(*copy));
        ReleaseSRWLockShared(&tss_lock);
        if (!copy)
            return;

        for (i = 0; i < n; i++) {
            void *value = TlsGetValue(copy[i].key);
            if (value) {
                TlsSetValue(copy[i].key, NULL);
                copy[i].dtor(value);
                any = 1;
            }
        }
        free(copy);
        if (!any)
            return;
    }
}

static VOID NTAPI on_thread_exit(PVOID data) {
    struct pf__thrd *t = (struct pf__thrd *)data;
    run_tss_dtors();
    self_block = NULL;
    if (t)
        release(t);
}

static BOOL CALLBACK init_global(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    (void)once, (void)param, (void)ctx;
    fls_slot = FlsAlloc(on_thread_exit);
    return fls_slot != FLS_OUT_OF_INDEXES;
}

static int ensure_init(void) {
    return InitOnceExecuteOnce(&init_once, init_global, NULL, NULL) ? 0 : -1;
}

/* The block of the calling thread, made on first use for threads the
   library did not create. */
static struct pf__thrd *self(void) {
    struct pf__thrd *t = self_block;
    if (t)
        return t;
    if (ensure_init())
        return NULL;
    t = (struct pf__thrd *)calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    t->id = GetCurrentThreadId();
    t->refs = 1;
    t->foreign = 1;
    if (!DuplicateHandle(
            GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
            &t->handle, 0, FALSE, DUPLICATE_SAME_ACCESS
        ))
        t->handle = NULL;
    self_block = t;
    FlsSetValue(fls_slot, t);
    return t;
}

/* ---- Threads ----------------------------------------------------------- */

static unsigned __stdcall trampoline(void *raw) {
    struct pf__thrd *t = (struct pf__thrd *)raw;
    self_block = t;
    FlsSetValue(fls_slot, t);
    if (t->name)
        (void)pf_thrd_set_name(t->name);
    t->res = t->func(t->arg);
    return (unsigned)t->res;
}

PF_API int pf_thrd_create_ex(
    pf_thrd_t *thr, pf_thrd_start_t func, void *arg,
    const pf_thrd_attr_t *attr
) {
    struct pf__thrd *t;
    uintptr_t h;

    if (!thr || !func)
        return PF_EINVAL;
    if (attr && attr->stack_size > UINT_MAX)
        return PF_EINVAL;
    if (ensure_init())
        return pf_error_last_win32();
    t = (struct pf__thrd *)calloc(1, sizeof(*t));
    if (!t)
        return PF_ENOMEM;
    if (attr && attr->name) {
        size_t n = strlen(attr->name);
        t->name = (char *)malloc(n + 1);
        if (!t->name) {
            free(t);
            return PF_ENOMEM;
        }
        memcpy(t->name, attr->name, n + 1);
    }
    t->func = func;
    t->arg = arg;
    t->refs = 2; /* the thread and the joining / detaching side */

    h = _beginthreadex(
        NULL, attr ? (unsigned)attr->stack_size : 0, trampoline, t,
        CREATE_SUSPENDED, (unsigned *)&t->id
    );
    if (h == 0) {
        int err = errno;
        free(t->name);
        free(t);
        return pf_error_from_errno(err);
    }
    t->handle = (HANDLE)h;
    *thr = t;
    ResumeThread(t->handle);
    return 0;
}

PF_API int pf_thrd_create(pf_thrd_t *thr, pf_thrd_start_t func, void *arg) {
    switch (pf_thrd_create_ex(thr, func, arg, NULL)) {
    case 0: return pf_thrd_success;
    case PF_ENOMEM: return pf_thrd_nomem;
    default: return pf_thrd_error;
    }
}

PF_API int pf_thrd_equal(pf_thrd_t a, pf_thrd_t b) {
    return a == b || (a && b && a->id == b->id);
}

PF_API pf_thrd_t pf_thrd_current(void) {
    return self();
}

PF_API int pf_thrd_sleep(
    const struct timespec *duration, struct timespec *remaining
) {
    return pf_nanosleep(duration, remaining) == 0 ? 0 : -2;
}

PF_API void pf_thrd_yield(void) {
    SwitchToThread();
}

PF_NORETURN PF_API void pf_thrd_exit(int res) {
    struct pf__thrd *t = self();
    if (t)
        t->res = res;
    _endthreadex((unsigned)res);
    PF_UNREACHABLE();
}

PF_API int pf_thrd_detach(pf_thrd_t thr) {
    if (!thr || thr->foreign)
        return pf_thrd_error;
    release(thr);
    return pf_thrd_success;
}

PF_API int pf_thrd_join(pf_thrd_t thr, int *res) {
    if (!thr || thr->foreign)
        return pf_thrd_error;
    if (WaitForSingleObject(thr->handle, INFINITE) != WAIT_OBJECT_0)
        return pf_thrd_error;
    if (res)
        *res = thr->res;
    release(thr);
    return pf_thrd_success;
}

/* ---- Timeouts ---------------------------------------------------------- */

/* Milliseconds until an absolute UTC deadline, rounded up; INFINITE for
   no deadline. */
static DWORD ms_until(const struct timespec *deadline) {
    struct timespec now;
    int64_t ns;

    if (!deadline)
        return INFINITE;
    if (!pf_timespec_get(&now, PF_TIME_UTC))
        return 0;
    ns = pf_timespec_to_ns(pf_timespec_sub(*deadline, now));
    if (ns <= 0)
        return 0;
    if (ns / 1000000 >= INFINITE - 1)
        return INFINITE - 1;
    return (DWORD)((ns + 999999) / 1000000);
}

static int passed(const struct timespec *deadline) {
    struct timespec now;
    return deadline && pf_timespec_get(&now, PF_TIME_UTC)
        && pf_timespec_cmp(now, *deadline) >= 0;
}

/* ---- Mutexes ----------------------------------------------------------- */

/* Plain and recursive mutexes are the SRWLOCK itself (with an owner and a
   count for recursion). Timed ones use it to guard `pf__locked`, waiting
   on their condition variable. */

PF_API int pf_mtx_init(pf_mtx_t *mtx, int type) {
    if (!mtx || (type & ~(pf_mtx_timed | pf_mtx_recursive)))
        return pf_thrd_error;
    memset(mtx, 0, sizeof(*mtx));
    InitializeSRWLock(SRW(mtx));
    InitializeConditionVariable(MTX_CV(mtx));
    mtx->pf__type = type;
    return pf_thrd_success;
}

static int timed_lock(pf_mtx_t *m, const struct timespec *deadline, int try_) {
    DWORD me = GetCurrentThreadId();

    AcquireSRWLockExclusive(SRW(m));
    if (m->pf__locked && m->pf__owner == me) {
        int ok = (m->pf__type & pf_mtx_recursive) != 0;
        if (ok)
            m->pf__count++;
        ReleaseSRWLockExclusive(SRW(m));
        return ok ? pf_thrd_success : try_ ? pf_thrd_busy : pf_thrd_error;
    }
    while (m->pf__locked) {
        if (try_) {
            ReleaseSRWLockExclusive(SRW(m));
            return pf_thrd_busy;
        }
        if (!SleepConditionVariableSRW(MTX_CV(m), SRW(m), ms_until(deadline), 0)
            && passed(deadline)) {
            ReleaseSRWLockExclusive(SRW(m));
            return pf_thrd_timedout;
        }
    }
    m->pf__locked = 1;
    m->pf__owner = me;
    m->pf__count = 1;
    ReleaseSRWLockExclusive(SRW(m));
    return pf_thrd_success;
}

PF_API int pf_mtx_lock(pf_mtx_t *mtx) {
    DWORD me = GetCurrentThreadId();

    if (mtx->pf__type & pf_mtx_timed)
        return timed_lock(mtx, NULL, 0);
    if (mtx->pf__type & pf_mtx_recursive) {
        if (InterlockedCompareExchange((volatile LONG *)&mtx->pf__owner, 0, 0)
            == (LONG)me) {
            mtx->pf__count++;
            return pf_thrd_success;
        }
        AcquireSRWLockExclusive(SRW(mtx));
        mtx->pf__owner = me;
        mtx->pf__count = 1;
        return pf_thrd_success;
    }
    AcquireSRWLockExclusive(SRW(mtx));
    mtx->pf__owner = me;
    return pf_thrd_success;
}

PF_API int pf_mtx_trylock(pf_mtx_t *mtx) {
    DWORD me = GetCurrentThreadId();

    if (mtx->pf__type & pf_mtx_timed)
        return timed_lock(mtx, NULL, 1);
    if ((mtx->pf__type & pf_mtx_recursive)
        && InterlockedCompareExchange((volatile LONG *)&mtx->pf__owner, 0, 0)
               == (LONG)me) {
        mtx->pf__count++;
        return pf_thrd_success;
    }
    if (!TryAcquireSRWLockExclusive(SRW(mtx)))
        return pf_thrd_busy;
    mtx->pf__owner = me;
    mtx->pf__count = 1;
    return pf_thrd_success;
}

PF_API int pf_mtx_timedlock(pf_mtx_t *mtx, const struct timespec *deadline) {
    if (mtx->pf__type & pf_mtx_timed)
        return timed_lock(mtx, deadline, 0);
    /* Not a timed mutex (undefined in C11): poll. */
    for (;;) {
        int rc = pf_mtx_trylock(mtx);
        if (rc != pf_thrd_busy)
            return rc;
        if (passed(deadline))
            return pf_thrd_timedout;
        Sleep(1);
    }
}

PF_API int pf_mtx_unlock(pf_mtx_t *mtx) {
    DWORD me = GetCurrentThreadId();

    if (mtx->pf__type & pf_mtx_timed) {
        int rc = pf_thrd_success;
        AcquireSRWLockExclusive(SRW(mtx));
        if (!mtx->pf__locked || mtx->pf__owner != me) {
            rc = pf_thrd_error;
        } else if (--mtx->pf__count == 0) {
            mtx->pf__locked = 0;
            mtx->pf__owner = 0;
            WakeConditionVariable(MTX_CV(mtx));
        }
        ReleaseSRWLockExclusive(SRW(mtx));
        return rc;
    }
    if (mtx->pf__owner != me)
        return pf_thrd_error;
    if ((mtx->pf__type & pf_mtx_recursive) && --mtx->pf__count > 0)
        return pf_thrd_success;
    InterlockedExchange((volatile LONG *)&mtx->pf__owner, 0);
    ReleaseSRWLockExclusive(SRW(mtx));
    return pf_thrd_success;
}

PF_API void pf_mtx_destroy(pf_mtx_t *mtx) {
    (void)mtx; /* SRWLOCK and CONDITION_VARIABLE need no cleanup */
}

/* ---- Once -------------------------------------------------------------- */

struct once_call {
    void (*func)(void);
};

static BOOL CALLBACK once_trampoline(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    (void)once, (void)ctx;
    ((struct once_call *)param)->func();
    return TRUE;
}

PF_API void pf_call_once(pf_once_flag *flag, void (*func)(void)) {
    struct once_call call;
    call.func = func;
    InitOnceExecuteOnce(
        (PINIT_ONCE)(void *)&flag->pf__once, once_trampoline, &call, NULL
    );
}

/* ---- Condition variables ----------------------------------------------- */

PF_API int pf_cnd_init(pf_cnd_t *cnd) {
    InitializeConditionVariable(CND_CV(cnd));
    return pf_thrd_success;
}

PF_API int pf_cnd_signal(pf_cnd_t *cnd) {
    WakeConditionVariable(CND_CV(cnd));
    return pf_thrd_success;
}

PF_API int pf_cnd_broadcast(pf_cnd_t *cnd) {
    WakeAllConditionVariable(CND_CV(cnd));
    return pf_thrd_success;
}

static int cnd_wait(pf_cnd_t *c, pf_mtx_t *m, const struct timespec *deadline) {
    DWORD me = GetCurrentThreadId();
    unsigned long count;
    BOOL ok;

    if (m->pf__type & pf_mtx_timed) {
        /* Hold the guard while giving up the mutex, so a signal sent by a
           thread that takes the mutex after us cannot be missed. */
        AcquireSRWLockExclusive(SRW(m));
        if (!m->pf__locked || m->pf__owner != me) {
            ReleaseSRWLockExclusive(SRW(m));
            return pf_thrd_error;
        }
        count = m->pf__count;
        m->pf__locked = 0;
        m->pf__owner = 0;
        m->pf__count = 0;
        WakeConditionVariable(MTX_CV(m));
        ok = SleepConditionVariableSRW(CND_CV(c), SRW(m), ms_until(deadline), 0);
        while (m->pf__locked)
            SleepConditionVariableSRW(MTX_CV(m), SRW(m), INFINITE, 0);
        m->pf__locked = 1;
        m->pf__owner = me;
        m->pf__count = count;
        ReleaseSRWLockExclusive(SRW(m));
    } else {
        if (m->pf__owner != me)
            return pf_thrd_error;
        count = m->pf__count;
        InterlockedExchange((volatile LONG *)&m->pf__owner, 0);
        ok = SleepConditionVariableSRW(CND_CV(c), SRW(m), ms_until(deadline), 0);
        m->pf__owner = me;
        m->pf__count = count;
    }
    if (ok)
        return pf_thrd_success;
    return passed(deadline) ? pf_thrd_timedout : pf_thrd_success;
}

PF_API int pf_cnd_wait(pf_cnd_t *cnd, pf_mtx_t *mtx) {
    return cnd_wait(cnd, mtx, NULL);
}

PF_API int pf_cnd_timedwait(
    pf_cnd_t *cnd, pf_mtx_t *mtx, const struct timespec *deadline
) {
    return cnd_wait(cnd, mtx, deadline);
}

PF_API void pf_cnd_destroy(pf_cnd_t *cnd) {
    (void)cnd;
}

/* ---- Thread-specific storage ------------------------------------------- */

PF_API int pf_tss_create(pf_tss_t *key, pf_tss_dtor_t dtor) {
    DWORD k;

    if (!key || ensure_init())
        return pf_thrd_error;
    k = TlsAlloc();
    if (k == TLS_OUT_OF_INDEXES)
        return pf_thrd_error;
    if (dtor) {
        int ok = 1;
        AcquireSRWLockExclusive(&tss_lock);
        if (tss_count == tss_cap) {
            size_t cap = tss_cap ? tss_cap * 2 : 16;
            struct tss_entry *grown = (struct tss_entry *)realloc(
                tss_dtors, cap * sizeof(*grown)
            );
            if (grown) {
                tss_dtors = grown;
                tss_cap = cap;
            } else {
                ok = 0;
            }
        }
        if (ok) {
            tss_dtors[tss_count].key = k;
            tss_dtors[tss_count].dtor = dtor;
            tss_count++;
        }
        ReleaseSRWLockExclusive(&tss_lock);
        if (!ok) {
            TlsFree(k);
            return pf_thrd_nomem;
        }
    }
    *key = k;
    return pf_thrd_success;
}

PF_API void pf_tss_delete(pf_tss_t key) {
    size_t i;
    AcquireSRWLockExclusive(&tss_lock);
    for (i = 0; i < tss_count; i++) {
        if (tss_dtors[i].key == key) {
            tss_dtors[i] = tss_dtors[--tss_count];
            break;
        }
    }
    ReleaseSRWLockExclusive(&tss_lock);
    TlsFree(key);
}

PF_API void *pf_tss_get(pf_tss_t key) {
    return TlsGetValue(key);
}

PF_API int pf_tss_set(pf_tss_t key, void *value) {
    /* Make sure the exit callback runs for this thread. */
    if (value && !self())
        return pf_thrd_nomem;
    return TlsSetValue(key, value) ? pf_thrd_success : pf_thrd_error;
}

/* ---- Names, id, affinity, priority ------------------------------------- */

typedef HRESULT(WINAPI *set_description_fn)(HANDLE, PCWSTR);
typedef HRESULT(WINAPI *get_description_fn)(HANDLE, PWSTR *);

/* Windows 10 1607 and later. */
static FARPROC kernel32_proc(const char *name) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    return k32 ? GetProcAddress(k32, name) : NULL;
}

PF_API int pf_thrd_set_name(const char *name) {
    set_description_fn set;
    pf__wstr w;
    HRESULT hr;
    int rc;

    if (!name)
        return PF_EINVAL;
    set = (set_description_fn)(void (*)(void))kernel32_proc(
        "SetThreadDescription"
    );
    if (!set)
        return PF_ENOTSUP;
    if ((rc = pf__wstr_from_utf8(&w, name)) != 0)
        return rc;
    hr = set(GetCurrentThread(), (PCWSTR)w.ptr);
    pf__wstr_free(&w);
    return SUCCEEDED(hr) ? 0 : PF_EINVAL;
}

PF_API int pf_thrd_get_name(char *buf, size_t size) {
    get_description_fn get;
    PWSTR desc = NULL;
    ptrdiff_t n;
    char *utf8;
    int rc;

    if (!buf || size == 0)
        return PF_EINVAL;
    get = (get_description_fn)(void (*)(void))kernel32_proc(
        "GetThreadDescription"
    );
    if (!get)
        return PF_ENOTSUP;
    if (FAILED(get(GetCurrentThread(), &desc)))
        return PF_EINVAL;
    n = pf_utf16_to_utf8((const uint16_t *)desc, PF_NTS, NULL, 0, 0);
    if (n < 0) {
        LocalFree(desc);
        return (int)n;
    }
    utf8 = (char *)malloc((size_t)n + 1);
    if (!utf8) {
        LocalFree(desc);
        return PF_ENOMEM;
    }
    pf_utf16_to_utf8((const uint16_t *)desc, PF_NTS, utf8, (size_t)n + 1, 0);
    LocalFree(desc);
    rc = pf__copy_name(buf, size, utf8);
    free(utf8);
    return rc;
}

PF_API uint64_t pf_thrd_id(void) {
    return GetCurrentThreadId();
}

/* CPUs are numbered group by group. */
static int group_of(size_t cpu, WORD *group, DWORD *index) {
    WORD g, groups = GetActiveProcessorGroupCount();
    for (g = 0; g < groups; g++) {
        DWORD n = GetActiveProcessorCount(g);
        if (cpu < n) {
            *group = g;
            *index = (DWORD)cpu;
            return 0;
        }
        cpu -= n;
    }
    return -1;
}

PF_API int pf_thrd_set_affinity(const uint64_t *mask, size_t ncpus) {
    GROUP_AFFINITY aff;
    size_t i;
    int found = 0;

    if (!mask || ncpus == 0)
        return PF_EINVAL;
    memset(&aff, 0, sizeof(aff));
    for (i = 0; i < ncpus; i++) {
        WORD g;
        DWORD idx;
        if (!(mask[i / 64] >> (i % 64) & 1))
            continue;
        if (group_of(i, &g, &idx) || idx >= sizeof(KAFFINITY) * CHAR_BIT)
            return PF_EINVAL;
        if (found && g != aff.Group)
            return PF_EINVAL; /* one processor group only */
        aff.Group = g;
        aff.Mask |= (KAFFINITY)1 << idx;
        found = 1;
    }
    if (!found)
        return PF_EINVAL;
    if (!SetThreadGroupAffinity(GetCurrentThread(), &aff, NULL))
        return pf_error_last_win32();
    return 0;
}

PF_API int pf_thrd_get_affinity(uint64_t *mask, size_t ncpus) {
    GROUP_AFFINITY aff;
    size_t first = 0, i;
    WORD g;

    if (!mask || ncpus == 0)
        return PF_EINVAL;
    if (!GetThreadGroupAffinity(GetCurrentThread(), &aff))
        return pf_error_last_win32();
    for (g = 0; g < aff.Group; g++)
        first += GetActiveProcessorCount(g);
    memset(mask, 0, (ncpus + 63) / 64 * sizeof(uint64_t));
    for (i = 0; i < sizeof(KAFFINITY) * CHAR_BIT; i++) {
        size_t cpu = first + i;
        if (cpu < ncpus && (aff.Mask >> i & 1))
            mask[cpu / 64] |= (uint64_t)1 << (cpu % 64);
    }
    return 0;
}

PF_API int pf_thrd_set_priority(int priority) {
    static const int levels[] = {
        THREAD_PRIORITY_LOWEST,       THREAD_PRIORITY_BELOW_NORMAL,
        THREAD_PRIORITY_NORMAL,       THREAD_PRIORITY_ABOVE_NORMAL,
        THREAD_PRIORITY_HIGHEST,
    };
    if (priority < PF_THRD_PRIORITY_LOWEST
        || priority > PF_THRD_PRIORITY_HIGHEST)
        return PF_EINVAL;
    if (!SetThreadPriority(GetCurrentThread(), levels[priority + 2]))
        return pf_error_last_win32();
    return 0;
}
