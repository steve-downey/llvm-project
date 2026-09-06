// RUN: %clang_cc1 -fbacktick -fsyntax-only -verify %s

int f(int, int);
int g(int, int);
int a, b, c;

// Empty operator slot.
int x1 = a ``b; // expected-error{{expected expression between backticks}}

// Unterminated backtick (no closing backtick before statement boundary).
int x2 = a `f b; // expected-error{{missing closing backtick for infix operator}} \
                 // expected-note{{to match this '`'}}

// Parenthesised nesting is well-formed (D3): slot = (f `g` h), callee = that result.
int x3 = a `(f)` b;
int x4 = a `g` b `f` c;

// D16: a builtin type in the slot parses as construction and is rejected by
// the same semantic rule as int(a, b) -- not by the parser.
int x5 = a `int` b; // expected-error{{excess elements in scalar initializer}}

// D16: a class template whose arguments cannot be deduced fails exactly as
// the spelled construction would.
template <typename T> struct NoGuide { NoGuide(); };  // expected-note 2 {{candidate function template not viable}} \
                                                      // expected-note 2 {{implicit deduction guide}}
int x6 = (a `NoGuide` b, 0); // expected-error{{no viable constructor or deduction guide for deduction of template arguments of 'NoGuide'}}
