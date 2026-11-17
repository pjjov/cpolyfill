/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    The poller on Windows: an I/O completion port.

    Sockets are polled through the AFD driver (IOCTL_AFD_POLL), the way
    libuv's uv_poll, Rust's mio and wepoll do: one poll per socket is
    submitted on a handle to \Device\Afd associated with the port, and it
    completes when the socket is ready. Everything else is a waitable
    handle: RegisterWaitForSingleObject runs a callback on the thread pool
    that posts to the port. Pipes are made waitable with a zero-byte
    overlapped read, which completes when data arrives.

    Registrations are level-triggered: after each event the registration
    is queued, and the next wait submits its poll (or wait) again.
*/

#include "../internal.h"

#include "../poller_impl.h"

#include <cpolyfill/pipe.h>
#include <cpolyfill/socket.h>

#include <stddef.h>
#include <string.h>
#include <windows.h>

const pf__pbackend *const pf__poller_poll = NULL;

/* ---- ntdll ------------------------------------------------------------- */

typedef LONG pf__ntstatus;

typedef struct pf__iosb {
    union {
        pf__ntstatus Status;
        void *Pointer;
    } u;
    ULONG_PTR Information;
} pf__iosb;

typedef struct pf__ustr {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} pf__ustr;

typedef struct pf__oattr {
    ULONG Length;
    HANDLE RootDirectory;
    pf__ustr *ObjectName;
    ULONG Attributes;
    void *SecurityDescriptor;
    void *SecurityQualityOfService;
} pf__oattr;

typedef struct pf__afd_handle {
    HANDLE Handle;
    ULONG Events;
    pf__ntstatus Status;
} pf__afd_handle;

typedef struct pf__afd_info {
    LARGE_INTEGER Timeout;
    ULONG NumberOfHandles;
    ULONG Exclusive;
    pf__afd_handle Handles[1];
} pf__afd_info;

typedef pf__ntstatus(NTAPI *pf__NtCreateFile_t)(
    HANDLE *,
    ACCESS_MASK,
    pf__oattr *,
    pf__iosb *,
    LARGE_INTEGER *,
    ULONG,
    ULONG,
    ULONG,
    ULONG,
    void *,
    ULONG
);
typedef pf__ntstatus(NTAPI *pf__NtDeviceIoControlFile_t)(
    HANDLE,
    HANDLE,
    void *,
    void *,
    pf__iosb *,
    ULONG,
    void *,
    ULONG,
    void *,
    ULONG
);
typedef pf__ntstatus(NTAPI *pf__NtCancelIoFileEx_t)(
    HANDLE, pf__iosb *, pf__iosb *
);
typedef ULONG(NTAPI *pf__RtlNtStatusToDosError_t)(pf__ntstatus);

#define PF__STATUS_SUCCESS ((pf__ntstatus)0x00000000L)
#define PF__STATUS_PENDING ((pf__ntstatus)0x00000103L)
#define PF__STATUS_CANCELLED ((pf__ntstatus)0xC0000120L)
#define PF__NT_SUCCESS(s) ((s) >= 0)
#define PF__FILE_OPEN 1

#define PF__IOCTL_AFD_POLL 0x00012024

#define PF__AFD_RECEIVE 0x0001
#define PF__AFD_RECEIVE_EXPEDITED 0x0002
#define PF__AFD_SEND 0x0004
#define PF__AFD_DISCONNECT 0x0008
#define PF__AFD_ABORT 0x0010
#define PF__AFD_LOCAL_CLOSE 0x0020
#define PF__AFD_ACCEPT 0x0080
#define PF__AFD_CONNECT_FAIL 0x0100

#ifndef SIO_BASE_HANDLE
    #define SIO_BASE_HANDLE 0x48000022
#endif
#ifndef SIO_BSP_HANDLE
    #define SIO_BSP_HANDLE 0x4800001B
#endif
#ifndef SIO_BSP_HANDLE_SELECT
    #define SIO_BSP_HANDLE_SELECT 0x4800001C
#endif
#ifndef SIO_BSP_HANDLE_POLL
    #define SIO_BSP_HANDLE_POLL 0x4800001D
#endif

static pf__NtCreateFile_t pf__NtCreateFile;
static pf__NtDeviceIoControlFile_t pf__NtDeviceIoControlFile;
static pf__NtCancelIoFileEx_t pf__NtCancelIoFileEx;
static pf__RtlNtStatusToDosError_t pf__RtlNtStatusToDosError;
static INIT_ONCE pf__nt_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK pf__nt_load(INIT_ONCE *once, void *arg, void **ctx) {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    (void)once, (void)arg, (void)ctx;
    if (!nt)
        return TRUE;
    pf__NtCreateFile = (pf__NtCreateFile_t)(void (*)(void))GetProcAddress(
        nt, "NtCreateFile"
    );
    pf__NtDeviceIoControlFile = (pf__NtDeviceIoControlFile_t)(void (*)(void))
        GetProcAddress(nt, "NtDeviceIoControlFile");
    pf__NtCancelIoFileEx = (pf__NtCancelIoFileEx_t)(void (*)(void))
        GetProcAddress(nt, "NtCancelIoFileEx");
    pf__RtlNtStatusToDosError = (pf__RtlNtStatusToDosError_t)(void (*)(void))
        GetProcAddress(nt, "RtlNtStatusToDosError");
    return TRUE;
}

static bool pf__nt_ready(void) {
    InitOnceExecuteOnce(&pf__nt_once, pf__nt_load, NULL, NULL);
    return pf__NtCreateFile && pf__NtDeviceIoControlFile && pf__NtCancelIoFileEx
        && pf__RtlNtStatusToDosError;
}

static int pf__nt_error(pf__ntstatus status) {
    return pf_error_from_win32(pf__RtlNtStatusToDosError(status));
}

/* ---- State ------------------------------------------------------------- */

/* Completion keys. */
#define PF__KEY_AFD 1
#define PF__KEY_WAIT 2
#define PF__KEY_WAKE 3

typedef struct pf__wreg {
    int id;      /* 0 once removed */
    int post_id; /* the id the wait callback posts (never changes) */
    int kind;
    HANDLE iocp;
    bool queued;  /* in the arm queue */
    bool pending; /* an AFD poll or zero-byte read is in flight */
    bool closed;  /* the socket was closed: no more polls */
    /* Sockets. */
    SOCKET base;
    pf__afd_info info;
    pf__iosb iosb;
    /* Pipes. */
    OVERLAPPED ov;
    HANDLE event;
    /* Registered waits. */
    HANDLE wait; /* NULL if none */
    HANDLE object;
} pf__wreg;

typedef struct pf__wbe {
    HANDLE iocp;
    HANDLE afd; /* NULL until the first socket */
    pf__wreg **queue;
    size_t nqueue;
    size_t qcap;
    pf__wreg **dead; /* removed sockets whose poll is being cancelled */
    size_t ndead;
    size_t dcap;
    volatile LONG wake_pending;
} pf__wbe;

static int pf__push(
    pf__poller *p, pf__wreg ***arr, size_t *n, size_t *cap, pf__wreg *w
) {
    if (*n == *cap) {
        size_t ncap = *cap ? *cap * 2 : 16;
        pf__wreg **na = (pf__wreg **)reallocate(
            p->alloc, *arr, *cap * sizeof(*na), ncap * sizeof(*na)
        );
        if (!na)
            return PF_ENOMEM;
        *arr = na;
        *cap = ncap;
    }
    (*arr)[(*n)++] = w;
    return PF_OK;
}

static void pf__drop(pf__wreg **arr, size_t *n, pf__wreg *w) {
    size_t i;
    for (i = 0; i < *n; i++)
        if (arr[i] == w) {
            arr[i] = arr[--*n];
            return;
        }
}

/* Queues `w` to be (re)armed by the next wait. */
static void pf__enqueue(pf__poller *p, pf__wreg *w) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    if (w->queued || w->id == 0)
        return;
    /* On failure the registration stays quiet until modified; there is
       no better way to report it here. */
    if (pf__push(p, &b->queue, &b->nqueue, &b->qcap, w) == PF_OK)
        w->queued = true;
}

/* ---- Sockets ----------------------------------------------------------- */

static int pf__afd_open(pf__wbe *b) {
    static const WCHAR name[] = L"\\Device\\Afd\\CPolyfill";
    pf__ustr us;
    pf__oattr oa;
    pf__iosb iosb;
    pf__ntstatus st;
    HANDLE h;

    if (b->afd)
        return PF_OK;
    if (!pf__nt_ready())
        return PF_ENOSYS;
    us.Length = (USHORT)(sizeof(name) - sizeof(WCHAR));
    us.MaximumLength = (USHORT)sizeof(name);
    us.Buffer = (PWSTR)name;
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(oa);
    oa.ObjectName = &us;
    st = pf__NtCreateFile(
        &h,
        SYNCHRONIZE,
        &oa,
        &iosb,
        NULL,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        PF__FILE_OPEN,
        0,
        NULL,
        0
    );
    if (!PF__NT_SUCCESS(st))
        return pf__nt_error(st);
    if (!CreateIoCompletionPort(h, b->iocp, PF__KEY_AFD, 0)
        || !SetFileCompletionNotificationModes(
            h, FILE_SKIP_SET_EVENT_ON_HANDLE
        )) {
        int rc = pf_error_last_win32();
        CloseHandle(h);
        return rc;
    }
    b->afd = h;
    return PF_OK;
}

/* The base provider's socket, which AFD knows (not a layered one). */
static SOCKET pf__base_socket(SOCKET s) {
    static const DWORD ioctls[] = {
        SIO_BASE_HANDLE,
        SIO_BSP_HANDLE_SELECT,
        SIO_BSP_HANDLE_POLL,
        SIO_BSP_HANDLE,
    };
    size_t i;
    for (i = 0; i < sizeof(ioctls) / sizeof(ioctls[0]); i++) {
        SOCKET base = INVALID_SOCKET;
        DWORD bytes;
        if (WSAIoctl(
                s, ioctls[i], NULL, 0, &base, sizeof(base), &bytes, NULL, NULL
            ) != SOCKET_ERROR
            && base != INVALID_SOCKET)
            return base;
    }
    return INVALID_SOCKET;
}

static void pf__afd_cancel(pf__wbe *b, pf__wreg *w) {
    pf__iosb cancel;
    if (w->pending)
        pf__NtCancelIoFileEx(b->afd, &w->iosb, &cancel);
}

static void pf__afd_submit(pf__poller *p, pf__preg *r, pf__wreg *w) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    ULONG mask = PF__AFD_ABORT | PF__AFD_CONNECT_FAIL | PF__AFD_LOCAL_CLOSE;
    pf__ntstatus st;

    if (r->events & PF_POLL_IN)
        mask |= PF__AFD_RECEIVE | PF__AFD_ACCEPT | PF__AFD_DISCONNECT;
    if (r->events & PF_POLL_OUT)
        mask |= PF__AFD_SEND;
    memset(&w->info, 0, sizeof(w->info));
    w->info.Timeout.QuadPart = INT64_MAX;
    w->info.NumberOfHandles = 1;
    w->info.Exclusive = FALSE;
    w->info.Handles[0].Handle = (HANDLE)w->base;
    w->info.Handles[0].Events = mask;
    w->iosb.u.Status = PF__STATUS_PENDING;
    st = pf__NtDeviceIoControlFile(
        b->afd,
        NULL,
        NULL,
        &w->iosb,
        &w->iosb,
        PF__IOCTL_AFD_POLL,
        &w->info,
        sizeof(w->info),
        &w->info,
        sizeof(w->info)
    );
    if (st == PF__STATUS_PENDING || st == PF__STATUS_SUCCESS) {
        /* Either way the completion comes through the port. */
        w->pending = true;
        return;
    }
    /* Most likely the socket is closed. */
    w->closed = true;
    pf__poller_emit(p, r->id, PF_POLL_ERR);
}

static unsigned pf__afd_events(const pf__preg *r, ULONG afd) {
    unsigned ev = 0;
    if (afd & (PF__AFD_RECEIVE | PF__AFD_ACCEPT | PF__AFD_RECEIVE_EXPEDITED))
        ev |= PF_POLL_IN;
    if (afd & PF__AFD_DISCONNECT)
        ev |= PF_POLL_IN | PF_POLL_HUP;
    if (afd & PF__AFD_SEND)
        ev |= PF_POLL_OUT;
    if (afd & PF__AFD_ABORT)
        ev |= PF_POLL_IN | PF_POLL_HUP | PF_POLL_ERR;
    if (afd & PF__AFD_CONNECT_FAIL)
        ev |= PF_POLL_OUT | PF_POLL_ERR;
    if (!(r->events & PF_POLL_IN))
        ev &= ~PF_POLL_IN;
    if (!(r->events & PF_POLL_OUT))
        ev &= ~PF_POLL_OUT;
    return ev;
}

static void pf__afd_done(pf__poller *p, pf__wreg *w) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    pf__preg *r;
    pf__ntstatus st = w->iosb.u.Status;

    w->pending = false;
    if (w->id == 0) {
        pf__drop(b->dead, &b->ndead, w);
        deallocate(p->alloc, w, sizeof(*w));
        return;
    }
    r = pf__poller_find(p, w->id);
    if (!r)
        return;
    if (st == PF__STATUS_CANCELLED) {
        /* pf_poller_modify asked for other events. */
    } else if (!PF__NT_SUCCESS(st)) {
        w->closed = true;
        pf__poller_emit(p, r->id, PF_POLL_ERR);
    } else if (w->info.NumberOfHandles >= 1) {
        ULONG afd = w->info.Handles[0].Events;
        if (afd & PF__AFD_LOCAL_CLOSE)
            w->closed = true;
        else
            pf__poller_emit(p, r->id, pf__afd_events(r, afd));
    }
    pf__enqueue(p, w);
}

/* ---- Registered waits -------------------------------------------------- */

static VOID CALLBACK pf__wait_cb(void *ctx, BOOLEAN timed_out) {
    pf__wreg *w = (pf__wreg *)ctx;
    (void)timed_out;
    PostQueuedCompletionStatus(w->iocp, (DWORD)w->post_id, PF__KEY_WAIT, NULL);
}

static void pf__wait_start(pf__poller *p, pf__preg *r, pf__wreg *w) {
    if (w->wait)
        return;
    if (!RegisterWaitForSingleObject(
            &w->wait, w->object, pf__wait_cb, w, INFINITE, WT_EXECUTEONLYONCE
        )) {
        w->wait = NULL;
        pf__poller_emit(p, r->id, PF_POLL_ERR);
    }
}

/* Stops a registered wait, waiting for a callback that is running. */
static void pf__wait_stop(pf__wreg *w) {
    if (w->wait) {
        UnregisterWaitEx(w->wait, INVALID_HANDLE_VALUE);
        w->wait = NULL;
    }
}

/* ---- Pipes ------------------------------------------------------------- */

static void pf__pipe_arm(pf__poller *p, pf__preg *r, pf__wreg *w) {
    pf_pipe_t *pipe = (pf_pipe_t *)r->obj;
    HANDLE h = (HANDLE)pipe->pf__h;
    static char zero;

    /* A pipe cannot say whether a write would block. */
    if (r->events & PF_POLL_OUT)
        pf__poller_emit(p, r->id, PF_POLL_OUT);
    if (!(r->events & PF_POLL_IN))
        return;
    if (!w->pending) {
        BOOL ok;
        memset(&w->ov, 0, sizeof(w->ov));
        ResetEvent(w->event);
        w->ov.hEvent = w->event;
        ok = ReadFile(h, &zero, 0, NULL, &w->ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            DWORD e = GetLastError();
            if (e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED
                || e == ERROR_NO_DATA)
                pf__poller_emit(p, r->id, PF_POLL_IN | PF_POLL_HUP);
            else
                pf__poller_emit(p, r->id, PF_POLL_ERR);
            return;
        }
        w->pending = true;
    }
    pf__wait_start(p, r, w);
}

static void pf__pipe_done(pf__poller *p, pf__preg *r, pf__wreg *w) {
    pf_pipe_t *pipe = (pf_pipe_t *)r->obj;
    DWORD n;
    if (GetOverlappedResult((HANDLE)pipe->pf__h, &w->ov, &n, FALSE)) {
        w->pending = false;
        pf__poller_emit(p, r->id, PF_POLL_IN);
    } else {
        DWORD e = GetLastError();
        if (e == ERROR_IO_INCOMPLETE)
            return; /* still in flight: waited for again */
        w->pending = false;
        if (e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED
            || e == ERROR_NO_DATA)
            pf__poller_emit(p, r->id, PF_POLL_IN | PF_POLL_HUP);
        else if (e != ERROR_OPERATION_ABORTED)
            pf__poller_emit(p, r->id, PF_POLL_ERR);
    }
}

static void pf__pipe_cancel(pf__preg *r, pf__wreg *w) {
    pf_pipe_t *pipe = (pf_pipe_t *)r->obj;
    if (w->pending) {
        DWORD n;
        CancelIoEx((HANDLE)pipe->pf__h, &w->ov);
        GetOverlappedResult((HANDLE)pipe->pf__h, &w->ov, &n, TRUE);
        w->pending = false;
    }
}

/* ---- Backend ----------------------------------------------------------- */

static int pf__iocp_init(pf__poller *p) {
    pf__wbe *b = (pf__wbe *)zallocate(p->alloc, sizeof(*b));
    if (!b)
        return PF_ENOMEM;
    b->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (!b->iocp) {
        int rc = pf_error_last_win32();
        deallocate(p->alloc, b, sizeof(*b));
        return rc;
    }
    p->bstate = b;
    return PF_OK;
}

static void pf__iocp_free(pf__poller *p) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    int rounds = 0;

    /* The polls of removed sockets still refer to their state: wait for
       their cancellation to complete (a few seconds at most). */
    while (b->ndead > 0 && rounds++ < 50) {
        OVERLAPPED_ENTRY e[16];
        ULONG n = 0, i;
        if (!GetQueuedCompletionStatusEx(b->iocp, e, 16, &n, 100, FALSE))
            continue;
        for (i = 0; i < n; i++)
            if (e[i].lpCompletionKey == PF__KEY_AFD) {
                pf__wreg *w = (pf__wreg *)((char *)e[i].lpOverlapped
                                           - offsetof(pf__wreg, iosb));
                w->pending = false;
                pf__drop(b->dead, &b->ndead, w);
                deallocate(p->alloc, w, sizeof(*w));
            }
    }
    if (b->afd)
        CloseHandle(b->afd);
    CloseHandle(b->iocp);
    /* Whatever did not complete is leaked rather than freed under the
       driver. */
    deallocate(p->alloc, b->queue, b->qcap * sizeof(pf__wreg *));
    if (b->ndead == 0)
        deallocate(p->alloc, b->dead, b->dcap * sizeof(pf__wreg *));
    deallocate(p->alloc, b, sizeof(*b));
}

static int pf__iocp_add(pf__poller *p, pf__preg *r) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    pf__wreg *w;
    int rc = PF_OK;

    if (r->kind == PF__PK_FD)
        return PF_ENOSYS;
    w = (pf__wreg *)zallocate(p->alloc, sizeof(*w));
    if (!w)
        return PF_ENOMEM;
    w->id = w->post_id = r->id;
    w->kind = r->kind;
    w->iocp = b->iocp;
    w->base = INVALID_SOCKET;
    switch (r->kind) {
    case PF__PK_SOCKET:
        rc = pf__afd_open(b);
        if (!rc) {
            w->base = pf__base_socket((SOCKET)r->h);
            if (w->base == INVALID_SOCKET)
                rc = PF_ENOTSOCK;
        }
        break;
    case PF__PK_PIPE:
        if (!((pf_pipe_t *)r->obj)->pf__overlapped) {
            rc = PF_ENOSYS;
            break;
        }
        w->event = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!w->event)
            rc = pf_error_last_win32();
        w->object = w->event;
        break;
    case PF__PK_PROCESS:
        if (r->h == (intptr_t)-1)
            rc = PF_EINVAL;
        w->object = (HANDLE)r->h;
        break;
    default:
        w->object = (HANDLE)r->h;
        break;
    }
    if (rc) {
        deallocate(p->alloc, w, sizeof(*w));
        return rc;
    }
    r->be = w;
    pf__enqueue(p, w);
    if (!w->queued) {
        if (w->event)
            CloseHandle(w->event);
        deallocate(p->alloc, w, sizeof(*w));
        r->be = NULL;
        return PF_ENOMEM;
    }
    return PF_OK;
}

static int pf__iocp_update(pf__poller *p, pf__preg *r) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    pf__wreg *w = (pf__wreg *)r->be;

    if (r->kind == PF__PK_SOCKET) {
        /* A poll for other events: cancel it, the completion requeues. */
        if (w->pending)
            pf__afd_cancel(b, w);
        else if (r->armed)
            pf__enqueue(p, w);
        if (r->armed)
            w->closed = false;
        return PF_OK;
    }
    /* Pipes and waits that fire for a disarmed registration are dropped by
       the core, and not armed again. */
    if (r->armed)
        pf__enqueue(p, w);
    return PF_OK;
}

static void pf__iocp_remove(pf__poller *p, pf__preg *r) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    pf__wreg *w = (pf__wreg *)r->be;

    if (!w)
        return;
    if (w->queued)
        pf__drop(b->queue, &b->nqueue, w);
    w->id = 0;
    r->be = NULL;
    if (r->kind == PF__PK_SOCKET) {
        if (w->pending) {
            pf__afd_cancel(b, w);
            if (pf__push(p, &b->dead, &b->ndead, &b->dcap, w) == PF_OK)
                return;
            /* Without room to remember it, leak it: freeing it now would
               let the driver write into freed memory. */
            return;
        }
    } else {
        if (r->kind == PF__PK_PIPE)
            pf__pipe_cancel(r, w);
        pf__wait_stop(w);
        if (w->event)
            CloseHandle(w->event);
    }
    deallocate(p->alloc, w, sizeof(*w));
}

/* Arms the queued registrations. */
static void pf__arm_queued(pf__poller *p) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    size_t i, n = b->nqueue;

    for (i = 0; i < n; i++) {
        pf__wreg *w = b->queue[i];
        pf__preg *r = pf__poller_find(p, w->id);
        w->queued = false;
        if (!r || !r->armed)
            continue;
        switch (w->kind) {
        case PF__PK_SOCKET:
            if (!w->pending && !w->closed)
                pf__afd_submit(p, r, w);
            break;
        case PF__PK_PIPE:
            pf__pipe_arm(p, r, w);
            break;
        case PF__PK_PIPE_SERVER: {
            intptr_t h;
            int rc = pf__pipe_server_poll_handle(
                (pf_pipe_server_t *)r->obj, &h
            );
            if (rc) {
                pf__poller_emit(p, r->id, PF_POLL_ERR);
                break;
            }
            w->object = (HANDLE)h;
            pf__wait_start(p, r, w);
            break;
        }
        default:
            pf__wait_start(p, r, w);
            break;
        }
    }
    /* Arming may have queued nothing new; keep what was added meanwhile. */
    if (b->nqueue > n)
        memmove(b->queue, b->queue + n, (b->nqueue - n) * sizeof(*b->queue));
    b->nqueue -= n;
}

static void pf__wait_done(pf__poller *p, int id) {
    pf__preg *r = pf__poller_find(p, id);
    pf__wreg *w;
    if (!r)
        return;
    w = (pf__wreg *)r->be;
    if (!w)
        return;
    if (w->wait) {
        /* The callback has run; this only releases the wait. */
        UnregisterWait(w->wait);
        w->wait = NULL;
    }
    switch (w->kind) {
    case PF__PK_PIPE:
        pf__pipe_done(p, r, w);
        break;
    case PF__PK_PIPE_SERVER:
        pf__poller_emit(p, r->id, PF_POLL_IN);
        break;
    case PF__PK_WATCHER:
        pf__poller_emit(p, r->id, PF_POLL_IN);
        break;
    default:
        pf__poller_emit(p, r->id, PF_POLL_FIRED);
        break;
    }
    pf__enqueue(p, w);
}

static int pf__iocp_wait(pf__poller *p, int64_t timeout_ns) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    OVERLAPPED_ENTRY e[64];
    size_t room;
    ULONG n = 0, i;
    int ms;

    pf__arm_queued(p);
    room = pf__poller_room(p);
    if (room == 0)
        return PF_OK;
    ms = p->count ? 0 : pf__poller_ms(timeout_ns);
    if (!GetQueuedCompletionStatusEx(
            b->iocp,
            e,
            (ULONG)(room < 64 ? room : 64),
            &n,
            ms < 0 ? INFINITE : (DWORD)ms,
            FALSE
        )) {
        DWORD err = GetLastError();
        return err == WAIT_TIMEOUT ? PF_OK : pf_error_from_win32(err);
    }
    for (i = 0; i < n; i++) {
        switch (e[i].lpCompletionKey) {
        case PF__KEY_AFD:
            pf__afd_done(
                p,
                (pf__wreg *)((char *)e[i].lpOverlapped
                             - offsetof(pf__wreg, iosb))
            );
            break;
        case PF__KEY_WAIT:
            pf__wait_done(p, (int)e[i].dwNumberOfBytesTransferred);
            break;
        case PF__KEY_WAKE:
            InterlockedExchange(&b->wake_pending, 0);
            pf__poller_emit(p, 0, PF_POLL_FIRED);
            break;
        default:
            break;
        }
    }
    return PF_OK;
}

static int pf__iocp_wake(pf__poller *p) {
    pf__wbe *b = (pf__wbe *)p->bstate;
    if (InterlockedExchange(&b->wake_pending, 1) != 0)
        return PF_OK;
    if (!PostQueuedCompletionStatus(b->iocp, 0, PF__KEY_WAKE, NULL)) {
        InterlockedExchange(&b->wake_pending, 0);
        return pf_error_last_win32();
    }
    return PF_OK;
}

static const pf__pbackend pf__iocp_backend = {
    pf__iocp_init,   pf__iocp_free, pf__iocp_add,  pf__iocp_update,
    pf__iocp_remove, pf__iocp_wait, pf__iocp_wake,
};

const pf__pbackend *const pf__poller_native = &pf__iocp_backend;
