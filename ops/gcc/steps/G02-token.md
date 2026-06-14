# G02 — libcpp `CPP_BACKTICK` token

**Goal.** Lex `` ` `` as `CPP_BACKTICK` when enabled; preserve today's
"stray '`'" behavior when off; other front ends unaffected.

**Depends on:** G01.
**Design refs:** §8 step 1; §3 D6/D10 (single token, no identifier synthesis
in the lexer).

## Do
1. Add `CPP_BACKTICK` to the token enum: the `TTYPE_TABLE` list in
   `libcpp/include/cpplib.h` (an `OP(CPP_BACKTICK, "`")` entry).
2. Gate it so libcpp stays language-agnostic: add a field to
   `struct cpp_options` (e.g. `backtick`), set from `flag_backtick` where the
   C++ front end initializes libcpp options (mirror the `dollars_in_ident`
   gating pattern).
3. In `_cpp_lex_direct` (`libcpp/lex.cc`), add `case '`':` setting
   `result->type = CPP_BACKTICK` **only when** the option is on; else fall
   through to today's path (the stray-character diagnostic in the front end).

## Build / Verify (gate)
- A `g++.dg` test: with `-fbacktick`, a bare `` ` `` no longer triggers the
  stray error in an accepting context (fully exercised once G03 lands). Without
  the flag, the stray-character error is unchanged.
- A C test confirms the C front end is unchanged (option off there).
- Baseline green.

## Done when
The token exists under the option; C and off-flag C++ behavior are unchanged.

## Capture in handoff
The `cpp_options` field name and where it's set from `flag_backtick`. Record
the exact current stray-character diagnostic text so G04 stays consistent.

## Pitfalls
libcpp is shared by all front ends — never lex `CPP_BACKTICK` unconditionally.
