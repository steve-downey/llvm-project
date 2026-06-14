# S04 — Diagnostics and the nested-paren rule

**Goal.** Clear errors for the malformed cases; enforce D3.

**Depends on:** S03.
**Design refs:** §3 D3; §6 step 6.

## Do
1. Empty operator slot (` `` `): diagnose "expected expression between
   backticks".
2. Unterminated backtick (no closing `` ` `` before a statement/expression
   boundary): diagnose with the open-backtick location noted.
3. Bare nested backtick in the operator slot (`x `f `g` h` y`): because the
   slot terminates at the first unparenthesized `` ` ``, this naturally
   produces a parse error — make the diagnostic explain that nested backtick
   operators must be parenthesised (`x `(f `g` h)` y`). Add new diagnostics
   to `clang/include/clang/Basic/DiagnosticParseKinds.td`.

## Build / Verify (gate)
- `-verify` lit tests with `expected-error` for each case.
- `check-clang` green.

## Done when
All three malformed cases produce intelligible, located diagnostics.

## Capture in handoff
The diagnostic IDs added (S09/S10 may reuse phrasing).
