// RUN: %clang_cc1 -std=c++17 -fbacktick -analyze -analyzer-config eagerly-assume=false \
// RUN:   -analyzer-checker=core,debug.ExprInspection -verify %s
// RUN: %clang_cc1 -std=c++17 -fbacktick -analyze -analyzer-checker=debug.DumpCFG \
// RUN:   -analyzer-config cfg-temporary-dtors=true %s 2>&1 | FileCheck %s

// A BacktickInfixExpr is a transparent wrapper over the call `x `f` y`
// desugars to. The analyzer must reason about it exactly as it reasons about
// the call: the CFG looks through the wrapper, Environment resolves a read of
// it to the call's binding, and LiveVariables keys liveness on the same
// expression the binding uses.
//
// Before this was so, ExprEngine::Visit had no case for the node, so every
// path reaching one was dropped and the *whole function* went unanalyzed --
// which is why the null dereferences below are the load-bearing assertions.

void clang_analyzer_eval(bool);

int pick(int a, int) { return a; }

void value_flows_through() {
  int x = 5 `pick` 7;
  clang_analyzer_eval(x == 5); // expected-warning{{TRUE}}
  int y = pick(5, 7);
  clang_analyzer_eval(y == 5); // expected-warning{{TRUE}}
}

int *identity(int *p, int) { return p; }

void bugs_are_still_found() {
  int *p = nullptr;
  int *q = p `identity` 0;
  *q = 1; // expected-warning{{Dereference of null pointer}}
}

struct Res {
  int v;
  ~Res();
};
Res mk(int a, int) { return Res{a}; }

void class_typed_result() {
  const Res &r = 1 `mk` 2;
  clang_analyzer_eval(r.v == 1); // expected-warning{{TRUE}}
}

// The wrapper is not a CFG element of its own, so the construction context of
// the call reaches the materialization ([B1.8], the MaterializeTemporaryExpr)
// rather than stopping at the CXXBindTemporaryExpr. And because the wrapper
// no longer swallows ExternallyDestructed on the way down, the
// lifetime-extended temporary is destroyed once, by the implicit destructor at
// end of scope -- not also by a temporary-object destructor.

// CHECK-LABEL: void class_typed_result()
// CHECK:      5: [B1.2]([B1.3], [B1.4]) (CXXRecordTypedCall, [B1.8])
// CHECK-NEXT: 6: [B1.5] (BindTemporary)
// The next element is the implicit cast, not the wrapper: the wrapper is not
// an element at all, it is only the printed spelling of the cast's operand.
// CHECK-NEXT: 7: [B1.3] `[B1.2]` [B1.4] (ImplicitCastExpr, NoOp, const Res)
// CHECK-NOT:  (Temporary object destructor)
// CHECK:      ~Res() (Implicit destructor)
