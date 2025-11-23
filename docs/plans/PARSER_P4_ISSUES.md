# Phase 4 Issues and Remediation Plan

## Overview
The Phase 4 goal from `LUAJIT_PARSER_REDESIGN.md` and `PARSER_P4.md` was to modernise operator and statement handling so the AST/allocator/CFG abstractions own register and control-flow management. Verification shows the legacy pathways are still active, so Phase 4 remains incomplete.

## Outstanding problems and required fixes
- **Operator emission still relies on legacy helpers.** `bcemit_arith` and `bcemit_comp` previously decremented `freereg` directly after emission. This has been refactored to release operand registers through `RegisterAllocator`/`ExpressionValue`, removing manual `freereg` manipulation. Remaining work is to push the helpers fully behind allocator/CFG contracts.【F:src/fluid/luajit-2.1/src/parser/parse_operators.cpp†L34-L163】
- **Assignment and value storage remain manual.** Plain and compound assignments call `bcemit_store`, `lex_state.assign_adjust`, and reset `freereg` explicitly instead of flowing through LValue descriptors and allocator-managed slots, so statement nodes still mirror legacy register choreography.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1022-L1108】
- **Loop control and defers patch jumps directly.** `emit_defer_stmt`, `emit_break_stmt`, and `emit_continue_stmt` issue raw `bcemit_jmp` calls and mutate `freereg`/`lex_state` state, bypassing the ControlFlowGraph abstraction and leaving defers, breaks, and continues outside the structured flow model.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L909-L983】

## Action plan
1. **Refactor operator helpers** to expose allocator/CFG-based operations through `OperatorEmitter`, eliminating direct `freereg` manipulation and pushing result ownership into value-category/LValue abstractions.
2. **Rebuild assignment emission** around LValue descriptors and allocator-managed slots so `emit_plain_assignment`/`emit_compound_assignment` no longer call legacy store/adjust helpers or reset registers manually.
3. **Route loop control and defers through ControlFlowGraph** by expressing `defer`, `break`, and `continue` as structured edges with allocator-aware scopes and automatic defer execution, removing direct `bcemit_jmp` patching.
4. **Add regression coverage** (unit or targeted integration) that asserts operator/statement emission avoids legacy helper counters and preserves allocator/CFG invariants.

## Function/class issue matrix
| Area | Functions / Classes | Issue summary |
| --- | --- | --- |
| Operator emission | `bcemit_arith`, `bcemit_comp` | Manual `freereg` decrements replaced with allocator-driven release; further work still needed to shift remaining legacy discharge patterns behind allocator/CFG APIs.【F:src/fluid/luajit-2.1/src/parser/parse_operators.cpp†L34-L163】 |
| Legacy operator path | `bcemit_binop_left` (legacy) | Keeps conditional branch patching and discharge logic in the legacy parser path, blocking full removal of register-level handling.【F:src/fluid/luajit-2.1/src/parser/parse_operators.cpp†L165-L200】 |
| Plain assignments | `IrEmitter::emit_plain_assignment` | Uses `bcemit_store`, `lex_state.assign_adjust`, and manual `freereg` resets instead of allocator/LValue-driven storage semantics.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1022-L1081】 |
| Compound assignments | `IrEmitter::emit_compound_assignment` | Duplicates table operands and materialises temporaries manually; result storage still goes through legacy helpers rather than allocator-owned value slots.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1086-L1108】 |
| Defer handling | `IrEmitter::emit_defer_stmt` | Inserts synthetic variables and rewrites `freereg`/`lex_state` directly, with no CFG-backed lifetime management for defer blocks.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L909-L957】 |
| Loop control flow | `IrEmitter::emit_break_stmt`, `IrEmitter::emit_continue_stmt` | Control transfers are patched with `bcemit_jmp` and defer execution manually, rather than emitting structured CFG edges that integrate with defers and allocator state.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L960-L983】 |
