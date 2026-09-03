// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fbacktick \
// RUN:   -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

// The backtick twin of unicode-operator.cpp, and the same four dispatch
// sites. `x `f` y` is sugar for f(x, y), and a BacktickInfixExpr wraps that
// call, so ClangIR must look through the wrapper at every site that does not
// reach CallExpr on its own. It did not: scalar and aggregate diagnosed "Not
// Yet Implemented" naming the node, complex diagnosed "cannot compile this
// complex expression yet" without naming it, and the l-value shape crashed on
// a null QualType from emitLValue's default arm.
//
// Each shape is checked against the call written out by hand.

struct Agg { int a; int b; };
struct Box { int v; };

int            plus(int a, int b) { return a + b; }
Agg            plus(Agg p, Agg q) { return Agg{p.a + q.a, p.b + q.b}; }
_Complex float plus(_Complex float p, _Complex float q) { return p + q; }
int           &at(Box &b, int n) { return b.v; }

// Scalar: ScalarExprEmitter::VisitBacktickInfixExpr.
int scalar_op(int a, int b) { return a `plus` b; }
// CHECK-LABEL: cir.func{{.*}} @_Z9scalar_opii
// CHECK:         cir.call @_Z4plusii

int scalar_call(int a, int b) { return plus(a, b); }
// CHECK-LABEL: cir.func{{.*}} @_Z11scalar_callii
// CHECK:         cir.call @_Z4plusii

// Aggregate: AggExprEmitter::VisitBacktickInfixExpr.
Agg agg_op(Agg p, Agg q) { return p `plus` q; }
// CHECK-LABEL: cir.func{{.*}} @_Z6agg_op3AggS_
// CHECK:         cir.call @_Z4plus3AggS_

Agg agg_call(Agg p, Agg q) { return plus(p, q); }
// CHECK-LABEL: cir.func{{.*}} @_Z8agg_call3AggS_
// CHECK:         cir.call @_Z4plus3AggS_

// Complex: ComplexExprEmitter::VisitBacktickInfixExpr.
_Complex float complex_op(_Complex float p, _Complex float q) {
  return p `plus` q;
}
// CHECK-LABEL: cir.func{{.*}} @_Z10complex_opCfS_
// CHECK:         cir.call @_Z4plusCfS_

_Complex float complex_call(_Complex float p, _Complex float q) {
  return plus(p, q);
}
// CHECK-LABEL: cir.func{{.*}} @_Z12complex_callCfS_
// CHECK:         cir.call @_Z4plusCfS_

// L-value: CIRGenFunction::emitLValue.
void lvalue_op(Box &b) { (b `at` 1) = 42; }
// CHECK-LABEL: cir.func{{.*}} @_Z9lvalue_opR3Box
// CHECK:         %[[REF_OP:.*]] = cir.call @_Z2atR3Boxi
// CHECK:         cir.store align(4) %{{.*}}, %[[REF_OP]] : !s32i, !cir.ptr<!s32i>

void lvalue_call(Box &b) { at(b, 1) = 42; }
// CHECK-LABEL: cir.func{{.*}} @_Z11lvalue_callR3Box
// CHECK:         %[[REF_CALL:.*]] = cir.call @_Z2atR3Boxi
// CHECK:         cir.store align(4) %{{.*}}, %[[REF_CALL]] : !s32i, !cir.ptr<!s32i>
