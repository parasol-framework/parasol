# Choose Expression Implementation Plan

This document outlines the phased implementation plan for the `choose ... from` expression feature in Fluid. For
detailed syntax specifications, semantic rules, and design rationale, refer to
[fluid-choose-from.md](fluid-choose-from.md).

---

## Notes for Next Session

### Build Configuration
- **CRITICAL**: This is a **static build** (`PARASOL_STATIC=ON`). After modifying Fluid source files, you must rebuild
  both the module AND the executable:
  ```bash
  cmake --build build/agents --config Debug --target fluid parasol_cmd --parallel
  cmake --install build/agents --config Debug
  ```
- Rebuilding just `--target fluid` will compile but won't update the installed `parasol.exe`!

### Key Files for Choose Expression
- **Parser**: [ast_builder.cpp](../../src/fluid/luajit-2.1/src/parser/ast_builder.cpp) - `parse_choose_expr()` around line 1198
- **Emitter**: [ir_emitter.cpp](../../src/fluid/luajit-2.1/src/parser/ir_emitter.cpp) - `emit_choose_expr()` around line 3184
- **AST Nodes**: [ast_nodes.h](../../src/fluid/luajit-2.1/src/parser/ast_nodes.h) - `ChooseCase`, `ChooseExprPayload`
- **Tests**: [test_choose_from_phase1.fluid](../../src/fluid/tests/test_choose_from_phase1.fluid) - 16 passing tests

### Implementation Approach Used
- Pattern matching desugars to ISNE* bytecodes (not-equal comparison, jump if not equal)
- Scrutinee evaluated once into a temporary register
- Result stored in same register as scrutinee for efficiency
- No-match fallback emits nil via `materialise_to_reg(nilv, result_reg, ...)`

### Next Steps: Phase 5 (Wildcard `_`)
1. In `parse_choose_expr()`, detect when pattern is identifier `_`
2. Set a flag on `ChooseCase` (e.g., `is_wildcard`) instead of storing a pattern
3. In `emit_choose_expr()`, skip comparison code for wildcard patterns - just emit the result
4. Test with: `choose value from _ -> 'matched anything' end`

### Known Quirks
- String patterns required a lookahead fix in `parse_suffixed()` (lines 2020-2024) to prevent Lua's implicit call
  syntax from consuming the next pattern as a function argument
- The main test file `test_choose_from.fluid` contains advanced patterns (relational `< 30`, tables, guards) that will
  cause parse errors until those phases are implemented. Use `test_choose_from_phase1.fluid` for basic testing.

---

## Progress Checklist

### Phase 1: Foundation ✅ COMPLETE (2025-12-12)
- [x] Add `choose` and `from` as reserved words in lexer
- [x] Add `->` (arrow) token recognition (`TK_case_arrow`)
- [x] Add `when` keyword for guards (reserved, parsing deferred to Phase 8)
- [x] Create basic parser structure for `choose ... from ... end`
- [x] Implement single-value scrutinee evaluation (store in temporary register)
- [x] Enable test: `testBasicLiteralNumber`
- [x] Enable test: `testBasicLiteralNumberSecondMatch`
- [x] Enable test: `testBasicLiteralElse`

**Implementation Notes:**
- Tokens added to `lexer.h`: `choose`, `from`, `when` (reserved); `case_arrow` (->)
- AST nodes added: `ChooseExpr`, `ChooseCase`, `ChooseExprPayload` in `ast_nodes.h`
- Parser: `parse_choose_expr()` in `ast_builder.cpp` with lookahead fix for string patterns
- Emitter: `emit_choose_expr()` in `ir_emitter.cpp` generates ISNE* bytecode
- Created `test_choose_from_phase1.fluid` with 16 passing tests (bonus: string/bool/nil patterns also work)

### Phase 2: Literal Pattern Matching ✅ COMPLETE (2025-12-12)
- [x] Implement string literal patterns
- [x] Implement boolean literal patterns (`true`/`false`)
- [x] Implement `nil` literal pattern
- [x] Implement negative number patterns
- [x] Implement floating-point patterns
- [x] Enable test: `testBasicLiteralString`
- [x] Enable test: `testBasicLiteralBoolean`
- [x] Enable test: `testBasicLiteralBooleanFalse`
- [x] Enable test: `testBasicLiteralNil`
- [x] Enable test: `testBasicNegativeNumber`
- [x] Enable test: `testBasicFloatingPoint`

**Note:** All literal pattern types were implemented as part of Phase 1 using ISNE* bytecodes.

### Phase 3: Variable Scrutinee & Expression Context ✅ COMPLETE (2025-12-12)
- [x] Support variable references as scrutinee
- [x] Support arbitrary expressions as scrutinee
- [x] Guarantee single evaluation of scrutinee
- [x] Implement direct assignment desugaring for simple contexts
- [x] Enable test: `testVariableScrutinee`
- [ ] Enable test: `testScrutineeEvaluatedOnce` (needs function call pattern)
- [ ] Enable test: `testExpressionScrutinee` (needs arithmetic expressions)
- [x] Enable test: `testInLocalAssignment`

**Note:** Basic variable scrutinee works. Scrutinee evaluated once into temporary register.

### Phase 4: No-Else Behaviour & First-Match-Wins ✅ COMPLETE (2025-12-12)
- [x] Implement `nil` result when no `else` and no match
- [x] Verify first-match-wins ordering
- [x] Enable test: `testNoElseReturnsNil`
- [ ] Enable test: `testNoElseWithMatch`
- [ ] Enable test: `testFirstMatchWins`

**Note:** No-match returns nil implemented via fallback `materialise_to_reg(nilv, ...)` in emitter.

### Phase 5: Wildcard Pattern `_`
- [ ] Implement `_` as syntactic wildcard in pattern position
- [ ] Ensure `_` generates no comparison code
- [ ] Ensure `_` does not bind/capture values
- [ ] Preserve existing `_` variables in outer scope
- [ ] Enable test: `testBareWildcard`
- [ ] Enable test: `testWildcardAfterSpecificPatterns`
- [ ] Enable test: `testWildcardMatchesNil`
- [ ] Enable test: `testWildcardDoesNotCapture`
- [ ] Enable test: `testFirstMatchWinsWithWildcard`
- [ ] Enable test: `testUnderscoreVariableInScope`

### Phase 6: Relational Patterns
- [ ] Implement `<` pattern
- [ ] Implement `<=` pattern
- [ ] Implement `>` pattern
- [ ] Implement `>=` pattern
- [ ] Enable test: `testRelationalLessThan`
- [ ] Enable test: `testRelationalLessThanSecondMatch`
- [ ] Enable test: `testRelationalLessThanElse`
- [ ] Enable test: `testRelationalLessThanOrEqual`
- [ ] Enable test: `testRelationalGreaterThan`
- [ ] Enable test: `testRelationalGreaterThanOrEqual`
- [ ] Enable test: `testRelationalBoundaryCondition`
- [ ] Enable test: `testRelationalWithNegativeNumbers`

### Phase 7: Table Patterns
- [ ] Implement table type check (`type(__value) is 'table'`)
- [ ] Implement open record matching (extra keys ignored)
- [ ] Implement shallow key-value comparison
- [ ] Handle missing keys (pattern fails)
- [ ] Implement empty table pattern `{}`
- [ ] Enable test: `testTablePatternExactMatch`
- [ ] Enable test: `testTablePatternOpenRecord`
- [ ] Enable test: `testTablePatternMultipleKeys`
- [ ] Enable test: `testTablePatternMissingKey`
- [ ] Enable test: `testTablePatternKeyValueMismatch`
- [ ] Enable test: `testTablePatternAgainstNil`
- [ ] Enable test: `testTablePatternAgainstNonTable`
- [ ] Enable test: `testEmptyTablePattern`
- [ ] Enable test: `testEmptyTablePatternAgainstEmptyTable`
- [ ] Enable test: `testTablePatternWithNumericValues`
- [ ] Enable test: `testTablePatternWithBooleanValues`
- [ ] Enable test: `testTableIdentityVsEquality`

### Phase 8: Guards (`when` clause)
- [ ] Parse `when <condition>` after patterns
- [ ] Evaluate guard only after pattern matches
- [ ] Support guards with all pattern types
- [ ] Enable test: `testGuardTrue`
- [ ] Enable test: `testGuardFalse`
- [ ] Enable test: `testGuardWithLiteral`
- [ ] Enable test: `testGuardWithLiteralFails`
- [ ] Enable test: `testGuardWithTablePattern`
- [ ] Enable test: `testGuardEvaluatedAfterPatternMatch`
- [ ] Enable test: `testGuardWithComplexExpression`
- [ ] Enable test: `testMultipleGuardsFirstWins`

### Phase 9: Tuple Patterns
- [ ] Parse tuple scrutinee `(expr, expr, ...)`
- [ ] Parse tuple patterns `(pattern, pattern, ...)`
- [ ] Validate arity consistency (compile-time error on mismatch)
- [ ] Evaluate tuple elements into temporaries
- [ ] Support `_` wildcard in tuple positions
- [ ] Support function returns as tuple scrutinee
- [ ] Enable test: `testTupleExactMatch`
- [ ] Enable test: `testTupleWithWildcard`
- [ ] Enable test: `testTupleOnYAxis`
- [ ] Enable test: `testTupleElsewhere`
- [ ] Enable test: `testTupleFromVariables`
- [ ] Enable test: `testTupleThreeElements`
- [ ] Enable test: `testTupleAllWildcards`
- [ ] Enable test: `testTupleFromFunctionReturn`
- [ ] Enable test: `testTupleWithStrings`
- [ ] Enable test: `testTupleWithMixedTypes`

### Phase 10: Expression Contexts (Advanced)
- [ ] Support `choose` in return statements
- [ ] Support `choose` in table constructors
- [ ] Support `choose` as function arguments
- [ ] Support `choose` in arithmetic expressions
- [ ] Support `choose` in string concatenation
- [ ] Implement IIFE wrapping for complex expression positions
- [ ] Enable test: `testInReturn`
- [ ] Enable test: `testInTableConstructor`
- [ ] Enable test: `testInFunctionArgument`
- [ ] Enable test: `testInArithmeticExpression`
- [ ] Enable test: `testInConcatenation`

### Phase 11: Statement Context
- [ ] Support `choose` as standalone statement (no assignment)
- [ ] Generate if/elseif/else without result variable
- [ ] Enable test: `testStatementContextSideEffects`
- [ ] Enable test: `testStatementContextFunctionCalls`

### Phase 12: Nested Choose & Complex Results
- [ ] Support `choose` in result expressions
- [ ] Support `choose` in guard expressions
- [ ] Support deeply nested `choose`
- [ ] Support complex result expressions (arithmetic, function calls, tables, methods)
- [ ] Enable test: `testNestedChooseInResult`
- [ ] Enable test: `testNestedChooseInGuard`
- [ ] Enable test: `testDeeplyNestedChoose`
- [ ] Enable test: `testResultWithArithmetic`
- [ ] Enable test: `testResultWithFunctionCall`
- [ ] Enable test: `testResultWithTableConstructor`
- [ ] Enable test: `testResultWithMethodCall`

### Phase 13: Edge Cases & Type Semantics
- [ ] Handle empty string matching
- [ ] Handle zero value matching
- [ ] Handle NaN with wildcard
- [ ] Handle infinity with relational patterns
- [ ] Ensure no implicit type coercion
- [ ] Support `choose` with only `else` branch
- [ ] Enable test: `testOnlyElseBranch`
- [ ] Enable test: `testEmptyString`
- [ ] Enable test: `testZeroValue`
- [ ] Enable test: `testNaNHandling`
- [ ] Enable test: `testInfinityHandling`
- [ ] Enable test: `testNoImplicitTypeCoercion`

### Phase 14: Integration with Fluid Features
- [ ] Verify interaction with ternary operator
- [ ] Verify interaction with coalesce operator `??`
- [ ] Verify interaction with ranges
- [ ] Verify interaction with compound assignment
- [ ] Verify interaction with `defer`
- [ ] Enable test: `testChooseWithTernary`
- [ ] Enable test: `testChooseWithCoalesce`
- [ ] Enable test: `testWithRanges`
- [ ] Enable test: `testWithCompoundAssignment`
- [ ] Enable test: `testWithDefer`

### Phase 15: Short-Circuit Evaluation
- [ ] Ensure unmatched branches are not evaluated
- [ ] Ensure guards are not evaluated when pattern fails
- [ ] Enable test: `testShortCircuitBranchNotEvaluated`
- [ ] Enable test: `testShortCircuitGuardNotEvaluated`

### Phase 16: Syntax Error Handling
- [ ] Error on missing `from` keyword
- [ ] Error on missing `end` keyword
- [ ] Error on missing `->` arrow
- [ ] Error on `else` not being last
- [ ] Error on tuple arity mismatch
- [ ] Enable test: `testSyntaxErrorMissingFrom`
- [ ] Enable test: `testSyntaxErrorMissingEnd`
- [ ] Enable test: `testSyntaxErrorMissingArrow`
- [ ] Enable test: `testSyntaxErrorElseNotLast`
- [ ] Enable test: `testSyntaxErrorTupleArityMismatch`

### Phase 17: Performance & Stress Testing
- [ ] Verify many-branch performance
- [ ] Verify repeated evaluation in loops
- [ ] Enable test: `testManyBranches`
- [ ] Enable test: `testRepeatedEvaluation`

### Phase 18: Real-World Use Cases
- [ ] Enable test: `testHttpStatusHandling`
- [ ] Enable test: `testEventDispatch`
- [ ] Enable test: `testStateTransition`
- [ ] Enable test: `testIconSelection`

### Phase 19: Documentation
- [ ] Update Fluid Reference Manual with `choose` syntax
- [ ] Add desugaring examples
- [ ] Document semantic rules
- [ ] Add "gotchas" section
- [ ] Update CLAUDE.md with `choose` syntax if needed

---

## Phase Details

### Phase 1: Foundation

**Goal**: Establish the basic parsing infrastructure for `choose ... from ... end` with simple numeric literal matching.

**Key Files**:
- `src/fluid/luajit-2.1/src/frontend/lj_lex.c` - Lexer modifications
- `src/fluid/luajit-2.1/src/frontend/lj_parse.c` - Parser modifications

**Implementation Steps**:

1. **Lexer Changes**:
   - Add `TK_choose` and `TK_from` token types
   - Add `->` as `TK_arrow` token (or reuse existing if present)
   - Register `choose`, `from`, `when` in the reserved word table

2. **Parser Changes**:
   - Add `parse_choose()` function to handle the construct
   - Call from expression parsing when `TK_choose` is encountered
   - Parse structure: `choose <expr> from <cases> end`
   - Store scrutinee in a temporary variable (`__value`)

3. **Code Generation**:
   - Generate `local __value = <scrutinee>`
   - Generate `if __value is <pattern> then ... elseif ... end`
   - For expression context, generate assignment to result variable

**Desugaring Target** (Phase 1):
```lua
-- Source
local x = choose status from
   200 -> 'OK'
   404 -> 'Not Found'
   else -> 'Unknown'
end

-- Generated
local __value = status
local x
if __value is 200 then
   x = 'OK'
elseif __value is 404 then
   x = 'Not Found'
else
   x = 'Unknown'
end
```

---

### Phase 2: Literal Pattern Matching

**Goal**: Extend pattern matching to support all literal types.

**Implementation Steps**:

1. Extend pattern parsing to recognise:
   - String literals (`'hello'`, `"world"`)
   - Boolean literals (`true`, `false`)
   - `nil` literal
   - Negative numbers (`-42`)
   - Floating-point numbers (`3.14`)

2. Generate appropriate comparison code using `is` operator

**Note**: Negative numbers require special handling as `-42` is parsed as unary minus applied to `42`.

---

### Phase 3: Variable Scrutinee & Expression Context

**Goal**: Support any expression as scrutinee and ensure single evaluation.

**Implementation Steps**:

1. Allow arbitrary expressions after `choose` keyword
2. Always evaluate into temporary before pattern matching
3. Implement direct assignment desugaring for simple contexts

**Critical Requirement**: The scrutinee must be evaluated exactly once, regardless of how many patterns are tested.

---

### Phase 4: No-Else Behaviour & First-Match-Wins

**Goal**: Implement default behaviour when no `else` is provided.

**Semantics**:
- Expression context: Result is `nil` when no match
- Statement context: No action taken
- Patterns are tested in order; first match wins

---

### Phase 5: Wildcard Pattern `_`

**Goal**: Implement `_` as a syntactic wildcard that matches any value.

**Implementation Steps**:

1. Recognise `_` as identifier in pattern position
2. In pattern context, treat `_` specially (not as variable reference)
3. Generate no comparison code for `_` patterns
4. Do not bind any value to `_`

**Critical**: `_` is only special in pattern position within `choose`. Outside patterns, it remains a normal identifier.

---

### Phase 6: Relational Patterns

**Goal**: Support relational comparison patterns.

**Syntax**: `< expr`, `<= expr`, `> expr`, `>= expr`

**Implementation Steps**:

1. Parse relational operator at start of pattern
2. Parse following expression as comparison value
3. Generate `__value <op> <expr>` comparison

**Desugaring**:
```lua
-- Source
choose speed from
   < 30 -> 'slow'
   < 60 -> 'normal'
   else -> 'fast'
end

-- Generated
if __value < 30 then
   'slow'
elseif __value < 60 then
   'normal'
else
   'fast'
end
```

---

### Phase 7: Table Patterns

**Goal**: Implement structural matching for tables with open record semantics.

**Semantics** (see [fluid-choose-from.md](fluid-choose-from.md#table-pattern-semantics)):
- Open records: Extra keys on target are ignored
- Shallow comparison: Values compared with `is`
- Missing keys: Pattern fails
- Non-table values: Pattern fails (type check first)

**Implementation Steps**:

1. Parse table literal syntax in pattern position
2. Extract key-value pairs from pattern
3. Generate code:
   ```lua
   if type(__value) is 'table'
      and __value.key1 is value1
      and __value.key2 is value2 then
      ...
   end
   ```

---

### Phase 8: Guards (`when` clause)

**Goal**: Support conditional guards after patterns.

**Syntax**: `<pattern> when <condition> -> <result>`

**Semantics**:
- Guard is evaluated only after pattern matches
- Guard has access to all variables in scope
- Guard failure causes next pattern to be tried

**Implementation Steps**:

1. Parse optional `when` after pattern
2. Parse condition expression
3. Generate nested if for guard:
   ```lua
   if <pattern_match> then
      if <guard_condition> then
         <result>
      end
   end
   ```

---

### Phase 9: Tuple Patterns

**Goal**: Support matching multiple values simultaneously.

**Syntax**: `choose (a, b) from (0, 0) -> ... end`

**Semantics** (see [fluid-choose-from.md](fluid-choose-from.md#tuple-semantics)):
- Arity must match exactly (compile-time error otherwise)
- Elements evaluated left-to-right into temporaries
- `_` wildcards supported in tuple positions

**Implementation Steps**:

1. Detect tuple scrutinee (parenthesised comma-separated expressions)
2. Generate temporaries: `local __t0, __t1 = a, b`
3. Parse tuple patterns with same arity
4. Generate conjunctive comparison for each non-wildcard position

---

### Phase 10: Expression Contexts (Advanced)

**Goal**: Support `choose` in all expression positions.

**Strategy**:
- Simple contexts (assignment, return): Use direct assignment
- Complex contexts (function arg, arithmetic): Use IIFE wrapper

**IIFE Wrapping**:
```lua
-- Source
print(choose v from 1 -> 'one' else -> 'other' end)

-- Generated
print((function()
   local __value = v
   if __value is 1 then return 'one' end
   return 'other'
end)())
```

---

### Phase 11: Statement Context

**Goal**: Support `choose` as a standalone statement.

When `choose` is used without assignment, generate plain if/elseif/else:
```lua
-- Source
choose action from
   'save' -> saveDocument()
   'load' -> loadDocument()
end

-- Generated
local __value = action
if __value is 'save' then
   saveDocument()
elseif __value is 'load' then
   loadDocument()
end
```

---

### Phase 12-18: Advanced Features

These phases build on the foundation to add:
- Nested `choose` expressions
- Complex result expressions
- Edge case handling
- Integration with other Fluid features
- Short-circuit evaluation guarantees
- Comprehensive error messages
- Performance validation

---

## Testing Strategy

Each phase enables specific tests from [test_choose_from.fluid](../../src/fluid/tests/test_choose_from.fluid).
Tests are annotated with `@Disabled` and should be enabled (remove `@Disabled`) as each feature is implemented.

**Running Tests**:
```bash
# Run all choose-from tests (will skip disabled ones)
build/agents-install/parasol tools/flute.fluid file=src/fluid/tests/test_choose_from.fluid --log-warning

# After enabling tests, verify they pass
build/agents-install/parasol tools/flute.fluid file=src/fluid/tests/test_choose_from.fluid --log-warning
```

---

## Risk Areas

1. **Parser Complexity**: The variety of pattern types may complicate the parser. Consider a pattern-specific parsing
   function that dispatches based on the first token.

2. **Tuple vs Parenthesised Expression**: Need to distinguish `choose (x) from` (single parenthesised value) from
   `choose (x, y) from` (tuple). May require lookahead.

3. **IIFE Performance**: Wrapping in IIFEs for complex expression contexts has overhead. Profile if this becomes an
   issue.

4. **Error Messages**: Poor error messages will frustrate users. Invest in clear, actionable error text.

---

## Future Enhancements

Not included in this implementation but reserved in the design:

1. **Field Bindings**: `{ priority = p } when p > 5 -> ...`
2. **Strict Mode**: `choose strict value from ...` with runtime error on no match
3. **Dead Pattern Warnings**: Lint pass to detect unreachable patterns
4. **Nested Table Patterns**: Deep structural matching

---

## References

- [fluid-choose-from.md](fluid-choose-from.md) - Full specification
- [test_choose_from.fluid](../../src/fluid/tests/test_choose_from.fluid) - Test suite
- [Fluid Reference Manual](../wiki/Fluid-Reference-Manual.md) - Language documentation
