# G04 — Diagnostics + nested-paren rule

**Goal.** Intelligible errors for malformed infix; enforce D3.

**Depends on:** G03.
**Design refs:** §3 D3; §8.

## Do
1. Empty slot (` `` `): "expected expression between '`' and '`'".
2. Unterminated backtick before a statement boundary: error noting the open
   '`' location.
3. Bare nested backtick (`x `f `g` h` y`): the slot terminates at the first
   unparenthesized '`', producing an error; phrase it to suggest parenthesising
   (`x `(f `g` h)` y`).
Use `error_at`/`inform` with the relevant `location_t`s.

## Build / Verify (gate)
- `g++.dg` tests with `dg-error` for each case. Baseline green.

## Done when
Each malformed case errors clearly.

## Capture in handoff
The message wording (keep consistent with G02's stray-character text).
