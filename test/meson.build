# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

suites = [
    'alloc',
    'assert',
    'atomic',
    'attribute',
    'cpuid',
    'bitwise',
    'ctype',
    'debug',
    'dirent',
    'dlfcn',
    'endian',
    'errno',
    'exception',
    'fs',
    'io',
    'macro',
    'mman',
    'overflow',
    'pipe',
    'platform',
    'process',
    'random',
    'semaphore',
    'signal',
    'socket',
    'std_names',
    'std_polyfill',
    'stdc',
    'stdlib',
    'string',
    'sysinfo',
    'term',
    'test',
    'threads',
    'time',
    'typeid',
    'types',
    'uchar',
    'unicode',
    'version',
]

# Suites of the optional modules.
if have_argparse
    suites += 'argparse'
endif
if have_cli
    suites += 'cli'
endif
if have_daemon
    suites += 'daemon'
endif

# The portable implementations, compared with the builtins.
test_sources = files(
    'main.c',
    'atomic_lib.c',
    'bitwise_portable.c',
    'exception_noloc.c',
    'overflow_portable.c',
)
foreach suite : suites
    test_sources += files(suite + '.c')
endforeach

test_args = [
    # The checks must not be compiled out, so tests cannot pass by accident.
    is_msvc ? '/UNDEBUG' : '-UNDEBUG',
    '-DPF_TEST_KNOWN_BUGS=@0@'.format(get_option('known_bugs') ? 1 : 0),
    '-DPF_TEST_ARGPARSE=@0@'.format(have_argparse ? 1 : 0),
    '-DPF_TEST_CLI=@0@'.format(have_cli ? 1 : 0),
    '-DPF_TEST_DAEMON=@0@'.format(have_daemon ? 1 : 0),
]
if not is_msvc
    test_args += '-Werror=undef'
endif

# A shared module for the dlfcn suite.
dl_module = shared_module(
    'cpolyfill-test-dl',
    'dl_module.c',
    include_directories: inc,
    install: false,
)
test_env = environment()
test_env.set('PF_TEST_DL_MODULE', dl_module.full_path())

# The tests link the library's objects directly, so they can also test the
# internal helpers (which are not exported from the shared library).
lib_objects = lib.extract_all_objects(recursive: true)

# The program the process suite starts.
child = executable(
    'cpolyfill-test-child',
    'child.c',
    objects: lib_objects,
    include_directories: inc,
    dependencies: sys_deps,
    install: false,
)
test_env.set('PF_TEST_CHILD', child.full_path())

# A test program with known outcomes, for the test suite.
sample = executable(
    'cpolyfill-test-sample',
    'sample.c',
    objects: lib_objects,
    include_directories: inc,
    dependencies: sys_deps,
    install: false,
)
test_env.set('PF_TEST_SAMPLE', sample.full_path())

# The same tests are built for several language standards, because what the
# headers do depends on the standard of the code including them:
#
#   default   the project default (gnu11)
#   gnu99     the oldest supported standard
#   gnu23     the newest standard
variants = {'default': []}
if not is_msvc
    variants += {'gnu99': ['c_std=gnu99']}
    if cc.has_argument('-std=gnu23')
        variants += {'gnu23': ['c_std=gnu23']}
    endif
endif

foreach name, options : variants
    exe = executable(
        'cpolyfill-test-' + name,
        test_sources,
        objects: lib_objects,
        include_directories: inc,
        dependencies: sys_deps,
        c_args: test_args,
        override_options: options,
        install: false,
    )

    prefix = name == 'default' ? 'cpolyfill' : 'cpolyfill-' + name
    foreach suite : suites
        test(
            prefix / suite,
            exe,
            args: ['--tap', suite],
            protocol: 'tap',
            suite: [name, suite],
            env: test_env,
            depends: [dl_module, child, sample],
            timeout: 120,
        )
    endforeach
endforeach

# The headers used from C++, where several macros have their own
# implementation.
if add_languages('cpp', required: false, native: false)
    cpp = meson.get_compiler('cpp')
    foreach std : is_msvc ? ['c++17', 'c++20'] : ['c++11', 'c++20']
        if not cpp.has_argument((is_msvc ? '/std:' : '-std=') + std)
            continue
        endif
        name = std.replace('+', 'x')
        cxx_exe = executable(
            'cpolyfill-test-' + name,
            'cxx.cpp',
            objects: lib_objects,
            include_directories: inc,
            dependencies: sys_deps,
            cpp_args: test_args,
            override_options: ['cpp_std=' + std],
            install: false,
        )
        test('cpolyfill' / name, cxx_exe, suite: ['cxx'])
    endforeach
endif

# Every public header on its own: it compiles in every supported standard
# and in C++, and links when included by two translation units.
python = find_program('python3', 'python', required: false)
if python.found()
    # The compilers and include directories, for the checks below.
    compiler_args = [
        '--style', is_msvc ? 'msvc' : 'gcc',
        '--include', meson.project_source_root() / 'include',
        '--include', meson.project_build_root() / 'include',
    ]
    foreach part : cc.cmd_array()
        compiler_args += ['--cc', part]
    endforeach
    if add_languages('cpp', required: false, native: false)
        foreach part : meson.get_compiler('cpp').cmd_array()
            compiler_args += ['--cxx', part]
        endforeach
    endif

    checker_args = [files('check_headers.py'), '--lib', lib] + compiler_args
    foreach header : public_headers
        checker_args += ['--header', header]
    endforeach
    # A sanitized library needs the sanitizer runtime to link.
    sanitize = get_option('b_sanitize')
    if meson.version().version_compare('>= 1.8')
        sanitize = ','.join(sanitize)
    endif
    if not is_msvc and sanitize not in ['', 'none']
        checker_args += '--link-arg=-fsanitize=' + sanitize
    endif
    if get_option('known_bugs')
        checker_args += '--known-bugs'
    endif
    test(
        'cpolyfill/headers',
        python,
        args: checker_args,
        suite: ['headers'],
        timeout: 600,
    )

    # Misuse of the compile-time checks must not compile.
    test(
        'cpolyfill/errors',
        python,
        args: [files('check_errors.py'), '--source', files('errors.c')]
            + compiler_args,
        protocol: 'tap',
        suite: ['errors'],
        timeout: 600,
    )
endif
