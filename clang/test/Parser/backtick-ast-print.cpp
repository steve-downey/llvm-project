// RUN: %clang_cc1 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT
// RUN: %clang_cc1 -fbacktick -ast-print %s 2>&1 | %clang_cc1 -fbacktick -fsyntax-only -x c++ -

// S11: -ast-print round-trip for backtick infix expressions.
// The pretty-printer must re-emit the backtick form; re-parsing must succeed.

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

// PRINT: {{.*}}`add`{{.*}}
int basic = 1 `add` 2;

// PRINT: {{.*}}`sub`{{.*}}
int chain = 10 `sub` 3;

// A class-typed result with a non-trivial destructor. Sema wraps the call it
// built in a CXXBindTemporaryExpr, so BacktickInfixExpr's subexpression is
// *not* the CallExpr -- the printer used to cast<CallExpr>() it and crash.
struct Res {
  int v;
  ~Res();
};
Res mk(int a, int b);

// PRINT: {{.*}}1 `mk` 2{{.*}}
void discarded() { (void)(1 `mk` 2); }

// Same node again, this time under a MaterializeTemporaryExpr.
// PRINT: {{.*}}3 `mk` 4{{.*}}
void bound() {
  const Res &r = 3 `mk` 4;
  (void)r.v;
}

// A member function in the slot yields a CXXMemberCallExpr, whose class-typed
// result is bound to a temporary the same way.
struct Maker {
  Res make(int a, int b) const;
};
// PRINT: {{.*}}5 `m.make` 6{{.*}}
void member(const Maker &m) { (void)(5 `m.make` 6); }

// Not applicable here: the keyword-escape form. `kw` occupies operand and
// declarator position, never the operator slot -- an escaped name between the
// backticks is diagnosed ("expected expression between backticks"), so it can
// never produce a BacktickInfixExpr. The escape's own -ast-print behaviour is
// covered by clang/test/Parser/backtick-escape.cpp.
