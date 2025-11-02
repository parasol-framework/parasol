# XPath Expression Evaluator Stack Overflow Fix

## Problem Summary

The XPath expression evaluator previously crashed with a stack overflow when processing deeply nested binary operations. The issue was reproduced by the `benchmarkTokenLongArithmetic()` test in `src/xml/tests/benchmark.fluid`, which generates an expression with 200 consecutive additions `(1 + 2 + 3 + ... + 200)`. Recent refactoring reduced the stack footprint enough for that benchmark to pass, but the evaluator still evaluates binary chains recursively, so the underlying risk remains for longer expressions.

### Root Cause

**Location:** `src/xquery/eval/eval_expression.cpp`  
Recursive evaluation within `XPathEvaluator::handle_binary_op()` and `XPathEvaluator::handle_binary_arithmetic()` visits each operand of a left-skewed binary tree via separate `evaluate_expression()` calls. The parser (see `src/xquery/parse/xquery_parser.cpp`) builds arithmetic chains as strictly left-associative spines, so a long sequence of `+` or `*` operators produces a call stack whose depth equals the number of operators.

**Stack Usage:**
- Recursion depth grows linearly with operator count
- Windows stack exhaustion was observed at 200 operators before the refactor
- The algorithm still permits unbounded recursion, so a sufficiently long chain can overflow again despite the smaller per-frame footprint

## Solution Design

Implement iterative evaluation for associative binary operation chains. Flattening these chains preserves semantics while eliminating unbounded recursion depth.

### Strategy

1. **Detect operation chains** - Identify sequences of the same associative binary operator.
2. **Flatten to iterative evaluation** - Collect operands and evaluate them in a loop.
3. **Preserve semantics** - Maintain left-to-right evaluation order and existing error handling.
4. **Limit scope** - Leave existing logic untouched for non-chain cases.

### Operations to Optimise

**Arithmetic (associative):**
- `ADD` (`+`)
- `MUL` (`*`)

**Not suitable for flattening:**
- Comparisons (`EQ`, `NE`, `LT`, `LE`, `GT`, `GE`) - not associative
- `SUB` (`-`) and `DIV` (`/`) - not associative; parentheses change results
- `MOD` - order of evaluation matters
- `RANGE` (`..`) - produces sequences, not associative
- `AND` / `OR` - require strict short-circuit behaviour; leave unchanged

## Implementation Plan

### Phase 1: Helper Functions

**File:** `src/xquery/eval/eval_expression.cpp`

Add helper functions (placed near existing evaluator utilities):

1. **`is_arithmetic_chain_candidate()`**
   - Input: `BinaryOperationKind`
   - Returns `true` for `ADD` or `MUL`, `false` otherwise

2. **`collect_operation_chain()`**
   - Inputs: root node, target `BinaryOperationKind`
   - Recursively walks the binary tree, gathering leaf operands
   - Stops when encountering a different operator or non-`BINARY_OP` node
   - Output: `std::vector<const XPathNode *>` in evaluation order

3. **`evaluate_arithmetic_chain()`**
   - Inputs: operand vector, operation kind, `CurrentPrefix`
   - Iteratively evaluates operands and combines results
   - Checks `expression_unsupported` after each evaluation
   - Returns a final `XPathVal`

### Phase 2: Modify `handle_binary_op()`

Update `XPathEvaluator::handle_binary_op()` to inspect arithmetic nodes before dispatching:

```cpp
auto operation = Node->get_value_view();
auto op_kind = map_binary_operation(operation);

if (is_arithmetic_chain_candidate(op_kind)) {
   std::vector<const XPathNode *> operands = collect_operation_chain(Node, op_kind);
   if (operands.size() >= 3) {
      return evaluate_arithmetic_chain(operands, op_kind, CurrentPrefix);
   }
}

// Fall back to existing handling (logical, set, comparison, arithmetic, etc.)
```

### Phase 3: Implementation Details

#### `collect_operation_chain()` Algorithm

```
function collect_operation_chain(node, target_op_kind):
   result = []

   function walk(n):
      if n.type != BINARY_OP:
         result.append(n)
         return

      if map_binary_operation(n.value) != target_op_kind:
         result.append(n)
         return

      walk(n.left_child)
      walk(n.right_child)

   walk(node)
   return result
```

> The helper is recursive but only traverses the parse tree; it does not evaluate nodes and therefore uses negligible stack space compared with the evaluator.

#### `evaluate_arithmetic_chain()` Algorithm

```
function evaluate_arithmetic_chain(operands, op_kind, prefix):
   accumulator = evaluate_expression(operands[0], prefix)
   if expression_unsupported: return empty

   result = accumulator.to_number()

   for i from 1 to operands.length - 1:
      operand_value = evaluate_expression(operands[i], prefix)
      if expression_unsupported: return empty

      operand_num = operand_value.to_number()

      switch op_kind:
         case ADD: result += operand_num
         case MUL: result *= operand_num

   return XPathVal(result)
```

### Phase 4: Testing Strategy

1. **Verify existing tests pass**
   ```bash
   cd src/xml/tests
   [path_to]/parasol.exe ../../../tools/flute.fluid file=[path_to_project]/src/xml/tests/test_basic.fluid --gfx-driver=headless
   ```

2. **Run the long-expression benchmark**
   ```bash
   cd src/xml/tests
   [path_to]/parasol.exe ../../../tools/flute.fluid file=[path_to_project]/src/xml/tests/benchmark.fluid --gfx-driver=headless
   ```

3. **Add targeted tests**
   - Short chains (2-5 operands) to verify correctness
   - Medium chains (20-50 operands) to confirm stack usage
   - Long chains (200+ operands) to validate the overflow fix
   - Mixed expressions to ensure non-flattened cases still behave
   - Nested expressions to test boundary detection

### Phase 5: Edge Cases and Considerations

1. **Preserve evaluation order** - `collect_operation_chain()` must emit operands in left-to-right order.
2. **Error propagation** - Return immediately if `expression_unsupported` becomes true.
3. **Type conversions** - Continue to use `to_number()` for arithmetic operands.
4. **Threshold tuning** - Start with a chain threshold of 3 operands (minimum to offset overhead); adjust after profiling.
5. **Non-arithmetic operations** - Leave comparisons, subtraction, division, modulus, and logical operators untouched.

## Implementation Order

1. [x] Create this plan document
2. [x] Add `is_arithmetic_chain_candidate()` helper
3. [x] Add `collect_operation_chain()` helper
4. [x] Add `evaluate_arithmetic_chain()` helper
5. [x] Modify `handle_binary_op()` to use chain detection
6. [x] Compile and verify no regressions with existing tests
7. [ ] Test with benchmark.fluid to verify fix
8. [ ] Add unit tests for chain evaluation
9. [ ] Performance testing and threshold tuning (optional)
10. [ ] Update AGENTS.md or other documentation if needed

## Success Criteria

- [ ] `benchmark.fluid` completes without stack overflow
- [ ] All existing XPath/XQuery tests pass
- [ ] Chain evaluation matches recursive evaluation results
- [ ] Stack usage remains bounded regardless of expression depth
- [ ] No performance regression for typical expressions (< 10 operands)

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking existing tests | High | Run full XPath/XQuery suites and targeted cases |
| Performance regression | Medium | Only flatten chains with >=3 operands; profile long expressions |
| Incorrect evaluation order | High | Maintain strict left-to-right operand collection |
| Missing edge cases | Medium | Add focused unit tests and extend benchmarks |

## Alternative Approaches Considered

1. **Increase stack size via linker options** - Treats the symptom, wastes memory, and still risks overflow on extreme inputs.
2. **Tail-call optimisation** - Not reliable in portable C++; the pattern is not tail-recursive.
3. **Trampoline or explicit stack** - Heavier refactor than necessary for this scope.
4. **Parser-side flattening** - Requires changing AST construction and impacts other consumers.

## Future Enhancements

- Investigate safe flattening or iterative execution for `AND`/`OR` while preserving short-circuit semantics.
- Profile the evaluator to tune the chain threshold and confirm performance gains.
- Explore compile-time simplification (e.g. constant folding) to shrink evaluation trees further.


