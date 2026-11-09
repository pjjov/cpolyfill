#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0
#
# Checks that misuse of the compile-time checks fails to compile.
#
# The source defines PF_ERROR_CASES and is built once per case with
# -DPF_ERROR_CASE=n, in several C standards and in C++. Case 0 must compile
# (warnings are errors), every other case must fail.

import argparse
import os
import re
import subprocess
import sys
import tempfile

C_STANDARDS = {
    'gcc': ['c99', 'gnu99', 'c11', 'c23'],
    'msvc': [None, 'c11'],
}
CXX_STANDARDS = {
    'gcc': ['c++11', 'c++20'],
    'msvc': ['c++17'],
}


def command(cc, style, std, source, includes, case, cxx):
    cmd = list(cc)
    if style == 'msvc':
        cmd += ['/nologo', '/TP' if cxx else '/TC', '/W4', '/WX']
        if std:
            cmd.append('/std:' + std)
        cmd += ['/I' + inc for inc in includes]
        return cmd + ['/DPF_ERROR_CASE=%d' % case, '/Zs', source]
    cmd += ['-Wall', '-Wextra', '-Wundef', '-Werror', '-std=' + std]
    if not std.startswith('gnu'):
        cmd.append('-pedantic-errors')
    if cxx:
        cmd += ['-x', 'c++']
    cmd += ['-I' + inc for inc in includes]
    # -fsyntax-only would skip the error attribute, which is diagnosed
    # when code is generated.
    return cmd + ['-DPF_ERROR_CASE=%d' % case, '-c', '-o', os.devnull, source]


def run(cmd):
    try:
        proc = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            universal_newlines=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired) as e:
        return False, str(e)
    return proc.returncode == 0, proc.stdout


def supported(cc, style, std, tmp, cxx):
    if not std:
        return True
    probe = os.path.join(tmp, 'probe.cpp' if cxx else 'probe.c')
    with open(probe, 'w') as f:
        f.write('int main(void) { return 0; }\n')
    flag = ('/std:' if style == 'msvc' else '-std=') + std
    extra = ['/Zs'] if style == 'msvc' else ['-fsyntax-only']
    return run(list(cc) + [flag] + extra + [probe])[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', action='append', required=True)
    parser.add_argument('--cxx', action='append', default=[])
    parser.add_argument('--style', choices=['gcc', 'msvc'], default='gcc')
    parser.add_argument('--include', action='append', required=True)
    parser.add_argument('--source', required=True)
    args = parser.parse_args()
    includes = [os.path.abspath(p) for p in args.include]

    with open(args.source) as f:
        cases = int(re.search(r'#define PF_ERROR_CASES (\d+)', f.read())
                    .group(1))

    with tempfile.TemporaryDirectory() as tmp:
        builds = [(args.cc, std, False) for std in C_STANDARDS[args.style]
                  if supported(args.cc, args.style, std, tmp, False)]
        if args.cxx:
            builds += [(args.cxx, std, True)
                       for std in CXX_STANDARDS[args.style]
                       if supported(args.cxx, args.style, std, tmp, True)]

    failed = 0
    print('TAP version 13')
    print('1..%d' % (cases + 1))
    for case in range(cases + 1):
        bad = []
        for cc, std, cxx in builds:
            ok, out = run(command(cc, args.style, std, args.source,
                                  includes, case, cxx))
            if ok != (case == 0):
                bad.append((std or 'default', out))
        what = 'compiles' if case == 0 else 'fails to compile'
        if not bad:
            print('ok %d - case %d %s' % (case + 1, case, what))
            continue
        failed += 1
        print('not ok %d - case %d %s' % (case + 1, case, what))
        for std, out in bad:
            print('#   %s' % std)
            for line in out.strip().splitlines()[:10]:
                print('#     ' + line)

    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
