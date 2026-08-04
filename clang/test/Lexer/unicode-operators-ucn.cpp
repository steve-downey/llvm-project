// RUN: %clang_cc1 -std=c++23 -funicode-operators -dump-tokens %s 2>&1 | FileCheck --check-prefix=ON %s
// RUN: %clang_cc1 -std=c++23 -dump-tokens %s 2>&1 | FileCheck --check-prefix=OFF %s
// RUN: not %clang_cc1 -std=c++23 -funicode-operators -fsyntax-only %s 2>&1 | FileCheck --check-prefix=EXCL %s

// U04/U11: a universal-character-name designating a U1 code point *is* that
// operator token, exactly as a UCN designating an XID character participates
// in an identifier.  The extended-character = UCN equivalence the language
// maintains for identifiers extends to these tokens because they are the first
// non-basic tokens; the absence of UCN spellings for the existing punctuators
// is an accident of every punctuator being basic-character-set, not a rule.
//
// This file is the *token-level* half of the claim.  It is necessary and not
// sufficient: -dump-tokens is a strictly-preprocessor action, so it runs with
// Preprocessor::isPreprocessedOutput() set, which suppresses the lexer's
// absorb-the-stray-character-into-the-identifier recovery.  The adjacency and
// identity halves are asserted under an ordinary Preprocessor by
// LexerTest.UnicodeOperatorUCNAdjacency and
// LexerTest.UnicodeOperatorUCNSpellingsAreOneOperator, and at the
// declaration/use level -- which is the level that matters, since the code
// point and not the spelling is the operator's identity -- by
// clang/test/Parser/unicode-operator-decl.cpp.

int a, b;

// The glyph, first, so the UCN groups below have something to be identical to.
int glyph = a ⊞ b;
// ON:      identifier 'glyph'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\342\212\236'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'glyph'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\342\212\236'
// OFF-NEXT: identifier 'b'

// The four-digit numeric form.
int ucn_u = a \u229E b;
// ON:      identifier 'ucn_u'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\\u229E'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'ucn_u'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\\u229E'
// OFF-NEXT: identifier 'b'

// The eight-digit numeric form.
int ucn_U = a \U0000229E b;
// ON:      identifier 'ucn_U'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\\U0000229E'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'ucn_U'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\\U0000229E'
// OFF-NEXT: identifier 'b'

// The C++23 delimited numeric form.
int ucn_delim = a \u{229E} b;
// ON:      identifier 'ucn_delim'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\\u{229E}'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'ucn_delim'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\\u{229E}'
// OFF-NEXT: identifier 'b'

// The C++23 named form.  This is the spelling U11 exists for: it stays
// writable and legible where the glyph is tofu.
int ucn_named = a \N{SQUARED PLUS} b;
// ON:      identifier 'ucn_named'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\\N{SQUARED PLUS}'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'ucn_named'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\\N{SQUARED PLUS}'
// OFF-NEXT: identifier 'b'

// Adjacency: no whitespace needed, in either spelling.  (See the note above --
// -dump-tokens cannot distinguish "the identifier stopped" from "the recovery
// path was disabled", so the load-bearing assertion is the unittest's.)
int adj = a\U0000229Eb;
// ON:      identifier 'adj'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\\U0000229E'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'adj'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\\U0000229E'
// OFF-NEXT: identifier 'b'

// Two operators in a row, spelled differently, are still two tokens: the
// spelling never participates in maximal munch because every operator is
// exactly one code point (U1).
int two = a \u229E\N{CIRCLED TIMES} b;
// ON:      identifier 'two'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\\u229E'
// ON-NEXT: user_operator '\\N{CIRCLED TIMES}'
// ON-NEXT: identifier 'b'

// A UCN inside a literal is a UCN inside a literal under both flag states --
// the operator classification sits on the token-formation path, which literal
// bodies never reach.
const char *s = "\u229E\N{SQUARED PLUS}";
// ON:      identifier 's'
// ON-NEXT: equal '='
// ON-NEXT: string_literal
// OFF:      identifier 's'
// OFF-NEXT: equal '='
// OFF-NEXT: string_literal

// A UCN naming an *excluded* code point is not a way in.  U+2212 MINUS SIGN is
// a named exclusion (confusable with '-'), so it must never alias '-' and must
// never lex as an operator, however it is spelled.  U05 owns the *reason* in
// the message; U04 only has to keep the code point out -- and it must stay out
// of an identifier too, or the exclusion would merely have moved.
int excl = a \u2212 b;
int excl_named = a \N{MINUS SIGN} b;
// ON-NOT: user_operator '\\u2212'
// ON-NOT: user_operator '\\N{MINUS SIGN}'
//
// The security property (U§10): U+2212 must never *alias* '-'. If either
// spelling had produced an operator token the parser would name it, so the
// absence of `operator−` anywhere in the diagnostics is the assertion, and
// the two errors are the ones an unlexable token gets.
// EXCL-NOT: 'operator−'
// EXCL: error: expected ';' after top level declarator
// EXCL: error: expected ';' after top level declarator
// EXCL-NOT: 'operator−'
//
// Note the asymmetry, which is upstream's and not this feature's, and which
// U05 has to decide about: a *glyph* that is neither an operator nor an
// identifier character is diagnosed by the lexer (`unexpected character
// '−' U+2212`) and dropped, while the same code point spelled as a UCN
// becomes tok::unknown with no lexer diagnostic at all -- because
// LexUnicodeIdentifierStart may only "drop the character" when it was spelled
// as a literal character, the standard requiring that an explicit UCN not be
// thrown away.  So an excluded code point is never silently *accepted* in
// either spelling, but only one spelling gets a message about the code point.
