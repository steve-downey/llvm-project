// Test that backtick is lexed as tok::backtick only when -fbacktick is on,
// and as tok::unknown otherwise.

// RUN: %clang_cc1 -x c++ -fbacktick -dump-tokens %s 2>&1 | FileCheck %s --check-prefix=WITH
// RUN: %clang_cc1 -x c++ -dump-tokens %s 2>&1 | FileCheck %s --check-prefix=WITHOUT

// WITH:      backtick '`'
// WITHOUT:   unknown '`'

int a = 1 ` 2;

// Backtick inside a string literal must remain part of the string token,
// not produce a separate tok::backtick.
//
// WITH: string_literal '\"hello ` world\"'
// WITHOUT: string_literal '\"hello ` world\"'
const char *s = "hello ` world";

// Backtick inside a raw-string literal must remain part of the raw-string
// token, not produce a separate tok::backtick.
//
// WITH: string_literal 'R\"(raw ` string)\"'
// WITHOUT: string_literal 'R\"(raw ` string)\"'
const char *r = R"(raw ` string)";

// Backtick inside a line comment produces no token at all; -dump-tokens
// skips comments, so no explicit check is needed, but exercise the path:
// `backtick in a comment` — no token expected.
