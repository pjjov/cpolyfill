# cpolyfill

C library filling the gaps between C standards, compilers and operating
systems. Being rewritten from v1 (header-only, `include/pf_*.h`, branch
`main`) to v2 (linkable meson library, `include/cpolyfill/*.h` + `src/`,
branch `v2`).

**On `v2`, read [REWRITE.md](REWRITE.md) first.** It holds the plan, the
step checklist, approved specs, conventions, the decision log and open
questions. Keep it up to date as you work.

## Working rules

- Go one step at a time. Propose a module's API to the maintainer and wait
  for approval before implementing, unless REWRITE.md already has an
  approved spec for it.
- Commit as work progresses. Conventional-commit subject line only
  (`feat(threads): ...`), no body, no co-author trailer.
- Never commit `update-headers.sh` or the root `allocator.h` (the
  maintainer's local files).
- Public headers are C99 and C++ compatible, and every macro used in `#if`
  is always defined (`-Wundef` is enforced by the header check). Library
  sources are C11 and include `src/internal.h` first.
- Include public headers as `<cpolyfill/name.h>`, never `"name.h"`.

## Build and test

```sh
meson setup build && meson test -C build
CC=clang meson setup build-asan -Db_sanitize=address,undefined -Db_lundef=false
meson setup build-static -Ddefault_library=static
build/test/cpolyfill-test-default --list     # suites and tests
```

No Windows toolchain is available on the maintainer's Linux machine, so
`src/win32/` code is not compiled locally. Say so when reporting.
