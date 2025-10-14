# Research & Design – XPath 2.0 FLWOR Clause Completion

## Current State Observations
- The tokenizer recognises only the original XPath 1.0 keywords (`for`, `let`, etc.) and does not expose tokens for `where`, `order`, `group`, `count`, or ordering modifiers, so the parser cannot even detect the additional clauses yet.【F:src/xpath/xpath_tokenizer.cpp†L20-L64】
- `parse_flwor_expr` currently accepts a leading block of `for`/`let` bindings and immediately expects a `return` clause, producing either a legacy `FOR_EXPRESSION`, a `LET_EXPRESSION`, or a basic `FLWOR_EXPRESSION` node with only binding children.【F:src/xpath/xpath_parser.cpp†L500-L651】
- The evaluator short-circuits FLWOR handling once it has iterated over `for`/`let` bindings. It requires the `return` expression to yield a node-set and emits descriptive errors for any other clause type, so the runtime has no hooks for `where`, `order by`, `group by`, or `count` yet.【F:src/xpath/xpath_evaluator_values.cpp†L1905-L2096】
- The Fluid suite already sketches the desired end state—`test_xpath_flwor_clauses.fluid` exercises filtering, ordering, grouping, counting, syntax validation, and error handling for the new clauses—but the tests currently print “not yet implemented” notices and rely on behaviour that does not exist.【F:src/xml/tests/test_xpath_flwor_clauses.fluid†L1-L200】

## Step-by-Step Plan for Steps 2–7

### Step 2 – AST Extensions
1. **Extend the FDL definition** (`src/xpath/xpath.fdl`) with clause-specific node types:
   - `WHERE_CLAUSE`, `ORDER_CLAUSE`, `ORDER_SPEC`, `GROUP_CLAUSE`, `GROUP_KEY`, and `COUNT_CLAUSE` so every clause is distinguishable in the AST.
   - Regenerate headers (`include/parasol/modules/xpath.h`, `src/xpath/xpath_def.c`) by running the existing CMake `BUILD_DEFS` pipeline to keep enum ordinals in sync.
2. **Augment `XPathNode`** in `include/parasol/modules/xpath.h` to carry structured metadata where needed:
   - Provide storage for `ORDER_SPEC` modifiers (ascending/descending, `empty greatest/least`, explicit collation URI) via a lightweight struct owned by the node.
   - Allow `GROUP_KEY` nodes to expose both the grouping variable name and the associated expression child without overloading the generic `value` string.
3. **Document AST expectations** alongside the enum additions (inline comments) so later phases understand clause ordering assumptions and child layout.

### Step 3 – Parser Enhancements
1. **Tokenizer upgrades** (`src/xpath/xpath_tokenizer.cpp/.h`):
   - Add keyword mappings for `where`, `order`, `by`, `group`, `count`, `stable`, `ascending`, `descending`, `empty`, `greatest`, `least`, and `collation`, plus recognise compound keywords such as `order by` and `group by` during token stream inspection.
   - Ensure identifier fallback still works so QNames like `order` in other contexts are handled correctly (e.g., guard with lookahead on reserved word sequences).
2. **Clause-aware parsing** (`src/xpath/xpath_parser.cpp`):
   - Refactor `parse_flwor_expr` into a staged loop that reads optional clauses in canonical order (`for`/`let` → `where` → `group by` → `order by` → `count` → `return`).
   - Introduce helpers (`parse_where_clause`, `parse_group_clause`, `parse_order_clause`, `parse_order_spec`, `parse_count_clause`) that each build the new AST node types and enforce the required syntax, including comma-separated `group by`/`order by` lists and optional modifiers (`stable`, direction keywords, `empty greatest/least`, `collation URI literal`).
   - Emit precise diagnostics when clauses are out of order or required elements are missing so the negative tests can assert on human-readable messages.
3. **AST assembly**: attach parsed clause nodes to the enclosing `FLWOR_EXPRESSION` (or existing `FOR_EXPRESSION`/`LET_EXPRESSION` shortcuts) in the same order they appear so evaluation can stream through clauses without reordering.

### Step 4 – Evaluator Implementation
1. **Introduce a dedicated FLWOR pipeline helper** inside `XPathEvaluator` (e.g., `evaluate_flwor_pipeline`) that transforms the AST into runtime stages. Represent each iteration as a `FlworTuple` containing:
   - The current `XPathContext` bindings for each variable.
   - Cached node/attribute handles for context-sensitive operations (so `group by` can later rebind grouped sequences).
   - Pre-evaluated sort keys to keep ordering stable.
2. **Stage-by-stage execution**:
   - **Binding expansion**: reuse the existing recursive binding logic to enumerate `for`/`let` combinations into a vector of tuples while preserving per-iteration context (position/size) for `for` variables.
   - **Where filtering**: evaluate each `WHERE_CLAUSE` expression within the tuple’s binding scope, dropping tuples whose predicate coerces to `false`.
   - **Group aggregation**: for `GROUP_CLAUSE` nodes, hash tuples by computed key values (respecting multiple `GROUP_KEY` entries) and merge the bound sequences for each variable so grouped bindings expose sequences to downstream stages. Store grouping keys in the tuple for later reference and surface evaluation errors (e.g., accessing undefined variables) immediately.
   - **Stable ordering**: evaluate each `ORDER_SPEC` expression (with `stable` default semantics) and store the resulting typed values. Implement a comparator that honours ascending/descending flags, handles `empty greatest/least`, and resolves string comparisons through the requested collation (fallback to the W3C codepoint collation via the existing accessor if no override is provided).
   - **Count assignment**: when a `COUNT_CLAUSE` is present, iterate over the (already ordered) tuples and inject/overwrite the named count variable with a numeric `XPathVal` representing the 1-based position, ensuring the bindings remain available to the return clause.
3. **Return clause evaluation**: iterate over the processed tuples, install their bindings into `XPathContext` (via `VariableBindingGuard`s), evaluate the return expression once per tuple, and accumulate results. Relax the current “return must be a node-set” restriction so non-node sequences can flow through future enhancements while keeping node-set fast paths for existing consumers.
4. **Error handling and diagnostics**: reuse `record_error` for clause-specific failures (e.g., unsupported collation URI, non-comparable sort keys). Ensure errors unwind cleanly without leaving context stacks inconsistent.

### Step 5 – Auxiliary Utilities
1. **Collation and comparison helpers**: create reusable string and numeric comparison utilities (likely in `src/xpath/xpath_evaluator_common.cpp`) that take an `XPathVal`, the desired collation URI, and direction/empty-order metadata.
2. **Grouping support code**: implement a lightweight key object (vector of `XPathVal` plus hashing/equality) to group tuples efficiently without flattening to strings, preserving type fidelity for future sequence support.
3. **Sequence materialisation helpers**: extend `xpath_value.cpp` if necessary with functions that wrap vectors of nodes/values into `XPathVal` instances so grouped bindings can expose the aggregated sequences expected by the tests.
4. **Performance safeguards**: ensure temporary storage leverages `XPathArena`/`pf::vector` to minimise allocations, and document any hot-path considerations introduced by sorting or grouping.

### Step 6 – Testing & Verification
1. **Activate and extend Fluid tests**: update `src/xml/tests/test_xpath_flwor_clauses.fluid` to remove the “not yet implemented” log statements, assert on real outputs, and add coverage for edge cases discovered during implementation (e.g., multiple `group by` keys, `empty least`, invalid `count` placement).
2. **Regression guardrails**: rerun existing FLWOR-related tests (e.g., `test_xpath_flwor.fluid`, `test_xpath_sequences.fluid`) to ensure legacy semantics remain intact.
3. **Build & execute**: document the required commands for the follow-up session: `cmake --build build/agents --config FastBuild --target xpath --parallel` followed by `ctest --build-config FastBuild --test-dir build/agents -L xml` to exercise the XML/XPath test suite.

### Step 7 – Documentation & Cleanup
1. **Inline comments**: annotate the new AST nodes and evaluator stages with brief explanations of clause ordering and runtime expectations so future contributors understand the pipeline.
2. **Developer documentation**: add a short section to the XPath developer docs (e.g., `docs/wiki` or module README) summarising the new clause support, including syntax, available modifiers, and current limitations (such as supported collations).
3. **Changelog / release notes**: if the project tracks changes, prepare a note describing FLWOR clause completion and reference the new tests.
4. **Final housekeeping**: remove any transitional logging, ensure `clang-format`/style guidelines are respected, and verify generated headers are committed alongside source changes.
