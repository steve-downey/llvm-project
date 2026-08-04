// REQUIRES: x86-registered-target

// U14, the runtime half: the properties of "it is just the call" that only a
// code generator can show. SemaCXX/unicode-operator-semantics.cpp owns
// everything decidable at compile time.
//
// The first two pairs of RUN lines are the strongest form of the inheritance
// claim available: the SAME function bodies are compiled twice, once written
// with operator syntax and once written as explicit calls, and the two modules
// must be byte-identical. Not "both compile" and not "both call the right
// symbol" -- identical IR, with and without -fbacktick.
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions -DFORM_OP -emit-llvm -o %t.op.ll %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions -DFORM_CALL -emit-llvm -o %t.call.ll %s
// RUN: diff %t.op.ll %t.call.ll
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions -DFORM_OP -emit-llvm -o %t.bt.op.ll %s
// RUN: diff %t.op.ll %t.bt.op.ll
//
// Then the individual properties, on the operator-syntax build.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -fcxx-exceptions -fexceptions -DFORM_OP -emit-llvm -o - %s | FileCheck %s

#ifdef FORM_OP
#define INFIX(a, b) ((a) ⊞ (b))
#define PREFIX(a) (⊖(a))
#define NOTHROW(a, b) ((a) ⊗ (b))
#define REF(a, b) ((a) ⊘ (b))
#define MEMBER(a, b) ((a) ⊪ (b))
#define FOLDED(a, b) ((a) ⊙ (b))
#else
#define INFIX(a, b) operator⊞((a), (b))
#define PREFIX(a) operator⊖((a))
#define NOTHROW(a, b) operator⊗((a), (b))
#define REF(a, b) operator⊘((a), (b))
#define MEMBER(a, b) (a).operator⊪((b))
#define FOLDED(a, b) operator⊙((a), (b))
#endif

int operator⊞(int, int);
int operator⊖(int);
int operator⊗(int, int) noexcept;
int &operator⊘(int, int);
constexpr int operator⊙(int a, int b) { return a * 100 + b; }

struct Obj {
  int v;
  int operator⊪(int) const;
};
Obj make_obj(int);
int side(int);

//===----------------------------------------------------------------------===//
// The same code, emitted
//===----------------------------------------------------------------------===//

// CHECK-LABEL: define {{.*}} @_Z6simpleii
// CHECK: call noundef i32 @_Zv28op_u229Eii
int simple(int a, int b) { return INFIX(a, b); }

// CHECK-LABEL: define {{.*}} @_Z6prefixi
// CHECK: call noundef i32 @_Zv18op_u2296i
int prefix(int a) { return PREFIX(a); }

// Nested operands: two calls, then the operator's.
// CHECK-LABEL: define {{.*}} @_Z6nestedii
// CHECK: call noundef i32 @_Z4sidei
// CHECK: call noundef i32 @_Z4sidei
// CHECK: call noundef i32 @_Zv28op_u229Eii
int nested(int a, int b) { return INFIX(side(a), side(b)); }

// A returned reference is an lvalue: the call's result is a pointer and the
// store goes through it.
// CHECK-LABEL: define {{.*}} @_Z13store_throughii
// CHECK: %[[R:.*]] = call {{.*}} ptr @_Zv28op_u2298ii
// CHECK: store i32 42, ptr %[[R]]
void store_through(int a, int b) { REF(a, b) = 42; }

// A constexpr operator called with constant operands is still a call at -O0,
// and the definition is emitted -- the same as for the explicit call.
// CHECK-LABEL: define {{.*}} @_Z6foldedv
// CHECK: call noundef i32 @_Zv28op_u2299ii(i32 noundef 3, i32 noundef 4)
int folded() { return FOLDED(3, 4); }

//===----------------------------------------------------------------------===//
// Exceptions (step item 5): a throwing operator propagates
//===----------------------------------------------------------------------===//

struct Guard {
  ~Guard();
};

// With a non-trivial destructor live across it, a potentially-throwing
// operator is an `invoke` with a cleanup landing pad -- exactly what the
// explicit call produces, which is what the diff above proves.
// CHECK-LABEL: define {{.*}} @_Z8throwingii
// CHECK: invoke noundef i32 @_Zv28op_u229Eii
// CHECK: landingpad { ptr, i32 }
// CHECK: cleanup
// CHECK: call void @_ZN5GuardD1Ev
int throwing(int a, int b) {
  Guard g;
  return INFIX(a, b);
}

// The exception it throws is caught by an ordinary handler.
// CHECK-LABEL: define {{.*}} @_Z6caughtii
// CHECK: invoke noundef i32 @_Zv28op_u229Eii
// CHECK: landingpad { ptr, i32 }
// CHECK: catch ptr @_ZTIi
// CHECK: call {{.*}} @__cxa_begin_catch
int caught(int a, int b) {
  try {
    return INFIX(a, b);
  } catch (int e) {
    return e;
  }
}

// A `noexcept` operator needs no landing pad: the specification is the
// callee's, inherited rather than restated.
// CHECK-LABEL: define {{.*}} @_Z12not_throwingii
// CHECK-NOT: invoke
// CHECK: call noundef i32 @_Zv28op_u2297ii
// CHECK-NOT: landingpad
int not_throwing(int a, int b) {
  Guard g;
  return NOTHROW(a, b);
}

//===----------------------------------------------------------------------===//
// Evaluation order (step item 6): only the member form's guarantee is checked
//===----------------------------------------------------------------------===//
//
// For a NON-MEMBER operator the two operands are two call arguments, which are
// indeterminately sequenced: which is emitted first is unspecified, so
// `nested` above deliberately does not check WHICH `side` call comes first,
// only that the operator's call comes after both. The diff at the top of this
// file is what pins the operator form to the call form; asserting an order
// here would assert more than the language guarantees.
//
// For a MEMBER operator the left operand is the object expression, part of the
// postfix-expression, and [expr.call]p8 sequences the postfix-expression
// before every argument. That IS an order, and it is checkable.
//
// CHECK-LABEL: define {{.*}} @_Z12member_orderi
// CHECK: call {{.*}} @_Z8make_obji
// CHECK: call noundef i32 @_Z4sidei
// CHECK: call noundef i32 @_ZNK3Objv28op_u22AAEi
int member_order(int a) { return MEMBER(make_obj(a), side(a)); }
