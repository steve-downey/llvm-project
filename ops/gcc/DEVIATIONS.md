# GCC deviation ledger

Same purpose as `ops/DEVIATIONS.md`, for the GCC track. Additionally record
**cross-compiler divergences**: anywhere GCC and Clang had to differ in
accepted programs, diagnostics, or behavior. Those differences are exactly
what CWG/EWG ask about and belong in the paper's implementation-experience
section.

| ID | Step | Design §/Clang ref | What differed | Cross-compiler note | Recommended doc change |
|----|------|--------------------|---------------|---------------------|------------------------|
| DEV-G04 | G04 | §3 D3; Clang DEV-04 | Bare nested backtick `x \`f \`g\` h\` y` cannot be detected and rejected: after parsing slot `f`, the next CPP_BACKTICK is indistinguishable from a close backtick, so it is consumed as the close. Result silently parses as two chained operators `h(f(x,g),y)`. Matches Clang DEV-04 exactly. | Cross-compiler match: both compilers silently accept. D3 enforcement requires lookahead or two-pass. Deferred. | §3 D3 note: enforcement is aspirational; both impls defer. |
| DEV-G05 | G05 | §8 point 3 ("inheriting ADL") | Pure ADL — where the function is *only* visible in a namespace and not at file scope — does NOT work. The backtick slot is parsed as a standalone assignment-expression; `cp_parser_lookup_name` fires on the slot identifier before `finish_call_expr` can apply Koenig. If the name is not in scope, parsing fails with "not declared in this scope". ADL *augmentation* (function in scope globally + ADL adds namespace overload) also fails because `slot` is already a resolved `FUNCTION_DECL`, not an overload set, so `finish_call_expr(koenig_p=true)` has no set to augment. | Cross-compiler difference from Clang: Clang's `BuildCallExpr` receives an `UnresolvedLookupExpr` from Sema, enabling full ADL. GCC resolves the name at parse time. Tested with qualified name (`ns::g`) instead. | §8 note: GCC ADL for backtick is parse-time-limited; a future step could treat a bare-name slot as a Koenig-eligible identifier. |
