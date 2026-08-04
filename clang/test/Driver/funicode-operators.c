// Test that -funicode-operators/-fno-unicode-operators are accepted and
// forwarded to -cc1, and that the flag composes with -fbacktick (U7).
//
// RUN: %clang -### -funicode-operators -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=ON
// RUN: %clang -### -fno-unicode-operators -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=OFF
// RUN: %clang -### -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=DEFAULT
// RUN: %clang -### -funicode-operators -fbacktick -x c++ -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=BOTH
//
// ON:      "-funicode-operators"
// ON-NOT:  "-fno-unicode-operators"
//
// OFF:     "-fno-unicode-operators"
// OFF-NOT: "-funicode-operators"
//
// DEFAULT-NOT: "-funicode-operators"
// DEFAULT-NOT: "-fno-unicode-operators"
//
// The two features are independent and composable: both flags reach cc1.
// BOTH-DAG: "-funicode-operators"
// BOTH-DAG: "-fbacktick"
