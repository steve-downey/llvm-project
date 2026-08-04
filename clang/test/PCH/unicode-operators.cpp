// U17: the CXXUserOperatorName DeclarationName kind and the UserOperatorExpr
// node survive a precompiled header.
//
// The load-bearing RUN line is the diff, not the FileCheck. The same source is
// compiled twice -- once whole (via -include, which parses this file's header
// half and then its body half), and once with the header half precompiled --
// and the two -ast-print outputs must be byte-identical. Both halves of the
// serialization can only fail loudly under that comparison:
//
//   * the name. A namespace-scope operator declaration lives in the PCH's
//     DeclContext lookup table, whose on-disk key is a DeclarationNameKey. If
//     the writer's key length, the writer's key bytes, the reader's key and
//     the stable hash do not all agree on "a uint32 code point", lookup for
//     operator<glyph> misses and every use below fails to compile.
//   * the node. A UserOperatorExpr in a serialized function body or variable
//     initializer must come back with its code point, its arity and its
//     operator location intact, or the printed form of a deserialized body
//     differs from the printed form of a parsed one.
//
// Before this step both were unreachable behind an llvm_unreachable naming it.

// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s

// RUN: %clang_cc1 -std=c++23 -funicode-operators -emit-pch -o %t.pch %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -include-pch %t.pch -fsyntax-only -verify %s

// RUN: %clang_cc1 -std=c++23 -funicode-operators -ast-print -include %s %s > %t.direct.txt
// RUN: %clang_cc1 -std=c++23 -funicode-operators -include-pch %t.pch -ast-print %s > %t.pch.txt
// RUN: diff -u %t.direct.txt %t.pch.txt
// RUN: FileCheck %s < %t.pch.txt

// The flag is independent of -fbacktick (U7): the same PCH content must behave
// identically with both flags on.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -emit-pch -o %t.bt.pch %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -include-pch %t.bt.pch -ast-print %s > %t.bt.txt
// RUN: diff -u %t.pch.txt %t.bt.txt

// expected-no-diagnostics

#ifndef HEADER
#define HEADER

//===--------------------------------------------------------------------===//
// The header half: everything that goes into the PCH.
//===--------------------------------------------------------------------===//

// Each overload returns a distinct type, so "the PCH gave back the same
// overload" is checkable by __is_same rather than by the code merely
// compiling.
template <int N> struct Tag {
  static constexpr int value = N;
};

constexpr Tag<1> operator⊞(int, int) { return {}; }
constexpr Tag<2> operator⊞(double, double) { return {}; }
constexpr Tag<3> operator⊞(const char *, const char *) { return {}; }

// A second operator, so the key encoding is exercised with more than one code
// point and a hash collision between two distinct operators would show.
constexpr Tag<4> operator⊗(int, int) { return {}; }

// The U1 set spans U+2190..U+2BFF, so no user-operator name fits in the byte
// that CXXOperatorName's key uses; this one is far enough from the others
// that a truncated key would alias.
constexpr Tag<5> operator⨁(int, int) { return {}; }

constexpr int add(int a, int b) { return a + b; }
constexpr int operator⊕(int a, int b) { return add(a, b); }

// The data member is declared *after* the member operators only to work
// around a pre-existing -ast-print/PCH artifact unrelated to this step:
// deserializing a class whose fields precede its methods prints the fields
// last, so the field-first spelling makes the diff below fail for a reason
// that has nothing to do with user operators.
struct Mem {
  constexpr Tag<6> operator⊗(Mem) const { return {}; }
  constexpr int operator⊕(int n) const { return v + n; }
  int v;
};

namespace ADL {
struct S {
  int v;
};
constexpr int operator⊕(S a, S b) { return a.v + b.v; }
} // namespace ADL

// A UserOperatorExpr inside a serialized variable initializer.
constexpr int header_value = 5 ⊕ 7;

// A UserOperatorExpr inside a serialized function body.
constexpr int header_fn(int a, int b) { return a ⊕ b ⊕ 1; }

// A *dependent* UserOperatorExpr inside a serialized template body: nothing
// is resolved when this is written to the PCH, so instantiating it after
// deserialization runs TreeTransform over a node that came off disk.
template <class T> constexpr auto header_tmpl(T a, T b) -> decltype(a ⊕ b) {
  return a ⊕ b;
}

template <class T>
concept Combinable = requires(T a, T b) { a ⊕ b; };

// The three TreeTransform shapes the AST test does not reach -- a class
// template with a *member* user operator, a partial specialization of it, and
// a use inside a lambda inside a template -- each here on the far side of a
// serialization boundary, which is strictly more than instantiating them in
// the TU that wrote them.
template <class T, class U> struct Box {
  constexpr int operator⊕(Box b) const { return v + b.v; }
  T v;
};

template <class T> struct Box<T, void> {
  constexpr int operator⊕(Box b) const { return v * b.v; }
  T v;
};

template <class T> constexpr int via_lambda(T a, T b) {
  auto f = [](T x, T y) { return x ⊕ y; };
  return f(a, b);
}

#else

//===--------------------------------------------------------------------===//
// The body half: compiled against the PCH.
//===--------------------------------------------------------------------===//

// 1. Name lookup through the deserialized DeclContext lookup table. If the
//    DeclarationNameKey encoding is wrong these do not compile at all.
static_assert(__is_same(decltype(1 ⊞ 2), Tag<1>));
static_assert(__is_same(decltype(1.0 ⊞ 2.0), Tag<2>));
static_assert(__is_same(decltype("a" ⊞ "b"), Tag<3>));
static_assert(__is_same(decltype(1 ⊗ 2), Tag<4>));
static_assert(__is_same(decltype(1 ⨁ 2), Tag<5>));

// The explicit call must select the same overload as the infix use (17.4).
static_assert(__is_same(decltype(operator⊞(1, 2)), Tag<1>));
static_assert(__is_same(decltype(operator⨁(1, 2)), Tag<5>));

// 2. Values computed in the PCH, and values computed against it.
static_assert(header_value == 12);
static_assert(header_fn(5, 7) == 13);
static_assert((5 ⊕ 7) == 12);

// 3. Member candidates, which are a property of the operator syntax and so
//    are the thing most likely to be lost across a lookup-table round trip.
constexpr Mem m1{3}, m2{4};
static_assert(__is_same(decltype(m1 ⊗ m2), Tag<6>));
static_assert((m1 ⊕ 4) == 7);

// 4. ADL against a namespace deserialized from the PCH.
constexpr ADL::S s1{10}, s2{20};
static_assert((s1 ⊕ s2) == 30);

// 5. Instantiating a template whose body came off disk -- TreeTransform over a
//    deserialized UserOperatorExpr -- including with a type declared *after*
//    the PCH was written, so the operator is found by ADL at phase 2.
namespace Late {
struct L {
  int v;
};
constexpr int operator⊕(L a, L b) { return a.v * b.v; }
} // namespace Late

static_assert(header_tmpl(2, 3) == 5);
static_assert(header_tmpl(Late::L{3}, Late::L{4}) == 12);
static_assert(header_tmpl(ADL::S{1}, ADL::S{2}) == 3);
static_assert(Combinable<int>);
static_assert(Combinable<Late::L>);
static_assert(!Combinable<Mem *>);

// 6. Class template with a member user operator, its partial specialization,
//    and a lambda body instantiated from the PCH.
static_assert((Box<int, int>{3} ⊕ Box<int, int>{4}) == 7);
static_assert((Box<int, void>{3} ⊕ Box<int, void>{4}) == 12);
static_assert(via_lambda(2, 3) == 5);
static_assert(via_lambda(Late::L{3}, Late::L{4}) == 12);

// 7. Uses written in this TU, printed back as operators.
constexpr int body_fn(int a, int b) { return a ⊕ b ⊕ 2; }
static_assert(body_fn(1, 2) == 5);

// CHECK: constexpr int header_value = 5 ⊕ 7;
// CHECK: constexpr int header_fn(int a, int b) {
// CHECK-NEXT: return a ⊕ b ⊕ 1;
// CHECK: constexpr int body_fn(int a, int b) {
// CHECK-NEXT: return a ⊕ b ⊕ 2;

#endif
