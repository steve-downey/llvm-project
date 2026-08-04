// REQUIRES: x86-registered-target

// The call side, in one translation unit.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// Two translation units, one declaring and calling, one defining. The merge
// turns the declaration into a definition, which *is* the resolution proof.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -DTU_USE -emit-llvm-bc -o %t.use.bc %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -DTU_DEF -emit-llvm-bc -o %t.def.bc %s
// RUN: llvm-link %t.use.bc %t.def.bc -S -o - | FileCheck --check-prefix=LINKED %s

// The same two TUs as real object files: an undefined symbol in one and a
// defined one in the other, spelled identically.
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -DTU_USE -emit-obj -o %t.use.o %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -DTU_DEF -emit-obj -o %t.def.o %s
// RUN: llvm-nm %t.use.o | FileCheck --check-prefix=NM-USE %s
// RUN: llvm-nm %t.def.o | FileCheck --check-prefix=NM-DEF %s

// U10, the codegen half: which symbol an *explicit call* to a user-defined
// operator actually emits, and that the U09 mangling resolves across
// translation units.
//
// CodeGenCXX/unicode-operator-mangle.cpp owns the mangling of *declarations*
// -- free/member/explicit-object/template/namespace forms, the U1 range
// endpoints, the MSVC diagnostic. This file deliberately does not repeat any
// of that. What it adds is the four things only a call can show:
//
//   1. overload resolution picking one of several `operator⊞` and emitting
//      that one's symbol;
//   2. an ADL-found callee emitting the symbol of the function in the
//      *associated* namespace, not some locally-visible one;
//   3. a dependent call emitting the instantiated template's symbol;
//   4. two TUs actually agreeing on the symbol (step file item 7).
//
// Every symbol here is `v <arity> <source-name>` with the source-name derived
// from the code point, per U09; see that file for the rule.

struct S { int x; };
struct T { int x; };

#if defined(TU_USE)

// Declares and calls; does not define.
int operator⊞(S, S);

int use(S a, S b) { return operator⊞(a, b); }

// LINKED-DAG: call {{.*}} @_Zv28op_u229E1SS_
// LINKED-DAG: define {{.*}} @_Zv28op_u229E1SS_
// LINKED-NOT: declare {{.*}} @_Zv28op_u229E1SS_

// NM-USE: U _Zv28op_u229E1SS_

#elif defined(TU_DEF)

// Defines only.
int operator⊞(S a, S b) { return a.x + b.x; }

// NM-DEF: T _Zv28op_u229E1SS_

#else

namespace N {
struct A { int x; };
int operator⊞(A, A);
} // namespace N

// A same-named function in the enclosing scope, not viable for N::A. It exists
// so the ADL case below cannot be satisfied by ordinary lookup.
int operator⊞(S, S);
int operator⊞(T, T);
int operator⊖(S);

struct X {
  int operator⊞(X) const;
};

template <class U> int operator⊗(U, U);

// 1. Overload resolution over an explicit call emits the selected overload's
//    symbol, and the two overloads are distinct symbols.
// CHECK-LABEL: define {{.*}} @_Z6pick_s1SS_
// CHECK: call {{.*}} @_Zv28op_u229E1SS_
int pick_s(S a, S b) { return operator⊞(a, b); }

// CHECK-LABEL: define {{.*}} @_Z6pick_t1TS_
// CHECK: call {{.*}} @_Zv28op_u229E1TS_
int pick_t(T a, T b) { return operator⊞(a, b); }

// 2. ADL: the callee is the one in the operand's associated namespace, and the
//    symbol proves it -- `_ZN1N...` is nested in N, so nothing about the
//    enclosing-scope overloads was silently used instead.
// CHECK-LABEL: define {{.*}} @_Z3adlN1N1AE
// CHECK: call {{.*}} @_ZN1Nv28op_u229EENS_1AES0_
int adl(N::A a) { return operator⊞(a, a); }

// The prefix arity goes down the same path.
// CHECK-LABEL: define {{.*}} @_Z6prefix1S
// CHECK: call {{.*}} @_Zv18op_u22961S
int prefix(S a) { return operator⊖(a); }

// 3. A member call, for contrast with the non-member selection above.
// CHECK-LABEL: define {{.*}} @_Z3mem1X
// CHECK: call {{.*}} @_ZNK1Xv28op_u229EES_
int mem(X x) { return x.operator⊞(x); }

// 4. Template instantiation through an explicit call, direct and dependent:
//    two instantiations, two symbols.
// CHECK-LABEL: define {{.*}} @_Z4tmpli
// CHECK: call {{.*}} @_Zv28op_u2297IiEiT_S0_
int tmpl(int a) { return operator⊗(a, a); }

template <class U> int dep(U a) { return operator⊗(a, a); }

// CHECK-LABEL: define {{.*}} @_Z4instd
// CHECK: call {{.*}} @_Zv28op_u2297IdEiT_S0_
int inst(double d) { return dep(d); }

#endif
