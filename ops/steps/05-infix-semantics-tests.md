# S05 — Semantics test sweep

**Goal.** Prove the desugaring inherits call semantics. Mostly tests; any
failure here is a real bug to fix in S03's code (record as a deviation).

**Depends on:** S03.
**Design refs:** §2; §6 step 7.

## Do — add lit tests covering
1. Overload resolution selects the same overload as `f(x,y)`.
2. ADL finds the operator-slot callee as it would in a call.
3. Dependent operands in templates instantiate (relies on
   `TreeTransform::TransformCallExpr` — no extra code expected).
4. `constexpr`: `x `f` y` is usable in a constant expression when `f` is.
5. CodeGen: `-emit-llvm` shows the same call as `f(x,y)`.
6. Value categories / qualified callee (`a `ns::g` b`).

## Verify (gate)
All new tests pass; `check-clang` green.

## Done when
The sweep is green. If any case needed a code change in S03's area, that's a
DEVIATIONS row.

## Capture in handoff
Any semantic surprise (e.g. a context where desugaring diverges from a hand-
written call) — high-value feedback for the paper.
