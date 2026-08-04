// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -fsyntax-only -verify %s
// RUN: not %clang_cc1 -std=c++23 -funicode-operators -ast-dump %s | FileCheck %s

// U13: candidate assembly for the operator syntax -- member candidates,
// non-member candidates from unqualified lookup *and ADL*, and no built-in
// candidates at all (U6).
//
// This file is the infix twin of SemaCXX/unicode-operator-call.cpp, which U10
// wrote for the explicit call `operator⊞(x, y)` before any expression syntax
// existed. Its section 4 was laid out to be mirrored line for line, and this
// file mirrors it. The two claims being proved:
//
//   * The section 17.4 equivalence, which is normative: for every case,
//     `x ⊞ y` selects the SAME overload as `operator⊞(x, y)`. Asserted here as
//     type identity over a tag-returning overload set -- each candidate returns
//     a distinct type, so "same overload" is checkable by __is_same and not
//     merely by two compilations both succeeding. Value equality is asserted
//     too, but the type identity is the load-bearing half.
//   * No built-in candidates (U2/U6). AddBuiltinOperatorCandidates is never
//     called for a user operator, so a fundamental-type pair finds the user's
//     operator or nothing -- never an arithmetic or pointer meaning.
//
// The member half is what U13 actually implemented; the non-member half was
// already working through the U11 stub, and the cases below that reproduce
// U10's section 4 are regression evidence that adding member candidates did not
// cost ADL. Losing ADL to an eagerly resolved callee is GCC's DEV-G05 and is
// the specific failure this file exists to detect.

// Each overload returns a distinct type, so overload identity is observable.
template <int N> struct Tag {
  static constexpr int value = N;
};

//===--------------------------------------------------------------------===//
// 1. The motivating line (U section 1), and the equivalence in miniature
//===--------------------------------------------------------------------===//

constexpr int operator⊞(int a, int b) { return a + b; }

static_assert(5 ⊞ 7 == 12);
static_assert(operator⊞(5, 7) == 12);
static_assert(__is_same(decltype(5 ⊞ 7), decltype(operator⊞(5, 7))));

// No class or enum operand anywhere, and the result is a constant expression.
// U2 removes the [over.oper]p7 restriction precisely so this line can exist.
static_assert(__is_same(decltype(5 ⊞ 7), int));

//===--------------------------------------------------------------------===//
// 2. No built-in candidates (U2/U6)
//===--------------------------------------------------------------------===//

// Nothing named operator⊠ is declared anywhere in this translation unit. The
// error is a lookup error that names the operator -- never a lexing or parsing
// error (U3), and never a silent fallback to `a + b`.
int no_operator_at_all(int a, int b) {
  return a ⊠ b;
  // expected-error@-1 {{use of undeclared 'operator⊠'}}
}

// An operator⊟ exists but does not accept ints. If built-in candidates were in
// the set, `a ⊟ b` would quietly become integer arithmetic. It does not: the
// only candidate is the user's, and it is not viable.
struct Base {};
int operator⊟(Base, Base);
int no_arithmetic_fallback(int a, int b) {
  return a ⊟ b;
  // expected-error@-1 {{no matching function for call to 'operator⊟'}}
  // expected-note@-4 {{candidate function not viable: no known conversion from 'int' to 'Base' for 1st argument}}
}

// The same for pointers: no built-in pointer arithmetic is synthesized for a
// user operator, even though operator⊞(int, int) is visible.
int *no_pointer_arithmetic(int *p, long n) {
  return p ⊞ n;
  // expected-error@-1 {{no matching function for call to 'operator⊞'}}
  // expected-note@-38 {{candidate function not viable: no known conversion from 'int *' to 'int' for 1st argument; dereference the argument with *}}
}

//===--------------------------------------------------------------------===//
// 3. ADL -- the normative case (U6, section 17.4)
//===--------------------------------------------------------------------===//

namespace Adl {
struct A {};
enum E { e1 };
struct Ptr {};

constexpr Tag<41> operator⊘(A, A) { return {}; }
constexpr Tag<43> operator⊘(E, E) { return {}; }
constexpr Tag<44> operator⊚(Ptr *, Ptr *) { return {}; }

// A hidden friend: reachable by ADL and by nothing else at all.
struct Hidden {
  friend constexpr Tag<45> operator⊛(Hidden, Hidden) { return {}; }
};
} // namespace Adl

// Nothing named operator⊘ / operator⊚ / operator⊛ is visible here by ordinary
// unqualified lookup. Every one of these is found purely by ADL, and each
// assertion pairs the value with the same-overload check.
static_assert((Adl::A{} ⊘ Adl::A{}).value == 41);
static_assert(__is_same(decltype(Adl::A{} ⊘ Adl::A{}),
                        decltype(operator⊘(Adl::A{}, Adl::A{}))));

static_assert((Adl::e1 ⊘ Adl::e1).value == 43);  // enumeration's namespace
static_assert(__is_same(decltype(Adl::e1 ⊘ Adl::e1),
                        decltype(operator⊘(Adl::e1, Adl::e1))));

static_assert((Adl::Hidden{} ⊛ Adl::Hidden{}).value == 45);  // hidden friend
static_assert(__is_same(decltype(Adl::Hidden{} ⊛ Adl::Hidden{}),
                        decltype(operator⊛(Adl::Hidden{}, Adl::Hidden{}))));

constexpr auto adl_pointer() {
  Adl::Ptr p;
  return &p ⊚ &p;  // pointee's namespace is associated
}
static_assert(adl_pointer().value == 44);

// A class template's argument contributes its namespace, as for any call.
template <class T> struct Wrap {};
namespace Adl2 {
struct B {};
constexpr Tag<46> operator⊜(Wrap<B>, Wrap<B>) { return {}; }
} // namespace Adl2
static_assert((Wrap<Adl2::B>{} ⊜ Wrap<Adl2::B>{}).value == 46);
static_assert(__is_same(decltype(Wrap<Adl2::B>{} ⊜ Wrap<Adl2::B>{}),
                        decltype(operator⊜(Wrap<Adl2::B>{}, Wrap<Adl2::B>{}))));

// ADL *augments* the ordinary-lookup set rather than being masked by it: a
// visible, non-viable ordinary-lookup candidate does not stop the ADL one from
// being found. This is the shape GCC's DEV-G05 got wrong.
struct Local {};
constexpr Tag<47> operator⊝(Local, Local) { return {}; }
namespace Adl3 {
struct C {};
constexpr Tag<48> operator⊝(C, C) { return {}; }
} // namespace Adl3
static_assert((Local{} ⊝ Local{}).value == 47);
static_assert((Adl3::C{} ⊝ Adl3::C{}).value == 48);
static_assert(__is_same(decltype(Adl3::C{} ⊝ Adl3::C{}),
                        decltype(operator⊝(Adl3::C{}, Adl3::C{}))));

// A using-declaration brings the name in by ordinary lookup.
namespace Adl4 {
constexpr Tag<49> operator⊨(int, int) { return {}; }
} // namespace Adl4
using Adl4::operator⊨;
static_assert((1 ⊨ 2).value == 49);

// ADL must *not* fire when nothing associates the namespace: fundamental
// operands only, operator in an unrelated namespace.
namespace Unrelated {
constexpr Tag<50> operator⊧(int, int) { return {}; }
} // namespace Unrelated
int adl_does_not_reach_here(int a, int b) {
  return (a ⊧ b).value;
  // expected-error@-1 {{use of undeclared 'operator⊧'}}
}

// ADL does not find *member* operators -- the same rule as for operator+, and
// the same result U10 measured for the explicit call.
namespace Adl5 {
struct D {
  constexpr int operator⊡(D) const { return 51; }
};
} // namespace Adl5
// The member IS found here, because this is the operator syntax and D is the
// left operand's class. The point of the pair below is that the explicit call
// on the next line is *not* equivalent, and is not supposed to be.
static_assert((Adl5::D{} ⊡ Adl5::D{}) == 51);
int adl_finds_no_members(Adl5::D d) {
  return operator⊡(d, d);
  // expected-error@-1 {{use of undeclared 'operator⊡'}}
}

//===--------------------------------------------------------------------===//
// 4. Member candidates -- the half U13 added
//===--------------------------------------------------------------------===//

struct Mem {
  int v;
  // Non-commutative on purpose: a wrong operand order is a wrong value.
  constexpr int operator⊕(Mem o) const { return v * 10 + o.v; }
};
static_assert((Mem{1} ⊕ Mem{2}) == 12);

// The equivalence for the member form: `x ⊕ y` is `x.operator⊕(y)` (U section 7
// "Desugaring"), same overload and same value.
static_assert(__is_same(decltype(Mem{1} ⊕ Mem{2}),
                        decltype(Mem{1}.operator⊕(Mem{2}))));
static_assert((Mem{1} ⊕ Mem{2}) == Mem{1}.operator⊕(Mem{2}));

// Inherited members are found: the qualified lookup is into the left operand's
// class and its bases, exactly as [over.match.oper]p3 says.
struct MemBase {
  constexpr Tag<52> operator⊗(int) const { return {}; }
};
struct MemDerived : MemBase {};
static_assert((MemDerived{} ⊗ 0).value == 52);

// Member and non-member candidates are ranked in ONE set, not in two passes:
// each of these picks the other kind of candidate.
struct MN {
  constexpr Tag<53> operator⊘(long) const { return {}; }
};
constexpr Tag<54> operator⊘(MN, int) { return {}; }
static_assert((MN{} ⊘ 0).value == 54);   // non-member exact beats member conversion
static_assert((MN{} ⊘ 0L).value == 53);  // member exact beats non-member conversion

// A member candidate does not suppress ADL either: the member set is non-empty
// here *and* the winning candidate for the second line comes from ADL.
namespace Adl6 {
struct F {
  constexpr Tag<55> operator⊜(F) const { return {}; }
};
constexpr Tag<56> operator⊜(F, int) { return {}; }
} // namespace Adl6
static_assert((Adl6::F{} ⊜ Adl6::F{}).value == 55);  // member
static_assert((Adl6::F{} ⊜ 1).value == 56);          // ADL, with members present

// Object cv- and ref-qualification select among member candidates as usual.
struct RQ {
  constexpr Tag<57> operator⊞(int) const & { return {}; }
  constexpr Tag<58> operator⊞(int) && { return {}; }
};
constexpr RQ rq{};
static_assert((rq ⊞ 0).value == 57);
static_assert((RQ{} ⊞ 0).value == 58);

// An explicit object parameter works, and so does a member template.
struct EO {
  constexpr Tag<59> operator⊗(this EO, int) { return {}; }
};
static_assert((EO{} ⊗ 0).value == 59);

struct TM {
  template <class T> constexpr Tag<60> operator⊤(T) const { return {}; }
};
static_assert((TM{} ⊤ 1).value == 60);

// Member candidate assembly reaches a member of a class template, and a member
// operator on a non-dependent object inside a template body.
template <class T> struct Boxed {
  T v;
  constexpr T operator⊥(Boxed b) const { return v * 10 + b.v; }
};
static_assert((Boxed<int>{3} ⊥ Boxed<int>{4}) == 34);

//===--------------------------------------------------------------------===//
// 4b. Member diagnostics -- ordinary ones, naming the operator
//===--------------------------------------------------------------------===//

struct MemBad {
  int operator⊣(MemBad) const;
};
int member_not_viable(MemBad m, Base b) {
  return m ⊣ b;
  // expected-error@-1 {{no matching function for call to 'operator⊣'}}
  // expected-note@-5 {{candidate function not viable: no known conversion from 'Base' to 'MemBad' for 1st argument}}
}

struct AmbMN {
  int operator⊢(int) const;
};
int operator⊢(AmbMN, int);
int member_nonmember_ambiguous(AmbMN a) {
  return a ⊢ 1;
  // expected-error@-1 {{call to 'operator⊢' is ambiguous}}
  // expected-note@-6 {{candidate function}}
  // expected-note@-5 {{candidate function}}
}

class PrivateOp {
  int operator⊪(PrivateOp) const;
};
int member_access(PrivateOp p) {
  return p ⊪ p;
  // expected-error@-1 {{'operator⊪' is a private member of 'PrivateOp'}}
  // expected-note@-5 {{implicitly declared private here}}
}

struct DeletedOp {
  int operator⊩(DeletedOp) const = delete;
};
int member_deleted(DeletedOp d) {
  return d ⊩ d;
  // expected-error@-1 {{call to deleted function 'operator⊩'}}
  // expected-note@-5 {{candidate function has been explicitly deleted}}
}

//===--------------------------------------------------------------------===//
// 5. Dependent operands -- two-phase behavior, inherited
//===--------------------------------------------------------------------===//

namespace Dep {
struct E2 {};
constexpr Tag<61> operator⋀(E2, E2) { return {}; }
} // namespace Dep

// Nothing named operator⋀ is visible where the template is defined; the
// candidate is found by ADL performed at instantiation, from the instantiation
// context. This is not reimplemented -- it is the ordinary dependent-call path.
template <class T> constexpr auto dependent_infix(T a, T b) { return a ⋀ b; }
static_assert(dependent_infix(Dep::E2{}, Dep::E2{}).value == 61);
static_assert(__is_same(decltype(dependent_infix(Dep::E2{}, Dep::E2{})),
                        decltype(operator⋀(Dep::E2{}, Dep::E2{}))));

// The same expression in a requires-expression: well-formed for E2, ill-formed
// for int, and neither is a hard error.
template <class T> concept Combinable = requires(T a, T b) { a ⋀ b; };
static_assert(Combinable<Dep::E2>);
static_assert(!Combinable<int>);

template <class T>
  requires Combinable<T>
constexpr auto constrained(T a, T b) {
  return a ⋀ b;
}
static_assert(constrained(Dep::E2{}, Dep::E2{}).value == 61);

// SFINAE: an unusable operator removes the candidate rather than erroring.
template <class T>
constexpr bool has_op(int, decltype(T{} ⋀ T{}) * = nullptr) {
  return true;
}
template <class T> constexpr bool has_op(...) { return false; }
static_assert(has_op<Dep::E2>(0));
static_assert(!has_op<Base>(0));

// A dependent operand with a *member* operator was the one shape that did not
// work when U13 landed, and the reason was structural: the dependent
// expression was an ordinary CallExpr whose callee carried the operator's name
// but not the fact that operator syntax was used, so TreeTransform rebuilt it
// as a plain call at instantiation -- [over.match.call], not
// [over.match.oper] -- and the member candidates were never assembled. ADL
// survived, because ADL is a property of the call; member candidates did not,
// because they are a property of the operator syntax.
//
// U16 closed it by recording the operator syntax in the AST (UserOperatorExpr,
// the analogue of what CXXOperatorCallExpr does for the existing operators);
// TransformUserOperatorExpr recovers the operands and re-runs
// CreateOverloadedUserOp on them. These two assertions were the pinned
// failures, and they are the regression test for that.
template <class T> constexpr auto dependent_member(T a, T b) { return a ⊕ b; }
static_assert(dependent_member(Mem{1}, Mem{2}) == 12);

template <class T> concept MemberCombinable = requires(T a, T b) { a ⊕ b; };
static_assert(MemberCombinable<Mem>);
static_assert(!MemberCombinable<int>);

// Member and non-member candidates are still ranked in *one* set when the
// operands are dependent, both ways round -- the observable that distinguishes
// this from a member-first fallback, now asserted at instantiation as well as
// at parse time.
template <class T, class U> constexpr auto dependent_ranked(T a, U b) {
  return a ⊘ b;
}
static_assert(dependent_ranked(MN{}, 0).value == 54);   // non-member
static_assert(dependent_ranked(MN{}, 0L).value == 53);  // member

//===--------------------------------------------------------------------===//
// 6. Composability with -fbacktick (U7)
//===--------------------------------------------------------------------===//

// Both -verify RUN lines above compile this whole file; the second adds
// -fbacktick. Every assertion here must hold identically under either, which is
// the U7 ground rule that the two flags are independent.

//===--------------------------------------------------------------------===//
// 7. The desugaring, in the AST
//===--------------------------------------------------------------------===//

int ast_nonmember(int a, int b) { return a ⊞ b; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_nonmember
// CHECK:         CallExpr {{.*}} 'int'
// CHECK-NEXT:      ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
// CHECK-NEXT:        DeclRefExpr {{.*}} Function {{.*}} 'operator⊞' 'int (int, int)'

int ast_member(Mem a, Mem b) { return a ⊕ b; }
// CHECK-LABEL: FunctionDecl {{.*}} ast_member
// CHECK:         CXXMemberCallExpr {{.*}} 'int'
// CHECK-NEXT:      MemberExpr {{.*}} '<bound member function type>' .operator⊕
