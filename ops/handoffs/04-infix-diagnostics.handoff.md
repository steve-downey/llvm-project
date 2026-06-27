# Handoff — S04 Diagnostics + nested-paren rule

- **Status:** DONE (gate passed, with deviation on D3 — see DEV-04)
- **Branch / commit:** `backtick` @ `d9e633069702`
- **Date / agent:** 2026-06-26

## What changed

- `clang/include/clang/Basic/DiagnosticParseKinds.td` — added three new diagnostics after `ext_gnu_conditional_expr`:
  - `err_backtick_empty_slot`: "expected expression between backticks"
  - `err_backtick_unterminated`: "missing closing backtick for infix operator"
  - `err_backtick_nested_requires_parens`: "nested backtick infix operator in operator slot must be parenthesised (e.g., 'x `(f `g` h)` y')" — added but NOT yet fired (see Deviations).
- `clang/include/clang/Parse/RAIIObjectsForParser.h` — `BalancedDelimiterTracker` now also inherits from `BacktickIsOperatorScope` (restores `BacktickIsOperator = true` inside any `()`/`[]`/`{}`). This enables D3 parenthesised nesting: `a \`(f \`g\` h)\` b` works because the inner `(` restores the flag.
- `clang/lib/Parse/ParseExpr.cpp` — reworked the backtick-middle block:
  - Detects empty slot (`Tok.is(tok::backtick)` before `ParseExpression`) → `err_backtick_empty_slot`.
  - Consumes the close backtick via explicit `ConsumeToken()` (replacing `ExpectAndConsume`) so missing-close can be detected separately.
  - Detects missing close (`!Tok.is(tok::backtick)` and no prior error) → `err_backtick_unterminated` + `note_matching` pointing to the open backtick location.
  - The broken "nested check" (`else if (Tok.is(tok::backtick))` after `ParseExpression`) was intentionally NOT included — see Deviations.
- `clang/test/Parser/backtick-diagnostics.cpp` — new `-verify` lit test covering empty slot (x1), unterminated (x2), parenthesised nesting is well-formed (x3, x4).
- `ops/PLAN.md` — S04 box checked, status log row added.
- `ops/DEVIATIONS.md` — DEV-04 added.

## Verification evidence

```
# Targeted lit tests
llvm-lit -v clang/test/Parser/backtick-diagnostics.cpp clang/test/Parser/backtick-infix.cpp
→ PASS: Clang :: Parser/backtick-diagnostics.cpp (1 of 2)
→ PASS: Clang :: Parser/backtick-infix.cpp (2 of 2)

# check-clang
52213 total (+1 vs S03): 46448 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline only): Clang :: Format/dump-config-objc-stdin.m
```

## Deviations from the plan / design

**DEV-04 (logged in ops/DEVIATIONS.md):** Step file says bare nested backtick `x \`f \`g\` h\` y` "naturally produces a parse error" because `BacktickIsOperator=false` makes the inner backtick have `prec::Unknown`. This is wrong: `prec::Unknown` causes `ParseExpression` to stop at the inner backtick, returning just `f` as the slot. The inner backtick is then consumed as the *close* backtick, and the parse continues as two chained operators — `h(f(x,g),b)`. No error, just silently wrong.

The naive fix (`else if (Tok.is(tok::backtick))` after `ParseExpression`) also fires on the NORMAL case `x \`f\` y` because the close backtick is also `Tok.is(tok::backtick)` after `ParseExpression` returns. The check cannot distinguish "close backtick" from "unexpected inner backtick" without lookahead.

`err_backtick_nested_requires_parens` is defined but not fired. Implementing D3 enforcement requires either (a) one token of lookahead past the apparent close backtick to determine if the structure is a nested chain, or (b) a two-pass approach. Deferred to a future step.

## Discoveries affecting later steps

- **Diagnostic IDs confirmed:**
  - `diag::err_backtick_empty_slot`
  - `diag::err_backtick_unterminated`
  - `diag::err_backtick_nested_requires_parens` (defined, not yet fired)
- **`BalancedDelimiterTracker` restores `BacktickIsOperator = true`** in any `()`, `[]`, `{}`. This means S09/S10 (escape identifier parsing) and S11 (AST wrapper) need not worry about the flag being wrong inside paren contexts.
- **`note_matching << tok::backtick`** produces the message "to match this '`'". S09's diagnostics can reuse the same `note_matching` idiom.
- **`err_backtick_nested_requires_parens` is NOT fired.** S05 and S06 can test `x \`f \`g\` h\` y` to document the current behavior (silently parses as `h(f(x,g),b)`). If a future step addresses DEV-04, those tests should be updated.

## Forward notes for the NEXT step (S05 — Semantics test sweep)

S05 is test-only — no parser/sema code changes expected. Concrete pointers:

### 1. Test file location
Add `clang/test/SemaCXX/backtick-semantics.cpp` (analogous pattern to other SemaCXX tests). Use `%clang_cc1 -fbacktick -fsyntax-only -verify` for error checks; use `%clang_cc1 -fbacktick -emit-llvm -o - | FileCheck` for CodeGen checks.

### 2. What to cover and how

**Overload resolution (item 1):**
```cpp
int f(int, int);
int f(double, double);
// expected: selects int overload
int r1 = 1 `f` 2;
static_assert(__builtin_types_compatible_p(typeof(r1), int));
```

**ADL (item 2):** Put `f` in a namespace with a type from that namespace:
```cpp
namespace ns {
  struct T {};
  int f(T, T);
}
ns::T x, y;
int r_adl = x `ns::f` y; // qualified — also test ADL: if f is found by ADL...
// Simpler: just test qualified name works
int r_qual = x `ns::f` y;
```
ADL fires when callee is unqualified and args are from a namespace — but the callee in the slot is explicitly written, so ADL applies to the SLOT expression lookup, not the final call. Actually, `BuildCallExpr` with an already-resolved callee expr won't re-do ADL for the callee. Test that qualified names work.

**Templates (item 3):**
```cpp
template <typename T, typename F>
auto apply(T a, T b, F func) { return a `func` b; }
// instantiate:
int r_tmpl = apply(1, 2, [](int a, int b){ return a + b; });
```
This tests `TreeTransform::TransformCallExpr` handles the desugared call in a template.

**constexpr (item 4):**
```cpp
constexpr int add(int a, int b) { return a + b; }
static_assert(1 `add` 2 == 3);
```

**CodeGen (item 5):** `-emit-llvm` should show a direct call instruction. Use FileCheck on the IR.

**Value categories (item 6):**
```cpp
int& ref_f(int&, int&);
int x = 1, y = 2;
int& r_ref = x `ref_f` y; // result is lvalue
```

### 3. Potential surprises

- Lambda in the slot: `1 \`[](int a, int b){ return a+b; }\` 2` — the lambda is a primary expression, and `ParseExpression()` with `BacktickIsOperator=false` will parse the lambda body. This should work. Worth testing.
- Member function pointer in slot: `obj \`&Cls::method\`..` — `&Cls::method` is not directly callable without an object; `BuildCallExpr` should reject this with the same error as `(&Cls::method)(obj, x)`. No special handling needed.
- Dependent types in templates: if `func` in the template above is a dependent type, `BuildCallExpr` defers to instantiation time. The call will be a `CallExpr` with a dependent callee — `TransformCallExpr` handles this automatically.

## Open risks / TODOs

- DEV-04 (nested backtick mis-parse) is unfixed. S05/S06 should note the current behavior in tests to prevent silent regression once it's fixed.
- `err_backtick_nested_requires_parens` in DiagnosticParseKinds.td is dead code. Consider removing it if it can't be fired cleanly, or keeping it for a future D3 enforcement pass.
