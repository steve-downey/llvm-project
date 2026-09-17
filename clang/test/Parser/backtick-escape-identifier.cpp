// RUN: %clang_cc1 -std=c++17 -fbacktick -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT20
// RUN: %clang_cc1 -std=c++17 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT17
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 \
// RUN:   | %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -x c++ -
// RUN: %clang_cc1 -std=c++17 -fbacktick -ast-print %s 2>&1 \
// RUN:   | %clang_cc1 -std=c++17 -fbacktick -fsyntax-only -x c++ -

// escape-content: the word between the backticks does not have to be a
// keyword. Anything spelled as an identifier may stand there, and what comes
// out is that identifier and nothing more specific -- `foobar` *is* foobar.
// See docs/backtick-operator-design.md, escape-content.

// expected-no-diagnostics

// --- Identity: one name, two spellings --------------------------------------
// The escape yields the interned identifier and keeps no record of how it was
// written, so a declaration and its uses may be spelled either way in either
// order. This is the whole of the decision; everything below is a consequence.

// AST: VarDecl {{.*}} plain 'int'
// PRINT20: int `foobar` = 0;
// PRINT17: int `foobar` = 0;
int `foobar` = 0;
int read_bare() { return foobar; }        // the same variable
int read_escaped() { return `foobar`; }   // and so is this

int fn(int x);                            // declared bare
int `fn`(int x) { return x; }             // defined escaped: one function
int call_both() { return fn(1) + `fn`(2); }

// --- Printing: the spelling is put back only where it has to be -------------
// One predicate serves both answers, and neither compiler is taught it. A name
// whose spelling is a keyword prints escaped, because printing it bare would
// not re-parse; a name whose spelling is an ordinary identifier prints bare,
// because it is an ordinary identifier. The check lines above and below are
// the two halves: `foobar` prints bare, `class` prints escaped.

// AST: TypedefDecl {{.*}} class 'int'
// PRINT20: typedef int `class`;
// PRINT17: typedef int `class`;
typedef int `class`;

// --- The same word, two dialects --------------------------------------------
// This is the decision's own argument, run as a test. `requires` is a keyword
// in C++20 and an ordinary identifier in C++17, and the escaped declaration
// means the same thing in both -- which is what lets a name be escaped
// *before* the committee takes the word rather than only after. Under the
// keyword-only rule this file did not compile as C++17 at all.
//
// The two PRINT prefixes are the other half of it: the identity is the same in
// both dialects, and only the spelling that round-trips differs.
// AST: FunctionDecl {{.*}} requires 'bool (int)'
// PRINT20: bool `requires`(int);
// PRINT17: bool requires(int);
bool `requires`(int);
bool call_requires(int x) { return `requires`(x); }

// --- Alternative tokens ([lex.digraph]) -------------------------------------
// In C++ `and` and its ten siblings are tokens, not macros, so they are words
// the language has claimed and the escape releases them like any other. They
// also pin the printing rule from the far side: `and` must print escaped,
// because a bare `and` lexes as && and would not re-parse -- and the predicate
// that knows this is the same one that prints `foobar` bare, because `and`'s
// IdentifierInfo carries TokenID tok::ampamp.
// AST: VarDecl {{.*}} and 'int'
// PRINT20: int `and` = 0;
// PRINT17: int `and` = 0;
int `and` = 0;
int read_and() { return `and`; }

// AST: VarDecl {{.*}} bitor 'int'
// PRINT20: int `bitor` = 0;
int `bitor` = 0;

// --- Reserved names stay reserved -------------------------------------------
// The escape does not launder a name, it only lets one be written. `__foo` is
// a reserved identifier escaped or not; nothing here asks the escape to make
// it anything else.
// AST: VarDecl {{.*}} __foo 'int'
// PRINT20: int `__foo` = 0;
int `__foo` = 0;

// --- Positions other than a declarator-id -----------------------------------
// The content rule is orthogonal to the position rule (escape-name-positions),
// so a non-keyword escape reaches everywhere a keyword escape does.
// AST: CXXRecordDecl {{.*}} struct Holder
struct Holder {
  int `member` = 0;
};
int read_member(Holder h) { return h.`member`; }

// AST: NamespaceDecl {{.*}} ns
namespace `ns` {
int `inner` = 0;
}
int read_qualified() { return `ns`::`inner` + ns::inner; }

// AST: EnumDecl {{.*}} Color
enum class `Color` { `red`, green };
Color pick() { return `Color`::`red`; }

template <class `T`> struct Box { `T` value; };
int read_box() { Box<int> b{7}; return b.value; }

// --- The infix operator is untouched ----------------------------------------
// Post-operand position is still the operator, and the slot is still an
// expression. The escape reaching further does not reach here.
int add(int a, int b) { return a + b; }
int infix_still_works() {
  int x = 1, y = 2;
  return x `add` y;
}
