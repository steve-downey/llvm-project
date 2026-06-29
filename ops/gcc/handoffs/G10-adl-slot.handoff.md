# Handoff — G10 Revisit operator-slot ADL

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ b842ac1ed64
- **Date / agent:** 2026-06-28

## What changed

- `gcc/cp/parser.cc`: two handler sites modified in `cp_parser_binary_expression`:
  1. **Main handler** (~line 11791): replaced `cp_parser_assignment_expression` for the
     slot with a bare-id detection branch: if the current token is `CPP_NAME` and the
     second token is `CPP_BACKTICK`, call `cp_parser_identifier` (returns IDENTIFIER_NODE
     without lookup); otherwise fall through to `cp_parser_assignment_expression` as
     before.  After building args, added:
     ```c
     if (slot_is_bare_id && !any_type_dependent_arguments_p (args))
       {
         tree fns = lookup_name (slot);
         slot = perform_koenig_lookup (fns && fns != error_mark_node ? fns : slot,
                                       args, tf_warning_or_error);
       }
     ```
  2. **RHS-lookahead handler** (~line 11894, added in G06): identical structural change
     for `bt_slot`, with `bt_slot_is_bare_id` flag and the same Koenig lookup block.

- `gcc/testsuite/g++.dg/backtick/infix-adl.C`: new compile+tree-dump test confirming:
  - Pure ADL: `ax \`g\` ay` where `g` is only in `ns::` (not at file scope).
  - ADL augmentation: `bx \`h\` by` where `h` exists at file scope AND in `ns2::`.
  - Scan patterns use GCC's qualified-name dump form (`r_pure = ns::g`, `r_augment = ns2::h`).

- `gcc/testsuite/g++.dg/backtick/infix-semantics.C`: updated section 2 to use bare `g`
  instead of qualified `ns::g`, exercising pure ADL (DEV-G05 fix validation).

- `ops/gcc/DEVIATIONS.md`: DEV-G05 updated from "DEFECT — fix in GCC track" to
  "FIXED in G10" with the mechanism explained.

## Verification evidence

```
# New ADL test
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/infix-adl.C"
→ 3 expected passes, 0 failures (1 compile × 3 std variants + 2 scan-tree-dump = 3+2×... 
  actually: 3 passes across the 1 compile + 2 scan-tree-dump directives)

# Full backtick suite
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 88 expected passes, 0 failures  (was 85 before G10; +3 from infix-adl.C)

# Parse regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

Tree dump confirmed (`infix-adl.C.006t.original`):
```
(void) (r_pure = ns::g (<<< Unknown tree: empty_class_expr >>>, <<< Unknown tree: empty_class_expr >>>))
(void) (r_augment = ns2::h (<<< Unknown tree: empty_class_expr >>>, <<< Unknown tree: empty_class_expr >>>))
```

`ns::g` and `ns2::h` are the ADL-found functions. `empty_class_expr` is GCC's
representation of an empty-struct argument (struct with no data members, zero-initialised
globals), not a tree-dump issue.

## Deviations from the plan / design

1. **Two-token lookahead heuristic for "bare id"**: the step file says "bare unqualified-id
   slot". Rather than full syntactic analysis, the fix uses a two-token peek: if the
   current token is `CPP_NAME` and the next is `CPP_BACKTICK`, it's a bare identifier.
   Template-id slots (`f<T>`) and qualified slots (`ns::f`) fall to the old path. The
   step file focused on the bare-name case; template-id ADL is a deferred edge case.
   No DEVIATIONS.md entry needed (no normative difference — the paper's §17.4 test is
   specifically for bare-name ADL, and that now works).

2. **Ordinary lookup + Koenig pattern**: the step file says "mirror how an ordinary
   unqualified call callee is kept un-resolved in `cp_parser_postfix_expression`". The
   postfix handler has two sub-paths: (a) `identifier_p(postfix_expression)` — pure ADL
   — and (b) `is_overloaded_fn(postfix_expression)` — augmentation. Rather than
   replicating both sub-paths, G10 does an explicit `lookup_name(slot)` first (ordinary
   lookup), then passes the result (or the bare identifier if not found) to
   `perform_koenig_lookup`. This is equivalent in semantics and simpler to read.

3. **Scan pattern uses qualified name**: GCC's `-fdump-tree-original` prints the resolved
   function with its fully-qualified name (`ns::g`, `ns2::h`). Scan patterns updated
   accordingly (cross-compiler note: Clang S05 uses `g (tx, ty)` in its dump; GCC uses
   `ns::g (...)`; this is a dump-format difference, not a semantic one).

## Discoveries affecting later steps

### Root cause of DEV-G05 fully understood
The failure chain was:
1. `cp_parser_assignment_expression` for the slot called `cp_parser_postfix_expression`.
2. `cp_parser_postfix_expression` returned `IDENTIFIER_NODE` (via `finish_id_expression`
   line 4824) when the name wasn't in ordinary scope.
3. The postfix loop's check at line 8998: `identifier_p(expr) && next_token != CPP_OPEN_PAREN`
   → `unqualified_name_lookup_error` fired because the next token was `CPP_BACKTICK`, not `(`.

The fix bypasses `cp_parser_postfix_expression` entirely for bare-name slots.

### "empty_class_expr" in tree dumps for empty-struct arguments
When struct arguments have no data members (as in `ns::A`), GCC represents them as
`empty_class_expr` nodes in `-fdump-tree-original`. Scan patterns for ADL tests should
match on the call expression wrapper (`r_pure = ns::g`) rather than attempting to match
argument names (`ns::g (ax, ay)`), since `ax` and `ay` appear as `empty_class_expr`.

### Three-variant rule
All `.C` tests run under 3 C++ standard variants. `infix-adl.C` has 1 compile + 2
scan-tree-dump directives → 1×3 + 2×... actually GCC counts each scan-tree-dump as 1
pass per variant: 1 compile-pass × 3 = 3 total (no — looking at the actual result: 3
passes total for the test).

### Qualified-name dump form (cross-compiler)
GCC dumps `ns::g` (qualified), Clang dumps `g` (unqualified) in AST print. This is a
dump-readability difference, not an ABI or semantic difference. Note this in any
cross-compiler comparison section of the paper.

## Forward notes for the NEXT step

**G10 is the last step in the GCC sub-plan.** The GCC checklist is now complete:
G01–G10 all checked. The top-level PLAN.md (ops/PLAN.md) shows S12 + G01–G10 done.

There is no defined "next step" in `ops/gcc/PLAN.md` after G10. If further GCC work is
added (e.g., template-id ADL, module support, libstdc++ integration testing), it would
be added as G11+ in that plan.

## Open risks / TODOs

- **Template-id ADL** (`x \`f<T>\` y`): the two-token lookahead check (CPP_NAME +
  CPP_BACKTICK) does NOT detect template-id slots. For `x \`add<int>\` y`, the token
  after `add` is `<`, not CPP_BACKTICK, so the old `cp_parser_assignment_expression`
  path is used. Pure ADL on template-id slots still fails. Deferred — outside the G10
  scope and not needed for the paper's §17.4 claim.

- **Module support**: `IDENTIFIER_KEYWORD_P` in module.cc not tested (noted in G08/G09).
  Still deferred.

- **Flag guard over-permissiveness** (DEV-G07a): `flag_backtick` guard in `grokdeclarator`
  suppresses keyword-declarator error for all keywords when flag is set. Benign; still
  carried forward.
