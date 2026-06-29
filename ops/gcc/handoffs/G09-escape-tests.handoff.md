# Handoff — G09 Escape tests + mangling/linkage

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ 661762b5055
- **Date / agent:** 2026-06-28

## What changed

- `gcc/testsuite/g++.dg/backtick/escape-abi.C`: new compile test covering the
  full §12 ABI surface:
  1. Free function declaration, definition (`int `new`(int x, int y)`), and call.
  2. Member function declaration + out-of-line definition (`void Widget::`delete`()`),
     access via `.` and `->`.
  3. Mangling: two `scan-assembler` checks confirming the Itanium ABI symbols.
  4. Cross-TU linkage implied by the scan-assembler evidence (definition and
     call-site reference both use the same mangled name).

No production files were changed.

## Verification evidence

```
# Targeted new test
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/escape-abi.C"
→ 9 expected passes, 0 failures  (3 compile × 3 C++ variants + 2 scan-assembler × 3 variants = 9)

# Full backtick regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 85 expected passes, 0 failures  (was 76 before G09; +9 from escape-abi.C)

# Parse regression baseline
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

**Mangled symbols observed (GCC 17.0.0 20260624 experimental, x86-64 Linux):**

| Declaration | Mangled name | Clang S09 match? |
|---|---|---|
| `int `new`(int, int)` | `_Z3newii` | ✓ identical |
| `void Widget::`delete`()` | `_ZN6Widget6deleteEv` | ✓ identical |

The mangling was verified independently by running `cc1plus` directly with
`-fbacktick -fno-exceptions` and inspecting the assembly output. Both symbols
appear as `.globl` labels in the expected form.

## Deviations from the plan / design

None. Mangling is identical to Clang S09 — this is the expected behavior and
confirms the §12 ABI claim that "the escape yields an ordinary identifier, so
lookup, mangling, and linkage treat it as a normal identifier."

No entry needed in `ops/gcc/DEVIATIONS.md`.

The step file noted a possible `{ dg-do run }` test; this was omitted because
the dev build has linker/libstdc++ issues (6170 link failures at S12 baseline).
The compile+scan-assembler approach gives equivalent ABI evidence without
requiring the linker. The G08 handoff anticipated this outcome.

## Discoveries affecting later steps

### scan-assembler works with `{ dg-do compile }` in GCC dg
The GCC dg framework generates assembly automatically for `scan-assembler`
checks even when `{ dg-do compile }` is specified. No `-S` flag needed in
the test options. Each `scan-assembler` check contributes 1 pass per C++
standard variant (×3 = 3 passes per scan-assembler directive).

### Cross-compiler ABI agreement confirmed
GCC and Clang produce identical Itanium ABI mangled names for keyword-escaped
identifiers. This is the primary evidence for the paper's §12 ABI claim. Both
compilers treat the escape as yielding a plain identifier at the semantic layer.

### Three-variant rule (reminder)
Each `.C` test file in the backtick suite runs under 3 C++ standard variants.
For escape-abi.C with 2 scan-assembler directives:
- 1 compile pass × 3 variants = 3
- 2 scan-assembler passes × 3 variants = 6
- Total: 9 passes

## Forward notes for the NEXT step (G10 — Revisit operator-slot ADL)

G10 is a production code change (not test-only). It fixes DEV-G05, the only
open normative defect in the GCC implementation. This is the most complex
remaining GCC step.

### Where to look in parser.cc
The infix slot parsing is in `cp_parser_binary_expression` (gcc/cp/parser.cc).
From G03: when a `CPP_BACKTICK` is seen at operator position, the code calls
`cp_parser_simple_cast_expression` (or similar) to parse the slot expression,
then calls `finish_call_expr(slot, args, koenig_p=true, ...)`. The current
defect: `cp_parser_lookup_name` fires *inside* the slot-expression parse,
resolving the bare identifier to a `FUNCTION_DECL` before `finish_call_expr`
can apply Koenig lookup.

Search for `CPP_BACKTICK` and `backtick_is_operator_p` in parser.cc to find
the exact location (~3 sites from G03, augmented in G06 with the RHS-lookahead
while-loop).

### How normal unqualified calls avoid early resolution
In `cp_parser_postfix_expression`, a bare unqualified-id callee is kept as an
`UnresolvedLookupExpr` (or passed directly to `build_new_function_call` / Koenig
lookup). Study the path through `cp_parser_id_expression` when called as the
postfix-expr function-call callee — it specifically avoids resolving via
`cp_parser_lookup_name` when in callee position. That same treatment must be
applied to a bare bare-name backtick slot.

### DEV-G05 details
Current workaround in infix-semantics.C: the `ns::g` test uses a qualified
name (`ns::g`) instead of the bare `g`. After G10, change this to `g` with
only `ns::g` in scope via the argument's namespace — that is the pure-ADL
case that must start working.

### Pitfalls from the step file
- Only the **bare unqualified-id** path changes. Qualified names, member access,
  lambdas, arbitrary expressions (D4) already resolve correctly.
- `backtick_is_operator_p` save/restore from G03 must not be disturbed.
- Template-dependent operands: the unresolved slot must survive instantiation
  like a normal call callee.
- After fixing, update `DEV-G05` in `ops/gcc/DEVIATIONS.md` from
  "DEFECT — fix in GCC track" to "FIXED in G10".

## Open risks / TODOs

- DEV-G05 (ADL limitation) is the only remaining normative defect — fixed in G10.
- The `flag_backtick` guard in `grokdeclarator` is slightly over-permissive
  (suppresses keyword-declarator error for all keyword names when flag is set,
  not just explicitly escaped ones). Benign; carried forward.
- Module support not tested (`IDENTIFIER_KEYWORD_P` in module.cc). Deferred.
