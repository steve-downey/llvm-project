# Handoff — S05 Semantics test sweep

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `08ea70ad08ed`
- **Date / agent:** 2026-06-26

## What changed

- `clang/test/SemaCXX/backtick-semantics.cpp` — new test file covering all
  six items from the step spec:
  1. Overload resolution (int vs double overload), verified by `static_assert`
     on the result type.
  2. Qualified callee `tx \`ns::g\` ty` — namespace-qualified name in slot.
  3. Templates — `apply<F,A>(a, b, func)` uses `a \`func\` b` in its body;
     instantiated with both a lambda and a named function.
  4. `constexpr` — `static_assert(1 \`add_cx\` 2 == 3)` and a chained variant.
  5. CodeGen — `codegen_fn()` is FileChecked to confirm a `call @_Z6add_cxii`
     appears in the emitted IR.
  6. Value categories — `int& ref_f(int&, int&)` used as callee; result bound
     to `int&` must compile.
  Also added: lambda in the operator slot (`3 \`[](int a, int b){...}\` 4`).
- `ops/PLAN.md` — S05 box checked, status log row added.

## Verification evidence

```
# Targeted lit test
llvm-lit -v clang/test/SemaCXX/backtick-semantics.cpp
→ PASS: Clang :: SemaCXX/backtick-semantics.cpp (1 of 1)

# check-clang
52214 total (+1 vs S04): 46449 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline): Clang :: Format/dump-config-objc-stdin.m
```

## Deviations from the plan / design

None. No code changes were needed in S03's area. All six semantic cases work
exactly as desugaring to `BuildCallExpr` implies.

The `add_cx` function used for the constexpr and CodeGen tests is `constexpr
int` (not `inline`), which suffices for both. Both RUN lines (syntax-only and
emit-llvm) exercise the same translation unit — FileCheck scans the IR output,
`-verify` scans for `expected-no-diagnostics`.

## Discoveries affecting later steps

- **Lambda in the slot works out of the box.** `BacktickIsOperator=false`
  causes `ParseExpression` to parse the full lambda body (including `{}`
  which `BalancedDelimiterTracker` handles) before returning. No special
  handling needed.
- **Value-category round-trip confirmed.** `BuildCallExpr` uses the callee's
  declared return type, so an `int&`-returning function produces an lvalue
  result that can bind to `int&`. This is free — no extra work needed.
- **Template instantiation confirmed.** `TreeTransform::TransformCallExpr`
  handles the desugared `CallExpr` in a template body. `apply<>` compiled
  and ran correctly with both a lambda and a named function.
- **CodeGen FileCheck pattern.** `call {{.*}}@_Z6add_cxii` is the right idiom
  for matching a direct call in the IR. The `{{.*}}` absorbs attributes.

## Forward notes for the NEXT step (S06 — Precedence/associativity test sweep)

S06 is also test-only. Concrete guidance:

### Test file location
Add `clang/test/Parser/backtick-precedence.cpp` (parser-level, not SemaCXX,
because these tests exercise AST structure via `-ast-dump`). Use:
```
// RUN: %clang_cc1 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
```

### Verified AST shapes (from a probe run against S05's build)

**`-x \`f\` -y` → `f(-x, -y)` (D2 confirmed, Option A):**
The AST shows `CallExpr → [UnaryOperator '-', UnaryOperator '-']`. Both
minus signs are inside the call args, not outside.
```
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
// AST: UnaryOperator {{.*}} prefix '-'
// AST: UnaryOperator {{.*}} prefix '-'
int r1 = -1 `f` -2;
```

**`a * b \`f\` c` → `a * f(b, c)` (tighter than `*`):**
AST: `BinaryOperator '*' [IntegerLiteral 2, CallExpr f(3,4)]`
```
// AST: BinaryOperator {{.*}} '*'
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
int r2 = 2 * 3 `f` 4;
```

**`a \`f\` b \`g\` c` → `g(f(a,b), c)` (left-associative):**
AST: `CallExpr g [CallExpr f(1,2), 3]`
```
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'g'
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
int r3 = 1 `f` 2 `g` 3;
```

**`a ? b : c \`f\` d` → `a ? b : f(c,d)` (ternary: backtick in else-branch):**
AST: `ConditionalOperator [1, 2, CallExpr f(3,4)]`
```
// AST: ConditionalOperator
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
int r4 = 1 ? 2 : 3 `f` 4;
```

**`a.b \`f\` c.d` — member access operands:**
No special handling; `a.b` is a postfix-expression (higher-prec than backtick)
so it folds into the LHS operand naturally.

### Potential surprises for S06
- The step mentions `-ast-dump` / FileCheck. The existing
  `clang/test/Parser/backtick-infix.cpp` already checks some of these; S06
  should extend or add a new file without duplicating what's there.
- `a ? b : c \`f\` d` needs to confirm that the ConditionalOperator's
  else-branch is `f(c,d)`, not that the entire conditional is the LHS of
  backtick. The probe confirmed the former.
- There is no `a.b \`f\` c.d` test in the existing suite; it's worth adding.

## Open risks / TODOs

- DEV-04 (bare nested backtick silently mis-parses as a chain) remains
  unaddressed. S06 can add a test pinning the current behavior if desired,
  but should note it will need updating once DEV-04 is fixed.
- `err_backtick_nested_requires_parens` in DiagnosticParseKinds.td is still
  dead code (carried from S04).
