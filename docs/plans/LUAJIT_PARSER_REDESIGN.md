# LuaJIT Parser Redesign Plan

## Overview
The LuaJIT parser living in `src/fluid/luajit-2.1/src/parser` was mechanically converted to C++20 but it still mirrors the original C implementation. Bytecode emission is interwoven with parsing logic, register allocation is manually coordinated through free functions, and parser state lives in globally shared structs. These patterns make it hard to add new Fluid language features and are brittle when we attempt larger refactors. This document captures the weak points that surfaced during the review and lays out a phased redesign strategy.

## Current Weak Points

### 1. Parsing and bytecode emission are fused together
* Functions such as `LexState::expr_table` both parse tokens and emit instructions, making it impossible to unit-test parsing independently from bytecode layout and forcing every feature change to reason about registers mid-parse.【F:src/fluid/luajit-2.1/src/parser/parse_expr.cpp†L144-L205】
* Compound assignment helpers (`assign_if_empty`, `assign_compound`) replicate this pattern, weaving control-flow and register juggling directly into the parser instead of emitting from a well-defined IR.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L64-L199】
* Because expression parsing mutates `FuncState` on the fly, we cannot cleanly stage optimisations, speculative parsing, or backtracking.

### 2. Parser state is still C-style, global, and difficult to reason about
* Error handling is still driven by free functions on `LexState`/`FuncState`, so callers manually thread raw pointers everywhere instead of working with richer parser context objects.【F:src/fluid/luajit-2.1/src/parser/parse_core.cpp†L7-L67】【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L14-L198】
* `LexState` and `FuncState` expose their internals directly, which encourages accidental invariants (e.g., manually toggling `fs->freereg`) and prevents us from isolating passes such as syntax validation, semantic analysis, and emission.
* There is no structured tracing or recovery; we cannot, for example, construct an AST to report multiple syntax errors in one pass.

### 3. Expression descriptors and flags remain low-level
* `ExpDesc` still uses a C `struct` with manually managed unions and bitfields, plus helper functions that operate on raw flags rather than encapsulated behaviour.【F:src/fluid/luajit-2.1/src/parser/parse_types.h†L17-L120】
* Concepts were introduced but we are not leveraging them to enforce invariants (e.g., no dedicated type for “resolved register expression”). This keeps downstream code dependent on `ExpDesc`’s layout and limits the benefits of C++20.
* Expression flags continue to be cleared manually through global helpers, which is error-prone and makes ownership of temporary registers unclear.【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L68-L105】

### 4. Register and jump management is brittle
* Register handling still lives in plain C functions (`bcreg_reserve`, `expr_discharge`, etc.) that take raw `FuncState*` arguments, so nothing stops callers from skipping required book-keeping or freeing registers out of order.【F:src/fluid/luajit-2.1/src/parser/parse_regalloc.cpp†L7-L200】
* Higher-level code is forced to duplicate table bases and indexes by hand before evaluating RHS expressions, which is both verbose and easy to get wrong when adding new operators.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L151-L199】
* Jump lists rely on callers understanding how to patch BC instructions manually via `JumpListView`, keeping implicit coupling between parse-time control-flow and bytecode layout.【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L24-L66】

### 5. Limited testability and instrumentation
* Because parsing, semantic analysis, and emission are intertwined, we cannot unit-test many helpers without spinning up an entire `FuncState`. Debugging currently depends on bytecode dumps rather than structured diagnostics or trace logs.
* There is no systematic way to validate invariants (register depth, pending jumps) other than assertions, which makes regressions harder to catch before runtime.

## Proposed Redesign (Phased)

### Phase 1 – Build a modern parser context and typed token stream
1. Introduce a `ParserContext` object that owns `LexState`, `FuncState`, allocator references, and an error collector. Provide lightweight `ParserResult`/`ParserStatus` structs (or `std::expected` equivalents) so helpers can return status without throwing.
2. Replace ad-hoc token helpers with a `TokenView`/`Lookahead` class that exposes typed tokens (`enum class TokenKind` + payload variant) so expression/statement parsers can work with strongly-typed data instead of re-reading `LexState` globals.
3. Wrap common operations (`match`, `consume`, `expect`) in context-aware methods that record errors instead of immediately aborting, laying the groundwork for better diagnostics.
4. Update existing modules to consume the new interfaces while still emitting bytecode directly; this phase focuses on isolating state and clarifying APIs so subsequent phases can swap implementations underneath.

### Phase 2 – Introduce an intermediate AST/IR layer
1. Define lightweight AST/IR node types (e.g., `ExprNode`, `StmtNode`) that capture the semantic structure of Fluid code without binding to registers. Use `std::variant`, `std::vector`, and `std::unique_ptr` to express recursive shapes.
2. Rework `expr_*` and `stmt_*` functions to build AST nodes using the new typed tokens, leaving `FuncState` untouched during parsing. Error recovery can now operate on AST boundaries.
3. Implement an `IrEmitter` pass that walks the AST and emits LuaJIT bytecode, retaining optimised patterns (folding, table templates) but isolating them from syntax parsing. This emitter becomes the sole owner of `FuncState` and register allocation.
4. Provide hooks for future passes (e.g., AST transforms for new Fluid features) by designing the node structures and traversal APIs with extensibility in mind.

**Status:** `parser/ast_nodes.h` and `parser/ast_nodes.cpp` now codify the node schema outlined above, providing builder helpers, ownership semantics, and lightweight statement/parameter views. Subsequent work on expression/statement parsing should target these factories instead of manipulating `FuncState` directly.

### Phase 3 – Rebuild register, jump, and expression management
1. Replace the global register helpers with a `RegisterAllocator` class that enforces lifetimes via RAII objects (e.g., `AllocatedRegister`, `RegisterSpan`). This allocator should expose explicit methods for duplicating table bases/indexes so compound operations no longer need to hand-roll copies.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L151-L199】
2. Encapsulate `ExpDesc` behaviour inside a class hierarchy or tagged union that knows how to discharge itself into the allocator, removing the need for external functions such as `expr_discharge`. Persist convenient constructors (nil/number/string) but hide raw flag manipulation behind methods.
3. Turn `JumpListView` into a higher-level `ControlFlowGraph` helper that models pending jumps as structured nodes instead of patching BC instructions inline. The emitter can then translate CFG edges into BC when finalising a block, greatly reducing manual patching sites.【F:src/fluid/luajit-2.1/src/parser/parse_internal.h†L24-L66】
4. Add debug-only verification hooks on the allocator/CFG to assert invariants (slot depth, unresolved jumps) at phase boundaries instead of scattering assertions across the parser.

### Phase 4 – Modernise operator and statement implementations
1. Rewrite operator handling (`parse_operators.cpp`, `expr_table`, compound assignments, ternaries, etc.) to operate on the AST + allocator abstractions. Many of the handwritten register juggling steps should disappear once operators work with value categories and temporaries managed by the allocator.【F:src/fluid/luajit-2.1/src/parser/parse_expr.cpp†L144-L205】【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L64-L199】
2. Model statement forms (loops, defers, continue/break) as dedicated node types so new Fluid constructs can be slotted in without rewriting large switch statements.
3. Where we still need immediate emission for performance (e.g., constant folding), keep that logic inside the emitter but guard it with clear contracts (inputs/outputs) and unit tests.

### Phase 5 – Instrumentation, testing, and migration
1. Introduce parser-level unit tests that operate on the AST (syntax only) and emitter-level tests that check bytecode for representative patterns. This decouples behavioural tests from integration-only Fluid scripts.
2. Add tracing hooks (compile-time toggles) inside `ParserContext`, `RegisterAllocator`, and `IrEmitter` to log high-level events for debugging without sprinkling `printf` statements in production code.
3. Plan a staged rollout: gate the new pipeline behind a build flag, run both parsers in parallel (old vs. new) in debug builds, and diff bytecode output to gain confidence before removing the legacy path.
4. Document migration guidelines so future contributors know how to extend the parser using the new abstractions (e.g., “add a new AST node, extend the emitter visitor, add unit tests”).

## Next Steps
1. Prototype the `ParserContext`/typed token APIs and convert a small subset of expressions to ensure the new scaffolding covers real scenarios.
2. Agree on the AST/IR schema and emitter responsibilities, including how much optimisation should happen per phase.
3. Schedule incremental conversions (expressions, statements, control-flow) so we can land improvements without destabilising the parser.
