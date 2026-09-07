// Completion-priority grouping for a member `operator⊞`.
//
// `getDeclPriority` in SemaCodeComplete.cpp demotes explicit operator and
// conversion-function calls on a class member to CCP_Unlikely (80) instead of
// CCP_MemberDeclaration (35), because spelling one out is rare next to
// writing the syntax it names. A user-defined operator is in exactly that
// position -- `v.operator⊞(w)` is the long way round for `v ⊞ w` -- so it
// belongs in the group with `operator+` and not in the group with a data
// member.
//
// This is checked with c-index-test rather than with `-code-completion-at`
// because priority is not observable through the latter: the printing
// consumer never emits it, and the `std::stable_sort` it runs first compares
// *names*, not priorities. c-index-test prints the priority in parentheses,
// so this asserts the number directly rather than an ordering that stands in
// for it.
//
// Line- and column-sensitive; the RUN lines are at the bottom.

struct Vec {
  int member;
  int mem_zzz;

  // The control: an existing operator name, already demoted, and it must
  // stay demoted.
  int operator+(Vec) const;

  // The two user-operator arities.
  int operator⊞(Vec) const;
  int operator⊖() const;
};

void member_access(Vec v) {
  v.
}

// RUN: c-index-test -code-completion-at=%s:34:5 %s -std=c++23 -funicode-operators \
// RUN:   | FileCheck %s

// Data members keep CCP_MemberDeclaration, which is what makes the demotion
// below a grouping and not a blanket change.
// CHECK: FieldDecl:{ResultType int}{TypedText mem_zzz} (35)
// CHECK: FieldDecl:{ResultType int}{TypedText member} (35)

// CHECK: CXXMethod:{ResultType int}{TypedText operator+}{LeftParen (}{Placeholder Vec}{RightParen )}{Informative  const} (80)

// The two that the fix moves. Before it they printed (35), grouped with the
// data members above; reverting the CXXUserOperatorName clause in
// getDeclPriority puts them back there and fails these two lines.
// CHECK: CXXMethod:{ResultType int}{TypedText operator⊖}{LeftParen (}{RightParen )}{Informative  const} (80)
// CHECK: CXXMethod:{ResultType int}{TypedText operator⊞}{LeftParen (}{Placeholder Vec}{RightParen )}{Informative  const} (80)
