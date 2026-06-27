# Handoff — S09 Escape test sweep + mangling check

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ `d84f91cbd5d4`
- **Date / agent:** 2026-06-27

## What changed

- `clang/test/Parser/backtick-escape-abi.cpp` — new test file covering the full
  usage surface and ABI evidence required by S09:
  - **Section 1:** Definition + call of `int `new`(int, int)` at namespace scope,
    with AST, IR, and demangled-IR checks.
  - **Section 2:** Member function `void Widget::`delete`()` with `.` and `->`
    access, with AST, IR, and demangled-IR checks.
  - **TU2 simulation:** `#ifdef USE_ONLY` compilation exposes `declare @_Z3newii`
    and `call @_Z3newii` in the same IR; checked with `TU2-DAG:` to prove that
    a defining TU and a using TU would link via the same `_Z3newii` symbol.
- `ops/PLAN.md` — S09 box ticked, status row appended.

## Verification evidence

```
# New targeted test
llvm-lit -v clang/test/Parser/backtick-escape-abi.cpp
→ PASS

# All prior backtick tests (regression check)
llvm-lit -v clang/test/Parser/backtick-escape-abi.cpp \
             clang/test/Parser/backtick-escape.cpp \
             clang/test/Parser/backtick-escape-diagnostics.cpp \
             clang/test/Parser/backtick-escape-tentative.cpp \
             clang/test/Parser/backtick-infix.cpp \
             clang/test/Parser/backtick-diagnostics.cpp \
             clang/test/Parser/backtick-precedence.cpp
→ All 7 PASS

# check-clang
52219 total (+1 vs S08): 46454 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline): Clang :: Format/dump-config-objc-stdin.m
```

## Mangling evidence (figure for the paper)

- `int `new`(int x, int y)` at C++ namespace scope mangles to **`_Z3newii`**.
  `llvm-cxxfilt -n` demangles it as **`new(int, int)`**.
- `void Widget::`delete`()` mangles to **`_ZN6Widget6deleteEv`**.
  `llvm-cxxfilt -n` demangles it as **`Widget::delete()`**.
- Both TU1 (`define @_Z3newii`) and TU2 (`call / declare @_Z3newii`) reference
  the same symbol, confirming §12 ABI-preservation claim.

## Deviations from the plan / design

None. The D3 nested-paren callee case was already pinned in `backtick-escape.cpp`
from S07; S09 did not duplicate it as the handoff suggested.

## Discoveries affecting later steps

### The TU2-DAG ordering issue
In LLVM IR output, `call` instructions inside a function body appear BEFORE the
`declare` statement for the same symbol at module scope. FileCheck sequential
`TU2:` checks would fail if `declare` is checked before `call` in source order.
The fix is `TU2-DAG:` for both, which makes them order-independent. Keep this
in mind for any future IR-order-sensitive checks.

### JS template-string handling is already isolated
`FormatTokenLexer::handleTemplateStrings()` is called only inside
`if (Style.isJavaScript())` at `clang/lib/Format/FormatTokenLexer.cpp:128-130`.
The new C++ backtick logic for S10 will never collide with JS template strings
as long as it is placed outside that guard (in the normal C++ token-processing
path).

## Forward notes for the NEXT step (S10 — clang-format for both backtick uses)

### Where the JS guard lives
`clang/lib/Format/FormatTokenLexer.cpp:128-130` — `handleTemplateStrings()` is
called only when `Style.isJavaScript()`. Any new C++ backtick handling belongs
in the token-processing loop that runs for all languages (or guarded with
`IsCpp` / `Style.Language == FormatStyle::LK_Cpp`).

### TT_ token type registration
New `TT_` roles are declared via the `TYPE(...)` macro in
`clang/lib/Format/FormatToken.h` at the TYPE list starting around line 28.
Suggested names for S10: `TT_BacktickInfixOpen`, `TT_BacktickInfixClose`,
`TT_BacktickEscapeOpen`, `TT_BacktickEscapeClose`. (Open and close backticks
need distinct types so `MatchingParen`-style pairing and break-policy rules can
differ between uses.)

### TokenAnnotator position logic
The parser's position rule ("post-operand position → infix operator; elsewhere →
escape") must be replicated in `TokenAnnotator`. Look at how `tok::star` and
`tok::amp` are classified (unary vs binary) in `TokenAnnotator.cpp` —
`getTypeOfToken()` and the state machine that builds `AnnotatedLine` — as the
closest analog.

### FormatTests target
The unittest target is `FormatTests` (add_distinct_clang_unittest in
`clang/unittests/Format/CMakeLists.txt`). Run with:
```bash
ninja -C $B FormatTests && $B/bin/FormatTests
```
New C++ backtick format tests belong in `clang/unittests/Format/FormatTest.cpp`.
A JS non-regression test (template literals still format correctly) belongs in
`clang/unittests/Format/FormatTestJS.cpp`.

### dump-config-objc-stdin.m is the environmental baseline failure
Always 1 environmental failure in check-clang on this machine. Not a regression.

## Open risks / TODOs

- `err_backtick_nested_requires_parens` in DiagnosticParseKinds.td remains dead
  code from S04 (carried forward from S08).
- DEV-04 (bare nested backtick silently mis-parses) still unaddressed.
- S11 (AST wrapper for `-ast-print` fidelity) has no dependency on S10; both
  can run after S09.
