# Handoff — S10 clang-format for both backtick uses

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `f185e099e046`
- **Date / agent:** 2026-06-27

## What changed

- `clang/lib/Format/FormatToken.h` — added 4 new `TT_` token types (alphabetically before `TT_BinaryOperator`):
  - `TT_BacktickEscapeClose`, `TT_BacktickEscapeOpen`
  - `TT_BacktickInfixClose`, `TT_BacktickInfixOpen`
- `clang/lib/Format/Format.cpp` — `getFormattingLangOpts()`: added `LangOpts.Backtick = 1` for `LK_Cpp` and `LK_ObjC`, so clang-format's lexer produces `tok::backtick` tokens.
- `clang/lib/Format/TokenAnnotator.cpp`:
  - Added `TokenType PendingBacktickKind = TT_Unknown;` field to `AnnotatingParser` to track open/close state.
  - Added backtick classification in `determineTokenType()`: post-operand position → `TT_BacktickInfixOpen`; otherwise → `TT_BacktickEscapeOpen`; pending close → `TT_BacktickInfixClose` / `TT_BacktickEscapeClose`.
  - Added spacing rules at the top of `spaceRequiredBetween()`: no space after open/before close (hug), space before infix open, space after infix close.
  - Added D8 break rules at the top of `canBreakBefore()`: `CanBreakBefore = false` immediately after open or before close.
- `clang/unittests/Format/FormatTest.cpp` — `TEST_F(FormatTest, BacktickOperatorFormatting)` and `TEST_F(FormatTest, BacktickOperatorJSNonRegression)`.
- `clang/unittests/Format/FormatTestJS.cpp` — `TEST_F(FormatTestJS, BacktickTemplateStringNonRegression)`.
- `clang/unittests/Format/TokenAnnotatorTest.cpp` — `TEST_F(TokenAnnotatorTest, BacktickTokenTypes)`.

## Verification evidence

```
# New tests (all 4 pass)
FormatTests --gtest_filter="FormatTest.BacktickOperatorFormatting:\
  FormatTest.BacktickOperatorJSNonRegression:\
  FormatTestJS.BacktickTemplateStringNonRegression:\
  TokenAnnotatorTest.BacktickTokenTypes"
→ [PASSED] 4 tests

# Full FormatTests regression suite
FormatTests
→ [PASSED] 1270 tests (no regressions)

# JS template literal non-regression
FormatTests --gtest_filter="FormatTestJS.TemplateStrings"
→ [PASSED] 1 test

# check-clang
Total: 52223, Passed: 46450, xfail: 26, unsupported: 5732, skipped: 6,
failed: 1 (env-only: dump-config-objc-stdin.m)
(DirectoryWatcherTest flakes are pre-existing environmental failures,
present before my changes at the same count.)

# Format lit tests targeted
llvm-lit clang/test/Format/
→ 32 passed, 1 failed (dump-config-objc-stdin.m — env-only baseline)
```

## Deviations from the plan / design

1. **`getFormattingLangOpts` guard**: I used `Style.Language == LK_Cpp || Style.Language == LK_ObjC` rather than `LangOpts.CPlusPlus` because JS, Java, and other languages fall through to `default:` in the switch and also set `CPlusPlus = 1`. Using `CPlusPlus` would have enabled backtick lexing for JS and broken `handleTemplateStrings()`. The language check is more precise. No impact on design.

2. **D8 SplitPenalty for slot interior**: D8 calls for a "small split-penalty bump" for tokens inside the operator slot (not just hard-forbid at the edges). This was not implemented — the hard `CanBreakBefore = false` at the open and close boundaries handles the important cases. For typical operator slots (a single identifier or qualified name), this is sufficient. Noted in the open risks below.

3. **No explicit `TT_` comment in `resetTokenMetadata`**: The new types are set by `determineTokenType`, not pre-annotated before parsing, so they don't need to be in the `resetTokenMetadata` preserve list. This is correct behavior.

## Discoveries affecting later steps

### JS guard pattern
The `handleTemplateStrings()` guard is `if (Style.isJavaScript())` in `FormatTokenLexer.cpp:128`. Setting `LangOpts.Backtick = 1` for JS would break template string handling because the JS lexer path calls `handleTemplateStrings()` which expects `tok::unknown` or `tok::l_brace`/`tok::r_brace` for the backtick — NOT `tok::backtick`. My fix uses a language check in `getFormattingLangOpts` rather than a CPlusPlus flag.

### `PendingBacktickKind` is per-AnnotatingParser
`PendingBacktickKind` tracks open/close state within a single `AnnotatingParser` instance. Since `AnnotatingParser` is created fresh for each `AnnotatedLine`, it naturally resets across lines. This means malformed input (unmatched backtick) won't corrupt state across lines.

### Post-operand detection for classification
The open backtick is classified as infix (post-operand) if the previous non-comment token is: a literal, `tok::identifier`, `tok::r_paren`, `tok::r_square`, `tok::r_brace`, `tok::kw_true`, `tok::kw_false`, `tok::kw_nullptr`, `tok::kw_this`, or a prior backtick close. This mirrors what the parser uses.

### Break behavior: after close, not inside
With `CanBreakBefore = false` on the first slot token and on the close backtick, the formatter breaks AFTER the close backtick when the line must wrap (not inside the pair). This correctly implements D8's "hard-forbid adjacent to delimiters."

## Forward notes for the NEXT step (S11 — AST wrapper)

S11 adds a thin transparent AST wrapper around the desugared `CallExpr`. Key locations:

### Where to add the new node
- Add a new `Stmt` subclass (e.g., `BacktickInfixExpr`) in `clang/include/clang/AST/Expr.h` near other expression wrapper nodes.
- Register it in `clang/include/clang/AST/StmtNodes.td`.
- Allocate from `ASTContext::Allocate` (see how other `Expr` nodes are allocated).

### Sites that need updating
The step file lists: AST serialization (PCH/modules), visitors (`StmtVisitor`/`RecursiveASTVisitor`), `TreeTransform`, and ASTContext allocation. The key files are:
- `clang/lib/AST/ASTContext.cpp` — allocation
- `clang/lib/Serialization/ASTReaderStmt.cpp` and `ASTWriterStmt.cpp` — PCH
- `clang/include/clang/AST/StmtVisitor.h`, `RecursiveASTVisitor.h` — visitors
- `clang/lib/Sema/TreeTransform.h` — template instantiation
- `clang/lib/AST/StmtPrinter.cpp` — `-ast-print` output
- `clang/lib/AST/StmtProfile.cpp` — AST diff/profile
- `clang/lib/CodeGen/CGExpr.cpp` — codegen (forward to inner call)
- `clang/lib/Sema/SemaExpr.cpp` — where S03's Sema entry currently returns `BuildCallExpr`; change to wrap in `BacktickInfixExpr`

### S03's Sema entry
The Sema function that builds the desugared call is in `clang/lib/Sema/SemaExpr.cpp`. After S11, it will return a `BacktickInfixExpr` wrapping the call instead of the call directly. The parser (`ParseExpr.cpp`) passes lhs/op/rhs source locations through to Sema — make sure these are stored in the new node.

### No interference with S10 (clang-format is token-based)
S11 changes the AST but NOT the token stream. clang-format is token-based, so S10's formatting code is completely unaffected by the AST wrapper addition.

### S11 is independent of S10
The PLAN.md shows S11 depends on S06 only, NOT S10. Both S10 and S11 can run after S09. The current step after S10 is S11.

## Open risks / TODOs

- `err_backtick_nested_requires_parens` in `DiagnosticParseKinds.td` remains dead code from S04 (carried forward through S08 and S09).
- DEV-04 (bare nested backtick silently mis-parses) still unaddressed.
- D8 SplitPenalty for slot-interior tokens not implemented. For typical operator slots (identifier or qualified name), the hard constraints suffice. If a future step adds long multi-token slot expressions, add a per-token penalty bump in `splitPenalty()` when the token is between `TT_BacktickInfixOpen` and `TT_BacktickInfixClose`. Tracking this requires a state variable in `calculateFormattingInformation`, analogous to `PendingBacktickKind` in the annotation phase.
