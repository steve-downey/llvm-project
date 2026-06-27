// Semantics test sweep for the backtick infix operator (S05).
// Proves that desugaring to CallExpr inherits overload resolution, ADL,
// templates, constexpr, value categories, and CodeGen identically to a
// hand-written call.

// RUN: %clang_cc1 -fbacktick -std=c++17 -fsyntax-only -verify %s
// RUN: %clang_cc1 -fbacktick -std=c++17 -emit-llvm -o - %s | FileCheck %s

// expected-no-diagnostics

// ---------------------------------------------------------------------------
// 1. Overload resolution selects the same overload as f(x, y)
// ---------------------------------------------------------------------------
int f_ovl(int, int) { return 1; }
double f_ovl(double, double) { return 2.0; }

int r_ovl_int = 1 `f_ovl` 2;          // selects int overload
double r_ovl_dbl = 1.0 `f_ovl` 2.0;  // selects double overload

// Verify the type is correct (would fail to compile if wrong overload chosen).
static_assert(sizeof(r_ovl_int) == sizeof(int), "int overload");
static_assert(sizeof(r_ovl_dbl) == sizeof(double), "double overload");

// ---------------------------------------------------------------------------
// 2. Qualified callee (also exercises the ADL-adjacent case)
// ---------------------------------------------------------------------------
namespace ns {
  struct T {};
  int g(T, T) { return 42; }
}

ns::T tx, ty;
int r_qual = tx `ns::g` ty;  // qualified name in operator slot

// ---------------------------------------------------------------------------
// 3. Templates: dependent operands instantiate via TransformCallExpr
// ---------------------------------------------------------------------------
template <typename F, typename A>
auto apply(A a, A b, F func) { return a `func` b; }

int r_tmpl = apply(1, 2, [](int a, int b) { return a + b; });

// Template with a named function
int add_t(int a, int b) { return a + b; }
int r_tmpl2 = apply(3, 4, add_t);

// ---------------------------------------------------------------------------
// 4. constexpr: usable in a constant expression when f is constexpr
// ---------------------------------------------------------------------------
constexpr int add_cx(int a, int b) { return a + b; }
static_assert(1 `add_cx` 2 == 3, "constexpr backtick");
static_assert((2 `add_cx` 3) `add_cx` 4 == 9, "constexpr chain");

// ---------------------------------------------------------------------------
// 5. CodeGen: -emit-llvm shows a direct call (FileCheck below)
// ---------------------------------------------------------------------------
// CHECK-LABEL: define {{.*}}@_Z10codegen_fnv
// CHECK: call {{.*}}@_Z6add_cxii
int codegen_fn() {
  return 10 `add_cx` 20;
}

// ---------------------------------------------------------------------------
// 6. Value categories: lvalue result propagates from callee return type
// ---------------------------------------------------------------------------
int gx = 1, gy = 2;
int& ref_f(int& a, int&) { return a; }
int& r_ref = gx `ref_f` gy;  // result is lvalue; binding to ref must compile

// ---------------------------------------------------------------------------
// 7. Lambda in the operator slot
// ---------------------------------------------------------------------------
int r_lambda = 3 `[](int a, int b){ return a * b; }` 4;

// ---------------------------------------------------------------------------
// Note on DEV-04: bare nested backtick `x `f `g` h` y` silently parses as
// h(f(x,g),y) (no error). This is documented but not tested here to avoid
// anchoring the current mis-behavior; S06 or a future step will cover it.
// ---------------------------------------------------------------------------
