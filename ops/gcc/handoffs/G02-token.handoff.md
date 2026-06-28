# Handoff — G02 libcpp CPP_BACKTICK token

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ 6ef769884d5
- **Date / agent:** 2026-06-28

## What changed

- `libcpp/include/cpplib.h`: added `OP(BACKTICK, "\`")` to TTYPE_TABLE after
  `OP(ATSIGN, "@")` (line ~118); updated `CPP_LAST_PUNCTUATOR = CPP_BACKTICK`
  (was `CPP_ATSIGN`); added `unsigned char backtick_is_operator;` to
  `struct cpp_options` after `dollars_in_ident` (line ~463).
- `libcpp/lex.cc`: added `case '`':` before `default:` (line ~4381) in
  `_cpp_lex_direct`; sets `result->type = CPP_BACKTICK` when
  `CPP_OPTION(pfile, backtick_is_operator)` is on; falls through via
  `/* FALLTHROUGH */` to `default:` (CPP_OTHER path) when option is off.
- `gcc/c-family/c-opts.cc`: added `case OPT_fbacktick: cpp_opts->backtick_is_operator = value; break;` after `OPT_fdollars_in_identifiers` handler (line ~494).
- `gcc/testsuite/g++.dg/backtick/lex-stray.C`: new test; without `-fbacktick`,
  `` ` `` produces "stray" error.
- `gcc/testsuite/g++.dg/backtick/lex-token.C`: new test; with `-fbacktick`,
  `` ` `` produces `CPP_BACKTICK` token (parse error, but NOT "stray").

## Verification evidence

```
# Build
cd /home/sdowney/bld/gcc/gcc-backtick-build && make -j18 all-gcc
→ success; -fself-test: 7664580 pass(es) in 0.353 seconds

# Targeted backtick lex tests
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/lex*.C"
→ 15 expected passes (2 tests × 3 std dialects × ~2-3 checks each)

# Parse regression (unchanged from G01 baseline)
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

1. **`CPP_LAST_PUNCTUATOR` updated to `CPP_BACKTICK`:** The step file did not
   mention this, but since `CPP_LAST_PUNCTUATOR` is used in `gcc/cp/parser.cc`
   line 7403 as an upper bound for punctuator tokens, and `CPP_BACKTICK` is a
   punctuator, it must be updated. `CPP_BACKTICK` comes after `CPP_ATSIGN` in
   the table, so the new last punctuator is `CPP_BACKTICK`.

2. **No C-frontend test created:** The step mentioned a C test confirming the C
   front end is unchanged. Since `-fbacktick` is a C++-only flag (the `C++`
   property in `c.opt` prevents it from being wired to `cpp_opts->backtick_is_operator`
   for C files), and the `OPT_fbacktick` handler is only in the C++ opts path,
   C files will always have `backtick_is_operator = 0`. The `lex-stray.C` test
   (which runs without `-fbacktick`) effectively covers the "off" path; a
   separate C test was not added.

## Discoveries affecting later steps

### Token enum values
- `CPP_BACKTICK` is now the last entry in the OP punctuator group in TTYPE_TABLE,
  immediately after `CPP_ATSIGN`.
- `CPP_LAST_PUNCTUATOR = CPP_BACKTICK` (was `CPP_ATSIGN`).
- `N_TTYPES` increases by 1; the `binops_by_token[N_CP_TTYPES]` array in
  `gcc/cp/parser.cc` is sized by `N_CP_TTYPES` (see below).

### N_CP_TTYPES vs N_TTYPES
- `binops_by_token` in `gcc/cp/parser.cc` line 2453 is declared as
  `static cp_parser_binary_operations_map_node binops_by_token[N_CP_TTYPES];`
- `N_CP_TTYPES` is defined as `N_TTYPES` (or equivalent) — verify this in
  `gcc/cp/parser.h` or `cpplib.h`. Since `CPP_BACKTICK` is now part of TTYPE_TABLE
  and `N_TTYPES` is the count after the table, `binops_by_token[CPP_BACKTICK]`
  is a valid slot. G03 can write to it (or leave it at 0/PREC_NOT_OPERATOR and
  handle CPP_BACKTICK specially before the `TOKEN_PRECEDENCE` lookup).

### Error message for CPP_BACKTICK token (pre-G03)
With `-fbacktick`, a bare `` ` `` in a function body gives:
```
error: expected primary-expression before '`' token
```
This is a GCC C++ parser generic error for an unrecognized token in expression
context, not "stray". Confirms the lexer is working.

### stray error message text
```
error: stray '`' in program
```
Emitted from `gcc/c-family/c-lex.cc:717` via `CPP_OTHER` path when
`backtick_is_operator` is off.

## Forward notes for the NEXT step (G03 — infix parse + desugar)

1. **Files to touch:**
   - `gcc/cp/parser.h` — add `bool backtick_is_operator_p;` near
     `greater_than_is_operator_p` (line ~292).
   - `gcc/cp/parser.cc` — initialization (find where `greater_than_is_operator_p`
     is set to `true` in `cp_parser_new`, around line 4724; do the same for
     `backtick_is_operator_p`).
   - `gcc/cp/parser.cc` — the main hook in `cp_parser_binary_expression`
     (function definition at line 11638; the `for (;;)` loop starts at 11664).

2. **Hook site in `cp_parser_binary_expression`:**
   After line 11681 (`new_prec = TOKEN_PRECEDENCE (token);`), add:
   ```c
   /* Backtick infix operator: highest binary precedence.  */
   if (flag_backtick
       && token->type == CPP_BACKTICK
       && parser->backtick_is_operator_p)
     {
       /* ... consume open backtick, parse slot, expect close, parse RHS,
          call finish_call_expr ... */
     }
   ```
   The check on `new_prec` loop control (`if (new_prec <= current.prec)`) will
   give `PREC_NOT_OPERATOR` for `CPP_BACKTICK` (it's not in `binops[]`), which
   would cause it to exit the loop. So the special-case check MUST come BEFORE
   the precedence comparison (or temporarily set `new_prec` to a sentinel).

3. **Backtick slot parsing pattern (model on ternary `cp_parser_question_colon_clause`):**
   ```c
   /* Consume open backtick.  */
   cp_lexer_consume_token (parser->lexer);
   /* Parse operator slot as assignment-expression.  */
   bool saved_backtick_is_operator_p = parser->backtick_is_operator_p;
   parser->backtick_is_operator_p = false;
   tree slot = cp_parser_assignment_expression (parser);
   parser->backtick_is_operator_p = saved_backtick_is_operator_p;
   /* Expect closing backtick.  */
   cp_parser_require (parser, CPP_BACKTICK, RT_CLOSE_BRACE); /* or a new RT_ */
   /* Parse RHS as cast-expression (Option A: highest binary, looser than unary). */
   cp_expr rhs = cp_parser_simple_cast_expression (parser);
   /* Build call: slot(lhs, rhs) with ADL.  */
   vec<tree, va_gc> *args;
   vec_alloc (args, 2);
   args->quick_push (current.lhs);
   args->quick_push (rhs);
   current.lhs = finish_call_expr (slot, &args, /*disallow_virtual=*/false,
                                   /*koenig_p=*/true, tf_warning_or_error);
   /* Left-associativity: loop back to get next token.  */
   continue;
   ```

4. **`cp_parser_require` for `CPP_BACKTICK`:** The `RT_` enum values are in
   `gcc/cp/parser.h` near `RT_CLOSE_BRACE`. Add `RT_CLOSE_BACKTICK` or reuse
   an existing generic one. Alternatively, use `cp_parser_require (parser,
   CPP_BACKTICK, RT_CLOSE_BRACE)` with a mismatch message and fix the message
   if it looks wrong.

5. **`N_CP_TTYPES`:** Verify that `binops_by_token[N_CP_TTYPES]` is large enough
   to index `CPP_BACKTICK`. Search for `N_CP_TTYPES` in `parser.cc` or `parser.h`.

6. **`cp_parser_simple_cast_expression`:** The RHS should be parsed with
   `cp_parser_simple_cast_expression` (not `cp_parser_binary_expression`). This
   gives cast-expression precedence (Option A) — operands are cast-expressions.

7. **Test:** After G03, `g++.dg/backtick/lex-token.C` needs updating (the
   `dg-error "expected primary-expression"` will be replaced by a successful
   parse). G05/G06 add the full semantic and precedence tests.

8. **Commit:** `[backtick][gcc] G03: infix parse + desugar`

## Open risks / TODOs

- The `RT_CLOSE_BACKTICK` token-required enum may need to be added; check what
  diagnostic is emitted by `cp_parser_require` for a custom RT value.
- The `backtick_is_operator_p` flag must be restored inside nested parens/brackets
  (D3's parenthesised nesting rule). The save/restore in the slot parsing handles
  the operator slot; `(` and `[` parsing in `cp_parser_primary_expression` and
  `cp_parser_postfix_expression` already preserve the flag automatically since
  they enter a new context.
