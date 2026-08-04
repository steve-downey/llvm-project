// RUN: %clang_cc1 -std=c++23 -funicode-operators -dump-tokens %s 2>&1 | FileCheck --check-prefix=ON %s
// RUN: %clang_cc1 -std=c++23 -dump-tokens %s 2>&1 | FileCheck --check-prefix=OFF %s
// RUN: not %clang_cc1 -std=c++23 -fsyntax-only %s 2>&1 | FileCheck --check-prefix=DIAG %s

// U03: a code point in the frozen U1 set lexes as a single tok::user_operator
// under -funicode-operators, and exactly as upstream without it.  -dump-tokens
// stops after phase 4, so this file tests lexing only -- there is no parse for
// tok::user_operator yet.  Each group is anchored on its unique declared name.
//
// NOTE: -dump-tokens is a "strictly preprocessor" action, so it runs with
// Preprocessor::isPreprocessedOutput() set.  That suppresses the lexer's
// absorb-the-stray-character-into-the-identifier recovery, which means this
// file cannot show the a<op>b adjacency case as an ordinary compile sees it.
// LexerTest.UnicodeOperatorAdjacency in clang/unittests/Lex covers that.

int a, b;

// One operator between two operands.
int c = a ⊞ b;
// ON:      identifier 'c'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\342\212\236'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'c'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\342\212\236'
// OFF-NEXT: identifier 'b'

// Adjacency: no whitespace needed.  No U1 code point is XID_Continue (U10), so
// the identifier lexer already stops in front of it and resumes after it.
int d = a⊞b;
// ON:      identifier 'd'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\342\212\236'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'd'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\342\212\236'
// OFF-NEXT: identifier 'b'

// Two operators in a row are two tokens: every operator is one code point and
// none is a prefix of another (U1), so there is no maximal-munch question.
int e = a ⊞⊗ b;
// ON:      identifier 'e'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\342\212\236'
// ON-NEXT: user_operator '\342\212\227'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'e'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\342\212\236'
// OFF-NEXT: unknown '\342\212\227'
// OFF-NEXT: identifier 'b'

// An arrow, from a different U1 block.
int f = a ↦ b;
// ON:      identifier 'f'
// ON-NEXT: equal '='
// ON-NEXT: identifier 'a'
// ON-NEXT: user_operator '\342\206\246'
// ON-NEXT: identifier 'b'
// OFF:      identifier 'f'
// OFF-NEXT: equal '='
// OFF-NEXT: identifier 'a'
// OFF-NEXT: unknown '\342\206\246'
// OFF-NEXT: identifier 'b'

// Literals, comments and raw strings are untouched under both flag states:
// their bodies never reach the non-ASCII slow path of LexTokenInternal.
const char *s = "⊞ ⊗ ↦";
// ON:      identifier 's'
// ON-NEXT: equal '='
// ON-NEXT: string_literal '\"\342\212\236 \342\212\227 \342\206\246\"'
// OFF:      identifier 's'
// OFF-NEXT: equal '='
// OFF-NEXT: string_literal '\"\342\212\236 \342\212\227 \342\206\246\"'

const char *r = R"(⊞ ⊗ ↦)";
// ON:      identifier 'r'
// ON-NEXT: equal '='
// ON-NEXT: string_literal 'R\"(\342\212\236 \342\212\227 \342\206\246)\"'
// OFF:      identifier 'r'
// OFF-NEXT: equal '='
// OFF-NEXT: string_literal 'R\"(\342\212\236 \342\212\227 \342\206\246)\"'

char k = '⊞';
// ON:      identifier 'k'
// ON-NEXT: equal '='
// ON-NEXT: char_constant ''\342\212\236''
// OFF:      identifier 'k'
// OFF-NEXT: equal '='
// OFF-NEXT: char_constant ''\342\212\236''

// line comment ⊞ ⊗ ↦
/* block comment ⊞ ⊗ ↦ */
int g;
// ON:      identifier 'g'
// ON-NEXT: semi ';'
// ON-NEXT: eof
// ON-NOT:  user_operator
// OFF:      identifier 'g'
// OFF-NEXT: semi ';'
// OFF-NEXT: eof

// Without the flag the code points reach the existing stray-character path
// verbatim -- this is the diagnostic U05 must stay consistent with.
// DIAG: error: unexpected character '⊞' U+229E
// DIAG: error: unexpected character '⊗' U+2297
// DIAG: error: unexpected character '↦' U+21A6
