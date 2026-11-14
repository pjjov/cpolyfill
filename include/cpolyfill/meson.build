# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

public_headers = files(
    'assert.h',
    'atomic.h',
    'attribute.h',
    'bitwise.h',
    'cpuid.h',
    'ctype.h',
    'debug.h',
    'dirent.h',
    'dlfcn.h',
    'endian.h',
    'errno.h',
    'exception.h',
    'fs.h',
    'io.h',
    'macro.h',
    'mman.h',
    'overflow.h',
    'pipe.h',
    'platform.h',
    'process.h',
    'random.h',
    'semaphore.h',
    'signal.h',
    'socket.h',
    'stdc.h',
    'stdlib.h',
    'string.h',
    'sysinfo.h',
    'term.h',
    'threads.h',
    'time.h',
    'typeid.h',
    'types.h',
    'unicode.h',
    'version.h',
)
# The test runner module, a feature with nothing to look for.
have_test_module = not get_option('test').disabled()
if have_test_module
    public_headers += files('test.h')
endif
have_argparse = not get_option('argparse').disabled()
if have_argparse
    public_headers += files('argparse.h')
endif
have_cli = not get_option('cli').disabled()
if have_cli
    public_headers += files('cli.h')
endif
have_daemon = not get_option('daemon').disabled()
if have_daemon
    public_headers += files('daemon.h')
endif

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
#include <time.h>
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
    # time.h
    'clock_gettime', 'clock_getres', 'nanosleep', 'timespec_get',
    'timespec_getres', 'gmtime_r', 'localtime_r', 'timegm',
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

# POSIX functions, with the headers that declare them where they exist.
posix_prefix = libc_prefix
foreach h : ['unistd.h', 'fcntl.h', 'sys/stat.h', 'sys/types.h', 'sys/random.h']
    if cc.has_header(h)
        posix_prefix += '#include <@0@>\n'.format(h)
    endif
endforeach
posix_functions = [
    # io.h
    'fseeko', 'ftello', 'pread', 'pwrite', 'fsync', 'ftruncate', 'fmemopen',
    'open_memstream',
    # fs.h
    'realpath', 'mkstemp', 'mkdtemp', 'symlink', 'readlink', 'link',
    'truncate',
    # random.h
    'getentropy', 'arc4random', 'arc4random_buf', 'arc4random_uniform',
    # time.h
    'sleep', 'usleep',
]
foreach fn : posix_functions
    found = cc.links(
        posix_prefix + '''
int main(void) {
    void (*volatile p)(void) = (void (*)(void))&@0@;
    return p == 0;
}
'''.format(fn),
        name: fn,
    )
    libc_have += {fn: found}
    conf.set10('PF_CONFIG_HAVE_' + fn.to_upper(), found)
endforeach

foreach h : ['dirent.h', 'dlfcn.h', 'sys/mman.h', 'afunix.h']
    conf.set10(
        'PF_CONFIG_HAVE_' + h.underscorify().to_upper(),
        cc.has_header(h),
    )
endforeach

# The value of O_CLOEXEC, which strict C99 hides (PF_O_CLOEXEC).
o_cloexec = 0
if not is_windows
    o_cloexec = cc.compute_int(
        'O_CLOEXEC',
        prefix: '#define _GNU_SOURCE 1\n#include <fcntl.h>',
    )
endif
conf.set('PF_CONFIG_O_CLOEXEC', o_cloexec)

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

# ---- Threads -----------------------------------------------------------

threads_opt = get_option('threads')
have_threads = false
threads_dep = dependency('', required: false)
if is_windows
    have_threads = not threads_opt.disabled()
elif not threads_opt.disabled()
    threads_dep = dependency('threads', required: threads_opt)
    have_threads = threads_dep.found() and cc.has_header('pthread.h')
    if threads_opt.enabled() and not have_threads
        error('threads enabled but pthreads were not found')
    endif
endif
conf.set10('PF_CONFIG_THREADS', have_threads)
conf.set10(
    'PF_CONFIG_HAVE_THREADS_H',
    cc.has_header('threads.h') and cc.links(
        '''
#include <threads.h>
static int run(void *arg) { return arg != 0; }
int main(void) {
    thrd_t t;
    return thrd_create(&t, run, 0);
}
''',
        dependencies: threads_dep,
        name: 'C11 threads',
    ),
)

config_h = configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration: conf,
    install: true,
    install_dir: get_option('includedir') / 'cpolyfill',
)
