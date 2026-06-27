# Handoff — S06 Precedence/associativity test sweep

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `0cfa70dd367c`
- **Date / agent:** 2026-06-27

## What changed

- `clang/test/Parser/backtick-precedence.cpp` — new test file with five
  FileCheck cases:
  1. D2 confirmed: `-1 \`f\` -2` → both `UnaryOperator prefix '-'` nodes land
     inside the `CallExpr` args, not outside (Option A, symmetric).
  2. Tighter than `*`: `2 * 3 \`f\` 4` → `BinaryOperator '*'` is the outer
     node with `CallExpr f(3,4)` as its RHS.
  3. Left-associativity: `1 \`f\` 2 \`g\` 3` → outer `CallExpr g`, inner
     `CallExpr f(1,2)` as first arg.
  4. Ternary: `1 ? 2 : 3 \`f\` 4` → `ConditionalOperator` is outer;
     `CallExpr f(3,4)` is the else-branch.
  5. Member-access operands: `s1.x \`f\` s2.x` → `CallExpr f` with two
     `MemberExpr .x` children.
- `ops/PLAN.md` — S06 box checked, status log row added.

## Verification evidence

```
# Targeted lit test
llvm-lit -v clang/test/Parser/backtick-precedence.cpp
→ PASS: Clang :: Parser/backtick-precedence.cpp (1 of 1)

# check-clang
52215 total (+1 vs S05): 46450 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline): Clang :: Format/dump-config-objc-stdin.m
```

## Deviations from the plan / design

None. All five precedence interactions behave exactly as §4 Option A predicts.
D2 is explicitly confirmed: both prefix-minus operators fold into the call args.

## Discoveries affecting later steps

- All existing backtick tests in `clang/test/Parser/backtick-infix.cpp`
  continue to pass — no regression in S03's infix machinery.
- The FileCheck patterns use `AST:` (not `AST-NEXT:`) for robustness; the
  AST dump indentation is deep and `AST-NEXT` is brittle across clang
  versions.

## Forward notes for the NEXT step (S07 — Keyword-escaped identifiers)

S07 adds position-based disambiguation for `` `kw` `` as an identifier escape.
Key implementation points from reading the code:

### Primary hook: `ParseCastExpression` (clang/lib/Parse/ParseExpr.cpp)

The switch on `SavedKind` starts at line 819. The `tok::identifier` case
is at line 936 (label `ParseIdentifier`). S07 should add a
`case tok::backtick:` branch **before** the identifier case (or fall through
to `ParseIdentifier` after synthesizing the IdentifierInfo). The branch should:
1. Consume the opening backtick.
2. Read `Tok` — check that it is a keyword (`Tok.isKeyword(getLangOpts())`
   or `Tok.getIdentifierInfo() == nullptr` — decide keyword-only policy).
3. Grab the spelling via `PP.getSpelling(Tok)`.
4. Get or create an `IdentifierInfo *II = PP.getIdentifierInfo(spelling)`.
5. Forcibly re-annotate the current token as `tok::identifier` with `II`
   (set `Tok.setKind(tok::identifier); Tok.setIdentifierInfo(II);`).
6. Consume the closing backtick.
7. Fall through to / `goto ParseIdentifier`.

### After `.` / `->` / `::` — member and qualified names

Member access (`obj.\`delete\`()`) is parsed in `ParsePostfixExpressionSuffix`
(`clang/lib/Parse/ParseExpr.cpp`). After consuming `.` or `->`, the parser
expects `tok::identifier` (or a keyword treated as identifier). Insert the
same backtick-consume-resymthesize logic there, guarded by
`getLangOpts().Backtick`.

Qualified names (`ns::\`new\``) are handled in `ParseUnqualifiedId`
(`clang/lib/Parse/ParseExprCXX.cpp`, around line 2043 where `tok::identifier`
is the common case). Same pattern.

### Declarator-id positions

`ParseDeclarator` → `ParseDirectDeclarator` calls `ParseUnqualifiedId` for
the name, so the `ParseUnqualifiedId` fix likely covers this path too. Verify
with `void \`new\`();`.

### D3 / nested-paren interaction

The infix operator slot suppresses `BacktickIsOperator`, so inside a slot
a bare `tok::backtick` cannot be an infix op. However, S07's escape handling
should *not* fire inside the operator slot either — the escape is a
name-position construct. Confirm by checking that `` x `(`new`)` y `` (D3
parens around escaped callee) still works: the paren restores
`BacktickIsOperator`, then the inner `` `new` `` is parsed as a
primary-expression (escape), yielding an identifier. Test this case.

### Keyword-only policy decision

The step says "Recommended: accept only actual keywords." To check: use
`Tok.isKeyword(getLangOpts())` or `Tok.getKind() >= tok::kw___attribute`.
Ordinary identifiers inside escape braces are syntactically redundant and
excluding them gives maximum disjointness. Record the decision in a
`ops/DEVIATIONS.md` row either way.

### Test file location

`clang/test/Parser/backtick-escape.cpp` — parallel to
`backtick-infix.cpp`. Use `-ast-dump` to confirm function names appear
as ordinary identifiers in the AST.

## Open risks / TODOs

- DEV-04 (bare nested backtick silently mis-parses as a chain) still
  unaddressed. Not affected by S07 but remains open.
- `err_backtick_nested_requires_parens` in DiagnosticParseKinds.td still dead
  code from S04.
