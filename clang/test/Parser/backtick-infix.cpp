// RUN: %clang_cc1 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -fbacktick -fsyntax-only %s

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

// Basic desugar: x `f` y -> f(x, y)
// AST: CallExpr {{.*}} 'int'
// AST-NEXT: ImplicitCastExpr
// AST-NEXT: DeclRefExpr {{.*}} 'add'
// AST-NEXT: IntegerLiteral {{.*}} 1
// AST-NEXT: IntegerLiteral {{.*}} 2
int basic = 1 `add` 2;

// Left-associativity: a `sub` b `mul` c == mul(sub(a,b), c)
// AST: CallExpr {{.*}} 'int'
// AST-NEXT: ImplicitCastExpr
// AST-NEXT: DeclRefExpr {{.*}} 'mul'
// AST-NEXT: BacktickInfixExpr {{.*}} 'int'
// AST-NEXT: CallExpr {{.*}} 'int'
// AST-NEXT: ImplicitCastExpr
// AST-NEXT: DeclRefExpr {{.*}} 'sub'
int chain = 10 `sub` 3 `mul` 2;

// Qualifier in operator slot
namespace ns { int f(int a, int b) { return a + b; } }
int qualified = 1 `ns::f` 2;

// Unary prefix on operands: -x `f` -y -> f(-x, -y)
int unary = -1 `add` -2;

// ---------------------------------------------------------------------------
// D16: a type-name in the operator slot yields construction:
// x `T` y == T(x, y), functional-style, CTAD applying.
// ---------------------------------------------------------------------------

struct Pt {
  int x, y;
  Pt(int a, int b) : x(a), y(b) {}
};

// Bare class name in the slot.
// AST: VarDecl {{.*}} type_bare 'Pt'
// AST: BacktickInfixExpr {{.*}} 'Pt'
// AST-NEXT: CXXTemporaryObjectExpr {{.*}} 'Pt' 'void (int, int)'
// AST-NEXT: IntegerLiteral {{.*}} 1
// AST-NEXT: IntegerLiteral {{.*}} 2
Pt type_bare = 1 `Pt` 2;

// Qualified type name in the slot.
namespace geo { struct Vec { Vec(int, int); }; }
// AST: VarDecl {{.*}} type_qual 'geo::Vec'
// AST: BacktickInfixExpr {{.*}} 'geo::Vec'
// AST-NEXT: CXXTemporaryObjectExpr {{.*}} 'geo::Vec'
geo::Vec type_qual = 1 `geo::Vec` 2;

// Class template name in the slot: CTAD, exactly as pr(1, 2) would deduce.
template <class A, class B> struct pr {
  A first; B second;
  pr(A a, B b) : first(a), second(b) {}
};
// AST: VarDecl {{.*}} type_ctad 'pr<int, int>'
// AST: BacktickInfixExpr {{.*}} 'pr<int, int>'
// AST-NEXT: CXXTemporaryObjectExpr {{.*}} 'pr<int, int>'
auto type_ctad = 1 `pr` 2;

// Template-id in the slot.
// AST: VarDecl {{.*}} type_tid 'pr<int, int>'
// AST: BacktickInfixExpr {{.*}} 'pr<int, int>'
auto type_tid = 3 `pr<int, int>` 4;

// Typedef name in the slot; the sugar is preserved on the node.
using PtAlias = Pt;
// AST: VarDecl {{.*}} type_alias 'Pt'
// AST: BacktickInfixExpr {{.*}} 'PtAlias'
Pt type_alias = 5 `PtAlias` 6;

// Dependent type in the slot: CXXUnresolvedConstructExpr until instantiation.
// AST: FunctionTemplateDecl {{.*}} dep_construct
// AST: BacktickInfixExpr {{.*}} 'T'
// AST-NEXT: CXXUnresolvedConstructExpr {{.*}} 'T'
template <class T> T dep_construct(int a, int b) { return a `T` b; }
Pt dep_inst = dep_construct<Pt>(3, 4);

// A name that denotes both a class and a function: ordinary lookup hides the
// class, so the slot is the call both(1, 2), not construction -- the same
// resolution the spelled form gets.
struct both { both(int, int); };
int both(int, int);
// AST: VarDecl {{.*}} type_hidden 'int'
// AST: BacktickInfixExpr {{.*}} 'int'
// AST-NEXT: CallExpr {{.*}} 'int'
int type_hidden = 1 `both` 2;

// The type slot is an expression in statement position too: no
// most-vexing-parse declaration reading can arise (design doc 17.3).
// AST: FunctionDecl {{.*}} stmt_position
// AST: BacktickInfixExpr {{.*}} 'Pt'
// AST-NEXT: CXXTemporaryObjectExpr {{.*}} 'Pt'
void stmt_position(int a, int b) { a `Pt` b; }
