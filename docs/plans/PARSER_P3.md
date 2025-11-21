# Phase 3 – Register, jump, and expression management overhaul

## Objective
Phase 3 replaces the remaining C-style register helpers, jump patching, and raw expression descriptors with structured, RAII-friendly abstractions that work with the AST + emitter pipeline introduced in Phase 2. The goal is to centralise ownership of registers and control-flow edges, making it easier to reason about lifetimes, support new Fluid constructs, and add debug-time verification hooks.

## Current footing after Phase 2
- The parser now builds AST nodes and emits bytecode via `IrEmitter`, but emission still leans on legacy helpers such as `expr_discharge`, direct `FuncState` register arithmetic, and manual jump patching through `JumpListView` and `JumpHandle`.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L70-L198】【F:src/fluid/luajit-2.1/src/parser/parse_regalloc.cpp†L9-L124】
- Expression descriptors continue to use the legacy `ExpDesc` struct and flag helpers, so ownership of temporary registers and jump lists remains implicit.【F:src/fluid/luajit-2.1/src/parser/parse_types.h†L17-L108】【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L71-L107】
- Compound assignment paths still duplicate table bases/indexes by hand to avoid register hazards, underscoring the need for allocator-provided duplication utilities.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L184-L212】

## Step-by-step implementation plan

1. **Design a `RegisterAllocator` interface and RAII handles**
   - Define a class (likely in `parse_regalloc.*`) that wraps `FuncState` slot growth, reservation, and freeing, exposing RAII types such as `AllocatedRegister` and `RegisterSpan` for single-slot and contiguous reservations.
   - Port the semantics of `bcreg_bump`, `bcreg_reserve`, `bcreg_free`, and `expr_free` into methods that track ownership, preventing callers from freeing out of order and enabling debug-time assertions for depth and reservation mismatches.【F:src/fluid/luajit-2.1/src/parser/parse_regalloc.cpp†L9-L124】
   - Add explicit helpers for duplicating table bases and index operands so callers no longer reimplement the duplication sequence used by compound assignments.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L184-L212】

2. **Refactor `ExpDesc` into a higher-level value category wrapper**
   - Introduce a class or tagged union that encapsulates current `ExpDesc` payloads (constants, locals, upvalues, indexed accesses, calls) and owns its pending jump lists.
   - Provide methods such as `to_reg(RegisterAllocator&)`, `discharge(ControlFlowGraph&)`, and `release()` to move value ownership rather than mutating raw fields, hiding flag manipulation behind behaviour-driven APIs.【F:src/fluid/luajit-2.1/src/parser/parse_types.h†L17-L108】
   - Update flag helpers in `parse_internal.h` to become member functions or thin wrappers that operate on the new type, ensuring callers migrate off direct flag bit-twiddling.【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L71-L107】

3. **Implement a `ControlFlowGraph` abstraction for jumps**
   - Replace `JumpListView`/`JumpHandle` usage with a CFG object that records edges (true/false lists, break/continue targets) and offers methods to append, merge, and patch at block finalisation time.【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L29-L69】【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L70-L126】
   - Provide translation logic in the emitter to lower CFG edges into bytecode patches, localising the BC-level knowledge to one place and simplifying statement emission.
   - Ensure the CFG can surface diagnostics or assertions when jumps remain unresolved at function end.

4. **Integrate allocator, expression wrapper, and CFG into `IrEmitter` and parsing paths**
   - Thread the new allocator and CFG through `IrEmitter` entry points so all expression/statement visitors request registers via RAII handles rather than touching `FuncState` directly.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L154-L198】
   - Update expression lowering to use the new value wrapper APIs (e.g., `to_reg` or `ensure_scalar`) instead of calling `expr_discharge`/`expr_toreg`, and replace manual jump patching with CFG operations.【F:src/fluid/luajit-2.1/src/parser/parse_regalloc.cpp†L82-L123】
   - Migrate hot paths that currently juggle register duplication (compound assignments, table constructors) to call allocator utilities, reducing bespoke logic in statement helpers.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L156-L220】

5. **Add debug-only verification and tracing hooks**
   - Extend the allocator and CFG with optional validation (guarded by `#if LJ_DEBUG` or a parser flag) to assert slot depth, live range balance, and unpatched jumps at key boundaries (function exit, block end, loop exit).
   - Add lightweight tracing callbacks in `ParserContext` or `IrEmitter` to log allocation and patching decisions, aiding regression debugging without scattering `printf` calls.【F:src/fluid/luajit-2.1/src/parser/parser_context.cpp†L1-L200】
   - Document new invariants and usage patterns in the parser plan and consider adding targeted unit tests alongside the AST pipeline to lock in behaviour.

