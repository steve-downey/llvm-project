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
