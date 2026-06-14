# Backtick Operator — GCC sub-plan

The second independent implementation. Mirrors the Clang sequence so the
WG21 paper can show two implementations and cross-compiler agreement. The
worktree + build baseline is established by the top-level **S12**; start here
at **G01**.

## Reuse
- Protocol: `ops/AGENT_PROTOCOL.md` (same one-step-per-agent loop).
- Handoff template: `ops/HANDOFF_TEMPLATE.md`; handoffs → `ops/gcc/handoffs/`.
- Deviations → `ops/gcc/DEVIATIONS.md` (record cross-compiler differences too).
- Architecture: `docs/backtick-operator-design.md` (§8 is GCC-specific).

## Ground rules
Same as the Clang plan: one step per agent; no green no check; all new
behavior gated behind the G01 flag (default off, other front ends
unaffected); minimal diffs; one commit per step `[backtick][gcc] GNN: <title>`.

## Build & test (pinned by S12)
Use S12's handoff values: the `gcc-backtick` worktree, the out-of-tree dev
build (`--disable-bootstrap --enable-languages=c,c++`), and the `g++.dg` gate
(`make -C gcc check-c++ RUNTESTFLAGS="dg.exp=..."`).

## Checklist
### GCC infix operator (mirrors Clang Phase A)
- [ ] **G01** Flag in `c.opt` — `ops/gcc/steps/G01-flag.md` (dep: S12)
- [ ] **G02** libcpp `CPP_BACKTICK` token — `ops/gcc/steps/G02-token.md` (dep: G01)
- [ ] **G03** Infix parse + desugar (`finish_call_expr`) — `ops/gcc/steps/G03-infix.md` (dep: G02)
- [ ] **G04** Diagnostics + nested-paren rule — `ops/gcc/steps/G04-diagnostics.md` (dep: G03)
- [ ] **G05** Semantics tests — `ops/gcc/steps/G05-semantics-tests.md` (dep: G03)
- [ ] **G06** Precedence/associativity tests — `ops/gcc/steps/G06-precedence-tests.md` (dep: G03)
### GCC keyword-escaped identifiers (mirrors Clang Phase B)
- [ ] **G07** Keyword-escape, position-based — `ops/gcc/steps/G07-escape.md` (dep: G03)
- [ ] **G08** Tentative-parse / decl-vs-expr — `ops/gcc/steps/G08-escape-tentative.md` (dep: G07)
- [ ] **G09** Escape tests + mangling — `ops/gcc/steps/G09-escape-tests.md` (dep: G08)

(No GCC analog of clang-format. Source-fidelity dumping is out of scope for
the GCC track; desugar-only is sufficient for the paper's second data point.)

## Status log
| Step | Date | Branch | Commit | Gate | Handoff |
|------|------|--------|--------|------|---------|
|      |      |        |        |      |         |
