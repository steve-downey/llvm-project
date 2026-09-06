// -fbacktick is a C++-only grammar extension and must be inert in C. C has no
// operator-slot production a backtick could appear in and no declarator-id it
// could escape, so the flag can only ever change C *tokenization* -- and
// without ShouldParseIf<cplusplus.KeyPath> on the option it did more than lose
// a diagnostic: a C compilation with -fbacktick *accepted* the C++ infix
// grammar and exited 0.
//
// The assertion is the rejection, not the wording: the two compilations must
// produce byte-identical output, and both must fail.
//
// RUN: not %clang_cc1 -fbacktick -fsyntax-only %s > %t.on.txt 2>&1
// RUN: not %clang_cc1 -fsyntax-only %s > %t.off.txt 2>&1
// RUN: diff %t.on.txt %t.off.txt
// RUN: FileCheck --input-file=%t.on.txt %s

int g(int, int);
int f(int a, int b) { return a `g` b; }
// CHECK: error: expected ';' after return statement
