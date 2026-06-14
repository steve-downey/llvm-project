# S02 — Lexer: backtick punctuator token

**Goal.** Lex `` ` `` as a dedicated punctuator token when `-fbacktick` is
on; unchanged (today's behavior) when off.

**Depends on:** S01.
**Design refs:** §6 step 1; §3 D6/D10 (single punctuator, no lexer-level
identifier synthesis — do **not** try to recognize escapes here).

## Do
1. Add `PUNCTUATOR(backtick, "`")` to
   `clang/include/clang/Basic/TokenKinds.def`.
2. In the punctuator switch in `Lexer::LexTokenInternal`
   (`clang/lib/Lex/Lexer.cpp`), add a case for `'`'` that mints
   `tok::backtick` **only when `LangOpts.Backtick`**; otherwise fall through
   to the existing behavior.
3. Do nothing about keyword-escape or infix here. This step only produces a
   token. Strings/char-literals/comments/raw-strings are handled earlier in
   the lexer and must remain unaffected.

## Build
`ninja -C build clang`

## Verify (gate)
- Add a lit test (`-dump-tokens` or a Lex unittest) showing `` ` `` →
  `tok::backtick` with `-fbacktick`, and the prior token kind/diagnostic
  without it.
- Confirm a backtick inside a string literal, a comment, and a raw-string
  body is still part of that literal/comment (not a `backtick` token).
- `check-clang` green.

## Done when
The token appears under the flag and nowhere else, with literals/comments
untouched.

## Capture in handoff
Confirm the token kind name (`tok::backtick`). Note what the off-flag
behavior is today (unknown token? specific diagnostic?) so S04 can phrase
diagnostics consistently.

## Pitfalls
Gate the case body, not just emit unconditionally — an ungated token would
regress every TU that legitimately... (there are none in valid C++, but the
off-path must still match upstream exactly, including diagnostics).
