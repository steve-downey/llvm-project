# G01 — Feature flag (`-fbacktick`)

**Goal.** A C++ flag gating the whole feature; default off; no behavior yet.

**Depends on:** S12.
**Design refs:** §3 D5; §8.

## Do
1. Add an option to `gcc/c-family/c.opt`:
   ```
   fbacktick
   C++ Var(flag_backtick) Init(0)
   Enable the backtick infix operator and keyword escaping.
   ```
2. Confirm `flag_backtick` is reachable in the C++ front end. libcpp will need
   to see it in G02; plan to mirror it into a libcpp option there (model on how
   `$`-in-identifiers is gated via a cpp option).
3. No lexer/parser behavior.

## Build / Verify (gate)
- `xg++ -fbacktick -fsyntax-only` on an empty file succeeds; `-fno-backtick`
  too; the option shows in `--help=c++`.
- `g++.dg` baseline subset still green.

## Done when
The flag parses and sets `flag_backtick`, changing nothing.

## Capture in handoff
The exact var name (`flag_backtick`) and how the C++ front end reads it —
G02/G03 gate on it.
