// RUN: %clang_cc1 -std=c++17 -fbacktick -analyze -analyzer-config eagerly-assume=false \
// RUN:   -analyzer-config suppress-null-return-paths=false \
// RUN:   -analyzer-checker=core,debug.ExprInspection -verify=expected,nosupp %s
// RUN: %clang_cc1 -std=c++17 -fbacktick -analyze -analyzer-config eagerly-assume=false \
// RUN:   -analyzer-checker=core,debug.ExprInspection -verify=expected %s
// RUN: %clang_cc1 -std=c++17 -fbacktick -analyze -analyzer-checker=debug.DumpCFG \
// RUN:   -analyzer-config cfg-temporary-dtors=true %s 2>&1 | FileCheck %s

// A BacktickInfixExpr is a transparent wrapper over the call `x `f` y`
// desugars to. The analyzer must reason about it exactly as it reasons about
// the call: the CFG looks through the wrapper, Environment resolves a read of
// it to the call's binding, LiveVariables keys liveness on the same expression
// the binding uses, and the bug reporter's tracker peels it before looking for
// the graph node that computed the value.
//
// Two RUN lines analyze this file, differing only in
// suppress-null-return-paths -- off in the first, at its default in the
// second. `expected-` directives are checked by both; `nosupp-` only by the
// first. A line with a `nosupp-` directive and no `expected-` one therefore
// asserts two things at once: the report is emitted with the suppression off,
// and it is *not* emitted with the suppression on.

void clang_analyzer_eval(bool);

int pick(int a, int) { return a; }

void value_flows_through() {
  int x = 5 `pick` 7;
  clang_analyzer_eval(x == 5); // expected-warning{{TRUE}}
  int y = pick(5, 7);
  clang_analyzer_eval(y == 5); // expected-warning{{TRUE}}
}

int *identity(int *p, int) { return p; }

// The load-bearing assertion, and what it is load-bearing *for*: before the
// CFG looked through the wrapper, ExprEngine::Visit had no case for the node,
// every path reaching one was dropped, and the whole function went unanalyzed
// -- so no report came out of it, for any reason at all.
//
// It now runs with suppress-null-return-paths=false, and did not always. The
// assertion used to sit at the default setting, where it passed only because
// the wrapper *defeated* a suppression the identically-desugaring explicit
// call received: it read as an assertion of parity while what it actually
// pinned was a divergence. The thing it was always for is the paragraph above,
// and that is what it tests now, on the setting where the two forms are meant
// to report alike.
void bugs_are_still_found() {
  int *p = nullptr;
  int *q = p `identity` 0;
  *q = 1; // nosupp-warning{{Dereference of null pointer}}
}

// Its counterpart, which is the whole point of the pair. suppress-null-return-
// paths, on by default, suppresses a null dereference whose null came from an
// inlined callee's return; both of these are that shape. So with the
// suppression off both report, and at the default neither does -- and the
// second RUN line enforces the "neither" by having no directive to match.
void bugs_are_still_found_explicit() {
  int *p = nullptr;
  int *q = identity(p, 0);
  *q = 1; // nosupp-warning{{Dereference of null pointer}}
}

// A bug the suppression was never meant to reach: the null is dereferenced in
// the *operand*, so nothing about it came from a return. It must be found at
// both settings, through the wrapper and through the call alike -- this is the
// assertion that the analyzer still walks into a wrapped expression, held at
// the default configuration where the two above are silent.
void operand_bugs_are_found_at_the_default_setting() {
  int *p = nullptr;
  int x = *p `pick` 0; // expected-warning{{Dereference of null pointer}}
  (void)x;
}

void operand_bugs_are_found_explicit() {
  int *p = nullptr;
  int x = pick(*p, 0); // expected-warning{{Dereference of null pointer}}
  (void)x;
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
