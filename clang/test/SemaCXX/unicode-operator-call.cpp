// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -fsyntax-only -verify %s

// U10: the *name* `operator⊞` behaves as an ordinary function name, in every
// position an unqualified-id can appear, before any expression syntax for the
// operator exists (U11/U12 are unwritten at the time this file was authored).
//
// This is a pure gate: nothing in the compiler was changed to make it pass. Its
// job is to prove that U06-U09 handed the rest of the front end a name it
// already knows how to handle -- lookup, overload sets, ADL, templates, SFINAE,
// constraints, address-taking -- so that U13 has an explicit statement of the
// behavior it must reproduce for the infix form.
//
// The two claims that are normative rather than incidental:
//
//   * ADL (U6, and section 17.4 of the backtick design doc, which makes it
//     normative). An operator use must find every overload the plain call
//     would; GCC's DEV-G05 is the recorded example of an implementation
//     resolving too early and losing exactly this. Everything under "ADL"
//     below is the behavior U13 has to match through the infix path.
//   * A use is never a *lexing* or *parsing* error (U3). When nothing is
//     viable, the diagnostic is an ordinary lookup/overload diagnostic that
//     names `operator⊞` -- see "Negative cases".
//
// NOT this file: mangling and symbol identity, which
// CodeGenCXX/unicode-operator-mangle.cpp owns and this file deliberately does
// not duplicate; the *call*-side symbol evidence lives in
// CodeGenCXX/unicode-operator-call.cpp.
//
// NOT this file, because it cannot be: UCN and \N{...} spellings of the
// operator (step file item 8). U04 is unchecked, so `operator⊞` still
// lexes as an identifier and errors. See the U10 handoff for the exact two
// cases to add here once U04 lands.

//===--------------------------------------------------------------------===//
// 1. The fundamental-only call, end to end
//===--------------------------------------------------------------------===//

// U2's motivating case: no class or enum operand anywhere. The call is an
// ordinary call and is a constant expression.
constexpr int operator⊞(int a, int b) { return a + b; }
constexpr int operator⊖(int a) { return -a; }

static_assert(operator⊞(5, 7) == 12);
static_assert(operator⊖(5) == -5);
static_assert(operator⊞(operator⊞(1, 2), operator⊖(3)) == 0);

int runtime(int a, int b) { return operator⊞(a, b); }

// The call expression has the declared return type, and is an ordinary
// prvalue.
static_assert(__is_same(decltype(operator⊞(1, 2)), int));

//===--------------------------------------------------------------------===//
// 2. Overload sets and ordinary ranking
//===--------------------------------------------------------------------===//

struct Base {};
struct Derived : Base {};

constexpr int operator⊠(int, int) { return 1; }
constexpr int operator⊠(double, double) { return 2; }
constexpr int operator⊠(Base, Base) { return 3; }
constexpr int operator⊠(Derived, Derived) { return 4; }

static_assert(operator⊠(1, 2) == 1);            // exact match
static_assert(operator⊠(1.0, 2.0) == 2);        // exact match
static_assert(operator⊠(1.0f, 2.0f) == 2);      // float -> double promotion
static_assert(operator⊠(Base{}, Base{}) == 3);
static_assert(operator⊠(Derived{}, Derived{}) == 4);  // beats the Base conversion

// Ordinary ambiguity, and the diagnostic names the operator readably.
int ambiguous() {
  return operator⊠(1, 2.0);
  // expected-error@-1 {{call to 'operator⊠' is ambiguous}}
  // expected-note@-15 {{candidate function}}
  // expected-note@-15 {{candidate function}}
}

//===--------------------------------------------------------------------===//
// 3. Qualified and member calls
//===--------------------------------------------------------------------===//

namespace NS {
struct Q {};
constexpr int operator⊞(Q, Q) { return 21; }
constexpr int operator⊖(Q) { return 22; }
} // namespace NS

constexpr int qualified() {
  NS::Q q;
  return NS::operator⊞(q, q) + NS::operator⊖(q);
}
static_assert(qualified() == 43);

struct Mem {
  constexpr int operator⊞(Mem) const { return 31; }  // member infix
  constexpr int operator⊖() const { return 32; }     // member prefix

  // An unqualified explicit call inside the class finds the member through
  // the implicit object argument, exactly as `operator+` would.
  constexpr int inside(Mem o) const { return operator⊞(o); }
};

constexpr int member_calls() {
  Mem m;
  Mem *p = &m;
  return m.operator⊞(m)         // object expression
       + p->operator⊞(m)        // through a pointer
       + m.Mem::operator⊞(m)    // qualified member call
       + m.operator⊖();         // member prefix form
}
static_assert(member_calls() == 31 * 3 + 32);
static_assert(Mem{}.inside(Mem{}) == 31);

//===--------------------------------------------------------------------===//
// 4. ADL -- the normative case (U6, section 17.4)
//===--------------------------------------------------------------------===//

namespace Adl {
struct A {};
enum E { e1 };
struct Ptr {};

constexpr int operator⊘(A, A) { return 41; }
constexpr int operator⊙(A) { return 42; }      // prefix form
constexpr int operator⊘(E, E) { return 43; }
constexpr int operator⊚(Ptr *, Ptr *) { return 44; }

// A hidden friend: reachable by ADL and by nothing else at all.
struct Hidden {
  friend constexpr int operator⊛(Hidden, Hidden) { return 45; }
};
} // namespace Adl

// Nothing named operator⊘ / operator⊙ / operator⊚ / operator⊛ is visible here
// by ordinary unqualified lookup. Every one of these is found purely by ADL.
static_assert(operator⊘(Adl::A{}, Adl::A{}) == 41);
static_assert(operator⊙(Adl::A{}) == 42);         // ADL for the prefix form too
static_assert(operator⊘(Adl::e1, Adl::e1) == 43); // enumeration's namespace
static_assert(operator⊛(Adl::Hidden{}, Adl::Hidden{}) == 45);  // hidden friend

constexpr int adl_pointer() {
  Adl::Ptr p;
  return operator⊚(&p, &p);  // pointee's namespace is associated
}
static_assert(adl_pointer() == 44);

// A class template's argument contributes its namespace, as for any call.
template <class T> struct Wrap {};
namespace Adl2 {
struct B {};
constexpr int operator⊜(Wrap<B>, Wrap<B>) { return 46; }
} // namespace Adl2
static_assert(operator⊜(Wrap<Adl2::B>{}, Wrap<Adl2::B>{}) == 46);

// ADL *augments* the ordinary-lookup set rather than being masked by it: an
// ordinary-lookup candidate exists here and is not viable, and the ADL
// candidate is still found. This is the shape GCC's DEV-G05 got wrong.
struct Local {};
constexpr int operator⊝(Local, Local) { return 47; }
namespace Adl3 {
struct C {};
constexpr int operator⊝(C, C) { return 48; }
} // namespace Adl3
static_assert(operator⊝(Local{}, Local{}) == 47);
static_assert(operator⊝(Adl3::C{}, Adl3::C{}) == 48);

// A using-declaration brings the name in by ordinary lookup.
namespace Adl4 {
constexpr int operator⊟(int, int) { return 49; }
} // namespace Adl4
using Adl4::operator⊟;
static_assert(operator⊟(1, 2) == 49);

// ADL does not find *member* operators -- the same rule as for operator+.
namespace Adl5 {
struct D {
  constexpr int operator⊡(D) const { return 50; }
};
} // namespace Adl5
int adl_finds_no_members(Adl5::D d) {
  return operator⊡(d, d);
  // expected-error@-1 {{use of undeclared 'operator⊡'}}
}

// Ordinary lookup finding a class member suppresses ADL, as always.
struct Suppress {
  constexpr int operator⊘(Adl::A) const { return 51; }
  // The member is found by ordinary lookup, so Adl::operator⊘ is not
  // considered and the call resolves to the member.
  constexpr int m(Adl::A a) const { return operator⊘(a); }
};
static_assert(Suppress{}.m(Adl::A{}) == 51);

//===--------------------------------------------------------------------===//
// 4b. Negative cases -- lookup diagnostics, never syntax ones (U3)
//===--------------------------------------------------------------------===//

struct NotViable {};
int operator⊢(Base, Base);

int no_viable_overload(NotViable n) {
  return operator⊢(n, n);
  // expected-error@-1 {{no matching function for call to 'operator⊢'}}
  // expected-note@-5 {{candidate function not viable: no known conversion from 'NotViable' to 'Base' for 1st argument}}
}

int wrong_argument_count(Base b) {
  return operator⊢(b, b, b);
  // expected-error@-1 {{no matching function for call to 'operator⊢'}}
  // expected-note@-11 {{candidate function not viable: requires 2 arguments, but 3 were provided}}
}

int never_declared(Base b) {
  return operator⊣(b, b);
  // expected-error@-1 {{use of undeclared 'operator⊣'}}
}

// The same, for the prefix arity: nothing about the *use* is special.
int prefix_not_declared(Base b) {
  return operator⊣(b);
  // expected-error@-1 {{use of undeclared 'operator⊣'}}
}

//===--------------------------------------------------------------------===//
// 5. Templates, SFINAE, constraints
//===--------------------------------------------------------------------===//

// A function template operator, deduced and explicitly specified.
template <class T> constexpr T operator⊤(T a, T b) { return a + b; }
static_assert(operator⊤(2, 3) == 5);
static_assert(operator⊤<int>(2, 3) == 5);
static_assert(operator⊤(2.5, 0.5) == 3.0);

// A class template's member operators.
template <class T> struct Box {
  T v;
  constexpr T operator⊥(Box b) const { return v + b.v; }  // member infix
  constexpr T operator⊨() const { return -v; }            // member prefix
};
static_assert(Box<int>{3}.operator⊥(Box<int>{4}) == 7);
static_assert(Box<int>{3}.operator⊨() == -3);

// Dependent calls: non-member (resolved at instantiation, by ADL), member,
// and inside decltype.
namespace Dep {
struct E {};
constexpr int operator⋀(E, E) { return 61; }
constexpr int operator⋁(E) { return 62; }
} // namespace Dep

template <class T> constexpr auto dependent_infix(T a, T b) { return operator⋀(a, b); }
template <class T> constexpr auto dependent_prefix(T a) { return operator⋁(a); }
template <class T> constexpr auto dependent_member(T a, T b) { return a.operator⊥(b); }
template <class T> constexpr auto dependent_decltype(T a, T b) -> decltype(operator⋀(a, b)) {
  return operator⋀(a, b);
}

static_assert(dependent_infix(Dep::E{}, Dep::E{}) == 61);
static_assert(dependent_prefix(Dep::E{}) == 62);
static_assert(dependent_member(Box<int>{1}, Box<int>{2}) == 3);
static_assert(dependent_decltype(Dep::E{}, Dep::E{}) == 61);

// SFINAE on the return type: an unusable `operator⋀` removes the candidate
// rather than producing a hard error.
template <class T> constexpr bool has_op(int, decltype(operator⋀(T{}, T{})) * = nullptr) {
  return true;
}
template <class T> constexpr bool has_op(...) { return false; }
static_assert(has_op<Dep::E>(0));
static_assert(!has_op<Base>(0));

// requires-expression and requires-clause.
template <class T> concept Combinable = requires(T a, T b) { operator⋀(a, b); };
static_assert(Combinable<Dep::E>);
static_assert(!Combinable<Base>);

template <class T>
  requires Combinable<T>
constexpr int constrained(T a, T b) {
  return operator⋀(a, b);
}
static_assert(constrained(Dep::E{}, Dep::E{}) == 61);

int unsatisfied(Base b) {
  return constrained(b, b);
  // expected-error@-1 {{no matching function for call to 'constrained'}}
  // expected-note@-8 {{candidate template ignored: constraints not satisfied [with T = Base]}}
  // expected-note@-10 {{because 'Base' does not satisfy 'Combinable'}}
  // expected-note@-16 {{because 'operator⋀(a, b)' would be invalid: use of undeclared 'operator⋀'}}
}

//===--------------------------------------------------------------------===//
// 6. Address-taking, and the operator-function-id as a name
//===--------------------------------------------------------------------===//

struct Addr {};
constexpr int operator⊩(Addr, Addr) { return 71; }
constexpr int operator⊪(Addr) { return 72; }

constexpr int (*fp_amp)(Addr, Addr) = &operator⊩;
constexpr int (*fp_bare)(Addr, Addr) = operator⊩;  // without &
constexpr auto fp_prefix = &operator⊪;
static_assert(fp_amp(Addr{}, Addr{}) == 71);
static_assert(fp_bare(Addr{}, Addr{}) == 71);
static_assert(fp_prefix(Addr{}) == 72);
static_assert(__is_same(decltype(&operator⊪), int (*)(Addr)));

// As a non-type template argument.
template <int (*F)(Addr, Addr)> struct Holder {
  static constexpr int call() { return F(Addr{}, Addr{}); }
};
static_assert(Holder<&operator⊩>::call() == 71);

// As a function argument, and target-typed out of an overload set.
constexpr int apply(int (*f)(Addr, Addr)) { return f(Addr{}, Addr{}); }
static_assert(apply(&operator⊩) == 71);
static_assert(apply(operator⊩) == 71);

constexpr int operator⊫(int, int) { return 73; }
constexpr double operator⊫(double, double) { return 74; }
constexpr int (*fp_overloaded)(int, int) = &operator⊫;  // target type selects
static_assert(fp_overloaded(0, 0) == 73);

// Pointer to member.
struct PM {
  constexpr int operator⊬(PM) const { return 75; }
};
constexpr int (PM::*pmf)(PM) const = &PM::operator⊬;
constexpr int through_pmf() {
  PM p;
  return (p.*pmf)(p);
}
static_assert(through_pmf() == 75);

//===--------------------------------------------------------------------===//
// 7. `template` disambiguator -- the shared literal-operator limitation
//===--------------------------------------------------------------------===//

// A member operator template is callable with explicit template arguments when
// the object expression is non-dependent...
struct TmplMem {
  template <class T> constexpr int operator⊭(T) const { return 81; }
};
static_assert(TmplMem{}.operator⊭<int>(0) == 81);
static_assert(TmplMem{}.template operator⊭<int>(0) == 81);

// ...but a *dependent* object expression with the `template` keyword is
// rejected. This is not a U10 finding to fix: it is exactly the behavior of a
// literal operator (see the control below), and it differs from `operator+`
// only because the shared path handles the built-in operator names specially.
// Asserted here so U11/U13 inherit a known baseline rather than rediscovering
// it. See the U10 handoff.
template <class T> int dependent_template_kw(T t) {
  return t.template operator⊭<int>(0);
  // expected-error@-1 {{'operator⊭' following the 'template' keyword cannot refer to a dependent template}}
}

// The control: the identical construct on a literal operator, unrelated to
// this feature and compiled the same way.
template <class T> int literal_operator_control(T t) {
  return t.template operator""_lit<int>(0);
  // expected-error@-1 {{'operator""_lit' following the 'template' keyword cannot refer to a dependent template}}
}

//===--------------------------------------------------------------------===//
// 8. Composability with -fbacktick (U7)
//===--------------------------------------------------------------------===//

// Both RUN lines above compile this whole file; the second adds -fbacktick.
// Every assertion here must hold identically under either, which is the U7
// ground rule that the two flags are independent.
