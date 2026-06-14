# S11 — Thin AST wrapper for `-ast-print` fidelity (phase 2)

**Goal.** Make `-ast-print` round-trip backtick syntax, additively, without
changing semantics.

**Depends on:** S06 (stable infix). Independent of B/C.
**Design refs:** §3 D7; §11 phase 2.

## Do
1. Add a thin, transparent expression node that wraps the desugared
   `CallExpr` and remembers the two backtick locations. It must **delegate**
   type, value category, constant evaluation, codegen, and instantiation to
   the wrapped call.
2. Teach the mechanical sites: AST serialization (PCH/modules), the
   visitors (`StmtVisitor`/`RecursiveASTVisitor`), `TreeTransform`, and
   ASTContext allocation. CodeGen and constant-eval just forward.
3. Have the pretty-printer emit `lhs `op` rhs` for the wrapper.
4. S03's Sema entry now returns the wrapper around the call.

## Build / Verify (gate)
- `-ast-print` on `x `f` y` re-emits backtick syntax and re-parses to the
  same AST (round-trip test).
- Re-run S05/S06 — semantics unchanged.
- `check-clang` green.

## Done when
Round-trip works and nothing semantic moved.

## Capture in handoff
Note that this is the one phase that touches serialization/visitors — record
each site changed so the GCC track and the paper's "implementation cost"
section are accurate.

## Pitfalls
Keep it transparent: if any semantic test changes behavior, the wrapper
leaked — fix the delegation, don't adjust the test.
