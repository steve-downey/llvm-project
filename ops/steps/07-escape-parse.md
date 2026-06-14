# S07 — Keyword-escaped identifiers (position-based)

**Goal.** `` `kw` `` becomes an ordinary identifier in name positions,
gated by `-fbacktick`, coexisting with the infix operator from S03.

**Depends on:** S03 (must not regress infix).
**Design refs:** §12; §3 D10.

## Do
1. In the parser, where a primary-expression / unqualified-id /
   declarator-id is expected (start of `ParseCastExpression`, declarator-id
   parsing, after `.`/`->`/`::`), recognize a leading `tok::backtick`:
   consume it, read the wrapped token, expect the closing `` ` ``, and
   synthesize an `IdentifierInfo` with the wrapped token's spelling and the
   keyword bit cleared. Feed that identifier into the normal name path.
2. **Position is the disambiguator.** Do not touch
   `ParseRHSOfBinaryExpression` — a post-operand backtick stays the infix
   operator. The two never share a position (§12).
3. Recommended: accept only actual keywords inside the escape (cleanest
   disjointness); decide and record. Ordinary-identifier escapes are
   redundant.

## Build / Verify (gate)
- `void `new`();` then `` `new`(a,b); `` parses; AST shows a function named
  `new` and a call to it.
- `obj.`delete`()` parses as member access named `delete`.
- Infix still works: re-run S03/S06 tests — all green.
- `check-clang` green.

## Done when
Escapes parse in name positions and infix is unregressed.

## Capture in handoff
Where exactly you hooked each name position (list the functions), and the
keyword-only decision. S08 extends the declaration paths.

## Pitfalls
The infix slot suppresses the backtick flag, so a keyword-named callee used
infix needs D3 parens (`x `(`new`)` y`) — don't try to special-case it here;
it should already work via S03's nested-paren handling. Add a test.
