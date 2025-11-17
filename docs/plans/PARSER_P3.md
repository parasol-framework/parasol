# LuaJIT Parser Redesign – Phase 3 Implementation Plan

## Introduction
Phase 3 rebuilds the low-level runtime services that power parsing and bytecode emission so the new AST/IR pipeline from Phase 2 can operate with modern, testable primitives. The focus is on register allocation, expression descriptors, and control-flow patching. This document orients future contributors by summarising the intent of this phase, listing the concrete deliverables, and outlining a step-by-step plan that references the redesign goals captured in `docs/plans/LUAJIT_PARSER_REDESIGN.md`.

## Objectives
1. Replace the legacy free-function register helpers with a structured allocator that enforces lifetimes, validates usage, and exposes high-level duplication/borrowing APIs for compound operations.
2. Encapsulate expression descriptor behaviour so parser and emitter code interact with type-safe value categories rather than raw `ExpDesc` unions and flag juggling.
3. Model pending jumps and control-flow patching through a dedicated helper (CFG or jump manager) that provides structured edges, making emitter code easier to reason about and validate.
4. Introduce debug and verification hooks that assert allocator and control-flow invariants at well-defined phase boundaries.

## Deliverables
1. `RegisterAllocator` subsystem with RAII handles (`AllocatedRegister`, `RegisterSpan`, etc.), duplication helpers for table bases/indexes, and integration points for the AST-driven emitter.
2. Refactored expression descriptor API (`ExpressionValue`, tagged unions, or small class hierarchy) with self-contained discharge logic and constructors for literals, temporaries, and references.
3. Control-flow management helper (`ControlFlowGraph`, `JumpPlanner`, or similar) that tracks pending jumps, block edges, and patching responsibilities.
4. Updated emitter/parser touchpoints that consume the new allocator/value/control-flow abstractions instead of the legacy helpers.
5. Debug-only validation utilities (slot depth checking, unresolved jump detection) and documentation describing how to exercise them during development.

## Step-by-Step Plan

### 1. Audit and extract legacy responsibilities
1. Catalogue every usage of `bcreg_*`, `expr_*` discharge helpers, and `JumpListView` to understand existing responsibilities, ownership rules, and edge cases.
2. Document register lifetime rules (allocation granularity, temporary vs. persistent registers, handling of table bases/indexes) so the new allocator enforces them explicitly.
3. Identify all jump patching patterns (forward jumps, loops, breaks/continues, logical short-circuit) to shape the new control-flow helper API.
4. Record current invariants (e.g., `FuncState::freereg` never decreases below active locals) as explicit requirements for the new components.

### 2. Design the RegisterAllocator API
1. Draft a header (`parser/register_allocator.h`) describing core types: allocator owner, single register handle, span handle, and duplication helpers.
2. Define lifecycle rules (acquire, duplicate, release) and ownership semantics (move-only handles, scoped spans) enforced through RAII.
3. Provide explicit helpers for compound operations called out in the redesign plan (e.g., `DuplicateTableBase`, `ReserveIndexPair`) so parser/emitter code no longer reimplements them manually.
4. Plan integration with `FuncState`: determine how the allocator records current `freereg`, interacts with `BCIns` emission, and exposes debug telemetry (current depth, max depth).
5. Outline error reporting pathways (status returns or debug asserts) when callers misuse registers (double free, leak, out-of-range access).

### 3. Implement the RegisterAllocator
1. Create the allocator implementation file and port legacy helper logic into class methods, ensuring the public API matches the design drafted above.
2. Introduce RAII handle classes (`AllocatedRegister`, `RegisterSpan`) that automatically release/restore register state when destroyed.
3. Add duplication helpers tailored to table operations and multi-value expressions, validating they preserve ordering rules from the legacy implementation.
4. Provide debug instrumentation (`VerifyDepth`, `DumpState`, assertions) compiled in debug builds to catch invariant violations early.
5. Update existing unit/integration hooks (if any) or add targeted tests that exercise allocation patterns (nested temporaries, spans, duplication).

### 4. Redesign expression descriptor handling
1. Define a modern replacement for `ExpDesc`, e.g., `ExpressionValue` variant or small hierarchy, encapsulating value categories (constants, registers, jumps, upvalues, table references).
2. Implement constructors/helpers for literals (`NilValue`, `NumberValue`, `StringValue`), identifiers, table fields, and function literals that attach source spans and metadata needed for diagnostics.
3. Embed discharge methods (`materialise(RegisterAllocator&)`, `as_constant()`, etc.) so value objects know how to lower themselves without exposing raw flags.
4. Provide conversion layers for any remaining legacy code paths, clearly marked for removal, to support incremental migration.
5. Update parser/emitter interactions to consume the new descriptor API, ensuring AST node emission uses these abstractions instead of touching registers directly.

### 5. Build the control-flow management helper
1. Specify a `ControlFlowGraph`/`JumpPlanner` interface that models pending jumps as nodes/edges rather than raw `BCPos` lists. Include APIs for creating jump tokens, merging lists, and finalising blocks.
2. Implement data structures to track jump targets, associated conditions (true/false branches), and metadata required for patching once block boundaries are known.
3. Provide integration hooks with the emitter so constructs like `if`, `while`, `repeat`, short-circuit booleans, and `break`/`continue` register their edges through the helper.
4. Add debug verifiers (`AssertAllPatched`, edge dumps) to detect dangling jumps before bytecode finalisation.
5. Remove or deprecate direct `JumpListView` usage throughout the parser/emitter, routing all control-flow decisions through the new helper.

### 6. Integrate the new subsystems with the AST emitter
1. Modify `IrEmitter` (Phase 2 deliverable) to own instances of `RegisterAllocator` and the control-flow helper, wiring them into expression/statement emission routines.
2. Update emission entry points (`emit_expr`, `emit_stmt`, `emit_block`) to request registers via the allocator and to plan jumps via the new helper instead of the legacy globals.
3. Ensure expression values produced during emission leverage the new descriptor abstraction, especially for temporary registers and short-circuit expressions.
4. Provide transitional adapters or feature flags to compare allocator/CFG output against the legacy path until confidence is gained.
5. Document the new responsibilities within `IrEmitter` headers so future contributors know how to request registers, duplicate bases, and manage control-flow.

### 7. Add verification hooks and developer tooling
1. Introduce debug-only build flags or logging levels (e.g., `PARASOL_VLOG`) that enable allocator/CFG tracing, showing allocations, releases, and jump planning steps.
2. Implement unit or integration tests that intentionally misuse the allocator/CFG to confirm invariants fire (double-release detection, unresolved jump assertion).
3. Extend the parser/emitter regression harness to optionally dump allocator/control-flow stats for Fluid scripts, assisting performance tuning.
4. Document troubleshooting steps (how to enable tracing, interpret dumps, common invariant failures) in this plan and relevant headers.

### 8. Update documentation and adoption guidance
1. Summarise Phase 3 progress in `docs/plans/LUAJIT_PARSER_REDESIGN.md`, linking to the new allocator/descriptor/CFG modules.
2. Add contributor notes describing how to allocate registers safely, how expression values should be constructed, and how to register new control-flow constructs.
3. Provide code samples (short snippets) demonstrating common patterns (evaluating a binary expression, emitting a loop) using the new APIs.
4. Capture known limitations or follow-up tasks for Phase 4 (operator rewrites) so the next phase can build directly on the new infrastructure.

## Risks and Mitigations
* **Parity drift:** The allocator and CFG rewrites can subtly change bytecode layouts. Mitigate via feature flags and bytecode diff harnesses inherited from Phase 2.
* **Incremental migration complexity:** Portions of the parser/emitter may need temporary adapters. Track these with TODOs and clear removal criteria to avoid long-lived hybrids.
* **Performance regressions:** Additional abstraction layers could add overhead. Use allocator pooling, span reuse, and targeted profiling to keep performance acceptable.
* **Invariant enforcement noise:** Overly aggressive assertions might trigger during legitimate edge cases; ensure verification hooks are configurable and well-documented.

## Success Criteria
* All parser/emitter code acquires registers exclusively through the `RegisterAllocator` (or wrappers) and releases them automatically via RAII handles.
* Expression handling no longer manipulates `ExpDesc` flags directly; value categories are encoded in the new descriptor abstraction with self-contained discharge logic.
* Control-flow constructs route through the new helper, leaving no direct `JumpListView` manipulation in migrated code paths.
* Debug builds can verify allocator depth and jump resolution without manual inspection, and documentation explains how to use these tools.
* The AST emitter can emit representative Fluid programs with feature parity relative to the legacy register/jump helpers, validated via automated comparisons.
