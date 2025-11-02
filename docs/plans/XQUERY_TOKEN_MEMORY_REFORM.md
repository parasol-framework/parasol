# XQuery Token Memory Reform

**Status**: Proposed  
**Priority**: 4  
**Complexity**: Medium–High  
**Estimated Effort**: 5–7 days (one developer)  
**Risk Level**: Moderate

---

## Executive Summary

The current tokenizer builds a full `std::vector<XPathToken>` where each token holds either a borrowed slice of the input string or an owned `std::string`. This dual-storage model complicates lifetime management, scatters token text across the heap, and forces the tokenizer to pay allocation costs even when parsing fails early. This plan introduces a unified token storage strategy that guarantees safe lifetimes, improves cache locality, and reduces allocator churn without rewriting the parser into a streaming coroutine.

---

## Objectives

1. Eliminate the `string_view`/`stored_value` split inside `XPathToken`; every token should reference text stored in a controllable, long-lived buffer.
2. Provide contiguous, reusable storage for all token text (including attribute value template fragments) to improve locality and reduce heap fragmentation.
3. Ensure the storage buffer travels with the token list so copies performed by `XPathParser` remain valid.
4. Reduce tokenisation overhead for failure paths and repeated invocations by reusing buffers where possible.
5. Preserve existing parser APIs and semantics to limit integration risk.

---

## Scope & Non-Goals

- In scope: tokenizer, `XPathToken` definition, parser token ingestion, attribute value template storage, unit tests touching token lifetimes.
- Out of scope: converting the parser to a streaming interface, altering AST structures, or rewriting the tokeniser’s lexical rules.
- Keep British English spelling and existing coding standards (IS macro, `and`/`or`, three-space indentation).

---

## Stage 0: Baseline Verification (0.5 day)

1. Capture allocator and timing profiles for representative queries (short literal, FLWOR, deep constructors).  
2. Document the number of heap allocations attributable to tokenisation and the proportion caused by escaped strings/attribute templates.  
3. Use these numbers as success metrics for later validation.

---

## Stage 1: Shared Token Storage Infrastructure (1–2 days)

1. **Introduce `TokenBuffer`**  
   - New struct owning a single `std::vector<char>` plus offset cursor and reset logic.  
   - Expose `write_copy(std::string_view)` returning `std::string_view` into the buffer.  
   - Reserve using geometric growth (start at 4 KB, double on demand) and expose `shrink_to_fit()` for tests.

2. **Define `TokenBlock`**  
   - Wrapper holding both `TokenBuffer` (shared via `std::shared_ptr`) and `std::vector<XPathToken>`.  
   - Provide move-friendly API so callers can hand tokens plus backing storage as a unit.

3. **Augment `XPathToken`**  
   - Replace `stored_value` with a single `std::string_view text`.  
   - Add `TokenTextKind` enum to describe the source (`BorrowedInput`, `ArenaOwned`).  
   - Update constructors to accept a `std::string_view` and optional `TokenTextKind`; remove duplicate storage logic.

4. **Parser Interface Adjustments**  
   - Change `XPathParser::parse` signature to accept a `TokenBlock` (or references to both token vector and shared storage).  
   - Inside `parse`, `std::move` the vector into the parser member to avoid deep copies while keeping the shared buffer alive.

---

## Stage 2: Tokeniser Adaptation (2 days)

1. **Integrate `TokenBuffer`**  
   - `XPathTokeniser::tokenize` now returns `TokenBlock`.  
   - For tokens that can safely borrow the source (identifiers, numbers, structural tokens), store `BorrowedInput` labels so the parser knows copying isn’t required if the source string survives.  
   - For tokens requiring transformation (escaped string literals, attribute value templates, normalised operators), copy into the arena using `write_copy`.

2. **Attribute Value Templates**  
   - Store `XPathAttributeValuePart::text` as views into `TokenBuffer` instead of independent `std::string` instances.  
   - Provide helper on `TokenBlock` to duplicate text when the parser needs ownership beyond token life (e.g., AST constructors may still require `std::string`).

3. **Failure-Path Optimisation**  
   - Ensure buffer offset rolls back when `tokenize` is called repeatedly (either reuse the buffer after `reset()` or allow caller to supply one).  
   - Update unit tests to exercise re-entrant calls and confirm no stale pointers remain after `reset()`.

---

## Stage 3: Parser Consumption & Safety (1 day)

1. **Parser Copy Semantics**  
   - Modify parser to retain a `std::shared_ptr<TokenBuffer>` alongside its token vector so borrowed views remain valid during parsing.  
   - Where the parser materialises AST strings (e.g., `parse_qname_string`, `parse_string_literal_value`), copy from the token view into the AST-owned `std::string` as today.

2. **Embedded Parser Uses**  
   - Update sites that spawn nested tokenisers (`parse_embedded_expr`, prolog import parsing) to propagate the `TokenBlock`.  
   - Audit helper utilities (tests, prolog evaluators) to ensure they handle the new return type.

3. **Attribute Template Flow**  
   - Confirm the evaluator still receives heap-owned strings where required (e.g., attribute constructors). Introduce explicit `duplicate_attribute_parts()` that copies from the buffer into AST-owned storage at the handoff point.

---

## Stage 4: Validation & Documentation (1 day)

1. **Build & Test**  
   - `cmake --build build/agents --config FastBuild --target xquery --parallel`  
   - `cmake --install build/agents`  
   - `ctest --build-config FastBuild --test-dir build/agents -L xquery`

2. **Benchmark Comparison**  
   - Re-run Stage 0 scenarios, confirm ≥30 % reduction in tokenisation allocations and measurable latency reduction on long inputs.  
   - Validate that repeated parsing of the same string now reuses buffers without leaks.

3. **Documentation**  
   - Update code comments near `XPathToken`, `XPathTokeniser`, and parser entry points describing the new ownership model.  
   - Append summary results to this plan file (including before/after allocation counts).

---

## Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
| --- | --- | --- | --- |
| Borrowed input becomes invalid (e.g., caller hands temporary string) | Medium | High | Document requirement; provide overload taking `std::string` to internalise input when needed; default API copies into buffer when caller passes temporary. |
| Shared pointer overhead negates gains for tiny queries | Low | Low | Small shim adds one atomic refcount; benchmark to confirm negligible impact. |
| Attribute value templates still trigger copies later | Medium | Low | Dedicated duplication function ensures copying happens once at AST construction, not per token. |
| Regression in tokeniser correctness | Low | High | Preserve existing lexical logic; add unit tests for escaped strings and attribute templates referencing arena storage. |

---

## Exit Criteria

- `XPathToken` no longer holds redundant storage; all text references come from either the original input or a shared `TokenBuffer`.
- Parser retains token storage for its lifetime and no dangling views are possible.  
- Attribute value template parts use pooled storage with explicit duplication when moving into the AST.  
- Tokenisation allocation count reduced by at least 30 % on benchmark scenarios; performance documentation appended here.  
- All xquery-labelled tests pass, and internal benchmarks show no regression relative to the baseline.

