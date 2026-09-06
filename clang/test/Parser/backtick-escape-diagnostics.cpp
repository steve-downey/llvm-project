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
