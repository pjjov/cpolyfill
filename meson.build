# Copyright 2025 Predrag Jovanović
# SPDX-FileCopyrightText: 2025 Predrag Jovanović
# SPDX-License-Identifier: Apache-2.0

project('cpolyfill', 'c')

cpolyfill_dep = declare_dependency(
    include_directories: include_directories('include')
)

meson.override_dependency('cpolyfill', cpolyfill_dep)

install_headers(
    'include/pf_argparse.h',
    'include/pf_assert.h',
    'include/pf_bitwise.h',
    'include/pf_cpuinfo.h',
    'include/pf_ctype.h',
    'include/pf_endian.h',
    'include/pf_errno.h',
    'include/pf_macro.h',
    'include/pf_overflow.h',
    'include/pf_socket.h',
    'include/pf_test.h',
    'include/pf_typeid.h',
    'include/pf_types.h',
)

tests = executable(
    'cpolyfill-test',
    dependencies: [cpolyfill_dep],
    sources: [
        'test/bitwise.c',
        'test/ctype.c',
        'test/endian.c',
        'test/macro.c',
        'test/main.c',
        'test/overflow.c',
    ]
)

test('cpolyfill/bitwise', tests, args: ['bitwise'], protocol: 'tap')
test('cpolyfill/ctype', tests, args: ['ctype'], protocol: 'tap')
test('cpolyfill/endian', tests, args: ['endian'], protocol: 'tap')
test('cpolyfill/macro', tests, args: ['macro'], protocol: 'tap')
test('cpolyfill/overflow', tests, args: ['overflow'], protocol: 'tap')
