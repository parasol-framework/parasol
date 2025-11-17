# LuaJIT Parser Redesign – Phase 1 Implementation Plan

## Objectives
Phase 1 establishes the foundational infrastructure required for the later parser refactors. The focus is isolating parser state, clarifying token handling, and providing structured error reporting while continuing to emit bytecode directly. This plan enumerates the concrete steps needed to deliver those capabilities with minimal disruption to current functionality.

## Deliverables
1. `ParserContext` (and supporting data types) that own parser-wide resources, expose lifecycle helpers, and offer status reporting.
2. A typed token representation (`TokenKind`, payload variants, iterators/lookahead helpers) plus integration with the existing lexer.
3. Context-aware utilities for token consumption and expectation tracking, wired into representative parser entry points.
4. Error collection/reporting scaffolding that enables multiple diagnostics per parse without immediate abort.
5. Migration of a pilot surface area (e.g., expression prefix parsing + a simple statement) to validate the new interfaces.

## Step-by-Step Plan

### 1. Establish the ParserContext scaffolding
1. Inventory ownership boundaries inside `src/fluid/luajit-2.1/src/parser` (LexState, FuncState, GC-managed allocators, diagnostics, VM state hooks). Document the intended lifetime semantics in code comments.
2. Create `parser_context.h/.cpp` that defines `ParserContext` with the following members:
   * References/pointers to `LexState`, `FuncState`, allocator, and `lua_State`.
   * `ParserConfig` struct capturing feature toggles (e.g., future AST gating) and tracing flags.
   * `ParserDiagnostics` object (see step 4) embedded by value to avoid dynamic allocation.
3. Provide RAII-friendly construction helpers:
   * `ParserContext::from(LexState&, FuncState&, Allocator&, ParserConfig)` to initialise derived state (e.g., token stream adapter).
   * `ParserSession` lightweight guard that temporarily overrides context options (useful for nested parses like function literals).
4. Replace direct uses of `LexState*`/`FuncState*` in shared helpers (start with `parse_core.cpp` utilities) to accept `ParserContext&`. Maintain overloads for legacy call sites using inline wrappers while migration is in progress.
5. Add `ParserResult<T>` alias (using `std::expected<T, ParserError>` or bespoke struct) along with convenience constructors `ok(T)` and `fail(ParserError)`. Ensure headers avoid C++ exception semantics per repository rules.

### 2. Build the typed token system
1. Define `enum class TokenKind` in `token_types.h` that covers all tokens produced by `lex_next` plus Fluid extensions (defer, ??, ?=, continue, etc.). Align names with existing `TK_*` constants to simplify the migration map.
2. Implement `struct TokenPayload` as a tagged union (e.g., `std::variant<std::monostate, double, TValue*, GCstr*, BCIns>`). Provide helper constructors (`Token::number(double)`, `Token::name(GCstr*)`, etc.) that perform any necessary conversions.
3. Create `class Token` encapsulating `TokenKind kind`, `TokenPayload payload`, source span metadata (line, column, absolute offset), and helper predicates (`is_identifier()`, `is_literal()`).
4. Write `TokenStreamAdapter` in `token_stream.h/.cpp` that wraps the legacy lexer:
   * `Token TokenStreamAdapter::current() const;`
   * `Token TokenStreamAdapter::peek(std::size_t lookahead);`
   * `Token TokenStreamAdapter::advance();`
   * `void TokenStreamAdapter::sync_from_lex(LexState&)` for recovery.
5. Integrate the adapter into `ParserContext` so consumers can access `context.tokens()` instead of touching `LexState` directly. Maintain backwards compatibility by exposing the underlying `LexState&` for code that has not migrated yet.
6. Update diagnostic helpers to accept `Token` spans to support future multi-token error messages.

### 3. Implement context-aware token utilities
1. Add member functions on `ParserContext` (or a nested `TokenCursor`) for recurring patterns:
   * `bool match(TokenKind)` – advances when the current token matches, records expectation otherwise.
   * `Token consume(TokenKind, ParserErrorCode)` – asserts presence and emits structured error entries via diagnostics when absent.
   * `bool check(TokenKind)` – lookahead without advancing.
   * `Token expect_identifier(ParserErrorCode)` – ensures identifier payload and emits consistent messaging.
2. Ensure these helpers emit `ParserResult<Token>` so callers can branch on success rather than relying on `lua_assert`.
3. Replace ad-hoc checks in a pilot set of functions (e.g., `expr_primary`, `expr_prefix`, and `stmt_local`) to use the new helpers, keeping behaviour identical while reducing direct lexing.
4. Add unit-style regression tests (or targeted Fluid scripts executed via `parasol`) that cover typical success/failure paths to verify no behaviour drift. Capture commands in plan for future automation.

### 4. Introduce structured diagnostics
1. Define `ParserDiagnostic` struct capturing severity, message ID/enum, formatted message, and source span.
2. Implement `ParserDiagnostics` container that stores a bounded vector (configurable in `ParserConfig`). Provide APIs:
   * `void report(ParserDiagnostic)`
   * `bool has_errors() const`
   * `std::span<const ParserDiagnostic> entries() const`
3. Extend `ParserContext` to expose `diagnostics()` for read-only access and `emit_error(...)` helpers that format messages from tokens plus extra context.
4. Retrofit existing fatal error pathways (`lj_lex_error`, `lj_parse_error`) to delegate to `ParserDiagnostics` while still triggering the old behaviour (e.g., abort on first error) until a later phase relaxes it. This ensures instrumentation is live even before multi-error support ships.
5. Document new diagnostic codes/messages in-line.

### 5. Validate via pilot migrations
1. Select representative parser entry points:
   * Expression path: `expr_primary` + `expr_prefix`.
   * Statement path: `stmt_local` or another simple construct without complex register interactions.
2. Convert these functions to:
   * Receive `ParserContext&`.
   * Use the typed token helpers for control flow.
   * Return `ParserResult<ExpDesc>` (or equivalent) to surface parsing failures upstream.
3. Update callers and surrounding helpers to propagate `ParserResult` without altering bytecode emission.
4. Add targeted unit/integration tests that parse contrived snippets covering identifiers, literals, and failure cases to demonstrate diagnostics accumulation.
5. Write migration notes (inline comments + to this document if necessary) capturing patterns discovered during the pilot, informing later phases on anticipated complexities.

### 6. Integration and hardening tasks
1. Add build-time toggles (e.g., `PARASOL_PARSER_TRACE`) to gate verbose tracing via `ParserContext` for debugging.
2. Ensure new headers are included in `CMakeLists.txt` and `pkg` exports so downstream modules can reference them when the parser becomes reusable.
3. Run the existing parser regression suites (Fluid scripts, if available) to confirm the Phase 1 changes are non-breaking. Capture invocation commands in commit messages for traceability.
4. Perform code-style and rule compliance checks (British English spelling, macro usage, `and`/`or`, `IS` macros) before landing.
5. Update this document “Next Steps” once Phase 1 completes to reflect the availability of the new infrastructure.

## Dependencies and Risks
* **Incremental adoption:** Maintain compatibility layers (inline wrappers, fallback error paths) so we can migrate subsystems gradually without touching every parser file at once.
* **Performance impact:** Introduce profiling hooks early to ensure the typed token adapter does not degrade hot paths. Consider small-object optimisations for `Token` payload storage.
* **Memory lifetime:** Clarify whether `Token` payloads own or reference Lua VM data (strings, TValue). Document invariants to avoid dangling references when GC runs during parsing.

## Success Criteria
* Parser helpers compile against `ParserContext` without direct access to global parser structs for the migrated pilot areas.
* Typed tokens, diagnostics, and context utilities are covered by tests and used in at least one expression + statement path.
* Legacy behaviour (single-error abort, direct bytecode emission) remains intact, ensuring later phases can focus on AST/IR work with confidence in the new scaffolding.

## Phase 1 Status (Implemented)
* Introduced `ParserContext`, diagnostics, and typed token stream scaffolding. `expr_primary`, the prefix path (`expr_simple` / `expr_unop` / `expr_binop`), and `parse_local` now use the new helpers while retaining legacy fallbacks so failures surface through `ParserResult` before legacy error handling runs.
* Added a lightweight Fluid regression script `src/fluid/tests/parser_phase1.fluid`. Build the Fluid module (`cmake --build build/agents --config Release --target fluid -- -j4`) and run `parasol --log-warning src/fluid/tests/parser_phase1.fluid` to validate both successful execution paths and negative cases covering missing local names, broken expressions, and control-flow syntax errors. The script output doubles as a quick diagnostic sanity check when `ParserResult`-based helpers change.
* Introduced the `PARASOL_PARSER_TRACE` CMake option, which flips `ParserConfig::trace_tokens` and `trace_expectations` defaults so `TokenStreamAdapter::advance`, `ParserContext::match`, and `ParserContext::emit_error` emit human-readable traces for token flow and diagnostics. The option is documented in this plan and `docs/wiki/Unit-Testing.md` so developers can enable it explicitly during debugging sessions.
* Parser helper utilities (`lex_*`, `err_*`, and `err_limit`) now route through `ParserContext` when an active context exists, and `parse_stmt` activates the shared context so statement parsing benefits from the new diagnostics and token adapters.
* `lj_parse` now builds a long-lived `ParserContext` for the main chunk, enters a `ParserSession` to honour build-time `ParserConfig` overrides, and reuses that context across every statement. Nested function bodies spawn their own sessions that inherit the parent configuration, and `parse_chunk` summarises collected diagnostics when `abort_on_error` is disabled so multiple errors surface together.
* Token spans now carry accurate line, column, and byte offsets sourced from the lexer, and `TokenStreamAdapter::peek()` can materialise arbitrary lookahead distances via the buffered token queue, completing the remaining Phase 1 deliverables.
