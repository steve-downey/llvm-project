# Handoff — S11 AST wrapper for `-ast-print` fidelity

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ 4243342f5438
- **Date / agent:** 2026-06-27

## What changed

26 files, 153 insertions, 1 deletion.

### Core node definition
- `clang/include/clang/Basic/StmtNodes.td` — `def BacktickInfixExpr : StmtNode<Expr>;` (after ParenExpr)
- `clang/include/clang/AST/Expr.h` — class `BacktickInfixExpr : public Expr` with `Stmt *Inner`, two constructors (normal + EmptyShell), `getSubExpr()`/`setSubExpr()`, `getBeginLoc()`/`getEndLoc()`, `classof()`, `children()` (before UnaryOperator)

### Dependence computation
- `clang/include/clang/AST/ComputeDependence.h` — forward decl `class BacktickInfixExpr;` + decl `ExprDependence computeDependence(BacktickInfixExpr *E);`
- `clang/lib/AST/ComputeDependence.cpp` — delegates to `E->getSubExpr()->getDependence()`

### Serialization (PCH/modules)
- `clang/include/clang/Serialization/ASTBitCodes.h` — `EXPR_BACKTICK_INFIX` enum value after `EXPR_PAREN`
- `clang/lib/Serialization/ASTReaderStmt.cpp` — `VisitBacktickInfixExpr` reads subexpr; allocation case `EXPR_BACKTICK_INFIX`
- `clang/lib/Serialization/ASTWriterStmt.cpp` — `VisitBacktickInfixExpr` writes subexpr, sets `Code = EXPR_BACKTICK_INFIX`

### Pretty printing (S11's key feature)
- `clang/lib/AST/StmtPrinter.cpp` — `VisitBacktickInfixExpr` extracts inner `CallExpr`, prints `LHS \`callee\` RHS`. Callee goes through `VisitImplicitCastExpr` → `VisitDeclRefExpr` which prints the function name.

### AST visitors
- `clang/include/clang/AST/RecursiveASTVisitor.h` — `DEF_TRAVERSE_STMT(BacktickInfixExpr, {})` (children traversal is automatic via `children()`)
- `clang/lib/AST/StmtProfile.cpp` — `VisitBacktickInfixExpr` calls `VisitExpr(S)` (children visited automatically)

### Expression classification (exhaustive switch — had `llvm_unreachable` at end)
- `clang/lib/AST/ExprClassification.cpp` — `case BacktickInfixExprClass:` recurses into `getSubExpr()` (after ParenExprClass)
- `clang/lib/AST/ExprConstant.cpp` — two sites:
  1. `ExprEvaluatorBase::VisitBacktickInfixExpr` delegates to `Visit(E->getSubExpr())`
  2. `CheckICE` case `BacktickInfixExprClass` delegates to `CheckICE(subexpr)` (exhaustive switch)
- `clang/lib/AST/ByteCode/Compiler.h` + `.cpp` — `VisitBacktickInfixExpr` calls `this->delegate(E->getSubExpr())`

### Mangling (exhaustive switch with `goto recurse`)
- `clang/lib/AST/ItaniumMangle.cpp` — `case BacktickInfixExprClass: E = getSubExpr(); goto recurse;`

### Expr utilities
- `clang/lib/AST/Expr.cpp` — three exhaustive switches:
  - `isUnusedResultAWarning`: delegates to subexpr
  - `isConstantInitializer`: delegates to subexpr
  - `HasSideEffects`: added to fallthrough list alongside `ParenExprClass`

### AST import
- `clang/lib/AST/ASTImporter.cpp` — `VisitBacktickInfixExpr` imports subexpr and wraps in new `BacktickInfixExpr`

### Exception spec (exhaustive switch — had `llvm_unreachable` at end)
- `clang/lib/Sema/SemaExceptionSpec.cpp` — `case BacktickInfixExprClass:` delegates to `canThrow(subexpr)` (before CallExprClass)

### Sema (wrapping site)
- `clang/lib/Sema/SemaExpr.cpp` — `ActOnBacktickOperator` now wraps the `BuildCallExpr` result in `new (Context) BacktickInfixExpr(Call.get())`

### Tree transform (templates)
- `clang/lib/Sema/TreeTransform.h` — `TransformBacktickInfixExpr` transforms subexpr and wraps in new `BacktickInfixExpr`

### CodeGen (all forward to subexpr)
- `clang/lib/CodeGen/CGExprScalar.cpp` — `VisitBacktickInfixExpr` → `Visit(E->getSubExpr())`
- `clang/lib/CodeGen/CGExprComplex.cpp` — `VisitBacktickInfixExpr` → `Visit(E->getSubExpr())`
- `clang/lib/CodeGen/CGExprAgg.cpp` — `VisitBacktickInfixExpr` → `Visit(E->getSubExpr())`
- `clang/lib/CodeGen/CGExprConstant.cpp` — `VisitBacktickInfixExpr` → `Visit(E->getSubExpr(), T)`
- `clang/lib/CodeGen/CGExpr.cpp` — four sites:
  1. `EmitLValue` switch: `BacktickInfixExprClass` delegates to subexpr (after ParenExprClass)
  2. `LValueBaseVisitor::VisitBacktickInfixExpr` → `Visit(E->getSubExpr())`
  3. `StructFieldAccess::VisitBacktickInfixExpr` → `Visit(E->getSubExpr())`
  4. ObjC GC: `if (auto *Exp = dyn_cast<BacktickInfixExpr>(E)) { ...; return; }` (before ParenExpr check)

### Tests
- `clang/test/Parser/backtick-infix.cpp` — added `AST-NEXT: BacktickInfixExpr {{.*}} 'int'` for chain test (the outermost `mul` call's first argument is now a `BacktickInfixExpr`, not a bare `CallExpr`)
- `clang/test/Parser/backtick-ast-print.cpp` — NEW: two-command round-trip test; first command checks backtick syntax is emitted, second re-parses the printed output

## Verification evidence

```
# Targeted backtick tests (4 tests)
/home/sdowney/src/llvm/build-backtick/bin/llvm-lit -v \
  clang/test/Parser/backtick-infix.cpp \
  clang/test/Parser/backtick-precedence.cpp \
  clang/test/Parser/backtick-escape.cpp \
  clang/test/Parser/backtick-ast-print.cpp
→ PASS: all 4 (Testing Time: 0.09s)

# Full check-clang gate
→ PASS — Total: 52224, Passed: 46459, xfail: 26, unsupported: 5732, skipped: 6,
  failed: 1 (env-only: Format/dump-config-objc-stdin.m, pre-existing)
```

## Deviations from the plan / design

1. **Backtick source locations not stored in wrapper (DEV-05):** The step says "remembers the two backtick locations," implying dedicated `SourceLocation` fields in `BacktickInfixExpr`. These are NOT stored separately. The backtick positions are already in the inner `CallExpr`'s `LParen`/`RParen` fields (passed as `OpenLoc`/`CloseLoc` to `BuildCallExpr`). The pretty-printer reconstructs syntax from the `CallExpr` structure, not from source locations, which is sufficient for `-ast-print` round-trip. See DEV-05 in `ops/DEVIATIONS.md`.

2. **AST dump is free:** The `-ast-dump` output for `BacktickInfixExpr` uses the generic class-name + type + children mechanism — no `VisitBacktickInfixExpr` needed in `ASTDumper`. This is correct behavior for dump (it should show the wrapper node), but worth knowing: dump shows `BacktickInfixExpr` as a node, print emits `` lhs `op` rhs ``.

3. **`IgnoreParens()` not extended:** `IgnoreParens()` was NOT extended to see through `BacktickInfixExpr` — it is not a paren-like no-op. Semantic code that wants the inner call should cast explicitly if needed.

## Discoveries affecting later steps

### GCC track (S12+)
- The Clang wrapper pattern uses 22 explicit call sites across AST/Sema/CodeGen. GCC's equivalent (if a phase-2 wrapper is added to the GCC track) would need similar coverage in GCC's gimplification and diagnostic paths. Document those equivalents in `ops/gcc/DEVIATIONS.md`.
- The key insight: wherever Clang has an exhaustive switch over `Stmt::StmtClass` with `llvm_unreachable` at the end (ExprClassification, ExprConstant::CheckICE, canThrow, ItaniumMangle), a new expr class MUST add a case or the binary crashes at runtime. GCC uses a different mechanism (tree codes + gimplification), so the same exhaustive-switch pitfall does not apply.

### Round-trip fidelity note
- `VisitImplicitCastExpr` in `StmtPrinter.cpp` is transparent (just forwards), so `PrintExpr(CE->getCallee())` prints the DeclRefExpr name directly. If callee were a more complex expression (e.g., a member function or namespace-qualified name), the qualified name would be printed correctly too.

### Serialization cost
- 2 files (ASTReaderStmt.cpp, ASTWriterStmt.cpp) + 1 enum value (ASTBitCodes.h). The serialization cost is minimal. The ABI of ASTBitCodes is stable only within a toolchain version, not across, so adding `EXPR_BACKTICK_INFIX` after `EXPR_PAREN` is safe.

## Forward notes for the NEXT step (S12 — GCC baseline)

S12 is independent of the Clang AST work. It stands up the GCC worktree + baseline.

- **GCC root:** Find the GCC git checkout first (`find /home/sdowney/src -name "*.c" -path "*/gcc/gcc.c" 2>/dev/null | head -3` or similar to locate it).
- **Branch pattern:** `git worktree add -b backtick ../gcc-backtick HEAD` from the GCC git root.
- **Build config:** `--enable-languages=c,c++ --disable-bootstrap --disable-multilib` into a separate build dir.
- **Build target:** `make all-gcc` stops at cc1plus, faster than full `make`.
- **Baseline gate:** `make -C gcc check-c++ RUNTESTFLAGS="dg.exp=*"` — record total/pass/fail counts.
- **Sub-plan:** `ops/gcc/PLAN.md` already exists with G01–G09. Handoffs go in `ops/gcc/handoffs/`.
- **Key reference for G-agents:** `§8` and `§5` (`greater_than_is_operator_p` analog in GCC).

## Open risks / TODOs

- `err_backtick_nested_requires_parens` in `DiagnosticParseKinds.td` remains dead code (DEV-04 still unresolved).
- DEV-04 (bare nested backtick silently mis-parses) still unaddressed.
- DEV-05: If fine-grained source location per backtick token is later needed (e.g., for an error "expected closing backtick here"), add `SourceLocation BacktickLocs[2]` fields to `BacktickInfixExpr` and thread them from `ActOnBacktickOperator`.
- S11 `backtick-ast-print.cpp` does not test a template context. A future test with `template<typename F> auto apply(F f, int a, int b) { return a \`f\` b; }` would verify that `TreeTransform<Derived>::TransformBacktickInfixExpr` and serialization work correctly for dependent expressions.
