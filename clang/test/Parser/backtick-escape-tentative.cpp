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
