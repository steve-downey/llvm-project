// RUN: %clang_cc1 -std=c++17 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++17 -fbacktick -fsyntax-only %s
// RUN: %clang_cc1 -std=c++17 -fbacktick -triple x86_64-unknown-linux-gnu \
// RUN:   -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -std=c++17 -fbacktick -triple x86_64-unknown-linux-gnu \
// RUN:   -emit-llvm -o - %s | llvm-cxxfilt -n | FileCheck %s --check-prefix=DEMANGLED
// RUN: %clang_cc1 -std=c++17 -fbacktick -triple x86_64-unknown-linux-gnu \
// RUN:   -emit-llvm -o - -DUSE_ONLY %s | FileCheck %s --check-prefix=TU2

// S09: Full usage surface and ABI/mangling evidence for keyword-escaped identifiers.
// Design §12: the escape yields an ordinary identifier — lookup, mangling, and
// linkage treat it as a normal identifier.  D10: ABI unchanged.

// ============================================================
// Section 1: Declaration, definition, and call at namespace scope
// ============================================================

// AST: FunctionDecl {{.*}} new 'int (int, int)'
// IR-LABEL: define {{.*}} @_Z3newii
// DEMANGLED-LABEL: define {{.*}} @new(int, int)
#ifndef USE_ONLY
int `new`(int x, int y) { return x + y; }
#else
int `new`(int, int);
#endif

// AST: FunctionDecl {{.*}} call_it 'void ()'
// AST:  CallExpr
// AST:   DeclRefExpr {{.*}} 'new' 'int (int, int)'
void call_it() { (void)`new`(1, 2); }

// TU2-DAG: call {{.*}} @_Z3newii
// TU2-DAG: declare {{.*}} @_Z3newii

// ============================================================
// Section 2: Member function with . and -> access
// ============================================================

struct Widget {
  void `delete`();
};
// AST: CXXMethodDecl {{.*}} delete 'void ()'
// IR-LABEL: define {{.*}} @_ZN6Widget6deleteEv
// DEMANGLED-LABEL: define {{.*}} @Widget::delete()
#ifndef USE_ONLY
void Widget::`delete`() {}
#endif

// AST: FunctionDecl {{.*}} member_dot 'void ()'
// AST:  MemberExpr {{.*}} .delete
void member_dot() { Widget w; w.`delete`(); }

// AST: FunctionDecl {{.*}} member_arrow 'void ()'
// AST:  MemberExpr {{.*}} ->delete
void member_arrow() { Widget *pw = nullptr; pw->`delete`(); }
