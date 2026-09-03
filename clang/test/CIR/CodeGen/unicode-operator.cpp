// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -funicode-operators \
// RUN:   -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

// A UserOperatorExpr is a transparent wrapper around the call `x ⊞ y`
// desugars to. Code generation is therefore that call's, and ClangIR has to
// say so at each of its four dispatch sites -- scalar, aggregate, complex and
// l-value -- because none of them reaches CallExpr through the wrapper on its
// own. Before this the first three diagnosed "Not Yet Implemented" naming the
// node, and the fourth was worse than that: it fell to emitLValue's default
// arm, which returns a default-constructed LValue whose null QualType asserts,
// so an l-value-returning user operator crashed the compiler rather than
// diagnosing it.
//
// Each shape is checked against the explicit call written out by hand. The
// claim is not "some call is emitted" but "the same call is emitted", which
// is the whole thesis of the feature at the level where it is finally cashed.

struct Agg { int a; int b; };
struct Box { int v; };

int            operator⊞(int a, int b) { return a + b; }
Agg            operator⊞(Agg p, Agg q) { return Agg{p.a + q.a, p.b + q.b}; }
_Complex float operator⊞(_Complex float p, _Complex float q) { return p + q; }
int           &operator⊠(Box &b, int n) { return b.v; }
int            operator⊟(int a) { return -a; }

// Scalar: ScalarExprEmitter::VisitUserOperatorExpr.
int scalar_op(int a, int b) { return a ⊞ b; }
// CHECK-LABEL: cir.func{{.*}} @_Z9scalar_opii
// CHECK:         cir.call @_Zv28op_u229Eii

int scalar_call(int a, int b) { return operator⊞(a, b); }
// CHECK-LABEL: cir.func{{.*}} @_Z11scalar_callii
// CHECK:         cir.call @_Zv28op_u229Eii

// Aggregate: AggExprEmitter::VisitUserOperatorExpr. The class-typed result is
// built in the caller's slot, so this is the site where a missing arm shows up
// as AggExprEmitter::VisitStmt rather than as a scalar-kind diagnostic.
Agg agg_op(Agg p, Agg q) { return p ⊞ q; }
// CHECK-LABEL: cir.func{{.*}} @_Z6agg_op3AggS_
// CHECK:         cir.call @_Zv28op_u229E3AggS_

Agg agg_call(Agg p, Agg q) { return operator⊞(p, q); }
// CHECK-LABEL: cir.func{{.*}} @_Z8agg_call3AggS_
// CHECK:         cir.call @_Zv28op_u229E3AggS_

// Complex: ComplexExprEmitter::VisitUserOperatorExpr. Its fallback is
// errorUnsupported, which does not name the offending node -- "cannot compile
// this complex expression yet" -- so this shape is the one that would have
// been hardest to attribute in the field.
_Complex float complex_op(_Complex float p, _Complex float q) { return p ⊞ q; }
// CHECK-LABEL: cir.func{{.*}} @_Z10complex_opCfS_
// CHECK:         cir.call @_Zv28op_u229ECfS_

_Complex float complex_call(_Complex float p, _Complex float q) {
  return operator⊞(p, q);
}
// CHECK-LABEL: cir.func{{.*}} @_Z12complex_callCfS_
// CHECK:         cir.call @_Zv28op_u229ECfS_

// L-value: CIRGenFunction::emitLValue. Unlike the CXXRewrittenBinaryOperator
// arm beside it, this one has an answer rather than a diagnostic -- an
// operator returning a reference is a call returning a reference. The store
// through the returned pointer is the part worth checking.
void lvalue_op(Box &b) { (b ⊠ 1) = 42; }
// CHECK-LABEL: cir.func{{.*}} @_Z9lvalue_opR3Box
// CHECK:         %[[REF_OP:.*]] = cir.call @_Zv28op_u22A0R3Boxi
// CHECK:         cir.store align(4) %{{.*}}, %[[REF_OP]] : !s32i, !cir.ptr<!s32i>

void lvalue_call(Box &b) { operator⊠(b, 1) = 42; }
// CHECK-LABEL: cir.func{{.*}} @_Z11lvalue_callR3Box
// CHECK:         %[[REF_CALL:.*]] = cir.call @_Zv28op_u22A0R3Boxi
// CHECK:         cir.store align(4) %{{.*}}, %[[REF_CALL]] : !s32i, !cir.ptr<!s32i>

// Prefix (U5): the same node with one operand, and a different mangling, so
// it is worth its own case rather than being assumed to follow.
int prefix_op(int a) { return ⊟ a; }
// CHECK-LABEL: cir.func{{.*}} @_Z9prefix_opi
// CHECK:         cir.call @_Zv18op_u229Fi

int prefix_call(int a) { return operator⊟(a); }
// CHECK-LABEL: cir.func{{.*}} @_Z11prefix_calli
// CHECK:         cir.call @_Zv18op_u229Fi
