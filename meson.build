# Copyright 2025-2026 Предраг Јовановић
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

project('cpolyfill', 'c', version: '1.7.2')

cpolyfill_dep = declare_dependency(
    include_directories: include_directories('include')
)

meson.override_dependency('cpolyfill', cpolyfill_dep)

install_headers(
    'include/pf_argparse.h',
    'include/pf_array.h',
    'include/pf_assert.h',
    'include/pf_atomic.h',
    'include/pf_attribute.h',
    'include/pf_bitwise.h',
    'include/pf_cli.h',
    'include/pf_cpuinfo.h',
    'include/pf_ctype.h',
    'include/pf_daemon.h',
    'include/pf_dirent.h',
    'include/pf_dlsym.h',
    'include/pf_endian.h',
    'include/pf_errno.h',
    'include/pf_exception.h',
    'include/pf_filesystem.h',
    'include/pf_io.h',
    'include/pf_macro.h',
    'include/pf_overflow.h',
    'include/pf_pipe.h',
    'include/pf_process.h',
    'include/pf_semaphore.h',
    'include/pf_socket.h',
    'include/pf_string.h',
    'include/pf_test.h',
    'include/pf_threads.h',
    'include/pf_typeid.h',
    'include/pf_types.h',
)

if not meson.is_subproject()
    cc = meson.get_compiler('c')
    is_msvc = cc.get_argument_syntax() == 'msvc'
    is_windows = host_machine.system() == 'windows'

    # Everything the headers may need from the system.
    test_deps = [cpolyfill_dep, dependency('threads')]
    if is_windows
        foreach lib : ['ws2_32', 'ole32', 'shell32', 'netapi32', 'advapi32']
            test_deps += cc.find_library(lib, required: false)
        endforeach
    else
        test_deps += cc.find_library('dl', required: false)
    endif

    test_args = [
        # The tests must not be compiled out, so they cannot pass by accident.
        is_msvc ? '/UNDEBUG' : '-UNDEBUG',
        '-DPF_TEST_KNOWN_BUGS=@0@'.format(get_option('known_bugs') ? 1 : 0),
    ]

    suites = [
        'argparse',
        'array',
        'assert',
        'atomic',
        'attribute',
        'bitwise',
        'cli',
        'cpuinfo',
        'ctype',
        'daemon',
        'dirent',
        'dlsym',
        'endian',
        'errno',
        'exception',
        'filesystem',
        'io',
        'macro',
        'overflow',
        'pipe',
        'process',
        'semaphore',
        'socket',
        'string',
        'test',
        'threads',
        'typeid',
        'types',
    ]

    test_sources = files('test/main.c')
    foreach suite : suites
        test_sources += files('test' / suite + '.c')
    endforeach

    # The same tests are built several times, because what the headers do
    # depends on the language standard and on which features are missing:
    #
    #   default          what the compiler picks, usually the newest standard.
    #   gnu99            C11 features (threads, atomics, ...) are missing, so
    #                    the polyfills are used instead of the system ones.
    #   atomic-fallback  atomics implemented with mutexes, as on platforms
    #                    that have threads but no atomic instructions.
    variants = {
        'default': {'args': [], 'suites': suites},
    }
    if not is_msvc
        variants += {
            'gnu99': {'args': ['-std=gnu99'], 'suites': suites},
            'atomic-fallback': {
                'args': ['-DPF_ATOMIC_NONE'],
                'suites': ['atomic'],
            },
        }
    endif

    foreach name, variant : variants
        exe = executable(
            'cpolyfill-test-' + name,
            install: false,
            dependencies: test_deps,
            sources: test_sources,
            c_args: test_args + variant['args'],
        )

        prefix = name == 'default' ? 'cpolyfill' : 'cpolyfill-' + name
        foreach suite : variant['suites']
            test(
                prefix / suite,
                exe,
                args: ['--tap', suite],
                protocol: 'tap',
                suite: [name, suite],
                timeout: 120,
            )
        endforeach
    endforeach

    # Every header on its own: it compiles, and links when used twice.
    python = find_program('python3', 'python', required: false)
    if python.found()
        checker_args = [
            files('test/check_headers.py'),
            '--include', meson.current_source_dir() / 'include',
            '--style', is_msvc ? 'msvc' : 'gcc',
        ]
        foreach part : cc.cmd_array()
            checker_args += ['--cc', part]
        endforeach
        if is_windows and not is_msvc
            checker_args += ['--lib=-lws2_32', '--lib=-lole32', '--lib=-lshell32', '--lib=-lnetapi32']
        elif not is_windows
            checker_args += ['--lib=-lpthread', '--lib=-ldl']
        endif
        if get_option('known_bugs')
            checker_args += '--known-bugs'
        endif
        test('cpolyfill/headers', python, args: checker_args, suite: ['headers'], timeout: 600)
    endif
endif

pkg = import('pkgconfig')
pkg.generate(
    name: 'cpolyfill',
    filebase: 'cpolyfill',
    description: 'Filling the gaps between C standards and compilers',
    version: meson.project_version(),
    subdirs: '',
    extra_cflags: []
)