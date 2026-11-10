#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0
#
# Checks that every public header can be used on its own:
#
#   compile  the header is the only thing included, in every supported C
#            standard (strict and GNU) and in C++, with warnings as errors
#            and -Wundef (all platform macros must be defined)
#   link     two translation units include it and are linked together with
#            the library, which fails if the header defines something that
#            is not static/inline
#
# Headers with a known defect are expected to fail (reported as "xfail").
# Pass --known-bugs to count those failures as errors.

import argparse
import os
import subprocess
import sys
import tempfile

# header -> why it fails
KNOWN = {}

C_STANDARDS = {
    'gcc': ['c99', 'gnu99', 'c11', 'gnu11', 'c17', 'gnu17', 'c23', 'gnu23'],
    'msvc': [None, 'c11', 'c17'],
}
CXX_STANDARDS = {
    'gcc': ['c++11', 'c++17', 'c++20'],
    'msvc': ['c++17', 'c++20'],
}

GCC_WARNINGS = ['-Wall', '-Wextra', '-Wundef', '-Werror']

# With PF_STD_NAMES, the system headers that define the same names are
# included after the header, so a conflicting definition shows up.
STD_NAMES_AFTER = '''\
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#if !defined(__STDC_NO_THREADS__) && defined(__has_include)
#if __has_include(<threads.h>)
#include <threads.h>
#endif
#endif
#endif
#if defined(__has_include)
#if __has_include(<stdbit.h>)
#include <stdbit.h>
#endif
#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#endif
#if __has_include(<endian.h>)
#include <endian.h>
#endif
#if __has_include(<byteswap.h>)
#include <byteswap.h>
#endif
#if __has_include(<strings.h>)
#include <strings.h>
#endif
#if __has_include(<uchar.h>)
#include <uchar.h>
#endif
#if __has_include(<sys/types.h>)
#include <sys/types.h>
#endif
#endif
'''


def compile_command(cc, style, std, source, includes, cxx=False):
    cmd = list(cc)
    if style == 'msvc':
        cmd += ['/nologo', '/TP' if cxx else '/TC', '/W4', '/WX']
        if std:
            cmd.append('/std:' + std)
        cmd += ['/I' + inc for inc in includes]
        return cmd + ['/Zs', source]
    cmd += GCC_WARNINGS
    if std:
        cmd.append('-std=' + std)
        if not std.startswith('gnu'):
            cmd.append('-pedantic-errors')
    if cxx:
        cmd += ['-x', 'c++']
    cmd += ['-I' + inc for inc in includes]
    return cmd + ['-fsyntax-only', source]


def link_command(cc, style, includes, libs, a, b, exe):
    cmd = list(cc)
    if style == 'msvc':
        cmd += ['/nologo', '/TC'] + ['/I' + inc for inc in includes]
        return cmd + [a, b, '/Fe' + exe, '/link'] + libs
    cmd += GCC_WARNINGS + ['-I' + inc for inc in includes]
    return cmd + [a, b, '-o', exe] + libs


def supported(cc, style, std, tmp, cxx=False):
    """Whether the compiler accepts the standard at all."""
    if not std:
        return True
    probe = os.path.join(tmp, 'probe.cpp' if cxx else 'probe.c')
    write(probe, 'int main(void) { return 0; }\n')
    flag = ('/std:' if style == 'msvc' else '-std=') + std
    extra = ['/Zs'] if style == 'msvc' else ['-fsyntax-only']
    ok, _ = run(list(cc) + [flag] + extra + [probe], tmp)
    return ok


def run(cmd, cwd):
    try:
        proc = subprocess.run(
            cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            universal_newlines=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired) as e:
        return False, str(e)
    return proc.returncode == 0, proc.stdout


def first_error(output):
    for line in output.splitlines():
        if 'error' in line.lower():
            return line.strip()
    lines = output.strip().splitlines()
    return lines[0] if lines else ''


def write(path, text):
    with open(path, 'w') as f:
        f.write(text)


def check_header(args, tmp, header, c_stds, cxx_stds):
    """Returns a list of (what, ok, output)."""
    include = '#include <cpolyfill/%s>\n' % header
    results = []

    source = os.path.join(tmp, 'only.c')
    write(source, include + 'int main(void) { return 0; }\n')
    names = os.path.join(tmp, 'names.c')
    write(names, '#define PF_STD_NAMES\n' + include + STD_NAMES_AFTER
          + 'int main(void) { return 0; }\n')
    for std in c_stds:
        ok, out = run(compile_command(
            args.cc, args.style, std, source, args.include), tmp)
        results.append(('compile ' + (std or 'default'), ok, out))
        ok, out = run(compile_command(
            args.cc, args.style, std, names, args.include), tmp)
        results.append(('PF_STD_NAMES ' + (std or 'default'), ok, out))

    if args.cxx:
        source = os.path.join(tmp, 'only.cpp')
        write(source, include + 'int main() { return 0; }\n')
        for std in cxx_stds:
            ok, out = run(compile_command(
                args.cxx, args.style, std, source, args.include, cxx=True), tmp)
            results.append(('compile ' + std, ok, out))

    a = os.path.join(tmp, 'a.c')
    b = os.path.join(tmp, 'b.c')
    write(a, include + 'int other(void);\nint main(void) { return other(); }\n')
    write(b, include + 'int other(void) { return 0; }\n')
    exe = os.path.join(tmp, 'linked.exe')
    ok, out = run(link_command(
        args.cc, args.style, args.include, args.lib + args.link_arg, a, b,
        exe), tmp)
    results.append(('link', ok, out))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', action='append', required=True,
                        help='C compiler command, one argument per --cc')
    parser.add_argument('--cxx', action='append', default=[],
                        help='C++ compiler command, one argument per --cxx')
    parser.add_argument('--style', choices=['gcc', 'msvc'], default='gcc')
    parser.add_argument('--include', action='append', required=True,
                        help='include directory (repeatable)')
    parser.add_argument('--header', action='append', required=True,
                        help='public header to check (repeatable)')
    parser.add_argument('--lib', action='append', default=[],
                        help='library to link with (repeatable)')
    parser.add_argument('--link-arg', action='append', default=[],
                        help='extra argument for the link check (repeatable)')
    parser.add_argument('--known-bugs', action='store_true')
    args = parser.parse_args()
    args.include = [os.path.abspath(p) for p in args.include]
    args.lib = [os.path.abspath(p) for p in args.lib]

    headers = sorted(os.path.basename(h) for h in args.header)
    failed = 0

    print('TAP version 13')
    print('1..%d' % len(headers))
    with tempfile.TemporaryDirectory() as tmp:
        c_stds = [s for s in C_STANDARDS[args.style]
                  if supported(args.cc, args.style, s, tmp)]
        cxx_stds = [s for s in CXX_STANDARDS[args.style]
                    if args.cxx and supported(args.cxx, args.style, s, tmp, True)]
        print('# C standards: %s' % ', '.join(s or 'default' for s in c_stds))
        if args.cxx:
            print('# C++ standards: %s' % ', '.join(cxx_stds))

        for n, header in enumerate(headers, 1):
            results = check_header(args, tmp, header, c_stds, cxx_stds)
            bad = [(what, out) for what, ok, out in results if not ok]
            known = KNOWN.get(header)

            if not bad:
                if known and not args.known_bugs:
                    print('ok %d - %s # TODO fixed? remove from KNOWN' % (n, header))
                else:
                    print('ok %d - %s' % (n, header))
                continue

            if known and not args.known_bugs:
                print('not ok %d - %s # TODO %s' % (n, header, known))
                continue

            failed += 1
            print('not ok %d - %s' % (n, header))
            for what, out in bad:
                print('#   %s: %s' % (what, first_error(out)))
                for line in out.strip().splitlines()[:20]:
                    print('#     ' + line)

    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
