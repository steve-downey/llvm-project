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
