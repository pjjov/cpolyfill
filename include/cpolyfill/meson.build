# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

public_headers = files(
    'assert.h',
    'attribute.h',
    'bitwise.h',
    'ctype.h',
    'debug.h',
    'endian.h',
    'errno.h',
    'macro.h',
    'overflow.h',
    'platform.h',
    'stdc.h',
    'stdlib.h',
    'string.h',
    'types.h',
    'unicode.h',
    'version.h',
)

version = meson.project_version().split('.')

conf = configuration_data()
conf.set('PF_VERSION_MAJOR', version[0])
conf.set('PF_VERSION_MINOR', version[1])
conf.set('PF_VERSION_PATCH', version[2])
conf.set('PF_VERSION_STRING', meson.project_version())
conf.set10('PF_CONFIG_SHARED', get_option('default_library') == 'shared')
conf.set10('PF_CONFIG_BIG_ENDIAN', host_machine.endian() == 'big')

# ---- What the C library has ---------------------------------------------
#
# The standard-name polyfills map a name only where it is missing here. A
# function counts as present if a program taking its address links, with
# the feature macros the library itself is built with. (cc.has_function
# would also accept compiler builtins such as __builtin_strndup.)

have_strings_h = cc.has_header('strings.h')
have_uchar_h = cc.has_header('uchar.h')
conf.set10('PF_CONFIG_HAVE_STRINGS_H', have_strings_h)
conf.set10('PF_CONFIG_HAVE_UCHAR_H', have_uchar_h)

libc_prefix = '''
#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1
#define __STDC_WANT_LIB_EXT1__ 1
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
'''
if have_strings_h
    libc_prefix += '#include <strings.h>\n'
endif
if have_uchar_h
    libc_prefix += '#include <uchar.h>\n'
endif

# MSVC has these only under deprecated POSIX names (`_strdup` is the real
# one), which warn on every use: treat them as missing.
msvc_deprecated = ['strdup', 'memccpy']

libc_functions = [
    # string.h
    'strdup', 'strndup', 'strnlen', 'strlcpy', 'strlcat', 'strcasecmp',
    'strncasecmp', 'strtok_r', 'strsep', 'strchrnul', 'memmem', 'memrchr',
    'mempcpy', 'stpcpy', 'stpncpy', 'memccpy', 'memset_explicit', 'asprintf',
    'vasprintf',
    # stdlib.h
    'reallocarray', 'getline', 'getdelim', 'setenv', 'unsetenv',
    # errno.h
    'strerror_r',
    # unicode.h
    'mbrtoc8', 'mbrtoc16', 'mbrtoc32', 'c8rtomb', 'c16rtomb', 'c32rtomb',
]
libc_have = {}
foreach fn : libc_functions
    found = false
    if not (is_msvc and fn in msvc_deprecated)
        found = cc.links(
            libc_prefix + '''
int main(void) {
    void (*volatile p)(void) = (void (*)(void))&@0@;
    return p == 0;
}
'''.format(fn),
            name: fn,
        )
    endif
    libc_have += {fn: found}
    conf.set10('PF_CONFIG_HAVE_' + fn.to_upper(), found)
endforeach

# qsort_r exists with two argument orders: POSIX 2024 / glibc (the one
# pf_qsort_r has) and the older BSD / macOS one.
strict_types = cc.get_supported_arguments(
    '-Werror=incompatible-pointer-types',
    '-Werror=int-conversion',
)
qsort_r_gnu = cc.links(
    libc_prefix + '''
int main(void) {
    void (*p)(void *, size_t, size_t,
              int (*)(const void *, const void *, void *), void *) = qsort_r;
    return p == 0;
}
''',
    args: strict_types,
    name: 'qsort_r (POSIX order)',
)
qsort_r_bsd = not qsort_r_gnu and cc.links(
    libc_prefix + '''
int main(void) {
    void (*p)(void *, size_t, size_t, void *,
              int (*)(void *, const void *, const void *)) = qsort_r;
    return p == 0;
}
''',
    args: strict_types,
    name: 'qsort_r (BSD order)',
)
conf.set10('PF_CONFIG_HAVE_QSORT_R', qsort_r_gnu)

config_h = configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration: conf,
    install: true,
    install_dir: get_option('includedir') / 'cpolyfill',
)
