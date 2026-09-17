// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s
// 'not', because the object-like macro case below is an intended error and
// the AST checks are about what the rest of the file still declares.
// RUN: not %clang_cc1 -std=c++20 -fbacktick -ast-dump %s 2>&1 | FileCheck %s --check-prefix=AST

// escape-content, the preprocessor half: the escape is a phase 7 construct
// built out of three preprocessing tokens, and phase 4 has never heard of it.
// So the word between the backticks is an ordinary identifier preprocessing
// token and the ordinary macro rules apply to it -- which is *not* the same as
// saying nothing changes, and this file is here to say exactly what does.

// --- An object-like macro name is replaced, escaped or not ------------------
// The escape does not shield it: by the time the parser sees the escape, the
// name is gone and what stands between the backticks is the replacement list.
#define OBJECT_MACRO 3
// The second error is the ordinary recovery from a name position that got no
// name; what matters is that the first one names the real problem and that
// the parse stops rather than looping.
int `OBJECT_MACRO` = 0; // expected-error {{backtick escape requires an identifier}}
                        // expected-error@-1 {{expected unqualified-id}}

// --- A function-like macro name is *not* replaced when escaped --------------
// Not because the escape shields it, but because a function-like macro is
// replaced only when its name is followed by '(' -- and here the next
// preprocessing token is the closing backtick. So the escape can name an
// entity whose spelling is a function-like macro, and the ordinary invocation
// still expands in the same translation unit.
#define FUNC_MACRO(x) ((x) + 1)
// AST: VarDecl {{.*}} FUNC_MACRO 'int'
int `FUNC_MACRO` = 7;
int read_it() { return `FUNC_MACRO`; }
int still_expands() { return FUNC_MACRO(1); } // 2, the macro

// --- The escape does not create a macro invocation either -------------------
// AST: FunctionDecl {{.*}} plain_fn 'int (int)'
int plain_fn(int);
int call_plain() { return `plain_fn`(1); }
