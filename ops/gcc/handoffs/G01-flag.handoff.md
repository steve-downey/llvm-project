# Handoff — G01 feature flag -fbacktick

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ e1575e693087
- **Date / agent:** 2026-06-27

## What changed

- `/home/sdowney/bld/gcc/gcc-backtick/gcc/c-family/c.opt`: added 4-line stanza (name + properties + description + blank separator) for `-fbacktick`, inserted alphabetically before `fchar8_t` (after the `fcanonical-system-headers` block, around line 1767).

## Verification evidence

```
# Build
cd /home/sdowney/bld/gcc/gcc-backtick-build && make -j18 all-gcc
→ success; -fself-test: 7664580 pass(es) in 0.368601 seconds

# Flag acceptance (via cc1plus — xg++ fails on missing liblto_plugin.so in dev build)
/home/sdowney/bld/gcc/gcc-backtick-build/gcc/cc1plus -fbacktick  -fsyntax-only /tmp/test_backtick.cc
→ exit 0 (PASS)

/home/sdowney/bld/gcc/gcc-backtick-build/gcc/cc1plus -fno-backtick -fsyntax-only /tmp/test_backtick.cc
→ exit 0 (PASS)

# Help text
/home/sdowney/bld/gcc/gcc-backtick-build/gcc/cc1plus --help 2>&1 | grep backtick
→ -fbacktick                  [disabled]

# Generated code confirming var and enum
grep backtick /home/sdowney/bld/gcc/gcc-backtick-build/gcc/options.h
→ extern int flag_backtick;
→ int x_flag_backtick;
→ #define flag_backtick global_options.x_flag_backtick
→ OPT_fbacktick = 1142

# Regression (targeted)
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

1. **`xg++` driver unusable for `-fsyntax-only` gate:** `xg++` fails with
   `liblto_plugin.so not found` even with `-fsyntax-only`, because the dev
   build (`make all-gcc`) does not build the LTO plugin. Used `cc1plus` directly
   instead. This does not affect feature correctness. `xg++` does run correctly
   for `g++.dg` tests via the dejagnu harness (those tests call it differently).
   Note for all G-agents: use `cc1plus` directly for quick syntax checks.

2. **`--help=c++` via `xg++` fails:** `xg++` tries to invoke `cc1` (not
   `cc1plus`) for the help output and fails because `cc1` is not built. The
   flag is visible via `cc1plus --help` and confirmed in `options.h`.

## Discoveries affecting later steps

### `flag_backtick` access
- Global variable: `flag_backtick` (macro aliasing `global_options.x_flag_backtick`)
- Enum value: `OPT_fbacktick = 1142` in the generated `options.h`
- Readable directly from any GCC C++ front-end translation unit that includes
  `config.h` / `system.h` → `options.h` (which is essentially all of them)

### libcpp gating pattern (for G02)
From `gcc/c-family/c-opts.cc`:
- Line 253: `cpp_opts->dollars_in_ident = DOLLARS_IN_IDENTIFIERS;`  (init)
- Line 491–492: `case OPT_fdollars_in_identifiers: cpp_opts->dollars_in_ident = value; break;`

Mirror this for `OPT_fbacktick` → `cpp_opts->backtick_is_operator = value` in the same file.

### stray-backtick path in libcpp/lex.cc
Backtick (`'`'`, ASCII 0x60) currently falls into the `default:` branch of the
big character switch in `_cpp_lex_direct` (around line 4381 of `libcpp/lex.cc`).
It ends up as `CPP_OTHER` via `create_literal(…, CPP_OTHER)` at line 4435.
The C++ parser then emits "error: stray …" from that `CPP_OTHER` token.
G02's `case '`':` should be inserted just before `default:` (after `case '@':` at
line 4379).

## Forward notes for the NEXT step (G02 — libcpp CPP_BACKTICK token)

1. **Three files to touch:**
   - `libcpp/include/cpplib.h` — add `OP(BACKTICK, "\`")` to TTYPE_TABLE
     (line 117 area, after `OP(ATSIGN, "@")` is a natural slot since `@` and
     `` ` `` are both non-standard punctuators)
   - `libcpp/include/cpplib.h` — add `unsigned char backtick_is_operator;` to
     `struct cpp_options` around line 462, next to `dollars_in_ident`
   - `libcpp/lex.cc` — add `case '`':` before `default:` (~line 4381), guarded
     by `CPP_OPTION (pfile, backtick_is_operator)`; if off, fall through to
     `default:` (existing CPP_OTHER path = stray-character warning unchanged)
   - `gcc/c-family/c-opts.cc` — add handler at line ~492:
     `case OPT_fbacktick: cpp_opts->backtick_is_operator = value; break;`

2. **TTYPE_TABLE entry:** Use `OP(BACKTICK, "\`")` (the OP macro with spelling).
   Natural placement: after `OP(ATSIGN, "@")` at line 117. This gives
   `CPP_BACKTICK` in the enum. The `OP` macro handles both `CPP_` prefix and
   the `.val.str` spelling.

3. **Test approach:** Since G03 (parse) hasn't landed yet, the G02 test can
   only confirm:
   - With `-fbacktick`: a `` ` `` in source does NOT trigger stray-character
     error (the token exists but is silently ignored or gives a different error)
   - Without `-fbacktick`: `` ` `` still triggers stray-character error
   Write a `.C` test using `// { dg-error "stray" }` / `// { dg-no-error }` as
   appropriate.

4. **Pitfall: libcpp is language-agnostic.** Never lex CPP_BACKTICK
   unconditionally — always guard on `CPP_OPTION (pfile, backtick_is_operator)`.
   A C file with `-fbacktick` should not be affected (the option won't be set
   for the C front end).

5. **Commit:** `[backtick][gcc] G02: libcpp CPP_BACKTICK token`

## Open risks / TODOs

- `xg++` driver is not usable for direct invocation in dev builds (missing
  LTO plugin and cc1). All G-agents should use `cc1plus` directly for
  quick checks. The `make check-c++` harness works correctly via `xg++`.
- The full g++.dg baseline (~9958 pre-existing failures) was not re-run for
  G01 since a flag-only c.opt change cannot regress tests. Run it for G03
  when actual parsing behavior changes.
