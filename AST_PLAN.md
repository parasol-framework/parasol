# AST Evaluator Migration Plan

This document outlines the staged work needed to replace the legacy string-based XPath evaluator with the AST-based implementation in `src/xml/xml_search.cpp`. Each phase lists prerequisites, concrete tasks, testing expectations, and completion criteria. Follow the phases in order; do not remove the fallback evaluator until Phase 5 succeeds.

---

## Progress Log
- **Phase 1 (AST Location Path Traversal Backbone)** – Basic traversal implemented in `xpath_evaluator.cpp` (child, self, descendant-or-self axes, callback plumbing). Attribute axis support remains deferred per plan TODO.
- **Phase 2 (Predicate Foundations)** – `evaluate_predicate` now honours numeric position predicates, attribute existence/equality (including `@*` name wildcards) and `[=value]` content checks through the AST pipeline. Unsupported predicate shapes still return `ERR::Failed` to drive the legacy fallback.
- **Phase 3 (Function & Expression Wiring)** – Boolean logic, arithmetic, relational comparisons, and path expressions now evaluate through `evaluate_expression`, delivering correctly typed values to functions like `count()` without falling back to the legacy evaluator.

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
**Goal:** Feed correct value types (especially node sets) into the XPath function library so predicates, boolean logic, and arithmetic/comparison expressions behave exactly like the legacy evaluator.

### Detailed Workstreams
1. **Parser & AST Enhancements**
   - Extend the expression grammar to build full binary/unary operator trees:
     - wire the existing `parse_relational_expr`, `parse_additive_expr`, `parse_multiplicative_expr`, `parse_unary_expr`, and `parse_path_expr` stubs so the precedence chain becomes `or → and → equality → relational → additive → multiplicative → unary → primary`.
     - Allow `path` expressions (absolute, relative, abbreviations, union) to surface as primary nodes so predicates like `[count(child::*) > 0]` generate the correct AST.
     - Preserve round-bracket groupings `( … )`, unary minus, and the XPath keywords `div`, `mod`, and `not` as dedicated tokens so evaluation can recognise intent without relying on string matching.

2. **Expression Evaluation Engine**
   - Implement `evaluate_expression` (and helpers such as `evaluate_binary_expression`, `evaluate_unary_expression`, `evaluate_path_expression`) in `xpath_evaluator.cpp` to:
     - execute sub-paths inside predicates/functions using the AST traversal stack (producing node-set `XPathValue`s without disturbing the caller’s cursor state).
     - perform XPath 1.0 type promotion rules for booleans, numbers, strings, and node sets when evaluating comparison or arithmetic operators.
     - honour short-circuit evaluation for `and`/`or`, `not()` semantics, and division/modulo special cases (e.g., divide-by-zero → IEEE Inf/NaN behaviour matching the legacy evaluator).
   - ✅ Initial equality comparison and function-call bridging implemented so predicates like `count(item)=3` evaluate via AST.

3. **Function Invocation & Context**
   - Teach `evaluate_function_call` to evaluate argument expressions through the new pipeline, populate `XPathContext` (`context_node`, `position`, `size`) for each invocation, and convert node-set arguments to the forms expected by `xpath_functions.cpp`.
   - Add helper hooks (e.g., `evaluate_argument_list`) to ensure functions like `count()` and `starts-with()` receive fully typed `XPathValue` objects.

4. **Predicate Integration**
   - Update `evaluate_predicate` so unsupported branches now pipe into `evaluate_expression`, leveraging the new evaluation engine for comparisons (`=`, `!=`, `<`, `<=`, `>`, `>=`) and boolean combinations (`and`, `or`, `not`).
   - Ensure round-bracket predicates `( … )` and arithmetic tests (e.g., `[@price + @tax = 110]`) succeed without delegating to the legacy evaluator.
   - ✅ Equality predicates (`=`) now dispatch through `evaluate_expression`; other operators still pending.

5. **Node-Set Set Operations**
   - Implement union (`|`) and boolean tests on node sets via the AST pathway, including document-order deduplication where required.
   - Provide stable helpers to merge node-set results returned by sub-expressions.

6. **Testing & Validation**
   - Primary suite: `ctest --build-config Release --test-dir build/agents -R xml_xpath` should pass end-to-end with only the known arithmetic tests still gating future work (if any remain).
   - Add targeted regression cases (Fluid-level) for arithmetic predicates, nested function calls, and node-set unions if gaps are discovered while implementing the above.

### Exit Criteria
- AST evaluator executes function calls, boolean logic, and arithmetic/comparison predicates without falling back to the legacy code path.
- `xml_xpath` and `xml_manipulation` tests that depend on function/arithmetics (e.g., `testFunctionPredicateNodeSets`, `testXPathFunctions`, `testMathematicalExpressions`) pass via the AST evaluator using the agent build.
- The legacy string evaluator is no longer invoked for expression-only predicates (still present for future phases involving unsupported axes).

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
