// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify %s

// U08: which `operator⊞` declarations are well-formed.
//
// Two rules, and only two:
//   * arity selects the form (U5) -- one operand prefix, two operands infix,
//     counting a member's implicit object parameter;
//   * there is NO "at least one class or enum parameter" requirement (U2).
//
// Everything else -- constexpr, consteval, templates, = delete, variadics,
// default arguments -- is whatever the ordinary function rules allow.
//
// The second half of this file is the control: the existing operators must
// behave exactly as they did before, which is what proves the U2 relaxation
// was scoped to the new name kind and not to a shared checker.

//===--------------------------------------------------------------------===//
// Accepted
//===--------------------------------------------------------------------===//

// U2's motivating case: fundamental types only, no class or enum in sight.
// [over.oper]p7 would reject the analogous `int operator+(int, int)` (see
// below); a user operator has no built-in meaning to protect.
constexpr int operator⊞(int a, int b) { return a + b; }
static_assert(operator⊞(5, 7) == 12);

struct Vec {};

// Free prefix (one parameter) and free infix (two).
Vec operator⊖(Vec const &);
Vec operator⊞(Vec const &, Vec const &);

struct S {
  int operator⊕(S) const;   // member infix:  1 declared + implicit object = 2
  int operator⊘() const;    // member prefix: 0 declared + implicit object = 1

  // An explicit object parameter is a declared parameter, so the counts move
  // together: two declared parameters are still the infix form.
  int operator⊙(this S, S); // member infix
  int operator⊚(this S);    // member prefix

  friend Vec operator⊠(S, S);
};

// Templates, and a specialization of one.
template <class T> T operator⊛(T a, T) { return a; }
template <> int operator⊛(int, int b) { return b; }
template <class T> T operator⊛(T a) { return a; }

// constexpr / consteval / deleted / defaulted-style spellings all follow the
// ordinary function rules; only arity is special.
consteval int operator⊜(int a) { return -a; }
static_assert(operator⊜(3) == -3);
void operator⊜(Vec) = delete;

// Variadic and defaulted parameters are *not* rejected here. The existing
// operators forbid both; that prohibition is part of [over.oper] and it
// protects an operator whose parse is fixed by the grammar. A user operator
// gets whatever an ordinary function gets (U§7 "Declaring").
int operator⊝(int, ...);
int operator⊟(int a, int b = 1);

// There is no postfix form and no way to spell one (U5): a trailing `int`
// parameter is an ordinary second operand here, not a postfix marker, so this
// is simply an infix operator and nothing is diagnosed.
int operator⊟(Vec, int);

//===--------------------------------------------------------------------===//
// Rejected -- arity, and only arity
//===--------------------------------------------------------------------===//

int operator⊡(); // expected-error {{user-defined operator 'operator⊡' must have one parameter (prefix) or two parameters (infix) (has 0 parameters)}}

int operator⋄(int, int, int); // expected-error {{user-defined operator 'operator⋄' must have one parameter (prefix) or two parameters (infix) (has 3 parameters)}}

struct T {
  // The free infix form written as a member: two declared parameters plus the
  // implicit object parameter is three operands.
  int operator⊗(T, T); // expected-error {{user-defined operator 'operator⊗' must have no parameters (prefix) or one parameter (infix) (has 2 parameters)}}

  // A static member function has no implicit object parameter, so it can name
  // neither form.
  static int operator⊢(T, T); // expected-error {{overloaded 'operator⊢' cannot be a static member function}}
};

// Out-of-line, and in a template: same rule, same message.
namespace N {
int operator⊡(int, int, int, int); // expected-error {{user-defined operator 'operator⊡' must have one parameter (prefix) or two parameters (infix) (has 4 parameters)}}
}

template <class U> U operator⋄(); // expected-error {{user-defined operator 'operator⋄' must have one parameter (prefix) or two parameters (infix) (has 0 parameters)}}

//===--------------------------------------------------------------------===//
// Control: no existing operator's rules moved
//===--------------------------------------------------------------------===//

// The check U2 waives for user operators is still enforced for the built-in
// operator tokens. If this ever stops being an error, the relaxation leaked
// out of the new name kind and into the shared checker.
int operator+(int, int); // expected-error {{overloaded 'operator+' must have at least one parameter of class or enumeration type}}

struct P {};
P operator*(P, P, P);      // expected-error {{overloaded 'operator*' must be a unary or binary operator (has 3 parameters)}}
P operator&(P, P, ...);    // expected-error {{overloaded 'operator&' cannot be variadic}}
P operator-(P a, P b = P()); // expected-error {{parameter of overloaded 'operator-' cannot have a default argument}}
struct Q {
  static P operator%(Q, Q); // expected-error {{overloaded 'operator%' cannot be a static member function}}
};
