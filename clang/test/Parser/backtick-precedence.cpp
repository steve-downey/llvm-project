// RUN: %clang_cc1 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -fbacktick -fsyntax-only %s

int f(int a, int b) { return a + b; }
int g(int a, int b) { return a - b; }

struct S { int x; };

// D2 confirmed (Option A): -x `f` -y -> f(-x, -y).
// Both unary minus operators land inside the call args, not outside.
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
// AST: UnaryOperator {{.*}} prefix '-'
// AST: UnaryOperator {{.*}} prefix '-'
int d2_unary = -1 `f` -2;

// Precedence vs multiplication: 2 * 3 `f` 4 -> 2 * f(3, 4).
// Backtick binds tighter than *, so CallExpr is the RHS of BinaryOperator.
// AST: BinaryOperator {{.*}} '*'
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
int mul_prec = 2 * 3 `f` 4;

// Left-associativity: 1 `f` 2 `g` 3 -> g(f(1,2), 3).
// Outer call is g; inner call f(1,2) is its first argument.
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'g'
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
int left_assoc = 1 `f` 2 `g` 3;

// Ternary: 1 ? 2 : 3 `f` 4 -> 1 ? 2 : f(3, 4).
// Backtick is in the else-branch; ConditionalOperator is the outer node.
// AST: ConditionalOperator
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
int ternary_prec = 1 ? 2 : 3 `f` 4;

// Member-access operands: s1.x `f` s2.x -> f(s1.x, s2.x).
// Member access is a postfix-expression and binds tighter than backtick.
// AST: CallExpr {{.*}} 'int'
// AST: DeclRefExpr {{.*}} 'f'
// AST: MemberExpr {{.*}} .x
// AST: MemberExpr {{.*}} .x
S s1 = {1}, s2 = {2};
int mem_prec = s1.x `f` s2.x;
