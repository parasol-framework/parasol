# Plan: Maps, Arrays, and Lookup Operator for the XQuery Module

## Objectives
1. Represent XQuery 3.0 map and array values in the runtime data model so they can be stored in `XPathValue`, passed through function calls, serialised back to users, and freed correctly.
2. Parse and evaluate the XQuery 3.0 constructors (`map {}` and `array {}`) and the lookup operator (`?`) so expressions such as `map {"a":1}("a")` and `$seq?price` execute per spec.
3. Provide the W3C-defined `map:*` and `array:*` function libraries so user code can create and manipulate maps/arrays without literal constructors.
4. Preserve backwards compatibility with existing XPath 2.0 semantics, error codes, and performance characteristics.

## References and Constraints
- Primary sources: W3C XQuery 3.0 Recommendation sections 3.9.3 (map constructors), 3.9.4 (array constructors), and 3.9.5 (lookup operator), plus the "Functions and Operators" (F&O) specification chapters 17 (maps) and 18 (arrays).
- Implementation touch points identified during the earlier gap analysis:
  - Tokeniser: `src/xquery/parse/xquery_tokeniser.cpp`
  - Parser and AST definitions: `src/xquery/parse/xquery_parser.cpp` and `src/xquery/xquery.h`
  - Evaluator: `src/xquery/eval/eval_expression.cpp`, `eval_values.cpp`, and supporting files
  - Function catalogue: `src/xquery/functions/` (especially `function_library.cpp`)
  - Tests: `src/xquery/tests/test_*`
- Follow project-wide conventions defined in `AGENTS.md` (British English, no `static_cast`, etc.).

## Step-by-step Implementation Plan

### Phase 1 – Data model groundwork
1. **Audit existing value representation**
   - Inspect `XPathValue`, `XPathVal` and related helpers in `src/xquery/xquery.h`, `include/parasol/modules/xml.h` and `src/xquery/eval/eval_values.cpp` to confirm how node sets, strings, and numerics are stored and reference-counted.
   - Document current memory ownership expectations (arena allocations, `pf::vector`, etc.) to ensure new map/array containers integrate cleanly.

2. **Extend `XPathValue` to support composite data**
   - Add enum entries for `Map` and `Array`. If `XPVT` is shared with XML, coordinate the change in `include/parasol/modules/xml.h` (or the shared header) and regenerate bindings if FDL-driven.
   - Introduce lightweight structures for map and array storage:
     - Map: ordered insertion is not mandated, so use `ankerl::unordered_dense::map<XPathValue, XPathValue>` or (preferably) a string-keyed map per spec (keys must be atomic). Store keys as canonicalised `XPathValue` plus value.
     - Array: `pf::vector<XPathValue>` referencing the evaluator arena for child values.
   - Embed these containers inside a `std::shared_ptr<XQueryMap>` / `XQueryArray` or custom ref-counted structs so copies of `XPathValue` remain cheap.

3. **Wire allocation and destruction paths**
   - Ensure `XPathValue::reset()` (or equivalent) releases map/array storage.
   - Update stringification routines (e.g., `xquery_class.cpp` conversions) so map/array results can be surfaced through API endpoints, even if displayed with placeholder text initially.

**Phase 1 status – completed**

- Audited the existing `XPathValue`/`XPathVal` storage rules, documenting that node sequences, attribute mirrors, and schema metadata live directly on the runtime struct and are cleared via a shared `reset()` routine. This informed how map/array containers needed to plug into the same lifecycle.
- Extended the XML FDL definitions so `XPVT` now advertises `Map` and `Array`, the generated header exposes `XPathValueSequence`, `XPathMapStorage`, and `XPathArrayStorage`, and new shared-pointer fields live on `XPathValue`. A dedicated `XPathValue::reset()` implementation now clears all arenas, vectors, and composite containers to avoid leaks when values are recycled by the evaluator.
- Updated the runtime helpers (`XPathVal::to_boolean()`, `to_number()`, `to_string()`, `is_empty()`, `size()`, schema-type mapping, and `XValueToString`) so map/array instances have sensible placeholder serialisation and obey the existing ownership model. With these pieces in place, the runtime can freely allocate, copy, and stringify composite values ahead of the parser/evaluator work.

### Phase 2 – Lexical support
4. **Tokenise `map`, `array`, and lookup operator lexemes**
   - In `xquery_tokeniser.cpp`, add entries for `map`, `array`, and ensure `?` can be emitted both as occurrence indicator (existing) and as an infix lookup operator token when it appears between a primary expression and a name/test.
   - Add recognition for `=>` and `!` only if necessary for lookup chaining (arrow operator interacts with lookup). For this task, ensure `?` followed by NCName, `*`, `(`, or integer literal triggers `LOOKUP` token.
   - Update unit tests in `src/xquery/tests/test_reserved_words.fluid` (or create a new lexical test) to confirm tokens.

**Phase 2 status – completed**

- Extended the token type enumeration with `MAP`, `ARRAY`, and `LOOKUP`, and updated the keyword table so `map`/`array` only switch to constructor tokens when the next non-whitespace character is `{`, leaving prefixed names untouched.
- Enhanced the operator scanner so `?` followed by NCName characters, integers, wildcards, or parenthesised expressions now emits `LOOKUP`, while other contexts continue to produce the existing `QUESTION_MARK` occurrence indicator.
- Added a dedicated tokeniser unit test covering the new keywords plus both lookup and occurrence-indicator heuristics to prevent regressions before parser support lands.

### Phase 3 – Parser and AST additions
5. **Define AST node kinds**
   - Extend the `XQueryNodeType` enum in `src/xquery/xquery.h` with `MAP_CONSTRUCTOR`, `ARRAY_CONSTRUCTOR`, and `LOOKUP_EXPRESSION`.
   - Add helper structs within `XPathNode` (or dedicated fields) to store constructor entries (pairs of child expression pointers) and lookup info (primary expression child plus key sequence).

6. **Parse map constructors**
   - In `parse_primary_expression()` (or equivalent), detect `map` keyword followed by `{`.
   - Parse a comma-separated list of `KeyExpr : ValueExpr` entries. Keys must be expressions returning single atomic values; enforce static cardinality check if possible.
   - Support empty map `map { }`.
   - Attach children to the new AST node (store as vector pairs or as zipped child arrays). Add grammar error messages for missing colon/comma.

7. **Parse array constructors**
   - Recognise `array {` and parse a comma-separated list of expressions, each allowed to yield sequences.
   - Support `array { }` and ensure whitespace/newline tokens are handled similarly to existing constructors.
   - Represent each member as a child pointer (use existing vector storage for child nodes).

8. **Parse lookup operator**
   - Update precedence table so lookup binds tighter than arithmetic/logical operations but after postfix operations.
   - Accept forms `PostfixExpr ? KeySpecifier` where `KeySpecifier` can be:
     - NCName or wildcard (maps to string literal)
     - Integer literal
     - Parenthesised expression (for dynamic keys)
   - Permit chained lookups (e.g., `$m?foo?bar`). Parser should emit a left-associative tree or encode as repeated nodes.
   - Update error recovery to fallback gracefully when `?` appears in type declarations (occurrence indicators) by checking context.

**Phase 3 status – completed**

- Extended the shared IDL constants so `XQueryNodeType` now advertises `MAP_CONSTRUCTOR`, `ARRAY_CONSTRUCTOR`, and `LOOKUP_EXPRESSION`, then regenerated the public header/definition pairs so downstream code compiles against the richer AST surface.
- Enlarged `XPathNode` with dedicated map-entry, array-member, and lookup-specifier containers (plus helper methods) so the parser can persist constructor pairs and lookup chains without abusing the generic child vector.
- Taught `parse_primary_expr()`/`parse_filter_expr()` to recognise literal `map {}` / `array {}` constructors and to treat lookup `?` sequences as first-class postfix operators, including new helper routines that capture NCName, wildcard, numeric, or expression keys.
- Added parser unit tests that assert the presence of the new AST nodes and stored metadata for simple map, array, and chained lookup expressions.

### Phase 4 – Evaluator integration
9. **Implement map constructor handler**
   - In `XPathEvaluator` add `handle_map_constructor()` and register it in `NODE_HANDLERS`.
   - Evaluation steps:
     1. Evaluate each key expression; verify each produces exactly one atomic value (raise `XPTY0004` otherwise).
     2. Convert key to canonical string (per `fn:string()`), since XQuery maps use string keys.
     3. Evaluate associated value expression (sequence allowed) and store copy in the map container.
     4. Handle duplicate keys per spec (later entry overwrites earlier) or raise `XQDY0137` depending on compliance decision; document the chosen behaviour.

10. **Implement array constructor handler**
    - Evaluate each member expression; arrays preserve sequence items (flattening is not automatic).
    - Store each result as a `pf::vector<XPathValue>` entry; consider referencing existing helper for materialising sequences.
    - Ensure lazy evaluation flags remain consistent (if referencing existing `XPathSequence` class, integrate accordingly).

11. **Implement lookup handler**
    - Evaluate the left operand once.
    - For map inputs: accept single atomic key; return stored sequence or empty sequence if absent.
    - For array inputs: accept numeric key (1-based). Throw `FOAY0001` on out-of-range index.
    - If left operand is a sequence, apply lookup to each item and concatenate results, as per spec.
    - For node sequences, reuse existing attribute/child navigation semantics if lookup is allowed (XQuery 3.1 allows `?` on nodes returning attributes/elements). Document supported forms and file follow-up tasks for unsupported permutations.
    - Update evaluator helper methods (possibly in `eval_values.cpp`) to test type and cardinality, returning appropriate error codes.

12. **Value coercion and comparison rules**
    - Extend `XPathValue` comparison/hash utilities so map keys can reuse existing equality semantics.
    - Implement `atomize_singleton()` helper if needed to ensure lookup keys are typed correctly.
    - Provide conversions for `map(*)` and `array(*)` when interacting with `treat as`, `instance of`, and sequence type annotations.

**Phase 4 status – completed**

- Added evaluator handlers for map and array constructors so each key/member expression is materialised into the shared `XPathMapStorage`/`XPathArrayStorage`, overwriting duplicate map keys per spec and storing empty sequences without leaking resources.
- Implemented lookup evaluation for `LOOKUP_EXPRESSION` nodes, covering literal, wildcard, and expression keys for maps and arrays (with FOAY0001 error reporting for invalid indexes) plus basic child/attribute selection when the base value is a node sequence.
- Introduced reusable helpers that atomise dynamic keys, copy stored `XPathValueSequence` data back into runtime values, and concatenate lookup results so wildcard queries return the concatenated sequences expected by XQuery 3.0.

### Phase 5 – Function library support
13. **Register namespaces and function signatures**
    - In `function_library.cpp`, register the `http://www.w3.org/2005/xpath-functions/map` and `/array` namespaces, binding built-in implementations for:
      - Maps: `map:put`, `map:get`, `map:contains`, `map:size`, `map:keys`, `map:merge`, `map:entry`, etc.
      - Arrays: `array:size`, `array:get`, `array:append`, `array:insert-before`, `array:remove`, `array:join`, `array:flatten`, etc.
    - Update dispatch tables so the parser can resolve prefixed names (ensure default function namespace handling acknowledges `map`/`array`).

14. **Implement runtime helpers**
    - Create dedicated helper files `func_maps.cpp` and `func_arrays.cpp` (mirroring existing categories) to keep implementations modular.
    - Each helper should:
      - Accept vector of `XPathValue` arguments, validate types (including optional function items for `map:for-each` / `array:for-each` if implemented later).
      - Reuse constructors and lookup logic for consistency.
      - Return `ERR::NoSupport` for advanced optional functions if not immediately implemented, but plan should prefer covering the core F&O subset required by lookup semantics (at least `map:put`, `map:get`, `map:contains`, `map:size`, `array:size`, `array:get`).

**Phase 5 status – completed**

- Added shared helper utilities plus dedicated `func_maps.cpp` / `func_arrays.cpp` sources so the function library can convert between runtime `XPathVal` instances and the stored `XPathValueSequence` payload used by map and array entries.
- Registered the `map:*` and `array:*` namespaces in `function_library.cpp` and implemented the core XQuery 3.1 helpers (`map:entry`, `map:put`, `map:get`, `map:contains`, `map:size`, `map:keys`, `map:merge`, `array:size`, `array:get`, `array:append`, `array:insert-before`, `array:remove`, `array:join`, `array:flatten`).
- Exercised the new APIs through a dedicated `test_maps_arrays.fluid` suite that covers size/counting behaviour, overwriting semantics, multi-map merges, and end-to-end array mutations to guard against regressions.
- Expanded the sequence materialisation path so comma expressions preserve map/array payloads via composite metadata, ensuring `map:merge((...))` and `array:join((...))` accept spec-compliant sequence arguments instead of only discrete parameters.

### Phase 6 – Static typing and sequence types
15. **Extend type system**
    - Update parser support for sequence types `map(*)`, `map(K, V)`, `array(*)`, and `function(item()*)` in type declarations (if needed for lookup). This may require grammar tweaks in `parse_sequence_type()`.
    - Ensure `instance of` and `treat as` recognise the new types by extending the type-checking logic in `eval_expression.cpp`.

16. **Error reporting**
    - Map/array operations introduce new error codes (`FOJS0004`, `FOAY0001`, etc.). Add them to `xquery_errors.h` and use via `record_error()` so users receive spec-aligned diagnostics.

**Phase 6 status – completed**

- Added parser handling and `SequenceTypeInfo` metadata for `map(*)`, `map(K, V)`, `array(*)`, and generic `function(...)` sequence types so the evaluator can reason about composite values.
- Implemented recursive map/array/type checking helpers wired into `instance of`, `treat as`, and `typeswitch` evaluation so runtime values honour key/member constraints, while `function(item()*)` now recognises map/array function items.
- Introduced shared error helpers for `FOAY0001`/`FOJS0004` in `xquery_errors.h` and updated lookup/array diagnostics to emit the spec codes consistently.

### Phase 7 – Testing and validation
17. **Unit tests**
    - Extend `src/xquery/tests/test_constructors.fluid` with cases covering literal map/array construction, duplicate keys, nested maps, and `array { (1,2), 3 }` semantics.
    - Add lookup-specific tests in `test_advanced.fluid` or a new `test_maps_arrays.fluid` verifying chaining (`$order?items?1?price`).
    - Add regression tests for `map:*`/`array:*` functions, including error-path coverage.

18. **Compliance tests**
    - Integrate relevant QT3 tests (if subset available in `src/xquery/tests/modules/`) focusing on maps, arrays, and lookup. Document any skipped tests with reasons.

19. **Performance/regression checks**
    - Run `ctest -L xquery` after building to ensure no regressions.
    - Benchmark representative queries (if harness exists) to ensure no significant slowdown from new token/AST logic.

**Phase 7 status – completed**

- Extended `test_constructors.fluid`, `test_maps_arrays.fluid`, and `test_advanced.fluid` with map/array lookup cases covering duplicate-key overwrites, nested map/array chains, wildcard extraction, and explicit FOAY0001 error paths so literal constructors, lookups, and library routines all exercise the semantics introduced in earlier phases.
- Added a QT3-inspired regression harness (`test_qt_maps_arrays.fluid`) that loads the curated subset stored in `tests/modules/qt3_map_array_subset.xml`, registers each `<test-case>` as an individual Flute test, and reports skipped execution whenever the optional QT3 package is not installed, documenting the pared-down subset that currently ships with the repository.
- Rebuilt the module and ran `ctest --build-config Release --test-dir build/agents -L xquery` to verify that the expanded suite passes in the Release configuration and that no regressions were introduced by the added coverage.

### Phase 8 – Documentation and rollout
21. **Developer notes**
    - Amend `XQUERY_30_REQUIREMENTS.md` to mark the map/array gap as addressed once implementation lands.
    - Consider adding an AGENT note under `src/xquery/functions/` outlining where map/array helpers reside for future contributors.

22. **Rollout checklist**
    - Build (`cmake --build build/agents --config Release --target xquery --parallel`).
    - Install (`cmake --install build/agents`).
    - Run `ctest --build-config Release --test-dir build/agents -L xquery`.
    - Regenerate documentation if necessary.

## Expected Follow-ups
- Arrow and simple map operators share infrastructure with lookup; plan to extend parser/evaluator once `?` support is stable.
- Higher-order functions (`function(*)` values) rely on similar `XPathValue` and `XPathVal` infrastructure; confirm compatibility when implementing `map:for-each`/`array:for-each`.
- Map/array serialisation for Fluid scripts may need additional helper APIs beyond the core implementation.
