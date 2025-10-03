# Plan: Implement Full XPath 2.0 Sequence Item Support

## Objective
Expand the XPath engine so it can represent, parse, and evaluate general item sequences (nodes and arbitrary atomic values) instead of limiting results to node sets.

## Assumptions & Constraints
- Maintain existing public APIs where possible; internal data structures may change.
- Follow repository coding standards (three-space indent, `IS` macro, `and`/`or`, etc.).
- Preserve backward compatibility with existing XPath 1.0 behaviours.

## Test Strategy (Defined Up-Front)
1. **Unit/Regression Tests**
   - Add new Fluid tests under `src/xml/tests/` to validate handling of mixed sequences (nodes + atomic values) and sequence operators/functions.
   - Cover edge cases such as empty sequences, singleton atomic sequences, and nested FLWOR results producing atomic sequences.
2. **Existing Test Suite**
   - Re-run existing XML/XPath tests to ensure no regressions.

## Work Breakdown Structure
### 1. Analysis & Design
- Review current `XPathValueType`/`XPathValue` implementations to identify node-set assumptions.
- Design a new representation for general sequences (e.g., tagged union of node pointers and atomic values within a sequence container).
- Determine any required adjustments to reference counting or arena allocation.

### 2. Update Core Data Structures
- Modify `src/xml/xpath/xpath_functions.h/.cpp` to introduce the new sequence container types and helper utilities.
- Ensure helper APIs expose iteration over mixed node/atomic items while remaining efficient.

### 3. Parser Enhancements
- Update `XPathParser::parse_expr` in `src/xml/xpath/xpath_parser.cpp` to implement the `exprSingle (',' exprSingle)*` grammar.
- Introduce AST node(s) representing explicit sequence construction.
- Validate compatibility with existing AST visitors/evaluators.

### 4. Evaluator Changes
- Adjust `XPathEvaluator` (`src/xml/xpath/xpath_evaluator.cpp`) to iterate over general sequences.
- Update FLWOR, quantified expressions, and other evaluators to accept/produce sequences containing atomic values.
- Ensure implicit conversions (boolean/numeric) respect XPath 2.0 sequence rules.

### 5. Built-in Function Adjustments
- Modify sequence-related built-ins in `src/xml/xpath/functions/func_sequences.cpp` and any others affected by the new representation.
- Confirm functions such as `distinct-values`, `index-of`, `subsequence` operate correctly with mixed sequences.

### 6. Testing & Verification
- Implement the planned Fluid tests first, using the new expected behaviour as assertions.
- Build the XML module: `cmake --build build/agents --config Release --target xml --parallel`.
- Run XML module tests via `ctest --build-config Release --test-dir build/agents -L xml` (or equivalent label) after installation if required.
- Investigate and resolve any failing tests.

### 7. Documentation & Cleanup
- Update inline comments or developer documentation if APIs change.
- Ensure compliance with coding guidelines (spacing, naming, macros).
- Remove temporary debugging output prior to commit.

## Risk Mitigation
- Maintain incremental commits (data structure, parser, evaluator, tests) to simplify debugging.
- Use existing sequence utility patterns elsewhere in the codebase as references to avoid reinventing memory management strategies.
- Validate performance impact with targeted benchmarks if sequence handling becomes heavier.
