# S03 — Parse the infix operator and desugar to a call (keystone)

**Goal.** Make `x `f` y` compile and run identically to `f(x, y)`,
left-associative, highest-precedence binary. End-to-end, desugar-only.

**Depends on:** S02.
**Design refs:** §2; §3 D1/D2/D4/D6; §4 Option A; §5 (terminator flag); §6
steps 2–4.

## Do
1. **Precedence.** Add a new top level above `PointerToMember` in
   `clang/include/clang/Basic/OperatorPrecedence.h` (e.g. `Backtick`). Make
   `getBinOpPrecedence(tok::backtick, ...)` return it.
2. **Terminator flag.** Add a parser flag analogous to
   `GreaterThanIsOperator` (call it `BacktickIsOperator`) plus an RAII scope
   like `GreaterThanIsOperatorScope`. While parsing the operator slot the
   flag is false (so a `` ` `` terminates the slot); inside nested
   parens/brackets it is restored to true (so D3 parenthesised nesting
   works). Have `getBinOpPrecedence` return `Unknown` for `tok::backtick`
   when the flag is false.
3. **Parser hook.** In `ParseRHSOfBinaryExpression`
   (`clang/lib/Parse/ParseExpr.cpp`), add a `tok::backtick` branch modeled
   on the `tok::question` ternary branch: consume open `` ` ``; with the
   flag suppressed, `ParseExpression()` (assignment-expression — no
   top-level comma) for the operator slot; expect close `` ` ``; parse the
   RHS operand via `ParseCastExpression`; continue the left-associative
   loop.
4. **Sema.** Add an entry point (e.g. `Sema::ActOnBacktickOperator`) that
   forwards to `BuildCallExpr` with callee = operator-slot expr, args =
   {LHS, RHS}, carrying the two backtick `SourceLocation`s. Do **not** add a
   new AST node (that's S11). The result is a plain `CallExpr`.

## Build
`ninja -C build clang`

## Verify (gate)
- `clang -fbacktick -Xclang -ast-dump -fsyntax-only` on `x `f` y` shows a
  `CallExpr` with callee `f` and args `x, y`.
- A runnable lit test (or `clang -fbacktick` + execute) where
  `int f(int,int)` and `a `f` b` returns `f(a,b)`.
- Left-assoc: `a `f` b `g` c` dumps as `g(f(a,b), c)`.
- `check-clang` green.

## Done when
The three checks pass and the off-flag build is unchanged.

## Capture in handoff
Exact name/signature of the Sema entry point and the precedence enumerator
(S05–S07, S11 all reference them). Note how the RAII scope is named and
where it must be re-armed (which sub-parses restore the flag).

## Pitfalls
- Don't let the operator slot's `ParseExpression` consume the closing
  backtick — that's the whole point of the suppression flag; test
  `a `f` b `g` c` early.
- RHS must be `ParseCastExpression` (not assignment-expression) to keep
  left-associativity and let the next backtick be seen by the outer loop.
