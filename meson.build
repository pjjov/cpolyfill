# Copyright 2025 Predrag Jovanović
# SPDX-FileCopyrightText: 2025 Predrag Jovanović
# SPDX-License-Identifier: Apache-2.0

project('cpolyfill', 'c')

cpolyfill_dep = declare_dependency(
    include_directories: include_directories('include')
)

meson.override_dependency('cpolyfill', cpolyfill_dep)

tests = executable(
    'cpolyfill-test',
    dependencies: [cpolyfill_dep],
    sources: [
        'test/endian.c',
        'test/main.c',
        'test/overflow.c',
    ]
)

test('cpolyfill/endian', tests, args: ['endian'])
test('cpolyfill/overflow', tests, args: ['overflow'])
