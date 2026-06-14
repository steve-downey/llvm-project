# S00 — Branch, worktree, and baseline

**Goal.** Cut a stable branch and an isolated git worktree + build dir for
the feature work, confirm it builds and record the test baseline, and pin
exact commands for every later step. Zero feature code.

**Depends on:** nothing.
**Design refs:** none (process step).

## Known-good ground truth (maintainer-provided)
- Git root / source: `/home/sdowney/src/llvm/main`
- Main build (do NOT disturb): `/home/sdowney/src/llvm/build-main` — Release,
  assertions Off, projects `clang;clang-tools-extra`, runtimes
  `libcxx;libcxxabi;compiler-rt;libunwind`, bootstrap on, compilers
  `clang++-23`/`clang-23`, 12 compile / 1 link job. It builds today.

## Do
1. **Branch + worktree from the current stable HEAD.**
   ```bash
   cd /home/sdowney/src/llvm/main
   git rev-parse HEAD                 # RECORD this base commit in the handoff
   git worktree add -b backtick /home/sdowney/src/llvm/backtick HEAD
   ```
2. **Configure a dedicated dev build dir** for the worktree. Mirror the main
   config but with iteration-friendly changes: assertions **On**, drop
   runtimes and bootstrap (not needed for parser/Sema work), host targets
   only. Keep the maintainer's compilers and job counts.
   ```bash
   cmake -G Ninja \
     -S /home/sdowney/src/llvm/backtick/llvm \
     -B /home/sdowney/src/llvm/build-backtick \
     -DCMAKE_BUILD_TYPE=Release \
     -DLLVM_ENABLE_ASSERTIONS=On \
     -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
     -DLLVM_TARGETS_TO_BUILD=host \
     -DCMAKE_CXX_COMPILER=clang++-23 -DCMAKE_C_COMPILER=clang-23 \
     -DLLVM_PARALLEL_COMPILE_JOBS=12 -DLLVM_PARALLEL_LINK_JOBS=1
   ```
3. **Build the stage-1 compiler only:**
   `ninja -C /home/sdowney/src/llvm/build-backtick clang`
4. **Record the baseline gate.** Run
   `ninja -C /home/sdowney/src/llvm/build-backtick check-clang` and record
   pass/fail counts. The maintainer noted tests were still building/running on
   the main config; **this** dev config is what the gate uses, so its baseline
   is what matters. If anything fails on the untouched tree, list exactly which
   tests as the known-failing baseline.
5. **Pin unit-test targets** for later: confirm the real gtest target names in
   this tree (e.g. `LexTests`, `ParseTests`, `SemaTests`, `FormatTests`) and
   that `ninja -C .../build-backtick check-clang-unit` runs them.
6. Confirm the fast targeted gate:
   `/home/sdowney/src/llvm/build-backtick/bin/llvm-lit -v clang/test/Parser/`.

## Verify (gate)
- Worktree exists on branch `backtick`; the dev build produces a working
  `clang`.
- `check-clang` result recorded (green, or a documented known-failing set).
- A targeted `llvm-lit` run passes.

## Done when
Worktree + dev build are up, the baseline is recorded, and commands are pinned.

## Capture in handoff
The **base commit hash**; the worktree path; the **exact** build-dir path and
configure command that worked; the `check-clang` baseline (counts + any
known-failing tests, so later agents don't blame themselves); confirmed
unit-test target names. Every later agent uses these verbatim.

## Pitfalls
- Build the `clang` target, not default `all` or the bootstrap — those pull in
  runtimes and a second stage and are far slower.
- Any failure that also shows up in the maintainer's in-progress main-build run
  is pre-existing — capture it as baseline, don't treat it as your regression.
- Assertions On is a deliberate change from the main build; it's worth it for
  catching parser/Sema invariant violations early.
