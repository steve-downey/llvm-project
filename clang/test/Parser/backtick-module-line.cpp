// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s
// escape-module-names (docs/backtick-operator-design.md): a backtick after
// 'module' at the start of a line makes a module directive, so a line
// beginning module `f` y is no longer an infix call on a variable named
// module -- the trade C++20 made for module x; and import x;.  The escape is
// the workaround, and so are parentheses: neither line begins with the word.

int fn(int a, int b) { return a + b; }

int t() {
  int module = 1, y = 2;
  int r = `module` `fn` y;
  r += (module) `fn` y;
  return r;
}

int u() {
  int module = 1, y = 2;
module `fn` y; // expected-error {{unexpected preprocessing token 'y' after module name}} \
               // expected-error {{expected expression}}
  return 0;
}
