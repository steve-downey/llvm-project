# S06 — Precedence & associativity test sweep

**Goal.** Lock the grammar behavior from §4 Option A in tests.

**Depends on:** S03.
**Design refs:** §2 (precedence); §4 Option A.

## Do — add `-ast-dump`/FileCheck tests asserting
- `-x `f` -y`  → `f(-x, -y)` (symmetric; the resolved D2 behavior).
- `a * b `f` c` → `a * f(b, c)` (tighter than `*`).
- `a.b `f` c.d` → `f(a.b, c.d)`.
- `a ? b : c `f` d` → `a ? b : f(c, d)`.
- `a `f` b `g` c` → `g(f(a,b), c)` (left-assoc).

## Verify (gate)
All pass; `check-clang` green.

## Done when
Each precedence interaction is pinned by a test.

## Capture in handoff
Confirm `-x `f` -y` really yields `f(-x,-y)`; if not, it's a serious D2
deviation — log it.
