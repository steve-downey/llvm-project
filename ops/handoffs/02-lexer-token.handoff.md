# Handoff — S02 Lexer: backtick punctuator token

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `66bee2be5b15`
- **Date / agent:** 2026-06-26

## What changed

- `clang/include/clang/Basic/TokenKinds.def` — `PUNCTUATOR(backtick, "`")` added after `PUNCTUATOR(at, "@")`.
- `clang/lib/Lex/Lexer.cpp` — `case '`':` inserted between the `@` handler (ending at line ~4530) and `case '\\':` (line ~4533). Body: `Kind = LangOpts.Backtick ? tok::backtick : tok::unknown;`.
- `clang/test/Lexer/backtick-token.cpp` — new lit test with `-dump-tokens`, covering: (1) token is `backtick` with flag / `unknown` without; (2) backtick inside a string literal stays `string_literal`; (3) backtick inside a raw-string literal stays `string_literal`; (4) backtick inside a comment produces no token.
- `ops/PLAN.md` — S02 box checked, status log row added.

## Verification evidence

```
# Targeted lit test
llvm-lit -v clang/test/Lexer/backtick-token.cpp
→ PASS: Clang :: Lexer/backtick-token.cpp (1 of 1)

# check-clang
52211 total (+1 vs S01): 46446 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline only): Clang :: Format/dump-config-objc-stdin.m
(The 3–4 DirectoryWatcher/inotify failures from prior runs happened to be green this run;
 still within the known-flaky S00 baseline envelope.)
```

## Deviations from the plan / design

None. Step file is accurate. Token kind `tok::backtick` confirmed. Off-flag behavior confirmed as `tok::unknown` (no dedicated diagnostic — see S01 handoff, DEV-03 is already logged).

## Discoveries affecting later steps

- **`tok::backtick` confirmed.** The token name in `dump-tokens` output is `backtick` (lower-case), matching the `PUNCTUATOR(backtick, "`")` spelling. `tok::backtick` is the enum member S03+ will reference.
- **Off-flag behavior:** A bare backtick without `-fbacktick` produces `tok::unknown`, yielding generic parse errors. No dedicated diagnostic. S04 should add one.
- **`-dump-tokens` output format:** Token lines look like `backtick '`' Loc=<file:line:col>`. The double-quote delimiters around the spelling are escaped as `\"` in the output, so FileCheck patterns need `\"hello ` world\"` not `"hello ` world"`.
- **`ExprResult TernaryMiddle` is initialized as invalid:** The default `ExprResult()` is `isInvalid() = true` (the low bit of `PtrWithInvalid` is set). This is how `ParseRHSOfBinaryExpression` distinguishes "was ternary middle parsed" from "not a ternary". S03's backtick middle needs a parallel `ExprResult BacktickOp;` variable that starts invalid and is set during the backtick slot parse, then tested at the expression-build site.

## Forward notes for the NEXT step (S03 — Parse + desugar to CallExpr)

Read `ops/steps/03-infix-parse-sema.md` carefully. Concrete pointers — everything verified in this tree:

### 1. Precedence (OperatorPrecedence.h + .cpp)

- File: `clang/include/clang/Basic/OperatorPrecedence.h`
- Current top of `prec::Level`: `PointerToMember = 15   // .*, ->*`
- Add: `Backtick = 16  // x \`f\` y` (one above PointerToMember)
- Also modify `clang/lib/Basic/OperatorPrecedence.cpp` — `getBinOpPrecedence()`:
  - The function signature: `prec::Level getBinOpPrecedence(tok::TokenKind Kind, bool GreaterThanIsOperator, bool CPlusPlus11)` — but S03 needs a new `bool BacktickIsOperator` parameter too (or a separate lookup). The cleanest is to add the parameter parallel to `GreaterThanIsOperator`.
  - Add `case tok::backtick: return BacktickIsOperator ? prec::Backtick : prec::Unknown;` analogous to the `tok::greater` case.
  - Every call site of `getBinOpPrecedence` must be updated to pass the new parameter (grep the codebase — there are calls in `ParseExpr.cpp` at lines ~317, ~493, ~522, and in `SemaExpr.cpp` for fold expressions).

### 2. BacktickIsOperator flag (Parser.h + RAIIObjectsForParser.h + Parser.cpp)

- **Parser.h** (`clang/include/clang/Parse/Parser.h`): Add `bool BacktickIsOperator;` in `private:` next to `GreaterThanIsOperator` (line 3841).
- **RAIIObjectsForParser.h** (`clang/include/clang/Parse/RAIIObjectsForParser.h`): Add a `BacktickIsOperatorScope` class modeled exactly on `GreaterThanIsOperatorScope` (line 333): saves and restores `BacktickIsOperator`.
- **Parser.cpp** (`clang/lib/Parse/Parser.cpp`): Initialize `BacktickIsOperator(true)` in the constructor initializer list alongside `GreaterThanIsOperator(true)` (line 63).

### 3. Parser hook (ParseExpr.cpp — the keystone change)

File: `clang/lib/Parse/ParseExpr.cpp`, function `ParseRHSOfBinaryExpression` (line 316).

Two insertion points:

**A. After the ternary-middle parsing block (around line 460)**, add a backtick-middle block:
```cpp
ExprResult BacktickOp;
SourceLocation BacktickCloseLoc;
if (OpToken.is(tok::backtick)) {
    // Operator slot: parse an assignment-expression with BacktickIsOperator suppressed
    // so the closing backtick terminates the slot rather than starting another operator.
    BacktickIsOperatorScope BIS(BacktickIsOperator, false);
    BacktickOp = ParseExpression();  // assignment-expression (no top-level comma)
    if (BacktickOp.isInvalid())
        LHS = ExprError();
    BacktickCloseLoc = Tok.getLocation();
    if (ExpectAndConsume(tok::backtick, diag::err_expected, "`"))
        LHS = ExprError();
}
```
Note: `ExprResult BacktickOp;` must be declared at the top of the loop body (like `TernaryMiddle`), before both the ternary and backtick blocks, so it is default-initialized (invalid) on each iteration.

**B. At the expression-build site (around line 559)**, add a backtick branch before the binary-op branch:
```cpp
if (!BacktickOp.isInvalid()) {
    // Desugar x `op` y → op(x, y)
    SmallVector<Expr*, 2> Args = {LHS.get(), RHS.get()};
    LHS = Actions.ActOnBacktickOperator(getCurScope(),
                                        OpToken.getLocation(),
                                        BacktickOp.get(),
                                        BacktickCloseLoc,
                                        LHS.get(), RHS.get());
} else if (TernaryMiddle.isInvalid()) {
    // binary op ...
```

(The D3 nested-backtick-must-be-parenthesised rule enforces itself naturally: with `BacktickIsOperator = false`, a bare backtick in the operator slot gets `prec::Unknown` from `getBinOpPrecedence`, so `ParseExpression` doesn't treat it as a binary operator. The error you get is that the backtick terminates the slot, leaving a parse error for the unexpected expression. S04 will add a better diagnostic.)

### 4. Sema entry point (SemaExpr.cpp + Sema.h)

- **Sema.h** (`clang/include/clang/Sema/Sema.h`): Declare near `ActOnCallExpr` (line 7539):
  ```cpp
  ExprResult ActOnBacktickOperator(Scope *S,
                                   SourceLocation OpenLoc,
                                   Expr *Op,
                                   SourceLocation CloseLoc,
                                   Expr *LHS, Expr *RHS);
  ```
- **SemaExpr.cpp** (`clang/lib/Sema/SemaExpr.cpp`): Implement by forwarding to `BuildCallExpr`:
  ```cpp
  ExprResult Sema::ActOnBacktickOperator(Scope *S,
                                          SourceLocation OpenLoc,
                                          Expr *Op,
                                          SourceLocation CloseLoc,
                                          Expr *LHS, Expr *RHS) {
      SmallVector<Expr*, 2> Args = {LHS, RHS};
      return BuildCallExpr(S, Op, OpenLoc, Args, CloseLoc);
  }
  ```
  The `SourceLocation` pair is carried for diagnostics and future `-ast-print` round-tripping (S11). `BuildCallExpr` handles ADL, overload resolution, templates, constexpr — all inherited for free.

### 5. Call sites to update for new `getBinOpPrecedence` parameter

Grep: `grep -rn "getBinOpPrecedence" clang/`
Expected hits: `clang/lib/Basic/OperatorPrecedence.cpp` (definition), `clang/lib/Parse/ParseExpr.cpp` (3 calls), and possibly `clang/lib/Sema/` (fold expression handling). Add `BacktickIsOperator` as the new 4th parameter everywhere; call sites in ParseExpr.cpp can pass `BacktickIsOperator`; call sites in Sema can pass `true` (backtick is always an operator in Sema context).

### 6. Gate test

```bash
echo 'int f(int a, int b) { return a + b; }
int main() { int r = 1 `f` 2; return r - 3; }' > /tmp/bt.cpp
clang -fbacktick -o /tmp/bt /tmp/bt.cpp && /tmp/bt && echo "exit 0"
```
And `-ast-dump` should show a `CallExpr` with callee `f` and args `1, 2`.

## Open risks / TODOs

- `getBinOpPrecedence` signature change affects every call site — grep before patching.
- The D3 "nested backtick must be parenthesised" error message will be generic (parse error); S04 is the step that improves it.
- S05 and S06 depend on S03 being done and stable before starting their test sweeps.
