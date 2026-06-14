# S01 — Feature flag `-fbacktick`

**Goal.** Introduce the language/driver flag that gates everything. No
syntax behavior yet — just a flag that parses and threads to `LangOptions`.

**Depends on:** S00.
**Design refs:** §3 D5 (gating).

## Do
1. Add a `LangOpt` in `clang/include/clang/Basic/LangOptions.def` (e.g.
   `LANGOPT(Backtick, 1, 0, "backtick operator and identifier escaping")`).
2. Add the driver/frontend flag in `clang/include/clang/Driver/Options.td`
   (`-fbacktick` / `-fno-backtick`), in the `CC1Option` group.
3. Wire parsing in `clang/lib/Frontend/CompilerInvocation.cpp` (and driver
   forwarding if needed) so `-fbacktick` sets `LangOpts.Backtick`.
4. No lexer/parser changes.

## Build
`ninja -C build clang`

## Verify (gate)
- `clang -fbacktick -x c++ -fsyntax-only` on an empty file succeeds.
- `clang -fbacktick -### ...` shows the flag forwarded to `-cc1`.
- Add a tiny lit test under `clang/test/Driver/` asserting the flag is
  accepted and forwarded; `check-clang` stays green.

## Done when
The flag exists, parses, sets the LangOpt, and changes no behavior.

## Capture in handoff
The **exact LangOpt name** and the **exact spelling** of the flag — S02+
will gate on `getLangOpts().<Name>`. Note whether you used one flag for both
features or reserved a second (recommendation: one flag now; revisit at S07).

## Pitfalls
Keep it off by default. Don't add it to any default-on language standard.
