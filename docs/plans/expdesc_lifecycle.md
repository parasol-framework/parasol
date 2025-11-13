# ExpDesc Lifecycle Audit

## Overview of ExpDesc Handling
- `ExpDesc` stores the parser state for expressions. `expr_init()` resets the kind, primary info, flags, and jump lists whenever a new expression descriptor is created.【F:src/fluid/luajit-2.1/src/parser/lj_parse_types.h†L39-L107】
- Register allocation helpers (`expr_discharge()`, `expr_toanyreg()`, `expr_free()`, etc.) transition descriptors between relocatable instructions and concrete registers while leaving flag bookkeeping to higher-level code.【F:src/fluid/luajit-2.1/src/parser/lj_parse_regalloc.c†L55-L146】
- Safe-navigation helpers (`expr_safe_nav_branch()` and friends) rely on `SAFE_NAV_CHAIN_FLAG` to remember that the resulting value lives in a scratch register reserved for optional chaining semantics.【F:src/fluid/luajit-2.1/src/parser/lj_parse_types.h†L57-L82】【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L99-L213】 Binary-operator setup consumes that flag when the value is passed into the `?:` operator.【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L120-L190】

## Findings

### 1. Ternary `?:` path drops descriptor metadata
When the parser recognises the ternary flavour of the `?:` operator, it hand-rolls the true/false branches inside `expr_binop()`. After evaluating both branches, it overwrites only `v->u.s.info` and `v->k`, leaving the original condition's `flags`, jump lists, and auxiliary payload in place.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L1002-L1089】 As a result:
- Safe-navigation or postfix metadata from the condition can leak into the ternary result, so later consumers (e.g. optional chaining cleanup) believe the value still lives in the original scratch register.
- Metadata from the actual branch result (`fexp`) is discarded, so any flags it needed (for example, when the branch itself produces a safe-navigation result) never propagate to the combined expression.

This breaks the expected lifecycle where a new expression value should carry its own descriptor state. The fix is to copy the full descriptor for the selected branch back into `*v` (or explicitly reset the stale fields) before continuing.

### 2. Shift/bitwise chaining never clears safe-navigation state
`expr_shift_chain()` reuses the left-hand descriptor while chaining multiple bitwise operators. It simply updates `lhs->k` and `lhs->u.s.info` to point at the reused base register and defers to `bcemit_shift_call_at_base()` for emission.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L900-L939】【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L226-L257】 Neither function clears `lhs->flags`, so any `SAFE_NAV_CHAIN_FLAG` carried by the original operand survives even though the result is now produced by a separate bit library call. If that flagged value subsequently flows into the optional operator, `bcemit_binop_left()` treats it as a safe-navigation temporary and applies the wrong register retention policy.【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L120-L190】

To keep the lifecycle consistent, the shift helpers should clear (or rebuild) the descriptor state once the call result replaces the original operand.

### 3. Method lookup keeps stale safe-navigation metadata
`bcemit_method()` transforms an object expression into the callee slot that will receive the looked-up method. Before this change it only overwrote `e->k` and `e->u.s.info`, which left any flags and jump lists from the original object descriptor intact. When the object was the product of safe navigation, the lingering `SAFE_NAV_CHAIN_FLAG` marked the callee itself as a safe-navigation temporary, so subsequent operators believed they still had to protect that register. The fix reinitialises the descriptor via `expr_init()`, clearing both the flag bits and any branch lists before the call proceeds.【F:src/fluid/luajit-2.1/src/parser/lj_parse_regalloc.c†L327-L345】

## Recommendations
1. In the ternary branch of `expr_binop()`, assign the full branch descriptor back to `*v` (or manually reset `flags`, `t`, `f`, and `u.s.aux`) before continuing the binary-op loop.
2. When emitting chained bitwise operations, explicitly clear `lhs->flags` (and other stale fields if necessary) after the call result is discharged so that optional-operator handling sees a clean descriptor.
3. Audit other sites that mutate only part of an `ExpDesc` (e.g. direct `u.s.info` writes) to ensure they either copy the whole structure or consciously reset every lifecycle-sensitive field.
4. Reset descriptor metadata inside `bcemit_method()` so method lookups cannot leak safe-navigation flags into the callee slot.【F:src/fluid/luajit-2.1/src/parser/lj_parse_regalloc.c†L327-L345】

## Audit follow-up
- Verified that `expr_safe_method_call()` deliberately preserves safe-navigation flags while preparing the object operand, then rebuilds the descriptor for the call result, so no extra resets are required there.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L172-L205】
- Confirmed that register-allocation helpers such as `expr_toanyreg()` and `expr_toreg()` already operate on the existing descriptor value and therefore must not wipe `flags`, keeping safe-navigation semantics intact.【F:src/fluid/luajit-2.1/src/parser/lj_parse_regalloc.c†L252-L289】
