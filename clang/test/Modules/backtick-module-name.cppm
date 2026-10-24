// escape-module-names (docs/backtick-operator-design.md): under -fbacktick
// the keyword escape reaches each component of a module-name and a
// module-partition, where the module is declared and where it is imported.
// `class` is the identifier class, so the module is acme.class.util -- its
// name, its BMI and its mangling are exactly those of the bare spelling,
// which no program could write.
//
// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++20 -fbacktick -triple x86_64-linux-gnu \
// RUN:   -emit-module-interface %t/util.cppm -o %t/acme.class.util.pcm
// RUN: %clang_cc1 -std=c++20 -fbacktick -triple x86_64-linux-gnu \
// RUN:   -emit-module-interface %t/part.cppm -o %t/new-and.pcm
// RUN: %clang_cc1 -std=c++20 -fbacktick -triple x86_64-linux-gnu \
// RUN:   -emit-module-interface %t/new.cppm -fmodule-file=new:and=%t/new-and.pcm \
// RUN:   -o %t/new.pcm
// RUN: %clang_cc1 -std=c++20 -fbacktick -triple x86_64-linux-gnu \
// RUN:   -fmodule-file=acme.class.util=%t/acme.class.util.pcm \
// RUN:   -fmodule-file=new=%t/new.pcm -fmodule-file=new:and=%t/new-and.pcm \
// RUN:   -emit-llvm %t/use.cpp -o - | FileCheck %s
// RUN: %clang_cc1 -std=c++20 -fbacktick -triple x86_64-linux-gnu \
// RUN:   -fsyntax-only -verify %t/errors.cppm

//--- util.cppm
export module acme.`class`.util;
export int f() { return 1; }

//--- part.cppm
// An escaped first component and partition; an alternative token is a word
// like any other, so `and` is the identifier and.
export module `new`:`and`;
export int g() { return 2; }

//--- new.cppm
export module `new`;
export import :`and`;

//--- use.cpp
// The escape may sit on any component, and the two spellings may be mixed.
import acme.`class`.util;
import acme.`class`.`util`;
import `new`;
int h() { return f() + g(); }

// CHECK: call {{.*}} @_ZW4acmeW5classW4util1fv()
// CHECK: call {{.*}} @_ZW3new1gv()

//--- errors.cppm
#define util 3
export module acme.`util`.`class; // expected-error {{module name component 'util' cannot be a object-like macro}} \
                                  // expected-note@-1 {{macro 'util' defined here}} \
                                  // expected-error {{missing closing backtick for keyword escape in module name}} \
                                  // expected-note {{to match this '`'}}
