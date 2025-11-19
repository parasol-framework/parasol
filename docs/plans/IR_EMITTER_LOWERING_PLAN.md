# IrEmitter Lowering Plan

## Goal
Bring the AST-to-bytecode emitter in `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` closer to the Phase 2 mandate by covering the core statement/expression forms that the AST builder already produces. This work unblocks the requirement for `IrEmitter` to own bytecode emission for general constructs instead of falling back to the legacy parser.【F:docs/plans/LUAJIT_PARSER_REDESIGN.md†L40-L44】【F:docs/plans/PARSER_P2_ISSUES.md†L10-L12】

## Tasks
1. **Extend expression lowering coverage** – Add visitors for unary expressions, member/index lookups, and call expressions (including method dispatch and multi-return argument forwarding). Map every `AstBinaryOperator` to the corresponding `BinOpr` so arithmetic, comparison, logical and `??` operators lower through `bcemit_binop` instead of stopping at `unsupported_expr`.
2. **Complete return lowering** – Mirror the legacy parser’s `return` semantics by supporting multi-value lists, tail calls, and `BC_RETM` emission so the emitter handles the standard `return expr[, ...]` matrix instead of only bare returns.

   *2025-11-19 update:* `IrEmitter::emit_return_stmt` now mirrors `LexState::parse_return` by marking `PROTO_HAS_RETURN`, emitting `BC_RET0/RET1/RET/RETM`, and downgrading single-call returns to tail calls. The parser unit tests gained `return_lowering` to lock the legacy/AST parity for bare, vararg, tail-call, and multi-value returns.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L166-L210】【F:src/fluid/luajit-2.1/src/parser/lj_parse_tests.cpp†L602-L690】
3. **Add regression coverage** – Introduce a parser unit test that exercises a call expression (member lookup + unary operand) under the AST pipeline to prove the new lowering path matches the legacy bytecode output, preventing regressions as more nodes gain coverage.

   *2025-11-19 update:* `test_ast_call_lowering` in `lj_parse_tests.cpp` now compiles a chunk that performs method dispatch via `context:compute(-3)`, ensuring the AST pipeline emits the same bytecode as the legacy parser for a member call with a unary operand and guarding future call-lowering changes.【F:src/fluid/luajit-2.1/src/parser/lj_parse_tests.cpp†L617-L651】
