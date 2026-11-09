# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

suites = [
    'alloc',
    'errno',
    'platform',
    'utf16',
    'version',
]

test_sources = files('main.c')
foreach suite : suites
    test_sources += files(suite + '.c')
endforeach

test_args = [
    # The checks must not be compiled out, so tests cannot pass by accident.
    is_msvc ? '/UNDEBUG' : '-UNDEBUG',
    '-DPF_TEST_KNOWN_BUGS=@0@'.format(get_option('known_bugs') ? 1 : 0),
]
if not is_msvc
    test_args += '-Werror=undef'
endif

# The tests link the library's objects directly, so they can also test the
# internal helpers (which are not exported from the shared library).
lib_objects = lib.extract_all_objects(recursive: true)

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
            timeout: 120,
        )
    endforeach
endforeach

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
