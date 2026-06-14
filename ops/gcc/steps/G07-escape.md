# G07 — Keyword-escaped identifiers (position-based)

**Goal.** `` `kw` `` becomes an identifier in name positions; coexists with
infix via position; gated.

**Depends on:** G03.
**Design refs:** §12; §3 D10.

## Do
1. In `gcc/cp/parser.cc`, where a name is expected — `cp_parser_unqualified_id`
   / `cp_parser_id_expression`, declarator-id parsing, and after `.`/`->`/`::`
   in `cp_parser_postfix_expression` — recognize a leading `CPP_BACKTICK`:
   consume it, read the wrapped token, expect closing '`', and build an
   `IDENTIFIER_NODE` from its spelling treated as an ordinary identifier (clear
   keyword-ness). Feed it into the normal name path.
2. Do **not** touch the binary-expression operator loop — a post-operand
   backtick stays infix (§12 position rule).
3. Recommended: accept only actual keywords inside the escape; record the
   decision (match Clang S07).

## Build / Verify (gate)
- `void `new`();` then `` `new`(a,b); `` parse; the entity is named `new`.
- `obj.`delete`()` parses as member access.
- Infix unregressed: re-run G03/G06 tests green. Baseline green.

## Done when
Escapes parse in name positions; infix intact.

## Capture in handoff
The exact parser entry points hooked and the keyword-only decision. Note any
divergence from Clang S07's hook set.

## Pitfalls
Keyword-named callee used infix needs D3 parens (`x `(`new`)` y`) — should work
via G03's nested handling; add a test rather than special-casing.
