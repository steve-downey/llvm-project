// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only %s

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

// Basic desugar: x `f` y -> f(x, y)
//
// The wrapper's range is pinned literally, not with a wildcard: it must span
// the operands as written. The desugared call's own range is the synthesized
// callee's -- the operator slot alone -- which is what BacktickInfixExpr
// overrides getBeginLoc/getEndLoc to correct.
// AST: BacktickInfixExpr {{.*}} <col:13, col:21> 'int'
// AST-NEXT: CallExpr {{.*}} 'int'
// AST-NEXT: ImplicitCastExpr
// AST-NEXT: DeclRefExpr {{.*}} 'add'
// AST-NEXT: IntegerLiteral {{.*}} 1
// AST-NEXT: IntegerLiteral {{.*}} 2
int basic = 1 `add` 2;

// Left-associativity: a `sub` b `mul` c == mul(sub(a,b), c)
// AST: BacktickInfixExpr {{.*}} <col:13, col:30> 'int'
// AST-NEXT: CallExpr {{.*}} 'int'
// AST-NEXT: ImplicitCastExpr
// AST-NEXT: DeclRefExpr {{.*}} 'mul'
// AST-NEXT: BacktickInfixExpr {{.*}} <col:13, col:22> 'int'
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
// A type slot desugars to construction, not a call; the range is recovered
// from the construction node's arguments and spans the operands just the same.
// AST: BacktickInfixExpr {{.*}} <col:16, col:23> 'Pt'
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

// An aggregate in the slot does not construct through a constructor: it
// initializes through parenthesized aggregate initialization, so Sema hands
// back a CXXFunctionalCastExpr over a CXXParenListInitExpr. That is a fourth
// inner shape, and the operands -- and so the range -- are recovered from the
// written initializers rather than from a call or a construction.
struct Agg { int x, y; };
// AST: VarDecl {{.*}} type_agg 'Agg'
// AST: BacktickInfixExpr {{.*}} <col:16, col:24> 'Agg'
// AST-NEXT: CXXFunctionalCastExpr {{.*}} 'Agg'
// AST-NEXT: CXXParenListInitExpr {{.*}} 'Agg'
Agg type_agg = 1 `Agg` 2;

// The same shape with a default member initializer: the full initializer list
// carries the defaulted member, so only the user-written initializers are the
// operands.
struct Agg3 { int x, y, z = 7; };
// AST: VarDecl {{.*}} type_agg3 'Agg3'
// AST: BacktickInfixExpr {{.*}} <col:18, col:27> 'Agg3'
Agg3 type_agg3 = 1 `Agg3` 2;

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

// A builtin with custom type checking rewrites the call to a node that is
// neither a call nor a construction, so no operand is recoverable and the
// range falls back to the semantic form's. The point of the case is that
// asking for the range does not crash.
typedef int v4i __attribute__((ext_vector_type(4)));
// AST: BacktickInfixExpr {{.*}} <col:40, col:63> 'v4i'
// AST-NEXT: ShuffleVectorExpr {{.*}} <col:40, col:63> 'v4i'
v4i shuffled(v4i a, v4i b) { return a `__builtin_shufflevector` b; }
