// -fbacktick is a C++-only grammar extension and must be inert in C, for the
// same reason and by the same mechanism as -funicode-operators: C has no
// `operator` keyword and no function-call slot grammar for a backtick, so the
// flag can only ever change C *tokenization*. The two flags take
// ShouldParseIf<cplusplus.KeyPath> together -- a divergence where one of the
// pair is C++-only would be a worse surprise than the symmetry. See DEV-U07.
//
// RUN: not %clang_cc1 -fbacktick -fsyntax-only %s > %t.on.txt 2>&1
// RUN: not %clang_cc1 -fsyntax-only %s > %t.off.txt 2>&1
// RUN: diff %t.on.txt %t.off.txt
// RUN: FileCheck --input-file=%t.on.txt %s

int g(int, int);
int f(int a, int b) { return a `g` b; }
// CHECK: error: expected ';' after return statement
