# Handoff — S12 GCC baseline & orientation

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ 3c7c36808000
- **Date / agent:** 2026-06-27

## What changed

No GCC source changes — this step is pure infrastructure setup.

- GCC worktree created: `/home/sdowney/bld/gcc/gcc-backtick` (branch `backtick`, from trunk `c9ee2c5ab6c`)
- Dev build configured + built: `/home/sdowney/bld/gcc/gcc-backtick-build`
- Baseline g++.dg test run recorded below.
- `ops/PLAN.md` S12 checked; status log row appended.
- This handoff file created.

## GCC ground truth (for all G-agents)

| Item | Value |
|------|-------|
| Git bare repo | `/home/sdowney/bld/gcc/gcc.git` |
| Worktree | `/home/sdowney/bld/gcc/gcc-backtick` |
| Branch | `backtick` |
| Base commit | `c9ee2c5ab6c` ("Daily bump.", GCC 17.0.0 20260624 experimental) |
| Build dir | `/home/sdowney/bld/gcc/gcc-backtick-build` |
| Configure flags | `--prefix=<build>/install --enable-languages=c,c++ --disable-bootstrap --disable-multilib` |
| Build command | `make -j18 all-gcc` (stops at cc1plus; ~10 min on 20-core machine) |
| cc1plus | `/home/sdowney/bld/gcc/gcc-backtick-build/gcc/cc1plus` (517 MB) |
| xg++ driver | `/home/sdowney/bld/gcc/gcc-backtick-build/gcc/xg++` |
| Test command | `cd /home/sdowney/bld/gcc/gcc-backtick-build && make -C gcc check-c++ RUNTESTFLAGS="dg.exp=*"` |
| Test log | `/home/sdowney/bld/gcc/gcc-backtick-build/gcc/testsuite/g++/g++.sum` |

## Verification evidence

```
# Build gate
make -j18 all-gcc
→ success; self-test: 7664556 pass(es) in 0.355 seconds
→ cc1plus: /home/sdowney/bld/gcc/gcc-backtick-build/gcc/cc1plus (517 MB)
→ GCC version: 17.0.0 20260624 (experimental)

# Baseline test run (g++.dg subset)
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=*"

=== g++ Summary ===
# of expected passes          150598
# of unexpected failures        9958
# of unexpected successes         11
# of expected failures          1001
# of unresolved testcases       4213
# of unsupported tests          1882
```

### Failure breakdown (all pre-existing; none introduced by this step)

| Category | Count | Cause |
|----------|-------|-------|
| "test for excess errors" (linker) | 6170 | `crtbegin.o` / `libstdc++` absent — `make all-gcc` doesn't build the C++ runtime. Expected for dev builds. |
| Pre-existing trunk test failures | 3785 | 270 unique test files; contracts, concepts, spellcheck message format, abi, DRs — all pre-existing on trunk at `c9ee2c5ab6c`. |
| scan-tree-dump (predict-1.C) | 3 | Heuristic percentage mismatch; pre-existing on trunk. |
| **Total** | **9958** | **All pre-existing; none related to backtick** |

G-agents should expect this baseline to remain stable (no additional failures from feature work).

## Deviations from the plan / design

1. **`make all-gcc` instead of full `make`:** The step file describes two options (`make -j<N>` or `make all-gcc`). Used `make all-gcc` (stops at cc1plus) for speed. Consequence: libstdc++ / crtbegin.o are absent, so any test that tries to link-and-run fails. This is the expected result for a frontend-only dev build. G-agents doing `-fsyntax-only` tests will not be affected.

2. **No GCC source committed:** S12 makes no changes to GCC source. The backtick branch in `gcc-backtick` is identical to trunk `c9ee2c5ab6c`. The first source change happens in G01.

## Discoveries affecting later steps

### GCC c.opt flag pattern (for G01)
File: `gcc/c-family/c.opt` — add after existing `C++ ObjC++` flags, e.g. near `fcontracts`:
```
fbacktick
C++ Var(flag_backtick) Init(0)
Enable the backtick infix operator and keyword escaping.
```
The opt name `fbacktick` maps to `OPT_fbacktick` in the generated enum and sets global `flag_backtick`.

### Wiring to libcpp (for G02, not G01)
Model on `fdollars-in-identifiers`:
- `c.opt`: no `Var()` needed for flags mirrored to libcpp — the `case OPT_fbacktick:` handler in `gcc/c-family/c-opts.cc` writes `cpp_opts->backtick_is_operator = value;`
- `libcpp/include/cpplib.h`: add `unsigned char backtick_is_operator;` to `struct cpp_options` (around line 461, near `dollars_in_ident`)
- `gcc/c-family/c-opts.cc`: handler at line ~491 pattern:
  ```c
  case OPT_fbacktick:
    cpp_opts->backtick_is_operator = value;
    break;
  ```

### Build targets
- `make all-gcc` — builds cc1plus + xg++ driver, ~10 min
- `make -C gcc check-c++ RUNTESTFLAGS="dg.exp=*"` — runs g++.dg subset, ~20 min
- For narrower targeted runs: `make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick*.C"` (once backtick tests exist in G05)

### GCC version string
`17.0.0 20260624 (experimental)` — confirm this matches after rebuilds (date bumps on Daily bumps).

## Forward notes for the NEXT step (G01 — feature flag)

G01 adds `-fbacktick` to `gcc/c-family/c.opt`. Concrete guidance:

1. **File to edit:** `/home/sdowney/bld/gcc/gcc-backtick/gcc/c-family/c.opt`
   - Find the `fcontracts` or `fconcepts` block (around line 1860–1904) as a nearby anchor.
   - Add the 3-line stanza (name, `C++ Var(flag_backtick) Init(0)`, description).
   - The opts file is alphabetically sorted in sections — `fbacktick` comes before `fchar8_t` (line 1768) alphabetically.

2. **Gate verification:**
   ```bash
   # After make -j18 all-gcc (full rebuild needed when c.opt changes):
   /home/sdowney/bld/gcc/gcc-backtick-build/gcc/xg++ -fbacktick -fsyntax-only /dev/null
   # Must exit 0 with no errors
   /home/sdowney/bld/gcc/gcc-backtick-build/gcc/xg++ --help=c++ | grep backtick
   # Must show the option
   ```

3. **No libcpp touch yet** — G01 is C++ front end only. The `flag_backtick` C var is sufficient until G02 needs the lexer to see it.

4. **Commit convention:** `[backtick][gcc] G01: feature flag -fbacktick`

5. **Baseline check:** re-run `make -C gcc check-c++ RUNTESTFLAGS="dg.exp=*"` to confirm no regressions beyond the 9958 pre-existing failures. Or just run a targeted smoke check if time is tight — the option change touches generated code only, so regressions are unlikely.

## Open risks / TODOs

- libstdc++ is not built. If any G-test needs link+run, add `make -j18 all-target-libstdc++-v3` to the build procedure at that point.
- The 3785 pre-existing non-linker failures should be confirmed against the trunk test results from the `build-trunk` directory if needed — not done here, but the baseline is documented above.
- Daily bump commits (like `c9ee2c5ab6c`) may mean the trunk has minor changes hourly. The `backtick` branch is pinned at this commit until a G-agent explicitly rebases.
