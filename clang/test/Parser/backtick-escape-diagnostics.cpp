// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s

// S07: Error cases for keyword-escape.

// Non-keyword inside escape: ordinary identifier is rejected.
void bad_nonkw() {
  (void)`foo`(); // expected-error {{backtick keyword-escape requires a C++ keyword}}
}
