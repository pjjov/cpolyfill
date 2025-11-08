/** Polyfill for C - Filling the gaps between C standards and compilers

    This file provides unique numeric identifiers for builtin C types.
    Defining them here enables other libraries to reuse them, ensuring
    consistent identifiers across various libraries.

    All builtin type identifiers are positive, while the negative
    range is reserved for special, non-primitive types. Custom types
    should ideally reserve their own namespace with an offset that
    is larger than PF__TYPE_STANDARD.

    SPDX-FileCopyrightText: 2025 Predrag Jovanović
    SPDX-License-Identifier: Apache-2.0

    Copyright 2025 Predrag Jovanović

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

#ifndef POLYFILL_TYPEID
#define POLYFILL_TYPEID

#ifndef PF_API
    #define PF_API static inline
#endif

#ifdef __has_builtin
    #if __has_builtin(__builtin_unreachable)
        #define pf__unreachable __builtin_unreachable
    #endif
#endif

#ifndef pf__unreachable
    #define pf__unreachable()
#endif

/* clang-format off */

/*
    XS - signed integers
    XU - unsigned integers
    XF - floating point numbers
    XA - atomic types
    XO - others
*/
#define PF_TYPEID(XS, XU, XF, XA, XO)                                  \
    /* uppercase,       lowercase,       type,            flags, id */ \
    XO(VOID,            void,            void,                 0, 0  ) \
    XO(PTR,             ptr,             void *,               0, 1  ) \
    XO(CHAR,            char,            char,                 0, 2  ) \
    XO(CSTRING,         cstring,         char *,               0, 3  ) \
    XS(SCHAR,           schar,           signed char,          0, 4  ) \
    XU(UCHAR,           uchar,           unsigned char,        0, 5  ) \
    XS(SHRT,            shrt,            short,                0, 6  ) \
    XU(USHRT,           ushrt,           unsigned short,       0, 7  ) \
    XS(INT,             int,             int,                  0, 8  ) \
    XU(UINT,            uint,            unsigned int,         0, 9  ) \
    XS(LONG,            long,            long,                 0, 10 ) \
    XU(ULONG,           ulong,           unsigned long,        0, 11 ) \
    XS(LLONG,           llong,           long long,            0, 12 ) \
    XU(ULLONG,          ullong,          unsigned long long,   0, 13 ) \
    XF(FLOAT,           float,           float,                0, 21 ) \
    XF(DOUBLE,          double,          double,               0, 22 ) \
    XF(LDOUBLE,         ldouble,         long double,          0, 23 ) \
    /* <stddef.h> types */                                             \
    XS(PTRDIFF,         ptrdiff,         ptrdiff_t,            0, 14 ) \
    XU(SIZE,            size,            size_t,               0, 15 ) \
    XO(MAX_ALIGN,       max_align,       max_align_t,          0, 16 ) \
    XO(NULLPTR,         nullptr,         nullptr_t,            0, 17 ) \
    /* <stdbool.h> */                                                  \
    XO(BOOL,            bool,            _Boolean,             0, 18 ) \
    /* <math.h> types */                                               \
    XF(MFLOAT,           mfloat,        float_t,               0, 24 ) \
    XF(MDOUBLE,          mdouble,       double_t,              0, 25 ) \
    /* <stdint.h> types */                                             \
    XS(INTPTR,           intptr,        intptr_t,              0, 28 ) \
    XU(UINTPTR,          uintptr,       uintptr_t,             0, 29 ) \
    XS(INTMAX,           intmax,        intmax_t,              0, 30 ) \
    XU(UINTMAX,          uintmax,       uintmax_t,             0, 31 ) \
    XS(INT8,             int8,          int8_t,                0, 32 ) \
    XU(UINT8,            uint8,         uint8_t,               0, 33 ) \
    XS(INT16,            int16,         int16_t,               0, 34 ) \
    XU(UINT16,           uint16,        uint16_t,              0, 35 ) \
    XS(INT32,            int32,         int32_t,               0, 36 ) \
    XU(UINT32,           uint32,        uint32_t,              0, 37 ) \
    XS(INT64,            int64,         int64_t,               0, 38 ) \
    XU(UINT64,           uint64,        uint64_t,              0, 39 ) \
    XS(INT_FAST8,        int_fast8,     int_fast8_t,           0, 40 ) \
    XU(UINT_FAST8,       uint_fast8,    uint_fast8_t,          0, 41 ) \
    XS(INT_FAST16,       int_fast16,    int_fast16_t,          0, 42 ) \
    XU(UINT_FAST16,      uint_fast16,   uint_fast16_t,         0, 43 ) \
    XS(INT_FAST32,       int_fast32,    int_fast32_t,          0, 44 ) \
    XU(UINT_FAST32,      uint_fast32,   uint_fast32_t,         0, 45 ) \
    XS(INT_FAST64,       int_fast64,    int_fast64_t,          0, 46 ) \
    XU(UINT_FAST64,      uint_fast64,   uint_fast64_t,         0, 47 ) \
    XS(INT_LEAST8,       int_least8,    int_least8_t,          0, 48 ) \
    XU(UINT_LEAST8,      uint_least8,   uint_least8_t,         0, 49 ) \
    XS(INT_LEAST16,      int_least16,   int_least16_t,         0, 50 ) \
    XU(UINT_LEAST16,     uint_least16,  uint_least16_t,        0, 51 ) \
    XS(INT_LEAST32,      int_least32,   int_least32_t,         0, 52 ) \
    XU(UINT_LEAST32,     uint_least32,  uint_least32_t,        0, 53 ) \
    XS(INT_LEAST64,      int_least64,   int_least64_t,         0, 54 ) \
    XU(UINT_LEAST64,     uint_least64,  uint_least64_t,        0, 55 ) \
    /* <stdio.h> types */                                              \
    XO(FILE,             file,          FILE *,                0, 100) \
    XO(FPOS,             fpos,          fpos_t,                0, 101) \
    /* <threads.h> types */                                            \
    XO(THRD,             thrd,          thrd_t,                0, 102) \
    XO(THRD_START,       thrd_start,    thrd_start_t,          0, 103) \
    XO(MTX,              mtx,           mtx_t,                 0, 104) \
    XO(CND,              cnd,           cnd_t,                 0, 105) \
    XO(TSS,              tss,           tss_t,                 0, 106) \
    XO(TSS_DTOR,         tss_dtor,      tss_dtor_t,            0, 107) \
    /* <time.h> types */                                               \
    XO(TIME,             time,          time_t,                0, 108) \
    XO(CLOCK,            clock,         clock_t,               0, 109) \
    XO(TM,               tm,            struct tm,             0, 110) \
    XO(TIMESPEC,         timespec,      struct timespec,       0, 111) \
    /* <uchar.h> types */                                              \
    XO(MB_STATE,         mb_state,      mb_state_t,            0, 112) \
    XO(CHAR8,            char8,         char8_t,               0, 113) \
    XO(CHAR16,           char16,        char16_t,              0, 114) \
    XO(CHAR32,           char32,        char32_t,              0, 115) \
    /* <wchar.h> and <wctype.h> types */                               \
    XO(WCHAR,            wchar,         wchar_t,               0, 116) \
    XO(WINT,             wint,          wint_t,                0, 117) \
    XO(WCTRANS,          wctrans,       wctrans_t,             0, 118) \
    XO(WCTYPE,           wctype,        wctype_t,              0, 119) \
    /* <stdjump.h> */                                                  \
    XO(JMP_BUF,          jmp_buf,       jmp_buf,               0, 120) \
    /* <signal.h> */                                                   \
    XO(SIG_ATOMIC,       sig_atomic,    sig_atomic_t,          0, 121) \
    /* <stdarg.h> */                                                   \
    XO(VA_LIST,          va_list,       va_list,               0, 122) \
    /* <stdatomic.h> types */                                          \
    XA(ATOMIC_BOOL,         atomic_bool,           atomic_bool,           1, 150) \
    XA(ATOMIC_CHAR,         atomic_char,           atomic_char,           1, 151) \
    XA(ATOMIC_SCHAR,        atomic_schar,          atomic_schar,          1, 152) \
    XA(ATOMIC_UCHAR,        atomic_uchar,          atomic_uchar,          1, 153) \
    XA(ATOMIC_SHORT,        atomic_short,          atomic_short,          1, 154) \
    XA(ATOMIC_USHORT,       atomic_ushort,         atomic_ushort,         1, 155) \
    XA(ATOMIC_INT,          atomic_int,            atomic_int,            1, 156) \
    XA(ATOMIC_UINT,         atomic_uint,           atomic_uint,           1, 157) \
    XA(ATOMIC_LONG,         atomic_long,           atomic_long,           1, 158) \
    XA(ATOMIC_ULONG,        atomic_ulong,          atomic_ulong,          1, 159) \
    XA(ATOMIC_LLONG,        atomic_llong,          atomic_llong,          1, 160) \
    XA(ATOMIC_ULLONG,       atomic_ullong,         atomic_ullong,         1, 161) \
    XA(ATOMIC_CHAR8,        atomic_char8,          atomic_char8_t,        1, 162) \
    XA(ATOMIC_CHAR16,       atomic_char16,         atomic_char16_t,       1, 163) \
    XA(ATOMIC_CHAR32,       atomic_char32,         atomic_char32_t,       1, 164) \
    XA(ATOMIC_WCHAR,        atomic_wchar,          atomic_wchar_t,        1, 165) \
    XA(ATOMIC_INT_LEAST8,   atomic_int_least8,     atomic_int_least8_t,   1, 166) \
    XA(ATOMIC_UINT_LEAST8,  atomic_uint_least8,    atomic_uint_least8_t,  1, 167) \
    XA(ATOMIC_INT_LEAST16,  atomic_int_least16,    atomic_int_least16_t,  1, 168) \
    XA(ATOMIC_UINT_LEAST16, atomic_uint_least16,   atomic_uint_least16_t, 1, 169) \
    XA(ATOMIC_INT_LEAST32,  atomic_int_least32,    atomic_int_least32_t,  1, 170) \
    XA(ATOMIC_UINT_LEAST32, atomic_uint_least32,   atomic_uint_least32_t, 1, 171) \
    XA(ATOMIC_INT_LEAST64,  atomic_int_least64,    atomic_int_least64_t,  1, 172) \
    XA(ATOMIC_UINT_LEAST64, atomic_uint_least64,   atomic_uint_least64_t, 1, 173) \
    XA(ATOMIC_INT_FAST8,    atomic_int_fast8,      atomic_int_fast8_t,    1, 174) \
    XA(ATOMIC_UINT_FAST8,   atomic_uint_fast8,     atomic_uint_fast8_t,   1, 175) \
    XA(ATOMIC_INT_FAST16,   atomic_int_fast16,     atomic_int_fast16_t,   1, 176) \
    XA(ATOMIC_UINT_FAST16,  atomic_uint_fast16,    atomic_uint_fast16_t,  1, 177) \
    XA(ATOMIC_INT_FAST32,   atomic_int_fast32,     atomic_int_fast32_t,   1, 178) \
    XA(ATOMIC_UINT_FAST32,  atomic_uint_fast32,    atomic_uint_fast32_t,  1, 179) \
    XA(ATOMIC_INT_FAST64,   atomic_int_fast64,     atomic_int_fast64_t,   1, 180) \
    XA(ATOMIC_UINT_FAST64,  atomic_uint_fast64,    atomic_uint_fast64_t,  1, 181) \
    XA(ATOMIC_INTPTR,       atomic_intptr,         atomic_intptr_t,       1, 182) \
    XA(ATOMIC_UINTPTR,      atomic_uintptr,        atomic_uintptr_t,      1, 183) \
    XA(ATOMIC_SIZE,         atomic_size,           atomic_size_t,         1, 184) \
    XA(ATOMIC_PTRDIFF,      atomic_ptrdiff,        atomic_ptrdiff_t,      1, 185) \
    XA(ATOMIC_INTMAX,       atomic_intmax,         atomic_intmax_t,       1, 186) \
    XA(ATOMIC_UINTMAX,      atomic_uintmax,        atomic_uintmax_t,      1, 187)

/* clang-format on */

#define PF__XIGNORE(...)
#define PF__X(u, l, t, f, i) PF_TYPE_##u = i,

enum pf_typeid {
    PF_TYPE_ERROR = -1,

    PF_TYPEID(PF__X, PF__X, PF__X, PF__X, PF__X)

        PF__TYPE_STANDARD
    = 1024,
};

#undef PF__X

#ifdef PF_TYPE_HELPERS

    #include <limits.h>
    #include <stddef.h>
    #include <stdint.h>

PF_API int pf_type_is_integer(int type) {
    if (type >= PF_TYPE_SCHAR && type <= PF_TYPE_SIZE)
        return 1;
    if (type >= PF_TYPE_INTPTR && type <= PF_TYPE_UINT_LEAST64)
        return 1;
    return 0;
}

PF_API int pf_type_is_float(int type) {
    return type >= PF_TYPE_FLOAT && type <= PF_TYPE_MDOUBLE;
}

PF_API int pf_type_is_unsigned(int type) {
    if (pf_type_is_integer(type))
        return type & 1;
    return PF_TYPE_ERROR;
}

PF_API size_t pf_type_int_size(int type) {
    if (!pf_type_is_integer(type))
        return 0;

    #define PF__XU(u, l, t, f, i) \
    case PF_TYPE_##u:             \
        return sizeof(t);

    switch (type | 1) {
        PF_TYPEID(PF__XIGNORE, PF__XU, PF__XIGNORE, PF__XIGNORE, PF__XIGNORE)
    default:
        pf__unreachable();
        break;
    }

    #undef PF__X

    return 0;
}

PF_API uintmax_t pf_type_int_max(int type) {
    if (!pf_type_is_integer(type))
        return 0;

    #define PF__X(u, l, t, f, i) \
    case PF_TYPE_##u:            \
        return u##_MAX;

    switch (type | 1) {
        PF_TYPEID(PF__X, PF__X, PF__XIGNORE, PF__XIGNORE, PF__XIGNORE)
    default:
        pf__unreachable();
        return 0;
    }

    #undef PF__X
}

PF_API intmax_t pf_type_int_min(int type) {
    if (pf_type_is_unsigned(type))
        return 0;

    #define PF__XS(u, l, t, f, i) \
    case PF_TYPE_##u:             \
        return u##_MIN;

    switch (type | 1) {
        PF_TYPEID(PF__XS, PF__XIGNORE, PF__XIGNORE, PF__XIGNORE, PF__XIGNORE)
    default:
        pf__unreachable();
        return 0;
    }

    #undef PF__XS
}

PF_API int pf_type_int_load(int type, const void *item, uintmax_t *out) {
    if (!pf_type_is_integer(type))
        return PF_TYPE_ERROR;

    #ifndef PF_TYPE_WEIRD_INT

    size_t size = pf_type_int_size(type);
    switch (size) {
        /* clang-format off */
    case 1: *out = *(uint8_t *)item; break;
    case 2: *out = *(uint16_t *)item; break;
    case 4: *out = *(uint32_t *)item; break;
    case 8: *out = *(uint64_t *)item; break;
    default: return PF_TYPE_ERROR;
        /* clang-format on */
    }

    #else

        #define PF__XU(u, l, t, f, i) \
        case PF_TYPE_##u:             \
            *out = *(t *)item;        \
            break;

        #define PF__XS(u, l, t, f, i)      \
        case PF_TYPE_##u:                  \
            *out = (intmax_t) * (t *)item; \
            break;

    switch (type) {
        PF_TYPEID(PF__XS, PF__XU, PF__XIGNORE, PF__XIGNORE, PF__XIGNORE)
    default:
        pf__unreachable();
        return PF_TYPE_ERROR;
    }

    #endif

    #undef PF__XS
    #undef PF__XU

    return 0;
}

PF_API int pf_type_int_store(int type, const uintmax_t *item, void *out) {
    if (!pf_type_is_integer(type))
        return PF_TYPE_ERROR;

    #ifndef PF_TYPE_WEIRD_INT

    size_t size = pf_type_int_size(type);
    switch (size) {
        /* clang-format off */
    case 1: *(uint8_t *)out = *item; break;
    case 2: *(uint16_t *)out = *item; break;
    case 4: *(uint32_t *)out = *item; break;
    case 8: *(uint64_t *)out = *item; break;
    default: return PF_TYPE_ERROR;
        /* clang-format on */
    }

    #else

        #define PF__XU(u, l, t, f, i) \
        case PF_TYPE_##u:             \
            *(t *)out = *item;        \
            break;

        #define PF__XS(u, l, t, f, i)      \
        case PF_TYPE_##u:                  \
            *(t *)out = (intmax_t) * item; \
            break;

    switch (type) {
        PF_TYPEID(PF__XS, PF__XU, PF__XIGNORE, PF__XIGNORE, PF__XIGNORE)
    default:
        pf__unreachable();
        return PF_TYPE_ERROR;
    }

    #endif

    #undef PF__XS
    #undef PF__XU

    return 0;
}

PF_API int pf_type_int_as_base(int type) {
    if (!pf_type_is_integer(type))
        return PF_TYPE_ERROR;

    uintmax_t max = pf_type_int_max(type);
    int sign = type & 1;

    if (sign)
        max = max * 2 + 1;

    if (max == UCHAR_MAX)
        return PF_TYPE_CHAR | sign;
    if (max == USHRT_MAX)
        return PF_TYPE_SHRT | sign;
    if (max == UINT_MAX)
        return PF_TYPE_INT | sign;
    if (max == ULONG_MAX)
        return PF_TYPE_LONG | sign;
    if (max == ULLONG_MAX)
        return PF_TYPE_LLONG | sign;
    return PF_TYPE_ERROR;
}

PF_API int pf_type_int_as_fixed(int type) {
    if (!pf_type_is_integer(type))
        return PF_TYPE_ERROR;

    size_t size = pf_type_int_size(type);
    int out, sign = type & 1;

    switch (size) {
        /* clang-format off */
    case 1: out = PF_TYPE_INT8; break;
    case 2: out = PF_TYPE_INT16; break;
    case 4: out = PF_TYPE_INT32; break;
    case 8: out = PF_TYPE_INT64; break;
    default: return PF_TYPE_ERROR;
        /* clang-format on */
    }

    return out | sign;
}

#endif

#undef PF__XIGNORE
#undef PF__X
#undef PF__XS
#undef PF__XU
#undef PF__XF
#undef PF__XA
#undef PF__XO

#endif
