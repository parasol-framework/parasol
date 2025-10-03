# Plan: Support XPath 2.0 Type and Range Expressions

## Objective
Add parsing and evaluation support for XPath 2.0 type-related expressions (`cast`, `castable`, `treat as`, `instance of`, `typeswitch`) and the `to` range operator.

## Assumptions & Constraints
- Leverage existing schema/type infrastructure under `src/xml/schema/`.
- Ensure error reporting aligns with XPath 2.0 specification.
- Maintain compliance with repository coding standards (macros, indentation, naming).

## Test Strategy (Defined Up-Front)
1. **New Fluid Tests**
   - Add tests to `src/xml/tests/` covering:
     - Successful and failing `cast`/`castable` operations for primitive types (string, decimal, date, etc.).
     - `treat as` with both satisfied and violated type assertions.
     - `instance of` across various schema types, including unions and lists.
     - `typeswitch` evaluating multiple branches and default clauses.
     - The `to` range operator for numeric sequences, including boundary conditions.
   - Include negative tests for invalid syntax and type errors.
2. **Regression**
   - Re-run existing XPath tests to verify no regressions in previous functionality.

## Work Breakdown Structure
### 1. Grammar & Token Updates
- Extend `XPathTokenizer` in `src/xml/xpath/xpath_parser.cpp` to recognise new keywords and the `to` operator.
- Update `XPathTokenType` enums and related mappings.
- Add any required AST node definitions in `src/xml/xpath/xpath_ast.h`.

### 2. Parser Implementation
- Implement parsing routines in `parse_expr` and helper functions for each new expression form.
- Enforce correct precedence and associativity, especially for the `to` range operator.
- Provide informative syntax errors when encountering unsupported forms.

### 3. Evaluator Enhancements
- Update `XPathEvaluator` (`src/xml/xpath/xpath_evaluator.cpp`) to evaluate the new expressions:
  - Integrate with schema helpers for type checking and casting.
  - Implement `typeswitch` control flow and variable scoping rules.
  - Generate sequences for `to` ranges respecting numeric type promotion rules.
- Ensure evaluation gracefully handles dynamic type errors per specification.

### 4. Support Utilities
- Add or refine helper functions within schema/type modules if additional conversions are required.
- Ensure collation or timezone considerations are accounted for where applicable (e.g., date/time casts).

### 5. Testing & Verification
- Implement the planned Fluid tests before modifying the core evaluator to lock expected behaviours.
- Build XML module: `cmake --build build/agents --config Release --target xml --parallel`.
- Execute targeted tests via `ctest --build-config Release --test-dir build/agents -L xml`.
- Address any failing cases, confirming detailed error messages where expected.

### 6. Documentation & Cleanup
- Update comments or developer documentation highlighting support for new expression types.
- Double-check coding standards (indentation, macros, British English spelling).
- Remove temporary logging or debug helpers prior to final commit.

## Risk Mitigation
- Develop and test each expression type incrementally to localise issues.
- Consult XPath 2.0 specification for edge-case behaviour (e.g., numeric ranges with NaN or INF values).
- Add asserts or validation helpers to prevent incorrect schema interactions.
