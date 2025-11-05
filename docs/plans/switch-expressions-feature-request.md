# Switch Expressions & Pattern Matching Feature Request

## Overview
Enhance Fluid's LuaJIT syntax with modernised `switch` capabilities inspired by C# 8+ switch expressions and pattern matching. The feature aims to reduce boilerplate, improve readability, and encourage explicit handling of complex state dispatch logic.

### Goals
1. Provide an expression-based `switch` construct that yields a value (analogous to nested `if`/`elseif` chains) while retaining a familiar, declarative syntax.
2. Introduce pattern matching options (table destructuring, tuple matches, relational guards) so developers can express intent succinctly, particularly in UI and data transformation code.
3. Ensure the implementation remains syntactic sugar that lowers to existing LuaJIT bytecode, avoiding VM changes.

### Non-Goals
* No new runtime types—pattern matches operate on existing tables, tuples (represented as multiple return values or arrays), and primitives.
* No compile-time exhaustiveness checking in the first iteration; unmatched cases fall through to an explicit `else`.

## Proposed Syntax

### 1. Basic Switch Expression
```lua
local status_text = switch status of
   200 -> 'OK'
   404 -> 'Not Found'
   else -> 'Unknown'
end
```
*Desugared form (conceptual)*:
```lua
local __value = status
local status_text
if __value == 200 then
   status_text = 'OK'
elseif __value == 404 then
   status_text = 'Not Found'
else
   status_text = 'Unknown'
end
```

### 2. Pattern Cases with Guards
```lua
local icon = switch notification of
   { type = 'message', unread = true } -> 'icon-inbox-unread'
   { type = 'message' } when notification.priority > 5 -> 'icon-priority'
   nil -> 'icon-alert'
   else -> 'icon-default'
end
```
*Desugars to sequential checks ensuring the table shape and guard expression succeed before yielding the branch result.*

### 3. Tuple Matching
```lua
local movement = switch (dx, dy) of
   (0, 0) -> 'standing'
   (0, _) -> 'vertical'
   (_, 0) -> 'horizontal'
   else -> 'diagonal'
end
```
*Tuples compile to temporary locals (`local __a, __b = dx, dy`) followed by comparisons. `_` acts as a wildcard.*

### 4. Relational Patterns
```lua
local category = switch speed of
   < 30 -> 'slow'
   < 60 -> 'normal'
   else -> 'fast'
end
```
*Rewritten as ordered relational tests on the switch expression value.*

## Implementation Sketch

1. **Lexer**
   - Introduce tokens for `switch`, `of`, `->`, `when`, tuple parentheses inside switch contexts, and wildcard `_`.

2. **Parser**
   - Parse `switch <expr> of` followed by a sequence of cases terminated by `end`.
   - Each case may be:
     * Literal value (numbers, strings, booleans).
     * Table literal pattern (`{ key = value, ... }`), optionally binding variables for use in the guard/body.
     * Tuple pattern `(expr, expr, ...)` with `_` wildcard.
     * Relational pattern (`< expr`, `<= expr`, etc.).
   - Optional `when <condition>` guard.
   - Optional `else` catch-all.
   - Generate AST nodes representing the ordered dispatch list and the switch expression.

3. **Desugaring**
   - Convert the AST into a local scope:
     ```lua
     do
        local __value = <switch expression>
        local __result
        -- sequential if/elseif/else with pattern checks and guards
        return __result  -- for expression context
     end
     ```
   - Ensure variable scopes behave intuitively (case bodies can introduce locals).
   - For statement context, allow `switch` without assigning to a variable, but expression form is the priority.

4. **Pattern Helpers**
   - Implement helper functions/macros for:
     * Table pattern checking (nil guard, key equality).
     * Tuple comparison (multiple locals).
   - Wildcard `_` compiles to `true`.
   - Guards become `and` conditions appended to the pattern test.

5. **Error Handling**
   - Require an `else` branch if the developer wants a defined fallback; otherwise emit a runtime error when no case matches (opt-in for strictness).
   - Provide syntax errors for misplaced `when`, invalid tuple lengths, or malformed patterns.

## Testing Strategy
1. Add Flute tests validating:
   - Basic value matching and default branch.
   - Table patterns with guards (including missing keys).
   - Tuple/wildcard matching ordering.
   - Relational patterns overlapping (ensure first match wins).
   - Nested switch usage and expression context (e.g., in table constructors).
2. Confirm desugared output preserves short-circuit behaviour for guards.
3. Validate mixed usage with upcoming features (`??`, optional chaining) to ensure precedence remains intuitive.

## Documentation & Rollout
1. Update Fluid language reference with new syntax, examples, and the desugaring explanation so behaviour is transparent.
2. Explain compatibility: old `switch` statements (if any) remain available or upgraded to new syntax.
3. Provide guidance on using `else` to enforce exhaustiveness and how to combine with guard conditions.

## Rationale
Developers frequently implement UI dispatch logic, icon selection, and state-based routing in Fluid. The proposed `switch` enhancements:
* Reduce verbosity over nested `if`/`elseif`.
* Encourage explicit handling of `nil` and unexpected values.
* Provide pattern matching expressiveness without adding runtime cost.
* Align Fluid with modern language ergonomics familiar to engineers coming from C# and other contemporary languages.

