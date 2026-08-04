// U14: the inheritance claim, evidenced.
//
// `x ⊞ y` *is* the call `operator⊞(x, y)` (U section 7 "Desugaring", D6), so
// everything the language gives calls comes along -- overload resolution,
// conversions, value categories, templates, two-phase lookup, SFINAE,
// constexpr, exceptions, evaluation order. This file is a sweep, not an
// implementation: nothing in Sema was written for any assertion below, and a
// failure here would have been a defect in U11/U12/U13/U16, not something to
// patch locally.
//
// What each sibling file already owns, and is deliberately not repeated here:
//   * SemaCXX/unicode-operator-call.cpp  -- the explicit call `operator⊞(x, y)`
//   * SemaCXX/unicode-operator-adl.cpp   -- candidate assembly, ADL, members
//   * Parser/unicode-operator-infix.cpp  -- infix parsing and grouping
//   * Parser/unicode-operator-prefix.cpp -- prefix parsing and grouping
//   * CodeGenCXX/unicode-operator-semantics.cpp -- the runtime half of items
//     5 and 6 (a throwing operator propagating; the operator form and the call
//     form emitting the same code)
//
// Item 9 ("the flag matrix") is the RUN lines: the feature is exercised with
// -funicode-operators under both constant evaluators, and with the flag off
// entirely.  The other half of item 9 -- that none of this changes when a
// second user-introduced infix feature sharing prec::UserInfix is also
// enabled (U7) -- has nothing in tree to compose with and is checked on the
// prototype branch that carries one.

// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fexperimental-new-constant-interpreter -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -DOFF -fsyntax-only -verify=off %s

#ifndef OFF

// Each overload returns a distinct type, so "which overload ran" is a
// compile-time assertion rather than two compilations both succeeding.
template <int N> struct Tag {
  static constexpr int value = N;
};

//===----------------------------------------------------------------------===//
// 1. Operands: class types, member and non-member forms, cv- and
//    ref-qualification, value categories, returned references as lvalues
//===----------------------------------------------------------------------===//

struct V {
  int v;
};

// Non-member overloads distinguished only by the operands' value category and
// constness: ordinary [over.ics.ref] ranking, reached through operator syntax.
constexpr Tag<1> operator⊞(const V &, const V &) { return {}; }
constexpr Tag<2> operator⊞(V &&, V &&) { return {}; }

constexpr V cv{1};
static_assert((cv ⊞ cv).value == 1);        // two lvalues
static_assert((V{1} ⊞ V{2}).value == 2);    // two rvalues
static_assert((cv ⊞ V{2}).value == 1);      // mixed: only the const& pair is viable

// Member ref-qualifiers select among member candidates, and for the *prefix*
// form the object argument is the only operand -- which makes the member
// prefix form the cheapest test of value-category propagation there is.
struct RQ {
  constexpr Tag<3> operator⊕(int) const & { return {}; }
  constexpr Tag<4> operator⊕(int) && { return {}; }
  constexpr Tag<5> operator⊖() const & { return {}; }
  constexpr Tag<6> operator⊖() && { return {}; }
};
constexpr RQ rq{};
static_assert((rq ⊕ 0).value == 3);
static_assert((RQ{} ⊕ 0).value == 4);
static_assert((⊖rq).value == 5);
static_assert((⊖RQ{}).value == 6);

// An rvalue-only member prefix operator rejects an lvalue operand, with the
// ordinary ref-qualifier diagnostic.
struct RvOnly {
  Tag<7> operator⊗() &&;
};
void rvalue_only(RvOnly r) {
  (void)(⊗r);
  // expected-error@-1 {{no matching function for call to 'operator⊗'}}
  // expected-note@-5 {{candidate function not viable: expects an rvalue for object argument}}
}

// The expression's own value category is the call's. A reference return is an
// lvalue and can be assigned through, in either fixity, member or not.
int global;
constexpr int &operator⊘(int, int);
static_assert(__is_same(decltype(1 ⊘ 2), int &));
static_assert(__is_same(decltype(1 ⊘ 2), decltype(operator⊘(1, 2))));

struct Cell {
  int v;
  constexpr int &operator⊙(int) { return v; }
  constexpr int &operator⊚() { return v; }
};
constexpr int assign_through() {
  Cell c{0};
  (c ⊙ 0) = 5;       // infix member, returned reference as an lvalue
  (⊚c) = (⊚c) + 2;   // prefix member, on both sides
  return c.v;
}
static_assert(assign_through() == 7);

// A prvalue result stays a prvalue, and an xvalue result stays an xvalue.
V operator⊛(V, V);
V &&operator⊜(V, V);
static_assert(__is_same(decltype(V{} ⊛ V{}), V));
static_assert(__is_same(decltype((V{} ⊜ V{})), V &&));

//===----------------------------------------------------------------------===//
// 2. Conversions on the arguments, ambiguity, explicit conversions
//===----------------------------------------------------------------------===//

// A converting constructor applies to both operands, as for any call.
struct FromInt {
  constexpr FromInt(int) {}
};
constexpr Tag<8> operator⊝(FromInt, FromInt) { return {}; }
static_assert((1 ⊝ 2).value == 8);

// A conversion function applies to the left operand of a non-member operator.
struct Conv {
  constexpr operator int() const { return 5; }
};
constexpr int operator⊟(int a, int b) { return 2 * a + b; }
static_assert((Conv{} ⊟ 1) == 11);

// An *explicit* converting constructor does not: copy-initialization of a
// parameter is not a direct-initialization, and the operator syntax gets the
// call's rule rather than a looser one.
struct Expl {
  explicit constexpr Expl(int) {}
};
constexpr Tag<9> operator⊠(Expl, Expl) { return {}; }
int explicit_ctor(int a) {
  return (a ⊠ a).value;
  // expected-error@-1 {{no matching function for call to 'operator⊠'}}
  // expected-note@-4 {{candidate function not viable: no known conversion from 'int' to 'Expl' for 1st argument}}
}
// Written explicitly, it works -- the operand, not the operator, was the
// problem.
static_assert((Expl{1} ⊠ Expl{2}).value == 9);

// An explicit conversion *function* likewise.
struct ExplConv {
  explicit constexpr operator int() const { return 5; }
};
int explicit_conv(ExplConv e) {
  return e ⊟ 1;
  // expected-error@-1 {{no matching function for call to 'operator⊟'}}
  // expected-note@-26 {{candidate function not viable: no known conversion from 'ExplConv' to 'int' for 1st argument}}
}
static_assert((static_cast<int>(ExplConv{}) ⊟ 1) == 11);

// Two candidates equally good: the ordinary [over.match.best] ambiguity, named
// for the operator.
constexpr Tag<10> operator⊡(long, long) { return {}; }
constexpr Tag<11> operator⊡(double, double) { return {}; }
int ambiguous(int a) {
  return (a ⊡ a).value;
  // expected-error@-1 {{call to 'operator⊡' is ambiguous}}
  // expected-note@-5 {{candidate function}}
  // expected-note@-5 {{candidate function}}
}
// And it is resolved the same way an ambiguous call is.
static_assert((1L ⊡ 2L).value == 10);

//===----------------------------------------------------------------------===//
// 3. constexpr and consteval
//===----------------------------------------------------------------------===//
//
// `static_assert(5 ⊞ 7 == 12)` -- the fundamental-only case that U2 exists to
// permit -- is asserted in SemaCXX/unicode-operator-adl.cpp section 1. What is
// added here is the rest of the constant-evaluation surface, and the fact that
// *both* of Clang's constant evaluators agree: two of the RUN lines above add
// -fexperimental-new-constant-interpreter, which routes through
// ByteCode/Compiler.cpp instead of ExprConstant.cpp.

// A constexpr member operator, both fixities.
struct CM {
  int v;
  constexpr int operator⊞(CM o) const { return v * 10 + o.v; }
  constexpr int operator⊖() const { return v + 100; }
};
static_assert((CM{1} ⊞ CM{2}) == 12);
static_assert((⊖CM{3}) == 103);

// A constant-evaluated ADL case: the candidate is found in the operand's
// namespace and the whole thing folds.
namespace CE {
struct C {};
constexpr Tag<12> operator⊢(C, C) { return {}; }
constexpr Tag<13> operator⊣(C) { return {}; }
} // namespace CE
constexpr auto ce_infix = CE::C{} ⊢ CE::C{};
constexpr auto ce_prefix = ⊣CE::C{};
static_assert(ce_infix.value == 12);
static_assert(ce_prefix.value == 13);

// consteval: an immediate invocation through operator syntax.
consteval int operator⊤(int a, int b) { return a * 100 + b; }
consteval int operator⊥(int a) { return a * 1000; }
static_assert((3 ⊤ 4) == 304);
static_assert((⊥5) == 5000);

// And the ordinary immediate-invocation diagnostic when the operands are not
// constants -- not a new diagnostic, the call's.
int consteval_misuse(int a) {
  return a ⊤ 1;
  // expected-error@-1 {{call to consteval function 'operator⊤' is not a constant expression}}
  // expected-note@-2 {{function parameter 'a' with unknown value cannot be used in a constant expression}}
  // expected-note@-4 {{declared here}}
}

// The operator is usable in every constant-expression context a call is.
struct DMI {
  int v = 1 ⊟ 2;
};
static_assert(DMI{}.v == 4);
int arr[1 ⊟ 2];
static_assert(sizeof(arr) / sizeof(int) == 4);
enum E { e = 1 ⊟ 2 };
static_assert(e == 4);
static_assert(Tag<1 ⊟ 2>::value == 4);

// The operator-function-id names the overload set wherever an unqualified-id
// does, so its address can be taken and called -- the U section 7.1 point that
// the "section" spelling already exists.
constexpr int (*pinfix)(int, int) = &operator⊟;
static_assert(pinfix(1, 2) == 4);

//===----------------------------------------------------------------------===//
// 4. Templates: dependent operands, two-phase lookup, SFINAE, concepts
//===----------------------------------------------------------------------===//
//
// This is the one item where the design was measurably wrong and then fixed.
// Before U16 a use of the operator syntax was an ordinary CallExpr, so
// TreeTransform rebuilt it at instantiation through ActOnCallExpr --
// [over.match.call], not [over.match.oper] -- and *member* candidates were
// lost, because member candidates are a property of the operator syntax while
// ADL is a property of the call. U16's UserOperatorExpr records the syntax and
// TransformUserOperatorExpr re-runs CreateOverloadedUserOp. The regression
// tests for that specific defect live in unicode-operator-adl.cpp section 5;
// what follows is the surrounding template surface.

// (a) A class template with a member user operator, and a partial
//     specialization of it: each instantiation gets its own member candidate.
template <class T> struct Box {
  T v;
  constexpr Tag<14> operator⊞(Box) const { return {}; }
  constexpr Tag<15> operator⊖() const { return {}; }
};
template <class T> struct Box<T *> {
  constexpr Tag<16> operator⊞(Box) const { return {}; }
};
static_assert((Box<int>{1} ⊞ Box<int>{2}).value == 14);
static_assert((⊖Box<int>{1}).value == 15);
static_assert((Box<int *>{} ⊞ Box<int *>{}).value == 16);

// (b) A member operator found through a DEPENDENT BASE. Nothing tested this
//     for either fixity before U14. The name is not looked up in the dependent
//     base at definition time; the operand `*this` is dependent, so the whole
//     use is dependent and the member candidate is assembled at instantiation.
template <class T> struct OpBase {
  constexpr Tag<17> operator⊕(int) const { return {}; }
  constexpr Tag<18> operator⊗() const { return {}; }
};
template <class T> struct Derived : OpBase<T> {
  constexpr auto infix(int x) const { return *this ⊕ x; }
  constexpr auto prefix() const { return ⊗*this; }
};
static_assert(Derived<int>{}.infix(0).value == 17);
static_assert(Derived<int>{}.prefix().value == 18);

// (c) A member operator template, deduced through the operator syntax.
struct MT {
  template <class T> constexpr Tag<19> operator⊘(T) const { return {}; }
};
static_assert((MT{} ⊘ 0).value == 19);

// (d) Two-phase lookup: an operator declared *after* the template is still
//     found for a dependent operand, by ADL from the instantiation context.
namespace Late {
struct L {};
} // namespace Late
template <class T> constexpr auto late(T a) { return a ⊙ a; }
template <class T> constexpr auto late_prefix(T a) { return ⊚a; }
namespace Late {
constexpr Tag<20> operator⊙(L, L) { return {}; }
constexpr Tag<21> operator⊚(L) { return {}; }
} // namespace Late
static_assert(late(Late::L{}).value == 20);
static_assert(late_prefix(Late::L{}).value == 21);

// (e) The other half of two-phase lookup, and the property that only exists
//     because TransformUserOperatorExpr always rebuilds: a NON-dependent use
//     inside a template is bound at definition time and must NOT be
//     re-resolved against whatever is visible at instantiation. The rebuild
//     re-runs overload resolution with the already-chosen function and ADL
//     off, so a better candidate declared later cannot steal the call.
struct A {};
struct B {
  constexpr operator A() const { return {}; }
};
constexpr Tag<22> operator⊛(A, A) { return {}; }
constexpr Tag<23> operator⊜(A) { return {}; }
template <class T> constexpr auto nondep(B b) { return b ⊛ b; }
template <class T> constexpr auto nondep_prefix(B b) { return ⊜b; }
constexpr Tag<24> operator⊛(B, B) { return {}; }  // an exact match, declared later
constexpr Tag<25> operator⊜(B) { return {}; }
static_assert(nondep<int>(B{}).value == 22);          // definition context wins
static_assert(nondep_prefix<int>(B{}).value == 23);
static_assert((B{} ⊛ B{}).value == 24);               // here, the later one wins
static_assert((⊜B{}).value == 25);

// (f) SFINAE: an unusable operator removes the candidate rather than erroring.
//     (The infix case is in unicode-operator-adl.cpp section 5; this is the
//     prefix twin, plus SFINAE on the operator's *noexcept*.)
struct Q {};
constexpr Tag<26> operator⊝(Q) { return {}; }
template <class T> constexpr bool prefixable(int, decltype(⊝T{}) * = nullptr) {
  return true;
}
template <class T> constexpr bool prefixable(...) { return false; }
static_assert(prefixable<Q>(0));
static_assert(!prefixable<int>(0));

// (g) Concepts, both fixities, and a requires-clause using one.
template <class T> concept Combinable = requires(T a, T b) { a ⊛ b; };
template <class T> concept Negatable = requires(T a) { ⊝a; };
static_assert(Combinable<A>);
static_assert(!Combinable<Q>);
static_assert(Negatable<Q>);
static_assert(!Negatable<A>);

template <class T>
  requires Negatable<T>
constexpr auto constrained(T a) {
  return ⊝a;
}
static_assert(constrained(Q{}).value == 26);

// A compound requirement on a user operator, including its noexcept-ness --
// the requires-expression machinery needs nothing new to inspect the operator
// syntax, because there is nothing new to inspect.
int operator⊞(Q, Q) noexcept;
double operator⊞(A, A);
template <class T> concept NothrowCombinable = requires(T a, T b) {
  { a ⊞ b } noexcept;
};
static_assert(NothrowCombinable<Q>);
static_assert(!NothrowCombinable<A>);

template <class T, class U> concept SameAs = __is_same(T, U);
template <class T> concept YieldsInt = requires(T a, T b) {
  { a ⊞ b } -> SameAs<int>;
};
static_assert(YieldsInt<Q>);
static_assert(!YieldsInt<A>);

//===----------------------------------------------------------------------===//
// 5. Exception specifications
//===----------------------------------------------------------------------===//
//
// `noexcept(a ⊞ b)` is the callee's specification because Sema::canThrow
// forwards through the node to the call it wraps. The runtime half -- a
// throwing operator actually propagating, and a noexcept one not needing a
// landing pad -- is CodeGenCXX/unicode-operator-semantics.cpp.

int operator⊣(int, int) noexcept;
double operator⊣(double, double);
int operator⊢(int) noexcept;
double operator⊢(double);

static_assert(noexcept(1 ⊣ 2));
static_assert(!noexcept(1.0 ⊣ 2.0));
static_assert(noexcept(⊢1));
static_assert(!noexcept(⊢1.0));

// The operand of noexcept is unevaluated, so a declared-but-undefined operator
// is fine, exactly as for a call.
static_assert(noexcept(1 ⊣ 2) == noexcept(operator⊣(1, 2)));

// A computed exception specification, and one forwarded through a template.
template <class T> T operator⊲(T a, T b) noexcept(sizeof(T) == sizeof(int));
static_assert(noexcept(1 ⊲ 2));
static_assert(!noexcept(1.0 ⊲ 2.0));

template <class T>
constexpr bool fwd(T a, T b) noexcept(noexcept(a ⊣ b)) {
  return true;
}
static_assert(noexcept(fwd(1, 2)));
static_assert(!noexcept(fwd(1.0, 2.0)));

// A member operator's specification, both fixities.
struct NX {
  int operator⊥(int) const noexcept;
  int operator⊨() const noexcept;
  double operator⊥(double) const;
};
static_assert(noexcept(NX{} ⊥ 1));
static_assert(!noexcept(NX{} ⊥ 1.0));
static_assert(noexcept(⊨NX{}));

//===----------------------------------------------------------------------===//
// 6. Evaluation order (D15 / section 17.2)
//===----------------------------------------------------------------------===//
//
// The operator adds NO evaluation-order rule of its own; it inherits
// [expr.call]. This section asserts exactly what that gives and nothing more.
// Read the three claims separately, because they are not the same claim:
//
//   (a) NON-MEMBER form: the two operands are the two arguments of a call, so
//       they are INDETERMINATELY SEQUENCED -- neither interleaves with the
//       other, and which goes first is UNSPECIFIED. Left-to-right is NOT
//       guaranteed and is not asserted here.
//   (b) MEMBER form: the left operand is the object expression, which is part
//       of the postfix-expression, and [expr.call]p8 sequences the
//       postfix-expression before every argument. So for a member operator the
//       left operand IS sequenced before the right. This is a guarantee, and
//       it is the same guarantee `x.operator⊞(y)` gives.
//   (c) Consequently, whether `x ⊞ y` sequences x before y depends on which
//       overload is selected. That falls straight out of "it is just the
//       call", but it differs from every existing operator: [over.match.oper]p2
//       makes an OVERLOADED built-in-spelled operator use the built-in's
//       sequencing regardless of member-ness, and a user operator has no
//       built-in to borrow sequencing from. Recorded as DEV-U16.
//
// The other half of [expr.call]p8 -- that since C++17 the callee is sequenced
// before both operands -- has nothing to say here: a Unicode operator's callee
// is a name, not an expression, so it can have no side effects of its own to
// order against the operands.

// Each of these writes to `i` twice, so an interleaving would be observable as
// a digit pattern that no permitted order produces.
constexpr int bump(int &i, int d) {
  i = i * 10 + d;
  i = i * 10 + d;
  return d;
}

constexpr int nonmember_op(int, int) { return 0; }
constexpr int operator⊩(int, int) { return 0; }

constexpr int order_op() {
  int i = 0;
  (void)(bump(i, 1) ⊩ bump(i, 2));
  return i;
}
constexpr int order_call() {
  int i = 0;
  (void)(nonmember_op(bump(i, 1), bump(i, 2)));
  return i;
}

// Indeterminately sequenced: 1122 or 2211, never 1212 or 2121. Both orders are
// conforming; this assertion is the whole guarantee and deliberately not more.
static_assert(order_op() == 1122 || order_op() == 2211);
// And whichever one this implementation picks, it picks it for the operator
// syntax and the call spelling alike. That is the inheritance claim stated as
// an equation rather than as two orders that happen to match.
static_assert(order_op() == order_call());

// The member form's guarantee, which IS an order.
struct Obj {
  int v;
  constexpr int operator⊪(int) const { return 0; }
};
constexpr Obj make(int &i, int d) {
  bump(i, d);
  return Obj{d};
}
constexpr int order_member() {
  int i = 0;
  (void)(make(i, 1) ⊪ bump(i, 2));
  return i;
}
constexpr int order_member_call() {
  int i = 0;
  (void)(make(i, 1).operator⊪(bump(i, 2)));
  return i;
}
static_assert(order_member() == 1122);  // guaranteed by [expr.call]p8
static_assert(order_member() == order_member_call());

// The same three claims, observed a second way: -Wunsequenced. This is the
// sharpest available evidence that the operator got the CALL's sequencing and
// not a built-in operator's, because the three behaviours are distinguishable.
int side_effects(int i) {
  // A built-in operator with its own sequencing rule: `<<` sequences its left
  // operand before its right, so this is well-defined and silent.
  (void)(i++ << i++);

  // A plain call: the arguments are indeterminately sequenced, and Clang warns
  // about the two modifications.
  (void)nonmember_op(i++, i++);
  // expected-warning@-1 {{multiple unsequenced modifications to 'i'}}

  // The non-member operator syntax: the SAME warning, at the same place, for
  // the same reason. Not `<<`'s silence.
  (void)(i++ ⊩ i++);
  // expected-warning@-1 {{multiple unsequenced modifications to 'i'}}

  return i;
}

Obj objs[8];
int side_effects_member(int i) {
  // The member form: silent, because the object expression is sequenced before
  // the argument -- and silent for the explicit desugaring too.
  (void)(objs[i++] ⊪ i++);
  (void)(objs[i++].operator⊪(i++));
  return i;
}

// A default argument is evaluated as the call's default argument is (DEV-U15:
// a two-parameter operator with a default argument is one-argument-viable, so
// it can be used in prefix position; the default argument is what fills the
// second parameter).
constexpr int operator⊬(int a, int b = 7) { return a * 100 + b; }
static_assert((⊬5) == 507);
static_assert((3 ⊬ 4) == 304);
static_assert((⊬5) == operator⊬(5));

//===----------------------------------------------------------------------===//
// 7. Deleted operators
//===----------------------------------------------------------------------===//
//
// The member form is covered in unicode-operator-adl.cpp section 4b; these are
// the non-member forms, which go through BuildCallExpr and therefore produce
// the ordinary use-of-deleted diagnostic naming the operator.

int operator⊭(int, int) = delete;
int deleted_infix(int a) {
  return a ⊭ a;
  // expected-error@-1 {{call to deleted function 'operator⊭'}}
  // expected-note@-4 {{candidate function has been explicitly deleted}}
}

int operator⋀(int) = delete;
int deleted_prefix(int a) {
  return ⋀a;
  // expected-error@-1 {{call to deleted function 'operator⋀'}}
  // expected-note@-4 {{candidate function has been explicitly deleted}}
}

// A deleted overload participates in overload resolution and can be *not*
// selected, exactly as for a call.
struct D1 {};
struct D2 {};
constexpr Tag<27> operator⋁(D1, D1) { return {}; }
Tag<28> operator⋁(D2, D2) = delete;
static_assert((D1{} ⋁ D1{}).value == 27);

// Deleted, and unsatisfying a concept rather than hard-erroring.
template <class T> concept Orable = requires(T a, T b) { a ⋁ b; };
static_assert(Orable<D1>);
static_assert(!Orable<D2>);

//===----------------------------------------------------------------------===//
// 8. Prefix-form equivalents
//===----------------------------------------------------------------------===//
//
// Every section above carries its prefix twin inline rather than in a separate
// block, because the point is that the two fixities are the same feature: a
// one-argument call and a two-argument call. Collected here are the prefix
// cases that have no infix analogue at all.

// A member prefix operator has NO parameters, so its only operand is the
// implicit object argument -- section 1 uses that for ref-qualification. Its
// consequence for overload resolution is that a prefix member cannot be
// overloaded on its operand at all; only on cv- and ref-qualification.
struct OnlyObject {
  constexpr Tag<29> operator⊮() const { return {}; }
  constexpr Tag<30> operator⊮() { return {}; }
};
constexpr OnlyObject oo_const{};
static_assert((⊮oo_const).value == 29);
constexpr int oo_nonconst() {
  OnlyObject oo{};
  return (⊮oo).value;
}
static_assert(oo_nonconst() == 30);

// Stacking is call nesting: `⊖⊖a` is `operator⊖(operator⊖(a))`, and every
// property above composes because composition is what calls do.
constexpr int operator⊯(int a) { return 3 * a + 1; }
static_assert((⊯⊯1) == 13);
static_assert((⊯⊯1) == operator⊯(operator⊯(1)));
static_assert(noexcept(⊯1) == noexcept(operator⊯(1)));

#else
//===----------------------------------------------------------------------===//
// 9. Flag off
//===----------------------------------------------------------------------===//
//
// Without -funicode-operators the code points are not operator tokens and not
// identifier characters either. One representative declaration and one
// representative use, infix and prefix.

int operator⊟(int, int);
// off-error@-1 {{character '⊟' U+229F not allowed in an identifier}}

int use_infix(int a, int b) { return a ⊟ b; }
// off-error@-1 {{unexpected character '⊟' U+229F}}
// off-error@-2 {{expected ';' after return statement}}

int use_prefix(int a) { return ⊟a; }
// off-error@-1 {{unexpected character '⊟' U+229F}}

#endif
