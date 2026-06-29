# Handoff — G07 Keyword-escape identifiers (position-based)

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ 570dc317d8e
- **Date / agent:** 2026-06-28

## What changed

- `gcc/cp/parser.cc`: three parser changes:
  1. **`cp_parser_primary_expression`** — added `case CPP_BACKTICK:` before `default:`
     that does `goto id_expression` when `flag_backtick` is set (routes backtick in
     primary-expression position to the id-expression path). Falls through to the
     existing `default:` error when flag is off.
  2. **`cp_parser_id_expression` (global-scope switch)** — added `case CPP_BACKTICK:`
     in the `switch (token->type)` at the `global_scope_p` branch (handles `:: `kw``).
     Calls `cp_parser_unqualified_id` when `flag_backtick` is set; falls through to
     error otherwise.
  3. **`cp_parser_unqualified_id`** — added `case CPP_BACKTICK:` before `default:`.
     When `flag_backtick` is set: consumes the opening backtick, requires the next
     token to be `CPP_KEYWORD` (emits "backtick keyword-escape requires a C++ keyword"
     if not, guarded by `!cp_parser_uncommitted_to_tentative_parse_p`), consumes the
     keyword, requires the closing backtick via `cp_parser_require(RT_CLOSE_BACKTICK,
     open_loc)`, returns `cp_expr(token->u.value, id_loc)`. Falls through to `default:`
     if flag not set.

- `gcc/cp/decl.cc`: modified `grokdeclarator` at the `IDENTIFIER_KEYWORD_P(dname)` check
  (line ~14210). Added `if (!flag_backtick)` guard so the "declarator-id missing; using
  reserved word" error is suppressed when `-fbacktick` is set. Without the guard, the
  `IDENTIFIER_KEYWORD_P` flag on the keyword's shared IDENTIFIER_NODE caused an error
  even after the escape was parsed correctly.

- `gcc/testsuite/g++.dg/backtick/escape.C`: new compile-only test covering:
  - Declarator-id: `void `new`(int, int);`
  - Primary-expression call: `` `new`(a, b); ``
  - Member field access: `s.`delete``
  - Member function call: `s.`new`(42)`
  - Infix regression: `a `add` b`
  - D3 interaction: `x `(`new`)` y` (escaped callee in infix paren slot)

- `gcc/testsuite/g++.dg/backtick/escape-diag.C`: new test confirming that
  `void `x`();` (non-keyword inside escape) emits "backtick keyword-escape requires
  a C++ keyword".

- `gcc/testsuite/g++.dg/backtick/lex-token.C`: updated to reflect new G07 error for
  a lone backtick in name position. Was "expected primary-expression" on line 6; now
  "backtick keyword-escape requires a C++ keyword" on line 9 (the `}` following the
  lone backtick, where the parser found a non-keyword token).

## Verification evidence

```
# Targeted backtick suite
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 73 expected passes, 0 failures  (was 64 before G07; +9 from 2 tests × 3 std variants
  for escape.C + escape-diag.C; lex-token.C updated and now passes)

# Parse regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

**DEV-G07a** (keyword-identifier flag in grokdeclarator): The step file said "Feed it
into the normal name path" after building an IDENTIFIER_NODE. The complication: in GCC,
the keyword's IDENTIFIER_NODE has `IDENTIFIER_KEYWORD_P` set as a permanent per-node
flag, and `grokdeclarator` (decl.cc) emits an error when it sees a keyword-flagged name.
The fix was to suppress that error with `if (!flag_backtick)` rather than trying to
de-keyword the identifier node (which is unsafe—it affects the shared interned node and
would break subsequent `new`-expression parsing which checks `token->keyword == RID_NEW`
at the token level, not `IDENTIFIER_KEYWORD_P`). The `IDENTIFIER_KEYWORD_P` check in
`grokdeclarator` is the ONLY semantic-analysis check that would block keyword-named
declarations; the two other places that use this flag (`lex.cc` token-to-keyword
conversion, `name-lookup.cc` fuzzy matcher) are unaffected by using keyword identifiers
as declarations.

Cross-compiler note: Clang avoided this issue by mutating the token in-place
(`Tok.setKind(tok::identifier)`) before it reached the semantic layer. GCC's approach
is to fix the semantic check instead.

## Discoveries affecting later steps

### Entry points hooked
All three hooks go through `cp_parser_unqualified_id` as the final parser:
- Primary-expression position: `cp_parser_primary_expression` → `goto id_expression` → 
  `cp_parser_id_expression` → else-branch → `cp_parser_unqualified_id`
- Member-access (`.`/`->`): `cp_parser_postfix_dot_deref_expression` → 
  `cp_parser_id_expression` → else-branch → `cp_parser_unqualified_id`
- Declarator-id: `cp_parser_direct_declarator` → `cp_parser_declarator_id` →
  `cp_parser_id_expression` → else-branch → `cp_parser_unqualified_id`
- After `::`: `cp_parser_id_expression` global-scope branch → new `case CPP_BACKTICK:` →
  `cp_parser_unqualified_id`

### Keyword-only policy confirmed
The escape accepts only `CPP_KEYWORD` tokens; ordinary identifiers give
"backtick keyword-escape requires a C++ keyword". The escape handler does NOT
check `backtick_is_operator_p` because name positions are unambiguous — there is
no operator/escape conflict at operand position.

### grokdeclarator deviation documentation
The `flag_backtick` guard in `grokdeclarator` (decl.cc line ~14212) is the ONLY
place in the semantic layer that needed changing. All other name-lookup, overload
resolution, and declaration machinery treats the keyword identifier as a normal
identifier after the parser returns it.

### Lone-backtick error behaviour changed
A lone backtick in name/primary position (no keyword following) now emits
"backtick keyword-escape requires a C++ keyword" at the location of the unexpected
token (the token AFTER the backtick). This is visible in `lex-token.C`. G08
will not see this issue since its cases have well-formed escapes.

### Tentative parsing
The `cp_parser_uncommitted_to_tentative_parse_p` guard suppresses the error
diagnostic during tentative parsing (so the tentative parse can silently fail).
`cp_parser_simulate_error` marks the tentative parse as failed, causing backtrack.

## Forward notes for the NEXT step (G08 — Tentative parse / decl-vs-expr)

G08 must verify that `T `new`;` (declaration) and `` `new`(a,b); `` (expression)
inside the same block are correctly disambiguated by GCC's tentative parser.

### Key tentative-parse entry points in GCC
GCC's C++ parser uses `cp_parser_parse_tentatively` + `cp_parser_parse_definitely`
(or `cp_parser_abort_tentative_parse`) throughout. The main disambiguation path for
"declaration vs. expression statement" goes through:
- `cp_parser_statement` → tries `cp_parser_simple_declaration` (tentatively)
  → if that fails, falls back to `cp_parser_expression_statement`
- `cp_parser_simple_declaration` → `cp_parser_decl_specifier_seq` then
  `cp_parser_init_declarator` (tentatively) → `cp_parser_declarator`
- Inside tentative declarator parsing, our `case CPP_BACKTICK:` in
  `cp_parser_unqualified_id` now fires. The error is suppressed by
  `cp_parser_uncommitted_to_tentative_parse_p`, and `cp_parser_simulate_error`
  marks failure if the escape is malformed.

### Likely working already
Since `cp_parser_unqualified_id` correctly handles `CPP_BACKTICK` and uses
`cp_parser_simulate_error` for tentative-parse error suppression, the G07
implementation should make `T `new`;` work correctly in tentative paths WITHOUT
additional changes. The G06 handoff noted that Clang needed S08 to teach
`TryParseDeclarator`; GCC uses a different mechanism (tentative parsing via
`cp_parser_unqualified_id` which already handles the backtick).

### Test to write for G08
```cpp
// In a function body:
struct T { int x; };
void test_disambiguation() {
    T `new` = {42};   // declaration: variable named 'new' of type T
    (void)`new`.x;    // expression: access member x of 'new'
}
```
Also verify: `` `new`(a,b); `` as an expression-statement does NOT get
misidentified as a declaration attempt.

### What to check if G08 fails
If disambiguation fails, look at `cp_parser_statement` (around line ~12400)
and `cp_parser_simple_declaration` (around line ~14600) in parser.cc, specifically
the tentative parse flags and whether `cp_parser_declarator` + `cp_parser_unqualified_id`
correctly returns `error_mark_node` (simulated error) when no matching escape is found.

## Open risks / TODOs

- DEV-G05 (ADL limitation) still open.
- The `flag_backtick` guard in `grokdeclarator` suppresses the error for ALL
  keyword-named declarators when `-fbacktick` is set, not just those from the
  explicit backtick escape. In practice this is fine because the parser would have
  already failed for non-escaped keywords before reaching `grokdeclarator`. But
  it is a slight over-permission.
- Module support (module.cc) not tested: `IDENTIFIER_KEYWORD_P` checks in
  `module.cc` lines 20117 and 20160 are for module streaming. If a keyword-named
  entity is exported in a module, these paths may need attention (deferred).
