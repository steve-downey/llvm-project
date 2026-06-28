# Handoff — G05 Semantics tests

- **Status:** DONE (gate passed, with deviation DEV-G05 — see below)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ 2961fe511bf
- **Date / agent:** 2026-06-28

## What changed

- `gcc/testsuite/g++.dg/backtick/infix-semantics.C`: new compile test covering:
  1. **Overload resolution** — `int f_ovl(int,int)` vs `double f_ovl(double,double)`;
     `__is_same` + `decltype` static_assert confirms correct overload selected;
     tree-dump scans confirm `f_ovl (a_i, b_i)` and `f_ovl (a_d, b_d)`.
  2. **Qualified callee** — `tx \`ns::g\` ty` where `ns::g(T,T)` is only declared
     in namespace `ns`; exercises the callee-is-expression path.
  3. **Templates** — `template<typename F> int apply(int,int,F) { return a \`func\` b; }`
     instantiated with `add_t`; compile-only (proves tsubst of CALL_EXPR works).
  4. **constexpr** — `static_assert(1 \`add_cx\` 2 == 3)` and chained variant;
     proves `finish_call_expr` evaluates constexpr callees at compile time.
  5. **Codegen tree dump** — `p \`add_cx\` q` (global non-constexpr args);
     tree-dump scan confirms `add_cx (p, q)` in original dump.
  6. **Lambda in slot** — `3 \`[](int a,int b){ return a*b; }\` 4` in a function
     body; confirms `operator()` lookup for closure types.
  Options: `-fbacktick -std=c++17 -fdump-tree-original`

## Verification evidence

```
# Targeted test (infix-semantics.C only)
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/infix-semantics.C"
→ 4 expected passes, 0 failures

# Full backtick suite
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 49 expected passes, 0 failures  (was 45 before G05; +4 from infix-semantics.C)

# Parse regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

**DEV-G05** (logged in `ops/gcc/DEVIATIONS.md`): The step file requires testing
"ADL finds the slot callee (koenig)". ADL does NOT work in GCC's backtick
implementation:

- **Pure ADL** (name only in namespace, not globally visible): fails with "not
  declared in this scope" because `cp_parser_lookup_name` fires during slot
  parsing — before `finish_call_expr` can add Koenig candidates.
- **ADL augmentation** (global name + ADL adds namespace overload): also fails
  because the parser resolves the slot to a concrete `FUNCTION_DECL`, not an
  overload set; `finish_call_expr(koenig_p=true)` has no set to augment.

Root cause: the slot is parsed as a standalone assignment-expression. In a
regular call `g(x, y)`, GCC's `cp_parser_postfix_expression` tracks the name
specially for Koenig. The backtick handler calls `cp_parser_assignment_expression`
which goes through `cp_parser_primary_expression` → `cp_parser_lookup_name`,
resolving the name immediately.

**Cross-compiler difference from Clang:** Clang's slot is also parsed as an
expression, but Clang's `BuildCallExpr` receives an `UnresolvedLookupExpr` for
unqualified names not yet in scope, enabling ADL. GCC does not produce an
`UnresolvedLookupExpr` equivalent for the backtick slot.

**Workaround in test:** Used a qualified-name test (`tx \`ns::g\` ty`) — the
same approach Clang's S05 used (Clang S05 also tested qualified names, not
pure-ADL). This is documented in the test file comment.

## Discoveries affecting later steps

### std=c++17 required for C++11+ features
The test framework exercises each file with multiple `-std=` variants (gnu++98,
gnu++17, gnu++20, gnu++29). Tests using `static_assert`, `constexpr`, `decltype`,
or lambdas MUST include `-std=c++17` in `dg-options` — this overrides the
framework's earlier `-std=` flag (last wins on the command line), preventing
C++98 failures.

### __is_same built-in works without headers
`__is_same(type1, type2)` is a GCC built-in type trait available without any
include. It returns `true/false` in a constant expression, suitable for
`static_assert`. Use `__is_same(decltype(expr), ExpectedType)` to verify
overload resolution return types at compile time.

### Tree dump scan patterns work for non-default std
The `dg-final { scan-tree-dump ... }` directives fire for each `-std=` variant
independently. When `-std=c++17` is in dg-options, it overrides the framework
std for all variants, so only one dump is produced. Patterns are stable.

### Lambda in function body works out of the box
`3 \`[](int a, int b){ return a * b; }\` 4` in a function body compiles
cleanly. The lambda body `{}` is handled by GCC's existing paren/bracket
save/restore of `backtick_is_operator_p`, just as D3 parens. No special code
needed.

### Template instantiation confirmed
`apply<int(*)(int,int)>` with a function-pointer `func` argument: `a \`func\` b`
desugars to a call through the pointer. `finish_call_expr` with a function
pointer callee works. No tsubst-specific code needed.

### ADL limitation is NOT a cross-compiler consensus
Clang effectively supports ADL for backtick (via `UnresolvedLookupExpr`) but
it was not tested in S05 and not confirmed. The practical workaround (qualified
names) is sufficient for the paper. A future step could fix GCC's ADL by treating
a bare-name slot specially — passing the IDENTIFIER_NODE directly to
`finish_call_expr` instead of the pre-resolved decl.

## Forward notes for the NEXT step (G06 — Precedence/associativity tests)

G06 is test-only (no parser/sema code changes expected). Key constraints:

1. **No linker**: `dg-do run` fails. Use `dg-do compile` with `-fdump-tree-original`
   tree-dump scans, same pattern as infix-basic.C and this step.

2. **Unary negation dump pattern**: `-a` prints as `-NON_LVALUE_EXPR <a>` for
   lvalue globals. Use `add .-.*a.*-.*b` style pattern (as in infix-basic.C line
   22) for the `-x \`f\` -y → f(-x,-y)` test.

3. **`a * b \`f\` c → a * f(b,c)`**: The tree for `a*b` at the left of `\`f\`` is
   `a * b` still as a binary operator, wrapping the CALL_EXPR for `f(b,c)`. Dump
   pattern: `scan-tree-dump "\\* .* f \\(b, c\\)" "original"` or similar.

4. **Member access `a.b \`f\` c.d`**: In the tree, `a.b` appears as `COMPONENT_REF`
   notation; the dump shows `a.b` or `a->b` depending on the type. For struct
   globals with `.`, the original dump shows `a.b`. Pattern: `f \\(a\\.b, c\\.d\\)`.

5. **Ternary `a ? b : c \`f\` d → a ? b : f(c,d)`**: The tree shows
   `ConditionalExpr` (or `COND_EXPR`) with the third branch being `f(c,d)`. Dump
   pattern: confirm the CALL_EXPR is inside the else-branch, not wrapping the
   conditional.

6. **Left-associativity `a \`f\` b \`g\` c → g(f(a,b),c)`**: Already confirmed in
   infix-basic.C (`mul (add (a, b), c)`) pattern. G06 should add explicit
   confirmation with different functions.

7. **All cases already covered by infix-basic.C**: The left-associativity case and
   the `-x \`f\` -y` case are already in infix-basic.C. G06 should add NEW cases
   (`*` precedence, member access, ternary) and cross-reference existing ones.

## Open risks / TODOs

- DEV-G05 (ADL limitation): a future step could fix by treating a bare-name slot
  as a Koenig-eligible identifier (pass `IDENTIFIER_NODE` instead of pre-resolved
  decl to `finish_call_expr`). Requires separate step; not in scope for G05/G06.
- DEV-G04 (D3 bare-nested non-enforcement) shared with Clang. Deferred.
