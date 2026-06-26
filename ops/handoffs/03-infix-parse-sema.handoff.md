# Handoff — S03 Parse + desugar to CallExpr

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `ecceeff072ff`
- **Date / agent:** 2026-06-26

## What changed

- `clang/include/clang/Basic/OperatorPrecedence.h` — added `prec::Backtick = 16` (highest binary level, above `PointerToMember = 15`); added `bool BacktickIsOperator = true` default parameter to `getBinOpPrecedence` declaration.
- `clang/lib/Basic/OperatorPrecedence.cpp` — added `BacktickIsOperator` parameter to `getBinOpPrecedence` definition; added `case tok::backtick: return BacktickIsOperator ? prec::Backtick : prec::Unknown;` at the top of the switch.
- `clang/include/clang/Parse/Parser.h` — added `bool BacktickIsOperator;` field after `bool GreaterThanIsOperator;` (line 3841).
- `clang/include/clang/Parse/RAIIObjectsForParser.h` — added `BacktickIsOperatorScope` class (saves/restores `BacktickIsOperator`) after `GreaterThanIsOperatorScope` (line 345).
- `clang/lib/Parse/Parser.cpp` — added `BacktickIsOperator(true)` in the constructor initializer list after `GreaterThanIsOperator(true)`.
- `clang/lib/Parse/ParseExpr.cpp`:
  - `isFoldOperator(prec::Level)`: added `Level != prec::Backtick` exclusion so backtick is not treated as a fold operator.
  - Three `getBinOpPrecedence(...)` calls inside `ParseRHSOfBinaryExpression` (initial + two loop-tail calls) updated to pass `BacktickIsOperator` as 4th argument.
  - Added `ExprResult BacktickOp(true); SourceLocation BacktickCloseLoc;` at top of while-loop body alongside `TernaryMiddle`.
  - Added backtick-middle parsing block after the ternary block: creates `BacktickIsOperatorScope BIS(BacktickIsOperator, false)`, calls `ParseExpression()`, then `ExpectAndConsume(tok::backtick, diag::err_expected)`.
  - Added backtick build branch at expression-build site (before `TernaryMiddle.isInvalid()` check): dispatches to `Actions.ActOnBacktickOperator(...)`.
- `clang/include/clang/Sema/Sema.h` — declared `ActOnBacktickOperator(Scope*, SourceLocation, Expr*, SourceLocation, Expr*, Expr*)` near `ActOnCallExpr`.
- `clang/lib/Sema/SemaExpr.cpp` — implemented `Sema::ActOnBacktickOperator`: builds a two-element `Expr*` array `{LHS, RHS}` and forwards to `BuildCallExpr(S, Op, OpenLoc, Args, CloseLoc)`.
- `clang/test/Parser/backtick-infix.cpp` — new lit test: AST-dump checks for basic desugar (`1 \`add\` 2` → `CallExpr` with callee `add`), left-associativity (`10 \`sub\` 3 \`mul\` 2` → outer `CallExpr` with callee `mul` wrapping inner with callee `sub`), qualified name in slot (`\`ns::f\``), and unary prefix on operands.
- `ops/PLAN.md` — S03 box checked, status log row added.

## Verification evidence

```
# Targeted lit test
llvm-lit -v clang/test/Parser/backtick-infix.cpp
→ PASS: Clang :: Parser/backtick-infix.cpp (1 of 1)

# End-to-end compile + run
echo 'int f(int a, int b) { return a + b; }
int main() { int r = 1 `f` 2; return r - 3; }' | clang -fbacktick -o /tmp/bt -x c++ - && /tmp/bt && echo exit 0
→ exit 0

# Left-assoc runtime
# g(f(1,2), 3) with f=sub, g=mul: (1-2)*3 = -3, main returns -3+3=0
→ exit 0

# AST dump confirms CallExpr with callee f and args 1, 2
→ CallExpr 'int' / DeclRefExpr 'f' / IntegerLiteral 1 / IntegerLiteral 2

# check-clang
52212 total (+1 vs S02): 46447 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline only): Clang :: Format/dump-config-objc-stdin.m
```

## Deviations from the plan / design

- **Expression-build discriminator:** Used `OpToken.is(tok::backtick)` as the discriminator at the build site (rather than `!BacktickOp.isInvalid()`) because `OpToken` is always available and cleanly separates backtick iterations from ternary/binary ones. `BacktickOp.isInvalid()` cannot distinguish "not a backtick" from "backtick parse failed" without a separate flag.
- **`getBinOpPrecedence` parameter:** Added `BacktickIsOperator` as a defaulted 4th parameter (`= true`) rather than a mandatory one. This avoids updating call sites outside `ParseRHSOfBinaryExpression` (the line-228 constraint-expression call and the `isFoldOperator` indirect call). All existing callers keep their correct `true` behavior without change.
- **`isFoldOperator` exclusion:** Added `Level != prec::Backtick` guard to `isFoldOperator(prec::Level)` so backtick cannot appear in fold expressions. The step file did not mention this; it is a necessary correctness fix since fold expressions are only for standard binary operators.

## Discoveries affecting later steps

- **Exact symbol names confirmed:**
  - `prec::Backtick` (value 16) in `clang/include/clang/Basic/OperatorPrecedence.h`
  - `BacktickIsOperator` (field in `Parser`, init `true`)
  - `BacktickIsOperatorScope` (in `RAIIObjectsForParser.h`)
  - `Sema::ActOnBacktickOperator(Scope*, SourceLocation OpenLoc, Expr* Op, SourceLocation CloseLoc, Expr* LHS, Expr* RHS)` in `SemaExpr.cpp`
- **D3 (bare nested backtick) already produces parse errors naturally.** With `BacktickIsOperator = false` inside the slot, `tok::backtick` gets `prec::Unknown`, so `ParseExpression` terminates at the first nested backtick. The slot parse then holds only `f`, the close backtick is found, and `g` is an unexpected token. The error is generic ("expected ';'"), not a specific D3 message — S04's job.
- **Empty slot (` `` `) behavior:** The suppressed-flag slot calls `ParseExpression()`. When the slot is empty (next token is `` ` ``), `ParseExpression` returns an invalid result. `LHS` becomes `ExprError()`. The current diagnostic is a generic "expected expression". S04 should add a targeted diagnostic.
- **`BuildCallExpr` carries SourceLocations for diagnostics.** `OpenLoc` and `CloseLoc` (the two backtick positions) are passed as the `LParenLoc`/`RParenLoc` analogs. This is already wired up and will give S11 the right location anchors for `-ast-print` round-tripping.

## Forward notes for the NEXT step (S04 — Diagnostics)

S04 adds three diagnostics in `DiagnosticParseKinds.td` and `-verify` tests. Concrete pointers:

### 1. New diagnostic IDs go in DiagnosticParseKinds.td
File: `clang/include/clang/Basic/DiagnosticParseKinds.td`
Add entries like:
```
def err_backtick_empty_operator_slot : Error<
    "expected expression between backticks">;
def err_backtick_unterminated : Error<
    "unterminated backtick operator; expected closing '%0'">;
def note_backtick_open : Note<"to match this '%0'">;
def err_backtick_nested_requires_parens : Error<
    "nested backtick operator in operator slot must be parenthesised">;
```

### 2. Where to fire them in ParseExpr.cpp

The backtick-middle block (in `ParseRHSOfBinaryExpression`) is where all three fire:

```cpp
if (OpToken.is(tok::backtick)) {
    BacktickIsOperatorScope BIS(BacktickIsOperator, false);
    // Case 1: empty slot — next token is already the closing backtick
    if (Tok.is(tok::backtick)) {
        Diag(Tok, diag::err_backtick_empty_operator_slot);
        LHS = ExprError();
    } else {
        BacktickOp = ParseExpression();
        if (BacktickOp.isInvalid())
            LHS = ExprError();
    }
    BacktickCloseLoc = Tok.getLocation();
    // Case 2: unterminated — ExpectAndConsume fires if close backtick missing
    if (ExpectAndConsume(tok::backtick, diag::err_backtick_unterminated))
        LHS = ExprError();
}
```

Case 3 (bare nested backtick) happens when `ParseExpression()` terminates early because the inner backtick has `prec::Unknown`. At that point, the backtick is still the current `Tok` when `ExpectAndConsume` is called. S04 should distinguish this from a generic unterminated case by checking: if the slot expression was parsed successfully but `Tok.is(tok::backtick)` still (i.e., the slot contained an expression followed by an unexpected backtick), emit the D3 parenthesisation diagnostic instead.

### 3. The `-verify` test skeleton

```cpp
// RUN: %clang_cc1 -fbacktick -fsyntax-only -verify %s
int f(int,int);
int a, b, c;
int x1 = a ``b;       // expected-error{{expected expression between backticks}}
int x2 = a `f b;      // expected-error{{unterminated backtick}}
int x3 = a `f `g` b`c; // expected-error{{nested backtick operator in operator slot must be parenthesised}}
```

## Open risks / TODOs

- S04's diagnostic for case 3 (nested backtick) requires careful detection: the slot expression parses up to `f`, then `\`g\`` starts — the `BacktickIsOperator=false` means the inner backtick gives `prec::Unknown`, so `ParseExpression` returns with `Tok` still being the backtick. The `ExpectAndConsume` then sees a backtick where it wasn't supposed to be. Adding a check `if (!LHS.isInvalid() && Tok.is(tok::backtick))` before `ExpectAndConsume` to emit the D3-specific message, then consuming the inner backtick sequence as error recovery, is the recommended approach.
- S05 and S06 can both start from S03 green (they have `dep: S03` in PLAN.md).
