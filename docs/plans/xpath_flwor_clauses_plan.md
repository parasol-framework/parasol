# Plan: Implement Missing XPath 2.0 FLWOR Clauses

## Objective
Extend the XPath parser and evaluator to support `where`, `order by`, `group by`, and `count` clauses in FLWOR expressions while maintaining compatibility with existing behaviour.

## Assumptions & Constraints
- New clauses must integrate with any future general sequence representation.
- Respect repository coding standards and avoid introducing performance regressions.
- Grouping and ordering must handle collation settings consistent with existing XPath infrastructure.

## Test Strategy (Defined Up-Front)
1. **New Fluid Tests**
   - Create dedicated tests under `src/xml/tests/` for each clause:
     - `where`: filtering with boolean predicates including atomic comparisons.
     - `order by`: ordering strings, numbers, and nodes with ascending/descending options.
     - `group by`: grouping values and verifying representative outputs.
     - `count`: verifying positional counting outputs and interactions with other clauses.
   - Add negative tests covering invalid clause orderings and incompatible expressions.
2. **Regression**
   - Run existing XPath FLWOR tests to confirm no regressions in baseline behaviour.

## Work Breakdown Structure
### 1. Research & Design
- Review XPath 2.0 FLWOR grammar and semantics to clarify clause ordering and evaluation rules.
- Inspect existing `XPathParser` FLWOR handling to determine extension points for new clauses.
- Outline evaluator data flow to accommodate filtering, ordering, grouping, and counting.

### 2. AST Extensions
- Update `src/xml/xpath/xpath_ast.h/.cpp` to define nodes/structures representing optional FLWOR clauses.
- Ensure AST serialisation/debug helpers reflect new structures.

### 3. Parser Enhancements
- Modify `parse_flwor_expr` in `src/xml/xpath/xpath_parser.cpp` to parse optional clauses in canonical order.
- Enforce syntax rules (e.g., `group by` must precede `order by`).
- Add descriptive error messages for malformed clause sequences.

### 4. Evaluator Implementation
- Extend `XPathEvaluator` in `src/xml/xpath/xpath_evaluator.cpp` to:
  - Apply `where` predicates to intermediate bindings.
  - Perform stable sorting for `order by` with support for ascending/descending and collation resolution.
  - Implement `group by` result bucketing, ensuring grouped variable visibility and aggregated sequence construction.
  - Track positional counters for `count` clause outputs.
- Integrate with sequence support for handling atomic values.

### 5. Auxiliary Utilities
- Add helper routines for grouping keys, collation lookup, and stable sort operations if not already present.
- Ensure memory management uses existing arenas or reference-counting mechanisms.

### 6. Testing & Verification
- Implement the planned Fluid tests first to lock expected behaviours before modifying parser/evaluator code.
- Rebuild XML module: `cmake --build build/agents --config Release --target xml --parallel`.
- Run targeted tests (new and existing) with `ctest --build-config Release --test-dir build/agents -L xml`.
- Investigate and fix any failures, ensuring deterministic ordering for tests.

### 7. Documentation & Cleanup
- Update inline comments and developer docs to describe new clause support.
- Remove temporary instrumentation and verify compliance with coding standards.
- Prepare release notes or changelog entries if required.

## Risk Mitigation
- Implement clauses incrementally (e.g., `where` first) with dedicated tests per step.
- Use feature flags or capability checks to avoid breaking existing consumers during development.
- Carefully handle sorting/grouping edge cases such as NaN, collation mismatches, or empty groups.
