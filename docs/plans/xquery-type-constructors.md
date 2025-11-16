# Plan: XQuery Atomic Type Constructors

## Background and Current Gaps

* `XPathEvaluator::evaluate_function_call()` currently normalises function QNames, optionally resolves user-defined bodies, and otherwise defers to `XPathFunctionLibrary`. There is no interception for EQNames that refer to schema-defined atomic types, so expressions such as `xs:double('INF')` or `xs:date(fn:current-dateTime())` fall through to the library and are reported as "Unsupported XPath function". 【F:src/xquery/eval/eval_values.cpp†L2230-L2303】
* The function library only exposes standard `fn:*` routines. No constructor functions are registered for the lexical namespaces defined by the XML Schema built-ins, reinforcing the observation that atomic constructors are unimplemented. 【F:src/xquery/functions/function_library.cpp†L6-L120】
* The schema registry already knows about all required builtin atomic types, but we never expose them as callable constructors and there is no shared helper that validates the W3C lexical spaces (e.g., special IEEE string forms, date/time components, duration facets). 【F:src/xml/schema/schema_types.cpp†L162-L219】
* `XPathVal::string_to_number()` delegates straight to `std::strtod()` and therefore rejects W3C lexical forms like `INF`, `-INF`, and `NaN` because `strtod()` expects lowercase `inf`/`nan` or `infinity`. Implementing constructors must guarantee the correct lexical space, so we need explicit handling instead of relying solely on the standard library. 【F:src/xml/xpath_value.cpp†L158-L188】

## Goals

Implement the XQuery 3.0 constructor function semantics described in §3.10.4 so that built-in and schema-aware atomic constructors work for:

1. IEEE 754 numeric types, including the lexical tokens `INF`, `-INF`, and `NaN`.
2. Date/time and duration constructors that accept either lexical strings or compatible atomic operands (e.g., `xs:date(xs:dateTime(...))`).
3. Schema-derived simple types once they are registered in `SchemaTypeRegistry`.
4. Error reporting that matches the specification (`XPTY0004`, `FONS0004`, `FORG0001`, etc.).

## Implementation Plan

### 1. Constructor Resolution Layer

* Extend `XPathEvaluator::evaluate_function_call()` with a pre-dispatch hook that recognises constructor call patterns:
  * If the function EQName (prefix, expanded QName, or lexical QName) resolves to a schema type name inside `SchemaTypeRegistry`, bypass the `XPathFunctionLibrary` and invoke a new helper (e.g., `evaluate_type_constructor`).
  * Normalise both prefixed and `Q{}` syntaxes by reusing `prolog->normalise_function_qname()` so that `xs:double`, `Q{http://www.w3.org/2001/XMLSchema}double`, and schema-derived types all resolve to the same key.
  * Reject arities other than one and raise `XPTY0004` to align with §3.10.4.

### 2. Shared Constructor Evaluation Helper

* Add `XPathEvaluator::evaluate_type_constructor(const SchemaTypeDescriptor &TargetType, std::vector<XPathVal> &Args, const XPathNode *CallSite)` that:
  * Atomises node-set arguments using the same logic as `fn:data()` (single item only). Return the empty sequence if the operand sequence is empty.
  * Converts the operand to `xs:untypedAtomic` when needed and runs lexical validation via `xml::schema::TypeChecker`. Provide a dedicated error message sink so we can surface `FORG0001` with schema-aware diagnostics.
  * Delegates to per-family coercion helpers:
    * Numerics: use a new `parse_ieee_lexical_double()` to accept exact `INF`, `-INF`, `NaN`, decimal literals, and exponent forms per XML Schema Part 2.
    * Boolean: reuse `parse_schema_boolean()` to accept `true/false/0/1` and emit `FORG0001` otherwise.
    * Date/time/duration: reuse the parsing utilities already in `eval_expression.cpp` (e.g., `parse_xs_date_components()`, timezone validators) to materialise canonical `XPathVal` values with the correct subtype (`XPVT::Date`, `XPVT::Time`, `XPVT::DateTime`). Implement down-casting rules so `xs:date(xs:dateTime(..))` strips the time component and preserves timezone, matching the spec.
    * QName/NOTATION: ensure the namespace context is available, otherwise raise `FONS0004`.
  * Ensure the resulting `XPathVal` gets its schema type metadata via `set_schema_type()` so downstream code recognises the type annotations.

### 3. Schema Type Registry Enhancements

* Provide a map from fully qualified type names to constructor metadata (arity, namespace URI) inside `SchemaTypeRegistry` or a lightweight adaptor so `evaluate_type_constructor()` can quickly look up descriptors.
* Add utility methods for determining whether a type is numeric, duration, date/time, or namespace-sensitive to simplify the helper logic.

### 4. `XPathVal` and Parsing Utilities

* Augment `XPathVal::string_to_number()` (or a new helper) with explicit checks for the uppercase IEEE tokens before falling back to `std::strtod()`. This ensures `xs:double('INF')`, `xs:double('-INF')`, and `xs:double('NaN')` honour the W3C lexical space.
* Expose `parse_xs_date_components()`, `is_valid_timezone()`, and related routines in a header (or move them into a shared utility) so that constructor evaluation can reuse the same validation paths as casting/type expressions.
* Add helpers to serialise canonical date/time strings (with timezone normalisation) to guarantee consistent results between constructors, casts, and literal handling.

### 5. Error Handling and Spec Compliance

* Implement the W3C-defined error conditions:
  * `XPTY0004` when the operand sequence has more than one item or arity is not one.
  * `FONS0004` when a namespace-sensitive constructor (e.g., `xs:QName`) is invoked without an in-scope namespace binding.
  * `FORG0001` for lexical validation failures (invalid dates, out-of-range timezone, malformed numerics).
  * `XPST0051`/`XPST0081` when the QName resolves to a type that is not in scope.
* Ensure the evaluator stores the diagnostic in `xml->ErrorMsg` exactly once so Fluid-facing APIs report a meaningful message.

### 6. Test Suite Additions

* Create `src/xquery/tests/test_type_constructors.fluid` dedicated to constructor semantics:
  * Happy paths: `xs:double('INF')`, `xs:double('-INF')`, `xs:double('NaN')`, `xs:float('1.23E4')`, `xs:boolean(1)`, `xs:date('2025-03-16Z')`, `xs:date(fn:current-dateTime())`, `xs:dateTime('2025-03-16T12:34:56-05:00')`, `xs:duration('P2DT3H4M')`.
  * Node atomisation: `xs:int(/root/value)` where `/root/value` is a single text node, plus failure coverage when two nodes are selected.
  * Namespace-sensitive constructors: `xs:QName('p:local')` with and without the prefix bound.
  * Error coverage: invalid lexical string for each family, zero arguments, two arguments, and attempts to construct unsupported schema types.
* Expand `test_type_expr.fluid` with mixed expressions that chain constructors with `cast as` to ensure consistent schema metadata propagation.
* Integrate relevant QT3 3.0 tests (folder `src/xquery/QT3_1_0/`) by adding a focussed driver that runs the `K2-ConstructorFunction*` and `cbcl-constructors-*` groups. These will guard against regressions and align behaviour with the W3C reference suite.

## Deliverables

1. Updated evaluator, schema helpers, and parsing utilities implementing constructor semantics.
2. Comprehensive Fluid test coverage plus QT3 suite integration.
3. Documentation updates (e.g., `docs/wiki/Fluid-Reference-Manual.md` or module AGENT notes) summarising constructor support once implemented.

## Progress

* Phases 1 and 2 are now implemented: `evaluate_function_call()` recognises XML Schema EQNames and dispatches them to the new `evaluate_type_constructor()` helper. The helper currently atomises operands, enforces single-argument arity, and materialises constructors for numerics (including IEEE `INF`, `-INF`, and `NaN`), booleans, and the `xs:date`/`xs:time`/`xs:dateTime` family—including down-casting from `xs:dateTime` inputs.
* Phase 3 is complete: the schema registry records namespace URIs, local names, constructor arity, and exposes expanded-QName lookups plus helper predicates for numeric, duration, date/time, and namespace-sensitive categories. Constructor resolution consumes these descriptors so both prefixed names and `Q{}` EQNames now bind correctly without depending on incidental prefix mappings.
* Duration and QName constructors are now wired through the shared parsing helpers so they share canonicalisation, default namespace handling, and diagnostic paths with the rest of the evaluator. The outstanding edge cases noted earlier (e.g., fallback XPST0051 diagnostics) are resolved by the new implementation, so constructor coverage now spans numerics, booleans, temporal values, durations, and QNames.
* Phase 4 is complete: temporal validation utilities moved into a shared helper (`date_time_utils.*`) so constructors, casts, and future duration/QName work reuse identical logic, canonicalisation helpers ensure date/time/time constructors normalise zero-offset timezones, and `XPathVal::string_to_number()` now accepts the uppercase IEEE tokens (`INF`, `-INF`, `NaN`) before deferring to `strtod()` for other lexemes.
* Phase 5 is complete: constructor resolution now differentiates between valid schema types, unresolved prefixes, and missing type definitions so that `XPST0081` and `XPST0051` are raised with precise diagnostics, and targeted Fluid tests assert the new behaviour. Namespace-sensitive constructors will automatically inherit the same error-path once their descriptors advertise the `namespace_sensitive` flag in later phases.
* Phase 6 Fluid regression coverage is now in place: `test_type_constructors.fluid` exercises happy paths, node atomisation, namespace-sensitive behaviour (now covering the implemented QName/duration constructors), and lexical/arity error conditions, while `test_type_expr.fluid` gained chaining scenarios that ensure constructor metadata survives `cast as`/`treat as` expressions. Integration of QT3 3.0 constructor groups remains deferred per the latest instructions, so that remains the sole open testing task.
* Latest validation: `ctest --build-config Release --test-dir build/agents -L xquery --output-on-failure` now passes all 23 labelled suites following the Release rebuild/install on this branch, confirming that the constructor additions did not introduce regressions. Re-run the same command after any further evaluator or schema updates.
