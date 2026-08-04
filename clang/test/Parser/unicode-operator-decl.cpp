// RUN: %clang_cc1 -std=c++20 -funicode-operators -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -funicode-operators -fbacktick -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -funicode-operators -ast-dump %s | FileCheck %s
// RUN: %clang_cc1 -std=c++20 -fsyntax-only -verify=off -DNO_UNICODE_OPERATORS %s

// U07: `operator⊞` parses as an operator-function-id wherever one may appear,
// and yields the U06 DeclarationName (CXXUserOperatorName), whose identity is
// the Unicode scalar value.
//
// NOT this step: arity rules, the class-or-enum relaxation, mangling, infix
// use (U08/U09/U11). A declaration that is semantically wrong should still
// parse. Everything here is -fsyntax-only / -ast-dump on purpose: a codegen or
// module test would reach U09's and U17's deliberate llvm_unreachable holes.

#ifdef NO_UNICODE_OPERATORS

// Without -funicode-operators the code point is not a token and the diagnostic
// is upstream's, unchanged: the identifier lexer's recovery path absorbs the
// character into the identifier `operator⊞` (U03's "adjacent" form).
int operator⊞(int, int); // off-error {{character '⊞' U+229E not allowed in an identifier}}

#else

// expected-no-diagnostics

//--- Free function: declaration, then definition. One entity. ---------------
constexpr int operator⊞(int, int);
constexpr int operator⊞(int a, int b) { return a + b; }
// This constant-evaluates only if the call resolves to the *defined*
// declaration, i.e. only if the two occurrences produced the same
// DeclarationName and were merged into one redeclaration chain.
// U04 has not landed, so the same declaration cannot yet be written
// `operator\N{SQUARED PLUS}`. Once it has, add exactly that: declare with a
// UCN spelling, define with the glyph, and this static_assert must still hold.
static_assert(operator⊞(2, 3) == 5);

// CHECK: FunctionDecl {{.*}} operator⊞ 'int (int, int)'

// The name's source range is the 'operator' keyword through the operator
// token: on the line above, `static_assert(` is 14 characters, so the keyword
// is at col 15 and ⊞ at col 23.
// CHECK: DeclRefExpr {{.*}} <col:15, col:23> {{.*}} 'operator⊞' 'int (int, int)'

//--- Explicit call and address-of -------------------------------------------
int (*pfn)(int, int) = &operator⊞;
int use() { return operator⊞(1, 2); }

//--- Members, friend, = delete ----------------------------------------------
struct S {
  int operator⊞(S) const;
  int operator⊗() const;               // prefix form (U5)
  S operator⊟(S) const = delete;
  friend int operator⊕(S, S);
};
int S::operator⊞(S) const { return 0; }

// CHECK: CXXMethodDecl {{.*}} operator⊞ 'int (S) const'

//--- Qualified declarator ---------------------------------------------------
namespace N {
int operator⊞(S, S);
}
int N::operator⊞(S, S) { return 1; }

//--- Template, explicit specialization, explicit instantiation --------------
template <class T> T operator⊠(T a, T) { return a; }
template <> int operator⊠(int, int b) { return b; }
template double operator⊠(double, double);
int tmpl() { return operator⊠<char>('a', 'b'); }

// CHECK: FunctionTemplateDecl {{.*}} operator⊠

//--- using-declaration ------------------------------------------------------
namespace M {
int operator⊞(S, int);
}
using M::operator⊞;

//--- Tentative parsing ------------------------------------------------------
// A statement starting with a type-name is ambiguous between a declaration and
// an expression, so the parser tentatively parses the declarator. That path
// (TryParseOperatorId) needed its own peer case for tok::user_operator: without
// it these are Errors and get re-parsed as expressions.
void tentative() {
  S operator⊛(S, S);
  S (operator⊝)(S, S);
}

// CHECK: FunctionDecl {{.*}} operator⊛ 'S (S, S)'
// CHECK: FunctionDecl {{.*}} operator⊝ 'S (S, S)'

//--- constexpr, noexcept, deleted at namespace scope ------------------------
constexpr int operator⊘(int a) noexcept { return -a; }
static_assert(operator⊘(3) == -3);
void operator⊚(S) = delete;

#endif
