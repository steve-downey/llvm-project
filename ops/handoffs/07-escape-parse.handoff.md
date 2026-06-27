# Handoff — S07 Keyword-escape identifiers (position-based)

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `0a9f8fc78fc2`
- **Date / agent:** 2026-06-27

## What changed

- `clang/include/clang/Basic/DiagnosticParseKinds.td` — two new diagnostics:
  - `err_backtick_escape_not_keyword`: "backtick keyword-escape requires a C++ keyword"
  - `err_backtick_escape_unterminated`: "missing closing backtick for keyword escape"
- `clang/lib/Parse/ParseExpr.cpp` — new `case tok::backtick:` in `ParseCastExpression`
  (before `case tok::identifier:` / `ParseIdentifier:` label) handling the escape
  in primary-expression position via the `PP.EnterToken` + Tok-mutation + `goto ParseIdentifier` pattern.
- `clang/lib/Parse/ParseExprCXX.cpp` — new `if (getLangOpts().Backtick && Tok.is(tok::backtick))`
  block at the start of `ParseUnqualifiedId` (before the `if (Tok.is(tok::identifier))` check),
  same pattern, covering declarator-id and member-access positions.
- `clang/lib/Parse/ParseDecl.cpp` — added `|| (getLangOpts().Backtick && Tok.is(tok::backtick))`
  to the `isOneOf(tok::identifier, tok::kw_operator, ...)` guard in `ParseDirectDeclarator`
  (line ~6766) so that `void \`new\`();` reaches `ParseUnqualifiedId`.
- `clang/test/Parser/backtick-escape.cpp` — new test: declarator-id, primary-expression call,
  two-arg call, member access, infix regression, D3 nested-paren callee.
- `clang/test/Parser/backtick-escape-diagnostics.cpp` — new test: non-keyword inside escape
  rejected with `err_backtick_escape_not_keyword`.

## Verification evidence

```
# Targeted lit tests
llvm-lit -v clang/test/Parser/backtick-escape.cpp \
             clang/test/Parser/backtick-escape-diagnostics.cpp
→ PASS: Clang :: Parser/backtick-escape.cpp (1 of 2)
→ PASS: Clang :: Parser/backtick-escape-diagnostics.cpp (2 of 2)

# Previous infix tests (regression check)
llvm-lit -v clang/test/Parser/backtick-infix.cpp \
             clang/test/Parser/backtick-diagnostics.cpp \
             clang/test/Parser/backtick-precedence.cpp
→ All 3 PASS

# check-clang
52217 total (+2 vs S06): 46452 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline): Clang :: Format/dump-config-objc-stdin.m
```

## Deviations from the plan / design

1. `ParseDirectDeclarator` (ParseDecl.cpp) required an explicit backtick guard in its
   `isOneOf(tok::identifier, ...)` check — the step file only named ParseCastExpression
   and ParseUnqualifiedId. Without this fix, `void \`new\`();` produced "expected
   unqualified-id" because `ParseDirectDeclarator` never called `ParseUnqualifiedId`
   for a backtick token. This is not a design deviation; it's an additional hook not
   named in the step file or design doc. No DEVIATIONS.md row needed (it's implementation
   detail, not a design-level finding).

2. Keyword-only policy confirmed: only tokens where `Tok.getIdentifierInfo()->isKeyword(getLangOpts())`
   returns true are accepted inside the escape. Ordinary identifiers give `err_backtick_escape_not_keyword`.
   This matches §12's "Optionally restrict the escape to actual keywords" recommendation.

## Discoveries affecting later steps

### Token synthesis technique
The escape is implemented by consuming all three tokens (`\``, keyword, `` \` ``), then:
```cpp
PP.EnterToken(Tok, /*IsReinject=*/true);  // push RealNext back
Tok.setKind(tok::identifier);
Tok.setIdentifierInfo(II);
Tok.setLocation(IILoc);
Tok.setLength(IILen);
goto ParseIdentifier;
```
This makes `Tok` look like an identifier, and `ConsumeToken()` inside `ParseIdentifier` will
correctly lex `RealNext` from the pushed token. The `PP.EnterToken` + Tok-mutation pattern
mirrors `UnconsumeToken()` and the transform-type-trait keyword-as-identifier hack.

### BalancedDelimiterTracker restores BacktickIsOperator
Confirmed from `RAIIObjectsForParser.h`: `BalancedDelimiterTracker` inherits `BacktickIsOperatorScope`
and sets `BacktickIsOperator = true` inside any paren/bracket block. This means the D3 case
`x \`(\`new\`)\` y` works automatically: inside the infix slot's paren, `BacktickIsOperator` is
restored to true, and `` `new` `` in primary-expression position is handled by S07's
`case tok::backtick:` in `ParseCastExpression`.

### AST output for escaped identifiers
The AST shows the identifier by its underlying name (e.g., `new`, `delete`) without backticks.
FileCheck patterns should match `new` not `` `new` ``.

## Forward notes for the NEXT step (S08 — Tentative-parse / decl-vs-expr integration)

S08 must teach `TryParseDeclarator` (ParseTentative.cpp) to recognize a backtick-escaped name
as a valid declarator-id. Key location:

### Primary guard: `TryParseDeclarator` line ~901 (ParseTentative.cpp)

```cpp
if ((Tok.isOneOf(tok::identifier, tok::kw_operator) ||
     (Tok.is(tok::annot_cxxscope) && (NextToken().is(tok::identifier) ||
                                      NextToken().is(tok::kw_operator)))) &&
    mayHaveIdentifier) {
```

Add `|| (getLangOpts().Backtick && Tok.is(tok::backtick))` to the outer condition.
Inside the `if`, add a branch for `tok::backtick`:
```cpp
} else if (getLangOpts().Backtick && Tok.is(tok::backtick)) {
  // Consume the backtick escape: `kw`
  ConsumeToken(); // opening `
  if (Tok.getIdentifierInfo())
    TentativelyDeclaredIdentifiers.push_back(Tok.getIdentifierInfo());
  ConsumeToken(); // keyword
  if (Tok.is(tok::backtick))
    ConsumeToken(); // closing `
}
```

The tentative parser doesn't need to validate keyword-only policy (it just needs to
recognize the token sequence as a valid declarator-id candidate).

### `isCXXDeclarationStatement` and `isDeclarationSpecifier`
These in ParseTentative.cpp call into `TryParseSimpleDeclaration` → `TryParseDeclarator`.
Once `TryParseDeclarator` handles backtick, those paths should work.

### Additional identifier checks in ParseTentative.cpp to audit
- Line 210: `if (Tok.is(tok::identifier))` — in `isCXXSimpleDeclaration`
- Line 860: `if (Tok.is(tok::identifier))` — in `TryParseDeclarator` scope specifier path
- Line 913: `else if (Tok.is(tok::identifier))` — inside the `mayHaveIdentifier` block
- Line 1061: `if (Tok.is(tok::identifier))` — in `isDeclarationSpecifier`
- Line 1673: `if (Tok.isNot(tok::identifier))` — in `TryParseProtocolQualifiers`
- Line 1788: `if (SeenType && Tok.is(tok::identifier))` — in parameter disambiguation

Line 913 is particularly important: it records `TentativelyDeclaredIdentifiers`. After adding
the backtick case at the outer guard, line 913 won't fire for backtick (it's not
`tok::identifier`). The new backtick branch above handles the push.

### Test case for S08 (from the step file)
```cpp
T `new`;    // declares variable named 'new' of type T
`new`(a,b); // expression statement: call
```
Both in the same block should parse correctly without a disambiguity error.

## Open risks / TODOs

- Tentative parsing: `T \`new\`;` without S08's fix may mis-classify as an expression
  statement. S07 does NOT regress this because S07 only adds hooks in the non-tentative
  parsing paths. S08 must fix the tentative parser.
- `err_backtick_nested_requires_parens` in DiagnosticParseKinds.td remains dead code from S04.
- DEV-04 (bare nested backtick silently mis-parses) still unaddressed.
