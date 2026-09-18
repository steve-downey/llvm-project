// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s

// S07: Error cases for keyword-escape.

// Not an identifier inside the escape. escape-content makes the content rule
// "any word spelled as an identifier", so what is left to reject is everything
// that is not a word: a number, punctuation, a literal. `foo` used to be here
// and is now well-formed; it lives in backtick-escape-identifier.cpp.
void bad_number() {
  (void)`3`(); // expected-error {{backtick escape requires an identifier}}
}
void bad_punct() {
  // Two, not one: the recovery leaves the parser on the closing backtick,
  // which opens an escape of its own over the '(' that follows it. That is
  // the same shape the keyword-only rule recovered with and is why the error
  // path has its own sweep (ops/probes/escape-errors.sh) -- what matters is
  // that it stops.
  (void)`+`(); // expected-error 2 {{backtick escape requires an identifier}}
}
void bad_string() {
  (void)`"s"`(); // expected-error {{backtick escape requires an identifier}}
}

// --- The two spellings are one identifier -----------------------------------
// The sharpest way to show identity is to make the spellings collide: this
// conflicts only if `clash` and clash are the same name. (escape-content)
extern int `clash`;  // expected-note {{previous declaration is here}}
extern float clash;  // expected-error {{redeclaration of 'clash' with a different type}}

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
NS::`3` q0;             // expected-error {{backtick escape requires an identifier}}
NS::`union q1;          // expected-error {{missing closing backtick for keyword escape}}
                        // expected-note@-1 {{to match this '`'}}

namespace Empty { int x; }
Empty::`union` q2;      // expected-error {{a type specifier is required for all declarations}}
                        // expected-error@-1 {{no member named '`union`' in namespace 'Empty'}}
                        // expected-error@-2 {{expected ';' after top level declarator}}
using QX = Empty::`union`;  // expected-error {{expected a type}}
                            // expected-error@-1 {{expected ';' after alias declaration}}
