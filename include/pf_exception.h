/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides a generic exception handling mechanism
    that can be used by different libraries simultaneously.

    Exceptions are handled by the `pf_exception_stack_t` object, which
    keeps track of registered exception handlers and their masking values.
    Exceptions can be identified by a module number and an error code.

    Handlers are registered by passing the stack to `pf_try_catch` function.
    Depending on the return value of that function, the user's program should
    continue with the `try` (NULL value) or the `catch` block (exception).

    If no handlers are registered, the exception will not be emitted and the
    given `code` will be returned. Users can define catch-all handlers that
    surround the entire program with 0 as masks for `module` and `code`.

    If the maximum number of handlers is reached, further calls will return
    `NULL`. This can be changed by redefining `PF_ON_MAX_EXCEPTIONS`.

    Libraries should expose functionality for setting the stack object.
    In multithreaded programs, there should be separate stacks for each
    thread (thread_local), or a single one protected by a lock.

    Reference API:
    - pf_init_exception_stack(stack, capacity, msgBuffer, msgCapacity)
    - pf_try_catch(stack, module, code)
    - pf_throw(stack, code, msg)
    - pf_throwf(stack, code, fmt, ...)
    - pf_vthrowf(stack, code, fmt, args)
    - pf_rethrow(stack, e);

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

#ifndef PF_EXCEPTION
#define PF_EXCEPTION

#ifndef PF_API
    #define PF_API static inline
#endif

#ifndef PF_INLINE
    #define PF_INLINE static inline
#endif

#include <setjmp.h>

#ifndef PF_EXCEPTION_NO_STDIO
    #include <stdarg.h>
    #include <stdio.h>
#endif

#ifndef PF_EXCEPTION_MODULE
    #define PF_EXCEPTION_MODULE 0
#endif

#ifndef PF_EXCEPTION_STACK_SIZE
    #define PF_EXCEPTION_STACK_SIZE
#endif

#ifndef PF_ON_MAX_EXCEPTIONS
    #define PF_ON_MAX_EXCEPTIONS return NULL
#endif

#ifdef NDEBUG
    #define PF__DBG_LOC NULL, NULL, NULL
#else
    #define PF__DBG_LOC __LINE__, __FILE__, __func__
#endif

typedef struct pf_exception_t {
    int line;
    int code;
    int module;
    const char *file;
    const char *func;
    const char *msg;
} pf_exception_t;

typedef struct pf_exception_handler_t {
    jmp_buf *jump;
    int module;
    int code;
} pf_exception_handler_t;

typedef struct pf_exception_stack_t {
    pf_exception_t current;

    int msgCapacity;
    char *msgBuffer;

    int capacity;
    int length;
    pf_exception_handler_t buffer[PF_EXCEPTION_STACK_SIZE];
} pf_exception_stack_t;

#define pf_throw(stack, code, msg)                 \
    pf__throw((stack), (code), PF__DBG_LOC, (msg))
#define pf_throwf(stack, code, fmt, ...)                         \
    pf__throwf((stack), (code), PF__DBG_LOC, (fmt), __VA_ARGS__)
#define pf_vthrowf(stack, code, fmt, args)                   \
    pf__vthrowf((stack), (code), PF__DBG_LOC, (fmt), (args))

PF_API void pf_init_exception_stack(
    pf_exception_stack_t *stack, int capacity, int msgCapacity, char *msgBuffer
) {
    if (stack) {
        stack->capacity = capacity;
        stack->length = 0;
        stack->msgBuffer = msgBuffer;
        stack->msgCapacity = msgCapacity;
    }
}

PF_API pf_exception_t *pf_try_catch(
    pf_exception_stack_t *stack, int module, int code
) {
    if (!stack)
        return NULL;

    if (stack->length >= stack->capacity)
        PF_ON_MAX_EXCEPTIONS;

    jmp_buf jump;

    if (0 == setjmp(jump)) {
        int index = stack->length++;
        stack->buffer[index].jump = &jump;
        stack->buffer[index].module = module;
        stack->buffer[index].code = code;
        return NULL;
    }

    return &stack->current;
}

PF_API int pf_rethrow(pf_exception_stack_t *stack, pf_exception_t *e) {
    if (!stack || !e)
        return -1;

    if (&stack->current != e)
        stack->current = *e;

    for (int i = stack->length - 1; i > 0; i--) {
        pf_exception_handler_t *handler = &stack->buffer[i];
        if (handler->module != 0 && handler->module != e->module)
            continue;
        if (handler->code != 0 && handler->code != e->code)
            continue;

        stack->length = i;
        longjmp(*handler->jump, 1);
    }

    return e->code;
}

PF_API int pf__throw(
    pf_exception_stack_t *stack,
    int code,
    int line,
    const char *file,
    const char *func,
    const char *msg
) {
    if (!stack || code == 0)
        return code;

    pf_exception_t *e = &stack->current;
    e->module = PF_EXCEPTION_MODULE;
    e->code = code;
    e->line = line;
    e->file = file;
    e->func = func;
    e->msg = msg;
    return pf_rethrow(stack, e);
}

#ifndef PF_EXCEPTION_NO_STDIO

PF_API int pf__vthrowf(
    pf_exception_stack_t *stack,
    int code,
    int line,
    const char *file,
    const char *func,
    const char *fmt,
    va_list args
) {
    if (!stack || code == 0)
        return code;

    const char *msg = fmt;

    if (!stack->msgBuffer || stack->msgCapacity <= 0) {
        int len = vsnprintf(stack->msgBuffer, stack->msgCapacity, fmt, args);
        if (len >= 0 || len < stack->msgCapacity)
            msg = stack->msgBuffer;
    }

    return pf__throw(stack, code, line, file, func, msg);
}

PF_API int pf__throwf(
    pf_exception_stack_t *stack,
    int code,
    int line,
    const char *file,
    const char *func,
    const char *fmt,
    ...
) {
    va_list args;
    va_start(args, fmt);
    int result = pf__vthrowf(stack, code, line, file, func, fmt, args);
    va_end(args);
    return result;
}

#endif

#endif
