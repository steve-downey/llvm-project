// Unicode user-defined operators: the AST node and -ast-print fidelity (U16).
//
// A user-operator use prints back as it was written -- `a ⊞ b` -- and not as
// the call `operator⊞(a, b)` it desugars to (U7). The node that makes that
// possible, UserOperatorExpr, is not merely cosmetic: it is what carries the
// operator syntax across template instantiation, so section 3 below is the
// load-bearing half of this file.
//
// Print, then re-parse the printed output and print again: the two must agree.
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-print %s > %t.print.cpp
// RUN: FileCheck --input-file=%t.print.cpp %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-print %t.print.cpp > %t.reprint.cpp
// RUN: diff -u %t.print.cpp %t.reprint.cpp
//
// The printed form is also what the *compiler* believes: it compiles, and to
// the same answers (every assertion below is a static_assert).
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only %t.print.cpp
//
// The node itself, and the operator's identity, in -ast-dump.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-dump %s \
// RUN:   | FileCheck %s --check-prefix=DUMP
//
// U04/U11 spelling independence, as a diff: -DSPELL_UCN respells section 6's
// operators as universal-character-names and changes nothing else. No spelling
// is stored -- printOperator() re-encodes the code point as UTF-8 -- so the
// two printed ASTs must be byte-identical. This is the one-command form of
// "all spellings are the same operator", and it is the assertion U16 could not
// write because U04 had not landed.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -DSPELL_UCN -ast-print %s > %t.ucn.cpp
// RUN: diff -u %t.print.cpp %t.ucn.cpp
// RUN: %clang_cc1 -std=c++23 -funicode-operators -DSPELL_UCN -fsyntax-only -verify %s

// expected-no-diagnostics

// Section 6 is compiled twice, once per spelling. The macro bodies below are
// already *tokens* -- a UCN designating a U1 code point forms the operator
// token in phase 3, before macro replacement -- so this is a spelling switch
// and not a rewriting of the grammar.
#ifdef SPELL_UCN
#define OP_SQ \N{SQUARED PLUS}
#define OP_CT \U00002297
#define OP_CM \u2296
#else
#define OP_SQ ⊞
#define OP_CT ⊗
#define OP_CM ⊖
#endif

//===----------------------------------------------------------------------===//
// 1. The infix form, as written
//===----------------------------------------------------------------------===//

constexpr int operator⊞(int a, int b) { return 2 * a + b; }
constexpr int operator⊗(int a, int b) { return 3 * a + b; }

constexpr int infix(int a, int b) { return a ⊞ b; }
// CHECK-LABEL: constexpr int infix(int a, int b) {
// CHECK-NEXT:  return a ⊞ b;

// A chain is left-associative (D1/U4) and needs no parentheses to print back.
constexpr int chain(int a, int b, int c) { return a ⊞ b ⊞ c; }
// CHECK-LABEL: constexpr int chain(int a, int b, int c) {
// CHECK-NEXT:  return a ⊞ b ⊞ c;
static_assert(chain(1, 2, 3) == 11);

// Parentheses are in the AST and survive; without them the chain would group
// the other way and give a different answer.
constexpr int parens(int a, int b, int c) { return a ⊞ (b ⊞ c); }
// CHECK-LABEL: constexpr int parens(int a, int b, int c) {
// CHECK-NEXT:  return a ⊞ (b ⊞ c);
static_assert(parens(1, 2, 3) == 9);

// Two different user operators chain at one level, left to right.
constexpr int mixed_ops(int a, int b, int c) { return a ⊞ b ⊗ c; }
// CHECK-LABEL: constexpr int mixed_ops(int a, int b, int c) {
// CHECK-NEXT:  return a ⊞ b ⊗ c;
static_assert(mixed_ops(1, 2, 3) == 15);

// The level is tighter than `*` and looser than unary (U4), so neither of
// these needs a parenthesis to print back as the tree it came from.
constexpr int tighter_than_star(int a, int b) { return 2 * a ⊞ b; }
// CHECK-LABEL: constexpr int tighter_than_star(int a, int b) {
// CHECK-NEXT:  return 2 * a ⊞ b;
static_assert(tighter_than_star(3, 4) == 20);

constexpr int symmetric_prefix(int a, int b) { return -a ⊞ -b; }
// CHECK-LABEL: constexpr int symmetric_prefix(int a, int b) {
// CHECK-NEXT:  return -a ⊞ -b;
static_assert(symmetric_prefix(1, 2) == -4);

// The operands are cast-expressions (U4), and print back as such.
constexpr int operands(int a) { return (int)1.5 ⊞ ~a ⊞ (a ? 1 : 2); }
// CHECK-LABEL: constexpr int operands(int a) {
// CHECK-NEXT:  return (int)1.5 ⊞ ~a ⊞ (a ? 1 : 2);
static_assert(operands(0) == 4);

//===----------------------------------------------------------------------===//
// 1a. The prefix form (U5)
//===----------------------------------------------------------------------===//
//
// The node records its arity rather than deriving it, so the printer knows to
// put the glyph before its single operand -- and a prefix and an infix use of
// the *same* code point print back distinguishably.

constexpr int operator⊖(int a) { return 3 * a + 1; }
constexpr int operator⊖(int a, int b) { return 100 * a + b; }

constexpr int prefix(int a) { return ⊖a; }
// CHECK-LABEL: constexpr int prefix(int a) {
// CHECK-NEXT:  return ⊖a;
static_assert(prefix(1) == 4);

// Stacked prefix uses need no parentheses to print back.
constexpr int stacked(int a) { return ⊖⊖a; }
// CHECK-LABEL: constexpr int stacked(int a) {
// CHECK-NEXT:  return ⊖⊖a;
static_assert(stacked(1) == 13);

// One code point, both fixities, one expression -- and the printed form
// re-parses to the same tree, which is what the reprint diff checks.
constexpr int both_fixities(int a, int b) { return ⊖a ⊖ b; }
// CHECK-LABEL: constexpr int both_fixities(int a, int b) {
// CHECK-NEXT:  return ⊖a ⊖ b;
static_assert(both_fixities(1, 2) == 402);

// Tighter than the user-infix level, which is itself tighter than `*`, so no
// parentheses appear: this is (⊖a ⊞ 2) * (⊖b).
constexpr int prefix_binding(int a, int b) { return ⊖a ⊞ 2 * ⊖b; }
// CHECK-LABEL: constexpr int prefix_binding(int a, int b) {
// CHECK-NEXT:  return ⊖a ⊞ 2 * ⊖b;
static_assert(prefix_binding(1, 2) == 70);

constexpr int prefix_parens(int a, int b) { return ⊖(a ⊞ b); }
// CHECK-LABEL: constexpr int prefix_parens(int a, int b) {
// CHECK-NEXT:  return ⊖(a ⊞ b);
static_assert(prefix_parens(1, 2) == 13);

//===----------------------------------------------------------------------===//
// 2. The member form
//===----------------------------------------------------------------------===//

struct Mem {
  int v;
  constexpr int operator⊕(Mem o) const { return v + o.v + 9; }
  constexpr int operator⊖() const { return v + 50; }
};

constexpr int member(Mem a, Mem b) { return a ⊕ b; }
// CHECK-LABEL: constexpr int member(Mem a, Mem b) {
// CHECK-NEXT:  return a ⊕ b;
static_assert(member(Mem{1}, Mem{2}) == 12);

// The member form's operand is the object argument, so it prints back the same
// way even when a temporary and an implicit const conversion sit between the
// node and the operand as written.
constexpr int member_temporary() { return Mem{1} ⊕ Mem{2}; }
// CHECK-LABEL: constexpr int member_temporary() {
// CHECK-NEXT:  return Mem{1} ⊕ Mem{2};
static_assert(member_temporary() == 12);

// A member *prefix* operator is declared with no parameters (U5), and its
// single operand is the object argument.
constexpr int member_prefix(Mem a) { return ⊖a; }
// CHECK-LABEL: constexpr int member_prefix(Mem a) {
// CHECK-NEXT:  return ⊖a;
static_assert(member_prefix(Mem{1}) == 51);

//===----------------------------------------------------------------------===//
// 3. Templates -- where the node stops being cosmetic
//===----------------------------------------------------------------------===//
//
// (The return types are written out rather than deduced only because
// -ast-print renders an `auto`-returning template's instantiation with the
// deduced type, which does not re-parse against the primary -- a pre-existing
// printer limitation with nothing to do with user operators.)
//
// A dependent use must be rebuilt at instantiation as an *operator*, not as a
// call: ADL is a property of the call and survives either way, but member
// candidates are a property of the operator syntax and are lost if the node
// does not carry it. Both halves are asserted here.

template <class T> constexpr int dependent(T a, T b) { return a ⊞ b; }
// CHECK-LABEL: template <class T> constexpr int dependent(T a, T b) {
// CHECK-NEXT:  return a ⊞ b;
static_assert(dependent(1, 2) == 4);

// The instantiation prints as the operator too, not as the resolved call.
// CHECK-LABEL: template<> constexpr int dependent<int>(int a, int b) {
// CHECK-NEXT:  return a ⊞ b;

template <class T> constexpr int dependent_member(T a, T b) { return a ⊕ b; }
// CHECK-LABEL: template <class T> constexpr int dependent_member(T a, T b) {
// CHECK-NEXT:  return a ⊕ b;
static_assert(dependent_member(Mem{1}, Mem{2}) == 12);

// CHECK-LABEL: template<> constexpr int dependent_member<Mem>(Mem a, Mem b) {
// CHECK-NEXT:  return a ⊕ b;

// The prefix form travels the same way: a dependent prefix use is rebuilt at
// instantiation as an operator, so the *member* candidate survives.
template <class T> constexpr int dependent_prefix(T a) { return ⊖a; }
// CHECK-LABEL: template <class T> constexpr int dependent_prefix(T a) {
// CHECK-NEXT:  return ⊖a;
static_assert(dependent_prefix(1) == 4);
static_assert(dependent_prefix(Mem{1}) == 51);

// CHECK-LABEL: template<> constexpr int dependent_prefix<int>(int a) {
// CHECK-NEXT:  return ⊖a;
// CHECK-LABEL: template<> constexpr int dependent_prefix<Mem>(Mem a) {
// CHECK-NEXT:  return ⊖a;

// ADL from the instantiation context still reaches a non-member declared
// nowhere the template can see.
namespace Adl {
struct A { int v; };
constexpr int operator⊘(A x, A y) { return x.v + y.v + 100; }
} // namespace Adl
template <class T> constexpr int dependent_adl(T a, T b) { return a ⊘ b; }
static_assert(dependent_adl(Adl::A{1}, Adl::A{2}) == 103);

// Member and non-member candidates are ranked in one set at instantiation,
// both ways round -- the observable that distinguishes this from a
// member-first fallback.
struct Both {
  constexpr int operator⊙(long) const { return 1; }
};
constexpr int operator⊙(Both, int) { return 2; }
template <class T, class U> constexpr int ranked(T a, U b) { return a ⊙ b; }
static_assert(ranked(Both{}, 0) == 2);
static_assert(ranked(Both{}, 0L) == 1);

// A requires-expression over the member form is satisfied.
template <class T> concept MemberCombinable = requires(T a, T b) { a ⊕ b; };
// CHECK: concept MemberCombinable = requires (T a, T b) { a ⊕ b; };
static_assert(MemberCombinable<Mem>);
static_assert(!MemberCombinable<int>);

template <class T> concept Combinable = requires(T a, T b) { a ⊞ b; };
static_assert(Combinable<int>);
static_assert(!Combinable<Mem>);

//===----------------------------------------------------------------------===//
// 4. Where a user-operator use appears inside another construct
//===----------------------------------------------------------------------===//

static_assert((5 ⊞ 7) == 17);
// CHECK: static_assert((5 ⊞ 7) == 17);

constexpr int in_arguments(int a) { return infix(a ⊞ 1, a ⊗ 2); }
// CHECK-LABEL: constexpr int in_arguments(int a) {
// CHECK-NEXT:  return infix(a ⊞ 1, a ⊗ 2);

constexpr bool in_condition(int a) { return (a ⊞ 1) > 3 && !(a ⊗ 1); }
// CHECK-LABEL: constexpr bool in_condition(int a) {
// CHECK-NEXT:  return (a ⊞ 1) > 3 && !(a ⊗ 1);

struct Ref { int v; };
constexpr Ref &operator⊚(Ref &a, Ref &) { return a; }
constexpr int assigned(Ref a, Ref b) { return ((a ⊚ b).v = 7); }
// CHECK-LABEL: constexpr int assigned(Ref a, Ref b) {
// CHECK-NEXT:  return ((a ⊚ b).v = 7);

//===----------------------------------------------------------------------===//
// 5. The node in -ast-dump
//===----------------------------------------------------------------------===//
//
// The dump names the node, the fixity and the operator's identity -- the code
// point, which is the operator's whole identity: no spelling is stored, so a
// use spelled with a universal-character-name would print and dump the same
// way (U11; the UCN spelling itself arrives with U04).

// DUMP-LABEL: FunctionDecl {{.*}} infix
// DUMP:         UserOperatorExpr {{.*}} 'int' infix '⊞' U+229E
// DUMP-NEXT:      CallExpr {{.*}} 'int'
// DUMP-NEXT:        ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
// DUMP-NEXT:          DeclRefExpr {{.*}} 'operator⊞'

// DUMP-LABEL: FunctionDecl {{.*}} prefix
// DUMP:         UserOperatorExpr {{.*}} 'int' prefix '⊖' U+2296
// DUMP-NEXT:      CallExpr {{.*}} 'int'
// DUMP-NEXT:        ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
// DUMP-NEXT:          DeclRefExpr {{.*}} 'operator⊖'

// One code point, two fixities, one expression: the arity is on the node, so
// the dump tells them apart.
// DUMP-LABEL: FunctionDecl {{.*}} both_fixities
// DUMP:         UserOperatorExpr {{.*}} 'int' infix '⊖' U+2296
// DUMP:           UserOperatorExpr {{.*}} 'int' prefix '⊖' U+2296

// DUMP-LABEL: FunctionDecl {{.*}} member
// DUMP:         UserOperatorExpr {{.*}} 'int' infix '⊕' U+2295
// DUMP-NEXT:      CXXMemberCallExpr {{.*}} 'int'
// DUMP-NEXT:        MemberExpr {{.*}} .operator⊕

// DUMP-LABEL: FunctionDecl {{.*}} member_prefix
// DUMP:         UserOperatorExpr {{.*}} 'int' prefix '⊖' U+2296
// DUMP-NEXT:      CXXMemberCallExpr {{.*}} 'int'
// DUMP-NEXT:        MemberExpr {{.*}} .operator⊖

//===----------------------------------------------------------------------===//
// 6. Spelling independence (U04/U11)
//===----------------------------------------------------------------------===//
//
// The same functions, spelled with glyphs under the default RUN lines and with
// universal-character-names under -DSPELL_UCN. Both compile, both evaluate to
// the same constants, and -- the point -- both *print* the same, because the
// node stores the code point and not the spelling. A printer that reproduced
// the spelling would be inventing an identity the language does not have.

constexpr int spelled_infix(int a, int b) { return a OP_SQ b OP_CT a; }
// CHECK-LABEL: constexpr int spelled_infix(int a, int b) {
// CHECK-NEXT:  return a ⊞ b ⊗ a;
static_assert(spelled_infix(1, 2) == 3 * 4 + 1);

constexpr int spelled_prefix(int a) { return OP_CM a OP_SQ a; }
// CHECK-LABEL: constexpr int spelled_prefix(int a) {
// CHECK-NEXT:  return ⊖a ⊞ a;
static_assert(spelled_prefix(2) == 2 * 7 + 2);

// The declaration side too: an operator-function-id spelled as a UCN prints as
// the glyph, because DeclarationName::print re-encodes the same code point.
constexpr int operator OP_SQ(double a, double b) { return 20; }
// CHECK-LABEL: constexpr int operator⊞(double a, double b) {
static_assert(operator OP_SQ(1.0, 2.0) == 20);
