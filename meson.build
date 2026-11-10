# Copyright 2025-2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

project(
    'cpolyfill',
    'c',
    version: '2.0.0',
    license: 'Apache-2.0',
    meson_version: '>= 1.3.0',
    default_options: ['c_std=gnu11,c11', 'warning_level=2'],
)

cc = meson.get_compiler('c')
is_msvc = cc.get_argument_syntax() == 'msvc'
is_windows = host_machine.system() == 'windows'

# System libraries the library needs. They are passed on to everything that
# depends on cpolyfill, so users never list them. Modules add to this list.
sys_deps = []

# Generated build configuration, installed as <cpolyfill/config.h>.
subdir('include/cpolyfill')

inc = include_directories('include')

sources = files(
    'src/alloc.c',
    'src/assert.c',
    'src/debug.c',
    'src/errno.c',
    'src/stdlib.c',
    'src/string.c',
    'src/uchar.c',
    'src/unicode.c',
    'src/version.c',
)
if is_windows
    sources += files('src/win32/error.c')
endif

lib_args = ['-DPF__BUILDING']
if is_msvc
    lib_args += ['-D_CRT_SECURE_NO_WARNINGS']
endif

# Internal facts about the C library (string.c, stdlib.c, errno.c).
lib_args += [
    '-DPF__HAVE_QSORT_R_BSD=@0@'.format(qsort_r_bsd ? 1 : 0),
    '-DPF__HAVE_GETC_UNLOCKED=@0@'.format(
        cc.links(
            libc_prefix + '''
int main(void) {
    flockfile(stdin);
    int c = getc_unlocked(stdin);
    funlockfile(stdin);
    return c;
}
''',
            name: 'getc_unlocked',
        ) ? 1 : 0,
    ),
    '-DPF__HAVE_EXPLICIT_BZERO=@0@'.format(
        cc.links(
            libc_prefix + '''
int main(void) {
    char b[4];
    explicit_bzero(b, sizeof(b));
    return b[0];
}
''',
            name: 'explicit_bzero',
        ) ? 1 : 0,
    ),
    '-DPF__HAVE_MEMSET_S=@0@'.format(
        cc.links(
            libc_prefix + '''
int main(void) {
    char b[4];
    memset_s(b, sizeof(b), 0, sizeof(b));
    return b[0];
}
''',
            name: 'memset_s',
        ) ? 1 : 0,
    ),
    '-DPF__STRERROR_R_GNU=@0@'.format(
        libc_have['strerror_r'] and cc.compiles(
            libc_prefix + '''
int main(void) {
    char buf[8];
    char *p = strerror_r(1, buf, sizeof(buf));
    return p == 0;
}
''',
            args: strict_types,
            name: 'GNU strerror_r',
        ) ? 1 : 0,
    ),
]

# Backtraces (debug.c): execinfo (glibc, macOS, BSDs with libexecinfo),
# else the unwinder of libgcc / LLVM (musl), and dladdr for symbol names.
if not is_windows
    have_execinfo = false
    if cc.has_header('execinfo.h')
        execinfo_prefix = '#include <execinfo.h>'
        if cc.has_function('backtrace', prefix: execinfo_prefix)
            have_execinfo = true
        else
            execinfo_dep = cc.find_library('execinfo', required: false)
            if execinfo_dep.found() and cc.has_function(
                'backtrace',
                prefix: execinfo_prefix,
                dependencies: execinfo_dep,
            )
                have_execinfo = true
                sys_deps += execinfo_dep
            endif
        endif
    endif
    have_unwind = cc.has_function(
        '_Unwind_Backtrace',
        prefix: '#include <unwind.h>',
    )

    dl_dep = cc.find_library('dl', required: false)
    dladdr_prefix = '#define _GNU_SOURCE\n#include <dlfcn.h>'
    have_dladdr = cc.has_function('dladdr', prefix: dladdr_prefix)
    if not have_dladdr and dl_dep.found()
        have_dladdr = cc.has_function(
            'dladdr',
            prefix: dladdr_prefix,
            dependencies: dl_dep,
        )
        if have_dladdr
            sys_deps += dl_dep
        endif
    endif

    lib_args += [
        '-DPF__HAVE_EXECINFO=@0@'.format(have_execinfo ? 1 : 0),
        '-DPF__HAVE_UNWIND=@0@'.format(have_unwind ? 1 : 0),
        '-DPF__HAVE_DLADDR=@0@'.format(have_dladdr ? 1 : 0),
    ]
endif

lib = library(
    'cpolyfill',
    sources,
    include_directories: inc,
    c_args: lib_args,
    dependencies: sys_deps,
    gnu_symbol_visibility: 'hidden',
    version: meson.project_version(),
    soversion: meson.project_version().split('.')[0],
    install: true,
)

cpolyfill_dep = declare_dependency(
    link_with: lib,
    include_directories: inc,
    dependencies: sys_deps,
)
meson.override_dependency('cpolyfill', cpolyfill_dep)

install_headers(public_headers, subdir: 'cpolyfill')

pkg = import('pkgconfig')
pkg.generate(
    lib,
    name: 'cpolyfill',
    filebase: 'cpolyfill',
    description: 'Filling the gaps between C standards and compilers',
)

if not meson.is_subproject() and get_option('tests')
    subdir('test')
endif
