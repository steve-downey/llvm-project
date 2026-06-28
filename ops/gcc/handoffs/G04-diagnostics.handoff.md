# Handoff — G04 Diagnostics + nested-paren rule

- **Status:** DONE (gate passed, with deviation DEV-G04 — see below)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ a05a770b1bf
- **Date / agent:** 2026-06-28

## What changed

- `gcc/cp/parser.cc`:
  - `get_matching_symbol`: added `case RT_CLOSE_BACKTICK: return "\`";` so that
    when `matching_location` cannot be consolidated into the rich diagnostic,
    the fallback `inform` ("to match this '`'") fires correctly instead of
    hitting `gcc_unreachable`.
  - `cp_parser_binary_expression` (backtick handler block, ~line 11720):
    - Added `location_t open_loc = token->location;` before consuming the
      opening backtick.
    - Added empty-slot check: after consuming the open backtick, peek at the
      next token; if it is CPP_BACKTICK, emit
      `error_at(next->location, "expected expression between %<\`%> and %<\`%>")`,
      consume the second backtick, set `current.lhs = error_mark_node`, and
      `continue` — avoids cascading errors.
    - Changed `cp_parser_require(…)` → `if (!cp_parser_require(parser,
      CPP_BACKTICK, RT_CLOSE_BACKTICK, open_loc))` — passes `open_loc` so a
      missing-close emits "expected '`'" at the bad token and, if nearby-check
      fails, also `inform(open_loc, "to match this '`'")` at the open. On
      failure, sets `current.lhs = error_mark_node` and `continue` to
      suppress cascading errors.
- `gcc/testsuite/g++.dg/backtick/infix-diag.C`: new compile test:
  - `test_empty_slot`: `a \`\`; ` → `dg-error "expected expression between"`
  - `test_unterminated`: `a \`add; ` → `dg-error "expected"`
  - `test_paren_slot`: `a \`(add)\` b` compiles clean (D3 positive case —
    parens in slot are well-formed; `backtick_is_operator_p` is restored to
    true inside the paren by the existing paren-save/restore).

## Verification evidence

```
# Build
cd /home/sdowney/bld/gcc/gcc-backtick-build && make -j18 all-gcc
→ success; -fself-test: 7664580 pass(es)

# Backtick tests (all variants)
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 45 expected passes, 0 failures  (was 36 before G04; +9 from infix-diag.C)

# Parse regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

**DEV-G04** (logged in `ops/gcc/DEVIATIONS.md`): The step file says bare
nested backtick `x \`f \`g\` h\` y` "produces an error" because
`backtick_is_operator_p=false` causes the slot to terminate at the inner
backtick. This is correct — the slot DOES terminate at `f` — but the next
token is then CPP_BACKTICK (the inner open), which `cp_parser_require`
happily accepts as the close backtick. The structure is thus parsed as two
chained operators without error, yielding `h(f(x,g),y)`. This matches
Clang's DEV-04 exactly. Enforcement of D3 would require lookahead past the
apparent close backtick to determine if it is really a nested open; deferred.

## Discoveries affecting later steps

### Empty-slot detection pattern
The check `if (next->type == CPP_BACKTICK)` fires immediately after consuming
the open backtick, before the slot expression parse. This is the reliable
detection point. Post-parse detection is impossible because `error_mark_node`
from a failed primary-expression also looks like "nothing was there".

### RT_CLOSE_BACKTICK in get_matching_symbol
`get_matching_symbol` previously hit `gcc_unreachable()` for
`RT_CLOSE_BACKTICK`. Fixed by adding `case RT_CLOSE_BACKTICK: return "\`";`.
Any future step that uses `cp_parser_require(..., RT_CLOSE_BACKTICK, nonzero_loc)`
will get the proper inform without crashing.

### matching_location consolidation
`richloc.add_location_if_nearby` usually succeeds for same-line locations,
so the matching-location note is folded into the primary diagnostic rather than
emitted as a separate `inform`. Tests should match `dg-error "expected"` not
`dg-message "to match this"` for single-line unterminated cases — the inform
may or may not fire as a separate line depending on column proximity.

### DEV-G04 / D3 enforcement
Bare nested backtick is silently accepted by both compilers. If a future step
addresses D3, it needs one token of lookahead past the apparent close: after
`cp_parser_require` succeeds, peek one more token; if it is also CPP_BACKTICK
immediately, the structure was ambiguously nested. This would be a separate step.

## Forward notes for the NEXT step (G05 — Semantics tests)

G05 is test-only (no parser/sema code changes expected). Key constraints:

1. **No linker**: `dg-do run` fails (missing libstdc++/crtbegin). Use
   `dg-do compile` with `-fdump-tree-original` tree-dump scans to verify
   overload resolution, ADL, constexpr, and template instantiation.

2. **Overload resolution**: `finish_call_expr` with `koenig_p=true` resolves
   the callee by unqualified lookup + ADL at the call site. For a test like
   `1 \`f\` 2` where `f` is overloaded, the right overload is selected.
   Verify via tree dump that the correct function appears in the CALL_EXPR.

3. **ADL**: The slot is a pre-parsed expression (an `ADDR_EXPR` or name),
   not an unqualified-id at call-build time. ADL in GCC's `finish_call_expr`
   fires on unqualified-id callees. If the slot is written as an unqualified
   name (e.g., `\`f\``), ADL applies. If it is qualified (`\`ns::f\``), ADL
   does not. Test the unqualified case with a type from a namespace to confirm
   ADL fires.

4. **Templates**: The CALL_EXPR built by `finish_call_expr` is a normal GCC
   tree; `tsubst` handles it automatically at instantiation. No special code
   is needed and no deviation is expected. A simple template like
   `template<typename F> auto apply(int a, int b, F f) { return a \`f\` b; }`
   should work; verify by instantiation.

5. **constexpr**: `finish_call_expr` supports constexpr callees. Test
   `constexpr int add(int,int)` with `static_assert(1 \`add\` 2 == 3);`.
   Use `dg-error "" /* no errors expected */` or just rely on absence of
   dg-error. A compile-only test with `static_assert` is the simplest gate.

6. **Tree-dump pattern reminder**: unary negation of lvalue vars prints as
   `-NON_LVALUE_EXPR <name>` not `-name` in `fdump-tree-original`. Patterns
   for overload arguments should use `.*` to absorb potential NON_LVALUE_EXPR
   wrappers.

## Open risks / TODOs

- DEV-G04 (D3 bare-nested non-enforcement) is shared with Clang. Both paper
  implementations behave identically. Future step could enforce D3 via one
  token of post-close lookahead.
- `test_paren_slot` in infix-diag.C tests `a \`(add)\` b` (non-nested parens).
  A true D3 nesting test (`x \`(f \`g\` h)\` y`) would require a function
  pointer return type and is left to G05 semantics tests if desired.
