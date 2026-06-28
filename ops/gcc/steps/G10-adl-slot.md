# G10 — Revisit operator-slot ADL (deferred fix)

**Goal.** `x `f` y` performs argument-dependent lookup on the operator slot
exactly as the call `f(x, y)` does. Both **pure ADL** (callee visible only in
an argument's namespace) and **ADL augmentation** (ordinary callee + a
namespace overload reached only via ADL) work for a bare unqualified-id slot.

**Depends on:** G03 (the infix parse + desugar). Independent of G07–G09.
**Design refs:** §17.4; §3 D6; §8 step 3; deviation `DEV-G05`.

## Background (the defect)
G03 parses the slot as a standalone assignment-expression, so the slot
identifier is resolved at *parse time* (`cp_parser_lookup_name`) and arrives
at `finish_call_expr` already a `FUNCTION_DECL`. Consequences (`DEV-G05`):
pure ADL fails ("not declared in this scope") and augmentation fails (no
overload set left to augment). Clang conforms (slot is an
`UnresolvedLookupExpr`); GCC must match — this is normative, not a permitted
cross-compiler difference.

## Do
1. **Defer resolution for a bare unqualified-id slot.** In the backtick branch
   of `cp_parser_binary_expression` (`gcc/cp/parser.cc`, where G03 parses the
   slot): when the slot is a simple **unqualified-id**, do not resolve it to a
   decl. Preserve it as the identifier / unresolved-overload form that
   `finish_call_expr` expects for Koenig lookup — mirror how an ordinary
   unqualified call *callee* is kept un-resolved in
   `cp_parser_postfix_expression`. Study `perform_koenig_lookup` and the
   normal-call callee path.
2. **Leave everything else alone.** Qualified names, member access, lambdas,
   and arbitrary expressions (D4) resolve normally — ADL does not apply to
   them, identical to normal calls. The change is gated to the same bare-name
   case the language already gates ADL to.
3. **Keep** `finish_call_expr(..., koenig_p=true, ...)` (already in from G03);
   the fix is upstream of it — what the slot *is* when it arrives.

## Build / Verify (gate)
`gcc-backtick` worktree, S12 build, `g++.dg` gate. Tests in
`gcc/testsuite/g++.dg/` (extend `infix-semantics.C` or add `infix-adl.C`):
- **Pure ADL**: convert the G05 `ns::g` qualified workaround to a callee
  visible only in an argument's namespace; compiles and runs (`f(a,b)`).
- **Augmentation**: ordinary callee in scope + a namespace overload selected
  only via ADL on the operands.
- **Negative control**: a name with neither ordinary nor ADL match still
  errors.
- `-fdump-tree-original` shows a `CALL_EXPR` to the ADL-found function.
- Qualified-name path still passes; off-flag (`-fno-backtick`) unchanged;
  baseline green (105 parse tests 0 fail).

## Done when
Pure ADL and augmentation work for a bare-name slot, matching the Clang test;
the same program compiles under both compilers. Update `DEV-G05` from
"DEFECT — fix in GCC track" to resolved, and append the G10 status-log row.

## Capture in handoff
Where in `cp_parser_binary_expression` the slot is now kept unresolved, the
`perform_koenig_lookup` / callee path you mirrored, and confirmation that the
arbitrary-expression slot (D4) and `backtick_is_operator_p` save/restore are
unaffected.

## Pitfalls
- Don't regress the arbitrary-expression slot (D4): only the bare
  unqualified-id path changes.
- Don't break the `backtick_is_operator_p` save/restore from G03.
- Dependent/template operands must still instantiate — the unresolved slot
  has to survive template substitution like a normal call callee.
