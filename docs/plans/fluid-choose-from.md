# Choose Expressions & Pattern Matching Feature Request

## Overview

Enhance Fluid's LuaJIT syntax with modernised `choose` capabilities inspired by C# 8+ switch expressions and pattern
matching. The feature aims to reduce boilerplate, improve readability, and encourage explicit handling of complex state
dispatch logic.

### Goals

1. Provide an expression-based `choose` construct that yields a value (analogous to nested `if`/`elseif` chains) while
   retaining a familiar, declarative syntax.
2. Introduce pattern matching options (table destructuring, tuple matches, relational guards) so developers can express
   intent succinctly, particularly in UI and data transformation code.
3. Ensure the implementation remains syntactic sugar that lowers to existing LuaJIT bytecode, avoiding VM changes.

### Non-Goals

* No new runtime types—pattern matches operate on existing tables, tuples (represented as multiple return values or
  arrays), and primitives.
* No compile-time exhaustiveness checking in the first iteration; unmatched cases fall through to an explicit `else` or
  return `nil`.

---

## Proposed Syntax

### 1. Basic Choose Expression

```lua
local status_text = choose status from
   200 -> 'OK'
   404 -> 'Not Found'
   else -> 'Unknown'
end
```

**Desugared form:**

```lua
local __value = status
local status_text
if __value is 200 then
   status_text = 'OK'
elseif __value is 404 then
   status_text = 'Not Found'
else
   status_text = 'Unknown'
end
```

**Semantics:**

* The scrutinee expression (`status`) is evaluated **exactly once** and stored in a temporary.
* Literal patterns use Fluid's `is` operator (equivalent to `==`) for equality comparison.
* Patterns are tested **in declaration order**; the first match wins.

---

### 2. Pattern Cases with Guards

```lua
local icon = choose notification from
   { type = 'message', unread = true } -> 'icon-inbox-unread'
   { type = 'message' } when notification.priority > 5 -> 'icon-priority'
   nil -> 'icon-alert'
   else -> 'icon-default'
end
```

**Desugared form:**

```lua
local __value = notification
local icon
if type(__value) is 'table' and __value.type is 'message' and __value.unread is true then
   icon = 'icon-inbox-unread'
elseif type(__value) is 'table' and __value.type is 'message' and __value.priority > 5 then
   icon = 'icon-priority'
elseif __value is nil then
   icon = 'icon-alert'
else
   icon = 'icon-default'
end
```

---

### 3. Tuple Matching

```lua
local movement = choose (dx, dy) from
   (0, 0) -> 'standing'
   (0, _) -> 'vertical'
   (_, 0) -> 'horizontal'
   else -> 'diagonal'
end
```

**Desugared form:**

```lua
local __t0, __t1 = dx, dy
local movement
if __t0 is 0 and __t1 is 0 then
   movement = 'standing'
elseif __t0 is 0 then
   movement = 'vertical'
elseif __t1 is 0 then
   movement = 'horizontal'
else
   movement = 'diagonal'
end
```

---

### 4. Relational Patterns

```lua
local category = choose speed from
   < 30 -> 'slow'
   < 60 -> 'normal'
   else -> 'fast'
end
```

**Desugared form:**

```lua
local __value = speed
local category
if __value < 30 then
   category = 'slow'
elseif __value < 60 then
   category = 'normal'
else
   category = 'fast'
end
```

**Supported relational operators:** `<`, `<=`, `>`, `>=`

---

## Semantic Specifications

### Table Pattern Semantics

Table patterns use **open record** matching with **shallow** comparison:

1. **Open records**: A pattern `{ key = value }` matches any table that has *at least* the specified keys with matching
   values. Extra keys on the matched table are ignored.

2. **Shallow comparison**: Pattern values are compared using `is` (`==`). Nested table patterns are **not** supported
   in v1; nested tables must be matched via guards or sequential `choose` expressions.

3. **Missing keys**: If a key specified in the pattern does not exist on the target table (i.e., returns `nil`), the
   pattern fails to match. This is consistent with `table.key is patternValue` returning `false` when `table.key` is
   `nil` and `patternValue` is not `nil`.

4. **Nil check**: Before checking keys, the compiler emits `type(__value) is 'table'` to guard against nil/non-table
   values.

**Example with extra keys:**

```lua
local t = { type = 'message', unread = true, timestamp = 12345 }
local result = choose t from
   { type = 'message', unread = true } -> 'match'  -- Matches! Extra 'timestamp' key is ignored
   else -> 'no match'
end
-- result is 'match'
```

### Field Bindings (Future Extension)

The grammar reserves space for field bindings in a future version:

```lua
{ type = 'message', priority = p } when p > 5 -> ...
```

This would desugar to:

```lua
if type(__value) is 'table' and __value.type is 'message' and __value.priority != nil then
   local p = __value.priority
   if p > 5 then
      __result = ...
   end
end
```

Field bindings are **not** included in v1 but the parser should accept them to avoid breaking changes later.

---

### Tuple Semantics

1. **Arity enforcement**: Pattern tuples must have **exactly the same arity** as the scrutinee tuple. Mismatched arity
   is a **compile-time error**.

   ```lua
   choose (x, y) from
      (0, 0, 0) -> 'error'  -- Compile error: pattern arity (3) != scrutinee arity (2)
   end
   ```

2. **Evaluation**: Scrutinee expressions are evaluated left-to-right, exactly once, into temporaries:

   ```lua
   local __t0, __t1 = dx, dy
   ```

3. **Function returns**: When the scrutinee is a function call, the arity is determined by the pattern with the most
   elements. Extra return values are discarded by standard Lua semantics.

   ```lua
   choose getCoords() from  -- getCoords() returns (x, y, z)
      (0, 0) -> 'origin'    -- Only first two values matched; z is discarded
   end
   ```

---

### Wildcard `_` Semantics

The underscore `_` is a **syntactic wildcard** that matches any value without binding:

1. **Pattern-only**: `_` is only special in pattern position within `choose`. Outside of patterns, `_` remains a normal
   identifier (preserving compatibility with existing code that uses `_` for i18n or unused variables).

2. **No code generation**: `_` slots generate no comparison code; they simply skip that position.

3. **Bare wildcard**: A bare `_` pattern matches anything:

   ```lua
   choose value from
      nil -> 'nil'
      _ -> 'something'  -- Matches any non-nil value (due to order)
   end
   ```

4. **No variable capture**: Unlike some languages, `_` does not bind the matched value. Use an explicit variable or
   guard to access the value.

---

### Guard Semantics

Guards are evaluated **only after** the structural pattern matches:

```lua
{ type = 'message' } when notification.priority > 5 -> ...
```

Desugars to:

```lua
if type(__value) is 'table' and __value.type is 'message' then
   if notification.priority > 5 then  -- Guard evaluated only if pattern matched
      __result = ...
   end
end
```

Guards have access to all variables in scope, including the original scrutinee variable (e.g., `notification`).

---

### First-Match-Wins Rule

Patterns are tested in **declaration order**. The first matching pattern's result is used; subsequent patterns are not
evaluated. This enables intentional shadowing:

```lua
choose speed from
   < 30 -> 'slow'
   < 60 -> 'normal'   -- Only reached if speed >= 30
   else -> 'fast'     -- Only reached if speed >= 60
end
```

**Warning**: Unreachable patterns (e.g., `< 60` before `< 30`) are not detected in v1. A future lint pass may warn
about obviously dead patterns.

---

## Expression vs Statement Context

### Expression Context

When `choose` appears where a value is expected (RHS of assignment, return, function argument, table constructor), the
compiler uses **direct assignment** when possible:

```lua
-- Source
local x = choose v from
   1 -> 'one'
   else -> 'other'
end

-- Desugared (direct assignment)
local __value = v
local x
if __value is 1 then
   x = 'one'
else
   x = 'other'
end
```

For complex expression positions (e.g., inside a function call or arithmetic), the compiler wraps in an IIFE:

```lua
-- Source
print(choose v from 1 -> 'one' else -> 'other' end)

-- Desugared (IIFE wrapper)
print((function()
   local __value = v
   if __value is 1 then return 'one' end
   return 'other'
end)())
```

### Statement Context

When `choose` appears as a standalone statement (no assignment), it desugars to a plain `if`/`elseif`/`else` chain with
no result variable:

```lua
-- Source
choose action from
   'save' -> saveDocument()
   'load' -> loadDocument()
   else -> showError('Unknown action')
end

-- Desugared
local __value = action
if __value is 'save' then
   saveDocument()
elseif __value is 'load' then
   loadDocument()
else
   showError('Unknown action')
end
```

---

## Exhaustiveness & Error Behaviour

### Default Behaviour (No `else`)

When no `else` branch is provided and no pattern matches:

* **Expression context**: Result is `nil`.
* **Statement context**: No action taken (silent fall-through).

This is the **default** behaviour, chosen to avoid surprising runtime crashes in production code.

```lua
local x = choose status from
   200 -> 'OK'
   404 -> 'Not Found'
end
-- If status is 500, x is nil
```

### Strict Mode (Optional)

A future enhancement may add a `strict` modifier or pragma that causes a runtime error when no pattern matches:

```lua
local x = choose strict status from
   200 -> 'OK'
   404 -> 'Not Found'
end
-- If status is 500, throws: "non-exhaustive choose at <file>:<line>"
```

This is **not** included in v1 but the design leaves room for it.

### Recommendation

Developers are encouraged to always include an `else` branch for explicit handling of unexpected values, even if it
just returns `nil` or logs a warning.

---

## Implementation Sketch

### 1. Lexer

* Introduce reserved words: `choose`, `from`
* Recognise `->` as the case arrow token
* Recognise `when` as the guard keyword
* `_` remains a normal identifier but is treated specially in pattern AST nodes

### 2. Parser

* Parse `choose <expr> from` followed by a sequence of cases terminated by `end`
* Each case:
  * Literal value (number, string, boolean, `nil`)
  * Table pattern `{ key = value, ... }`
  * Tuple pattern `(expr, expr, ...)`
  * Relational pattern (`< expr`, `<= expr`, `> expr`, `>= expr`)
  * Wildcard `_`
* Optional `when <condition>` guard after any pattern
* Optional `else -> <result>` as final case
* Generate AST nodes preserving pattern order

### 3. Semantic Analysis

* Validate tuple arity consistency across all patterns
* Check for obvious errors (e.g., `else` not last, duplicate literal patterns)
* Determine expression vs statement context

### 4. Code Generation

* Emit scrutinee evaluation into temporaries
* Generate `if`/`elseif`/`else` chain with pattern conditions
* Inline simple patterns (literals, relationals) directly
* Generate type checks and key comparisons for table patterns
* Skip comparison code for `_` wildcards
* Wrap in IIFE only when necessary for complex expression positions

---

## Testing Strategy

### Core Tests

1. **Basic value matching**: literals, strings, booleans, `nil`
2. **Default branch**: verify `else` catches unmatched values
3. **No else behaviour**: verify `nil` result when no match
4. **Table patterns**:
   * Exact key match
   * Extra keys on target (open record semantics)
   * Missing keys (pattern fails)
   * Nested tables (requires guard, not direct pattern)
5. **Tuple patterns**:
   * Exact arity match
   * Arity mismatch (compile error)
   * Wildcard `_` in various positions
   * Function return as scrutinee
6. **Relational patterns**: `<`, `<=`, `>`, `>=` with various types
7. **Guards**: ensure evaluated only after pattern matches
8. **First-match-wins**: verify order-dependent matching
9. **Nested choose**: `choose` inside another `choose` body
10. **Expression positions**: assignment, return, function argument, table constructor
11. **Statement context**: side-effect-only branches

### Edge Cases

* `_` identifier already in scope (should not interfere with wildcard)
* Empty table pattern `{}` (matches any table)
* Single-element tuple `(x)` vs parenthesised expression
* `choose` with only `else` branch
* Interaction with `??` and other Fluid operators

---

## Documentation & Rollout

1. Update Fluid language reference with:
   * Complete syntax specification
   * Desugaring examples for transparency
   * Semantic rules (open records, arity, first-match-wins)
2. Provide migration guidance for existing `if`/`elseif` chains
3. Document the `nil` default for missing `else` prominently
4. Include "gotchas" section covering:
   * Order-dependent relational patterns
   * Table pattern limitations (shallow only)
   * `_` wildcard vs `_` variable

---

## Rationale

Developers frequently implement UI dispatch logic, icon selection, and state-based routing in Fluid. The `choose`
expression:

* Reduces verbosity over nested `if`/`elseif`
* Encourages explicit handling of `nil` and unexpected values
* Provides pattern matching expressiveness without runtime cost
* Aligns Fluid with modern language ergonomics familiar to engineers from C#, Kotlin, and Rust
* Maintains Fluid's philosophy of readable, predictable code through clear desugaring semantics
