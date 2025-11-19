# IrEmitter Feature Parity Plan

## Context
Phase 2 of the LuaJIT parser redesign requires an `IrEmitter` pass that can traverse the full AST and reproduce the legacy parser's bytecode without relying on inline emission.【F:docs/plans/LUAJIT_PARSER_REDESIGN.md†L40-L45】【F:docs/plans/PARSER_P2.md†L6-L11】
The current implementation only handles a narrow subset of statements (`expression`, `return`, simple `local`, trivial `do`) and a handful of expression forms before reporting `unsupported_*`, leaving large portions of the AST un-emitted.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L123-L316】
Assignments are restricted to plain single-identifier writes, preventing property assignments, destructuring, compound operators, and multi-target semantics that the legacy parser already supports.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L260-L290】
To reach feature parity we must extend the emitter to cover every node listed in `AstNodeKind` (control flow, function literals/definitions, table constructors, update/ternary/presence forms, etc.) while inheriting the legacy register/jump behaviour.【F:src/fluid/luajit-2.1/src/parser/ast_nodes.h†L57-L111】

## Step-by-step plan

1. **Audit legacy coverage and define an emitter matrix**
   1. Enumerate all expression/statement helpers invoked by the legacy parser (`expr_*`, `stmt_*`) and map them to `AstNodeKind` entries, producing a coverage table checked into the docs folder. This provides a concrete target for parity instead of ad-hoc expansions.
   2. For every entry mark the legacy bytecode helpers it triggers (e.g., `bcemit_method`, `bcemit_loop_head`, `lj_lex:var_new`). Use this to prioritise emitter work and to guard against regressions when features are ported.
   3. Introduce debug counters or trace logs inside `IrEmitter::emit_statement/emit_expression` that record unsupported node kinds while the AST pipeline is opt-in, enabling the coverage table to be validated against real Fluid sources.
   _Status (Mar 2025): Complete. The coverage and helper mappings now live in `docs/plans/IREMITTER_MATRIX.md`, and IrEmitter records every unsupported statement/expression it encounters so the matrix can be verified against Fluid builds before the AST path becomes mandatory.【F:docs/plans/IREMITTER_MATRIX.md†L1-L177】【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1-L151】_

2. **Generalise assignment and declaration lowering**
   1. Extend `emit_assignment_stmt` to accept all `AssignmentOperator` values, multi-target lists, and complex lvalues (member/index/update expressions). Reuse the legacy `lj_bcreg` helpers to mirror behaviour for table fields, method assignments, and `??` compound writes.
   2. Teach the emitter to resolve upvalues/globals for writes by modelling the legacy `bcreg_upvalue` and `bcreg_global` flows so AST identifier metadata is respected even when `local_bindings` misses.
   3. Augment `emit_local_decl_stmt` to support function locals, table destructuring tails, and default initialisers. Ensure blank identifiers and varargs follow the same padding semantics as the legacy parser.
   _Status (Mar 2025): Complete. `emit_assignment_stmt` now lowers every assignment form (multi-target, compound, and `??`) against identifier, member, or index lvalues, reusing the same register guards and hazard checks as the legacy parser, while `emit_identifier_expr`/`emit_local_decl_stmt` keep the binding tables in sync so globals, upvalues, blank locals, and default initialisers behave identically to the original bytecode emitter._

3. **Complete expression coverage**
   1. Implement visitors for `TableExpr`, `FunctionExpr`, `UpdateExpr`, `TernaryExpr`, and postfix operators so every expression node defined in `ast_nodes.h` lowers cleanly. Table literals must sequence record/array/computed fields with the same register reservation and constant folding as `expr_table` did.
   2. Handle `CallExpr` variations beyond direct/method invocation, including colon-call sugar (`CallDispatch::Method`), vararg forwarding, and multi-result propagation. Ensure `emit_expression_list` handles trailing call/vararg nodes without emitting extra registers.
   3. Add support for Fluid-specific constructs such as presence checks, `??`/`?=` chains, postfix increments, and cdata literals by mirroring the existing bytecode helpers the legacy path uses.
   _Status (Mar 2025): Complete. `IrEmitter` now lowers every expression node: table constructors reuse the legacy `TDUP/TSETM` flow with template tables, function literals spin up child `FuncState`s and emit nested prototypes, update/postfix operators rewrite to register-safe compound assignments, and ternaries materialise the same falsy jump chains as `expr_binop`. Call emission honours AST-provided multi-result hints so colon sugar, varargs, and trailing calls forward their return sets, closing the remaining gaps in expression coverage._

4. **Implement control-flow and function statements**
   1. Add `emit_if_stmt`, `emit_while_stmt`, `emit_repeat_stmt`, `emit_numeric_for_stmt`, `emit_generic_for_stmt`, `emit_defer_stmt`, `emit_break_stmt`, `emit_continue_stmt`, `emit_goto_stmt`, and `emit_label_stmt`, wiring their jump management to helper methods that encapsulate patch lists (preparing for Phase 3's control-flow refactor). Loops must reserve exit/continue targets identical to the legacy `FuncState` stack discipline.
   2. Handle `FunctionStmt` and `LocalFunctionStmt` by emitting nested prototypes: reuse `FuncScope`/`ScopeGuard` to create child `FuncState` instances, assign upvalues/locals, and register the resulting prototype with the parent chunk, matching `expr_lambda` and `stmt_func` semantics.
   3. Ensure `BlockStmt` visitation understands scope flags such as `FuncScopeFlag::Loop` or `FuncScopeFlag::Try` so defer execution, `continue`, and `break` semantics match the existing bytecode.
   _Status (Apr 2025): Complete. IrEmitter now lowers every control-flow and function statement: local/global function declarations reuse the function-expression visitor, `if`/`while`/`repeat` and both `for` styles mirror the legacy register discipline (including iterator prediction and continue targets), `defer` scopes record callable/argument locals, and `break`/`continue`/label/goto nodes feed through the existing gola machinery so patch lists stay in sync with FuncScope flags._

5. **Strengthen register/jump infrastructure inside the emitter**
   1. Wrap frequently repeated register operations (`expr_tonextreg`, `expr_toreg`, `expr_free`) behind emitter-local helpers that enforce balanced allocation and prepare the codebase for the Phase 3 allocator. Emitters for loops/conditionals should request structured jump objects rather than manipulating `BCPos` directly.
   2. Mirror the legacy parser's handling of `FuncState::freereg`, `nactvar`, and pending close instructions for nested functions and `defer` statements. Add assertions or debug logs when the emitter leaves registers unreleased.
   3. Encapsulate `LocalBindingScope` lookups in a richer symbol table that tracks depth and allows shadowing so multi-level scopes behave identically to the legacy resolver.
   _Status (Apr 2025): Complete. Register and lifetime operations now route through emitter helpers that log/register-balance violations, while blocks automatically validate `freereg`/`nactvar` parity and defer scopes. Conditionals and loop statements consume structured `JumpHandle`s instead of raw `BCPos` nodes, and `LocalBindingScope` gained a depth-aware table so shadowed locals resolve identically to the legacy parser._

6. **Bring test and regression coverage in line with Phase 2 requirements**
   1. Update `parser_unit_tests.cpp` to compile representative snippets (control-flow ladders, function literals, table constructors, compound assignments) through both the legacy path and the AST pipeline, diffing bytecode to prove parity for every node family as coverage lands.【F:docs/plans/PARSER_P2.md†L63-L86】
   2. Extend `parser_phase2.fluid` (and Flute harnesses) to force-enable the AST pipeline during CI so the new emitter runs under real Fluid workloads instead of being guarded by a manual `--jit-options ast-pipeline` flag.
   3. Add targeted regression tests around newly supported constructs whenever emitter work lands, preventing future changes from silently reintroducing unsupported node kinds.
   _Status (Apr 2025): Complete. The parser unit tests now include an `ast_statement_matrix` fixture that feeds representative control-flow, iterator, function, table, and goto snippets through both pipelines and diffs their bytecode snapshots, while the Fluid regression harness forces `glJITPipeline` via `pf_set_ast_pipeline()` and executes an expanded snippet list so CI runs the AST path by default.【F:src/fluid/luajit-2.1/src/parser/lj_parse_tests.cpp†L620-L772】【F:src/fluid/tests/parser_phase2.fluid†L1-L79】_

By executing this plan sequentially—first cataloguing coverage, then expanding expressions, assignments, statements, infrastructure, and finally tests—we can evolve `IrEmitter` into the single owner of bytecode generation mandated by the Phase 2 redesign.
