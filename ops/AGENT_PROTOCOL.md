# Agent protocol — read this first, every time

You are an independent agent with no prior context. You will do **exactly
one step** and stop. Follow this without improvising on process.

1. **Orient.** Read `ops/PLAN.md` in full. Pick the first unchecked step
   whose dependencies (listed beside it) are all checked. That is *your*
   step. If none qualifies, write nothing and report that the plan is
   blocked or complete.
2. **Load context.**
   - Read your step file `ops/steps/NN-*.md` completely.
   - Read the previous step's handoff in `ops/handoffs/` if one exists.
     Treat its "Forward notes" and "Discoveries" as authoritative — they
     override the step file where they conflict.
   - Read the design-doc sections your step file lists from
     `docs/backtick-operator-design.md`.
3. **Execute** the step's "Do" exactly. Keep the diff minimal and gated
   behind `-fbacktick`. If you must deviate, do the smallest thing that
   works and record why (step 6c).
4. **Gate.** Run the step's verification commands. Do **not** proceed unless
   every gate passes, including `check-clang` if the step requires it.
   - If the gate cannot pass: leave the checkbox unchecked, write a handoff
     with Status **BLOCKED** describing exactly where you stopped and what
     you tried, and STOP.
5. **Record green.** Tick your step's box in `ops/PLAN.md`, append one row
   to its Status log, and commit (`[backtick] SNN: <title>`).
6. **Hand off.** This is the part that makes the chain work:
   a. **Read the *next* step's file** `ops/steps/<next>.md` now.
   b. Copy `ops/HANDOFF_TEMPLATE.md` to `ops/handoffs/NN-<slug>.handoff.md`.
   c. Fill it in: what changed, verification evidence, deviations,
      discoveries — and, having just read the next step, write **specific
      forward notes** for the next agent (exact symbol names you found,
      paths that differed, gotchas, anything that will save them a
      discovery). Vague handoffs break the chain; be concrete.
   d. If anything contradicted the design doc, also append a row to
      `ops/DEVIATIONS.md`.
7. **Stop.** Do not begin the next step.
