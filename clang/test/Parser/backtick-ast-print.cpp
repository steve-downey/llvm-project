// RUN: %clang_cc1 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT
// RUN: %clang_cc1 -fbacktick -ast-print %s 2>&1 | %clang_cc1 -fbacktick -fsyntax-only -x c++ -

// S11: -ast-print round-trip for backtick infix expressions.
// The pretty-printer must re-emit the backtick form; re-parsing must succeed.

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

// PRINT: {{.*}}`add`{{.*}}
int basic = 1 `add` 2;

// PRINT: {{.*}}`sub`{{.*}}
int chain = 10 `sub` 3;
