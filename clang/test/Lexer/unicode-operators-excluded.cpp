// RUN: %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only -verify=on,common %s
// RUN: %clang_cc1 -std=c++23 -fsyntax-only -verify=off,common %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -dump-tokens %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TOK --implicit-check-not='user_operator'

// U05: an excluded code point says *why* it is excluded (U§8 paragraph 2), and
// is never aliased to the token it apes (U§5 predicate 5, U§10).
//
// The three exclusion reasons in the U02 table do not have the same scope, and
// that is this step's finding:
//
//   ConfusableWith / EmojiPresentation -- code points that are not identifier
//     characters and never were, so nothing but an operator could have been
//     meant.  Diagnosed by the *lexer*, at the classification point, for the
//     glyph and for every universal-character-name spelling alike.
//
//   IdentifierProfile (∂ ∇ ∞) -- code points that ARE identifier characters
//     here: Clang's D137051 mathematical-notation extension is on by default
//     in every -std= mode, so `int ∂(int);` compiles.  A lexer diagnostic for
//     these would fire on every legitimate use of one as a name.  Diagnosed
//     only in operator-name position, and only as a note on a parse that has
//     already failed -- see section 5.
//
// Both -verify runs read the same source: `on` is -funicode-operators, `off`
// is upstream, `common` is what neither flag changes.

int a, b;

//--- 1. ConfusableWith, all three spellings of one code point ---------------
//
// U11 says the spellings of an operator are equivalent; an exclusion that
// caught only the glyph would undercut that.  All three get the same message.

int c1 = a − b;
// on-error@-1 {{'−' U+2212 is not a user-defined operator: it is confusable with '-'}}
// off-error@-2 {{unexpected character '−' U+2212}}
// common-error@-3 {{expected ';' after top level declarator}}

int c2 = a \u2212 b;
// on-error@-1 {{'−' U+2212 is not a user-defined operator: it is confusable with '-'}}
// common-error@-2 {{expected ';' after top level declarator}}

int c3 = a \N{MINUS SIGN} b;
// on-error@-1 {{'−' U+2212 is not a user-defined operator: it is confusable with '-'}}
// common-error@-2 {{expected ';' after top level declarator}}

// Adjacency: an excluded code point ends an identifier rather than being
// absorbed into it, exactly as a U1 code point does (U03).  Without that the
// only thing reported would be "not allowed in an identifier", which is true
// and says nothing about why U+2212 in particular is out.
int c4 = a−b;
// on-error@-1 {{'−' U+2212 is not a user-defined operator: it is confusable with '-'}}
// off-error@-2 {{character '−' U+2212 not allowed in an identifier}}
// on-error@-3 {{expected ';' after top level declarator}}
// off-error@-4 {{use of undeclared identifier 'a−b'}}

//--- 2. ConfusableWith, the whole table ------------------------------------
//
// One line per ConfusableWith entry.  The ASCII token in each message comes
// from the generated table (UnicodeOperatorCharSets.h), not from a switch in
// the lexer, so this is also a test that the derivation and the diagnostic
// cannot drift apart.

int d1 = a ⁄ b;   // U+2044 FRACTION SLASH -- outside the U1 blocks entirely
// on-error@-1 {{'⁄' U+2044 is not a user-defined operator: it is confusable with '/'}}
// off-error@-2 {{unexpected character '⁄' U+2044}}
// common-error@-3 {{expected ';' after top level declarator}}

int d2 = a ⇐ b;
// on-error@-1 {{'⇐' U+21D0 is not a user-defined operator: it is confusable with '<='}}
// off-error@-2 {{unexpected character '⇐' U+21D0}}
// common-error@-3 {{expected ';' after top level declarator}}

int d3 = a ⇒ b;
// on-error@-1 {{'⇒' U+21D2 is not a user-defined operator: it is confusable with '=>'}}
// off-error@-2 {{unexpected character '⇒' U+21D2}}
// common-error@-3 {{expected ';' after top level declarator}}

int d4 = a ⇔ b;
// on-error@-1 {{'⇔' U+21D4 is not a user-defined operator: it is confusable with '<=>'}}
// off-error@-2 {{unexpected character '⇔' U+21D4}}
// common-error@-3 {{expected ';' after top level declarator}}

int d5 = a ∕ b;
// on-error@-1 {{'∕' U+2215 is not a user-defined operator: it is confusable with '/'}}
// off-error@-2 {{unexpected character '∕' U+2215}}
// common-error@-3 {{expected ';' after top level declarator}}

int d6 = a ∗ b;
// on-error@-1 {{'∗' U+2217 is not a user-defined operator: it is confusable with '*'}}
// off-error@-2 {{unexpected character '∗' U+2217}}
// common-error@-3 {{expected ';' after top level declarator}}

int d7 = a ∙ b;
// on-error@-1 {{'∙' U+2219 is not a user-defined operator: it is confusable with '.'}}
// off-error@-2 {{unexpected character '∙' U+2219}}
// common-error@-3 {{expected ';' after top level declarator}}

int d8 = a ∣ b;
// on-error@-1 {{'∣' U+2223 is not a user-defined operator: it is confusable with '|'}}
// off-error@-2 {{unexpected character '∣' U+2223}}
// common-error@-3 {{expected ';' after top level declarator}}

int d9 = a ∶ b;
// on-error@-1 {{'∶' U+2236 is not a user-defined operator: it is confusable with ':'}}
// off-error@-2 {{unexpected character '∶' U+2236}}
// common-error@-3 {{expected ';' after top level declarator}}

int d10 = a ≤ b;
// on-error@-1 {{'≤' U+2264 is not a user-defined operator: it is confusable with '<='}}
// off-error@-2 {{unexpected character '≤' U+2264}}
// common-error@-3 {{expected ';' after top level declarator}}

int d11 = a ≥ b;
// on-error@-1 {{'≥' U+2265 is not a user-defined operator: it is confusable with '>='}}
// off-error@-2 {{unexpected character '≥' U+2265}}
// common-error@-3 {{expected ';' after top level declarator}}

int d12 = a ⋅ b;
// on-error@-1 {{'⋅' U+22C5 is not a user-defined operator: it is confusable with '.'}}
// off-error@-2 {{unexpected character '⋅' U+22C5}}
// common-error@-3 {{expected ';' after top level declarator}}

//--- 3. EmojiPresentation --------------------------------------------------

int e1 = a ⌚ b;
// on-error@-1 {{'⌚' U+231A is not a user-defined operator: characters with emoji presentation are excluded from the operator set}}
// off-error@-2 {{unexpected character '⌚' U+231A}}
// common-error@-3 {{expected ';' after top level declarator}}

int e2 = a ⭐ b;
// on-error@-1 {{'⭐' U+2B50 is not a user-defined operator: characters with emoji presentation are excluded from the operator set}}
// off-error@-2 {{unexpected character '⭐' U+2B50}}
// common-error@-3 {{expected ';' after top level declarator}}

//--- 4. The IdentifierProfile characters are still identifiers -------------
//
// The composability claim of U10 / U§7.1, asserted rather than predicted: the
// operator set and the (extended) identifier set are disjoint, so turning
// -funicode-operators on must not take ∂ ∇ ∞ away from the identifier side.
// If the exclusion reason were emitted at the classification point, every one
// of these lines would be an error.

namespace ids {
int ∂(int);                        // common-warning {{mathematical notation character '∂' U+2202 in an identifier is a C++2d extension}}
int ∇ = 0;                         // common-warning {{mathematical notation character '∇' U+2207 in an identifier is a C++2d extension}}
int ∞ = 0;                         // common-warning {{mathematical notation character '∞' U+221E in an identifier is a C++2d extension}}
int use∂ = ∂(1) + ∇ + ∞;
// common-warning@-1 {{mathematical notation character '∂' U+2202 in an identifier is a C++2d extension}}
// common-warning@-2 {{mathematical notation character '∂' U+2202 in an identifier is a C++2d extension}}
// common-warning@-3 {{mathematical notation character '∇' U+2207 in an identifier is a C++2d extension}}
// common-warning@-4 {{mathematical notation character '∞' U+221E in an identifier is a C++2d extension}}
} // namespace ids

//--- 5. IdentifierProfile in operator-name position ------------------------
//
// The one place the reason can be given: an operator was unambiguously meant,
// and the conversion-function-id reading has already failed.  A note, not an
// error, because the error is the one the parse already produced.

struct S {};
int operator ∂(S, S);
// common-warning@-1 {{mathematical notation character '∂' U+2202 in an identifier is a C++2d extension}}
// common-error@-2 {{unknown type name '∂'}}
// on-note@-3 {{'∂' U+2202 is an identifier character (mathematical notation profile), not a user-defined operator}}

// The same code point spelled as a UCN reaches the same place.
int operator \u2207(S, S);
// common-warning@-1 {{mathematical notation character '∇' U+2207 in an identifier is a C++2d extension}}
// common-error@-2 {{unknown type name '∇'}}
// on-note@-3 {{'∇' U+2207 is an identifier character (mathematical notation profile), not a user-defined operator}}

// And the note stays out of the way of a real conversion function whose type
// happens to be named by one of the three -- which is why it is attached to a
// failure rather than emitted on sight.
using ∞ = int;
// common-warning@-1 {{mathematical notation character '∞' U+221E in an identifier is a C++2d extension}}
struct T { operator ∞(); };
// common-warning@-1 {{mathematical notation character '∞' U+221E in an identifier is a C++2d extension}}

//--- 6. Non-aliasing, at the token level -----------------------------------
//
// The security property (U§10): a character that looks like `-` must fail to
// lex, never quietly mean something else.  --implicit-check-not='user_operator'
// on the -dump-tokens run above covers every line of this file at once: not
// one excluded code point, in any spelling, ever mints an operator token.
// TOK: unknown '\342\210\222'
