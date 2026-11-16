/** cpolyfill - Filling the gaps between C standards and compilers

    Numeric identifiers for the standard C types, shared by libraries that
    describe data at run time (serializers, argument parsers, containers),
    so they agree on the numbers. The numbers never change.

    Identifiers of standard types are below PF_TYPE_USER (1024); custom
    types should take their own range from PF_TYPE_USER up. PF_TYPE_ERROR
    (-1) is no type.

        PF_TYPEID(XS, XU, XF, XA, XO)
                The table, calling one macro per type:
                    X(UPPER, lower, type, flags, id)
                XS signed integers, XU unsigned integers, XF floating
                types, XA atomic types (flags 1), XO the others. The
                `type` of XS, XU and XF entries is a type once this header
                is included; for XA and XO entries it is only a name (it
                may not exist in every standard, or at all).
        pf_typeid_t             The enum: PF_TYPE_<UPPER> = id, plus
                                PF_TYPE_ERROR and PF_TYPE_USER.

    The 128-bit integers (PF_TYPE_INT128, PF_TYPE_UINT128) are XO entries,
    since not every compiler has `__int128`; the functions below count them
    as integers where the library was built with one.

    Functions (an unknown `type` is no error: false, 0 or NULL):

        bool pf_type_is_integer(int type)
                The XS and XU types and the 128-bit ones; not `char`
                (signed or not depending on the platform), `bool` or the
                character types.
        bool pf_type_is_signed(int type)
        bool pf_type_is_unsigned(int type)
                For integers only.
        bool pf_type_is_float(int type)
        bool pf_type_is_atomic(int type)
        size_t pf_type_size(int type)
                sizeof the type: integers, floats, pointers and the other
                types the library can name; 0 for void, the atomic types
                and those of <threads.h>.
        const char *pf_type_name(int type)
                The lower-case name of the table ("uint8").
        uintmax_t pf_type_int_max(int type)
        intmax_t pf_type_int_min(int type)
                The limits; 0 for other types. For the 128-bit types,
                those of what a load or store carries: INTMAX_MIN to
                INTMAX_MAX, 0 to UINTMAX_MAX.
        int pf_type_int_load(int type, const void *item, uintmax_t *out)
                Reads the integer at `item`; a signed value is converted
                through intmax_t, so (intmax_t)*out is the value. 0,
                PF_EINVAL (not an integer, NULL pointer), or PF_ERANGE (a
                128-bit value beyond intmax_t / uintmax_t).
        int pf_type_int_store(int type, const uintmax_t *value, void *out)
                Writes `*value` cut to the type (its low bits); the 128-bit
                types get it extended by sign (signed) or zeros. 0 or
                PF_EINVAL.
        int pf_type_int_as_base(int type)
                The first of signed char, short, int, long and long long
                (or the unsigned one) of the same size and signedness:
                PF_TYPE_INT for PF_TYPE_INT32.
        int pf_type_int_as_fixed(int type)
                The intN_t / uintN_t of the same size and signedness:
                PF_TYPE_INT64 for PF_TYPE_LONG on LP64.
                Both PF_TYPE_ERROR when there is none.

    SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
    SPDX-License-Identifier: Apache-2.0
*/

#ifndef CPOLYFILL_TYPEID_H
#define CPOLYFILL_TYPEID_H

#include <cpolyfill/platform.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

/* clang-format off */
#define PF_TYPEID(XS, XU, XF, XA, XO)                                         \
    /* uppercase,          lowercase,           type,                  flags, id */ \
    XO(VOID,               void,                void,                     0, 0  ) \
    XO(PTR,                ptr,                 void *,                   0, 1  ) \
    XO(CHAR,               char,                char,                     0, 2  ) \
    XO(CSTRING,            cstring,             char *,                   0, 3  ) \
    XS(SCHAR,              schar,               signed char,              0, 4  ) \
    XU(UCHAR,              uchar,               unsigned char,            0, 5  ) \
    XS(SHRT,               shrt,                short,                    0, 6  ) \
    XU(USHRT,              ushrt,               unsigned short,           0, 7  ) \
    XS(INT,                int,                 int,                      0, 8  ) \
    XU(UINT,               uint,                unsigned int,             0, 9  ) \
    XS(LONG,               long,                long,                     0, 10 ) \
    XU(ULONG,              ulong,               unsigned long,            0, 11 ) \
    XS(LLONG,              llong,               long long,                0, 12 ) \
    XU(ULLONG,             ullong,              unsigned long long,       0, 13 ) \
    XF(FLOAT,              float,               float,                    0, 21 ) \
    XF(DOUBLE,             double,              double,                   0, 22 ) \
    XF(LDOUBLE,            ldouble,             long double,              0, 23 ) \
    /* <stddef.h> */                                                          \
    XS(PTRDIFF,            ptrdiff,             ptrdiff_t,                0, 14 ) \
    XU(SIZE,               size,                size_t,                   0, 15 ) \
    XO(MAX_ALIGN,          max_align,           max_align_t,              0, 16 ) \
    XO(NULLPTR,            nullptr,             nullptr_t,                0, 17 ) \
    /* <stdbool.h> */                                                         \
    XO(BOOL,               bool,                bool,                     0, 18 ) \
    /* <math.h> */                                                            \
    XF(MFLOAT,             mfloat,              float_t,                  0, 24 ) \
    XF(MDOUBLE,            mdouble,             double_t,                 0, 25 ) \
    /* <stdint.h> */                                                          \
    XS(INTPTR,             intptr,              intptr_t,                 0, 28 ) \
    XU(UINTPTR,            uintptr,             uintptr_t,                0, 29 ) \
    XS(INTMAX,             intmax,              intmax_t,                 0, 30 ) \
    XU(UINTMAX,            uintmax,             uintmax_t,                0, 31 ) \
    XS(INT8,               int8,                int8_t,                   0, 32 ) \
    XU(UINT8,              uint8,               uint8_t,                  0, 33 ) \
    XS(INT16,              int16,               int16_t,                  0, 34 ) \
    XU(UINT16,             uint16,              uint16_t,                 0, 35 ) \
    XS(INT32,              int32,               int32_t,                  0, 36 ) \
    XU(UINT32,             uint32,              uint32_t,                 0, 37 ) \
    XS(INT64,              int64,               int64_t,                  0, 38 ) \
    XU(UINT64,             uint64,              uint64_t,                 0, 39 ) \
    XS(INT_FAST8,          int_fast8,           int_fast8_t,              0, 40 ) \
    XU(UINT_FAST8,         uint_fast8,          uint_fast8_t,             0, 41 ) \
    XS(INT_FAST16,         int_fast16,          int_fast16_t,             0, 42 ) \
    XU(UINT_FAST16,        uint_fast16,         uint_fast16_t,            0, 43 ) \
    XS(INT_FAST32,         int_fast32,          int_fast32_t,             0, 44 ) \
    XU(UINT_FAST32,        uint_fast32,         uint_fast32_t,            0, 45 ) \
    XS(INT_FAST64,         int_fast64,          int_fast64_t,             0, 46 ) \
    XU(UINT_FAST64,        uint_fast64,         uint_fast64_t,            0, 47 ) \
    XS(INT_LEAST8,         int_least8,          int_least8_t,             0, 48 ) \
    XU(UINT_LEAST8,        uint_least8,         uint_least8_t,            0, 49 ) \
    XS(INT_LEAST16,        int_least16,         int_least16_t,            0, 50 ) \
    XU(UINT_LEAST16,       uint_least16,        uint_least16_t,           0, 51 ) \
    XS(INT_LEAST32,        int_least32,         int_least32_t,            0, 52 ) \
    XU(UINT_LEAST32,       uint_least32,        uint_least32_t,           0, 53 ) \
    XS(INT_LEAST64,        int_least64,         int_least64_t,            0, 54 ) \
    XU(UINT_LEAST64,       uint_least64,        uint_least64_t,           0, 55 ) \
    /* __int128 (GCC, Clang) */                                               \
    XO(INT128,             int128,              __int128,                 0, 56 ) \
    XO(UINT128,            uint128,             unsigned __int128,        0, 57 ) \
    /* <stdio.h> */                                                           \
    XO(FILE,               file,                FILE *,                   0, 100) \
    XO(FPOS,               fpos,                fpos_t,                   0, 101) \
    /* <threads.h> */                                                         \
    XO(THRD,               thrd,                thrd_t,                   0, 102) \
    XO(THRD_START,         thrd_start,          thrd_start_t,             0, 103) \
    XO(MTX,                mtx,                 mtx_t,                    0, 104) \
    XO(CND,                cnd,                 cnd_t,                    0, 105) \
    XO(TSS,                tss,                 tss_t,                    0, 106) \
    XO(TSS_DTOR,           tss_dtor,            tss_dtor_t,               0, 107) \
    /* <time.h> */                                                            \
    XO(TIME,               time,                time_t,                   0, 108) \
    XO(CLOCK,              clock,               clock_t,                  0, 109) \
    XO(TM,                 tm,                  struct tm,                0, 110) \
    XO(TIMESPEC,           timespec,            struct timespec,          0, 111) \
    /* <uchar.h> */                                                           \
    XO(MB_STATE,           mb_state,            mbstate_t,                0, 112) \
    XO(CHAR8,              char8,               char8_t,                  0, 113) \
    XO(CHAR16,             char16,              char16_t,                 0, 114) \
    XO(CHAR32,             char32,              char32_t,                 0, 115) \
    /* <wchar.h>, <wctype.h> */                                               \
    XO(WCHAR,              wchar,               wchar_t,                  0, 116) \
    XO(WINT,               wint,                wint_t,                   0, 117) \
    XO(WCTRANS,            wctrans,             wctrans_t,                0, 118) \
    XO(WCTYPE,             wctype,              wctype_t,                 0, 119) \
    /* <setjmp.h> */                                                          \
    XO(JMP_BUF,            jmp_buf,             jmp_buf,                  0, 120) \
    /* <signal.h> */                                                          \
    XO(SIG_ATOMIC,         sig_atomic,          sig_atomic_t,             0, 121) \
    /* <stdarg.h> */                                                          \
    XO(VA_LIST,            va_list,             va_list,                  0, 122) \
    /* <stdatomic.h> */                                                       \
    XA(ATOMIC_BOOL,        atomic_bool,         atomic_bool,              1, 150) \
    XA(ATOMIC_CHAR,        atomic_char,         atomic_char,              1, 151) \
    XA(ATOMIC_SCHAR,       atomic_schar,        atomic_schar,             1, 152) \
    XA(ATOMIC_UCHAR,       atomic_uchar,        atomic_uchar,             1, 153) \
    XA(ATOMIC_SHORT,       atomic_short,        atomic_short,             1, 154) \
    XA(ATOMIC_USHORT,      atomic_ushort,       atomic_ushort,            1, 155) \
    XA(ATOMIC_INT,         atomic_int,          atomic_int,               1, 156) \
    XA(ATOMIC_UINT,        atomic_uint,         atomic_uint,              1, 157) \
    XA(ATOMIC_LONG,        atomic_long,         atomic_long,              1, 158) \
    XA(ATOMIC_ULONG,       atomic_ulong,        atomic_ulong,             1, 159) \
    XA(ATOMIC_LLONG,       atomic_llong,        atomic_llong,             1, 160) \
    XA(ATOMIC_ULLONG,      atomic_ullong,       atomic_ullong,            1, 161) \
    XA(ATOMIC_CHAR8,       atomic_char8,        atomic_char8_t,           1, 162) \
    XA(ATOMIC_CHAR16,      atomic_char16,       atomic_char16_t,          1, 163) \
    XA(ATOMIC_CHAR32,      atomic_char32,       atomic_char32_t,          1, 164) \
    XA(ATOMIC_WCHAR,       atomic_wchar,        atomic_wchar_t,           1, 165) \
    XA(ATOMIC_INT_LEAST8,  atomic_int_least8,   atomic_int_least8_t,      1, 166) \
    XA(ATOMIC_UINT_LEAST8, atomic_uint_least8,  atomic_uint_least8_t,     1, 167) \
    XA(ATOMIC_INT_LEAST16, atomic_int_least16,  atomic_int_least16_t,     1, 168) \
    XA(ATOMIC_UINT_LEAST16,atomic_uint_least16, atomic_uint_least16_t,    1, 169) \
    XA(ATOMIC_INT_LEAST32, atomic_int_least32,  atomic_int_least32_t,     1, 170) \
    XA(ATOMIC_UINT_LEAST32,atomic_uint_least32, atomic_uint_least32_t,    1, 171) \
    XA(ATOMIC_INT_LEAST64, atomic_int_least64,  atomic_int_least64_t,     1, 172) \
    XA(ATOMIC_UINT_LEAST64,atomic_uint_least64, atomic_uint_least64_t,    1, 173) \
    XA(ATOMIC_INT_FAST8,   atomic_int_fast8,    atomic_int_fast8_t,       1, 174) \
    XA(ATOMIC_UINT_FAST8,  atomic_uint_fast8,   atomic_uint_fast8_t,      1, 175) \
    XA(ATOMIC_INT_FAST16,  atomic_int_fast16,   atomic_int_fast16_t,      1, 176) \
    XA(ATOMIC_UINT_FAST16, atomic_uint_fast16,  atomic_uint_fast16_t,     1, 177) \
    XA(ATOMIC_INT_FAST32,  atomic_int_fast32,   atomic_int_fast32_t,      1, 178) \
    XA(ATOMIC_UINT_FAST32, atomic_uint_fast32,  atomic_uint_fast32_t,     1, 179) \
    XA(ATOMIC_INT_FAST64,  atomic_int_fast64,   atomic_int_fast64_t,      1, 180) \
    XA(ATOMIC_UINT_FAST64, atomic_uint_fast64,  atomic_uint_fast64_t,     1, 181) \
    XA(ATOMIC_INTPTR,      atomic_intptr,       atomic_intptr_t,          1, 182) \
    XA(ATOMIC_UINTPTR,     atomic_uintptr,      atomic_uintptr_t,         1, 183) \
    XA(ATOMIC_SIZE,        atomic_size,         atomic_size_t,            1, 184) \
    XA(ATOMIC_PTRDIFF,     atomic_ptrdiff,      atomic_ptrdiff_t,         1, 185) \
    XA(ATOMIC_INTMAX,      atomic_intmax,       atomic_intmax_t,          1, 186) \
    XA(ATOMIC_UINTMAX,     atomic_uintmax,      atomic_uintmax_t,         1, 187)
/* clang-format on */

#define PF__TYPEID_ENUM(u, l, t, f, i) PF_TYPE_##u = i,

/* clang-format off */
typedef enum pf_typeid_t {
    PF_TYPE_ERROR = -1,
    PF_TYPEID(PF__TYPEID_ENUM, PF__TYPEID_ENUM, PF__TYPEID_ENUM,
              PF__TYPEID_ENUM, PF__TYPEID_ENUM)
    PF_TYPE_USER = 1024
} pf_typeid_t;
/* clang-format on */

PF_BEGIN_DECLS

PF_API bool pf_type_is_integer(int type);
PF_API bool pf_type_is_signed(int type);
PF_API bool pf_type_is_unsigned(int type);
PF_API bool pf_type_is_float(int type);
PF_API bool pf_type_is_atomic(int type);
PF_API size_t pf_type_size(int type);
PF_API const char *pf_type_name(int type);
PF_API uintmax_t pf_type_int_max(int type);
PF_API intmax_t pf_type_int_min(int type);
PF_API int pf_type_int_load(int type, const void *item, uintmax_t *out);
PF_API int pf_type_int_store(int type, const uintmax_t *value, void *out);
PF_API int pf_type_int_as_base(int type);
PF_API int pf_type_int_as_fixed(int type);

PF_END_DECLS

#endif /* CPOLYFILL_TYPEID_H */
