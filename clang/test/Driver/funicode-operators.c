// Test that -funicode-operators/-fno-unicode-operators are accepted and
// forwarded to -cc1.
//
// RUN: %clang -### -funicode-operators -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=ON
// RUN: %clang -### -fno-unicode-operators -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=OFF
// RUN: %clang -### -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
//
// ON:      "-funicode-operators"
// ON-NOT:  "-fno-unicode-operators"
//
// OFF:     "-fno-unicode-operators"
// OFF-NOT: "-funicode-operators"
//
// DEFAULT-NOT: "-funicode-operators"
// DEFAULT-NOT: "-fno-unicode-operators"
