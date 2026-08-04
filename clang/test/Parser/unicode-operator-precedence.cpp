// Precedence and associativity of Unicode user-defined operators (U4/U9,
// U§6): **one** precedence level for all user-introduced infix, left
// associative, operands are cast-expressions, prefix binds tighter than any
// binary operator. This file is the consolidated grammar evidence; U11's
// Parser/unicode-operator-infix.cpp and U12's Parser/unicode-operator-prefix.cpp
// establish the two productions, and SemaCXX/unicode-operator-{call,adl,
// semantics}.cpp own which function gets called and what happens when it does.
//
// Two oracles, deliberately:
//
//   * **values** -- every operator is constexpr and neither commutative nor
//     associative (2a+b, 3a+b, 5a+b, 3a+1), so a wrong grouping is a wrong
//     number. The same constants U11 and U12 use, so the numbers in the three
//     Parser files can be read against each other.
//   * **types** -- the same operators are overloaded on Tag<N>, computing the
//     same arithmetic in the *type* system, so a grouping claim can be written
//     as `__is_same(decltype(EXPR), Tag<N>)`. That form survives AST-printing
//     changes, and it lets every chain be asserted equal to its explicitly
//     parenthesized left-nested form -- which is what "left-associative, one
//     level" means, stated once per chain rather than argued from a number.
//
// U9 says there is no user-declared precedence, ever. The evidence for that is
// negative and is spread through the whole file: no declaration below says
// anything about binding, the declaration *order* of ⊞ and ⊗ is arbitrary, and
// every mixed chain groups strictly left-to-right regardless.
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -DERRORS -fsyntax-only -verify=err %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-dump %s | FileCheck %s
//
// Item 9 of the sweep -- "the same expressions parse identically whether or not
// another user-introduced infix feature sharing prec::UserInfix is also
// enabled" -- was checked on the prototype branch as a byte-for-byte diff of
// two -ast-print runs. There is no second such feature in tree, so there is
// nothing to compare against: the item is vacuous here rather than untested.

// expected-no-diagnostics

//===----------------------------------------------------------------------===//
// 0. The operators
//===----------------------------------------------------------------------===//

template <int N> struct Tag { static constexpr int value = N; };

// The value oracle.
constexpr int operator⊞(int a, int b) { return 2 * a + b; }
constexpr int operator⊗(int a, int b) { return 3 * a + b; }
constexpr int operator⊖(int a) { return 3 * a + 1; }

// The type oracle: the same arithmetic, one level up. Nothing here declares,
// requests or hints at a precedence -- there is no syntax with which to.
template <int A, int B> constexpr Tag<2 * A + B> operator⊞(Tag<A>, Tag<B>) { return {}; }
template <int A, int B> constexpr Tag<3 * A + B> operator⊗(Tag<A>, Tag<B>) { return {}; }
template <int A> constexpr Tag<3 * A + 1> operator⊖(Tag<A>) { return {}; }

// U04/U11: a redeclaration of the same operator, spelled as a
// universal-character-name. If the two spellings did not collapse to one code
// point this would declare a *second*, never-defined operator, and every
// assertion in this file that uses `⊞` in a UCN spelling below would fail to
// evaluate. The declaration is free; it makes the whole file's precedence
// evidence hold for UCN spellings too, since the grammar level is a property
// of the token kind and not of how the token was written.
constexpr int operator\U0000229E(int a, int b);
static_assert(1 \u229E 2 == 4);
static_assert(1 \N{SQUARED PLUS} 2 \u229E 3 == 11);   // still left-associative
static_assert(\u2296 1 \u229E 2 == 10);               // still prefix-tighter

constexpr Tag<1> a{};
constexpr Tag<2> b{};
constexpr Tag<3> c{};
constexpr Tag<4> d{};

//===----------------------------------------------------------------------===//
// 1. Left-associative (U4)
//===----------------------------------------------------------------------===//

// Right-associative would be 2*1 + (2*2 + 3) == 9.
static_assert(1 ⊞ 2 ⊞ 3 == 11);
static_assert(__is_same(decltype(a ⊞ b ⊞ c), Tag<11>));
static_assert(__is_same(decltype(a ⊞ b ⊞ c), decltype((a ⊞ b) ⊞ c)));
static_assert(!__is_same(decltype(a ⊞ b ⊞ c), decltype(a ⊞ (b ⊞ c))));

// Four terms, one operator: strictly left-nested, not a balanced tree and not
// right-nested.
static_assert(1 ⊞ 2 ⊞ 3 ⊞ 4 == 26);
static_assert(__is_same(decltype(a ⊞ b ⊞ c ⊞ d), decltype(((a ⊞ b) ⊞ c) ⊞ d)));

//===----------------------------------------------------------------------===//
// 2. One level for all user-introduced infix (U4, U9)
//===----------------------------------------------------------------------===//
//
// Two *different* user operators in one chain still group left to right. If
// there were an inter-operator precedence -- any at all, however it were
// spelled -- one of the two orders below would bracket the other way.

static_assert(1 ⊞ 2 ⊗ 3 == 15);   // (1 ⊞ 2) ⊗ 3 == 3*4 + 3
static_assert(1 ⊗ 2 ⊞ 3 == 13);   // (1 ⊗ 2) ⊞ 3 == 2*5 + 3
static_assert(__is_same(decltype(a ⊞ b ⊗ c), Tag<15>));
static_assert(__is_same(decltype(a ⊗ b ⊞ c), Tag<13>));
static_assert(__is_same(decltype(a ⊞ b ⊗ c), decltype((a ⊞ b) ⊗ c)));
static_assert(__is_same(decltype(a ⊗ b ⊞ c), decltype((a ⊗ b) ⊞ c)));

// Four terms, two operators, both interleavings. The two chains are token-wise
// mirror images and both nest left; a two-level table would make them differ in
// shape, not merely in value.
static_assert(1 ⊞ 2 ⊗ 3 ⊞ 4 == 34);
static_assert(1 ⊗ 2 ⊞ 3 ⊗ 4 == 43);
static_assert(__is_same(decltype(a ⊞ b ⊗ c ⊞ d), decltype(((a ⊞ b) ⊗ c) ⊞ d)));
static_assert(__is_same(decltype(a ⊗ b ⊞ c ⊗ d), decltype(((a ⊗ b) ⊞ c) ⊗ d)));

// The level does not depend on which operator was declared first: ⊞ was
// declared before ⊗ above, and ⊚ is declared *here*, after every use of the
// other two, with the same result.
constexpr int operator⊚(int x, int y) { return 7 * x + y; }
template <int A, int B> constexpr Tag<7 * A + B> operator⊚(Tag<A>, Tag<B>) { return {}; }
static_assert(1 ⊞ 2 ⊚ 3 ⊗ 4 == 97);   // ((1 ⊞ 2) ⊚ 3) ⊗ 4 == 3*31 + 4
static_assert(__is_same(decltype(a ⊞ b ⊚ c ⊗ d), decltype(((a ⊞ b) ⊚ c) ⊗ d)));

//===----------------------------------------------------------------------===//
// 3. Tighter than every built-in binary operator (D2 Option A / §4)
//===----------------------------------------------------------------------===//
//
// The user-infix level is the highest-precedence *binary* level, so in each
// pair below the ⊞ groups first no matter which side of the built-in it is on.

static_assert(2 * 3 ⊞ 4 == 20);       // 2 * (3 ⊞ 4)
static_assert(2 ⊞ 3 * 4 == 28);       // (2 ⊞ 3) * 4
static_assert(12 / 3 ⊞ 4 == 1);       // 12 / (3 ⊞ 4) == 12 / 10
static_assert(13 % 3 ⊞ 4 == 3);       // 13 % 10
static_assert(1 + 2 ⊞ 3 == 8);        // 1 + (2 ⊞ 3)
static_assert(10 - 2 ⊞ 3 == 3);       // 10 - (2 ⊞ 3)
static_assert((1 << 1 ⊞ 2 ) == 16);   // 1 << (1 ⊞ 2) == 1 << 4
static_assert((256 >> 1 ⊞ 2) == 16);  // 256 >> 4
static_assert((1 ⊞ 2 < 5) == true);   // (1 ⊞ 2) < 5
static_assert((1 ⊞ 2 > 5) == false);
static_assert(2 ⊞ 3 == 7);            // (2 ⊞ 3) == 7
static_assert((12 & 1 ⊞ 2) == 4);     // 12 & (1 ⊞ 2)
static_assert((12 ^ 1 ⊞ 2) == 8);
static_assert((8 | 1 ⊞ 2) == 12);

// '&&' and '||' with non-constant operands, so -Wconstant-logical-operand
// stays out of the way. The claim is the same: the ⊞ is the operand.
constexpr bool land(int x, int y) { return x ⊞ y && y; }
constexpr bool lor(int x, int y) { return x ⊞ y || y; }
static_assert(land(1, 0) == false);   // (1 ⊞ 0) && 0
static_assert(lor(0, 0) == false);    // (0 ⊞ 0) || 0
static_assert(lor(1, 0) == true);     // (1 ⊞ 0) || 0 == 2 || 0

//===----------------------------------------------------------------------===//
// 4. Looser than unary: both operands are cast-expressions (§4 symmetry)
//===----------------------------------------------------------------------===//
//
// U§6's first worked example. The operand grammar is a cast-expression on
// *both* sides -- the property D2 chose Option A for -- so a prefix built-in
// binds to its own operand and the shape is symmetric. If the level bound
// tighter than unary these would be -(1 ⊞ -2) == 0 and -(1 ⊞ 2) == -4.

static_assert(-1 ⊞ -2 == -4);
static_assert(-1 ⊞ 2 == 0);
static_assert(1 ⊞ -2 == 0);
static_assert(+1 ⊞ +2 == 4);
static_assert(!0 ⊞ 1 == 3);
static_assert(~0 ⊞ 1 == -1);
static_assert(sizeof(int) ⊞ 0 == 8);

// Symmetry stated as a shape rather than as a number: the two operands of
// `-a ⊞ -b` are the two unary expressions, and nothing regroups.
static_assert(__is_same(decltype(-1 ⊞ -2), decltype((-1) ⊞ (-2))));

//===----------------------------------------------------------------------===//
// 5. A prefix user operator binds tighter than any binary (U5, U§6)
//===----------------------------------------------------------------------===//

static_assert(⊖1 ⊞ 2 == 10);          // operator⊞(⊖1, 2) == 2*4 + 2
static_assert(1 ⊞ ⊖2 == 9);           // operator⊞(1, ⊖2) == 2 + 7
static_assert(⊖1 ⊞ ⊖2 == 15);
static_assert(__is_same(decltype(⊖a ⊞ b), decltype((⊖a) ⊞ b)));
static_assert(__is_same(decltype(a ⊞ ⊖b), decltype(a ⊞ (⊖b))));

// **Three binding strengths in one expression**, which U§6's worked examples
// never show together and which is the shape that trips readers: prefix binds
// tighter than the user-infix level, which binds tighter than '*'. So
// `⊖a ⊞ 2 * ⊖b` is `((⊖a) ⊞ 2) * (⊖b)` -- the '*' is the *outermost* operator,
// even though it is written between two user-operator uses.
static_assert(⊖1 ⊞ 2 * ⊖2 == 70);     // ((3*1+1) ⊞ 2) * (3*2+1) == 10 * 7
static_assert(⊖1 ⊞ (2 * ⊖2) == 22);   // the reading most people expect: 2*4 + 14
static_assert(⊖2 * 3 ⊞ 4 == 7 * 10);  // (⊖2) * (3 ⊞ 4)

// Stacked prefix uses stay tighter than the level however deep.
static_assert(⊖⊖1 ⊞ 2 == 28);         // 2*13 + 2

//===----------------------------------------------------------------------===//
// 6. Relation to the levels below (item 6)
//===----------------------------------------------------------------------===//

// Equality and relational: already above, restated as the classic shape.
static_assert((1 ⊞ 2 == 2 ⊞ 2) == false);   // 4 == 6
static_assert((1 ⊞ 2 == 1 ⊞ 2) == true);

// Assignment: the whole chain is the right-hand side, and '=' is still
// right-associative through it.
constexpr int assigns() {
  int x = 0, y = 0;
  x = 1 ⊞ 2 ⊞ 3;       // x = ((1 ⊞ 2) ⊞ 3)
  y = x ⊞ 1;
  x += 1 ⊞ 0;          // compound assignment
  return 100 * x + y;
}
static_assert(assigns() == 1323);   // x == 13, y == 23

constexpr int assign_chain() {
  int x = 0, y = 0;
  x = y = 1 ⊞ 2;       // x = (y = (1 ⊞ 2))
  return x + y;
}
static_assert(assign_chain() == 8);

// The *comma operator* is looser, which is a different claim from "the comma
// in an argument list is not an operator" (U11's file makes that one).
constexpr int comma_op() {
  int x = 0;
  int y = (x = 1 ⊞ 2, 3 ⊞ 4);   // both sides are complete chains
  return 100 * y + x;
}
static_assert(comma_op() == 1004);  // y == 10, x == 4

// Conditional: the chain is a complete operand in all three positions.
static_assert((1 ⊞ 2 ? 3 ⊞ 4 : 5 ⊞ 6) == 10);
static_assert((0 ⊞ 0 ? 3 ⊞ 4 : 5 ⊞ 6) == 16);
static_assert((true ? 1 : 2) ⊞ 3 == 5);
// A conditional as an *operand* has to be parenthesized -- it is not a
// cast-expression -- which is the same rule every binary operator imposes.
static_assert(__is_same(decltype((true ? a : a) ⊞ b), Tag<4>));

//===----------------------------------------------------------------------===//
// 7. Postfix and cast operands (item 7)
//===----------------------------------------------------------------------===//
//
// The operand is a cast-expression, so every postfix suffix and every
// explicit cast binds tighter than the operator on either side.

constexpr int v1 = 3, v2 = 4;
constexpr const int *p = &v1, *q = &v2;
static_assert(*p ⊞ *q == 10);                 // unary '*' on both operands

struct S {
  int m;
  constexpr int mf() const { return m + 1; }
};
constexpr S s1{3}, s2{4};
constexpr const S *ps = &s1;
static_assert(s1.m ⊞ s2.m == 10);             // member access
static_assert(ps->m ⊞ s2.m == 10);            // arrow
static_assert(s1.mf() ⊞ s2.mf() == 13);       // member call

constexpr int g(int x) { return x + 1; }
static_assert(g(2) ⊞ g(3) == 10);             // call
constexpr int arr[] = {1, 2, 3};
static_assert(arr[1] ⊞ arr[2] == 7);          // subscript
static_assert((int)1.9 ⊞ (int)2.9 == 4);      // C-style cast
static_assert(static_cast<int>(1.9) ⊞ 2 == 4);// named cast (a postfix-expression)

// Increment/decrement, in a context where they mean something. The two
// operands touch different objects, so this is about binding and not about
// DEV-U16's sequencing question.
constexpr int incr() {
  int i = 3, j = 4;
  int r = i++ ⊞ j++;      // (i++) ⊞ (j++) == 2*3 + 4
  return r + i + j;
}
static_assert(incr() == 19);

//===----------------------------------------------------------------------===//
// 8. Parentheses regroup, everywhere (item 10)
//===----------------------------------------------------------------------===//
//
// Sections 1-7 assert each chain *equal* to its natural left-nested
// parenthesization. This section is the other half: a different
// parenthesization must give a different parse, or the equalities above would
// be vacuous.

static_assert(__is_same(decltype(a ⊞ (b ⊞ c)), Tag<9>));      // vs Tag<11>
static_assert(__is_same(decltype(a ⊞ (b ⊗ c)), Tag<11>));     // vs Tag<15>
static_assert(__is_same(decltype(a ⊗ (b ⊞ c)), Tag<10>));     // vs Tag<13>
static_assert(__is_same(decltype(a ⊞ (b ⊗ c ⊞ d)), Tag<24>));
static_assert(⊖(1 ⊞ 2) == 13);                                // vs ⊖1 ⊞ 2 == 10
static_assert((2 * 3) ⊞ 4 == 16);                             // vs 2 * 3 ⊞ 4 == 20
static_assert(2 * (3 ⊞ 4) == 20);
static_assert(-(1 ⊞ -2) == 0);                                // vs -1 ⊞ -2 == -4

//===----------------------------------------------------------------------===//
// 9. Not a fold operator (DEV-U11 part 3, U§13)
//===----------------------------------------------------------------------===//
//
// A user operator is not in `isFoldOperator`'s set: prec::UserInfix is
// excluded wholesale, so a fold-expression cannot be written over one. The
// behaviour is pinned here, not endorsed -- U§13 lists the question as open.

#ifdef ERRORS
template <int... N> constexpr int lfold() { return (... ⊞ N); }
// err-error@-1 {{expected expression}}
template <int... N> constexpr int rfold() { return (N ⊞ ...); }
// err-error@-1 {{expected expression}}

// The built-in operator at the neighbouring precedence level folds fine, so
// this is about the level's membership in the fold set and nothing else.
template <int... N> constexpr int plusfold() { return (... + N); }
static_assert(plusfold<1, 2, 3>() == 6);
#endif

//===----------------------------------------------------------------------===//
// 11a. The grouping is printable, whatever the operands are
//===----------------------------------------------------------------------===//
//
// Every Tag assertion above puts a *chain* inside a `decltype` that the
// -ast-dump and -ast-print RUN lines then have to print, so the whole file is
// already this test for the trivially-destructible case. `Res` closes the gap:
// a class with a non-trivial destructor, so the call's result is bound to a
// temporary. UserOperatorExpr stores its operands, so there is no "the child is
// always a CallExpr" invariant to be wrong about, and both fixities print.
//
// That is the design argument in miniature: an expression node that *is* the
// operator has no invariant about its child to get wrong, where a node that
// merely wraps a call does.

struct Res {
  int v;
  ~Res();
};
Res operator⊢(Res, Res);
Res operator⊢(Res);
using ChainType = decltype(Res{1} ⊢ Res{2} ⊢ Res{3});
using PrefixChainType = decltype(⊢⊢Res{1} ⊢ Res{2});
// CHECK: TypeAliasDecl {{.*}} ChainType 'decltype(Res{1} ⊢ Res{2} ⊢ Res{3})'
// CHECK: TypeAliasDecl {{.*}} PrefixChainType 'decltype(⊢⊢Res{1} ⊢ Res{2})'

//===----------------------------------------------------------------------===//
// 11. AST shape
//===----------------------------------------------------------------------===//
//
// The values and types above are the real assertions; these pin the *tree*,
// which is what a reader of the paper will picture. Outer node first.

// Left-associative: the outer call is the second operator.
int ast_chain(int x, int y, int z) { return x ⊞ y ⊗ z; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_chain
// CHECK:       UserOperatorExpr {{.*}} infix '⊗' U+2297
// CHECK:       UserOperatorExpr {{.*}} infix '⊞' U+229E

// Tighter than '*': the built-in multiply is outermost.
int ast_tighter(int x, int y, int z) { return x * y ⊞ z; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_tighter
// CHECK:       BinaryOperator {{.*}} '*'
// CHECK:       UserOperatorExpr {{.*}} infix '⊞' U+229E

// Three strengths: '*' outermost, then the user-infix level, then the prefix
// use inside its left operand.
int ast_three(int x, int y) { return ⊖x ⊞ 2 * ⊖y; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_three
// CHECK:       BinaryOperator {{.*}} '*'
// CHECK:       UserOperatorExpr {{.*}} infix '⊞' U+229E
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296
// CHECK:       UserOperatorExpr {{.*}} prefix '⊖' U+2296
