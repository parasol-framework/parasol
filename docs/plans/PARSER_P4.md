# Phase 4 Plan – Modernise operator and statement implementations

## Current state vs. Phase 4 requirements
* The AST pipeline is active (see `run_ast_pipeline` in `lj_parse.cpp`), but operator emission still leans on the legacy helper set `bcemit_binop_left`/`bcemit_binop` from `parse_operators.cpp`, which mutates `ExpDesc` and `freereg` directly instead of funnelling through higher-level allocator/CFG abstractions. This keeps bytecode layout intertwined with low-level register juggling.【F:src/fluid/luajit-2.1/src/parser/lj_parse.cpp†L498-L536】【F:src/fluid/luajit-2.1/src/parser/parse_operators.cpp†L31-L163】
* Statement emission in `IrEmitter` mirrors the legacy paths: assignments rebuild the classic `assign_adjust` flow and manually juggle register lifetimes, while control-flow nodes (if/loops/defer) still patch jumps via `FuncState` rather than a structured CFG layer. This deviates from the Phase 4 goal of modelling statement forms as dedicated nodes with modern ownership semantics.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L920-L1018】
* Binary/unary/presence operators are mapped from AST nodes but are emitted through the legacy opcode helpers without isolating value categories or reusing the new `ControlFlowGraph`. This means presence/ternary/compound operations still rely on hand-authored jump and register manipulation instead of allocator-managed temporaries.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1368-L1410】

## Implementation Progress

### ✅ Step 1: Create an operator/statement capability matrix (COMPLETE)
**Status:** Implemented and committed (commit 18d23e0f)

**Achievements:**
* Added `LegacyHelperRecorder` tracking infrastructure to `ir_emitter.cpp` (lines 138-180)
  - Tracks 9 categories of legacy helper calls: `bcemit_binop_left`, `bcemit_binop`, `bcemit_unop`, `bcemit_presence_check`, `bcemit_store`, `assign_adjust`, `bcemit_branch_t`, `bcemit_branch_f`, and manual jump patching
  - Logs first 8 occurrences and every 32nd call for visibility
  - Provides `dump_statistics()` method for progress monitoring
* Instrumented all legacy helper call sites in expression and statement emitters
  - `emit_unary_expr`, `emit_binary_expr`, `emit_update_expr`, `emit_presence_expr`
  - `emit_plain_assignment`, `emit_compound_assignment`, `emit_if_empty_assignment`
* Created comprehensive capability matrix document: `docs/plans/OPERATOR_STATEMENT_MATRIX.md`
  - Catalogues all 38 AST operator and statement kinds
  - Maps each to current legacy helper usage with exact source locations
  - Defines expected value-category inputs/outputs (ValueUse, ValueSlot, LValue) for target implementation
  - Provides detailed roadmap for Steps 2-7

**Files Modified:**
* `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` - Added tracking infrastructure and instrumentation
* `docs/plans/OPERATOR_STATEMENT_MATRIX.md` - Created comprehensive analysis document

**Next:** Complete Step 2 compilation issues and begin Step 3

---

### ✅ Step 2: Extract a dedicated OperatorEmitter facade (COMPLETE)
**Status:** Implemented and committed (commit 90c15c90 + follow-up)

**Achievements:**
* Created `OperatorEmitter` class structure with proper API design
  - `operator_emitter.h` - Facade header with forward declarations
  - `operator_emitter.cpp` - Implementation wrapping legacy helpers
* Integrated `OperatorEmitter` into `IrEmissionContext` alongside RegisterAllocator and ControlFlowGraph
* Exposed legacy operator functions (foldarith, bcemit_arith, bcemit_comp, bcemit_unop) with extern linkage
* Added facade methods:
  - `fold_constant_arith()` - Wraps `foldarith()`
  - `emit_unary()` - Wraps `bcemit_unop()`
  - `emit_binary_arith()` - Wraps `bcemit_arith()`
  - `emit_comparison()` - Wraps `bcemit_comp()`
  - `emit_binary_bitwise()` - Wraps `bcemit_arith()` for bitwise ops

**Resolution of include issues:**
* Header (`operator_emitter.h`) includes `parse_types.h` for complete type definitions
* Implementation (`operator_emitter.cpp`) includes prerequisite headers before operator_emitter.h:
  - `lj_bc.h` for NO_JMP and BC opcode definitions
  - `lj_lex.h` for LexState definition
* BCOp parameter uses `int` in header declaration to avoid typedef conflicts, casts to BCOp in implementation

**Files Created:**
* `src/fluid/luajit-2.1/src/parser/operator_emitter.h` - Facade header
* `src/fluid/luajit-2.1/src/parser/operator_emitter.cpp` - Facade implementation

**Files Modified:**
* `src/fluid/luajit-2.1/src/parser/ir_emitter.h` - Added OperatorEmitter to IrEmissionContext
* `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` - Initialize OperatorEmitter
* `src/fluid/luajit-2.1/src/parser/parse_operators.cpp` - Made foldarith, bcemit_arith, bcemit_comp, bcemit_unop extern
* `src/fluid/luajit-2.1/src/parser/parse_internal.h` - Changed declarations to extern for exported functions
* `src/fluid/CMakeLists.txt` - Added operator_emitter.cpp to build

**Next:** Adapt emit_unary_expr() and emit_binary_expr() in IrEmitter to use OperatorEmitter facade (Step 3)

---

## Step-by-step implementation plan
1. **Create an operator/statement capability matrix** ✅ COMPLETE
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
