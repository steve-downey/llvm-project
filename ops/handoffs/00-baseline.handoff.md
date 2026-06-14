# Handoff — S00 Branch, worktree, and baseline

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (this is the tip commit on the branch; base commit `a815e6f267c12417757743a95fdc89cc3cde4499`)
- **Date / agent:** 2026-06-14

## What changed
No feature code. Process-only:
- Created git worktree `/home/sdowney/src/llvm/backtick` on new branch `backtick` from base `a815e6f267c1`.
- Moved the plan scaffolding (`ops/`, `docs/backtick-operator-design.md`) out of pristine `main` and onto the `backtick` branch so per-step commits (status log, checkboxes, handoffs) live with the feature work. `main` stays untouched.
- Configured dev build dir `/home/sdowney/src/llvm/build-backtick` (Ninja, Release, **assertions ON**, projects `clang;clang-tools-extra`, `LLVM_TARGETS_TO_BUILD=host`, no runtimes, no bootstrap, compilers `clang++-23`/`clang-23`, 12 compile / 1 link job).
- Built stage-1 `clang` (target `clang`, not `all`).

## Verification evidence
Pinned commands (use these verbatim):
```bash
WT=/home/sdowney/src/llvm/backtick            # worktree, branch: backtick
B=/home/sdowney/src/llvm/build-backtick         # dev build dir
ninja -C "$B" clang                            # build the compiler  (~3250 edges)
ninja -C "$B" check-clang                      # full regression gate
"$B"/bin/llvm-lit -v "$B"/tools/clang/test/Parser   # fast targeted gate
```
- `clang` builds clean: `clang version 23.0.0git (... a815e6f267c1)`.
- **`check-clang` baseline** (`Testing Time: 177s`, 52209 tests discovered):
  - Passed: **46440**, Expectedly Failed: 26, Unsupported: 5732, Skipped: 6.
  - **Failed: 5 — ALL environmental / pre-existing on untouched HEAD. This is the known-failing baseline; do NOT treat these as your regression:**
    1. `Clang :: Format/dump-config-objc-stdin.m` — clang-format walks up the dir tree and picks up a stray **`/home/sdowney/src/.clang-format`** (dated 2018) that has no Objective-C section, so `-dump-config` for an ObjC file errors and emits nothing. Environmental, not a code bug. **Relevant to S10** (clang-format) — any clang-format run from this tree inherits that stray config.
    2–5. `Clang-Unit :: ./AllClangUnitTests/DirectoryWatcherTest/{AddFiles,DeleteWatchedDir,InvalidatedWatcher,InvalidatedWatcherAsync}` — `inotify_init1()` "Too many open files". `fs.inotify.max_user_instances=128` is exhausted on this host (`ulimit -n` is fine at 524288). Environmental resource limit, unrelated to the feature.
- **Targeted lit** `clang/test/Parser`: 411 passed, 1 unsupported, 1 expectedly-failed, 0 unexpected. Green.

## Deviations from the plan / design
Two pre-existing tree facts contradict the design doc / step files. Both logged in `ops/DEVIATIONS.md` (rows DEV-01, DEV-02). Summary:
- **DEV-01 — `Options.td` moved.** Design §6.5 and step S01 say `clang/include/clang/Driver/Options.td`. The real path is **`clang/include/clang/Options/Options.td`**.
- **DEV-02 — `LANGOPT` is now 5-arg.** Step S01 shows `LANGOPT(Backtick, 1, 0, "...")` (4 args). The real macro is `LANGOPT(Name, Bits, Default, Compatibility, Description)`, e.g. `LANGOPT(C99, 1, 0, NotCompatible, "C99")`. The 4-arg form will not compile.

## Discoveries affecting later steps
- **Unit-test layout (affects S02/S03/S11).** Modern Clang folds most unittests into ONE binary **`AllClangUnitTests`** (via `add_clang_unittest`). There are **no** standalone `LexTests`, `ParseTests`, `ASTTests`, `ToolingTests` executables — those names from the plan don't exist as ninja targets. To run a suite:
  ```bash
  ninja -C "$B" AllClangUnitTests
  "$B"/tools/clang/unittests/AllClangUnitTests --gtest_filter='Lex*'    # or Parse*, AST*, etc.
  ```
  Still **distinct** executables (via `add_distinct_clang_unittest`): `SemaTests`, `FormatTests` (← S10 confirmed), `BasicTests`. `check-clang-unit` runs everything; `check-clang` includes the unit tests.
- Build is ~3250 edges for `clang` alone; `check-clang`'s first invocation also builds FileCheck/lit tooling + clang-tools-extra test deps before running (~3 min total here).

## Forward notes for the NEXT step (S01 — feature flag `-fbacktick`)
Read `ops/steps/01-feature-flag.md`. Concrete pointers (verified in this tree):
- **LangOpt:** add to `clang/include/clang/Basic/LangOptions.def` using the **5-arg** form (DEV-02):
  `LANGOPT(Backtick, 1, 0, NotCompatible, "backtick operator and identifier escaping")`.
  `NotCompatible` is the right compatibility kind (the flag changes the language and must not be silently merged across modules); copy what nearby language-feature LANGOPTs use if unsure.
- **Driver/CC1 flag:** edit **`clang/include/clang/Options/Options.td`** (DEV-01, NOT `…/Driver/Options.td`). Model the option on an existing marshalled `-f` LangOpts flag — e.g. search `MarshallingInfoFlag<LangOpts<"SinglePrecisionConstants">>` (around line 1115) or `LangOpts<"...">` defvars (line 628+). Give it `MarshallingInfoFlag<LangOpts<"Backtick">>` and put it in the `CC1Option` group.
- **CompilerInvocation wiring is mostly automatic:** with `MarshallingInfoFlag<LangOpts<"Backtick">>`, the parse/generate code in `CompilerInvocation.cpp` is table-generated — step 3 likely needs no hand-written code. Verify `-fbacktick … -###` forwards `-fbacktick` to `-cc1` and that `getLangOpts().Backtick` is set.
- **Tests:** add a `clang/test/Driver/` lit test asserting the flag is accepted and forwarded. The exact LangOpt name **`Backtick`** and spelling **`-fbacktick`/`-fno-backtick`** are what S02+ will gate on (`getLangOpts().Backtick`) — keep them exactly these.
- **Gate:** `clang -fbacktick -x c++ -fsyntax-only /dev/null` succeeds; `-###` shows forwarding; new Driver lit test passes; `ninja -C "$B" check-clang` stays at the 5-failure baseline above (no new failures).

## Open risks / TODOs
- The stray `/home/sdowney/src/.clang-format` will keep failing the ObjC dump-config test and may perturb S10's clang-format tests — the S10 agent should run clang-format with an explicit `-style=` / isolate from that ancestor config rather than "fix" the test.
- `inotify` DirectoryWatcher failures are host state; they may come and go. Always diff against this 5-failure baseline, not zero.
- One flag for both features (infix + escape) per S01 guidance; revisit at S07.
