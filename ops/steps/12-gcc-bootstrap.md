# S12 — GCC track baseline & orientation

**Goal.** Stand up the second implementation's worktree + baseline and hand
off to the GCC sub-plan (`ops/gcc/`, steps G01–G09, written below). This is
the GCC analog of S00. No feature code.

**Depends on:** Phase A green (design validated in one compiler).
**Design refs:** §8; §5 (`greater_than_is_operator_p` analog); §3 D6.

## Do
1. Locate the GCC git checkout; record root + base commit.
2. **Branch + worktree** from a stable point (same pattern as S00):
   ```bash
   cd <gcc-git-root>
   git worktree add -b backtick ../gcc-backtick HEAD
   ```
3. **Configure a dev build** out-of-tree, C/C++ only, no bootstrap:
   ```bash
   mkdir -p ../gcc-backtick-build && cd ../gcc-backtick-build
   ../gcc-backtick/configure --prefix="$PWD/install" \
     --enable-languages=c,c++ --disable-bootstrap --disable-multilib
   make -j<N>        # or 'make all-gcc' to stop at cc1plus for speed
   ```
4. **Baseline gate:** run a `g++.dg` subset and record results, e.g.
   `make -C gcc check-c++ RUNTESTFLAGS="dg.exp=*"` (narrow the `dg.exp` for
   speed). Document any known-failing tests.
5. The GCC sub-plan already exists at `ops/gcc/PLAN.md` (G01–G09). It reuses
   `ops/AGENT_PROTOCOL.md` and `ops/HANDOFF_TEMPLATE.md`; its handoffs go in
   `ops/gcc/handoffs/`, deviations in `ops/gcc/DEVIATIONS.md`.

## Verify (gate)
- Worktree on branch `backtick`; cc1plus / `g++` builds.
- A `g++.dg` subset runs green (or a documented known-failing baseline).

## Done when
The GCC worktree + baseline are up and pinned; agents can pick up G01.

## Capture in handoff
The GCC root, base commit, and exact configure/build/test commands, plus the
`g++.dg` baseline — the G-agents need the same ground truth S00 gave the Clang
agents. Track cross-compiler divergences in `ops/gcc/DEVIATIONS.md`.

## Pitfalls
- `--disable-bootstrap` is essential for iteration speed (a bootstrap rebuilds
  GCC with itself).
- libcpp changes (G02) affect all front ends — keep them gated so C is
  unaffected.
