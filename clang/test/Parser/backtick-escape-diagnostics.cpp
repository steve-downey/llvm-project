// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s

// S07: Error cases for keyword-escape.

// Non-keyword inside escape: ordinary identifier is rejected.
void bad_nonkw() {
  (void)`foo`(); // expected-error {{backtick keyword-escape requires a C++ keyword}}
}

// --- The escape is part of the spelling a diagnostic names ------------------
// Under -fbacktick the escape is the only way to write this name, so a
// diagnostic that called the entity "new" would be naming it with a spelling
// no program can contain. Both diagnostic argument kinds agree: the first of
// these is passed a DeclarationName, the second a NamedDecl*. See
// docs/backtick-operator-design.md, keyword-escape-printing.
void `new`(int);
void diagnostic_names_the_escape() {
  `new`(); // expected-error {{no matching function for call to '`new`'}}
           // expected-note@-3 {{candidate function not viable}}
}

int `class`;   // expected-note {{previous definition is here}}
int `class`;   // expected-error {{redefinition of '`class`'}}

// --- A qualified name whose final component is an escape --------------------
// Malformed escapes diagnose the same in a qualified position as anywhere
// else, and a *well-formed* escape naming something that is not a type has to
// diagnose and stop. The last of these looped forever when the qualified
// type-specifier arm was first written: the recovery path for an unresolved
// qualified name is implicit-int, which does not apply to an escape, so the
// decl-specifier loop re-entered its own case with the token stream unchanged.
namespace NS {
struct `union` { int a; };
int `new` = 1;
}
NS::`notakeyword` q0;   // expected-error {{backtick keyword-escape requires a C++ keyword}}
NS::`union q1;          // expected-error {{missing closing backtick for keyword escape}}
                        // expected-note@-1 {{to match this '`'}}

namespace Empty { int x; }
Empty::`union` q2;      // expected-error {{a type specifier is required for all declarations}}
                        // expected-error@-1 {{no member named '`union`' in namespace 'Empty'}}
                        // expected-error@-2 {{expected ';' after top level declarator}}
using QX = Empty::`union`;  // expected-error {{expected a type}}
                            // expected-error@-1 {{expected ';' after alias declaration}}
