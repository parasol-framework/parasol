# Plan: Reserving `?` for ternaries by moving if-empty to `??`

## Objective
Adopt the `??` token for the binary if-empty operator so that single `?` tokens always introduce ternary expressions. Completing this migration allows the parser to drop `lookahead_has_top_level_ternary_sep()` and simplifies ternary handling in `expr_binop()` while keeping the postfix presence operator intact.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L740-L756】【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L761-L831】【F:src/fluid/luajit-2.1/src/lj_lex.c†L402-L411】

## Current behaviour
* `token2binop()` now maps the lexed `??` token (`TK_if_empty`) to `OPR_IF_EMPTY`, while single `?` tokens translate to `OPR_TERNARY`, eliminating the ambiguity that previously required a lexer lookahead.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L754-L769】
* `lookahead_has_top_level_ternary_sep()` remains available during Phase 1 but is no longer used to disambiguate `?`; ternary handling proceeds when `OPR_TERNARY` is encountered, and `OPR_IF_EMPTY` is always treated as the binary if-empty operator.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L987-L1047】
* Postfix presence checks reuse the same `??` token but rely on `should_emit_presence()` to ensure the operator is consumed only when no value-expression follows, preserving the old semantics of `value??` without impacting binary parsing.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L559-L585】【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L948-L1001】
* The if-empty operator continues to depend on `bcemit_binop_left()`/`bcemit_binop()` for register management and extended-falsey evaluation; those helpers operate unchanged against the new token stream.【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L123-L189】【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L408-L520】

## Strategy
Reserve `?` exclusively for ternary expressions by lexing and parsing `??` as the binary if-empty operator. Preserve postfix presence semantics by teaching the parser to distinguish infix versus postfix usage and then remove the bespoke ternary lookahead once no ambiguity remains.【F:src/fluid/luajit-2.1/src/lj_lex.c†L402-L411】【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L761-L831】

## Phase 1 – Switch binary if-empty to `??`
**Goal:** Re-tokenise and parse `value ?? fallback` as `OPR_IF_EMPTY` while updating Fluid tests to the new spelling. Phase 1 completes when the modified test suite passes.

**Status:** Implementation is merged on this branch. The Release `luajit_codegen` target was rebuilt to validate the parser changes; a full Fluid regression run remains outstanding until the broader Parasol build is practical.

1. **Extend lexer tokenisation.** The `case '?'` branch in `lj_lex.c` now emits `TK_if_empty` only when two consecutive `?` characters are read, returning the bare `'?'` token otherwise so ternaries and safe-indexing continue to work.【F:src/fluid/luajit-2.1/src/lj_lex.c†L402-L411】
2. **Disambiguate parser entry points.** `expr_primary()` and `expr_unop()` call `should_emit_presence()` to consume postfix `??` only when the next token cannot begin an expression, otherwise leaving the operator for `expr_binop()` to treat as binary if-empty.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L559-L585】【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L948-L1001】
3. **Ensure operator helpers still align.** `bcemit_binop_left()`/`bcemit_binop()` continue to receive `OPR_IF_EMPTY` for `value ?? fallback`, reusing the existing register bookkeeping without modification.【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L123-L189】【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L408-L520】
4. **Refresh Fluid tests and docs.** Core language tests and bundled Fluid scripts have been converted to the `value ?? fallback` spelling, while preserving dedicated coverage for postfix `value??` semantics and ternary interactions.
5. **Verify the suite.** Build at least the `luajit_codegen` target (or the full Fluid module) and run the Fluid-focused tests once the interpreter is rebuilt to ensure the new spelling behaves as expected.

## Phase 2 – Remove ternary lookahead
**Goal:** Deprecate `lookahead_has_top_level_ternary_sep()` now that `?` is unambiguous and validate parser behaviour without changing existing ternary tests.

1. **Excise the lookahead.** Delete `lookahead_has_top_level_ternary_sep()` and simplify the `OPR_IF_EMPTY` branch in `expr_binop()` so ternary handling proceeds immediately after consuming `?`, relying on the new lexer guarantees.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L761-L1067】
2. **Clean up call sites.** Remove any remaining references to the lookahead helper across the parser and adjust documentation or comments that previously described the ambiguity resolution.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L761-L1067】
3. **Regression testing.** Re-run the parser and Fluid suites to ensure ternary expressions (including nested forms) still behave correctly without the lookahead. No new test files are required because ternary syntax remains unchanged.

## Risks and mitigations
* **Token ambiguity:** Carefully validate the lexer changes with scripts that use `value??`, `value ?? fallback`, and whitespace/comment variations to ensure postfix and infix forms are classified correctly before removing the lookahead.【F:src/fluid/luajit-2.1/src/lj_lex.c†L402-L411】
* **Presence operator regressions:** Ensure `expr_unop()` retains its current fast path for postfix `??` so property access chains (`foo.bar??`) and method calls continue to succeed; add targeted regression tests if necessary.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L945-L972】
* **Language migration:** Communicate the spelling change broadly and provide automated rewrites or linting where possible so existing Fluid codebases adopt `value ?? fallback` seamlessly.
