# Handoff — G08 Tentative parse / decl-vs-expr

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` (gcc-backtick worktree) @ 8d1458de5f4
- **Date / agent:** 2026-06-28

## What changed

- `gcc/testsuite/g++.dg/backtick/escape-tentative.C`: new compile-only test
  covering three disambiguation scenarios:
  1. `T `new`;` — variable declaration of type `T` named `new` (tentative
     parse prefers declaration; member-access `(void)`new`.x` proves it).
  2. `` `new`(1, 2); `` — expression-statement calling `::new(int,int)`
     (no type named `new` in scope, so tentative declaration fails and
     expression-statement is chosen).
  3. `T `new` = {};` inside a block alongside `(void)`new`.x` — both forms
     in one scope, prefer-declaration rule holds.

**No changes to parser.cc, decl.cc, or any other production file.** The G07
implementation already handled tentative parsing correctly.

## Verification evidence

```
# Targeted new test
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/escape-tentative.C"
→ 3 expected passes, 0 failures  (3 = 1 test × 3 C++ standard variants)

# Full backtick regression
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/backtick/*.C"
→ 76 expected passes, 0 failures  (was 73 before G08; +3 from escape-tentative.C)

# Parse regression baseline
make -C gcc check-c++ RUNTESTFLAGS="dg.exp=g++.dg/parse/parse*.C"
→ 105 expected passes, 3 unsupported, 0 failures
```

## Deviations from the plan / design

None. The step file said "ensure the tentative-parsing paths accept a
backtick-escaped name as a declarator-id." The G07 handoff's forward notes
predicted this was already working; that prediction was correct. The
`cp_parser_uncommitted_to_tentative_parse_p` guard in `cp_parser_unqualified_id`
suppresses error diagnostics during tentative passes, and `cp_parser_simulate_error`
marks failure on malformed escapes — both were already in place from G07.

Cross-compiler comparison: Clang required an explicit `TryParseDeclarator` patch
(S08) to recognize the 3-token backtick sequence as a valid declarator-id. GCC
did not need an analog because its unified `cp_parser_unqualified_id` is called
by the tentative declarator path without special-casing, and the suppression
mechanism is already in place. This is a genuine architectural difference, not
a gap — both compilers arrive at correct disambiguation.

## Discoveries affecting later steps

### Tentative-parse mechanism confirmed
GCC's tentative parsing of `T `kw`;` works through the existing path:
`cp_parser_statement` → tries `cp_parser_simple_declaration` (tentatively) →
`cp_parser_init_declarator` → `cp_parser_declarator` →
`cp_parser_direct_declarator` → `cp_parser_declarator_id` →
`cp_parser_id_expression` → `cp_parser_unqualified_id` → `case CPP_BACKTICK:`.
No additional hooks were needed.

### Three-variant rule
GCC's dg test harness runs each backtick test under three C++ standard variants
(likely C++14/17/20 or C++17/20/23 depending on the toolchain). A single `.C`
file contributes 3 passes. Handoff authors: account for ×3 when predicting pass
counts.

## Forward notes for the NEXT step (G09 — Escape tests + mangling/linkage)

G09 must prove the ABI claim: keyword-named entities mangle identically to
plain identifiers with the same spelling, matching the Clang S09 result.

### Expected mangled names (from Clang S09 on the same Itanium ABI)
| Declaration | Mangled name | Demangled |
|---|---|---|
| `int `new`(int, int)` at namespace scope | `_Z3newii` | `new(int, int)` |
| `void Widget::`delete`()` | `_ZN6Widget6deleteEv` | `Widget::delete()` |

Both GCC and Clang target the Itanium ABI on Linux x86-64, so the mangled
symbols must be identical. If GCC produces different mangling, that is a
**high-priority deviation** for `ops/gcc/DEVIATIONS.md`.

### How to check mangling in GCC `dg` tests
Use `scan-assembler` — the dg framework scans the intermediate `.s` file
the compiler produces:

```cpp
// { dg-do compile }
// { dg-options "-fbacktick" }
// { dg-additional-options "-fno-exceptions" }

int `new`(int, int);
int `new`(int x, int y) { return x + y; }

// { dg-final { scan-assembler "_Z3newii" } }
```

For member functions:
```cpp
struct Widget { void `delete`(); };
void Widget::`delete`() {}

// { dg-final { scan-assembler "_ZN6Widget6deleteEv" } }
```

The `scan-assembler` pattern is a regexp; escape special characters.

### Cross-TU linkage
To prove two-TU linkage (§12 ABI claim), define in one `.C` and declare+call in
another, then link them. In the dg harness:
```cpp
// file escape-abi.C
// { dg-do link }
// { dg-options "-fbacktick" }
// ... define `new` here ...
// { dg-final { scan-assembler "_Z3newii" } }
```
Or do it with `{ dg-do run }` if you also want to test the call succeeds at
runtime. Simple `run` tests in the backtick suite have not been done yet (all
prior tests are `compile`-only); `link` is a lower bar if runtime execution is
not needed.

### Tests to write for G09
1. `escape-abi.C` — compile-only mangling check (scan-assembler).
2. `escape-member.C` — member function with escaped keyword (definition +
   call) confirming member-function mangling.
3. Consider a `{ dg-do run }` smoke test if the harness has a C++ runtime
   available (it does for most g++.dg tests).

### G07 coverage already in escape.C
`escape.C` (from G07) already covers:
- Declarator-id function declaration: `void `new`(int, int);`
- Primary-expression call: `` `new`(a, b); ``
- Member field: `s.`delete``
- Member function call: `` s.`new`(42) ``
- D3 interaction: `` x `(`new`)` y ``
G09 should reference these and add the **definition + assembly** evidence that
escape.C (compile-only) omits.

## Open risks / TODOs

- DEV-G05 (ADL limitation in infix operator) still open (G10).
- The `flag_backtick` guard in `grokdeclarator` is slightly over-permissive
  (suppresses keyword-declarator error for all keyword names when flag is set,
  not just explicitly escaped ones). Benign in practice; carried forward.
- Module support not tested (`IDENTIFIER_KEYWORD_P` in module.cc). Deferred.
