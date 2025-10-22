## Objective
Add parsing and evaluation support for XPath 2.0 type-related expressions (`cast`, `castable`, `treat as`, `instance of`, `typeswitch`) and the `to` range operator.

## Assumptions & Constraints
- Leverage existing schema/type infrastructure under `src/xml/schema/`.
- Ensure error reporting aligns with XPath 2.0 specification.
- Maintain compliance with repository coding standards (macros, indentation, naming).

## Test Strategy (Defined Up-Front)
1. **New Fluid Tests** (DONE)
   - Add tests to `src/xpath/tests/test_type_expr.fluid` covering:
     - Successful and failing `cast`/`castable` operations for primitive types (string, decimal, date, etc.).
     - `treat as` with both satisfied and violated type assertions.
     - `instance of` across various schema types, including unions and lists.
     - `typeswitch` evaluating multiple branches and default clauses.
     - The `to` range operator for numeric sequences, including boundary conditions.
   - Include negative tests for invalid syntax and type errors.
2. **Regression**
   - Re-run existing XPath tests to verify no regressions in previous functionality.

## Shared Foundation

For each implementation:

- Survey the existing tokeniser, AST, parser, and evaluator entry points to confirm current behaviour and identify integration seams for new syntax (keyword_mappings, XPathTokenType, parse_expr*, and XPathEvaluator::evaluate_expression).

- Review available schema/type support to determine which helpers already exist for type lookup, coercion, and subtype checking before extending evaluation logic.

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

### 5. Implementation Details

#### cast Expression

[x] Extend the tokeniser to recognise cast/as keywords (ensure as is treated correctly as a keyword token where needed) and add any missing token enum entries for clarity.

[x] Introduce dedicated AST node types (e.g., CAST_EXPRESSION) in xpath.fdl and regenerate headers so the evaluator has a semantic hook.

[x] Update parse_expr_single/parse_unary_expr to detect the Expr cast as SequenceType pattern, parse the target type with existing sequence-type helpers, and emit the new AST node (carry source location for error reporting).

[x] Implement evaluation support that resolves the target schema type, enforces single-item cardinality, performs type coercion via SchemaTypeDescriptor::coerce_value, and raises XPTY0004/XPTY0006 errors with helpful context when conversion fails.

#### castable Expression

[x] Tokeniser: ensure castable is recognised distinctly so it does not collide with element names in non-keyword contexts.

[x] AST: add CASTABLE_EXPRESSION node with room to record the type literal and optional ? occurrence indicator.

[x] Parser: add a dedicated production alongside cast that parses Expr castable as SequenceType, reusing the sequence-type parser and storing any ? occurrence marker for evaluation.

[x] Evaluator: implement boolean result semantics that mirror cast without performing the conversion, using schema helpers to determine castability while suppressing exceptions; ensure memoised type resolution to avoid repeated registry lookups.

[x] Tests: cover true/false outcomes for the same inputs as cast, plus edge cases where castable succeeds but cast would raise a dynamic error.

#### treat as Expression

[x] Add treat keyword handling in the tokeniser and AST node type TREAT_AS_EXPRESSION with fields for the asserted sequence type (including occurrence).

[x] Parser: hook into parse_unary_expr (before union/path parsing) to consume Expr treat as SequenceType, ensuring proper precedence and reuse of the sequence-type parsing routine.

[x] Evaluator: implement runtime cardinality/type verification without coercion; reuse schema registry to validate each item and raise XPTY0004 with descriptive message when assertion fails.

[x] Tests: assert success/failure for matching and non-matching types, including optional (?), one-or-more (+), and zero-or-more (*) occurrence indicators.

#### instance of Expression

[ ] Tokeniser: add instance keyword and ensure of is treated as part of the keyword sequence while remaining usable as an NCName when required.

[ ] AST: define INSTANCE_OF_EXPRESSION storing both operand and parsed sequence type details.

[ ] Parser: integrate the Expr instance of SequenceType grammar near the relational/equality layer so it binds tighter than and/or but looser than arithmetic; update operator precedence tables accordingly.

[ ] Evaluator: implement boolean semantics that iterate sequence members, consult schema descriptors for subtype relationships, and correctly handle empty sequences and occurrence indicators per XPath 2.0 rules.

[ ] Tests: create coverage for atomic types, union/list types (if available), and mismatched instances.

#### typeswitch Expression

[ ] Extend tokeniser keyword mapping for typeswitch, case, and default while keeping them valid NCNames in constructor contexts.

[ ] AST: add node types such as TYPESWITCH_EXPRESSION, CASE_CLAUSE, and DEFAULT_CASE, storing variable names, sequence types, and branch expressions.

[ ] Parser: implement a new production (likely dispatched from parse_expr_single) that handles typeswitch ( Expr ) case $Var as SequenceType return Expr ... default return Expr, including support for optional $Var bindings and case fall-through ordering. Reuse sequence-type parsing helpers and ensure braces/parentheses are consumed accurately for error reporting.

[ ] Evaluator: add dispatcher logic that evaluates the operand once, iterates case clauses performing instance of checks in order, binds the variable (if present) within branch scope, and executes the matching branch or the default; propagate dynamic errors and respect variable scoping rules in XPathEvaluator.

[ ] Tests: cover scenarios with multiple cases, first-match wins, default-only fallbacks, and type mismatches.

#### to Range Operator

[ ] Tokeniser/enum: add a dedicated token for the to keyword and ensure it participates in operator precedence resolution.

[ ] Parser: introduce a parse_range_expr layer between additive and relational parsing so Expr to Expr binds tighter than comparison but looser than additive operations; ensure right-hand operand parsing allows nested ranges correctly.

[ ] AST: either reuse a binary-op node with explicit operator string "to" or add RANGE_EXPRESSION for clarity in the evaluator.

[ ] Evaluator: implement sequence generation that handles numeric type promotion, ascending/descending validation (raise XPTY0004 for NaN or non-numeric inputs), and large-range safeguards; integrate into map_binary_operation and downstream execution switch.

[ ] Tests: add coverage for positive ranges, negative step expectations (should error), boundary cases like identical endpoints, and invalid operands.

#### Cross-Cutting Testing & Verification

Build and run the XML/XPath modules (cmake --build ... --target xml xpath) and execute targeted ctest labels after each feature increment to catch regressions early.

[X] Create comprehensive Fluid test suites under src/xpath/tests/ targeting the new expressions and ensuring error codes align with spec expectations.

[ ] Perform regression sweeps on the existing XPath and XML test suites once all features are integrated.

### 5. Testing & Verification
- Implement the planned Fluid tests before modifying the core evaluator to lock expected behaviours.
- Build XPath module: `cmake --build build/agents --config [BuildType] --target xpath --parallel`.
- Execute targeted tests via `ctest --build-config [BuildType] --test-dir build/agents -L xpath`.
- Address any failing cases, confirming detailed error messages where expected.

### 6. Documentation & Cleanup
- Update comments or developer documentation highlighting support for new expression types.
- Double-check coding standards (indentation, macros, British English spelling).
- Remove temporary logging or debug helpers prior to final commit.

## Risk Mitigation
- Develop and test each expression type incrementally to localise issues.
- Consult XPath 2.0 specification for edge-case behaviour (e.g., numeric ranges with NaN or INF values).
- Add asserts or validation helpers to prevent incorrect schema interactions.
