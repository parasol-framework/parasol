# Attribute Value Template (AVT) Investigation Plan

## Current Status

**Test Results**: 5 out of 8 FLWOR tests passing (improved from 4/8)

**Completed Fixes**:
1. ✅ XPath variable context architecture - changed `XPathContext::variables` from owned map to pointer
2. ✅ EXPRESSION wrapper handling - added handler to unwrap EXPRESSION nodes to their child nodes

**Remaining Issues**: 3 tests still failing with different symptoms

---

## Investigation Priorities

### 1. GROUP BY Variable Rebinding (Test 4 - HIGHEST PRIORITY)

**Test**: `testGroupByAggregatesSequences`

**XPath Expression**:
```xpath
for $book in /library/book
group by $genre := string($book/@genre)
return <group genre="{$genre}" count="{count($book)}"/>
```

**Symptom**:
- `{$genre}` ✅ works correctly (fixed by EXPRESSION wrapper)
- `{count($book)}` ❌ returns `nil` instead of the count of grouped items
- Error: `Fiction group should contain two members, got nil`

**Investigation Questions**:
1. How is `$book` rebound after GROUP BY?
   - In FLWOR GROUP BY, the variable should become a sequence of all items in that group
   - Is this rebinding happening correctly?

2. Is the grouped sequence stored in the correct XPathVal type?
   - Does `count()` expect a specific sequence type?
   - Is the NodeSet structure correct?

3. What is the value of `$book` after grouping vs. before grouping?

**Investigation Steps**:

1. Add logging in `evaluate_flwor_pipeline()` after GROUP BY rebinding:
   ```cpp
   // After GROUP BY creates grouped sequences
   pf::Log log("XPath");
   log.warning("After GROUP BY: variable='%s', type=%d, is_nodeset=%s, size=%zu",
      variable_name.c_str(),
      int(book_value.Type),
      book_value.Type == XPVT::NodeSet ? "yes" : "no",
      book_value.Type == XPVT::NodeSet ? book_value.node_set.size() : 0);
   ```

2. Add logging in `function_count()` implementation:
   ```cpp
   // At start of function_count
   pf::Log log("XPath");
   log.warning("count() called with %zu args, arg[0] type=%d",
      Args.size(), Args.size() > 0 ? int(Args[0].Type) : -1);
   if (Args.size() > 0 && Args[0].Type == XPVT::NodeSet) {
      log.warning("  NodeSet size=%zu", Args[0].node_set.size());
   }
   ```

3. Check if grouped sequences maintain proper XPathVal structure

**Files to Examine**:
- `src/xpath/xpath_evaluator_values.cpp` - `evaluate_flwor_pipeline()` function, GROUP BY handling
- `src/xpath/functions/sequence_functions.cpp` or `xpath_functions.cpp` - `function_count()` implementation

**Expected Fix**:
- Likely issue with how grouped sequences are stored in the variable
- May need to ensure proper NodeSet structure when creating grouped sequences

**2025-10-15 Findings**:
- Running `xml.mtFindTag` directly with debugging confirms the GROUP BY clause forms three grouped tuples, yet no `<group>` nodes are emitted and both `genre`/`count` attributes remain empty.
- The callback bound to the FLWOR query never fires, implying the return expression currently yields an empty node-set even though tuples exist after grouping.
- Instrumentation showed the grouping phase executes, so the fault likely resides in how the grouped bindings are surfaced to the return clause (e.g. the aggregated `$book` value is not exposed as the expected node-set when the return expression runs).
- Next session should focus on the return-clause evaluation path after grouping, especially how `TupleScope` materialises grouped bindings for attribute value templates such as `count($book)`.

---

### 2. Path Expressions on Variables in AVTs (Test 6)

**Test**: `testCountProvidesPosition`

**XPath Expression**:
```xpath
for $book in /library/book
order by number($book/@price) ascending
count $position
return <entry pos="{$position}" id="{$book/@id}"/>
```

**Symptom**:
- `{$position}` ✅ likely works (variable lookup fixed)
- `{$book/@id}` ❌ returns empty string
- Error: `COUNT clause should align positional counters with sorted order, got ""`

**Investigation Questions**:
1. Can path expressions be evaluated on variable references?
2. Is there a PATH node wrapping the variable reference?
3. What AST structure represents `$book/@id`?
   - Likely: `PATH(VARIABLE_REFERENCE('book'), STEP(@id))`

**Investigation Steps**:

1. Add logging to see the AST structure:
   ```cpp
   // In evaluate_attribute_value_template, before evaluating expression
   pf::Log log("XPath");
   std::string ast_sig = build_ast_signature(expr);
   log.warning("AVT expression AST: %s", ast_sig.c_str());
   ```

2. Check if PATH nodes need unwrapping:
   - Similar to EXPRESSION wrapper fix
   - PATH might need special handling when first child is VARIABLE_REFERENCE

3. Trace the evaluation of `$book/@id`:
   ```cpp
   // In evaluate_expression
   if (ExprNode->type IS XPathNodeType::PATH) {
      log.warning("PATH node with %zu children", ExprNode->child_count());
      if (ExprNode->child_count() > 0) {
         log.warning("  child[0] type=%d value='%s'",
            int(ExprNode->get_child(0)->type),
            ExprNode->get_child(0)->value.c_str());
      }
   }
   ```

**Files to Examine**:
- `src/xpath/xpath_evaluator_values.cpp` - PATH node handling in `evaluate_expression()`
- `src/xpath/xpath_evaluator_values.cpp` - `evaluate_path_expression_value()` function

**Expected Fix**:
- Likely need to handle PATH nodes that start with VARIABLE_REFERENCE
- May need to evaluate the variable first, then apply the path steps to the result

---

### 3. Nested FLWOR in AVTs (Test 5 - MOST COMPLEX)

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

## Recommended Investigation Order

1. **Test 4 (GROUP BY count)** - Start here, likely quickest fix
   - Debug the grouped sequence structure
   - Verify `count()` function can handle grouped sequences

2. **Test 6 (Path on variable)** - Medium complexity
   - Check AST structure for `$variable/@attribute`
   - Likely need PATH node handling similar to EXPRESSION fix

3. **Test 5 (Nested FLWOR)** - Most complex, tackle last
   - Requires proper nested scope handling
   - May need multiple node type handlers

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
- ❌ testGroupByAggregatesSequences → Fix count($book) returning nil
- ❌ testGroupBySequenceAccess → Fix nested FLWOR in AVT
- ❌ testCountProvidesPosition → Fix $book/@id path expression
- ✅ testInvalidOrdering
- ✅ testInvalidGroupByExpression
