// RUN: %clang_cc1 -std=c++17 -funicode-operators -analyze \
// RUN:   -analyzer-config eagerly-assume=false \
// RUN:   -analyzer-config suppress-null-return-paths=false \
// RUN:   -analyzer-checker=core,debug.ExprInspection -verify=expected,nosupp %s
// RUN: %clang_cc1 -std=c++17 -funicode-operators -analyze \
// RUN:   -analyzer-config eagerly-assume=false \
// RUN:   -analyzer-checker=core,debug.ExprInspection -verify=expected %s
// RUN: %clang_cc1 -std=c++17 -funicode-operators -analyze \
// RUN:   -analyzer-checker=debug.DumpCFG \
// RUN:   -analyzer-config cfg-temporary-dtors=true %s 2>&1 | FileCheck %s

// A UserOperatorExpr is a transparent wrapper over the call `x ⊞ y` desugars
// to. The analyzer must reason about it exactly as it reasons about that call:
// the CFG looks through the wrapper, Environment resolves a read of it to the
// call's binding, LiveVariables keys liveness on the same expression the
// binding uses, and the bug reporter's tracker peels the wrapper before
// looking for the graph node that computed the value.
//
// Two RUN lines analyze this file, differing only in
// suppress-null-return-paths -- off in the first, at its default in the
// second. `expected-` directives are checked by both; `nosupp-` only by the
// first. A line carrying a `nosupp-` directive and no `expected-` one
// therefore asserts two things at once: the report is emitted with the
// suppression off, and it is not emitted with the suppression on.
//
// "Transparent" here is the analyzer's sense, not TreeTransform's. The node is
// deliberately *not* transparent to transformation -- TransformUserOperatorExpr
// recovers the operands as written and re-runs overload resolution on them,
// which is the whole reason the node exists (DEV-U13). But it constructs itself
// from its semantic form's type, value kind and object kind, and its only child
// is that form, so value-wise it *is* the call, and every site below is about
// value.
//
// Before this was so, the wrapper was a CFG element of its own and liveness
// keyed on it rather than the call, so every user-operator result was reaped
// the instant it was bound and read back as UNKNOWN -- which is why the
// parity assertions below are load-bearing, not decorative.

void clang_analyzer_eval(bool);

int operator⊞(int a, int b) { return a + b; }
int operator⊟(int a) { return -a; }

// Value flow: the operator form must report exactly what the explicit call
// reports. Before the fix the operator forms reported UNKNOWN.
void value_flows_through_infix() {
  int x = 1 ⊞ 2;
  clang_analyzer_eval(x == 3); // expected-warning{{TRUE}}
  int y = operator⊞(1, 2);
  clang_analyzer_eval(y == 3); // expected-warning{{TRUE}}
}

// The prefix form (U12) is the same node with NumOperands == 1, but the
// operand recovery differs, so it is worth its own case.
void value_flows_through_prefix() {
  int x = ⊟ 5;
  clang_analyzer_eval(x == -5); // expected-warning{{TRUE}}
  int y = operator⊟(5);
  clang_analyzer_eval(y == -5); // expected-warning{{TRUE}}
}

struct M {
  int k;
  int operator⊠(int b) const { return k + b; }
};

// The member form's semantic form is a CXXMemberCallExpr, whose object
// argument is operand 0.
void value_flows_through_member() {
  M m{10};
  int x = m ⊠ 5;
  clang_analyzer_eval(x == 15); // expected-warning{{TRUE}}
  int y = m.operator⊠(5);
  clang_analyzer_eval(y == 15); // expected-warning{{TRUE}}
}

int *operator⊘(int *p, int) { return p; }
int *operator⊙(int *p) { return p; }
int *identity(int *p, int) { return p; }

// A bug found through the explicit call must still be found through the
// operator. Before the fix the operator forms produced no report at all,
// because the result was UNKNOWN.
//
// All three are the shape suppress-null-return-paths exists for: the null
// comes from an inlined callee's return. So with the suppression off all three
// report, and at the default none of them does -- which the second RUN line
// enforces by having no directive to match. That second half is new. It used
// to be false: the wrapper hid the call from the suppression, so the operator
// forms reported where the identically-desugaring call was silent, and this
// file could only test parity by turning the suppression off. It now tests
// parity at both settings.
void bugs_are_still_found_infix() {
  int *p = nullptr;
  int *q = p ⊘ 0;
  *q = 1; // nosupp-warning{{Dereference of null pointer}}
}

void bugs_are_still_found_prefix() {
  int *p = nullptr;
  int *q = ⊙ p;
  *q = 1; // nosupp-warning{{Dereference of null pointer}}
}

void bugs_are_still_found_explicit() {
  int *p = nullptr;
  int *q = identity(p, 0);
  *q = 1; // nosupp-warning{{Dereference of null pointer}}
}

// A bug the suppression was never meant to reach: the null is dereferenced in
// an *operand*, so nothing about it came from a return. It must be found at
// both settings, through either wrapper form and through the call alike --
// this is the assertion that the analyzer still walks into a wrapped
// expression, held at the default configuration where the three above are
// silent.
void operand_bugs_are_found_at_the_default_setting() {
  int *p = nullptr;
  int x = *p ⊞ 0; // expected-warning{{Dereference of null pointer}}
  (void)x;
}

void operand_bugs_are_found_prefix() {
  int *p = nullptr;
  int x = ⊟ *p; // expected-warning{{Dereference of null pointer}}
  (void)x;
}

void operand_bugs_are_found_explicit() {
  int *p = nullptr;
  int x = operator⊞(*p, 0); // expected-warning{{Dereference of null pointer}}
  (void)x;
}

struct Res {
  int v;
  ~Res();
};
Res operator⊕(int a, int) { return Res{a}; }

void class_typed_result() {
  const Res &r = 1 ⊕ 2;
  clang_analyzer_eval(r.v == 1); // expected-warning{{TRUE}}
}

// The wrapper is not a CFG element of its own, so the construction context of
// the call reaches the materialization ([B1.8], the MaterializeTemporaryExpr)
// rather than stopping at the CXXBindTemporaryExpr. And because the wrapper no
// longer swallows ExternallyDestructed on the way down, the lifetime-extended
// temporary is destroyed once, by the implicit destructor at end of scope --
// not also by a temporary-object destructor.

// CHECK-LABEL: void class_typed_result()
// CHECK:      5: [B1.2]([B1.3], [B1.4]) (CXXRecordTypedCall, [B1.8])
// CHECK-NEXT: 6: [B1.5] (BindTemporary)
// The next element is the implicit cast, not the wrapper: the wrapper is not
// an element at all, it is only the printed spelling of the cast's operand.
// CHECK-NEXT: 7: [B1.3] ⊕ [B1.4] (ImplicitCastExpr, NoOp, const Res)
// CHECK-NOT:  (Temporary object destructor)
// CHECK:      ~Res() (Implicit destructor)
