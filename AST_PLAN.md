# AST Evaluator Migration Plan

This document outlines the staged work needed to replace the legacy string-based XPath evaluator with the AST-based implementation in `src/xml/xml_search.cpp`. Each phase lists prerequisites, concrete tasks, testing expectations, and completion criteria. Follow the phases in order; do not remove the fallback evaluator until Phase 5 succeeds.

---

## Progress Log
- **Phase 1 (AST Location Path Traversal Backbone)** – Basic traversal implemented in `xpath_evaluator.cpp` (child, self, descendant-or-self axes, callback plumbing). Attribute axis support remains deferred per plan TODO.

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
   - Carry position metadata into predicate evaluation (`context.position`, `context.size`).
   - Implement numeric `[n]` handling inside the AST evaluator instead of returning true only for `n == 1`.

2. **Attribute & Content Predicates**
   - Re-implement `attribute-equals`, `attribute-exists`, `content-equals`, and wildcard attribute logic using AST nodes.
   - Support both `"value"` and unquoted `[=value]` predicates (Parasol extension).

3. **Boolean / Comparison Operators**
   - Expand predicate evaluation to handle `=`, `!=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `not` using the AST expression tree (delegating to Phase 3 function wiring for node-set semantics).

4. **Round-Bracket Equivalents**
   - Ensure alternate predicate syntax `( ... )` remains functional.

5. **Testing & Regression**
   - Re-enable/verify tests such as `testAttributeWildcards`, `testContentMatching`, `testNamespaceSupport` (predicates relying on namespace prefixes).

### Exit Criteria
- Predicate-heavy tests from `test_xpath_queries.fluid` pass when related functionality is implemented.
- Indices, attribute filters, and `[=value]` work through AST traversal without falling back.
- Attribute wildcard behaviour matches the legacy path.

---

## Phase 3 – Function & Expression Wiring
**Goal:** Feed correct value types (especially node sets) into the XPath function library so expressions like `count(item)` evaluate properly.

### Tasks
1. **Node-Set Primary Expressions**
   - Teach `parse_primary_expression` / `evaluate_expression` to recognise path expressions and evaluate them to node sets rather than string literals.
   - Introduce helper functions to execute sub-paths within the current context safely.

2. **Function Evaluation Context**
   - Populate `XPathContext` (`position`, `size`, `context_node`) accurately before invoking library routines.
   - Ensure the library functions (`count`, `sum`, `starts-with`, etc.) receive properly typed arguments.

3. **Arithmetic & Comparison Normalisation**
   - Handle numeric/string coercion per XPath 1.0 rules.
   - Address divide-by-zero, NaN, and Infinity semantics similarly to the legacy implementation.

4. **Union & Set Operations**
   - Verify unions (`|`) and boolean operations on node sets behave consistently.

5. **Testing**
   - `testFunctionPredicateNodeSets`, `testXPathFunctions`, and arithmetic/operator tests should now pass.
   - Add new regression tests if edge cases are discovered.

### Exit Criteria
- All function-related tests in `test_xpath_queries.fluid` (and `test_basic`, etc.) succeed via AST evaluation.
- No function calls depend on the legacy fallback.

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
