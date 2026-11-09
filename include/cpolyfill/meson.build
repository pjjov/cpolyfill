# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0

public_headers = files(
    'assert.h',
    'attribute.h',
    'errno.h',
    'macro.h',
    'platform.h',
    'stdc.h',
    'types.h',
    'version.h',
)

version = meson.project_version().split('.')

conf = configuration_data()
conf.set('PF_VERSION_MAJOR', version[0])
conf.set('PF_VERSION_MINOR', version[1])
conf.set('PF_VERSION_PATCH', version[2])
conf.set('PF_VERSION_STRING', meson.project_version())
conf.set10('PF_CONFIG_SHARED', get_option('default_library') == 'shared')

config_h = configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration: conf,
    install: true,
    install_dir: get_option('includedir') / 'cpolyfill',
)
