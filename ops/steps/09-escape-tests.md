# S09 — Escape test sweep incl. mangling/linkage

**Goal.** Prove the ABI claim and full usage surface.

**Depends on:** S08.
**Design refs:** §12 (ABI); §3 D10.

## Do — add tests covering
1. Declaration, definition, and call of a keyword-named function via escape.
2. Member function named with an escaped keyword; access via `.`/`->`.
3. **Mangling:** `-emit-llvm`/`llvm-cxxfilt` shows the external/mangled name
   is the plain identifier (e.g. a function `` `new` `` mangles as a normal
   `new`). This is the ABI-preservation evidence.
4. Cross-check that a TU declaring `` `new` `` and another referring to the
   same plain symbol link (symbol-name identity).
5. The nested infix-callee paren case from S07.

## Verify (gate)
All pass; `check-clang` green.

## Done when
Usage surface + mangling evidence are pinned.

## Capture in handoff
The exact mangled symbol observed — useful as a figure in the paper.
