// Infix parse of Unicode user-defined operators (U4/U§6): one shared
// user-infix precedence level, left-associative, operands are
// cast-expressions.  This file is about *grouping*; which function gets
// called is SemaCXX/unicode-operator-call.cpp's job.
//
// The grouping assertions are values, not AST text: each operator is
// constexpr and non-commutative, so a wrong grouping is a wrong number.
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-dump %s | FileCheck %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -DERRORS -fsyntax-only -verify=err %s
// RUN: %clang_cc1 -std=c++23 -DOFF -fsyntax-only -verify=off %s

#ifndef OFF
// expected-no-diagnostics

// 2a + b and 3a + b: neither is commutative and neither is associative, so
// every grouping below has a distinct value.
constexpr int operator⊞(int a, int b) { return 2 * a + b; }
constexpr int operator⊗(int a, int b) { return 3 * a + b; }

// --- the operator parses at all -------------------------------------------
static_assert((1 ⊞ 2) == 4);

// --- left-associative (D1/U4) ---------------------------------------------
// Same operator: right-associative would be 2*1 + (2*2+3) == 9.
static_assert(1 ⊞ 2 ⊞ 3 == 11);
// Two different operators at the same level: right-associative would be
// 2*1 + (3*2+3) == 11, and a two-level table could give anything.
static_assert(1 ⊞ 2 ⊗ 3 == 15);
static_assert(1 ⊗ 2 ⊞ 3 == 13);
// Parentheses regroup, and give the value the chain does *not* have.
static_assert((1 ⊞ (2 ⊗ 3)) == 11);

// --- tighter than every other binary operator (D2 Option A / §4) ----------
// Looser-or-equal to '*' would be 2*(2*3) + 4 == 16.
static_assert(2 * 3 ⊞ 4 == 20);
static_assert(2 ⊞ 3 * 4 == 28);   // (2 ⊞ 3) * 4 == 7 * 4
static_assert(1 + 2 ⊞ 3 == 8);    // 1 + (2*2+3)
static_assert(2 ⊞ 3 == 7);        // '==' is looser: (2 ⊞ 3) == 7
static_assert((1 ⊞ 2 ? 3 ⊞ 4 : 5) == 10);
static_assert((true ? 1 : 2) ⊞ 3 == 5);

// --- looser than unary: both operands are cast-expressions (§4 symmetry) ---
// If the operator bound tighter than unary minus these would be -(1 ⊞ -2)
// == 0 and -(1 ⊞ 2) == -4 respectively.
static_assert(-1 ⊞ -2 == -4);
static_assert(-1 ⊞ 2 == 0);
static_assert(!0 ⊞ 1 == 3);
static_assert(+1 ⊞ +2 == 4);
static_assert(sizeof(int) ⊞ 0 == 8);

// A cast-expression on either side, which is exactly what the grammar says
// an operand is.
static_assert((int)1.9 ⊞ (int)2.9 == 4);

// --- the level is an ordinary level: it composes with the rest ------------
constexpr int assigns() {
  int x = 0;
  x = 1 ⊞ 2;          // assignment slot
  x += 1 ⊞ 0;         // compound assignment
  return x;
}
static_assert(assigns() == 6);

constexpr int arr[] = {1 ⊞ 2, 3 ⊞ 4};   // no comma confusion
static_assert(arr[0] == 4 && arr[1] == 10);

constexpr int call(int a, int b) { return a + b; }
static_assert(call(1 ⊞ 2, 3) == 7);      // argument position

template <int N> struct Box { static constexpr int value = N; };
static_assert(Box<(1 ⊞ 2)>::value == 4); // template-argument position

// --- templates: the parse is the same, dependent or not -------------------
template <class T> constexpr auto dep(T a, T b) { return a ⊞ b ⊗ a; }
static_assert(dep(1, 2) == 13);

// --- AST shape ------------------------------------------------------------
int ast_infix(int a, int b) { return a ⊞ b; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_infix
// CHECK:       CallExpr
// CHECK:       DeclRefExpr {{.*}} 'operator⊞'

// Left-associative: the *outer* call is the second operator.
int ast_chain(int a, int b, int c) { return a ⊞ b ⊗ c; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_chain
// CHECK:       CallExpr
// CHECK:       DeclRefExpr {{.*}} 'operator⊗'
// CHECK:       CallExpr
// CHECK:       DeclRefExpr {{.*}} 'operator⊞'

// Tighter than '*': the outer node is the builtin multiply.
int ast_tighter(int a, int b, int c) { return a * b ⊞ c; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_tighter
// CHECK:       BinaryOperator {{.*}} '*'
// CHECK:       CallExpr
// CHECK:       DeclRefExpr {{.*}} 'operator⊞'

// Symmetric: both operands are unary expressions.
int ast_symmetric(int a, int b) { return -a ⊞ -b; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_symmetric
// CHECK:       CallExpr
// CHECK:       DeclRefExpr {{.*}} 'operator⊞'
// CHECK:       UnaryOperator {{.*}} prefix '-'
// CHECK:       UnaryOperator {{.*}} prefix '-'

#ifdef ERRORS
// D9 carried: no braced-init-list operand.
int braced(int a) { return a ⊞ {1, 2}; }
// err-error@-1 {{initializer list cannot be used on the right hand side of operator '⊞'}}

// A use of an operator nobody declared is a lookup failure, never a lexing
// or parsing failure (U3).
int undeclared(int a, int b) { return a ⊠ b; }
// err-error@-1 {{use of undeclared 'operator⊠'}}

// Still a binary operator, so a missing right operand is the ordinary
// missing-operand diagnostic.
int missing(int a) { return a ⊞ ; }
// err-error@-1 {{expected expression}}
#endif

#else
// --- flag off: unchanged (the token does not exist) -----------------------
int off(int a, int b) { return a ⊞ b; }
// off-error@-1 {{unexpected character '⊞' U+229E}}
// off-error@-2 {{expected ';' after return statement}}
#endif
