// RUN: %clang_cc1 -std=c++17 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++17 -fbacktick -fsyntax-only %s

// S08: Tentative-parse / declaration-vs-expression integration.
// TryParseDeclarator must recognize backtick-escaped names as declarator-ids
// so that the "prefer declaration" rule governs ambiguous statements.

struct T {};
int `new`(int, int);

// --- Declaration: variable named with an escaped keyword --------------------
// AST: FunctionDecl {{.*}} test_decl 'void ()'
void test_decl() {
  // AST:  DeclStmt
  // AST:   VarDecl {{.*}} new 'T'
  T `new`;
}

// --- Expression: call to a keyword-named function ---------------------------
// AST: FunctionDecl {{.*}} test_expr 'void ()'
void test_expr() {
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'new' 'int (int, int)'
  `new`(1, 2);
}

// --- The same two shapes with a name that is not a keyword ------------------
// escape-content widens what may stand between the backticks, and the tentative
// parse has to reach the same verdicts on the wider set: the ambiguous
// statement is still a declaration, and the call is still a call. Under the
// keyword-only rule neither of these lines parsed at all.
int `ordinary`(int, int);

// AST: FunctionDecl {{.*}} test_decl_ordinary 'void ()'
void test_decl_ordinary() {
  // AST:  DeclStmt
  // AST:   VarDecl {{.*}} v 'T'
  T `v`;
}

// AST: FunctionDecl {{.*}} test_expr_ordinary 'void ()'
void test_expr_ordinary() {
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'ordinary' 'int (int, int)'
  `ordinary`(1, 2);
}

// --- And the infix operator still wins in post-operand position -------------
// The one shape where the token sequences coincide: `ordinary` is a
// well-formed escape *and* a well-formed operator slot, and position alone
// decides. Here it is the operator.
// AST: FunctionDecl {{.*}} test_infix_ordinary 'int (int, int)'
int test_infix_ordinary(int a, int b) {
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'ordinary' 'int (int, int)'
  return a `ordinary` b;
}
