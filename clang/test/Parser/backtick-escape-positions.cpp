// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 | FileCheck %s
// RUN: %clang_cc1 -std=c++20 -fbacktick -ast-print %s 2>&1 \
// RUN:   | %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -x c++ -
// expected-no-diagnostics

// The keyword escape in every name position [lex.name] admits, not only the
// declarator-ids the first implementation happened to reach: an
// escaped-identifier may appear wherever the grammar uses identifier as a
// terminal. Each position below is *declared and then used*, because
// accepting the declaration is only half of it -- a type nothing can name is
// not an escape hatch. See docs/backtick-operator-design.md section 12.
//
// In every one of these positions a backtick was previously always an error,
// with or without the flag, so nothing here can have changed the meaning of a
// well-formed program. backtick-c-mode.c and the GCC suite's escape-diag.C
// pin the other half of that: a program containing no backtick diagnoses
// identically with the flag on and off.

// --- class-head-name, and the type it declares ------------------------------
// CHECK: struct `union` {
struct `union` { int x; };
// CHECK: `union` u0;
`union` u0;
int read_u0() { return u0.x; }

// --- constructor name, base-specifier, mem-initializer ----------------------
// CHECK: struct `class` {
struct `class` { `class`(int); };
// CHECK: struct Derived : `class` {
// CHECK: Derived() : `class`(0) {
struct Derived : `class` { Derived() : `class`(0) { } };

// --- enum-name and enumerators, scoped and unscoped -------------------------
// CHECK: enum `namespace` {
// CHECK-NEXT: `new`
// CHECK-NEXT: `delete`
enum `namespace` { `new`, `delete` };
// CHECK: `namespace` e0 = `new`;
`namespace` e0 = `new`;
// CHECK: enum class `struct`
enum class `struct` { `try` };
// CHECK: `struct` e1 = `struct`::`try`;
`struct` e1 = `struct`::`try`;

// --- namespace-name, qualified use, using-directive -------------------------
// CHECK: namespace `template` {
namespace `template` { int v; }
// CHECK: return `template`::v;
int read_v() { return `template`::v; }
// CHECK: using namespace `template`;
using namespace `template`;
int read_v_again() { return v; }

// --- template parameter names, type and template ----------------------------
// CHECK: template <class `typename`> struct Box {
// CHECK-NEXT: `typename` m;
template<class `typename`> struct Box { `typename` m; };
// CHECK: template <template <class> class `typedef`> struct Meta {
template<template<class> class `typedef`> struct Meta { `typedef`<int> b; };
Box<int> b0;
Meta<Box> m0;

// --- alias-declaration, alias-template, concept name ------------------------
// CHECK: using `enum` = int;
using `enum` = int;
// CHECK: `enum` a0 = 0;
`enum` a0 = 0;
// CHECK: template <class T> using `friend` = T;
template<class T> using `friend` = T;
// CHECK: `friend`<int> a1 = 0;
`friend`<int> a1 = 0;
template<class T> concept `explicit` = true;
template<`explicit` T> void constrained(T) { }

// --- a keyword-escaped name as a nested-name-specifier component -------------
// The last component of a qualified name is an unqualified-id and was always
// reachable. A component in the *middle* is read by the scope-specifier loop,
// which is a different parser and a place the parser speculates.
namespace `template` { namespace `do` { int w; } }
// CHECK: int deep = `template`::`do`::w;
int deep = `template`::`do`::w;

// --- label, and the goto that reaches it ------------------------------------
void labelled() {
  int n = 0;
  // CHECK: `goto`:
  `goto`:
  // CHECK: goto `goto`;
  if (n++ < 1)
    goto `goto`;
}
