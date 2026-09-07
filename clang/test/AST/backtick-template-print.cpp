// RUN: %clang_cc1 -fbacktick -ast-print %s 2>&1 | FileCheck %s --check-prefix=PRINT
// RUN: %clang_cc1 -fbacktick -ast-print %s 2>&1 \
// RUN:   | %clang_cc1 -fbacktick -fsyntax-only -Wno-instantiation-after-specialization -x c++ -

// -ast-print round-trip for a backtick expression in a *template* context.
//
// clang/test/Parser/backtick-ast-print.cpp covers the non-dependent forms and
// one dependent type slot, all in the template *pattern*. Nothing covered an
// instantiation, so `TreeTransform::TransformBacktickInfixExpr` -- the hook
// that rebuilds the wrapper when a template is instantiated -- had no
// round-trip coverage at all: it could have dropped the wrapper and left the
// bare call, and every test would still have passed.
//
// The observable is that -ast-print prints instantiated class-template member
// bodies as well as patterns, so a printed specialization that still says
// `` a `add` b `` is the transformed node coming back out in the written form.
//
// Every function here has an explicit return type on purpose. `auto` would
// test upstream's deduced-return-type printing bug instead of this feature --
// see auto-return-round-trip in ops/BACKLOG.md.
//
// The second RUN line re-parses the printed output. -Wno-instantiation-after-
// specialization is not about backtick: -ast-print emits both `template<>
// struct Box<int> { ... }` and the `template struct Box<int>;` that caused it,
// and re-parsing that pair warns.
//
// NOT covered here, deliberately: argument-dependent lookup on the slot. The
// Clang slot is resolved at parse time and does not get ADL, in a template or
// out of one -- see clang-slot-adl in ops/BACKLOG.md. Do not read this file as
// evidence that lookup in a dependent context is right; it pins printing.

int add(int a, int b) { return a + b; }

struct Res { int v; ~Res(); };
Res mk(int, int);

struct Pair2 { Pair2(int, int); };

// 1. The pattern of a function template. Dependent operands, a non-dependent
//    slot; the wrapper survives to the printer.
// PRINT: template <class T> int fn(T a, T b) {
// PRINT-NEXT: return a `add` b;
template <class T> int fn(T a, T b) { return a `add` b; }
template int fn<int>(int, int);

// 2. A class template, printed twice -- pattern and instantiation. The second
//    is TransformBacktickInfixExpr's output, and is the whole point of the file.
template <class T> struct Box {
  int m(T a, T b) { return a `add` b; }

  // Class-typed result: Sema wraps the call in a CXXBindTemporaryExpr, so the
  // transformed wrapper's subexpression is again not the CallExpr.
  Res r(T a, T b) { return a `mk` b; }
};
template struct Box<int>;

// PRINT: template <class T> struct Box {
// PRINT: return a `add` b;
// PRINT: return a `mk` b;
// PRINT: template<> struct Box<int> {
// PRINT: return a `add` b;
// PRINT: return a `mk` b;

// 3. The slot is a template parameter, so the transform rebuilds the inner
//    call around a substituted callable rather than around a named function.
template <class T, class F> int slot(T a, T b, F f) { return a `f` b; }
int use_slot = slot(1, 2, add);
// PRINT: template <class T, class F> int slot(T a, T b, F f) {
// PRINT-NEXT: return a `f` b;
// PRINT: int slot<int, int (*)(int, int)>(int a, int b, int (*f)(int, int)) {
// PRINT-NEXT: return a `f` b;

// 4. A dependent qualified name in the slot. It is not an unqualified-id, so
//    it is an ordinary expression slot both before and after substitution.
template <class T> struct Holder { static int h(T, T); };
template <class T> int viaqual(T a, T b) { return a `Holder<T>::h` b; }
template int viaqual<int>(int, int);
// PRINT: template <class T> int viaqual(T a, T b) {
// PRINT-NEXT: return a `Holder<T>::h` b;

// 5. The type slot, substituted. In the pattern the inner node is a
//    CXXUnresolvedConstructExpr and in the instantiation a
//    CXXTemporaryObjectExpr, and both printer arms have to agree that the
//    written form is the backtick one.
template <class T> struct MkBox { T make(int a, int b) { return a `T` b; } };
template struct MkBox<Pair2>;
// PRINT: template <class T> struct MkBox {
// PRINT: return a `T` b;
// PRINT: template<> struct MkBox<Pair2> {
// PRINT: return a `Pair2` b;
