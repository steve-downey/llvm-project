# Handoff — G03 Infix parse + desugar

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ 852f6e1ad54
- **Date / agent:** 2026-06-28

## What changed

- `gcc/cp/parser.h`: added `bool backtick_is_operator_p;` field to `cp_parser`
  after `greater_than_is_operator_p` (line ~292+7).
- `gcc/cp/parser.cc`:
  - Added `RT_CLOSE_BACKTICK` to `enum required_token` (after `RT_CLOSE_SPLICE`)
  - Added `case RT_CLOSE_BACKTICK: return CPP_BACKTICK;` in `get_required_cpp_ttype`
  - Added `case RT_CLOSE_BACKTICK: gmsgid = G_("expected %<\`%>");` in
    `cp_parser_required_error`
  - Initialized `parser->backtick_is_operator_p = true;` in `cp_parser_new`
  - Added save/restore of `backtick_is_operator_p = true` in six paren/bracket
    contexts (primary expression paren, subscript `[`, cast expression paren,
    argument-list paren (both exit paths), noexcept paren, decltype paren) —
    mirroring `greater_than_is_operator_p` as the design spec says
  - Added backtick infix handler in `cp_parser_binary_expression` loop, BEFORE
    the `new_prec <= current.prec` check (since CPP_BACKTICK is not in
    `binops_by_token` and would otherwise be PREC_NOT_OPERATOR → break)
- `gcc/testsuite/g++.dg/backtick/infix-basic.C`: new compile+tree-dump test;
  verifies `a \`add\` b → add(a,b)`, `a \`add\` b \`mul\` c → mul(add(a,b),c)`,
  unary prefix operands
- `gcc/testsuite/g++.dg/backtick/infix-dump.C`: new compile+tree-dump test;
  verifies single infix desugars to `f(a, b)` CALL_EXPR

## Verification evidence

```
# Build
cd /home/sdowney/bld/gcc/gcc-backtick-build && make -j18 all-gcc
→ success; -fself-test: 7664580 pass(es)

# Backtick tests (all variants)
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 36 expected passes, 0 failures

# Parse regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

Tree dump confirmed:
- `r1 = add (a, b)` — basic desugaring ✓
- `r2 = mul (add (a, b), c)` — left-associativity ✓
- `r3 = add (-NON_LVALUE_EXPR <a>, -NON_LVALUE_EXPR <b>)` — Option A unary prefix ✓

## Deviations from the plan / design

1. **`current.lhs != error_mark_node` guard added:** The step file and handoff
   template assumed `cp_parser_error_occurred` would prevent the backtick loop
   from firing after a failed LHS. In fact, `cp_parser_error_occurred` only
   returns TRUE during tentative parsing; in normal (non-tentative) contexts it
   always returns false. Without the guard, a bare `` ` `` in expression context
   (as in `lex-token.C`) triggered the infix handler with an `error_mark_node`
   LHS, producing a cascade of spurious errors. The fix is the additional
   `current.lhs != error_mark_node` predicate in the backtick if-condition.

2. **`dg-do run` changed to `dg-do compile` with tree-dump scans:** The GCC
   dev environment lacks a usable libstdc++/crtbegin for linking, so `dg-do run`
   tests fail with linker errors. Runtime correctness is verified indirectly via
   tree-dump scans confirming the correct `finish_call_expr` structure.

3. **`saved_backtick_is_operator_p_cast` and `_noexcept` and `_decltype` names:**
   These extra-unique local variable names were needed because those code blocks
   don't use a new inner scope, so the usual `saved_backtick_is_operator_p` name
   would collide in some cases. Workaround: use longer unique names per context.

## Discoveries affecting later steps

### backtick_is_operator_p flag
- Field added at the end of the bool cluster in `cp_parser`, after
  `greater_than_is_operator_p` (now at line ~300 in parser.h).
- Initialized `true` in `cp_parser_new` (line ~4726 after patch).
- Set to `false` inside the backtick slot parsing, restored after.
- Set to `true` in all paren/bracket contexts (6 sites mirroring
  `greater_than_is_operator_p`).

### Hook location in cp_parser_binary_expression
- The hook lives BEFORE the `new_prec <= current.prec` check (now at ~line 11716).
- Token is re-peeked at the end of the hook with `token = cp_lexer_peek_token`
  and then `continue` — the loop top re-reads the token so this is correct.
- The `continue` is inside the `for (;;)` loop body — left-assoc is handled
  automatically by looping back.

### Token form in tree dump
- Unary negation of lvalue variables prints as `-NON_LVALUE_EXPR <varname>` in
  `fdump-tree-original`, NOT as `-varname`. Test patterns must account for this.

### releasing_vec usage
- `releasing_vec args; vec_safe_push(args, (tree)lhs); vec_safe_push(args, (tree)rhs);`
  is the correct pattern for building the arg vector.
- `&args` gives `vec<tree, va_gc> **` via `releasing_vec::operator&`.
- `cp_expr` implicitly converts to `tree` via `operator tree()`.

### D3 parenthesised nesting
- Works for `x \`(f \`g\` h)\` y` because the paren handler restores
  `backtick_is_operator_p = true`.
- `x \`f \`g\` h\` y` correctly fails: with `backtick_is_operator_p=false` in
  the slot, the inner backtick has `PREC_NOT_OPERATOR` and the slot parse ends
  at `f`; then `cp_parser_require(CPP_BACKTICK, RT_CLOSE_BACKTICK)` finds `g`,
  not `` ` ``, and reports the error. (G04 will add better diagnostics.)

## Forward notes for the NEXT step (G04 — Diagnostics + nested-paren rule)

1. **Empty slot ` `` `:** After G03, an empty slot between two consecutive
   backticks would be parsed as: consume open `` ` ``, call
   `cp_parser_assignment_expression` with `backtick_is_operator_p = false`, see
   the closing `` ` `` (which is now PREC_NOT_OPERATOR and not an operator), so
   `cp_parser_cast_expression` sees `` ` `` → error "expected primary-expression
   before '`' token". Then `cp_parser_require(CPP_BACKTICK, RT_CLOSE_BACKTICK)`
   succeeds on the closing `` ` ``. A test for ` `` ` would have 1 error now.
   G04 should add a specific check: if the slot is `error_mark_node` AND the
   token was CPP_BACKTICK, emit "expected expression between '`' and '`'".

2. **Unterminated backtick:** Save the opening `` ` `` token's location in a
   `location_t open_loc` before consuming the open. Then, if
   `cp_parser_require(CPP_BACKTICK, RT_CLOSE_BACKTICK)` fails, pass `open_loc`
   as the `matching_location` argument to note the matching open.

3. **Bare nested backtick:** The `cp_parser_assignment_expression` call with
   `backtick_is_operator_p = false` will naturally terminate the slot at the
   first `` ` ``. The error will come from `cp_parser_require` finding the wrong
   content. G04 should detect this case and add an `inform` suggesting
   parenthesisation.

4. **location_t for open backtick:** Save `token->location` before the
   `cp_lexer_consume_token` call at the start of the backtick handler.

5. **Where to add the checks:** All diagnostic improvements are in the backtick
   infix handler block in `cp_parser_binary_expression` (now at line ~11716).
   The block is self-contained; G04 only needs to touch that block.

6. **Test pattern:** Tests should use `dg-do compile` (not `dg-do run`) since
   linking is unavailable in this environment.

## Open risks / TODOs

- G04 tests need to handle `NON_LVALUE_EXPR` in scan patterns.
- The `saved_backtick_is_operator_p_cast` etc. naming is ugly; could be cleaned
  up with inner blocks `{ }` in future.
- No test for `(f \`g\` h)` as a slot expression yet — G04 or G05 should add
  this to verify D3 nested parens work correctly.
