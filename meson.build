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

# System libraries the library needs (src/meson.build). They are passed on
# to everything that depends on cpolyfill, so users never list them.
sys_deps = []

# The public headers, the optional modules and the generated
# <cpolyfill/config.h>, with what the C library has.
subdir('include/cpolyfill')

inc = include_directories('include')

# The library itself.
subdir('src')

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
