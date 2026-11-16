/** cpolyfill - Filling the gaps between C standards and compilers

    Exceptions on setjmp / longjmp, which several libraries can share.

    An exception stack keeps the try blocks that are running (innermost
    first) and the exception being handled. A throw jumps to the innermost
    try block whose filter matches; when none does, nothing happens and the
    throw returns its code, so it doubles as an error return.

        #define PF_EXCEPTION_MODULE 7   // this library's exceptions
        #include <cpolyfill/exception.h>

        static int parse(pf_exception_stack_t *s, const char *text) {
            if (!*text)
                return PF_THROWF(s, PF_EINVAL, "empty input at %p", text);
            ...
        }

        pf_exception_stack_t *s = pf_exception_thread_stack();
        PF_TRY(s, 0, 0) {
            parse(s, "");
        } PF_CATCH(e) {
            fprintf(stderr, "%s:%d: %s\n", e->file, e->line, e->msg);
        } PF_TRY_END;

    Try blocks:

        PF_TRY(stack, module, code) { ... } PF_CATCH(e) { ... } PF_TRY_END;
                Runs the first block; an exception thrown on `stack` while
                it runs (also in the functions it calls) jumps to the
                second, where `e` is a `const pf_exception_t *` to it (good
                until the next throw on `stack`). `module` and `code` pick
                what this block catches, 0 meaning any. PF_CATCH is
                required. With a NULL `stack` the first block just runs.
                Rules, as for setjmp:
                - Do not leave the first block with return, break, continue
                  or goto: the try block would stay on the stack. The next
                  try block that ends notices and fails an assertion.
                - Local variables changed in the first block and read in
                  the second (or after) must be `volatile`.
                - A throw through C++ frames does not run their
                  destructors.

    Throwing (the call site and PF_EXCEPTION_MODULE are added):

        int PF_THROW(stack, code, msg)
        int PF_THROWF(stack, code, fmt, ...)
        int PF_VTHROWF(stack, code, fmt, va_list)
                Throws `code` with a message, copied into the stack's
                message buffer (cut to fit; without a buffer, `msg` / `fmt`
                itself is kept). Returns `code` when no try block catches
                it; a `code` of 0 and a NULL `stack` throw nothing.
        PF_EXCEPTION_MODULE     The module of exceptions thrown with the
                                macros (default 0); define it before
                                including this header.
        PF_EXCEPTION_NO_LOCATION
                                Define it before including this header to
                                leave the call site out (`file` and `func`
                                NULL, `line` 0), so the file names are not
                                in the program.

    Types and functions:

        pf_exception_t          module, code   what was thrown
                                file, line, func  where (see above)
                                msg            the message, or NULL
        pf_exception_stack_t    Private fields; one stack per thread.
        void pf_exception_stack_init(pf_exception_stack_t *s, char *msg_buf,
                                     size_t msg_size)
                An empty stack, copying messages into `msg_buf` (may be
                NULL with `msg_size` 0). Needs no clean-up.
        pf_exception_stack_t *pf_exception_thread_stack(void)
                The calling thread's own stack (with a 256-byte message
                buffer), for libraries that do not ask for one.
        int pf_throw_at(pf_exception_stack_t *s, int module, int code,
                        const char *file, int line, const char *func,
                        const char *msg)
        int pf_throwf_at(..., const char *fmt, ...)
        int pf_vthrowf_at(..., const char *fmt, va_list ap)
                What the macros call, for wrappers that pass on their own
                call site.
        int pf_rethrow(pf_exception_stack_t *s, const pf_exception_t *e)
                Throws `e` again (from a PF_CATCH block, to an outer try
                block), or an exception of another stack. Returns its code
                when nothing catches it, PF_EINVAL for a NULL `e`.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_EXCEPTION_H
#define CPOLYFILL_EXCEPTION_H

#include <cpolyfill/attribute.h>
#include <cpolyfill/platform.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#ifndef PF_EXCEPTION_MODULE
    #define PF_EXCEPTION_MODULE 0
#endif

PF_BEGIN_DECLS

typedef struct pf_exception_t {
    int module;
    int code;
    const char *file;
    int line;
    const char *func;
    const char *msg;
} pf_exception_t;

typedef struct pf_exception_frame_t {
    /* Private. */
    jmp_buf pf__jump;
    struct pf_exception_frame_t *pf__prev;
    int pf__module;
    int pf__code;
} pf_exception_frame_t;

typedef struct pf_exception_stack_t {
    /* Private. */
    pf_exception_frame_t *pf__top;
    pf_exception_t pf__current;
    char *pf__msg_buf;
    size_t pf__msg_size;
} pf_exception_stack_t;

PF_API void pf_exception_stack_init(
    pf_exception_stack_t *s, char *msg_buf, size_t msg_size
);
PF_API pf_exception_stack_t *pf_exception_thread_stack(void);

PF_API int pf_throw_at(
    pf_exception_stack_t *s,
    int module,
    int code,
    const char *file,
    int line,
    const char *func,
    const char *msg
);
PF_PRINTF(7, 8) PF_API int pf_throwf_at(
    pf_exception_stack_t *s,
    int module,
    int code,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
);
PF_PRINTF(7, 0) PF_API int pf_vthrowf_at(
    pf_exception_stack_t *s,
    int module,
    int code,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list ap
);
PF_API int pf_rethrow(pf_exception_stack_t *s, const pf_exception_t *e);

/* What the try macros call. */
PF_API void pf__try_push(
    pf_exception_stack_t *s, pf_exception_frame_t *f, int module, int code
);
PF_API void pf__try_pop(pf_exception_stack_t *s, pf_exception_frame_t *f);
PF_API const pf_exception_t *pf__try_caught(pf_exception_stack_t *s);

PF_END_DECLS

#ifdef PF_EXCEPTION_NO_LOCATION
    #define PF__EXC_LOC NULL, 0, NULL
#else
    #define PF__EXC_LOC __FILE__, __LINE__, __func__
#endif

#define PF_THROW(stack, code, msg) \
    pf_throw_at((stack), PF_EXCEPTION_MODULE, (code), PF__EXC_LOC, (msg))
#define PF_THROWF(stack, code, ...) \
    pf_throwf_at((stack), PF_EXCEPTION_MODULE, (code), PF__EXC_LOC, __VA_ARGS__)
#define PF_VTHROWF(stack, code, fmt, ap)                                 \
    pf_vthrowf_at(                                                       \
        (stack), PF_EXCEPTION_MODULE, (code), PF__EXC_LOC, (fmt), (ap) \
    )

/* setjmp is the whole controlling expression of an `if` compared with a
   constant, one of the uses the C standard allows. clang-format cannot lay
   out the block macros, nor code using them. */
/* clang-format off */
#define PF_TRY(stack, module, code)                                       \
    do {                                                                  \
        pf_exception_frame_t pf__exc_frame;                               \
        pf_exception_stack_t *const pf__exc_stack = (stack);              \
        pf__try_push(pf__exc_stack, &pf__exc_frame, (module), (code));    \
        if (setjmp(pf__exc_frame.pf__jump) == 0) {
#define PF_CATCH(e)                                                       \
            pf__try_pop(pf__exc_stack, &pf__exc_frame);                   \
        } else {                                                          \
            const pf_exception_t *const e = pf__try_caught(pf__exc_stack); \
            (void)e;
#define PF_TRY_END \
        }          \
    } while (0)
/* clang-format on */

#endif /* CPOLYFILL_EXCEPTION_H */
