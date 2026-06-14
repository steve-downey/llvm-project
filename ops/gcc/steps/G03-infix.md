# G03 — Infix parse + desugar (keystone)

**Goal.** `x `f` y` compiles and runs as `f(x, y)`; left-assoc; highest binary
precedence; desugar-only (no new tree code).

**Depends on:** G02.
**Design refs:** §2; §3 D1/D2/D4/D6; §4 Option A; §5; §8 steps 2–3.

## Do
1. **Terminator flag.** Add `backtick_is_operator_p` to `cp_parser`
   (`gcc/cp/parser.cc`/`.h`), modeled on `greater_than_is_operator_p`, with
   save/restore around the operator slot; restored (true) inside nested
   parens/brackets so D3 parenthesised nesting works.
2. **Parse.** In the operator loop of `cp_parser_binary_expression`, special-
   case `CPP_BACKTICK` at the highest precedence (it is not a plain
   precedence-table row because of the delimited slot): consume `` ` ``; with
   `backtick_is_operator_p` false, parse the slot as an assignment-expression;
   expect closing `` ` ``; parse the RHS operand at cast-expression level
   (Option A); build the call.
3. **Desugar.** Build with `finish_call_expr (slot, &args,
   /*disallow_virtual=*/false, /*koenig_p=*/true, complain)` so overload
   resolution and ADL apply. No new tree code.

## Build / Verify (gate)
- `g++.dg` run-test: `int f(int,int); ... a `f` b` returns `f(a,b)`.
- `-fdump-tree-original` shows a CALL_EXPR to `f`.
- Left-assoc: `a `f` b `g` c` builds `g(f(a,b), c)`.
- Baseline green.

## Done when
End-to-end infix works; off-flag unchanged.

## Capture in handoff
Where in `cp_parser_binary_expression` you hooked, the `backtick_is_operator_p`
save/restore sites, and the `finish_call_expr` argument shape. G07 reuses the
parser; G05/G06 test it.

## Pitfalls
- Don't let the slot parse eat the closing backtick — test `a `f` b `g` c`
  immediately.
- Pass `koenig_p` so unqualified slot callees get ADL like a normal call.
