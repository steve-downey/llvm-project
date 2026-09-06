// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only %s
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 \
// RUN:   | %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -x c++ -

// S07: Keyword-escaped identifiers in name positions.
// `kw` in primary-expression / declarator-id / member-access position becomes
// an ordinary identifier; the post-operand position stays the infix operator.

// --- Declarator-id: function named with a keyword ---------------------------
// The escape yields an ordinary identifier, but its *spelling* is a keyword,
// so printing it bare produces source that does not re-parse. Every printer
// that emits source re-escapes it, and so does every diagnostic that names it
// (backtick-escape-diagnostics.cpp). -ast-dump deliberately goes the other
// way, and the AST expectations below are what pins that: the dump reports the
// identifier the escape yields, which is the evidence that the name really is
// ordinary. See docs/backtick-operator-design.md, keyword-escape-printing.
// AST: FunctionDecl {{.*}} new 'void ()'
// PRINT: void `new`();
void `new`();

// --- Primary-expression: call a keyword-named function ----------------------
void call_new() {
  // AST: FunctionDecl {{.*}} call_new 'void ()'
  // AST:  CallExpr
  // AST-NEXT:  ImplicitCastExpr
  // AST-NEXT:  DeclRefExpr {{.*}} 'new' 'void ()'
  // PRINT: `new`();
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
// PRINT: int `delete`;
struct S {
  int `delete`;
};

void member_access() {
  // AST: FunctionDecl {{.*}} member_access 'void ()'
  S s;
  // AST:  MemberExpr {{.*}} .delete
  // PRINT: (void)s.`delete`;
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

// --- Declarators the type printer names, not DeclarationName::print ---------
// A typedef, a variable and a parameter hand their name to the *type* printer
// as a placeholder string, which never reaches DeclarationName::print. Each has
// to re-escape it or the printed declaration does not re-parse -- which is what
// the second RUN line above checks for the whole file.
// AST: TypedefDecl {{.*}} class 'int'
// PRINT: typedef int `class`;
typedef int `class`;

// AST: VarDecl {{.*}} typename 'int'
// PRINT: int `typename` = 0;
int `typename` = 0;

// AST: FunctionDecl {{.*}} takes_escaped 'void (int)'
// AST-NEXT: ParmVarDecl {{.*}} throw 'int'
// PRINT: void takes_escaped(int `throw`);
void takes_escaped(int `throw`);

// --- Escaped callee in infix slot (D3): x `(`new`)` y ----------------------
// The paren in the infix slot restores BacktickIsOperator, so the inner
// `new` is parsed as a primary-expression (escape) -> identifier "new".
int `new`(int, int);
void test_d3() {
  // AST: FunctionDecl {{.*}} test_d3 'void ()'
  int a = 1, b = 2;
  // AST:  CallExpr
  // The parenthesized slot survives into the printed form, so the escaped
  // callee is re-escaped inside its parens and the whole thing re-parses.
  // AST:   DeclRefExpr {{.*}} 'new' 'int (int, int)'
  // PRINT: (void)(a `(`new`)` b);
  (void)(a `(`new`)` b);
}
