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
    'src/cpuid.c',
    'src/atomic.c',
    'src/assert.c',
    'src/debug.c',
    'src/dirent.c',
    'src/dlfcn.c',
    'src/errno.c',
    'src/exception.c',
    'src/fs.c',
    'src/io.c',
    'src/mman.c',
    'src/pipe.c',
    'src/process.c',
    'src/random.c',
    'src/semaphore.c',
    'src/signal.c',
    'src/socket.c',
    'src/stdlib.c',
    'src/string.c',
    'src/sysinfo.c',
    'src/term.c',
    'src/threads.c',
    'src/time.c',
    'src/typeid.c',
    'src/uchar.c',
    'src/unicode.c',
    'src/version.c',
)
if is_windows
    sources += files(
        'src/win32/dirent.c',
        'src/win32/error.c',
        'src/win32/fs.c',
        'src/win32/io.c',
        'src/win32/path.c',
        'src/win32/process.c',
        'src/win32/threads.c',
    )
else
    sources += files(
        'src/posix/dirent.c',
        'src/posix/fs.c',
        'src/posix/io.c',
        'src/posix/process.c',
        'src/posix/threads.c',
    )
endif

if have_test_module
    sources += files('src/test.c')
endif
if have_argparse
    sources += files('src/argparse.c')
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

# Files (posix/fs.c): statx for the creation time, copy_file_range.
if not is_windows
    lib_args += [
        '-DPF__HAVE_STATX=@0@'.format(
            cc.has_function(
                'statx',
                prefix: '#define _GNU_SOURCE\n#include <fcntl.h>\n#include <sys/stat.h>',
            ) ? 1 : 0,
        ),
        '-DPF__HAVE_COPY_FILE_RANGE=@0@'.format(
            cc.has_function(
                'copy_file_range',
                prefix: '#define _GNU_SOURCE\n#include <unistd.h>',
            ) ? 1 : 0,
        ),
    ]
endif

# getrandom (random.c), in glibc since 2.25.
if host_machine.system() in ['linux', 'android']
    lib_args += '-DPF__HAVE_GETRANDOM=@0@'.format(
        cc.has_function('getrandom', prefix: '#include <sys/random.h>') ? 1 : 0,
    )
endif
if is_windows
    sys_deps += cc.find_library('bcrypt')
endif

# Sockets (socket.c, pipe.c): Winsock, and accept4 where it exists.
if is_windows
    sys_deps += cc.find_library('ws2_32')
else
    lib_args += '-DPF__HAVE_ACCEPT4=@0@'.format(
        cc.has_function(
            'accept4',
            prefix: '#define _GNU_SOURCE\n#include <sys/socket.h>',
        ) ? 1 : 0,
    )
endif

# Threads (threads.c, atomic.c).
if have_threads
    sys_deps += threads_dep
endif
if is_windows
    # WaitOnAddress (atomic.c).
    sys_deps += cc.find_library('synchronization')
    # Known folders (shell32, ole32), accounts and the registry (advapi32)
    # in win32/fs.c.
    foreach name : ['shell32', 'ole32', 'advapi32']
        sys_deps += cc.find_library(name)
    endforeach
elif have_threads
    lib_args += [
        '-DPF__HAVE_PTHREAD_MUTEX_TIMEDLOCK=@0@'.format(
            cc.has_function(
                'pthread_mutex_timedlock',
                prefix: '#include <pthread.h>',
                dependencies: threads_dep,
            ) ? 1 : 0,
        ),
        '-DPF__HAVE_SEM_CLOCKWAIT=@0@'.format(
            cc.has_function(
                'sem_clockwait',
                prefix: '#define _GNU_SOURCE\n#include <semaphore.h>',
                dependencies: threads_dep,
            ) ? 1 : 0,
        ),
        '-DPF__HAVE_PTHREAD_SETNAME_NP=@0@'.format(
            cc.has_function(
                'pthread_setname_np',
                prefix: '#define _GNU_SOURCE\n#include <pthread.h>',
                dependencies: threads_dep,
            ) ? 1 : 0,
        ),
        '-DPF__HAVE_PTHREAD_GETNAME_NP=@0@'.format(
            cc.has_function(
                'pthread_getname_np',
                prefix: '#define _GNU_SOURCE\n#include <pthread.h>',
                dependencies: threads_dep,
            ) ? 1 : 0,
        ),
    ]
endif
# GCC calls libatomic for atomics that are not lock-free (<cpolyfill/atomic.h>
# before C11, with __atomic builtins).
if not is_msvc
    atomic_test = '''
#include <stdint.h>
struct big { char c[24]; };
int main(void) {
    static struct big a, b;
    static uint64_t w;
    __atomic_load(&a, &b, __ATOMIC_SEQ_CST);
    return (int)__atomic_fetch_add(&w, 1, __ATOMIC_SEQ_CST) + b.c[0];
}
'''
    if not cc.links(atomic_test, name: 'atomics without libatomic')
        atomic_dep = cc.find_library('atomic', required: false)
        if atomic_dep.found() and cc.links(
            atomic_test,
            dependencies: atomic_dep,
            name: 'atomics with libatomic',
        )
            sys_deps += atomic_dep
        endif
    endif
endif

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
    dl_added = false
    if not have_dladdr and dl_dep.found()
        have_dladdr = cc.has_function(
            'dladdr',
            prefix: dladdr_prefix,
            dependencies: dl_dep,
        )
        if have_dladdr
            sys_deps += dl_dep
            dl_added = true
        endif
    endif

    # dlopen is in libdl before glibc 2.34 (dlfcn.c).
    if not dl_added and dl_dep.found() and not cc.has_function(
        'dlopen',
        prefix: '#include <dlfcn.h>',
    )
        sys_deps += dl_dep
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
    if have_test_module
        subdir('test')
    else
        warning('The tests are not built: they need the test module (-Dtest).')
    endif
endif
