# G08 — Tentative parse / decl-vs-expr

**Goal.** Escapes work through GCC's tentative-parse disambiguation so
`T `new`;` declarations and ambiguous statements resolve correctly.

**Depends on:** G07.
**Design refs:** §12 (Costs).

## Do
1. Ensure the tentative-parsing paths (`cp_parser_parse_tentatively` /
   `cp_parser_parse_definitely`, and the simple-declaration vs
   expression-statement disambiguation) accept a backtick-escaped name as a
   declarator-id.
2. Confirm the existing "prefer declaration" resolution still holds.

## Build / Verify (gate)
- `T `new`;` declares a variable named `new`; an escaped-name call statement
  parses as an expression-statement.
- Re-run G07 tests; baseline green.

## Done when
Escaped names survive tentative parsing in both contexts.

## Capture in handoff
Any disambiguation surprise; compare to Clang S08 (cross-compiler note).
