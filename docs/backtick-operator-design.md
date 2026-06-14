# Infix Backtick Operator — Design, Implementation Plan & Decisions Log

Working document for a WG21 proposal. Tracks the syntax/semantics, the
Clang / clang-format / GCC implementation plans, and a running log of
design decisions and their rationale (the part EWG/CWG will scrutinise).

---

## 1. Proposal in one line

`x `op` y` is sugar for `op(x, y)` (equivalently `(op)(x, y)`), where the
text between the backticks is an arbitrary call-eligible expression. The
value and semantics are exactly those of the corresponding call.

Examples:

```cpp
a `plus` b              // plus(a, b)
a `std::min` b          // std::min(a, b)
m `at` k                // at(m, k)
a `f` b `g` c           // g(f(a, b), c)        // left-associative
```

---

## 2. Settled semantics

- **Associativity:** left. `a `f` b `g` c` == `g(f(a,b), c)`.
- **Precedence:** highest-precedence *binary* operator — tighter than `*`,
  looser than unary/prefix. Both operands are cast-expressions, so prefix
  operators attach symmetrically: `-x `f` -y` == `f(-x, -y)`.
- **Operator slot (between backticks):** an arbitrary expression, parsed
  as an *assignment-expression* (no top-level comma). Nested backticks in
  the operator slot must be parenthesised (see D3).
- **Desugaring:** a plain call expression, built in Sema/the front end so
  overload resolution, ADL, templates, SFINAE, constexpr, and codegen are
  all inherited rather than reimplemented.

---

## 3. Decisions log

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| D1 | Left-associative | **Resolved** | `(x `op` y) `op` z`; matches reading order; fewest surprises for chaining. |
| D2 | Precedence vs. unary prefix | **Resolved — Option A (§4)** | Highest-precedence *binary* operator (looser than unary). `-x `f` -y` -> `f(-x, -y)` is symmetric, consistent with every other binary operator, and the most teachable; an implementor concurred. The rejected alternative (tighter than unary) made backtick the only operator floating a leading prefix out of its operand. |
| D3 | Nested backticks require parentheses | Proposed | Open and close are the same character, so bare nesting is ambiguous; `x `(f `g` h)` y` is well-formed, `x `f `g` h` y` is not. |
| D4 | Operator slot = assignment-expression | Proposed | Excludes a top-level comma operator in the operator slot; operands and the operator slot all read as call arguments would. |
| D5 | Gated behind a language flag | Proposed | Non-standard during proposal; keeps existing valid programs unchanged and makes the feature opt-in. |
| D6 | Desugar to a call expression for the MVP | **Resolved** | Inherits overload resolution / ADL / templates / constexpr / codegen with no new node. Enough for a working, testable compiler. |
| D7 | Source-fidelity AST wrapper deferred to phase 2 | **Resolved** | A thin transparent node (delegating type / value category / constexpr / codegen / instantiation to the wrapped call) is purely additive and lands after the MVP, once people are kicking the tires. Enables `-ast-print` to round-trip backtick syntax. Does not affect clang-format (token-based) or the GCC front end. |
| D8 | clang-format break policy | **Resolved** | Hard-forbid breaks adjacent to the backticks (after open, before close); allow breaks inside the operator slot but mildly disfavor them with a small split-penalty bump — slightly stickier than a normal expression, not a no-break zone. |
| D9 | No braced-init-list operands | **Resolved** | Operands are cast-expressions (already implied by D2's grammar), which excludes braced-init-lists; the slot is never a list (not callable). A brace operand `x `f` {1,2}` would mean `f(x, {1,2})` — meaningful as a call argument, *not* meaningless — but supporting it needs initializer-clause operand grammar, and a leading-brace LHS collides with block syntax. Excluded for the MVP; write the call directly. Revisitable. |
| D10 | Coexists with a backtick keyword-escape | **Resolved (mechanism); scope open (§10)** | Backtick stays one punctuator (no lexer identifier synthesis); the parser disambiguates by position — operand / primary / declarator-id position is a keyword-escaped identifier, post-operand position is the infix operator. Positions are mutually exclusive (same strategy as `*`, `&`, `<`). The escape yields a normal identifier, so lookup / mangling / linkage / ABI are unchanged. Details and examples in §12. |

Backtick is available because it has no current meaning in C++ source
outside string/character literals and raw-string delimiters, all of which
are lexed before the punctuator stage and are therefore unaffected.

---

## 4. Precedence (D2 — resolved: Option A)

Both options were left-associative and desugared to the same call; they
differed only in how a **prefix operator on an operand** binds. Option A
is adopted; Option B is recorded as the considered-and-rejected
alternative for the paper's design-rationale section.

### Option A — highest-precedence *binary* operator (ADOPTED)

Binds tighter than `*`, looser than unary/prefix. Operands are
cast-expressions, so both accept unary operands naturally.

```
backtick-expression:
    cast-expression
    backtick-expression ` operator-expression ` cast-expression

operator-expression:
    assignment-expression       // nested backticks parenthesised (D3)
```

Behaviour:

```cpp
-x `f` y     ->  f(-x, y)
-a `f` -b    ->  f(-a, -b)      // symmetric; matches every other binary op
a * b `f` c  ->  a * f(b, c)
```

Implementation: slots straight into `ParseRHSOfBinaryExpression`,
mirroring the ternary branch (the closest existing analog of a
delimited-middle operator). New top level in `prec::Level`.

### Option B — tighter than unary prefix (REJECTED)

```
unary-expression:
    backtick-expression
    unary-operator cast-expression
    ...

backtick-expression:
    postfix-expression
    backtick-expression ` operator-expression ` <unary-expression, backtick suppressed>
```

Behaviour:

```cpp
-x `f` y     ->  -f(x, y)       // the motivating case
-a `f` -b    ->  -f(a, -b)      // ASYMMETRIC: left '-' floats out, right '-' stays in
```

Implementation: hook `ParsePostfixExpressionSuffix` (must bind tighter
than the unary handling in `ParseCastExpression`); suppress the backtick
terminator flag while parsing the RHS operand so the RHS can still be
`-b`/`*p` while staying left-associative.

### Resolution

Option A adopted. Option B satisfied the "highest precedence" intuition in
isolation but made backtick the only operator where a leading prefix
operator floats out of its operand — `-a `f` -b` -> `-f(a, -b)` — which is
itself a surprise and harder to teach. Symmetry with every other binary
operator won out. The sole consequence of A is that `-x `f` y` is
`f(-x, y)`, which is the consistent reading anyway.

---

## 5. The same-delimiter parsing problem (applies to both compilers)

The open and close delimiter are the same token, so the operator slot's
expression parser would otherwise treat the closing backtick as the start
of *another* backtick operator. Resolve exactly as Clang/GCC already do
for `>` inside template-argument lists:

- **Clang:** model on `Parser::GreaterThanIsOperator` /
  `GreaterThanIsOperatorScope` and the `GreaterThanIsOperator` parameter
  of `getBinOpPrecedence`. Add a `BacktickIsOperator` flag: false while
  parsing the operator slot, restored to true inside nested parens/brackets
  so D3's parenthesised nesting works.
- **GCC:** model on `parser->greater_than_is_operator_p`; add
  `backtick_is_operator_p`.

---

## 6. Clang implementation plan

1. **Lexer.** Add `PUNCTUATOR(backtick, "`")` to
   `clang/include/clang/Basic/TokenKinds.def`; add `case '`':` in the
   punctuator switch in `Lexer::LexTokenInternal`
   (`clang/lib/Lex/Lexer.cpp`). Only mint the token when D5's flag is on.
2. **Precedence / level.** New top level in `prec::Level`
   (`OperatorPrecedence.h`); thread the `BacktickIsOperator` flag through
   `getBinOpPrecedence`.
3. **Parser** (`clang/lib/Parse/ParseExpr.cpp`). New `tok::backtick`
   branch in `ParseRHSOfBinaryExpression`, modeled on the `tok::question`
   ternary branch (the closest existing delimited-middle operator): on a
   sufficiently-high precedence backtick, consume open ``` ` ```; suppress
   the `BacktickIsOperator` flag; `ParseExpression` (assignment-expression)
   for the operator slot; expect close ``` ` ```; parse the RHS operand via
   `ParseCastExpression`; continue the left-associative loop.
4. **Sema** (`clang/lib/Sema/SemaExpr.cpp`). Small entry point forwarding
   to `BuildCallExpr` with callee = operator slot, args = {LHS, RHS}.
   Carry the backtick source locations for diagnostics and `-ast-print`.
5. **Gating** (D5). `LangOptions.def`, a driver flag in `Options.td`,
   wired in `CompilerInvocation.cpp`.
6. **Diagnostics.** Empty operator slot (` `` `), unterminated backtick,
   D3 ambiguity (bare nested backtick). Callee/arity/constexpr errors fall
   out of `BuildCallExpr`.
7. **Tests.** Lexer token kind; parser `-ast-dump` (shows the desugared
   call); precedence/associativity mixing with `*`, unary, `.`, `?:`; Sema
   (overloads, ADL, dependent operands); constexpr; CodeGen IR; a couple of
   preprocessor tests since backtick is now a real token in macro bodies.

---

## 7. clang-format plan

**Key conflict:** clang-format already lexes backtick for JavaScript
template literals. It reuses Clang's real lexer, so `tok::backtick` flows
in automatically once §6.1 lands — but the C++ handling must be guarded by
`Style.Language` / `LangOpts` so it never perturbs JS/TS template-string
logic, and the JS path must not misfire on C++.

Work items (`clang/lib/Format/`):

- **TokenAnnotator.cpp:** new `TT_` roles for the open/close backtick;
  recognise the matched pair in `annotate()`; set spacing in
  `spaceRequiredBefore` / `spaceRequiredBetween` (proposed canonical style:
  spaces outside the pair, hug the operator inside — `x `f` y`).
- **Break policy (D8):** hard-forbid a break after the open backtick and
  before the close backtick via `CanBreakBefore = false` in
  `canBreakBefore`. Inside the operator slot, breaks are allowed but mildly
  disfavored — a small additive bump to `SplitPenalty` on slot-interior
  tokens, so a long slot (e.g. a qualified name) is treated as an ordinary
  expression the formatter slightly prefers to keep intact, not a no-break
  zone.
- **Optional style option** (e.g. spacing-in-backtick-operators) — defer
  unless reviewers ask.
- **Tests** in `unittests/Format/`.

---

## 8. GCC implementation plan

Structurally parallel to Clang; decisions in §4–§6 port directly.

1. **libcpp tokeniser.** Add `CPP_BACKTICK` to the token-type list in
   `libcpp/include/cpplib.h`; add a case in `_cpp_lex_direct`
   (`libcpp/lex.cc`) replacing today's "stray '`' in program" diagnostic.
   Gate on the flag.
2. **C++ parser** (`gcc/cp/parser.cc`). Extend the precedence table used
   by `cp_parser_binary_expression` with a new highest binary level. Use
   `backtick_is_operator_p` (modeled on `greater_than_is_operator_p`) for
   the operator slot.
3. **Build the call** via `finish_call_expr`, inheriting overload
   resolution, ADL, template substitution (`pt.cc`), and constexpr
   (`constexpr.cc`).
4. **Gating** in `gcc/c-family/c.opt` plus the C++ lang hooks.
5. **Tests** in `gcc/testsuite/g++.dg/`.

GCC is the harder codebase (sparser docs, more idiosyncratic), but the
mapping above is 1:1 with the Clang plan, so settle the Clang design first
and port.

---

## 9. For the paper (Brazil)

- **Implementation experience** section carrying both Clang and GCC status
  — two independent implementations is strong evidence of implementability
  for EWG/CWG. A desugar-only Clang MVP (phase 1, §11) is sufficient on its
  own; the AST wrapper and a Compiler Explorer deployment strengthen the
  story but are not blocking for Brazil.
- Document **D2's resolution** and the rejected alternative (the
  `-a `f` -b` asymmetry) in §4 as design rationale — show EWG the choice was
  deliberate.
- Document **D3** (parenthesised nesting) and **D4** (operator slot grammar)
  as deliberate restrictions with rationale.
- Carry this decisions log forward; record each EWG/CWG poll outcome
  against its decision ID.

---

## 10. Open questions

- **Scope:** propose the infix operator and the backtick keyword-escape
  (§12) jointly, or as companion proposals sharing the lexical syntax? The
  escape is independently motivated (a future-keyword escape hatch; cf.
  Swift `` `class` ``, Rust `r#`).

---

## 11. Sequencing

- **Phase 1 — MVP (Clang).** Lexer, precedence level, parser hook, Sema
  desugar to a call (D6), gating flag, tests. Result: `x `op` y` compiles
  and runs identically to `op(x, y)`; `-ast-print` shows the desugared
  call. Enough for people to kick the tires and for the paper's
  implementation-experience section.
- **Phase 2 — source fidelity (Clang).** Thin transparent AST wrapper (D7)
  so `-ast-print` round-trips backtick syntax. Purely additive; lands once
  the MVP is stable.
- **Phase 3 — reach.** Compiler Explorer deployment once stable; GCC
  implementation in parallel for the second independent data point.

---

## 12. Coexistence with backtick keyword-escaped identifiers

A separate, independently-motivated use of backtick: escape a keyword so it
can name an entity — the hatch that lets a *future* keyword avoid breaking
existing code that used that word as an identifier. The two uses coexist;
this records why and how.

**Mechanism.** Backtick stays a single punctuator token (D6); neither use
synthesizes identifiers in the lexer. The parser disambiguates by
grammatical position, always one of two mutually-exclusive kinds:

- *Operand / primary-expression / declarator-id / after `.` `->` `::`* —
  the grammar wants a name or operand; a backtick opens a keyword-escaped
  identifier.
- *Post-operand* (`ParseRHSOfBinaryExpression`) — the grammar wants a
  binary operator; a backtick is the infix operator.

C++ expression parsing strictly alternates operand/operator, so the two
positions never coincide. Same position-based disambiguation C++ already
applies to `*`, `&`, and `<`.

```cpp
void `new`();        // declarator-id  -> escaped identifier "new"
`new`(a, b);         // primary        -> call to function "new"
obj.`delete`();      // after '.'      -> member named "delete"
x `f` y;             // post-operand   -> infix: f(x, y)
x `(`new`)` y;       // escaped callee -> new(x, y)   (uses D3 parens)
```

**Reinforcement.** Inner content also differs — the escape wraps a single
keyword (not a valid callee expression), the infix slot wraps an
expression. Optionally restrict the escape to *actual keywords* for maximal
disjointness; position alone suffices without it.

**ABI.** The escape yields an ordinary identifier token whose spelling is
the keyword, so lookup, mangling, and linkage treat it as a normal
identifier — `void `new`();` links as a function named `new`. Purely
source-level; external names and ABI unchanged.

**Costs.** Bounded added context-sensitivity: tentative
declaration-vs-expression parsing (`TryParseDeclarator` and friends) must
recognize escapes, and clang-format / tooling must distinguish the two
uses. The nested keyword-named-callee case reuses D3's parenthesisation, so
needs no new rule.

**Prior art (escape hatch):** Swift `` `class` ``, Kotlin backtick
identifiers, F# double-backtick names, Rust `r#` raw identifiers.
