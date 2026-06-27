# Handoff — S08 Tentative-parse / decl-vs-expr integration

- **Status:** DONE (gate passed)
- **Branch / commit:** `backtick` @ TBD (update after commit)
- **Date / agent:** 2026-06-27

## What changed

- `clang/lib/Parse/ParseTentative.cpp` — modified `TryParseDeclarator` (line ~901):
  - Extended the outer `if` condition to include `|| (getLangOpts().Backtick && Tok.is(tok::backtick))`.
  - Added `bool IsBacktickEscape = false;` inside the block.
  - Added `else if (getLangOpts().Backtick && Tok.is(tok::backtick))` branch that:
    1. Consumes the opening backtick token.
    2. Pushes `Tok.getIdentifierInfo()` to `TentativelyDeclaredIdentifiers` (the keyword's IdentifierInfo).
    3. Consumes the keyword token.
    4. Consumes the closing backtick token (if present).
    5. Sets `IsBacktickEscape = true`.
  - Wrapped the final `if (Tok.is(tok::kw_operator)) { ... } else ConsumeToken();` in
    `if (!IsBacktickEscape) { ... }` to avoid double-consuming after the 3-token escape sequence.
- `clang/test/Parser/backtick-escape-tentative.cpp` — new test file covering:
  - `T `new`;` parsed as VarDecl (declaration context).
  - `` `new`(1, 2); `` parsed as CallExpr (expression context).

## Verification evidence

```
# New targeted test
llvm-lit -v clang/test/Parser/backtick-escape-tentative.cpp
→ PASS: Clang :: Parser/backtick-escape-tentative.cpp (1 of 1)

# All prior backtick tests (regression check)
llvm-lit -v clang/test/Parser/backtick-escape.cpp \
             clang/test/Parser/backtick-escape-diagnostics.cpp \
             clang/test/Parser/backtick-infix.cpp \
             clang/test/Parser/backtick-diagnostics.cpp \
             clang/test/Parser/backtick-precedence.cpp
→ All 5 PASS

# check-clang
52218 total (+1 vs S07): 46453 passed, 26 xfail, 5732 unsupported, 6 skipped, 1 failed
Failed (environmental baseline): Clang :: Format/dump-config-objc-stdin.m
```

## Deviations from the plan / design

None. The change is exactly as described in the S07 handoff forward notes. The tentative parser
doesn't validate the keyword-only policy (that's left to the real parse paths established in S07);
it merely recognizes the 3-token sequence as a valid declarator-id candidate.

## Discoveries affecting later steps

### TentativelyDeclaredIdentifiers receives the keyword's IdentifierInfo
`Tok.getIdentifierInfo()` on a keyword token returns the keyword's `IdentifierInfo` (non-null for
any reserved keyword). The guard `if (Tok.getIdentifierInfo())` is safe but always true for C++
keywords. Downstream lookups keyed on `IdentifierInfo` for tentatively declared names (e.g. in
`isTentativelyDeclared`) work correctly because the escaped identifier and the regular identifier
share the same `IdentifierInfo` pointer.

### No changes needed to `isCXXDeclarationStatement` or `isDeclarationSpecifier`
These drive into `TryParseSimpleDeclaration` → `TryParseDeclarator`. Once `TryParseDeclarator`
recognizes backtick as a valid declarator-id, the full chain works without further surgery.

### The other `tok::identifier` checks in ParseTentative.cpp are not relevant for S08
- Line 210 (elaborated-type-specifier name after `struct/class/union`): that position is the
  type name, not the variable name being declared. No change needed for `struct `new` x;` style —
  that's an uncommon usage not targeted by S08.
- Lines 860, 1061, 1673, 1788: all in unrelated disambiguation paths (user-defined suffix,
  parameter, protocol qualifiers). No changes needed.

## Forward notes for the NEXT step (S09 — Escape test sweep + mangling check)

S09 adds tests for the full usage surface and ABI evidence. Key facts:

### Mangling: escaped keyword is the plain identifier
The AST stores the identifier by its underlying name (e.g. `new` not `` `new` ``). The mangled
name for `int `new`(int, int)` is exactly the same as if you could write `int new(int, int)` in
C — use `-emit-llvm` and `llvm-cxxfilt` to verify. For a global `int `new`(int, int)` at
namespace scope in C++17, the mangled name will be `_Z3newii` (standard name mangling for
`int new(int, int)`). Run:
```bash
$B/bin/clang -std=c++17 -fbacktick -emit-llvm -S -o - file.cpp | grep "define.*new"
```

### Test structure recommendation
Use a single `backtick-escape-mangling.cpp` or expand into `backtick-escape-abi.cpp`:
- Declare + define + call a keyword-named function at namespace scope.
- Check the IR for the mangled symbol name via `FileCheck --check-prefix=IR`.
- For the two-TU linkage test, two RUN lines with `-emit-llvm -S` then grep for symbol name identity.

### Member function with escaped keyword
```cpp
struct S {
  void `delete`();
};
void S::`delete`() {}
void test() { S s; s.`delete`(); }
```
S07's `ParseUnqualifiedId` hook handles `` `delete` `` in `S::`delete`()` and `s.`delete`()`.
Confirm these round-trip through AST dump as `delete`.

### The D3 nested-paren callee case
Already covered in `backtick-escape.cpp` from S07 — no need to duplicate in S09. Cross-reference
it from the S09 test if desired.

## Open risks / TODOs

- `err_backtick_nested_requires_parens` in DiagnosticParseKinds.td remains dead code from S04.
- DEV-04 (bare nested backtick silently mis-parses) still unaddressed.
- S09 should cover member-function syntax for the paper's ABI claim.
