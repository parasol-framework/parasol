# Phase 2 Completion Gaps

This document captures the outstanding requirements discovered while auditing the current Phase 2 implementation against `docs/plans/LUAJIT_PARSER_REDESIGN.md` and `docs/plans/PARSER_P2.md` (specifically the "Testing, validation, and performance safeguards" step).

## 1. Parser entry points still mutate `FuncState`
* `parse_expr.cpp` continues to implement `LexState::expr_*` helpers that immediately manipulate `ExpDesc` and interact with `FuncState` (e.g., `expr_field`, `expr_bracket`, `expr_table`). These functions emit bytecode, reserve registers, and consume `FuncState::freereg`, which contradicts Step 2's requirement for expression/statement helpers to return AST nodes while leaving `FuncState` untouched during parsing.【F:src/fluid/luajit-2.1/src/parser/parse_expr.cpp†L19-L199】

## 2. `IrEmitter` lacks comprehensive lowering support
* The `IrEmitter` visitor only recognises expression statements, bare `return` statements, and three expression kinds (literal, identifier, vararg). Every other AST node routes through `unsupported_stmt`/`unsupported_expr`, so the emitter neither owns register allocation for most constructs nor handles control-flow patching as mandated by Step 3.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L39-L172】

## 3. Bytecode equivalence harness skips parity checks
* The "bytecode equivalence" unit test in `lj_parse_tests.cpp` warns and exits early whenever the AST pipeline fails to compile the snippet, meaning no bytecode diff is performed even though Step 5 requires enforcing equivalence to catch regressions. This undermines the safeguard because AST failures silently skip comparison instead of failing the test.【F:src/fluid/luajit-2.1/src/parser/lj_parse_tests.cpp†L464-L499】

## 4. Performance profiling hooks and `--jit-options profile` flag are absent
* `lj_parse.cpp` configures parser diagnostics via `trace`, `diagnose`, `ast-pipeline`, `trace-boundary`, and `trace-bytecode` flags, but there is no `profile` option nor any timing instrumentation around AST construction or emission. Consequently, the profiling safeguard described in Step 5 is missing, and parse/emission duration cannot be measured or gated.【F:src/fluid/luajit-2.1/src/parser/lj_parse.cpp†L92-L156】

These issues confirm that Phase 2 of the LuaJIT parser redesign is not yet complete, particularly around the testing, validation, and performance safeguards that gate the AST pipeline rollout.
