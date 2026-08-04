// -funicode-operators is a C++-only grammar extension and must be inert in C
// (DEV-U07). C has no `operator` keyword, so no `operator⊞` can be declared
// and the token has no production it could appear in -- but before the flag
// gained ShouldParseIf<cplusplus.KeyPath> it still changed C *tokenization*:
// with the flag, a U1 code point lexed as tok::user_operator, which suppressed
// the accurate stray-character diagnostic and left only the misleading
// recovery one. A feature whose grammar is C++-only should not have a flag
// that changes C tokenization.
//
// Stated as a diff rather than as two expectations, because the claim is
// equality and not a particular message:
//
// RUN: not %clang_cc1 -funicode-operators -fsyntax-only %s > %t.on.txt 2>&1
// RUN: not %clang_cc1 -fsyntax-only %s > %t.off.txt 2>&1
// RUN: diff %t.on.txt %t.off.txt
// RUN: FileCheck --input-file=%t.on.txt %s
//
// The driver still accepts and forwards the flag in C mode (see
// Driver/funicode-operators.c); it simply has no effect once it arrives.

int f(int a, int b) { return a ⊞ b; }
// CHECK: error: unexpected character '⊞' U+229E
// CHECK: error: expected ';' after return statement
