#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 Предраг Јовановић
# SPDX-License-Identifier: Apache-2.0
#
# Checks that every header can be used on its own, the way a user copying a
# single file into a project would use it:
#
#   compile  the header is the only thing included, in several C standards
#   link     two translation units include it and are linked together, which
#            fails if the header defines something that is not static/inline
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

STANDARDS = {
    'gcc': ['gnu99', 'gnu11', 'gnu17', None],
    'msvc': [None],
}


def compile_command(cc, style, std, source):
    cmd = list(cc)
    if style == 'msvc':
        cmd += ['/nologo', '/TC']
        if std:
            cmd.append('/std:' + std)
        cmd += ['/Zs', source]
    else:
        cmd += ['-Werror=implicit-function-declaration']
        if std:
            cmd.append('-std=' + std)
        cmd += ['-fsyntax-only', source]
    return cmd


def link_command(cc, style, libs, a, b, exe):
    cmd = list(cc)
    if style == 'msvc':
        return cmd + ['/nologo', '/TC', a, b, '/Fe' + exe, '/link'] + libs
    return cmd + ['-Werror=implicit-function-declaration', a, b, '-o', exe] + libs


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


def check_header(args, tmp, header):
    """Returns a list of (what, ok, output)."""
    path = os.path.abspath(os.path.join(args.include, header)).replace('\\', '/')
    results = []

    for std in STANDARDS[args.style]:
        src = os.path.join(tmp, 'one.c')
        write(src, '#include "%s"\n' % path)
        ok, out = run(compile_command(args.cc, args.style, std, src), tmp)
        results.append(('compile ' + (std or 'default'), ok, out))
        if not ok:
            return results

    a = os.path.join(tmp, 'a.c')
    b = os.path.join(tmp, 'b.c')
    write(a, '#include "%s"\nint pf_other(void) { return 0; }\n' % path)
    write(b, '#include "%s"\nint main(void) { return 0; }\n' % path)
    ok, out = run(
        link_command(args.cc, args.style, args.lib, a, b,
                     os.path.join(tmp, 'link.exe')), tmp)
    results.append(('link twice', ok, out))

    if header == 'pf_assert.h':
        # With NDEBUG, the checks become plain conditions.
        write(a, '#define NDEBUG\n#include "%s"\n'
                 'int main(void) { return pf_check(1) && !pf_check_false(1) '
                 '? 0 : 1; }\n' % path)
        ok, out = run(compile_command(args.cc, args.style, None, a), tmp)
        results.append(('NDEBUG', ok, out))
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--include', required=True, help='directory of the headers')
    ap.add_argument('--style', choices=['gcc', 'msvc'], default='gcc')
    ap.add_argument('--known-bugs', action='store_true')
    ap.add_argument('--lib', action='append', default=[],
                    help='linker argument, can be repeated')
    ap.add_argument('--cc', action='append', required=True,
                    help='part of the compiler command, can be repeated')
    args = ap.parse_args()

    headers = sorted(h for h in os.listdir(args.include)
                     if h.startswith('pf_') and h.endswith('.h'))
    failures = 0
    expected = 0
    fixed = []

    with tempfile.TemporaryDirectory() as tmp:
        for header in headers:
            results = check_header(args, tmp, header)
            bad = [r for r in results if not r[1]]

            if header in KNOWN and not args.known_bugs:
                if not bad:
                    fixed.append(header)
                for what, ok, out in bad:
                    expected += 1
                    print('xfail  %-16s %-14s %s' % (header, what, KNOWN[header]))
                continue

            for what, ok, out in bad:
                failures += 1
                print('FAIL   %-16s %-14s %s' % (header, what, first_error(out)))

    for header in fixed:
        print('note   %s passes now; remove it from KNOWN' % header)
    print('%d headers, %d failures, %d expected failures'
          % (len(headers), failures, expected))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
