// escape-module-names (docs/backtick-operator-design.md): the dependency
// scanner finds a module directive whose first component, or whose imported
// partition, is escaped, and reports the name the bare spelling would have.
// The scanner lexes without -fbacktick; the preprocessor that replays the
// directive has it.
//
// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: clang-scan-deps -format=p1689 -- %clang++ -std=c++20 -fbacktick \
// RUN:   -c %t/util.cppm -o %t/util.o | FileCheck %s --check-prefix=UTIL
// RUN: clang-scan-deps -format=p1689 -- %clang++ -std=c++20 -fbacktick \
// RUN:   -c %t/part.cppm -o %t/part.o | FileCheck %s --check-prefix=PART
// RUN: clang-scan-deps -format=p1689 -- %clang++ -std=c++20 -fbacktick \
// RUN:   -c %t/new.cppm -o %t/new.o | FileCheck %s --check-prefix=NEW
// RUN: clang-scan-deps -format=p1689 -- %clang++ -std=c++20 -fbacktick \
// RUN:   -c %t/use.cpp -o %t/use.o | FileCheck %s --check-prefix=USE

//--- util.cppm
export module acme.`class`.util;
// UTIL: "logical-name": "acme.class.util"

//--- part.cppm
export module `new`:`and`;
// PART: "logical-name": "new:and"

//--- new.cppm
module;
export module `new`;
export import :`and`;
// NEW:      "provides": [
// NEW:        "logical-name": "new"
// NEW:      "requires": [
// NEW-NEXT:   {
// NEW-NEXT:     "logical-name": "new:and"

//--- use.cpp
import acme.`class`.util;
import `new`;
// USE:      "requires": [
// USE-NEXT:   {
// USE-NEXT:     "logical-name": "acme.class.util"
// USE:          "logical-name": "new"
