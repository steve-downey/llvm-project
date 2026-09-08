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

// --- a keyword-escaped name as the *final* component of a qualified type ----
// The final component of a qualified name is read as an unqualified-id only
// when it is an object or a function. When a type is wanted, the name is read
// by the decl-specifier and typename-specifier paths instead, and those need
// their own arm -- which is why N::`new` worked from the start and
// N::`union` did not (design doc section 12).
namespace `switch` {
// CHECK: struct `union` {
struct `union` { int a; struct S { int b; }; };
// CHECK: struct `while` : `union` {
struct `while` : `union` { };
int `new` = 1;
}
// CHECK: `switch`::`union` q0;
`switch`::`union` q0;
// CHECK: struct `switch`::`union` q1;
struct `switch`::`union` q1;
// CHECK: using QA = `switch`::`union`;
using QA = `switch`::`union`;
// CHECK: `switch`::`union`::S q2;
`switch`::`union`::S q2;
// CHECK: void qparam(`switch`::`union`);
void qparam(`switch`::`union`);
// CHECK: `switch`::`union` qret();
`switch`::`union` qret();
// CHECK: template <class T> struct QW {
template<class T> struct QW { };
// CHECK: QW<`switch`::`union`> q3;
QW<`switch`::`union`> q3;
// CHECK: struct QD : `switch`::`union` {
// CHECK: QD() : `switch`::`union`() {
struct QD : `switch`::`union` { QD() : `switch`::`union`() { } };
int qblock() {
  // CHECK: `switch`::`union` q4;
  `switch`::`union` q4;
  return sizeof(`switch`::`union`) + static_cast<`switch`::`union`>(q4).a +
         `switch`::`new`;
}
// A dependent qualified name after 'typename' reads its final component in a
// third place again.
// CHECK: template <class T> struct QT {
// CHECK-NEXT: typename T::`union` m;
template<class T> struct QT { typename T::`union` m; };
QT<`switch`::`union`::S> *qt0;

// --- an escaped class name, defined out of line, including its constructor --
// CHECK: struct `static` {
struct `static` { `static`(); void `new`(); };
// CHECK: `static`::`static`() {
`static`::`static`() { }
// CHECK: void `static`::`new`() {
void `static`::`new`() { }

// --- an escape whose keyword is a *type* keyword -----------------------------
// Clang's keywords carry no binding, so these were never in doubt here; GCC
// binds `int' and its siblings at global scope for the benefit of code that
// looks builtin types up by name, and had to be taught that an escaped
// declaration is not redeclaring one. Pinned in both suites: the keyword still
// names the builtin in the same translation unit, and the escaped name is an
// ordinary identifier that mangles as itself.
// CHECK: int `int` = 0;
int `int` = 0;
// CHECK: void `long`() {
void `long`() { }
// CHECK: using `char` = double;
using `char` = double;
// CHECK: struct `bool` {
struct `bool` { int a; };
// CHECK: template <class T> using `float` = T;
template<class T> using `float` = T;
// CHECK: enum `short` {
enum `short` { SA };
// CHECK: namespace `void` {
namespace `void` { int x; }
int type_keywords() {
  int builtin = 1;              // the keyword still means the type
  long builtin2 = 2;
  // CHECK: `bool` v{3};
  `bool` v{3};
  // CHECK: `char` d = 1.5;
  `char` d = 1.5;
  // CHECK: `float`<int> f = 4;
  `float`<int> f = 4;
  // CHECK: `short` e = SA;
  `short` e = SA;
  `long`();
  return `int` + builtin + (int)builtin2 + v.a + (int)d + f + (int)e +
         `void`::x;
}
