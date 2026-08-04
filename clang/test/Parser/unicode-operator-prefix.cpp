// Prefix parse of Unicode user-defined operators (U5/U§6): a user operator in
// *operand* position is the prefix form, binding like the other unary
// operators -- tighter than any binary operator, including the user-infix
// level itself.
//
// The claim this file exists to test is not that `⊖a` parses. It is that the
// prefix and infix productions are told apart by grammatical position alone:
// no lookahead, no whitespace rule, and no consultation of what has been
// declared (U3). Section 3 is the acceptance test -- the *same* code point
// used both ways in one expression.
//
// As in the infix file, the grouping assertions are values rather than AST
// text: every operator here is constexpr and non-commutative, so a wrong
// parse is a wrong number.
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -DBACKTICK -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-dump %s | FileCheck %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -DBACKTICK -ast-dump %s | FileCheck %s --check-prefixes=CHECK,BT
// RUN: %clang_cc1 -std=c++23 -funicode-operators -DERRORS -fsyntax-only -verify=err %s
// RUN: %clang_cc1 -std=c++23 -DOFF -fsyntax-only -verify=off %s

#ifndef OFF
// expected-no-diagnostics

// ⊖ is declared BOTH ways, deliberately: one parameter is the prefix form and
// two the infix form (U5), and both declarations are visible everywhere below.
// Nothing in the parser looks at either of them.
constexpr int operator⊖(int a) { return 3 * a + 1; }
constexpr int operator⊖(int a, int b) { return 100 * a + b; }

// An infix-only operator, to mix with.
constexpr int operator⊞(int a, int b) { return 2 * a + b; }

//===----------------------------------------------------------------------===//
// 1. The prefix form parses, and stacks
//===----------------------------------------------------------------------===//

static_assert(⊖1 == 4);

// Stacked: the second ⊖ is in the first's operand position, so it is prefix
// too. Nothing had to look ahead to discover that.
static_assert(⊖⊖1 == 13);   // 3*(3*1 + 1) + 1
static_assert(⊖⊖⊖1 == 40);  // 3*13 + 1

//===----------------------------------------------------------------------===//
// 2. It binds like the other unary operators (U5/U§6)
//===----------------------------------------------------------------------===//

// Tighter than the user-infix level itself. If it were looser these would be
// ⊖(1 ⊞ 2) == 13 and 2*1 + ⊖(2 ⊞ 3) == 24 respectively.
static_assert(⊖1 ⊞ 2 == 10);   // operator⊞(⊖1, 2) == 2*4 + 2
static_assert(1 ⊞ ⊖2 == 9);    // operator⊞(1, ⊖2) == 2*1 + 7
static_assert(⊖1 ⊞ ⊖2 == 15);  // 2*4 + 7

// Tighter than '*', which the user-infix level is already tighter than. If the
// prefix form bound looser than '*' this would be ⊖(2 * 3) == 19.
static_assert(⊖2 * 3 == 21);
static_assert(2 * ⊖3 == 20);

// Parentheses regroup, and give the value the unparenthesised form does not.
static_assert(⊖(1 ⊞ 2) == 13);

// Interleaved with the built-in unary operators, either order.
static_assert(⊖-1 == -2);  // 3*(-1) + 1
static_assert(-⊖1 == -4);
static_assert(⊖!0 == 4);
static_assert(⊖~0 == -2);
static_assert(⊖sizeof(int) == 13);

// The operand is a cast-expression, so an explicit cast and every postfix
// suffix bind tighter than the operator.
static_assert(⊖(int)1.9 == 4);
constexpr int arr[] = {1, 2, 3};
static_assert(⊖arr[2] == 10);
constexpr int twice(int a) { return 2 * a; }
static_assert(⊖twice(3) == 19);
struct Field { int v; };
constexpr Field fld{5};
static_assert(⊖fld.v == 16);

//===----------------------------------------------------------------------===//
// 3. The acceptance test: one code point, both forms, one expression
//===----------------------------------------------------------------------===//
//
// `⊖a ⊖ b` is operator⊖(operator⊖(a), b). The first ⊖ opens the expression,
// so it is in operand position and is prefix; the second follows a complete
// operand, so it is infix. Both `operator⊖` overloads are declared and in
// scope, and the parser consults neither.

static_assert(⊖1 ⊖ 2 == 402);       // 100*(3*1 + 1) + 2
static_assert(1 ⊖ ⊖2 == 107);       // 100*1 + (3*2 + 1)
static_assert(⊖1 ⊖ ⊖2 == 407);      // 100*4 + 7
static_assert(⊖⊖1 ⊖ ⊖⊖2 == 1322);   // 100*13 + 22

// The infix form is still left-associative when prefix uses are mixed in.
// Right-associative would be 100*4 + (100*7 + 3) == 1103.
static_assert(⊖1 ⊖ ⊖2 ⊖ 3 == 40703);   // 100*(100*4 + 7) + 3

// Both spellings of the level chain together, left to right, with the prefix
// form still bound tighter than either: ((⊖1) ⊖ 2) ⊞ 3 == 2*402 + 3.
static_assert(⊖1 ⊖ 2 ⊞ 3 == 807);

//===----------------------------------------------------------------------===//
// 4. An ordinary operand: the prefix form composes with everything else
//===----------------------------------------------------------------------===//

constexpr int slots() {
  int x = ⊖1;      // assignment slot
  x += ⊖1;         // compound assignment
  return x;
}
static_assert(slots() == 8);

constexpr int list[] = {⊖1, ⊖2};
static_assert(list[0] == 4 && list[1] == 7);
static_assert(twice(⊖1) == 8);            // argument position
static_assert((true ? ⊖1 : ⊖2) == 4);     // conditional
template <int N> struct Box { static constexpr int value = N; };
static_assert(Box<(⊖1)>::value == 4);     // template-argument position

//===----------------------------------------------------------------------===//
// 5. Member candidates, ADL, and templates
//===----------------------------------------------------------------------===//

// A member prefix operator is declared with *no* parameters (U5's arity rule
// counts the implicit object parameter).
struct Mem {
  int v;
  constexpr int operator⊕() const { return v + 9; }
};
static_assert(⊕Mem{1} == 10);

// ADL reaches an operator declared only in the operand's namespace, and a
// hidden friend reachable by nothing else.
namespace Adl {
struct A {
  int v;
};
constexpr int operator⊘(A x) { return x.v + 100; }
struct H {
  friend constexpr int operator⊗(H) { return 45; }
};
} // namespace Adl
static_assert(⊘Adl::A{1} == 101);
static_assert(⊗Adl::H{} == 45);

// A dependent operand: the use is rebuilt at instantiation as an *operator*,
// so the member candidate -- a property of the operator syntax, not of the
// call -- survives (U16).
template <class T> constexpr int applied(T a) { return ⊕a; }
static_assert(applied(Mem{2}) == 11);
template <class T> constexpr int applied_adl(T a) { return ⊘a; }
static_assert(applied_adl(Adl::A{3}) == 103);
template <class T> constexpr int applied_twice(T a) { return ⊖⊖a; }
static_assert(applied_twice(1) == 13);

template <class T> concept Negatable = requires(T a) { ⊕a; };
static_assert(Negatable<Mem>);
static_assert(!Negatable<int>);

//===----------------------------------------------------------------------===//
// 6. The default-argument corner (U08's open item, DEV-U06(b))
//===----------------------------------------------------------------------===//
//
// [over.oper]p8 forbids default arguments on operator functions; DEV-U06
// records that a user operator waives that restriction, because
// CheckUserOperatorDeclaration only checks arity. The consequence, measured
// here rather than argued: a *two*-parameter operator whose second parameter
// has a default argument is one-argument-viable, so it can be used in prefix
// position as well as infix. Declared arity does not select the form; call
// viability does.
//
// This is the behaviour U6/§17.4 requires, not a leak: `⊟5` desugars to the
// call `operator⊟(5)`, and the operator form must find exactly what that call
// finds. It is nevertheless the one place where U5's "arity selects the form"
// is not literally true of a *use*, and it is what [over.oper]p8 exists to
// prevent for the built-in operator tokens.

constexpr int operator⊟(int a, int b = 1) { return 7 * a + b; }
static_assert(⊟5 == 36);          // prefix use of a two-parameter operator
static_assert(2 ⊟ 3 == 17);       // and the infix use is unaffected
static_assert(operator⊟(5) == 36);  // the explicit call finds the same thing

//===----------------------------------------------------------------------===//
// 7. AST shape
//===----------------------------------------------------------------------===//

int ast_prefix(int a) { return ⊖a; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_prefix
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296
// CHECK:       DeclRefExpr {{.*}} 'operator⊖' 'int (int)'

// Stacked: two prefix nodes, one inside the other.
int ast_stacked(int a) { return ⊖⊖a; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_stacked
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296

// One code point, both fixities, one expression: the outer node is the infix
// use and the inner one is the prefix use, and the node records which is
// which.
int ast_both(int a, int b) { return ⊖a ⊖ b; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_both
// CHECK:       UserOperatorExpr {{.*}} infix '⊖' U+2296
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296

// Tighter than '*': the outer node is the builtin multiply.
int ast_tighter(int a, int b) { return ⊖a * b; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_tighter
// CHECK:       BinaryOperator {{.*}} '*'
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296

// The member form's operand is the object argument.
int ast_member(Mem m) { return ⊕m; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_member
// CHECK:       UserOperatorExpr {{.*}} prefix '⊕' U+2295
// CHECK-NEXT:  CXXMemberCallExpr
// CHECK-NEXT:  MemberExpr {{.*}} .operator⊕

#ifdef ERRORS
// There is no postfix form (U5), and a postfix *use* is what diagnoses the
// attempt: nothing about the declaration says postfix was meant, so U08
// deliberately says nothing there. Position makes `a⊖` an infix use with its
// right operand missing, and the diagnostic is the ordinary missing-operand
// one -- the same text and the same caret as `a +;`. It does not mention
// postfix, because the parser has no way to know that is what was wanted.
int postfix(int a) { return a⊖; }
// err-error@-1 {{expected expression}}

int postfix_spaced(int a) { return a ⊖ ; }
// err-error@-1 {{expected expression}}

// A prefix use with no operand at all is the same diagnostic.
int no_operand() { return ⊖; }
// err-error@-1 {{expected expression}}

// A use of an operator nobody declared is a lookup failure naming the
// operator, never a lexing or parsing failure (U3) -- in operand position
// exactly as in operator position.
int undeclared(int a) { return ⊠a; }
// err-error@-1 {{use of undeclared 'operator⊠'}}

// The operand is a cast-expression, so a braced-init-list is not one.
int braced() { return ⊖{1, 2}; }
// err-error@-1 {{expected expression}}

// A prefix overload and a defaulted-second-parameter infix overload of the
// same code point make a prefix use ambiguous -- the ordinary ambiguity for
// `f(x)` given `f(int)` and `f(int, int = 1)`, reached through operator
// syntax. See section 6.
constexpr int operator⊡(int a) { return 1000 + a; }
constexpr int operator⊡(int a, int b = 1) { return 7 * a + b; }
constexpr int ambiguous = ⊡5;
// err-error@-1 {{call to 'operator⊡' is ambiguous}}
// err-note@-4 {{candidate function}}
// err-note@-4 {{candidate function}}
#endif

// --- composability with -fbacktick (U7) -----------------------------------
// The prefix form binds tighter than the shared user-infix level whichever
// spelling occupies it. Everything above this point also runs with -fbacktick
// off, which is the other direction of the same claim.
#ifdef BACKTICK
constexpr int f(int a, int b) { return 5 * a + b; }

// f(⊖2, 3) == 5*7 + 3; a looser prefix form would be ⊖(2 `f` 3) == 3*13 + 1.
static_assert(⊖2 `f` 3 == 38);
static_assert(1 `f` ⊖2 == 12);      // f(1, ⊖2) == 5 + 7
static_assert(⊖1 `f` ⊖2 == 27);     // f(4, 7)
static_assert(⊖1 ⊞ 2 `f` 3 == 53);  // f(⊖1 ⊞ 2, 3) == 5*10 + 3

int ast_mixed(int a, int b) { return ⊖a `f` b; }
// BT-LABEL: FunctionDecl {{.*}} ast_mixed
// BT:       BacktickInfixExpr
// BT:       UserOperatorExpr {{.*}} prefix '⊖' U+2296
#endif

#else
// --- flag off: unchanged (the token does not exist) -----------------------
int off(int a) { return ⊖a; }
// off-error@-1 {{unexpected character '⊖' U+2296}}
#endif
