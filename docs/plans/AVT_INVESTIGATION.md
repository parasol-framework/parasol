# Attribute Value Template (AVT) Investigation Plan

### Nested FLWOR in AVTs (Test 5 - MOST COMPLEX)

**Test**: `testGroupBySequenceAccess`

**XPath Expression**:
```xpath
for $book in /library/book
group by $genre := $book/@genre
return <group genre="{$genre}"
              members="{string-join(for $member in $book return string($member/@id), ",")}"/>
```

**Symptom**:
- Still getting: `Attribute value template expression could not be evaluated`
- The AVT contains a nested FOR expression inside a function call

**Structure**:
```
AVT {
  string-join(
    for $member in $book
    return string($member/@id),
    ","
  )
}
```

**Investigation Questions**:
1. Are FOR_EXPRESSION nodes handled in `evaluate_expression()`?
2. Does the nested FOR properly inherit the outer scope's variables?
3. Is the variable context pointer preserved through nested evaluations?

**Investigation Steps**:

1. Check if FOR_EXPRESSION is handled:
   ```bash
   grep -n "FOR_EXPRESSION" src/xpath/xpath_evaluator_values.cpp
   ```

2. If not handled, need to add handler (similar to EXPRESSION wrapper)

3. Verify variable scope inheritance:
   ```cpp
   // When evaluating nested FOR
   log.warning("Nested FOR evaluation: context.variables=%p, size=%zu",
      context.variables,
      context.variables ? context.variables->size() : 0);
   ```

4. Check that the nested FOR can see `$book` from outer scope

**Files to Examine**:
- `src/xpath/xpath_evaluator_values.cpp` - FOR_EXPRESSION handling
- `src/xpath/xpath_evaluator_values.cpp` - `evaluate_flwor_pipeline()` for nested FLWOR

**Expected Fix**:
- May need to add FOR_EXPRESSION handler to dispatch to `evaluate_flwor_pipeline()`
- Ensure variable context is properly passed to nested evaluations

---

## Quick Diagnostic Test

Add this logging in `evaluate_attribute_value_template()` after expression evaluation:

```cpp
XPathVal value = evaluate_expression(expr, CurrentPrefix);

pf::Log log("XPath");
log.warning("AVT evaluated: expr_type=%d(%s), result_type=%d, to_string='%s'",
   int(expr->type),
   expr->value.c_str(),
   int(value.Type),
   value.to_string().c_str());

if (evaluation_failed) {
   log.warning("  FAILED: expression_unsupported was set");
}
```

**Location**: `src/xpath/xpath_evaluator_values.cpp:1092` (after `XPathVal value = evaluate_expression(expr, CurrentPrefix);`)

This will show exactly what each AVT expression is producing and help pinpoint where the nil/empty values are coming from.

---

## Related Files

**Core evaluation**:
- `src/xpath/xpath_evaluator_values.cpp` - Main expression evaluation
- `src/xpath/xpath_evaluator_context.cpp` - Context management

**Functions**:
- `src/xpath/xpath_functions.cpp` or `src/xpath/functions/sequence_functions.cpp` - count(), string-join(), etc.

**Test file**:
- `src/xml/tests/test_xpath_flwor_clauses.fluid` - All test cases

**AST definitions**:
- `include/parasol/modules/xpath.h` - XPathNodeType enum

---

## Success Criteria

All 8 tests passing in `test_xpath_flwor_clauses.fluid`:
- ✅ testWhereFiltersNodes
- ✅ testOrderByAscendingStrings
- ✅ testOrderByDescendingNumbers
- ✅ testGroupByAggregatesSequences → Fix count($book) returning nil
- ❌ testGroupBySequenceAccess → Fix nested FLWOR in AVT
- ✅ testCountProvidesPosition → Fix $book/@id path expression
- ✅ testInvalidOrdering
- ✅ testInvalidGroupByExpression
