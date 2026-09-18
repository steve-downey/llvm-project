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

// ============================================================
// Section 3: escape-content -- a non-keyword escape is the same symbol
// ============================================================
// The identity claim in mangled form. One function, defined with its name
// escaped and called with its name bare, is one symbol; nothing in the
// mangling records that a spelling had backticks around it, because nothing
// in the AST does. Under the keyword-only content rule this section did not
// compile at all: `ordinary` was an error, not a name.

// AST: FunctionDecl {{.*}} ordinary 'int (int)'
// IR-LABEL: define {{.*}} @_Z8ordinaryi
// DEMANGLED-LABEL: define {{.*}} @ordinary(int)
#ifndef USE_ONLY
int `ordinary`(int x) { return x; }
#else
int ordinary(int);
#endif

// AST: FunctionDecl {{.*}} call_ordinary 'void ()'
void call_ordinary() {
  (void)ordinary(1);    // bare
  (void)`ordinary`(2);  // escaped: the same callee, the same symbol
}
// TU2-DAG: call {{.*}} @_Z8ordinaryi
// TU2-DAG: declare {{.*}} @_Z8ordinaryi

// An alternative token as a type name, which is the shape the type-keyword
// case pins for keywords: the parameter's type is named by an escape and
// mangles as the ordinary source name it is.
struct `and` { int v; };
// Two halves of the dump, asserted together because they disagree and the
// design doc claimed they did not. The *declaration's* own name is dumped
// bare -- that is the ABI evidence, the name really is an ordinary identifier
// -- while the same name *inside a type string* is dumped escaped. That split
// is not new here and is not the content rule's doing: `union` behaves
// identically, and has since the escape was built. It is recorded as
// ast-dump-type-name-spelling in the backtick repo's ops/DEVIATIONS.md.
// AST: CXXRecordDecl {{.*}} struct and definition
// AST: FunctionDecl {{.*}} takes_and 'void (int, `and`)'
// IR-LABEL: define {{.*}} @_Z9takes_andi3and
// DEMANGLED-LABEL: define {{.*}} @takes_and(int, and)
#ifndef USE_ONLY
void takes_and(int, `and`) {}
#endif
