// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only %s

// S07: Keyword-escaped identifiers in name positions.
// `kw` in primary-expression / declarator-id / member-access position becomes
// an ordinary identifier; the post-operand position stays the infix operator.

// --- Declarator-id: function named with a keyword ---------------------------
// AST: FunctionDecl {{.*}} new 'void ()'
void `new`();

// --- Primary-expression: call a keyword-named function ----------------------
void call_new() {
  // AST: FunctionDecl {{.*}} call_new 'void ()'
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'new' 'void ()'
  `new`();
}

// --- Two-arg call after escape -----------------------------------------------
void `delete`(int, int);
void call_delete() {
  // AST: FunctionDecl {{.*}} call_delete 'void ()'
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'delete'
  `delete`(1, 2);
}

// --- Member access: member named with a keyword -----------------------------
struct S {
  int `delete`;
};

void member_access() {
  // AST: FunctionDecl {{.*}} member_access 'void ()'
  S s;
  // AST:  MemberExpr {{.*}} .delete
  (void)s.`delete`;
}

// --- Infix operator is unaffected -------------------------------------------
int add(int a, int b) { return a + b; }
void test_infix() {
  // AST: FunctionDecl {{.*}} test_infix 'void ()'
  int x = 1, y = 2;
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'add'
  (void)(x `add` y);
}

// --- Escaped callee in infix slot (D3): x `(`new`)` y ----------------------
// The paren in the infix slot restores BacktickIsOperator, so the inner
// `new` is parsed as a primary-expression (escape) -> identifier "new".
int `new`(int, int);
void test_d3() {
  // AST: FunctionDecl {{.*}} test_d3 'void ()'
  int a = 1, b = 2;
  // AST:  CallExpr
  // AST:   DeclRefExpr {{.*}} 'new' 'int (int, int)'
  (void)(a `(`new`)` b);
}
