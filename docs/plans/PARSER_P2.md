# LuaJIT Parser Redesign – Phase 2 Implementation Plan

## Objectives
Phase 2 severs the direct coupling between syntax parsing and bytecode emission by constructing a lightweight abstract syntax / intermediate representation (AST/IR) for Fluid programs. The parser will consume the Phase 1 typed token stream to build tree nodes that model expressions and statements independently of registers. Bytecode emission is deferred to a dedicated pass so semantic analysis, diagnostics, and future language features can operate on structured data instead of raw `FuncState` mutation.

## Deliverables
1. A set of AST/IR node definitions that capture all Fluid expressions, statements, and control-flow constructs parsed today, along with helper factories and traversal utilities.
2. Parser entry points (`expr_*`, `stmt_*`, and top-level helpers) that return AST nodes (or `ParserResult<AstNode>` equivalents) while leaving `FuncState` untouched.
3. An `IrEmitter` (or equivalent visitor) that lowers AST nodes to LuaJIT bytecode, managing register allocation and control-flow patching internally while preserving existing optimisations.
4. Debugging hooks and diagnostics that operate on AST boundaries, enabling clearer error reporting and easier instrumentation.
5. Documentation and regression tests that describe how to construct, serialise, and emit AST nodes, ensuring future phases can extend the pipeline safely.

## Step-by-Step Plan

### 1. Define the AST/IR schema
1. Catalogue every expression and statement form handled under `src/fluid/luajit-2.1/src/parser` (prefix expressions, suffix operations, literals, control-flow statements, function literals, etc.) and describe the required operands, spans, and attributes for each.
2. Create `ast_nodes.h/.cpp` (or extend `parse_types.h`) to house:
   * Core node tags (`enum class AstNodeKind`), identifier/literal wrappers, and source span metadata shared across nodes.
   * Expression node structs (`LiteralExpr`, `BinaryExpr`, `TableCtorExpr`, `CallExpr`, etc.) expressed via `std::variant` or tagged unions that own child nodes through `std::unique_ptr`/`std::vector`.
   * Statement node structs (`LocalStmt`, `AssignmentStmt`, `IfStmt`, `LoopStmt`, `DeferStmt`, etc.) that explicitly encode nested blocks and labels.
3. Provide builder helpers (`make_literal`, `make_call`, `make_block`) that validate invariants (e.g., operand counts) and attach diagnostic context.
4. Define lightweight views/iterators for node collections (block statements, function parameters) so later passes can traverse without leaking internal storage details.
5. Document the schema inside the new header, including ownership semantics and extension guidelines (e.g., how to add a new operator node).

### 2. Thread AST construction through the parser
1. Update `ParserContext` (Phase 1 deliverable) with factories or allocators for AST nodes if pooled allocation becomes necessary, otherwise rely on standard containers but document lifetime expectations.
2. Modify expression parsing functions (`expr_primary`, `expr_prefix`, `expr_binop`, etc.) to build and return AST nodes:
   * Replace `ExpDesc` mutation with `ParserResult<ExprNode>` outputs.
   * Use typed token helpers to attach `Token` spans to nodes for later diagnostics.
3. Perform the same rewrite for statement parsers (`parse_local`, `parse_if`, `parse_loop`, `parse_block`, etc.), ensuring scope/label information is represented structurally (e.g., `BlockNode` with explicit child vectors) rather than via raw stack manipulation.
4. Introduce transitional adapters where immediate emission is still required (e.g., complex table constructors) so the rest of the parser can migrate incrementally without regressing behaviour. These adapters should convert AST nodes back into the legacy `ExpDesc` path only when unavoidable and be annotated for removal.
5. Extend diagnostics to report at node boundaries (e.g., “invalid break outside loop” attaches to the `BreakStmt` node) using the span metadata stored on nodes.

### 3. Implement the IR emission pass
1. Create `ir_emitter.h/.cpp` that exposes an `IrEmitter` class responsible for walking AST nodes and invoking legacy bytecode helpers. The emitter owns `FuncState`, register allocation, jump management, and any pending constant folding.
2. Define visitor entry points (`emit_block(const BlockNode&)`, `emit_expr(const ExprNode&)`, `emit_stmt(const StmtNode&)`) that translate nodes into bytecode while respecting existing optimisations (e.g., tail calls, constant folding, loop invariants).
3. Encapsulate register allocation within the emitter for this phase, even if the full Phase 3 allocator is not yet available. Provide interim helper classes (e.g., `TempRegister`) that mimic RAII semantics to prepare for Phase 3’s allocator refactor.
4. Ensure control-flow constructs use structured patching APIs inside the emitter (e.g., helper methods to create jump lists for `if`/`loop` nodes) rather than manipulating `BCPos` manually in parser code.
5. Add hooks so later phases can swap the emitter implementation or insert transformation passes between parsing and emission (e.g., AST rewrites for new language features).

### 4. Integrate parsing and emission boundaries
1. Decide on a top-level parse contract: e.g., `ParserContext::parse_chunk()` returns `ParserResult<BlockNode>` which, on success, is passed to `IrEmitter::emit(BlockNode&)`. Document this handshake in both headers.
2. Update callers (`lj_parse`, module loaders) to instantiate the emitter and feed it the AST after a successful parse. Maintain a feature flag that can re-enable the legacy direct emission path for debugging during the migration.
3. Provide conversion helpers or logging utilities to diff AST-driven bytecode against the legacy output for representative Fluid scripts, easing validation.
4. Record instrumentation (trace logs or debug dumps) at the parse/emission boundary to help Phase 5 introduce deeper testing. These dumps should include node kinds, child counts, and token spans for reproducibility.

### 5. Testing, validation, and performance safeguards
1. Add parser-level unit tests (or scripted Fluid snippets) that assert the produced AST shape for canonical inputs (literal expressions, nested tables, loops, function literals). Consider serialising nodes to a debug JSON/text format purely for tests.
2. Extend existing regression scripts (e.g., `src/fluid/tests/parser_phase1.fluid`) or add new ones dedicated to Phase 2 to ensure AST parsing covers success and failure modes.
3. Implement bytecode comparison harnesses that run both the legacy and AST-driven emitters (guarded by a build flag) and assert bytecode equivalence for curated samples. Failures should produce actionable diffs.
4. Profile parsing/emission time on representative workloads to ensure the extra AST allocation overhead stays within acceptable limits; document mitigation strategies (arena allocators, node pooling) if necessary.

NOTE:
- Unit tests can be added to lj_parse_tests.cpp and are executed by running the Flute script `src/fluid/tests/test_unit_tests.fluid` directly.  Use printf or cout for sending output.
- The `--jit:trace` commandline option enables the associated diagnosis options in make_parser_config()
- The `--jit:diagnose` commandline option enables the associated tracing options in make_parser_config()
- Additional commandline options can be added to fluid.cpp in the MODInit() function if necessary.

### 6. Documentation and developer enablement
1. Update `docs/plans/LUAJIT_PARSER_REDESIGN.md` Phase 2 section summary (once implementation begins) with progress notes, caveats, and links to the new files.
2. Create contributor notes (either inline in headers or a short guide under `docs/plans/`) describing how to add new AST nodes, how the emitter visitor should be extended, and how diagnostics tie into spans.
3. Capture troubleshooting steps for AST/emitter mismatches (e.g., enabling trace dumps, running the bytecode diff harness) to accelerate onboarding for future maintainers.

## Dependencies and Risks
* **Incremental migration:** Because rewriting all parser entry points simultaneously is risky, schedule the conversion in slices (expressions first, then statements) and maintain temporary bridging code. Clearly annotate any hybrid paths.
* **Memory pressure:** AST construction introduces additional allocations. Investigate arena allocators or object pools if profiling shows regressions, and provide instrumentation toggles to measure node counts.
* **Emitter parity:** Any behavioural drift between the new emitter and the legacy inline emission can break Fluid programs. The bytecode diff harness and feature flag are critical for safe rollout.

## Success Criteria
* Parser functions return AST nodes and no longer mutate `FuncState` directly during syntax analysis for the migrated surfaces.
* The new emitter reproduces existing bytecode layouts for the covered constructs, validated via automated comparisons.
* Diagnostics and tracing operate on AST spans, enabling clearer error reporting and groundwork for future multi-error recovery.
* Documentation and tests describe the AST schema, emission contract, and validation procedures, ensuring subsequent phases (register allocator rewrite, operator modernisation) have a stable platform.
