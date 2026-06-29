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
| D3 | Nesting requires parentheses; the bare form is a chain | **Resolved — reframed (§17.1)** | The slot's open/close are the same token, so the first interior backtick closes it: the slot can never hold a bare backtick, and "bare nesting" is *token-identical* to a D1 left-assoc chain (`x `f `g` h` y` == `h(f(x,g),y)`). It therefore cannot be diagnosed without contradicting D1. To nest, parenthesize — `x `(f `g` h)` y` == `(g(f,h))(x,y)`; without parens you get a chain — ordinary operator grouping, the same answer-changing-but-undiagnosed regroup as non-associative binary minus (`a-b-c` ≠ `a-(b-c)`). The original "produces a parse error" wording was impossible; this reclassifies DEV-04 / DEV-G04 from deferred-enforcement to no-enforcement-needed. |
| D4 | Operator slot = assignment-expression | **Resolved** | Excludes a top-level comma operator in the operator slot; operands and the operator slot all read as call arguments would. |
| D5 | Gated behind a language flag | **Resolved** | Non-standard during proposal; keeps existing valid programs unchanged and makes the feature opt-in. (A standardized form drops the gate; the flag is the prototype vehicle.) |
| D6 | Desugar to a call expression for the MVP | **Resolved** | Inherits overload resolution / ADL / templates / constexpr / codegen with no new node. Enough for a working, testable compiler. |
| D7 | Source-fidelity AST wrapper deferred to phase 2 | **Resolved** | A thin transparent node (delegating type / value category / constexpr / codegen / instantiation to the wrapped call) is purely additive and lands after the MVP, once people are kicking the tires. Enables `-ast-print` to round-trip backtick syntax. Does not affect clang-format (token-based) or the GCC front end. |
| D8 | clang-format break policy | **Resolved** | Hard-forbid breaks adjacent to the backticks (after open, before close); allow breaks inside the operator slot but mildly disfavor them with a small split-penalty bump — slightly stickier than a normal expression, not a no-break zone. |
| D9 | No braced-init-list operands | **Resolved** | Operands are cast-expressions (already implied by D2's grammar), which excludes braced-init-lists; the slot is never a list (not callable). A brace operand `x `f` {1,2}` would mean `f(x, {1,2})` — meaningful as a call argument, *not* meaningless — but supporting it needs initializer-clause operand grammar, and a leading-brace LHS collides with block syntax. Excluded for the MVP; write the call directly. Revisitable. |
| D10 | Coexists with a backtick keyword-escape | **Resolved (mechanism); scope open (§10)** | Backtick stays one punctuator (no lexer identifier synthesis); the parser disambiguates by position — operand / primary / declarator-id position is a keyword-escaped identifier, post-operand position is the infix operator. Positions are mutually exclusive (same strategy as `*`, `&`, `<`). The escape yields a normal identifier, so lookup / mangling / linkage / ABI are unchanged. Details and examples in §12. |
| D11 | Backtick is the sole spelling; no alternative/digraph spelling | **Proposed — disfavored alternatives recorded (§13)** | Markup friction (Markdown inline code) and keyboard ergonomics are real but minor: CommonMark's multi-backtick span already makes inline prose expressible and fenced blocks cover code samples (capability, not just ergonomics, is already there). A second spelling doubles teaching / clang-format / `-ast-print` / tooling surface, fragments the idiom, and swims against the trigraph-removed (C++17) / digraph-vestigial trend. If EWG ever forces one, an asymmetric self-delimiting pair (`\< … \>`) is the front-runner because it would *also* retire §5 and D3 — but that is a different operator, not a backtick alias. Full analysis and rebuttals in §13. |
| D12 | Orthogonal to P2011 `\|>` (pipeline-rewrite, "pizza"); does not replace it | **Resolved** | Both bottom out in a call and the 2-arg case overlaps, but backtick is *symmetric binary infix* desugaring to an ordinary overload-resolved call, while `\|>` is a *non-overloadable syntactic rewrite* prepending the left operand to an arbitrary-arity call. Different shape, arity, precedence, mechanism, and idiom; they compose rather than compete. Backtick also deliberately declines the `\|>` spelling (§13.3 / §14.3) so both can coexist in one program. Full analysis in §15. |
| D13 | Scope: pure core-language proposal; no standard-library additions | **Resolved** | Standardizing pipeline/composition helpers (`pipe`, `then`, `mbind`, …) would route the paper through LEWG as well as EWG/CWG — two tracks, the time-and-motion cost of D11 rebuttal 7 doubled. The operator needs no library to function; the §16 helpers are each a few lines of ordinary user code. Keep this paper language-only (EWG/CWG), target C++29, and defer any standard helpers to a companion library paper once usage experience shows which earn it. §16 carries them as *motivation*, not proposal. |
| D14 | Both backtick usages (infix operator + keyword-escape) proposed jointly, in one paper | **Resolved** | Same lexical token (D10), same committee (EWG/CWG). Joint proposal *conserves EWG attention* — one "what does backtick mean" discussion, not two — and prevents the two uses being designed into *contradiction* if pursued independently (punctuator vs. lexer-synthesized identifier; divergent disambiguation). Consistent with D13, not contrary to it: the rule is **bundle what shares a design surface within one committee; split what is separable across committees** — so the two language uses bundle, the library layer (D13) splits off to LEWG. Resolves the §10 scope question. |
| D15 | Evaluation order is the call's; operand order unspecified | **Resolved (§17.2)** | `x `f` y` is defined as `f(x, y)` and adds *no* evaluation-order rule: operand order is **unspecified** (the same [expr.call] situation that defeated past LTR/RTL proposals), and since C++17 the callee/slot is sequenced *before* both operands. Source order `(x, slot, y)` is therefore not the evaluation order `(slot, then {x, y})`. Falls out of "it is just the call." |
| D16 | A type-name in the slot yields construction | **Resolved (§17.3)** | The slot is any callable expression and a type-name is callable, so `x `T` y` == `T(x, y)` (functional-style construction; CTAD applies). Always an *expression* (slot is an assignment-expression, D4; result is an expression by construction), so no most-vexing-parse declaration reading can arise, and no collision with the §12 escape (different grammatical position). Blessed as a consequence, not a special rule. |

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
   to `BuildCallExpr` with callee = operator slot, args = {LHS, RHS}. The two
   backtick token locations need no dedicated fields — pass them as the call's
   open/close paren locations (the inner `CallExpr`'s LParen/RParen), which is
   enough for source ranges and lets the phase-2 wrapper's pretty-printer
   reconstruct the syntax from structure (LHS, callee, RHS) rather than from
   stored locations (DEV-05). Dedicated backtick-location storage is needed
   only if a diagnostic must point at an individual backtick token.
5. **Gating** (D5). A `LANGOPT` in `LangOptions.def` — use the current 5-arg
   form `LANGOPT(Name, Bits, Default, Compatibility, Description)`, e.g.
   `LANGOPT(Backtick, 1, 0, NotCompatible, "backtick operator")`; the old
   4-arg form no longer compiles (DEV-02). The driver/`-cc1` flag lives in
   `clang/include/clang/Options/Options.td` — the file moved there from
   `.../Driver/Options.td` (DEV-01). Add marshalling in `CompilerInvocation.cpp`, but
   note marshalling alone does **not** forward the flag into the `-cc1` argv:
   `Clang.cpp::ConstructJob()` needs an explicit
   `Args.addLastArg(CmdArgs, OPT_fbacktick, OPT_fno_backtick)` (as
   `-fsized-deallocation` / `-freflection` do) for driver-level visibility
   (DEV-03).
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

- **Scope — Resolved (D14): jointly, one paper.** The infix operator and the
  keyword-escape share one lexical token (D10), so they are co-designed in a
  single paper — to conserve EWG attention (one backtick discussion, not two)
  and to keep two independent designs from contradicting each other. The
  escape stays independently *motivated* (a future-keyword escape hatch; cf.
  Swift `` `class` ``, Rust `r#`) but is not independently *proposed*. Contrast
  D13: the library layer *is* split off, because it is separable and crosses
  into LEWG — the rule is bundle-within-a-committee, split-across-committees.

---

## 11. Sequencing

- **Phase 1 — MVP (Clang).** Lexer, precedence level, parser hook, Sema
  desugar to a call (D6), gating flag, tests. Result: `x `op` y` compiles
  and runs identically to `op(x, y)`; `-ast-print` shows the desugared
  call. Enough for people to kick the tires and for the paper's
  implementation-experience section.
- **Phase 2 — source fidelity (Clang).** Thin transparent AST wrapper (D7)
  so `-ast-print` round-trips backtick syntax. The wrapper's pretty-printer
  reconstructs the surface form from structure (LHS, callee, RHS); the
  backtick token locations it needs are already the inner `CallExpr`'s
  open/close paren locations, so no separate `SourceLocation` fields are
  required on the wrapper node (DEV-05). Purely additive; lands once the MVP
  is stable.
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

---

## 13. Alternative spellings (considered, disfavored) — D11

Backtick is the proposed spelling and the strongly preferred one. This
section exists so the alternatives are *explored on the record* with the
rebuttals pre-loaded for EWG, not because any is recommended. The bar an
alternative must clear is high: it must be (a) lexically unambiguous, and
(b) worth doubling the spelling surface — and none clears (b).

### 13.1 Why anyone raises it

- **Markdown inline code.** A single backtick is Markdown's inline
  code-span delimiter, so `x `op` y` in *running prose* fights the markup.
  (Fenced blocks — the dominant case, code samples — are unaffected.)
- **Keyboard ergonomics.** Backtick is a dead-key or awkward on some
  non-US layouts (it was historically one of the ISO-646-variant
  characters, alongside `# [ ] { } | ~ ^ \`).

Both are real and both are *minor*. Critically, neither is a *capability*
gap: CommonMark lets a longer backtick run delimit a span containing
shorter runs (with one leading/trailing space stripped) [CommonMark
§ Code spans, spec.commonmark.org/0.31.2/#code-spans], so inline prose is
expressible today — just ugly:

```
`` a `plus` b ``     renders the code span:   a `plus` b
```

So any alternative spelling buys *ergonomics for the minority (inline prose)
case*, nothing more.

**Concrete evidence — the venue itself.** This is not merely a spec nicety: the
double-backtick span renders correctly in the tools where committee members
actually write inline prose. In particular **Mattermost, the WG21 chat server,
supports it** — posting `` x `f` y `` displays as `x `f` y` (author-verified) —
as does GitHub and any other CommonMark-based renderer. So in the very forum
where the operator would most often be typed in running text, the friction is
already a solved problem, which substantially weakens the motivation for an
alternative spelling at its strongest point.

### 13.2 The lexical filter

An alternative is an additional *alternative token* lexed by maximal munch
(like the existing digraphs), minted only under `-fbacktick` (D5). To be
unambiguous the two-character sequence must never appear adjacent in a valid
current program. Three traps a candidate must survive — each has bitten a
real digraph before:

1. **Maximal-munch theft of an existing operator** (the reason `<<` vs `<`
   is delicate).
2. **The `::` / `<:` neighborhood** — `a<:b` needed the `<::` carve-out
   ([lex.pptoken]/3.2) because `vector<::std::string>` broke. Any new
   `<`-prefixed token lives in this neighborhood.
3. **Universal-character-name munch.** `\uXXXX` is a UCN that can begin an
   identifier, so `a<éfoo` is valid today (`a < éfoo`). A candidate
   whose second character is `\` (e.g. `<\`) followed by `u`/`U` will munch
   the `<\` and split the UCN — a non-obvious break.

### 13.3 Candidates

| Spelling | Self-delim? | Lexically clean? | Verdict |
|----------|-------------|------------------|---------|
| `\< … \>` | yes | **yes** — `\` is never a token today; `\` first means it can't start a UCN (`\<`/`\>` ≠ `\u`), and not at EOL so no line-splice. No carve-out needed. | **Front-runner if forced.** Asymmetric → retires §5 and D3. |
| `<| … |>` | yes | yes — `|`/`>` can't start a UCN; `<\|`-style theft N/A. One cosmetic edge: `&X::operator<|x` re-munches `operator<` (ill-formed today regardless). | **Blocked:** `|>` is P2011's pipeline-rewrite operator (Revzin); collides with a live proposal. Also reads as "pipe" (F#/OCaml/Elm). |
| `<\ … \>` | yes | **no** — `<\` munches the `<` of `a<éfoo` (UCN trap #3); needs a `<::`-style carve-out. | Inferior to `\< … \>` for no benefit; reject. |
| `(\| … \|)` (banana brackets) | yes | yes — `(|`/`|)` not valid adjacent today. | Heavy; Haskell-idiom connotation; reads worse than backtick. |
| single `\` (`x \op\ y`) | no | yes — stray `\` is ill-formed today. | Visually too light (confusable with escapes); symmetric, so keeps §5 + D3. |
| `<: :>`, `<% %>` | — | — | **Taken** — already digraphs for `[ ] { }`. |
| `\|: … :\|` / `:\| …` | — | **no** — `\|:` munches `a\| ::b` (`| ::`, trap #2). | Reject. |
| `$ … $` | no | **no** — `$` is an identifier char under `-fdollars-in-identifiers` (on by default in Clang/GCC); `a$b` already lexes as one identifier. | Reject. |
| `@ … @` | no | clean in C++ but `@` is the Objective-C sigil (shared lexer) and reads as implementation-reserved. | Reject. |
| `??x` trigraph-style | — | — | Trigraphs removed in C++17; dead on arrival. |

### 13.4 The front-runner, if ever forced

`\< … \>` is the only alternative that is both lexically bulletproof and
asymmetric. The asymmetry is not incidental: distinct open/close tokens
would **eliminate the same-delimiter parsing problem (§5)** — no
`BacktickIsOperator` flag — and **eliminate D3**, since nesting becomes
unambiguous (`x \<f \<g\> h\> y` parses with no parentheses). That is a
genuinely *better-engineered* operator than the backtick.

It is therefore important to state plainly: adopting `\< … \>` would not be
a backtick *alias* — it would be choosing a *different primary spelling*.
The decision in D11 is to keep backtick as the single spelling, not to ship
backtick *plus* an alias.

### 13.5 Rebuttals (pre-loaded for EWG)

Applicable to *any* alternative spelling:

1. **It's ergonomics, not capability.** Inline prose already works via
   CommonMark multi-backtick spans (§13.1); fenced blocks cover code. The
   gain is cosmetic and confined to running text.
2. **Two spellings is a permanent tax.** Teaching doubles; clang-format
   must pick and normalize a canonical; `-ast-print` must choose; grep /
   tooling / linters grow a second case — forever, for a cosmetic win.
3. **Direction of travel.** Trigraphs were *removed* in C++17 and digraphs
   are vestigial and periodically floated for removal. A *new* alternative
   token invites "and will you deprecate this one too?"
4. **It fragments the idiom.** The readability case for the operator rests
   on one recognizable form; two camps (backtick vs. digraph) undercuts it.
5. **None reads better than backtick.** Backtick is the established
   infix-quote idiom (Haskell). The alternatives carry foreign
   connotations: `<| |>`/`\< \>` say "pipe"/"escape," `(| |)` says
   "banana bracket."
6. **If the markup clash truly warranted a spelling change, it argues
   against the primary, not for a second.** We considered that and chose
   backtick-primary anyway, because fenced blocks dominate and the inline
   workaround exists. Adding an *alias* is the worst of both worlds.
7. **"Add it later if needed" is not a cheap option.** In committee time
   and motion, a follow-up alternate spelling costs almost as much process
   as deciding now — its own paper, an EWG design poll, CWG wording, and a
   ballot cycle. Deferral buys no real option value; it only splits the
   decision across two papers and risks shipping the operator first and
   bolting a second spelling on afterward (the worst sequencing). So the
   choice is made *here*, with conviction, not punted.

### 13.6 Conclusion

No alternative spelling is proposed, and the decision is taken *now* rather
than deferred — because (rebuttal 7) deferring it is nearly as much
committee work as settling it, so there is no option-value reason to leave
it open. Backtick is the sole spelling (D11). The analysis is recorded so
that, if EWG raises the Markdown/keyboard ergonomics, the answer is ready:
capability already exists, a second spelling is a standing tax against the
trend, and the only alternative worth considering (`\< … \>`) is not an
alias but a different operator we deliberately declined. If EWG nonetheless
wants to reopen the spelling, the place to do it is this paper — settling
the question against the recorded analysis — not a future one.

---

## 14. Reference: available ASCII lexical real estate

A digraph (an alias for an existing token, §13) and a brand-new operator
draw on the same pool: ASCII sequences that are *not already a token* and
*never appear adjacent in a valid current program*. This appendix inventories
that pool. It is reference material — most of it is moot for *this* proposal
(see §14.4), but it is exactly what gets asked in the room.

### 14.1 The availability rule

A two-character sequence `XY` is available iff:

1. `XY` is not a current token or digraph, **and**
2. after `X`, the character `Y` cannot begin a valid operand or continue a
   token — i.e. `Y` is not one of the unary-prefix operators
   `- + * & ~ !`, not `(`/`[`/identifier/literal start, and `XY` is not the
   prefix of a longer real token.

Clause 2 is the one that surprises people. Where `X` is a binary operator or
`<`, putting a unary-capable character after it is *already valid*:

```
a < -b      a < +b      a < *p      a < &x      a < ~b      a < !b
a * *p   (== a * (*p))  a + +b      a - -b      a & &x
```

So `<-`, `<+`, `<*`, `<&`, `<~`, `<!`, `**`, `!!`, `~~`, … are **blocked** —
adding any of them as a token silently changes the meaning of existing code
(`!!x`, the bool-cast idiom, and `a * *p` are the cautionary cases). `<|`
survives *only* because `|` is the one "bar" with no unary form. Plus the two
traps from §13.2: the `::` / `<:` neighborhood (`a | ::b`, `a<:b`) and UCN
munch (`a<éfoo`).

### 14.2 Free standalone characters

The only printable ASCII characters with no C++ token meaning at all:

| Char | Status |
|------|--------|
| `` ` `` | **Claimed by this proposal.** Otherwise free (literals/raw-string delimiters are lexed earlier). |
| `\` | Free as a token, but it *is* line-continuation (phase 2) and the UCN lead-in; usable only in combos that keep it off EOL and away from `u`/`U` (§13.2 trap 3). |
| `@` | Free in C++, but the Objective-C sigil (shared lexer) and reads as implementation-reserved. |
| `$` | An identifier character under `-fdollars-in-identifiers` (default-on in Clang/GCC): `a$b` already lexes as one identifier. Effectively unavailable. |

Every other printable ASCII char is a token or token-prefix.

### 14.3 Candidate multi-character sequences

Verdict for the sequences people actually ask about. "Available" = lexically
clean to mint under a flag; "blocked" = breaks valid code or already taken.

| Seq | Available? | Note |
|-----|-----------|------|
| `=>` | yes | `a = >b` is ill-formed today. Strong "arrow/lambda" connotation (C#, JS, Rust). |
| `==>` | yes | `a == >b` ill-formed today. Natural spelling for **logical implication** (§14.4). |
| `<==` | yes | Munches cleanly (`<=` then `=` is ill-formed today). Converse implication, if ever wanted. |
| `<==>` | yes | Biconditional / "iff", if ever wanted. |
| `<\|` | yes | Reverse-pipe; the only clean `<X`. |
| `\|>` | **blocked (social)** | Lexically clean, but it is P2011's pipeline-rewrite operator (Revzin). |
| `~>` | yes | `a ~> b` ill-formed today (`~` has no binary form). "leads-to" connotation. |
| `\< … \>`, `(\| … \|)` | yes | Asymmetric self-delimiting pairs — see §13.3. |
| `<-` `<+` `<*` `<&` `<~` `<!` | **blocked** | `a < -b`, `a < *p`, … already valid (14.1). |
| `**` | **blocked** | `a * *p` already valid. (So no `**` exponentiation.) |
| `!!` `~~` | **blocked** | `!!x`, `~~x` already valid (unary idioms). |
| `^^` | **blocked (taken)** | C++26 reflection operator (P2996). Was available (`a ^ ^b` ill-formed; `^` has no unary form) before P2996 claimed it — see §14.5. |
| `%%` | yes | `a % %b` ill-formed today (`%` has no unary form); doubling-a-no-unary-form operator, like `^^` before it was taken (§14.5). |
| `<=>` | **blocked (taken)** | Spaceship (landed). |
| `<\ … \>` | needs carve-out | UCN munch trap (§13.2); inferior to `\< … \>`. |
| `<: :>` `<% %>` `%:` | **blocked (taken)** | Existing digraphs. |

### 14.4 Why new operators are mostly off the table — and the one that isn't

This proposal is, in effect, a *general* infix-operator facility: any named
binary operation is `x `op` y` with no new punctuator. `x `implies` y`,
`x `pow` y`, `x `dot` y` all work today under the feature. So the standing
demand for new operator *punctuators* — which previously justified spending
scarce lexical real estate — largely evaporates. That is a point worth making
affirmatively in the paper: backtick is the reason the table above can stay
mostly unspent.

The residual cases where a *dedicated* operator still earns its keep are the
ones a desugar-to-call **cannot** express:

- **Non-strict / short-circuit evaluation.** A call evaluates all arguments.
  **Walter Brown's logical-implication operator** is the canonical example:
  `p ==> q` ≡ `!p || q`, whose RHS is **not evaluated when `p` is false**.
  `p `implies` q` desugared to `implies(p, q)` evaluates `q` unconditionally
  — observably different (side effects, cost, well-definedness) *when `q` is a
  bare expression*. **But** a helper taking the RHS as a *thunk*
  (`p `implies` [&]{ q }`) recovers the short-circuit (§16.5), and that is a
  general user-space capability the language otherwise reserves to `&&`/`||`
  (which overloading cannot restore). So the residual value of a dedicated
  `==>` is *ergonomic* — omitting the per-call thunk for the common boolean
  case — not a hard capability gap. It remains a reasonable candidate; `==>`
  is lexically available (14.3) and mnemonic for `⟹` (the missing
  short-circuit sibling of `&&`/`||`).
- **Custom precedence/associativity** that the single backtick level (§4)
  cannot give.
- **Ultra-high-frequency** operations where `x `op` y` ceremony genuinely
  outweighs a glyph — a high bar.

Everything else: write it as a backtick call. The inventory in 14.1–14.3 is
therefore best read as *what remains technically possible*, with the
expectation that this proposal removes most of the *motivation* to spend it —
implication's lazy RHS being the notable exception.

### 14.5 Prior art for this analysis

This exact "what ASCII is actually free" exercise has been run to a
conclusion in committee before, which is why the converse/biconditional rows
are kept above (14.3) even without a current proponent — the next person to
revisit operator real estate inherits the worked example rather than redoing
it. The clearest precedent is **P2996 reflection**: it began on a single `^`
and moved to the `^^` digraph (the "neko" / mountain operator) only after the
same availability analysis showed single `^` was too entangled — it is
bitwise-xor, and `^` is already the Clang/Objective-C blocks sigil. The
double form cleared the filter (`a ^ ^b` — `^` has no unary form — is
ill-formed today, so `^^` was unclaimed; cf. 14.1) and shipped. The lesson
carried into this appendix: doubling an operator with **no unary form** is
the reliable way to find clean real estate (`^^`, and likewise `%%` would be
available), whereas doubling one that *has* a unary form is blocked
(`**`, `!!`, `~~` — 14.3).

---

## 15. Relationship to the pipeline-rewrite operator (P2011, `|>`)

Barry Revzin's `|>` (the "pizza" operator, P2011) and backtick both ultimately
produce a call expression, and their degenerate 2-argument cases look alike,
so the relationship must be stated explicitly: **they are orthogonal,
complementary, and neither replaces the other.** Backtick deliberately leaves
`|>` unspelled (§13.3 / §14.3) precisely so the two can coexist in one
program.

### 15.1 What each one is

- **Backtick** — `x `f` y` desugars to `f(x, y)`. Symmetric *binary infix*
  application of a callable: the **callee sits between two operands**, and the
  result is an ordinary call, so overload resolution, ADL, templates, and
  function objects all apply (D6).
- **P2011 `|>`** — `x |> f(args...)` is *rewritten* to `f(x, args...)`. A
  **syntactic rewrite** that prepends the left operand as the first argument
  of the *call expression* written on the right. There is no `operator|>`; it
  is **not overloadable**, and the right-hand call may have **any arity**.

### 15.2 Side by side

| Aspect | backtick `` x `f` y `` | pipeline `x |> f(...)` |
|--------|------------------------|------------------------|
| Shape | symmetric binary infix | directional "prepend-arg" thread |
| Right-hand syntax | a single operand (a value) | a call expression with its own args |
| Operator slot | the callee, between the ticks | n/a — callee is on the right |
| Resulting call arity | exactly 2 | `1 + (RHS args)`, any N |
| Mechanism | desugar to a normal call | pure syntactic rewrite |
| Overloadable? | yes (it *is* a call) | no (by design) |
| Precedence | highest binary (tighter than `*`) | low (pipeline level) |
| Native idiom | binary *operations* — `a `min` b` | transformation *chains* — `r \|> filter(p) \|> sum()` |

### 15.3 Where they overlap — and why neither becomes redundant

The 2-argument case coincides: `a `plus` b`, `a |> plus(b)`, and `plus(a, b)`
all yield the same call, and both operators left-fold —
`a `f` b `g` c` and `a |> f(b) |> g(c)` both give `g(f(a, b), c)`. But the
overlap stops there:

- Backtick cannot express what `|>` does beyond binary. `x |> f(a, b, c)`
  threads `x` into an arbitrary-arity call; backtick's right-hand side is a
  single operand, not an argument list, so there is no backtick spelling of
  `f(x, a, b, c)`. **Beyond two operands, only `|>` threads.**
- `|>` cannot write a binary operation *symmetrically between* its operands.
  Its right-hand side is always a (partial) call and the left is always
  threaded in front, so `a `min` b` becomes `a |> min(b)` — which reads as a
  pipe *stage*, not an *operation*. **For "x op y" notation — predicates,
  arithmetic, comparisons — backtick is the spelling.**

### 15.4 Why backtick does not replace `|>`

Backtick is not a pipeline operator. It does not thread a value through a
sequence of N-ary transformations, and it has the wrong precedence (high,
binary) and the wrong shape (symmetric, single-operand RHS) for chaining.
P2011's entire purpose — UFCS-style left-to-right chaining of range adaptors
and free functions that carry extra arguments, *without* the `operator|`
machinery — is untouched by backtick.

### 15.5 Why `|>` does not replace backtick

`|>` is not an infix-operator facility. It cannot place an arbitrary binary
callable symmetrically between two operands; it always prepends the left
operand to a call on the right, and it is non-overloadable. Backtick's
purpose — infix notation for binary operations that desugars to ordinary,
overload-resolved calls — is untouched by `|>`.

### 15.6 They compose

The two are at their best together: backtick supplies infix detail *inside* a
pipeline stage, `|>` threads the value *between* stages.

```cpp
r |> filter([](auto e){ return e `mod` 2 `eq` 0; }) |> sum()
//                              \_____ eq(mod(e, 2), 0) _____/
```

Guidance for the paper: present them as complementary — backtick for "this is
a binary operation," `|>` for "thread this value through these stages" — and
explicitly disclaim that either subsumes the other. Recording it here so the
EWG question ("doesn't one of these make the other unnecessary?") has a
ready, worked answer.

---

## 16. Producing pipeline-like outcomes with backtick

Backtick is symmetric binary infix, not a pipeline operator (§15) — but its
operator slot is an *arbitrary callable expression*, and it chains
left-associatively. Those two facts let a handful of patterns — each a few
lines of *ordinary user code*, no standard-library addition — reproduce most
pipeline / `|>` / ranges-`|` outcomes with no core language change beyond
backtick itself. **The paper proposes none of these helpers**; scope is
language-only (§16.7 / D13). They appear here as *motivation* — showing the
operator's reach, and marking precisely where the one real gap vs. `|>` sits.

### 16.1 The threading pattern — `pipe(x, f) = f(x)`

One trivial helper turns backtick into a left-to-right value-threading
operator:

```cpp
inline constexpr auto pipe =
    [](auto&& x, auto&& f) -> decltype(auto)
    { return std::invoke(std::forward<decltype(f)>(f),
                         std::forward<decltype(x)>(x)); };

x `pipe` f `pipe` g `pipe` h     // == h(g(f(x))) — left-assoc, data-flow order
```

Each stage is a unary callable; the reading order matches `|>`.

### 16.2 It drives the existing range-adaptor closures unchanged

The decisive case. Range adaptor *closures* (`views::filter(pred)`,
`views::transform(fn)`) are **already unary callables** — `c | a` is *defined*
as `a(c)`. So `pipe` feeds them directly, with no `bind`:

```cpp
r `pipe` views::filter(pred) `pipe` views::transform(fn)
// identical result and laziness to:
r |  views::filter(pred) |  views::transform(fn)
```

Same closures, same lazy views. Backtick + one helper is a drop-in spelling of
the range pipe. The bespoke per-library `operator|` overloads exist only to
choose the `|` *syntax*; the closure objects themselves need nothing, so they
work under backtick for free.

### 16.3 Parameterized free-function stages — `bind_back`

For a plain free function that takes the piped value first plus extra
arguments, fix the trailing args with `std::bind_back` (C++23) or a lambda:

```cpp
r `pipe` std::bind_back(filter, pred) `pipe` std::bind_back(transform, fn)
// == transform(filter(r, pred), fn)
```

This is exactly the case P2011 `|>` writes more directly —
`r |> filter(pred) |> transform(fn)`, arguments inline. Backtick needs the
`bind_back`/lambda wrapper to turn the stage into a unary callable: same
result, more ceremony. **This is the one ergonomic gap vs. `|>`** (§16.6).

### 16.4 Reusable point-free pipelines — `then` (composition)

Compose stages into a named pipeline once, apply it many times:

```cpp
inline constexpr auto then =
    [](auto f, auto g)
    { return [=](auto&&... a) -> decltype(auto)
        { return g(f(std::forward<decltype(a)>(a)...)); }; };

auto clean = trim `then` lower `then` dedup;   // a reusable callable
clean(s);
```

Mirrors building a reusable view/adaptor chain; left-assoc backtick gives
left-to-right composition.

### 16.5 User-defined short-circuiting (non-strict) operators

This is the strongest single argument in §16, so it leads. C++ reserves
short-circuit / non-strict evaluation to a fixed set of built-ins — `&&`,
`||`, `?:`, `,` — and you **cannot** get it back by overloading: an overloaded
`operator&&` / `operator||` evaluates both operands (the classic footgun, and
the reason the standard discourages overloading them). Backtick reopens this
for users. A helper whose right operand is a *callable* controls whether — and
when — that operand runs:

```cpp
// short-circuiting logical implication:  p ==> q  ≡  !p || q
inline constexpr auto implies =
    [](bool p, auto&& q) -> bool { return !p || q(); };

p `implies` [&]{ return expensive(); }   // q() runs only when p holds
```

The same shape gives lazy defaults (`opt `or_else` [&]{ costly(); }`), guarded
effects, and bespoke control operators — any binary operation that must *not*
evaluate its right side unconditionally. This is a *general* user-facing
capability the language otherwise denies, not a niche trick.

The **monadic chain** is simply the zero-ceremony special case: the stages are
already functions, so no thunk is written and short-circuiting falls out for
free:

```cpp
inline constexpr auto mbind =
    [](auto&& m, auto&& f)
    { return std::forward<decltype(m)>(m)
                 .and_then(std::forward<decltype(f)>(f)); };

parse(s) `mbind` validate `mbind` store;   // stops at the first empty / error
```

(If "monadic" costs more audience than it earns in EWG, lead with the
short-circuit framing above and present this as "chaining fallible steps" — the
capability is the point, not the vocabulary.)

This refines §14.4: backtick **can** express short-circuiting implication after
all — when the right operand is passed as a thunk. What a dedicated `==>` adds
is only the *ergonomics* of omitting that thunk for the common boolean case; it
is not a hard capability gap. A bare-*expression* RHS still evaluates eagerly
(backtick desugars to a call), so the thunk is the price of generality.

### 16.6 What this recovers — and the one thing it doesn't

Recovered, library-only (no core change beyond backtick itself):

- left-to-right value threading (16.1–16.2),
- the **entire existing ranges adaptor-closure ecosystem**, unchanged (16.2),
- parameterized stages (16.3),
- reusable point-free composition (16.4),
- **user-defined short-circuiting / non-strict operators** (16.5) — a
  capability the language otherwise reserves to `&&` / `||` / `?:` and that
  operator overloading cannot recover; the monadic/fallible chain is its
  zero-ceremony special case.

Not recovered — the precise boundary with `|>`:

- **P2011's inline-argument stage syntax.** `x |> f(a, b, c)` writes the extra
  args in the call and threads `x` in front. Backtick stages must be *unary
  callables*, so the extra args go through `bind_back`/a lambda (16.3). Same
  outcome, more ceremony — this is exactly why backtick does not make `|>`
  redundant (§15.4).
- **Precedence direction.** Backtick binds *high* (tighter than `*`, §4),
  whereas `|>` binds *low*. Pipe-style stages are normally primaries
  (`views::filter(pred)`), so this is usually invisible; but a stage that is
  itself a low-precedence expression must be parenthesised — the opposite
  default from `|>`.

Net: for the *common* pipeline use-cases, backtick plus a one-line helper
(often just `pipe`) is sufficient and reuses the existing closure ecosystem;
the dedicated `|>` earns its keep specifically for inline-argument stages and
low-precedence chaining. Complementary, as §15 concludes.

### 16.7 Scope: motivation, not a library proposal

Every helper above is a few lines of *ordinary user code* — no standard-library
addition is required for any of it, and the paper proposes none. That is
deliberate and load-bearing for process:

- **Keeps the paper in one committee track.** A core-language operator goes
  through EWG/CWG. Bundling standard helpers would add an LEWG track — the
  time-and-motion cost (D11 rebuttal 7, §13.5) doubled across two committees,
  on two schedules, with two sets of bikeshedding. This proposal is
  language-only (D13).
- **The library layer is optional and can mature independently.** Because the
  operator is expressive enough that `pipe` / `then` / `mbind` are
  user-writable one-liners, there is no rush: land the language feature early
  (targeting C++29), let real usage reveal which helpers are actually worth
  standardizing, and bring those in a separate companion library paper later —
  with field experience behind them rather than ahead.
- **The patterns still pull their weight here, as *motivation*.** Showing the
  reachable outcomes — especially that backtick drives the existing ranges
  adaptor-closure ecosystem unchanged (§16.2) — helps EWG members who spend
  less time on library design see *why* the operator is useful, without asking
  them to approve any library surface. Direction without commitment.

So §16 is a worked illustration of reach, explicitly out of scope for
standardization, with a companion library paper named as the future home for
anything that earns it.

---

## 17. Further semantic clarifications (D3 reframed, D15, D16, ADL)

Resolutions reached after implementation, sharpening four points the original
decisions log under-specified.

### 17.1 Nesting vs. chaining — the D3 grouping rule

The operator slot's open and close delimiters are the same token, so the first
interior backtick always closes the slot. Two consequences: the slot can never
contain a *bare* backtick, and what looks like "bare nesting" is
token-identical to an ordinary left-associative chain.

```
a ` f ` b ` g ` c     (D1 chain, blessed)   -->  g(f(a, b), c)
x ` f ` g ` h ` y     ("bare nesting")      -->  h(f(x, g), y)
```

Same shape, different names. Therefore:

- "Bare nesting" is not a distinct construct and **cannot be diagnosed** — it
  is exactly the chain D1 already defines and blesses. A diagnostic would have
  to fire on legal D1 chaining, a contradiction.
- The original D3 wording ("bare nesting naturally produces a parse error") was
  not just wrong but impossible; the parser is correct to accept it, and Clang
  and GCC agree (DEV-04 / DEV-G04, reclassified from "deferred enforcement" to
  "no enforcement needed").

**Rule (D3, reframed):** to nest a backtick expression in the operator slot,
parenthesize it — `x `(f `g` h)` y` == `(g(f, h))(x, y)`. Without parentheses
you get a left-associative chain (D1). The syntax is new, but the problem class
is old and well-understood: it is the **binary-minus situation**. Subtraction
is *non-associative*, so `a - b - c` == `(a - b) - c` ≠ `a - (b - c)` — the
default left grouping silently changes the result, and the language has never
diagnosed it. Parentheses override the grouping; they do not avoid an error.
(The same `-` is also the precedent for D10's position-based disambiguation:
`-` is unary in operand position, binary in post-operand position, exactly as
backtick is escape vs. infix.)

**The silent-surprise case, and why it is left undiagnosed.** Because the
greedy chain is always *syntactically* valid, whether it also *type-checks*
depends on the callables. Almost always the chain fails to type-check when a
user actually meant to nest, yielding a (misleading) error rather than a wrong
answer. A fully silent miscompile is possible only with a pathological type
that is simultaneously callable, value-convertible, and non-symmetric:

```cpp
struct Op {
    int v;
    Op operator()(Op a, Op b) const { return Op{a.v*2 + b.v + v}; } // non-symmetric
    operator int() const { return v; }
};
Op f{1}, g{2}, h{3}, x{10}, y{20};

int bare     = x `f `g` h` y;       // greedy chain : h(f(x,g), y)  == 69
int intended = x `(f `g` h)` y;     // nested       : (g(f,h))(x,y) == 47
```

Both compile and differ (69 vs. 47) — the very same answer-changing-on-regroup
that `a - b - c` ≠ `a - (b - c)` already exhibits for binary minus, which no
compiler diagnoses. So this is not even a new category of hazard. It falls
squarely under the **Murphy / Machiavelli rule**: the language defends against
Murphy (honest mistakes), not Machiavelli (deliberate self-sabotage). Building `Op` to be
callable *and* a value *and* asymmetric, then omitting the parentheses, is
self-inflicted; the fix is one pair of parentheses. It does not justify a
normative diagnostic — least of all one that cannot distinguish itself from
blessed D1 chaining.

*Possible QoI follow-up (non-normative).* It may still be worth investigating a
*heuristic* Clang warning — e.g. when a chain's intermediate operand is itself
a callable used in operand position, suggest parentheses. That would be opt-in,
off-by-default diagnostic quality-of-implementation, never a language rule, and
must not fire on ordinary chaining. Flagged for investigation, not committed.

### 17.2 Evaluation order (D15)

`x `f` y` is defined as the call `f(x, y)`, so it introduces **no new
evaluation-order rule** and inherits [expr.call] wholesale:

- operand evaluation order is **unspecified** (indeterminately sequenced) — the
  same long-standing situation that defeated past attempts to mandate LTR/RTL
  for call arguments;
- since C++17 the callee is sequenced *before* the arguments, so the **slot is
  evaluated before both operands**, even though it is written *between* them.
  Source order `(x, slot, y)` is therefore not the evaluation order
  `(slot, then {x, y})`.

A feature of "it is just `f(x, y)`," not a special case: anyone who knows call
semantics already knows backtick's.

### 17.3 Type-name in the operator slot (D16)

The slot is any callable expression, and a type-name is callable, so a type in
the slot is well-formed and yields construction:

```cpp
x `T` y          // == T(x, y) : a prvalue T, functional-style construction
a `std::pair` b  // == std::pair(a, b), with CTAD
```

It is always an **expression** (the slot is parsed as an assignment-expression,
D4; backtick's result is an expression by construction), so it can never appear
in declaration position — the most-vexing-parse declaration reading cannot
arise. The keyword-escape use of backtick (§12) occupies operand/declarator
position, not the post-operand infix position, so there is no collision.
Blessed as a consistent, useful consequence rather than a special rule.

### 17.4 ADL is normative (cross-compiler note)

`x `f` y` performs argument-dependent lookup on the slot exactly as the call
`f(x, y)` would (D6). This is **normative**: backtick must not silently have
weaker lookup than the call it desugars to. Implementation status, for the
implementation-experience section: Clang delivers full ADL (the slot reaches
`BuildCallExpr` as an `UnresolvedLookupExpr`); GCC currently resolves the slot
name at parse time, so pure-ADL and ADL-augmentation fail (DEV-G05). That is a
**defect to correct** in the in-progress GCC track — carry the slot as an
unresolved/dependent name into `finish_call_expr` — not a permitted
cross-compiler difference.
