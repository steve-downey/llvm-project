# S08 — Tentative-parse / declaration-vs-expression integration

**Goal.** Make the escape work through Clang's disambiguation machinery so
`T `kw`;` declarations and ambiguous statements resolve correctly.

**Depends on:** S07.
**Design refs:** §12 (Costs).

## Do
1. Teach the tentative-parsing paths (`TryParseDeclarator` and the
   declaration-vs-expression disambiguation in `ParseStatement` /
   `isCXXDeclarationStatement`) to recognize a backtick-escaped name as a
   valid declarator-id.
2. Verify the existing "prefer declaration" rule still governs genuinely
   ambiguous statements.

## Build / Verify (gate)
- `T `class` x;` declares `x` of type `T` (escape in type? no — escape is
  the declared name: re-read; use `T `new`;` declaring a variable named
  `new`).
- A statement that is a declaration with an escaped name is parsed as a
  declaration; an expression-statement using an escaped-name call is parsed
  as an expression.
- Re-run S07 tests; `check-clang` green.

## Done when
Escaped names survive tentative parsing in both declaration and expression
contexts.

## Capture in handoff
Any disambiguation edge that surprised you (candidate paper material and a
DEVIATIONS row).
