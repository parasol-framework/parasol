# Phase 4 Plan – Modernise operator and statement implementations

## Current state vs. Phase 4 requirements
* The AST pipeline is active (see `run_ast_pipeline` in `lj_parse.cpp`), but operator emission still leans on the legacy helper set `bcemit_binop_left`/`bcemit_binop` from `parse_operators.cpp`, which mutates `ExpDesc` and `freereg` directly instead of funnelling through higher-level allocator/CFG abstractions. This keeps bytecode layout intertwined with low-level register juggling.【F:src/fluid/luajit-2.1/src/parser/lj_parse.cpp†L498-L536】【F:src/fluid/luajit-2.1/src/parser/parse_operators.cpp†L31-L163】
* Statement emission in `IrEmitter` mirrors the legacy paths: assignments rebuild the classic `assign_adjust` flow and manually juggle register lifetimes, while control-flow nodes (if/loops/defer) still patch jumps via `FuncState` rather than a structured CFG layer. This deviates from the Phase 4 goal of modelling statement forms as dedicated nodes with modern ownership semantics.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L920-L1018】
* Binary/unary/presence operators are mapped from AST nodes but are emitted through the legacy opcode helpers without isolating value categories or reusing the new `ControlFlowGraph`. This means presence/ternary/compound operations still rely on hand-authored jump and register manipulation instead of allocator-managed temporaries.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1368-L1410】

## Step-by-step implementation plan
1. **Create an operator/statement capability matrix**
   * Catalogue which AST operator and statement kinds are exercised in `IrEmitter` and identify the places that still call legacy helpers. Add tracing counters or assertions to flag fallback paths so gaps are visible during refactors.
   * Define expected value-category inputs/outputs (constants, relocatable registers, table operands, CFG edges) for each operator/statement form to guide API redesign.

2. **Extract a dedicated OperatorEmitter facade**
   * Introduce a class (or set of helpers) owned by `IrEmissionContext` that translates AST operator payloads into allocator/Cfg-aware actions. Move `foldarith`, `bcemit_arith/comp`, and presence handling into methods that accept `ExpressionValue`/`ValueSlot` wrappers instead of raw `ExpDesc*`.
   * Replace direct `freereg` mutation with RAII reservations via `RegisterAllocator` and ensure the facade records CFG edges for short-circuit/presence operators instead of emitting jumps inline.

3. **Rework binary/unary/ternary emission to use value categories**
   * Define lightweight structs for `ValueUse` (read-only constant, movable temp, addressable l-value) and adapt `emit_binary_expr`, `emit_unary_expr`, and `emit_ternary_expr` to request/produce these shapes. Collapse constant folding and register materialisation into reusable utilities so operator code no longer calls `expr_toreg`/`expr_free` directly.
   * Implement presence/`??`/logical short-circuit handling through `ControlFlowGraph` nodes, returning explicit true/false edges rather than patching jumps through `FuncState`.

4. **Modernise statement emission around AST nodes**
   * Introduce typed l-value descriptors for assignments (locals, upvalues, table slots, member calls) so compound and `??` assignments can reuse operator emission without rebuilding legacy `assign_adjust` logic. Manage hazard detection and table operand duplication via `RegisterAllocator` utilities rather than manual `BC_MOV` sequences.
   * For control-flow statements, route break/continue/defer/loop exits through `ControlFlowGraph` bookkeeping (e.g., structured edge lists that are resolved at block finalisation). This reduces reliance on raw `gola_new`/`bcemit_jmp` in statement emitters.

5. **Retire or isolate legacy operator/statement helpers**
   * Delete or quarantine `parse_operators.cpp` and `parse_stmt.cpp` helpers behind the legacy parser build flag once the new emitter covers all AST forms. Ensure the legacy path continues to compile for `ast-legacy` mode, but the default pipeline should no longer depend on legacy emission helpers.
   * Update `IrEmitter` to consume the new operator/statement APIs exclusively and remove redundant `ExpDesc` plumbing where AST ownership makes it unnecessary.

6. **Add targeted tests and instrumentation**
   * Expand parser unit tests to cover operator precedence, presence/ternary edge cases, and compound assignments using the AST pipeline only. Include bytecode pattern checks for short-circuit behaviour and table updates.
   * Add optional debug tracing (guarded by `ParserConfig`/profiler toggles) that records allocator reservations and CFG edge resolution per operator/statement to catch regressions during the refactor.

7. **Migration and validation steps**
   * Refactor incrementally: land operator facade changes first, then update expression emitters, followed by statement emitters. After each stage, run Fluid regression tests and compare bytecode dumps between legacy and modern paths using the existing dual-parser flags.
   * Document the new extension points in `docs/plans/LUAJIT_PARSER_REDESIGN.md` once modernised emission is stable, noting how to add operators/statements using the allocator/CFG abstractions.
