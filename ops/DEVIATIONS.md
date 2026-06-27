# Deviation ledger — implementation reality vs. the proven design

Each row is a place the build taught us something the paper didn't know.
The paper author reconciles these into `docs/backtick-operator-design.md`
§3 (decisions log) and the relevant section. This file is the operational
half of "we have merely proved, not tested."

| ID | Step | Design section affected | What the design said | What was true | Recommended doc change |
|----|------|------------------------|----------------------|---------------|------------------------|
| DEV-01 | S00 (for S01) | §6.5; step S01 | Driver flag lives in `clang/include/clang/Driver/Options.td` | `Options.td` moved to `clang/include/clang/Options/Options.td` | Update the path in §6.5 and step S01 |
| DEV-02 | S00 (for S01) | §6.5; step S01 | `LANGOPT(Backtick, 1, 0, "...")` (4 args) | `LANGOPT` now takes 5 args incl. a compatibility kind: `LANGOPT(Name, Bits, Default, Compatibility, Description)`, e.g. `LANGOPT(C99, 1, 0, NotCompatible, "C99")`. 4-arg form won't compile. | Update the LANGOPT example to the 5-arg `NotCompatible` form |
| DEV-03 | S01 | §6.5; step S01 | `BoolFOption` with `BothFlags<[], [ClangOption, CC1Option]>` + marshalling is sufficient for driver→cc1 forwarding | Marshalling alone does NOT forward the flag. The driver parses it (ClangOption) but won't emit it in the cc1 argv without an explicit `Args.addLastArg(CmdArgs, OPT_fbacktick, OPT_fno_backtick)` in `Clang.cpp::ConstructJob()`. Reference: `-freflection` (CC1Option only, no forwarding); `-fsized-deallocation` (also needs explicit `addLastArg`). | §6.5 should note that BoolFOption+marshalling requires explicit addLastArg in Clang.cpp for driver-level visibility |
| DEV-04 | S04 | §3 D3; §6 step 6 | Bare nested backtick `x \`f \`g\` h\` y` "naturally produces a parse error" because BacktickIsOperator=false causes the inner backtick to have prec::Unknown, terminating the slot | The greedy-close parse is ambiguous but NOT an error: the parser treats the inner backtick as the close, giving two chained operators `x \`f\` g \`h\` b` = `h(f(x,g),b)` — silently wrong. Detecting this requires lookahead (checking what follows the apparent close backtick) and was deferred. | §3 D3 should change from "naturally produces a parse error" to "silently mis-desugars unless a special diagnostic check is added; detecting bare nesting requires lookahead past the closing backtick" |
