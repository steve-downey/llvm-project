// Argument-dependent lookup in the backtick operator slot.
//
// The rule is normative: `x `f` y` performs ADL on the slot exactly as the
// call f(x, y) would, and must not silently have weaker lookup than the call
// it desugars to. Every shape below is written twice -- once as a spelled
// call, once as the operator -- and the spelled call is the control, so a
// shape that stops working for an unrelated reason fails on both lines.
//
// The third and fourth sections are the ones that matter most: they are the
// shapes in which weaker lookup on the slot produces no diagnostic at all,
// just a different function. Do not weaken them into a compiles/does-not
// check.
//
// This is the test this feature did not have. The ADL section of
// backtick-semantics.cpp uses a *qualified* name, which correctly gets no
// ADL either way and therefore passes whatever the slot does.

// RUN: %clang_cc1 -fbacktick -std=c++20 -fsyntax-only -verify %s

// ---------------------------------------------------------------------------
// 1. A hidden friend is reachable only by ADL.
// ---------------------------------------------------------------------------
struct S {
  int v;
  friend constexpr int hf(S a, S b) { return a.v + b.v; }
};
constexpr S s1{1}, s2{2};
static_assert(hf(s1, s2) == 3);
static_assert((s1 `hf` s2) == 3);

// ---------------------------------------------------------------------------
// 2. A namespace member reachable only through its argument's namespace.
// ---------------------------------------------------------------------------
namespace ns {
struct U {
  int v;
  constexpr operator double() const { return v; }
};
constexpr int go(U a, U b) { return a.v * b.v; }
} // namespace ns
constexpr ns::U u3{3}, u4{4};
static_assert(go(u3, u4) == 12);
static_assert((u3 `go` u4) == 12);

// ---------------------------------------------------------------------------
// 3. Augmentation: an ordinary-lookup candidate is visible AND viable, and a
//    better ADL candidate exists. Both forms must select the ADL candidate.
//    When the slot does not get ADL this compiles either way and binds a
//    different function, with no diagnostic -- which is the one failure the
//    desugaring cannot absorb. The tags make the choice observable.
// ---------------------------------------------------------------------------
struct AdlTag {};
struct OrdinaryTag {};
constexpr OrdinaryTag pick(double, double) { return {}; }
namespace ns {
constexpr AdlTag pick(U, U) { return {}; }
} // namespace ns
static_assert(__is_same(decltype(pick(u3, u4)), AdlTag));
static_assert(__is_same(decltype(u3 `pick` u4), AdlTag));

// ---------------------------------------------------------------------------
// 4. The same, with an overload *set* visible by ordinary lookup rather than
//    a single function, so the slot cannot get the right answer by accident.
// ---------------------------------------------------------------------------
constexpr OrdinaryTag choose(double, double) { return {}; }
constexpr OrdinaryTag choose(int, int) { return {}; }
constexpr OrdinaryTag choose(double, int) { return {}; }
namespace ns {
constexpr AdlTag choose(U, U) { return {}; }
} // namespace ns
static_assert(__is_same(decltype(choose(u3, u4)), AdlTag));
static_assert(__is_same(decltype(u3 `choose` u4), AdlTag));

// ---------------------------------------------------------------------------
// 5. A template-id slot. ADL binds wherever the slot is an unqualified name,
//    "whether or not it carries template arguments" -- this is the production
//    one grammar step over from section 1, and the place a fix that looks for
//    a bare identifier alone silently stops.
// ---------------------------------------------------------------------------
namespace ns {
template <class T> constexpr int addt(U a, U b) {
  return a.v + b.v + static_cast<int>(sizeof(T));
}
} // namespace ns
static_assert(addt<char>(u3, u4) == 8);
static_assert((u3 `addt<char>` u4) == 8);

// ---------------------------------------------------------------------------
// 6. Two-phase lookup: the ADL candidate is declared *after* the template
//    that uses it, so binding it requires lookup at the point of
//    instantiation. A slot resolved at definition time binds the wrong thing
//    or nothing at all.
// ---------------------------------------------------------------------------
namespace ns {
struct V {
  int v;
};
} // namespace ns
template <class T> constexpr int viaCall(T a, T b) { return later(a, b); }
template <class T> constexpr int viaTick(T a, T b) { return a `later` b; }
template <class T> constexpr int viaTickTmplId(T a, T b) {
  return a `laterT<char>` b;
}
namespace ns {
constexpr int later(V a, V b) { return a.v - b.v; }
template <class T> constexpr int laterT(V a, V b) {
  return a.v - b.v + static_cast<int>(sizeof(T));
}
} // namespace ns
constexpr ns::V v5{5}, v2{2};
static_assert(viaCall(v5, v2) == 3);
static_assert(viaTick(v5, v2) == 3);
static_assert(viaTickTmplId(v5, v2) == 4);

// ---------------------------------------------------------------------------
// 7. Where a call gets no ADL, the slot must get none either -- and must keep
//    parsing exactly as it did. A qualified name, a member access, a callable
//    object and a function pointer are all ordinary expressions in the slot.
// ---------------------------------------------------------------------------
// 8. ADL is not a licence to accept anything. A name reachable by neither
//    ordinary lookup nor ADL is still an error; so is a qualified name that
//    does not exist; and a block-scope function declaration still turns ADL
//    off, in the slot exactly as in the call ([basic.lookup.argdep]/3).
// ---------------------------------------------------------------------------
struct Unrelated {};
constexpr Unrelated un;
// expected-error@+1 {{use of undeclared identifier 'zqxwv'}}
int r_nowhere = un `zqxwv` un;
// expected-error@+1 {{no member named 'zqxwvu' in namespace 'ns'}}
int r_qual_nowhere = u3 `ns::zqxwvu` u4;

void blockScope() {
  // [basic.lookup.argdep]/3: a block-scope function declaration suppresses
  // ADL. Both forms must therefore bind the block-scope declaration, and not
  // ns::pick -- which is the mirror image of section 3 and the check that the
  // slot has not simply gained unconditional ADL.
  OrdinaryTag pick(ns::U, ns::U);
  static_assert(__is_same(decltype(pick(u3, u4)), OrdinaryTag));
  static_assert(__is_same(decltype(u3 `pick` u4), OrdinaryTag));
}
