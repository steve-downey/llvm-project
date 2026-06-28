# GCC deviation ledger

Same purpose as `ops/DEVIATIONS.md`, for the GCC track. Additionally record
**cross-compiler divergences**: anywhere GCC and Clang had to differ in
accepted programs, diagnostics, or behavior. Those differences are exactly
what CWG/EWG ask about and belong in the paper's implementation-experience
section.

| ID | Step | Design §/Clang ref | What differed | Cross-compiler note | Recommended doc change |
|----|------|--------------------|---------------|---------------------|------------------------|
| DEV-G04 | G04 | §3 D3; Clang DEV-04 | Bare nested backtick `x \`f \`g\` h\` y` cannot be detected and rejected: after parsing slot `f`, the next CPP_BACKTICK is indistinguishable from a close backtick, so it is consumed as the close. Result silently parses as two chained operators `h(f(x,g),y)`. Matches Clang DEV-04 exactly. | Cross-compiler match: both compilers silently accept. D3 enforcement requires lookahead or two-pass. Deferred. | §3 D3 note: enforcement is aspirational; both impls defer. |
