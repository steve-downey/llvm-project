// U17: a Unicode user-defined operator crosses a C++20 named-module boundary.
//
// This is the sharper half of the serialization gate. A PCH re-reads a lookup
// table the same compilation wrote; a module interface is written by one
// compilation and read by another, and the importer finds the operator only
// by looking its DeclarationName up in the on-disk hash table. That lookup
// goes through DeclarationNameKey's stable hash, which must agree with the
// key bytes the writer emitted -- and a disagreement is silent: lookup simply
// misses and the operator "is not declared".
//
// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -emit-module-interface %t/ops.cppm -o %t/ops.pcm
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fmodule-file=ops=%t/ops.pcm %t/use.cpp -fsyntax-only -verify
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fmodule-file=ops=%t/ops.pcm %t/use.cpp -ast-print | FileCheck %t/use.cpp
//
// The reduced BMI drops function bodies that are not needed, which is a
// different serialization path over the same names.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -emit-reduced-module-interface %t/ops.cppm -o %t/ops-reduced.pcm
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fmodule-file=ops=%t/ops-reduced.pcm %t/use.cpp -fsyntax-only -verify
//
// Composability with -fbacktick (U7): the flags are independent, so a module
// built with both must behave exactly as one built with only -funicode-
// operators. Both LangOptions are NotCompatible, so the module has to be
// rebuilt rather than reused -- which is itself the check that the flag is
// recorded in the module and not silently dropped.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -emit-module-interface %t/ops.cppm -o %t/ops-bt.pcm
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -fmodule-file=ops=%t/ops-bt.pcm %t/use.cpp -fsyntax-only -verify
//
// And the negative: a module that declares user operators cannot be loaded
// into a compilation that has the feature off. The names would have nothing
// to be looked up as.
// RUN: not %clang_cc1 -std=c++23 -fmodule-file=ops=%t/ops.pcm %t/use.cpp -fsyntax-only 2>&1 \
// RUN:   | FileCheck --check-prefix=NOFLAG %s
// NOFLAG: Unicode user-defined operators was enabled in precompiled file
// NOFLAG-SAME: but is currently disabled

//--- ops.cppm
export module ops;

export template <int N> struct Tag {
  static constexpr int value = N;
};

export constexpr Tag<1> operator⊞(int, int) { return {}; }
export constexpr Tag<2> operator⊞(double, double) { return {}; }
export constexpr Tag<3> operator⊗(int, int) { return {}; }

// The prefix form (U5): a one-parameter operator sharing a code point with a
// two-parameter one. The node stores its arity rather than deriving it, so the
// two stay distinguishable across the module boundary.
export constexpr Tag<5> operator⊞(int) { return {}; }

export struct Mem {
  int v;
  constexpr Tag<4> operator⊗(Mem) const { return {}; }
  constexpr int operator⊕(int n) const { return v + n; }
  // No parameters is the member prefix form.
  constexpr Tag<6> operator⊕() const { return {}; }
};

export constexpr int operator⊕(int a, int b) { return a + b; }
export constexpr int operator⊕(int a) { return a + 100; }

export namespace ADL {
struct S {
  int v;
};
constexpr int operator⊕(S a, S b) { return a.v * b.v; }
} // namespace ADL

// A use written inside the interface, so a UserOperatorExpr is serialized
// into the module and evaluated by the importer.
export constexpr int module_value = 6 ⊕ 7;
export constexpr int module_fn(int a, int b) { return a ⊕ b ⊕ 1; }
export constexpr int module_prefix(int a) { return ⊕⊕a; }

// A dependent use: unresolved when written, transformed by the importer.
export template <class T>
constexpr auto module_tmpl(T a, T b) -> decltype(a ⊕ b) {
  return a ⊕ b;
}

export template <class T>
constexpr auto module_prefix_tmpl(T a) -> decltype(⊕a) {
  return ⊕a;
}

export template <class T>
concept Combinable = requires(T a, T b) { a ⊕ b; };

//--- use.cpp
// expected-no-diagnostics
import ops;

// The operator is found only through the module's DeclContext lookup table.
static_assert(__is_same(decltype(1 ⊞ 2), Tag<1>));
static_assert(__is_same(decltype(1.0 ⊞ 2.0), Tag<2>));
static_assert(__is_same(decltype(1 ⊗ 2), Tag<3>));
static_assert(__is_same(decltype(operator⊞(1, 2)), Tag<1>));

// Prefix and infix uses of the same imported code point select different
// overloads: the arity travelled with the node and with the name.
static_assert(__is_same(decltype(⊞1), Tag<5>));

// Member candidates survive: they are a property of the operator syntax.
constexpr Mem m1{3}, m2{4};
static_assert(__is_same(decltype(m1 ⊗ m2), Tag<4>));
static_assert((m1 ⊕ 5) == 8);
static_assert(__is_same(decltype(⊕m1), Tag<6>));

// ADL into an imported namespace.
constexpr ADL::S s1{3}, s2{4};
static_assert((s1 ⊕ s2) == 12);

// Values and bodies that came off disk.
static_assert(module_value == 13);
static_assert(module_fn(6, 7) == 14);
static_assert(module_prefix(1) == 201);
static_assert(module_prefix_tmpl(1) == 101);
static_assert(__is_same(decltype(module_prefix_tmpl(m1)), Tag<6>));

// Instantiating an imported template, including for a type declared here, so
// the operator is found by ADL at phase 2 over a deserialized body.
namespace Local {
struct L {
  int v;
};
constexpr int operator⊕(L a, L b) { return a.v - b.v; }
} // namespace Local

static_assert(module_tmpl(2, 3) == 5);
static_assert(module_tmpl(Local::L{9}, Local::L{4}) == 5);
static_assert(Combinable<int>);
static_assert(Combinable<Local::L>);
static_assert(!Combinable<Mem *>);

constexpr int here(int a, int b) { return a ⊕ b ⊕ 2; }
static_assert(here(1, 2) == 5);

constexpr int here_prefix(int a) { return ⊕a; }
static_assert(here_prefix(1) == 101);

// CHECK: constexpr int here(int a, int b) {
// CHECK-NEXT: return a ⊕ b ⊕ 2;
// CHECK: constexpr int here_prefix(int a) {
// CHECK-NEXT: return ⊕a;
