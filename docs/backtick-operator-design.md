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
| D11 | Backtick is the sole spelling; no alternative/digraph spelling | **Proposed — disfavored alternatives recorded (§13)** | Markup friction (Markdown inline code) and keyboard ergonomics are real but minor: CommonMark's multi-backtick span already makes inline prose expressible and fenced blocks cover code samples (capability, not just ergonomics, is already there). A second spelling doubles teaching / clang-format / `-ast-print` / tooling surface, fragments the idiom, and swims against the trigraph-removed (C++17) / digraph-vestigial trend. If EWG ever forces one, an asymmetric self-delimiting pair (`\< … \>`) is the front-runner because it would *also* retire §5 and D3 — but that is a different operator, not a backtick alias. Full analysis and rebuttals in §13. |
| D12 | Orthogonal to P2011 `\|>` (pipeline-rewrite, "pizza"); does not replace it | **Resolved** | Both bottom out in a call and the 2-arg case overlaps, but backtick is *symmetric binary infix* desugaring to an ordinary overload-resolved call, while `\|>` is a *non-overloadable syntactic rewrite* prepending the left operand to an arbitrary-arity call. Different shape, arity, precedence, mechanism, and idiom; they compose rather than compete. Backtick also deliberately declines the `\|>` spelling (§13.3 / §14.3) so both can coexist in one program. Full analysis in §15. |

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
shorter runs (with one leading/trailing space stripped), so inline prose is
expressible today — just ugly:

```
`` a `plus` b ``     renders the code span:   a `plus` b
```

So any alternative spelling buys *ergonomics for the minority (inline prose)
case*, nothing more.

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
  — observably different (side effects, cost, well-definedness). So
  implication is genuinely *not* subsumed by backtick and remains a live
  candidate for a real operator; `==>` is lexically available (14.3) and
  mnemonic for `⟹`. (`&&`/`||` are in the language for exactly this
  short-circuit reason; implication is the missing third.)
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
