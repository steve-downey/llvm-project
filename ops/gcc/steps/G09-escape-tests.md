# G09 — Escape tests + mangling/linkage

**Goal.** Prove the ABI claim and full usage surface in GCC; confirm identical
mangling to Clang S09.

**Depends on:** G08.
**Design refs:** §12 (ABI); §3 D10.

## Do — `g++.dg` tests covering
1. Declaration, definition, call of a keyword-named function via escape.
2. Member function named via escape; access via `.`/`->`.
3. Mangling: assert the external symbol is the plain identifier (scan-assembler
   / `nm` / `c++filt`). It must match the Itanium mangling Clang produced in S09.
4. Cross-TU link by the plain symbol name.
5. The nested infix-callee paren case from G07.

## Verify (gate)
All pass; baseline green.

## Capture in handoff
The mangled symbol observed; confirm it is identical to Clang's (a key
cross-compiler agreement point for the paper). Log any difference as a
high-priority deviation.
