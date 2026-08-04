// U17: ODR hashing of a Unicode user-defined operator across two modules.
//
// The two claims the step owes, in one file:
//
//   * two declarations of the *same* operator in two translation units of one
//     build must hash equal, so importing both merges them silently. If
//     ODRHash::AddDeclarationNameInfo hashed anything context-dependent
//     (a pointer, an interned spelling) this would report a spurious
//     violation on every user operator.
//   * two *different* operators must not collide, so a genuine difference is
//     still reported. The identity being hashed is the code point.
//
// Both the declaration name and a statement using the operator are covered:
// the first goes through ODRHash::AddDeclarationNameInfo, the second through
// StmtProfiler, which ODRHash reuses via Stmt::ProcessODRHash.

// RUN: rm -rf %t
// RUN: mkdir -p %t/cache %t/Inputs
// RUN: split-file %s %t/Inputs
//
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fmodules \
// RUN:   -fimplicit-module-maps -fmodules-cache-path=%t/cache -I%t/Inputs \
// RUN:   -fsyntax-only -verify %t/Inputs/main.cpp

//--- module.modulemap
module First {
  header "first.h"
  export *
}
module Second {
  header "second.h"
  export *
}

//--- shared.h
#pragma once
int operator⊞(int, int);
int operator⊗(int, int);
int operator⊞(int);

//--- first.h
#include "shared.h"

// Identical in both modules: same operator name, same body, same everything.
struct SameName {
  int v;
  int operator⊞(SameName) const;
};

struct SameBody {
  int v;
  int f(int a, int b) const { return a ⊞ b; }
};

// A prefix use, identical in both modules.
struct SamePrefixBody {
  int v;
  int f(int a) const { return ⊞a; }
};

// Same code point, different *fixity*: a prefix use against an infix one.
struct DifferentFixity {
  int v;
  int f(int a) const { return ⊞a; }
};

// Differs in the *name* of the member operator.
struct DifferentName {
  int v;
  int operator⊞(DifferentName) const;
};

// Differs in which operator the body uses.
struct DifferentBody {
  int v;
  int f(int a, int b) const { return a ⊞ b; }
};

//--- second.h
#include "shared.h"

struct SameName {
  int v;
  int operator⊞(SameName) const;
};

struct SameBody {
  int v;
  int f(int a, int b) const { return a ⊞ b; }
};

struct SamePrefixBody {
  int v;
  int f(int a) const { return ⊞a; }
};

struct DifferentFixity {
  int v;
  int f(int a) const { return a ⊞ a; }
};

struct DifferentName {
  int v;
  int operator⊗(DifferentName) const;
};

struct DifferentBody {
  int v;
  int f(int a, int b) const { return a ⊗ b; }
};

//--- main.cpp
#include "first.h"
#include "second.h"

// No diagnostic for these two: same code point, same hash, silent merge.
SameName sn;
SameBody sb;
SamePrefixBody spb;

// A different code point in the member operator's *name*: caught by
// ODRHash::AddDeclarationNameInfo, which hashes the code point.
DifferentName dn;
// expected-error@second.h:* {{'DifferentName::operator⊗' from module 'Second' is not present in definition of 'DifferentName' in module 'First'}}
// expected-note@first.h:* {{definition has no member 'operator⊗'}}

// A different operator inside a member's *body*: caught by the statement
// hash, which reaches StmtProfiler::VisitUserOperatorExpr through
// Stmt::ProcessODRHash.
DifferentBody db;
// expected-error@first.h:* {{'DifferentBody' has different definitions in different modules; first difference is definition in module 'First' found method 'f' with body}}
// expected-note@second.h:* {{but in 'Second' found method 'f' with different body}}

// The same code point used with a different *fixity* in the two bodies. The
// statement profile does not hash the arity directly -- it does not have to:
// the semantic forms are a one-argument and a two-argument call, so they
// differ in their children. Recorded rather than assumed, because it is the
// only place the arity's contribution to the ODR hash is observable.
DifferentFixity df;
// expected-error@first.h:* {{'DifferentFixity' has different definitions in different modules; first difference is definition in module 'First' found method 'f' with body}}
// expected-note@second.h:* {{but in 'Second' found method 'f' with different body}}
