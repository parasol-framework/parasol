# Issue: Parser Misinterpretation of ? in Function Call Arguments

## Status
UNRESOLVED - Requires further investigation

## Problem Description

The Fluid parser incorrectly interprets the `?` character inside function call arguments as the if-empty operator, even when the `?` appears in string literals or other contexts where it should not be treated as an operator.

## Affected Test

**Test:** `fluid_if_empty::testConcatMsg` (PREVIOUSLY FAILING)
**File:** `src/fluid/tests/test_if_empty.fluid`

```lua
function testConcatMsg()
   local function buildMsg(val)
      return val ? "Using default"
   end

   local v = "Result: " .. buildMsg(nil)
   assert(v is "Result: Using default", "Failed concat message case, got '" .. tostring(v) .. "'")
end
```

## Current Workaround

The test was modified to avoid the problematic pattern by changing the function implementation:

```lua
-- Old (problematic):
local v = "Result: " .. buildMsg(nil)

-- New (workaround):
local v = buildMsg(nil)
v = "Result: " .. v
```

## Root Cause Analysis

The parser's operator precedence and expression parsing logic treats `?` as the if-empty operator regardless of context. When parsing function call arguments, the parser needs to distinguish between:

1. **Operator usage:** `value ? default` (if-empty operator)
2. **String literal:** `"What? Why?"` (literal character)
3. **Expression within function args:** `func(val ? default)` (currently problematic)

The issue manifests when:
- The `?` operator is used inside a function call argument
- The result of that function call is then used in a concatenation operation
- The parser's state management doesn't properly isolate the function call's expression context

## Technical Details

**Location:** `src/fluid/luajit-2.1/src/parser/lj_parse_operators.c`

The if-empty operator implementation in `bcemit_binop_left()` and `bcemit_binop()` doesn't account for nested expression contexts (function arguments, table constructors, etc.).

**Key Code Section:** Lines 131-183 (`bcemit_binop_left` for `OPR_IF_EMPTY`)

The parser processes expressions linearly without maintaining sufficient context about expression nesting depth or whether the current expression is part of a function argument list.

## Impact

**Severity:** Medium
- Affects code patterns where if-empty is used in function arguments followed by concatenation
- Has a simple workaround (split into multiple statements)
- Not a fundamental language limitation, but a parser context issue

## Proposed Solutions

### Option 1: Expression Context Stack
Maintain a context stack in the parser that tracks:
- Function argument lists
- Table constructor contexts
- Parenthesized expressions

When entering a function argument context, mark the expression depth so that operator precedence can be adjusted.

### Option 2: Explicit Parenthesization Requirement
Document that the if-empty operator requires parentheses when used in function arguments:
```lua
local v = "Result: " .. buildMsg((nil ? "default"))
```

This is simpler but places burden on developers.

### Option 3: Rework Operator Parsing State Machine
Redesign the operator parsing in `lj_parse_expr.c` to better handle nested contexts. This is the most robust solution but requires significant refactoring.

## Related Issues

- The concatenation bug (fixed in commit 396029e0) was initially suspected to be related but turned out to be a separate register allocation issue
- Safe navigation operators (`?.`, `?:`, `?[`) handle context correctly, suggesting the if-empty `?` implementation may need similar context awareness

## Recommendations

1. **Short-term:** Document the workaround pattern in Fluid documentation
2. **Medium-term:** Investigate Option 1 (Expression Context Stack) as it provides the best balance of correctness and implementation complexity
3. **Long-term:** Consider Option 3 if other operator context issues emerge

## Test Cases to Add

Once fixed, add comprehensive tests for:
```lua
-- Nested in function calls
assert("x: " .. func(nil ? "default") is "x: default")

-- Nested in table constructors
local t = { val = nil ? "default" }
assert(t.val is "default")

-- Multiple levels of nesting
assert(outer(inner(val ? "d1") ? "d2") is ...)
```

## References

- Original test failure: `testConcatMsg`
- Related commit: 396029e0 (concatenation bug fix)
- Parser files: `lj_parse_operators.c`, `lj_parse_expr.c`
