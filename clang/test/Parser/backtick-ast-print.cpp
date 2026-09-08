// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 | %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -x c++ -

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

// D16: a type-name in the slot desugars to construction, not a call, so the
// printer recovers the type from the CXXTemporaryObjectExpr (or, in a
// dependent context, the CXXUnresolvedConstructExpr). Printing the semantic
// form P(1, 2) here would be a round-trip hole, not a pass.
struct Pair2 {
  Pair2(int, int);
};
// PRINT: {{.*}}1 `Pair2` 2{{.*}}
Pair2 t1 = 1 `Pair2` 2;

namespace nn { struct Q { Q(int, int); }; }
// PRINT: {{.*}}3 `nn::Q` 4{{.*}}
nn::Q t2 = 3 `nn::Q` 4;

// CTAD: the slot is printed as the deduced specialization, which re-parses
// as a template-id type slot with the same meaning.
template <class A, class B> struct pp { pp(A, B); };
// PRINT: {{.*}}5 `pp<int, int>` 6{{.*}}
auto t3 = 5 `pp` 6;

// An aggregate type slot initializes through parenthesized aggregate
// initialization rather than through a constructor, so the operands come from
// the CXXParenListInitExpr under the functional cast. Printing Agg(1, 2) here
// would be the desugaring, not the surface syntax.
struct Agg { int x, y; };
// PRINT: {{.*}}1 `Agg` 2{{.*}}
Agg t5 = 1 `Agg` 2;

// The same shape under CTAD, printed as the deduced specialization for the
// same reason the constructor form is: the type is recovered from the
// semantic node.
template <class T> struct aggT { T x; T y; };
// PRINT: {{.*}}3 `aggT<int>` 4{{.*}}
auto t6 = 3 `aggT` 4;

// Dependent construction prints the written type.
// PRINT: {{.*}}a `T` b{{.*}}
template <class T> T t4(int a, int b) { return a `T` b; }

// Not applicable here: the keyword-escape form. `kw` occupies operand and
// declarator position, never the operator slot -- an escaped name between the
// backticks is diagnosed ("expected expression between backticks"), so it can
// never produce a BacktickInfixExpr. The escape's own -ast-print round-trip
// is a separate question, and is pinned by the -ast-print RUN lines in
// clang/test/Parser/backtick-escape.cpp -- which that file did not have until
// keyword-escape-round-trip was fixed, which is why the hole survived nine
// steps behind this sentence.
