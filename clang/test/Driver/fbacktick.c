// Test that -fbacktick/-fno-backtick are accepted and forwarded to -cc1.
//
// RUN: %clang -### -fbacktick -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=ON
// RUN: %clang -### -fno-backtick -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=OFF
// RUN: %clang -### -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
//
// ON:      "-fbacktick"
// ON-NOT:  "-fno-backtick"
//
// OFF:     "-fno-backtick"
// OFF-NOT: "-fbacktick"
//
// DEFAULT-NOT: "-fbacktick"
// DEFAULT-NOT: "-fno-backtick"
