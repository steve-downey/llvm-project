# Handoff — S01 Feature flag `-fbacktick`

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `6aec144f9244`
- **Date / agent:** 2026-06-14

## What changed

- `clang/include/clang/Basic/LangOptions.def` — added `LANGOPT(Backtick, 1, 0, NotCompatible, "backtick operator and identifier escaping")` after `Reflection`.
- `clang/include/clang/Options/Options.td` — added `defm backtick : BoolFOption<"backtick", LangOpts<"Backtick">, DefaultFalse, PosFlag<SetTrue, [], [], "...">, NegFlag<SetFalse>, BothFlags<[], [ClangOption, CC1Option]>>;` after the `reflection` entry (line ~3979).
- `clang/lib/Driver/ToolChains/Clang.cpp` — added `Args.addLastArg(CmdArgs, options::OPT_fbacktick, options::OPT_fno_backtick);` immediately after the `sized-deallocation` block (~line 7748).
- `clang/test/Driver/fbacktick.c` — new lit test asserting `-fbacktick` is accepted, forwarded to `-cc1`, and absent by default.
- `ops/PLAN.md` — S01 box checked, status log row added.

## Verification evidence

```
# Syntax-only on empty file
clang -fbacktick -x c++ -fsyntax-only /dev/null  → exit 0, no warnings

# Flag forwarded to -cc1
clang -fbacktick -x c++ -fsyntax-only -### /dev/null 2>&1 | grep fbacktick
→ "-fbacktick" (appears in cc1 argv)

# Lit test
llvm-lit -v clang/test/Driver/fbacktick.c
→ PASS: Clang :: Driver/fbacktick.c (1 of 1)

# check-clang
52210 total: 46442 passed, 26 xfail, 5732 unsupported, 6 skipped, 4 failed
Failed (all environmental baseline): dump-config-objc-stdin.m,
  DirectoryWatcherTest/{DeleteFile,InitialScanSync,ModifyFile}
```

The DirectoryWatcher subtests differ from the S00 baseline (AddFiles/DeleteWatchedDir/InvalidatedWatcher/InvalidatedWatcherAsync) — same inotify root cause, different subtests each run. Not a regression.

## Deviations from the plan / design

- **DEV-01 (already known):** `Options.td` is at `clang/include/clang/Options/Options.td`, not `clang/include/clang/Driver/Options.td`.
- **DEV-02 (already known):** `LANGOPT` is 5-arg; used `NotCompatible` as the compatibility kind.
- **DEV-03 (new):** The step file and S00 handoff implied that `MarshallingInfoFlag` + `BothFlags<[], [ClangOption, CC1Option]>` would provide automatic driver forwarding. It does **not**. The flag is parsed by the driver (ClangOption) but NOT forwarded to cc1 without an explicit `Args.addLastArg(CmdArgs, OPT_fbacktick, OPT_fno_backtick)` call in `Clang.cpp::ConstructJob()`. Pattern verified against `sized-deallocation` (which does forward) and `reflection` (which does NOT forward — only CC1Option, not ClangOption). Added explicit forwarding after the `sized-deallocation` block.

## Discoveries affecting later steps

- **Exact LangOpt field name:** `LangOpts.Backtick` (check with `getLangOpts().Backtick`).
- **Exact flag spellings:** `-fbacktick` / `-fno-backtick`; OPT names `OPT_fbacktick` / `OPT_fno_backtick`.
- **Off-flag backtick behavior:** A bare backtick in source with no `-fbacktick` hits the `default:` branch in `LexTokenInternal` (Lexer.cpp:4552), yields `tok::unknown`, and triggers the generic "expected ';' after top level declarator" parse error. There is no dedicated diagnostic. S04 should phrase the "backtick without -fbacktick" diagnostic (if any) to be distinct from this generic error.
- **Lexer switch layout:** `case '@':` is at Lexer.cpp:4505; `case '\\':` at 4533; `default:` at 4552. The new backtick case should go between `@` and `\\` (or just before `default:`).

## Forward notes for the NEXT step (S02 — Lexer: backtick punctuator token)

Read `ops/steps/02-lexer-token.md` carefully; the file is accurate. Concrete pointers:

- **TokenKinds.def:** Add `PUNCTUATOR(backtick, "\`")` near line 262 (after `tok::at`).
- **Lexer.cpp switch:** Insert a new `case '\`':` block just before `case '\\':` (line 4533) or after `case '@':` (line 4530). The body should be:
  ```cpp
  case '`':
    if (LangOpts.Backtick) {
      Kind = tok::backtick;
    } else {
      Kind = tok::unknown;
    }
    break;
  ```
  (The `default:` at 4552 already handles unknown ASCII with `Kind = tok::unknown`; falling through to it works but an explicit `else` is cleaner.)
- **String/char/raw-string literals:** Backtick inside string literals, char literals, comments, and raw-string delimiters is consumed BEFORE the punctuator switch (by `LexStringLiteral`, `LexCharConstant`, `SkipBlockComment`, etc.). No special handling needed.
- **Test:** Use `-dump-tokens` to assert `tok::backtick` appears with the flag and `tok::unknown` without it. The `-dump-tokens` output for `tok::unknown` says `unknown '`'`; for the new token it will say `backtick '`'`.
- **`-fno-backtick` by default means no token:** When the flag is off, a backtick in non-literal context produces `tok::unknown`, and standard C++ compile errors follow. The lit test should cover both paths.

## Open risks / TODOs

- DEV-03 (driver forwarding) should be logged in `ops/DEVIATIONS.md`; update that file.
- The stray `/home/sdowney/src/.clang-format` remains; still relevant to S10.
- inotify DirectoryWatcher tests remain flaky by host state; always diff against the 5-failure S00 baseline, not zero.
