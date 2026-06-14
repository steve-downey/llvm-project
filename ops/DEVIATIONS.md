# Deviation ledger — implementation reality vs. the proven design

Each row is a place the build taught us something the paper didn't know.
The paper author reconciles these into `docs/backtick-operator-design.md`
§3 (decisions log) and the relevant section. This file is the operational
half of "we have merely proved, not tested."

| ID | Step | Design section affected | What the design said | What was true | Recommended doc change |
|----|------|------------------------|----------------------|---------------|------------------------|
| DEV-01 | S00 (for S01) | §6.5; step S01 | Driver flag lives in `clang/include/clang/Driver/Options.td` | `Options.td` moved to `clang/include/clang/Options/Options.td` | Update the path in §6.5 and step S01 |
| DEV-02 | S00 (for S01) | §6.5; step S01 | `LANGOPT(Backtick, 1, 0, "...")` (4 args) | `LANGOPT` now takes 5 args incl. a compatibility kind: `LANGOPT(Name, Bits, Default, Compatibility, Description)`, e.g. `LANGOPT(C99, 1, 0, NotCompatible, "C99")`. 4-arg form won't compile. | Update the LANGOPT example to the 5-arg `NotCompatible` form |
