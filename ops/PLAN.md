# Backtick Operator — Operational Plan (master)

This is the entry point. Read it fully before doing anything. The
architecture it implements is proven on paper in
`docs/backtick-operator-design.md`; this plan exists to *test* that it
holds in a real compiler. Every step is gated on passing builds and tests,
and every place reality contradicts the design is logged so the paper
author can fold it back in.

## How to use this plan
1. Read `ops/AGENT_PROTOCOL.md` — it defines exactly how to execute one step.
2. Find the first unchecked step below whose dependencies are all checked.
3. Execute only that step. Then stop.

## Ground rules
- **One step per agent.** Never start the next step.
- **No green, no check.** A step's box is ticked only after its
  verification gate passes. If it can't pass, leave it unchecked, write a
  BLOCKED handoff, stop.
- **Gated and regression-free.** All new behavior sits behind `-fbacktick`.
  A default build (flag off) must behave exactly as upstream. `check-clang`
  must stay green.
- **Minimal diffs.** Touch only what the step names.
- **One commit per step**, message `[backtick] SNN: <title>`.
- **Feedback loop.** If reality differs from `docs/backtick-operator-design.md`,
  append a row to `ops/DEVIATIONS.md` and note it in your handoff. The paper
  author reconciles deviations into the design's §3 decisions log.

## Build & test (S00 pins these; values below reflect the known-good setup)
The maintainer's main build lives at `/home/sdowney/src/llvm/build-main`
(source/git root `/home/sdowney/src/llvm/main`). It builds today. Feature
work happens in a **separate worktree + build dir** so the main build is
never disturbed — S00 creates them. The dev build trims runtimes/bootstrap
and turns assertions **on** for fast iteration and early invariant checks:
```bash
WT=/home/sdowney/src/llvm/backtick           # worktree (branch: backtick)
B=/home/sdowney/src/llvm/build-backtick        # its own build dir
ninja -C "$B" clang                           # build the compiler
ninja -C "$B" check-clang                     # full regression gate
"$B"/bin/llvm-lit -v clang/test/...           # fast targeted gate
```

## Checklist

### Phase A — Clang infix operator (MVP, desugar-only)
- [x] **S00** Baseline build + harness orientation — `ops/steps/00-baseline.md`
- [x] **S01** Feature flag `-fbacktick` — `ops/steps/01-feature-flag.md` (dep: S00)
- [x] **S02** Lexer: backtick punctuator token — `ops/steps/02-lexer-token.md` (dep: S01)
- [ ] **S03** Parse + desugar to `CallExpr` — `ops/steps/03-infix-parse-sema.md` (dep: S02)
- [ ] **S04** Diagnostics + nested-paren rule (D3) — `ops/steps/04-infix-diagnostics.md` (dep: S03)
- [ ] **S05** Semantics test sweep — `ops/steps/05-infix-semantics-tests.md` (dep: S03)
- [ ] **S06** Precedence/associativity test sweep — `ops/steps/06-infix-precedence-tests.md` (dep: S03)

### Phase B — Clang keyword-escaped identifiers (after infix)
- [ ] **S07** Parser: keyword-escape, position-based — `ops/steps/07-escape-parse.md` (dep: S03)
- [ ] **S08** Tentative-parse / decl-vs-expr integration — `ops/steps/08-escape-tentative.md` (dep: S07)
- [ ] **S09** Escape test sweep + mangling check — `ops/steps/09-escape-tests.md` (dep: S08)

### Phase C — Tooling & source fidelity
- [ ] **S10** clang-format (both uses) — `ops/steps/10-clang-format.md` (dep: S09)
- [ ] **S11** AST wrapper for `-ast-print` fidelity — `ops/steps/11-ast-wrapper.md` (dep: S06)

### Phase D — Second implementation (GCC; full sub-plan in `ops/gcc/PLAN.md`)
- [ ] **S12** GCC baseline & orientation — `ops/steps/12-gcc-bootstrap.md` (dep: A green)
  - then GCC steps **G01–G09** in `ops/gcc/PLAN.md`

## Status log (S00 + each agent appends one line)
| Step | Agent date | Branch | Commit | Gate result | Handoff |
|------|-----------|--------|--------|-------------|---------|
| S00  | 2026-06-14 | backtick | tip of `backtick` (base a815e6f267c1) | PASS (5 env-only known-fails) | ops/handoffs/00-baseline.handoff.md |
| S01  | 2026-06-14 | backtick | 6aec144f9244 | PASS (4 env-only known-fails, 52210 total, 46442 passed) | ops/handoffs/01-feature-flag.handoff.md |
| S02  | 2026-06-26 | backtick | 66bee2be5b15 | PASS (1 env-only known-fail, 52211 total, 46446 passed) | ops/handoffs/02-lexer-token.handoff.md |
