# G05 — Semantics tests

**Goal.** Prove desugaring inherits call semantics. Any failure is a real G03
bug — fix it and log a deviation.

**Depends on:** G03.
**Design refs:** §2; §8.

## Do — add `g++.dg` tests for
1. Overload resolution matches `f(x,y)`.
2. ADL finds the slot callee (koenig).
3. Dependent operands in templates instantiate (via `tsubst` of CALL_EXPR — no
   extra code expected).
4. `constexpr` usability when the callee is constexpr.
5. Codegen: a run-test producing the same result as the explicit call.

## Verify (gate)
All new tests pass; baseline green.

## Capture in handoff
Any semantic divergence from a hand-written call, and any place GCC differs
from Clang's S05 results (cross-compiler note in `ops/gcc/DEVIATIONS.md`).
