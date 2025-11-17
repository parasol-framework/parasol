# Plan: Remediate Outstanding Phase 1 Requirements

The following items capture the gaps identified while auditing Phase 1 of the LuaJIT parser redesign. Each section summarises the outstanding work and lists concrete steps to resolve it so the next session can drive Phase 1 to completion.

## 1. Adopt `ParserContext` Across Parser Helpers
- Only `expr_primary_with_context` and `parse_local` currently use `ParserContext`; every other helper still manipulates `LexState*` and `FuncState*` directly.
- **Actions:**
  1. Inventory the shared helpers under `src/fluid/luajit-2.1/src/parser/` (especially `parse_core.cpp` utilities) and refactor them to accept `ParserContext&` instead of naked state pointers.
  2. Update their callers to thread the context object, ensuring diagnostics and allocator access go through the new API.
  3. Remove any remaining direct access to `ParserSession::lex`/`func` internals to guarantee consistent state ownership.

## 2. Complete the Pilot Migration to `ParserResult`
- Phase 1 required both the `expr_primary`/`expr_prefix` pair and a statement entry point to return `ParserResult`. Today only `expr_primary` wraps legacy logic, and `parse_local` still returns `void` while mutating legacy structs.
- **Actions:**
  1. Rewrite `expr_prefix` and at least one statement parser (e.g., `parse_local` or another small statement) to return `ParserResult` objects that encapsulate success/failure and produced AST/descriptor data.
  2. Propagate those results through their callers, eliminating direct mutation of `ExpDesc`/`FuncState` where the plan called for result passing.
  3. Extend unit/regression tests (or add temporary asserts) to ensure callers respect the new result-based control flow.

## 3. Integrate `ParserDiagnostics` With Error Paths
- The plan mandated funnelling `lj_lex_error`, `lj_parse_error`, etc. through `ParserDiagnostics` so multiple diagnostics can accumulate, but only `expr_primary_with_context` uses it today.
- **Actions:**
  1. Replace direct calls to `lj_lex_error`, `lj_parse_error`, and similar fatal helpers with `ParserDiagnostics::emit_error` (or equivalent) throughout the Phase 1 scope.
  2. Ensure each parser entry point checks the diagnostics object before returning so callers can react appropriately.
  3. Add regression coverage (or logging hooks) to confirm multiple diagnostics can be recorded before the parser aborts.

## 4. Consume the `ParserSession` RAII Guard
- `ParserSession` exists but has zero call sites, so configuration overrides never apply.
- **Actions:**
  1. Identify the parser entry points (likely in `lj_parse.cpp`) where sessions should be established and wrap parsing invocations in `ParserSession` instances.
  2. Thread the resulting session/context into downstream helpers so scoped options (e.g., diagnostic modes, allocator settings) take effect.
  3. Document the intended usage in `docs/plans/PARSER_P1.md` once the guard is active, clarifying how sessions should be managed for future migrations.