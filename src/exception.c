/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/exception.h>: try blocks are frames on the caller's stack,
    linked from the innermost one.
*/

#include "internal.h"

#include <cpolyfill/assert.h>
#include <cpolyfill/exception.h>
#include <cpolyfill/stdc.h>

#include <stdio.h>
#include <string.h>

#define PF__EXC_THREAD_MSG 256
/* Formatted messages go through a buffer of this size first, since their
   arguments may point into the stack's own buffer (rethrowing with more
   context). */
#define PF__EXC_FORMAT_MAX 1024

PF_API void pf_exception_stack_init(
    pf_exception_stack_t *s, char *msg_buf, size_t msg_size
) {
    if (!s)
        return;
    memset(s, 0, sizeof(*s));
    s->pf__msg_buf = msg_size ? msg_buf : NULL;
    s->pf__msg_size = msg_buf ? msg_size : 0;
}

PF_API pf_exception_stack_t *pf_exception_thread_stack(void) {
    static PF_THREAD_LOCAL pf_exception_stack_t stack;
    static PF_THREAD_LOCAL char msg[PF__EXC_THREAD_MSG];
    static PF_THREAD_LOCAL bool ready;

    if (!ready) {
        pf_exception_stack_init(&stack, msg, sizeof(msg));
        ready = true;
    }
    return &stack;
}

/* ---- Try blocks -------------------------------------------------------- */

PF_API void pf__try_push(
    pf_exception_stack_t *s, pf_exception_frame_t *f, int module, int code
) {
    f->pf__module = module;
    f->pf__code = code;
    f->pf__prev = NULL;
    if (!s)
        return;
    f->pf__prev = s->pf__top;
    s->pf__top = f;
}

PF_API void pf__try_pop(pf_exception_stack_t *s, pf_exception_frame_t *f) {
    if (!s)
        return;
    if (s->pf__top != f)
        pf_assert_fail(
            "PF_TRY",
            __FILE__,
            __LINE__,
            "pf__try_pop",
            "an inner try block was left without reaching PF_CATCH "
            "(return, break or goto out of it)"
        );
    s->pf__top = f->pf__prev;
}

PF_API const pf_exception_t *pf__try_caught(pf_exception_stack_t *s) {
    return &s->pf__current;
}

/* ---- Throwing ---------------------------------------------------------- */

/* Copies `msg` into the stack's buffer (which `msg` may overlap), cut to
   fit; returns what the exception keeps. */
static const char *pf__exc_keep(pf_exception_stack_t *s, const char *msg) {
    size_t n;
    if (!msg || !s->pf__msg_buf)
        return msg;
    n = strlen(msg);
    if (n >= s->pf__msg_size)
        n = pf__utf8_truncate(msg, s->pf__msg_size - 1);
    memmove(s->pf__msg_buf, msg, n);
    s->pf__msg_buf[n] = '\0';
    return s->pf__msg_buf;
}

/* Jumps to the innermost matching try block, removing it and the ones
   inside it; returns the code if there is none. */
static int pf__exc_dispatch(pf_exception_stack_t *s) {
    const pf_exception_t *e = &s->pf__current;
    pf_exception_frame_t *f;

    for (f = s->pf__top; f; f = f->pf__prev) {
        if (f->pf__module != 0 && f->pf__module != e->module)
            continue;
        if (f->pf__code != 0 && f->pf__code != e->code)
            continue;
        s->pf__top = f->pf__prev;
        longjmp(f->pf__jump, 1);
    }
    return e->code;
}

PF_API int pf_throw_at(
    pf_exception_stack_t *s,
    int module,
    int code,
    const char *file,
    int line,
    const char *func,
    const char *msg
) {
    pf_exception_t *e;

    if (!s || code == 0)
        return code;
    e = &s->pf__current;
    e->module = module;
    e->code = code;
    e->file = file;
    e->line = line;
    e->func = func;
    e->msg = pf__exc_keep(s, msg);
    return pf__exc_dispatch(s);
}

PF_API int pf_vthrowf_at(
    pf_exception_stack_t *s,
    int module,
    int code,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list ap
) {
    char tmp[PF__EXC_FORMAT_MAX];

    if (!s || code == 0)
        return code;
    if (!s->pf__msg_buf || !fmt || vsnprintf(tmp, sizeof(tmp), fmt, ap) < 0)
        return pf_throw_at(s, module, code, file, line, func, fmt);
    return pf_throw_at(s, module, code, file, line, func, tmp);
}

PF_API int pf_throwf_at(
    pf_exception_stack_t *s,
    int module,
    int code,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
) {
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = pf_vthrowf_at(s, module, code, file, line, func, fmt, ap);
    va_end(ap);
    return rc;
}

PF_API int pf_rethrow(pf_exception_stack_t *s, const pf_exception_t *e) {
    if (!e)
        return PF_EINVAL;
    if (!s || e->code == 0)
        return e->code;
    if (e == &s->pf__current)
        return pf__exc_dispatch(s);
    return pf_throw_at(
        s, e->module, e->code, e->file, e->line, e->func, e->msg
    );
}
