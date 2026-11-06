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
    'src/errno.c',
    'src/utf16.c',
    'src/version.c',
)
if is_windows
    sources += files('src/win32/error.c')
endif

lib_args = ['-DPF__BUILDING']
if is_msvc
    lib_args += ['-D_CRT_SECURE_NO_WARNINGS']
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
