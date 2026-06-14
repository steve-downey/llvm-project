# G06 — Precedence & associativity tests

**Goal.** Pin §4 Option A behavior in GCC; confirm it matches Clang S06.

**Depends on:** G03.
**Design refs:** §2; §4 Option A.

## Do — `g++.dg` tests (`-fdump-tree-original` / run-tests) asserting
- `-x `f` -y` → `f(-x, -y)` (symmetric, D2).
- `a * b `f` c` → `a * f(b, c)`.
- `a.b `f` c.d` → `f(a.b, c.d)`.
- `a ? b : c `f` d` → `a ? b : f(c, d)`.
- `a `f` b `g` c` → `g(f(a,b), c)`.

## Verify (gate)
All pass; baseline green.

## Capture in handoff
Confirm `-x `f` -y` → `f(-x,-y)`; any GCC/Clang disagreement on a row is a
high-priority cross-compiler deviation.
