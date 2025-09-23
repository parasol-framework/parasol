# AST Evaluator Migration Plan

This document outlines the staged work needed to replace the legacy string-based XPath evaluator with the AST-based implementation in `src/xml/xml_search.cpp`. Each phase lists prerequisites, concrete tasks, testing expectations, and completion criteria. Follow the phases in order; do not remove the fallback evaluator until Phase 5 succeeds.

---

## Progress Log
- **Phase 1 (AST Location Path Traversal Backbone)** – Basic traversal implemented in `xpath_evaluator.cpp` (child, self, descendant-or-self axes, callback plumbing). Attribute axis support remains deferred per plan TODO.
- **Phase 2 (Predicate Foundations)** – `evaluate_predicate` now honours numeric position predicates, attribute existence/equality (including `@*` name wildcards) and `[=value]` content checks through the AST pipeline. Unsupported predicate shapes still return `ERR::Failed` to drive the legacy fallback.
- **Phase 3 (Function & Expression Wiring)** – Parser/expression scaffolding landed (`XPathParser` emits `BinaryOp` nodes; `evaluate_expression` / `evaluate_function_call` stubs exist) but everything still defers to the legacy evaluator whenever a predicate requires function or boolean handling.

---

## Phase 0 – Current State & Ground Rules
- File banner in `src/xml/xml_search.cpp` already documents the staged rewrite and that temporary regressions are expected.
- Legacy evaluator is still responsible for production behaviour; AST path should be treated as experimental until parity is reached.
- Every phase must keep the module buildable and the `xml_xpath` test target runnable.
- When a feature is intentionally deferred, add/maintain TODO notes directly in code for traceability.

**Deliverables**
1. Ensure this plan (AST_PLAN.md) stays up to date as work progresses.
2. Keep the git history linear per phase (one or more commits per phase are fine, but they should remain scoped to the phase objectives).

---

## Phase 1 – AST Location Path Traversal Backbone
**Goal:** Make the AST evaluator walk location paths end-to-end (without using the legacy fallback) while supporting callbacks and deep scans.

### Prerequisites
- Understanding of `SimpleXPathEvaluator`, `extXML`, and the existing AST structures (`XPathNode`, `XPathToken`).
- Namespace-prefixed node tests already parse correctly (patched earlier).

### Tasks
1. **Cursor and Context Setup**
   - Extend `SimpleXPathEvaluator` to maintain stacks for `CursorTags` and `Cursor` so AST traversal can drill into child vectors without deferring to the string evaluator.
   - Decide how absolute vs. relative paths initialise traversal (e.g., start from document root when the location path begins with `/`).

2. **Implement `evaluate_location_path`**
   - Interpret `Root` nodes (`/` vs `//`) and set traversal mode (direct child vs. descendant search).
   - Iterate over `Step` nodes in order, invoking a new helper (e.g., `execute_step`) for each.
   - For `//`, insert an implicit `descendant-or-self::node()` step equivalent.

3. **Implement `evaluate_step_ast` for traversal**
   - Determine the candidate node set for the current context using a dedicated axis dispatcher (initial support: `child`, `descendant-or-self`, and implicit `self` when omitted).
   - For each candidate:
     - Update evaluator context (`context.context_node`, `context.position`, `context.size`).
     - Invoke node-test matching (reuse `match_node_test`, updating it if required to work in the AST pipeline).
     - If the step is terminal and matches, either return `ERR::Okay` (no callback) or run the callback hook exactly like the legacy evaluator.
     - If the step is not terminal, recurse into the next step with the candidate as the new context.

4. **Callback Semantics**
   - Match the legacy behaviour: when a callback is provided the traversal must continue even after matches, unless the callback requests termination.
   - Populate `xml->Attrib` for attribute projections (`/@attr` paths) when appropriate.

5. **Attribute Axis Handling**
   - For now, treat attribute node tests as deferred. When encountered, log a TODO and return `ERR::Failed`; the fallback will pick up the slack until Phase 2/3 covers it. Add explicit TODOs in code explaining what remains.

### Testing
- Temporarily add an environment flag or debug log to assert when AST traversal is exercised.
- Run `ctest -C Release -R xml_xpath` after changes.
- Expect numerous failures (predicates/functions not yet implemented). Ensure the new traversal does not crash and that basic paths (without predicates) succeed.

### Exit Criteria
- `evaluate_location_path` no longer returns `ERR::Failed` immediately.
- Simple absolute/relative paths (without predicates) succeed through AST traversal.
- Legacy fallback remains in place for unsupported scenarios.

---

## Phase 2 – Predicate Parity
**Goal:** Support the same predicate semantics inside AST traversal as the legacy evaluator (indexes, attribute filters, Parasol `[=value]`, wildcard attributes, namespace-aware matches).

### Tasks
1. **Index Predicates**
   - ✅ Carry position metadata into predicate evaluation (`context.position`, `context.size`).
   - ✅ Implement numeric `[n]` handling inside the AST evaluator. Future work: ensure fractional / out-of-range operands map to XPath 1.0 semantics once expression parsing lands.

2. **Attribute & Content Predicates**
   - ✅ Re-implement `attribute-equals`, `attribute-exists`, `content-equals`, and wildcard attribute-name logic using AST nodes. Attribute value wildcards currently flow through `pf::wildcmp`; richer namespace-aware comparisons remain TODO.
   - ✅ Support both `"value"` and unquoted `[=value]` predicates (Parasol extension). Multi-node text concatenation still delegates to legacy evaluator until Phase 3 expression support is in place.

3. **Boolean / Comparison Operators**
   - ⬜ Expand predicate evaluation to handle `=`, `!=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `not` using the AST expression tree (delegating to Phase 3 function wiring for node-set semantics).

4. **Round-Bracket Equivalents**
   - ⬜ Ensure alternate predicate syntax `( ... )` remains functional once expression evaluation is wired.

5. **Testing & Regression**
   - 🔄 Continue running `ctest -C Release -R xml_xpath`; update expectations in `test_xpath_queries.fluid` as AST parity improves (boolean/comparison tests still expect fallback errors).

### Exit Criteria
- Predicate-heavy tests from `test_xpath_queries.fluid` pass when related functionality is implemented.
- Indices, attribute filters, and `[=value]` work through AST traversal without falling back. ✅
- Attribute wildcard behaviour matches the legacy path. ✅

---

## Phase 3 – Function & Expression Wiring
**Goal:** Feed correct value types (especially node sets) into the XPath function library so expressions like `count(item)` evaluate properly.

### Status Snapshot
- `SimpleXPathEvaluator::evaluate_expression` / `evaluate_function_call` currently return empty `XPathValue` instances, so any predicate that reaches this path reports `ERR::Failed` and triggers the legacy fallback.
- `XPathParser` already builds `BinaryOp` trees for `and`/`or`/`=`/`!=` tokens, but bare identifiers are still emitted as `Literal` nodes; the evaluator therefore never sees a `Path` / node-set primary when parsing `count(item)`.
- `XPathValue` and `XPathFunctionLibrary` scaffolding exists (`xpath_functions.cpp`), with core conversions implemented and secondary functions (`substring-before`, `translate`, etc.) still tagged with TODO comments for later refinement.

### Tasks
1. **Parser adjustments for node-set operands**
   - Extend `XPathParser::parse_primary_expr` (and related helpers) so that identifiers followed by path separators (`/`, `//`, `::`, predicates) are wrapped in a `XPathNodeType::Path` child rather than a raw `Literal`.
   - When parsing function arguments, ensure `parse_function_call` accepts location paths by delegating to `parse_path_expr` instead of treating them as plain strings; keep TODO notes where grammar coverage remains intentionally partial.
   - Update unit tests or add debug asserts so we can spot cases where predicates still surface as literal strings (a signal that AST parsing fell back to legacy assumptions).

2. **Node-set execution helpers**
   - Introduce a dedicated helper (e.g., `evaluate_path_expression`) that clones the current cursor/context via `push_cursor_state`/`pop_cursor_state` before running `evaluate_step_sequence` on a nested location path, returning a `std::vector<XMLTag *>`.
   - Ensure this helper preserves document order and deduplicates nodes as required by XPath 1.0, mirroring the behaviour of the legacy evaluator’s `count()`/`exists()` flows.
   - Record explicit TODOs if namespace-aware comparisons or attribute projections need extra plumbing so future phases can pick them up.

3. **Expression evaluation core**
   - Implement `SimpleXPathEvaluator::evaluate_expression` to handle at minimum `Number`, `String`, `Literal`, `Path`, `UnaryOp`, `BinaryOp`, and `FunctionCall` nodes. Leverage `XPathValue` conversions for boolean/numeric coercion rather than ad-hoc casts.
   - Extend `evaluate_predicate` so any predicate child that is not a numeric literal delegates to `evaluate_expression`, interpreting the resulting `XPathValue` using XPath truthiness rules (node-set → true if non-empty, number → true if 1, etc.). Unsupported node types must still surface `PredicateResult::Unsupported` to activate the fallback.
   - Add guards to prevent runaway recursion and to guarantee cursor/context stacks unwind on all control-flow paths.

4. **Function invocation pipeline**
   - Flesh out `SimpleXPathEvaluator::evaluate_function_call`: evaluate each argument via `evaluate_expression`, pass them to `function_library.call_function`, and propagate the current `XPathContext` (`context_node`, `position`, `size`).
   - Audit existing `XPathFunctionLibrary` entries to ensure high-priority functions (`count`, `sum`, `string-length`, `boolean`, etc.) return sensible defaults when invoked with malformed arguments. Leave TODO notes on secondary helpers so subsequent phases can prioritise them.
   - Wire function results back into predicate evaluation (e.g., `[count(item)=3]`, `last()`, `position()`), and confirm we still honour legacy callbacks/attribute extraction semantics.

5. **Comparison, boolean, and arithmetic semantics**
   - Expand `evaluate_expression` to interpret `BinaryOp` values of `and`, `or`, `=`, `!=`, `<`, `<=`, `>`, `>=`, `+`, `-`, `*`, `div`, and `mod`, following XPath 1.0 conversion rules. Implement short-circuit behaviour for `and`/`or` once boolean operands are available.
   - Handle `XPathNodeType::UnaryOp` for unary minus and logical negation (in addition to the existing `not()` function), reusing `XPathValue` helpers for conversions.
   - Explicitly cover divide-by-zero, NaN, and Infinity semantics so `ctest -R xml_xpath` expectations match the legacy path. If behaviour differs from W3C spec, document it inline so reviewers understand the divergence.

6. **Union and node-set hygiene**
   - Teach the evaluator to process `XPathNodeType::Union` nodes (`|`) by merging node-set results from both operands, removing duplicates, and preserving document order.
   - Confirm that node-set boolean evaluation aligns with XPath rules when unions feed into predicates (`[count(./item | ./extra)=3]`).

7. **Testing & instrumentation**
   - Re-enable the disabled sections in `test_xpath_queries.fluid` (`testFunctionPredicateNodeSets`, `testXPathFunctions`, boolean/comparison operator blocks) once their prerequisites pass through the AST path.
   - Add targeted tests for regression scenarios discovered during development (e.g., nested function calls, multi-step path arguments, zero-length node sets).
   - Keep verbose logging (`pf::Log`) around the new helpers until the suite is stable so that Phase 4 can diagnose axis issues without reintroducing printf debugging.

### Exit Criteria
- All function-related tests in `test_xpath_queries.fluid` (and `test_basic`, etc.) succeed via AST evaluation without dropping to the legacy fallback.
- No function call or boolean/comparison predicate depends on the string-based evaluator; remaining TODOs are limited to optional XPath 1.0 functions earmarked for later phases.

---

## Phase 4 – Axis & Node Test Completion
**Goal:** Implement the remaining XPath axes and node tests to reach full feature parity.

### Tasks
1. **Axis Support**
   - Implement explicit axes: `self`, `parent`, `ancestor`, `ancestor-or-self`, `descendant`, `descendant-or-self`, `following`, `following-sibling`, `preceding`, `preceding-sibling`, `attribute`.
   - Validate document order where relevant (following/preceding).

2. **Node Tests**
   - Support node type tests (`node()`, `text()`, `comment()`, etc.) and ensure `*` wildcards work with namespaces.

3. **Namespace Matching**
   - Confirm prefix handling uses the namespace registry consistently for both element and attribute nodes.

4. **Custom Extensions**
   - Restore Parasol-specific features (e.g., callback semantics, content extraction) to AST evaluation.

5. **Exhaustive Testing**
   - Run entire XML test suite: `ctest -C Release --verbose -L xml` (or the equivalent aggregate target).
   - Validate performance isn’t significantly degraded.

### Exit Criteria
- All existing XML module tests pass without relying on the legacy evaluator.
- Manual smoke tests (e.g., sample apps or scripts) demonstrate the same behaviour as before.

---

## Phase 5 – Remove Legacy Fallback & Cleanup
**Goal:** Delete the Phase-1 string-based evaluator and tidy up supporting code now that AST evaluation has full parity.

### Tasks
1. Remove `SimpleXPathEvaluator::parse_path`, `match_tag`, `evaluate_step` (string variants) and any helper structures used solely by the legacy path.
2. Simplify `extXML::find_tag` to dispatch only through AST evaluation.
3. Prune redundant TODOs and dead code introduced during earlier phases.
4. Update documentation (both inline and user-facing) to describe the final architecture.
5. Final regression run across the full test matrix.

### Exit Criteria
- Code base no longer references the legacy evaluator.
- All tests pass.
- Documentation reflects the new AST-only implementation.

---

## Ongoing Responsibilities
- Keep regression tests up to date; add new ones when bugs are fixed.
- Monitor performance—if AST evaluation introduces hotspots, profile and optimise.
- Ensure future changes maintain parity; expand AST tests as new features are added.
