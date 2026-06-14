# S10 — clang-format for both backtick uses

**Goal.** Format both uses correctly without disturbing JavaScript template
literals.

**Depends on:** S09 (both uses exist).
**Design refs:** §7; §3 D8; §12.

## Do
1. **Guard the JS conflict first.** clang-format already lexes backtick for
   JS template strings. Ensure all new logic is conditioned on the C++
   language (`Style.Language == LK_Cpp` / LangOpts) and that JS template-
   string handling is untouched. Add a JS regression test that still passes.
2. **Annotate** the open/close backticks (new `TT_` roles) in
   `TokenAnnotator`, recognizing both the infix pair and the escape pair by
   the same position logic the parser uses.
3. **Spacing** (`spaceRequiredBefore`/`Between`): proposed canonical style —
   spaces outside the pair, hug the contents inside.
4. **Break policy (D8):** hard `CanBreakBefore = false` immediately after
   the open and before the close; inside the operator slot, allow breaks but
   add a small `SplitPenalty` bump (mildly disfavored, not forbidden).
5. Tests in `clang/unittests/Format/`.

## Build / Verify (gate)
- `FormatTests` (confirm target name from S00) green, including new C++
  cases and an unchanged JS template-literal case.
- `check-clang` green.

## Done when
Both uses format sensibly; JS is provably unaffected.

## Capture in handoff
The `TT_` role names and any place the token-based formatter couldn't
reproduce the parser's position decision (note for the paper).
