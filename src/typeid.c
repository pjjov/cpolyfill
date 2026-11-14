/*
    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0

    <cpolyfill/typeid.h>: a table indexed by type id, built from
    PF_TYPEID, and per-type switches for the limits, loads and stores (each
    integer is accessed through its own type).
*/

#include "internal.h"

#include <cpolyfill/typeid.h>

#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#ifdef __SIZEOF_INT128__
    #define PF__INT128 1
__extension__ typedef __int128 pf__i128;
__extension__ typedef unsigned __int128 pf__u128;
#else
    #define PF__INT128 0
#endif

enum { PF__TK_NONE, PF__TK_S, PF__TK_U, PF__TK_F, PF__TK_A, PF__TK_O };

typedef struct pf__typeinfo {
    const char *name;
    unsigned short kind;
    unsigned short size; /* of integers and floats, else 0 */
} pf__typeinfo;

/* Names are stringized here, before `bool` could expand to `_Bool`. */
#define PF__TI(kind, name, size, i) [i] = { name, kind, size },
#define PF__TI_S(u, l, t, f, i) PF__TI(PF__TK_S, #l, sizeof(t), i)
#define PF__TI_U(u, l, t, f, i) PF__TI(PF__TK_U, #l, sizeof(t), i)
#define PF__TI_F(u, l, t, f, i) PF__TI(PF__TK_F, #l, sizeof(t), i)
#define PF__TI_A(u, l, t, f, i) PF__TI(PF__TK_A, #l, 0, i)
#define PF__TI_O(u, l, t, f, i) PF__TI(PF__TK_O, #l, 0, i)
#define PF__IGNORE(u, l, t, f, i)

static const pf__typeinfo pf__types[PF_TYPE_ATOMIC_UINTMAX + 1] = {
    PF_TYPEID(PF__TI_S, PF__TI_U, PF__TI_F, PF__TI_A, PF__TI_O)
};

static const pf__typeinfo *pf__type(int type) {
    if (type < 0 || type >= (int)(sizeof(pf__types) / sizeof(pf__types[0]))
        || !pf__types[type].name)
        return NULL;
    return &pf__types[type];
}

static int pf__kind(int type) {
    const pf__typeinfo *t = pf__type(type);
#if PF__INT128
    if (type == PF_TYPE_INT128)
        return PF__TK_S;
    if (type == PF_TYPE_UINT128)
        return PF__TK_U;
#endif
    return t ? t->kind : PF__TK_NONE;
}

/* ---- Classification ---------------------------------------------------- */

PF_API bool pf_type_is_integer(int type) {
    int k = pf__kind(type);
    return k == PF__TK_S || k == PF__TK_U;
}

PF_API bool pf_type_is_signed(int type) { return pf__kind(type) == PF__TK_S; }

PF_API bool pf_type_is_unsigned(int type) { return pf__kind(type) == PF__TK_U; }

PF_API bool pf_type_is_float(int type) { return pf__kind(type) == PF__TK_F; }

PF_API bool pf_type_is_atomic(int type) { return pf__kind(type) == PF__TK_A; }

PF_API const char *pf_type_name(int type) {
    const pf__typeinfo *t = pf__type(type);
    return t ? t->name : NULL;
}

PF_API size_t pf_type_size(int type) {
    const pf__typeinfo *t = pf__type(type);

    if (!t)
        return 0;
    if (t->size)
        return t->size;
    switch (type) {
    case PF_TYPE_PTR:
    case PF_TYPE_NULLPTR:
        return sizeof(void *);
    case PF_TYPE_CHAR:
        return sizeof(char);
    case PF_TYPE_CSTRING:
        return sizeof(char *);
    case PF_TYPE_MAX_ALIGN:
#if PF_CC_MSVC && !PF_CC_CLANG
        return sizeof(double); /* MSVC's max_align_t */
#else
        return sizeof(max_align_t);
#endif
    case PF_TYPE_BOOL:
        return sizeof(bool);
#if PF__INT128
    case PF_TYPE_INT128:
    case PF_TYPE_UINT128:
        return sizeof(pf__i128);
#endif
    case PF_TYPE_FILE:
        return sizeof(FILE *);
    case PF_TYPE_FPOS:
        return sizeof(fpos_t);
    case PF_TYPE_TIME:
        return sizeof(time_t);
    case PF_TYPE_CLOCK:
        return sizeof(clock_t);
    case PF_TYPE_TM:
        return sizeof(struct tm);
    case PF_TYPE_TIMESPEC:
        return sizeof(struct timespec);
    case PF_TYPE_MB_STATE:
        return sizeof(mbstate_t);
    case PF_TYPE_CHAR8:
        return sizeof(unsigned char);
    case PF_TYPE_CHAR16:
        return sizeof(uint_least16_t);
    case PF_TYPE_CHAR32:
        return sizeof(uint_least32_t);
    case PF_TYPE_WCHAR:
        return sizeof(wchar_t);
    case PF_TYPE_WINT:
        return sizeof(wint_t);
    case PF_TYPE_WCTRANS:
        return sizeof(wctrans_t);
    case PF_TYPE_WCTYPE:
        return sizeof(wctype_t);
    case PF_TYPE_JMP_BUF:
        return sizeof(jmp_buf);
    case PF_TYPE_SIG_ATOMIC:
        return sizeof(sig_atomic_t);
    case PF_TYPE_VA_LIST:
        return sizeof(va_list);
    default:
        return 0;
    }
}

/* ---- Limits ------------------------------------------------------------ */

#define PF__MAX(u, l, t, f, i)     \
    case i:                        \
        return (uintmax_t)u##_MAX;
#define PF__MIN(u, l, t, f, i)    \
    case i:                       \
        return (intmax_t)u##_MIN;

PF_API uintmax_t pf_type_int_max(int type) {
    switch (type) {
        PF_TYPEID(PF__MAX, PF__MAX, PF__IGNORE, PF__IGNORE, PF__IGNORE)
#if PF__INT128
    case PF_TYPE_INT128:
        return INTMAX_MAX;
    case PF_TYPE_UINT128:
        return UINTMAX_MAX;
#endif
    default:
        return 0;
    }
}

PF_API intmax_t pf_type_int_min(int type) {
    switch (type) {
        PF_TYPEID(PF__MIN, PF__IGNORE, PF__IGNORE, PF__IGNORE, PF__IGNORE)
#if PF__INT128
    case PF_TYPE_INT128:
        return INTMAX_MIN;
#endif
    default:
        return 0;
    }
}

/* ---- Loads and stores -------------------------------------------------- */

#define PF__LOAD_S(u, l, t, f, i)                       \
    case i:                                             \
        *out = (uintmax_t)(intmax_t)(*(const t *)item); \
        return PF_OK;
#define PF__LOAD_U(u, l, t, f, i)             \
    case i:                                   \
        *out = (uintmax_t)(*(const t *)item); \
        return PF_OK;

PF_API int pf_type_int_load(int type, const void *item, uintmax_t *out) {
    if (!item || !out)
        return PF_EINVAL;
    switch (type) {
        PF_TYPEID(PF__LOAD_S, PF__LOAD_U, PF__IGNORE, PF__IGNORE, PF__IGNORE)
#if PF__INT128
    case PF_TYPE_INT128: {
        pf__i128 v = *(const pf__i128 *)item;
        if (v < INTMAX_MIN || v > INTMAX_MAX)
            return PF_ERANGE;
        *out = (uintmax_t)(intmax_t)v;
        return PF_OK;
    }
    case PF_TYPE_UINT128: {
        pf__u128 v = *(const pf__u128 *)item;
        if (v > UINTMAX_MAX)
            return PF_ERANGE;
        *out = (uintmax_t)v;
        return PF_OK;
    }
#endif
    default:
        return PF_EINVAL;
    }
}

/* The low `size` bytes of `v`, as a two's complement integer of that size
   (converting to a signed type out of its range is implementation-defined,
   so signed integers are written through their bits). */
static int pf__store_bits(void *out, size_t size, uintmax_t v) {
    switch (size) {
    case 1: {
        uint8_t b = (uint8_t)v;
        memcpy(out, &b, 1);
        return PF_OK;
    }
    case 2: {
        uint16_t b = (uint16_t)v;
        memcpy(out, &b, 2);
        return PF_OK;
    }
    case 4: {
        uint32_t b = (uint32_t)v;
        memcpy(out, &b, 4);
        return PF_OK;
    }
    case 8: {
        uint64_t b = (uint64_t)v;
        memcpy(out, &b, 8);
        return PF_OK;
    }
    default:
        return PF_EINVAL;
    }
}

#define PF__STORE_S(u, l, t, f, i)                     \
    case i:                                            \
        return pf__store_bits(out, sizeof(t), *value);
#define PF__STORE_U(u, l, t, f, i) \
    case i:                        \
        *(t *)out = (t) * value;   \
        return PF_OK;

PF_API int pf_type_int_store(int type, const uintmax_t *value, void *out) {
    if (!value || !out)
        return PF_EINVAL;
    switch (type) {
        PF_TYPEID(PF__STORE_S, PF__STORE_U, PF__IGNORE, PF__IGNORE, PF__IGNORE)
#if PF__INT128
    case PF_TYPE_INT128: {
        /* (intmax_t)*value, without the implementation-defined conversion */
        uintmax_t v = *value;
        intmax_t s = v <= (uintmax_t)INTMAX_MAX
            ? (intmax_t)v
            : -(intmax_t)(UINTMAX_MAX - v) - 1;
        *(pf__i128 *)out = s;
        return PF_OK;
    }
    case PF_TYPE_UINT128:
        *(pf__u128 *)out = *value;
        return PF_OK;
#endif
    default:
        return PF_EINVAL;
    }
}

/* ---- Equivalent types -------------------------------------------------- */

PF_API int pf_type_int_as_base(int type) {
    static const int bases[] = {
        PF_TYPE_SCHAR, PF_TYPE_SHRT, PF_TYPE_INT, PF_TYPE_LONG, PF_TYPE_LLONG,
    };
    size_t size, k;

    if (!pf_type_is_integer(type))
        return PF_TYPE_ERROR;
    size = pf_type_size(type);
    for (k = 0; k < sizeof(bases) / sizeof(bases[0]); k++)
        if (pf_type_size(bases[k]) == size)
            return pf_type_is_signed(type) ? bases[k] : bases[k] + 1;
    return PF_TYPE_ERROR;
}

PF_API int pf_type_int_as_fixed(int type) {
    static const int fixed[] = {
        PF_TYPE_INT8,
        PF_TYPE_INT16,
        PF_TYPE_INT32,
        PF_TYPE_INT64,
    };
    size_t size, k;

    if (!pf_type_is_integer(type))
        return PF_TYPE_ERROR;
    size = pf_type_size(type);
    for (k = 0; k < sizeof(fixed) / sizeof(fixed[0]); k++)
        if (pf_type_size(fixed[k]) == size)
            return pf_type_is_signed(type) ? fixed[k] : fixed[k] + 1;
    return PF_TYPE_ERROR;
}
