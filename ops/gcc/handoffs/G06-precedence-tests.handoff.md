# Handoff — G06 Precedence/associativity tests

- **Status:** DONE (gate passed, with deviation DEV-G06a — see below)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ abacf28ecda
- **Date / agent:** 2026-06-28

## What changed

- `gcc/cp/parser.cc`: added RHS-lookahead backtick handler (45 lines) inside
  `cp_parser_binary_expression`, between the `rhs = cp_parser_simple_cast_expression()`
  call and the "Get another operator token" comment.  The handler is a while-loop
  that processes any backtick(s) on the just-parsed RHS before the enclosing
  binary operation is built.  Without this fix `a * b \`f\` c` yielded `f(a*b, c)`
  (backtick had lowest effective binary precedence); with it the result is `a * f(b,c)`,
  matching §4 Option A.

- `gcc/testsuite/g++.dg/backtick/infix-precedence.C`: new compile+tree-dump test
  confirming four precedence interactions:
  1. **Multiplicative**: `a * b \`f\` c` → `f(b, c) * a` (GCC canonical MULT form)
  2. **Member access**: `s1.x \`f\` s2.x` → `f(s1.x, s2.x)` ✓
  3. **Ternary**: `p ? q : r \`f\` d` → GCC normalises to `p == 0 ? f(r,d) : q`
  4. **Left-assoc** (cross-reference infix-basic.C): `a \`f\` b \`g\` c` → `g(f(a,b),c)` ✓

## Verification evidence

```
# Targeted test
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/infix-precedence.C"
→ 15 expected passes, 0 failures  (+15 from infix-precedence.C across 3 std variants)

# Full backtick suite
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 64 expected passes, 0 failures  (was 49 before G06; +15 from infix-precedence.C)

# Parse regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

**DEV-G06a** (logged in `ops/gcc/DEVIATIONS.md`): G06 is supposed to be test-only,
but G03's backtick hook gave backtick the *lowest* effective binary precedence
(it fired on `current.lhs` only after all standard binary ops had been folded in).
A code fix was required before the §4 tests could pass.  The fix (RHS-lookahead
while-loop) is the smallest change that correctly implements §4 Option A.
Cross-compiler note: Clang had correct behavior from S03; GCC needed the fix.

**DEV-G06b** (logged in `ops/gcc/DEVIATIONS.md`): GCC canonicalises commutative
`MULT_EXPR` with the more-complex subexpression on the left.  `a * f(b,c)` is
stored and printed as `f(b,c) * a`.  Test pattern adjusted accordingly.

**DEV-G06c** (logged in `ops/gcc/DEVIATIONS.md`): GCC normalises ternary
`p != 0 ? q : f(r,d)` to `p == 0 ? f(r,d) : q` at the GENERIC tree level.
Test pattern uses `"p == 0 \\? f \\(r, d\\) : q"` (note escaped `\\?` — unescaped
`?` would be treated as ERE quantifier by dejagnu's `scan-tree-dump`).

## Discoveries affecting later steps

### RHS-lookahead handler location
- Inserted at `gcc/cp/parser.cc` between `rhs = cp_parser_simple_cast_expression(parser)`
  and the `/* Get another operator token */` comment (around the old line 11824).
- It is a scoped block `{ cp_token *bt_tok = ...; while (...) { ... } }` so that
  the local `bt_tok` variable does not leak into the surrounding function scope.
- The while-loop also fires on `goto get_rhs` paths (higher-precedence binary ops
  parsed recursively via the stack mechanism), which is correct: any backtick
  immediately following a binary op's RHS gets highest priority.

### ERE escaping in dg-final scan-tree-dump patterns
- `?` is an ERE quantifier.  Inside a dg-final pattern, `?` must be `\\?` to
  produce the ERE `\?` (literal `?`).  Similarly `*` → `\\*`, `(` → `\\(`, `)` → `\\)`,
  `.` → `\\.`.  Bare `?` in a pattern will silently mis-match.
- The ternary test pattern was `"p == 0 \\? f \\(r, d\\) : q"` — failing to escape
  `?` was the only error in the first attempt.

### GCC MULT_EXPR canonical order
- For `int` multiplication with a function-call operand and a variable operand,
  GCC's `fold_build2` places the call on the left: `f(b,c) * a` not `a * f(b,c)`.
- Write patterns for multiplication tests as `"call_expr \\* var"` not `"var \\* call_expr"`.
- This is commutative so semantics are unaffected; it is purely a dump-readability
  detail.

### All four §4 behaviors confirmed cross-compiler
- `-x \`f\` -y → f(-x,-y)`: already in infix-basic.C (G03); GCC ✓, Clang ✓
- `a * b \`f\` c → a * f(b,c)`: GCC now ✓ (after G06 fix), Clang ✓
- `a.b \`f\` c.d → f(a.b, c.d)`: GCC ✓, Clang ✓
- `a ? b : c \`f\` d → a ? b : f(c,d)`: GCC ✓ (normalised form), Clang ✓
- `a \`f\` b \`g\` c → g(f(a,b),c)`: GCC ✓, Clang ✓

## Forward notes for the NEXT step (G07 — Keyword-escaped identifiers)

G07 adds `` `kw` `` as an identifier escape in name positions.  Key points from
reading the step file and the Clang S07 handoff:

1. **Entry points to hook** in `gcc/cp/parser.cc`:
   - `cp_parser_unqualified_id` or `cp_parser_id_expression` — primary name position
   - After `.` / `->` in `cp_parser_postfix_expression` — member access
   - After `::` in qualified-name parsing
   - Declarator-id position (likely covered by the `cp_parser_unqualified_id` hook)
   - Do NOT touch the binary-expression operator loop — backtick there stays infix

2. **Keyword-only policy**: Accept only actual keywords inside the escape.  Use
   `token->type >= CPP_KEYWORD` or `cpp_token_is_keyword(token)` to check.
   Record the decision in DEVIATIONS.md. Clang S07 also adopted keyword-only.

3. **Identifier synthesis**: After consuming `` `kw` ``, get the keyword's spelling
   with `cpp_token_as_text(reader, token)` or `identifier_to_locale(keyword_name)`,
   then look up or create an IDENTIFIER_NODE via `get_identifier(spelling)`.  This
   gives a normal identifier that lookup/mangling/ABI treat as ordinary.

4. **D3 interaction**: The backtick operator inside a D3-escaped slot still works
   because `backtick_is_operator_p` is restored to `true` inside parens.  The
   sequence `` x `(`new`)` y `` (D3 paren around escaped callee) should work:
   paren restores the flag, inner `` `new` `` is in a primary-expression position
   (escape), yields identifier `new`, then `` y `` is the RHS.  Add a test case.

5. **backtick_is_operator_p flag interaction**: The escape handler in name
   positions must NOT set `backtick_is_operator_p = false`.  That flag is only
   cleared inside the operator slot.  In a name position, a leading backtick
   is unambiguously an escape and there is no operator/escape ambiguity.

6. **Test**: Confirm `void `new`();` declares a function named `new`; that
   `` `new`(a,b); `` calls it; that `obj.`delete`()` works as member access.
   Infix regression: re-run the full `g++.dg/backtick/*.C` suite.

## Open risks / TODOs

- DEV-G05 (ADL limitation) still open — not in scope for G07.
- DEV-G06a demonstrates that G03's original hook placement was subtly wrong;
  future reviewers should note the two-handler structure (top-of-loop LHS handler
  + after-rhs-parse RHS-lookahead handler) and why both are needed.
