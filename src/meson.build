# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

# ---- Sources -------------------------------------------------------------

sources = files(
    'alloc.c',
    'assert.c',
    'atomic.c',
    'cpuid.c',
    'debug.c',
    'dirent.c',
    'dlfcn.c',
    'errno.c',
    'exception.c',
    'fs.c',
    'io.c',
    'mman.c',
    'pipe.c',
    'process.c',
    'random.c',
    'semaphore.c',
    'signal.c',
    'socket.c',
    'stdlib.c',
    'string.c',
    'sysinfo.c',
    'term.c',
    'threads.c',
    'time.c',
    'typeid.c',
    'uchar.c',
    'unicode.c',
    'version.c',
)
if is_windows
    sources += files(
        'win32/dirent.c',
        'win32/error.c',
        'win32/fs.c',
        'win32/io.c',
        'win32/path.c',
        'win32/process.c',
        'win32/threads.c',
    )
else
    sources += files(
        'posix/dirent.c',
        'posix/fs.c',
        'posix/io.c',
        'posix/process.c',
        'posix/threads.c',
    )
endif

# The optional modules (see meson_options.txt).
optional_modules = {
    'test': have_test_module,
    'argparse': have_argparse,
    'cli': have_cli,
    'daemon': have_daemon,
}
foreach name, enabled : optional_modules
    if enabled
        sources += files(name + '.c')
    endif
endforeach

# ---- Internal configuration ----------------------------------------------
#
# Facts only the library's sources need, passed as PF__<NAME>=0/1. The
# public ones are in <cpolyfill/config.h> (include/cpolyfill/meson.build).

internal = {
    # cli.c offers --help and --color when argparse is built.
    'HAVE_ARGPARSE': have_argparse,
    # string.c, stdlib.c, errno.c.
    'HAVE_QSORT_R_BSD': qsort_r_bsd,
    'HAVE_GETC_UNLOCKED': cc.links(
        libc_prefix + '''
int main(void) {
    flockfile(stdin);
    int c = getc_unlocked(stdin);
    funlockfile(stdin);
    return c;
}
''',
        name: 'getc_unlocked',
    ),
    'HAVE_EXPLICIT_BZERO': cc.links(
        libc_prefix + '''
int main(void) {
    char b[4];
    explicit_bzero(b, sizeof(b));
    return b[0];
}
''',
        name: 'explicit_bzero',
    ),
    'HAVE_MEMSET_S': cc.links(
        libc_prefix + '''
int main(void) {
    char b[4];
    memset_s(b, sizeof(b), 0, sizeof(b));
    return b[0];
}
''',
        name: 'memset_s',
    ),
    'STRERROR_R_GNU': libc_have['strerror_r'] and cc.compiles(
        libc_prefix + '''
int main(void) {
    char buf[8];
    char *p = strerror_r(1, buf, sizeof(buf));
    return p == 0;
}
''',
        args: strict_types,
        name: 'GNU strerror_r',
    ),
}

# POSIX and GNU functions, checked by name: {macro: [function, prefix]}.
gnu_functions = {}
if not is_windows
    gnu_functions += {
        # posix/fs.c: creation time, in-kernel copies.
        'HAVE_STATX': [
            'statx',
            '#include <fcntl.h>\n#include <sys/stat.h>',
        ],
        'HAVE_COPY_FILE_RANGE': ['copy_file_range', '#include <unistd.h>'],
        # socket.c, pipe.c.
        'HAVE_ACCEPT4': ['accept4', '#include <sys/socket.h>'],
    }
endif
if host_machine.system() in ['linux', 'android']
    # random.c, in glibc since 2.25.
    gnu_functions += {
        'HAVE_GETRANDOM': ['getrandom', '#include <sys/random.h>'],
    }
endif
foreach macro, check : gnu_functions
    internal += {
        macro: cc.has_function(
            check[0],
            prefix: '#define _GNU_SOURCE\n' + check[1],
        ),
    }
endforeach

# posix/threads.c, semaphore.c.
if have_threads and not is_windows
    foreach macro, check : {
        'HAVE_PTHREAD_MUTEX_TIMEDLOCK': ['pthread_mutex_timedlock', 'pthread.h'],
        'HAVE_SEM_CLOCKWAIT': ['sem_clockwait', 'semaphore.h'],
        'HAVE_PTHREAD_SETNAME_NP': ['pthread_setname_np', 'pthread.h'],
        'HAVE_PTHREAD_GETNAME_NP': ['pthread_getname_np', 'pthread.h'],
    }
        internal += {
            macro: cc.has_function(
                check[0],
                prefix: '#define _GNU_SOURCE\n#include <@0@>'.format(check[1]),
                dependencies: threads_dep,
            ),
        }
    endforeach
endif

# ---- System libraries ----------------------------------------------------

if have_threads
    sys_deps += threads_dep
endif

if is_windows
    # bcrypt: random.c. ws2_32: socket.c, pipe.c. synchronization:
    # WaitOnAddress in atomic.c. shell32, ole32, advapi32: known folders,
    # accounts and the registry in win32/fs.c.
    foreach name : [
        'bcrypt',
        'ws2_32',
        'synchronization',
        'shell32',
        'ole32',
        'advapi32',
    ]
        sys_deps += cc.find_library(name)
    endforeach
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
# dlopen (dlfcn.c) is in libdl before glibc 2.34.
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

    dl_dep = cc.find_library('dl', required: false)
    dladdr_prefix = '#define _GNU_SOURCE\n#include <dlfcn.h>'
    have_dladdr = cc.has_function('dladdr', prefix: dladdr_prefix)
    need_dl = false
    if dl_dep.found()
        if not have_dladdr
            have_dladdr = cc.has_function(
                'dladdr',
                prefix: dladdr_prefix,
                dependencies: dl_dep,
            )
            need_dl = have_dladdr
        endif
        if not need_dl
            need_dl = not cc.has_function(
                'dlopen',
                prefix: '#include <dlfcn.h>',
            )
        endif
    endif
    if need_dl
        sys_deps += dl_dep
    endif

    internal += {
        'HAVE_EXECINFO': have_execinfo,
        'HAVE_UNWIND': cc.has_function(
            '_Unwind_Backtrace',
            prefix: '#include <unwind.h>',
        ),
        'HAVE_DLADDR': have_dladdr,
    }
endif

# ---- The library ---------------------------------------------------------

lib_args = ['-DPF__BUILDING']
foreach macro, value : internal
    lib_args += '-DPF__@0@=@1@'.format(macro, value ? 1 : 0)
endforeach
if is_msvc
    lib_args += '-D_CRT_SECURE_NO_WARNINGS'
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
