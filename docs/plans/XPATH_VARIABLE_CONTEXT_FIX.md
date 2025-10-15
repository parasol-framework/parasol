# XPath Variable Context Architecture Fix

## Problem Statement

XPath FLWOR expressions (GROUP BY, COUNT) fail with "Attribute value template expression could not be evaluated" error when variables like `$genre` and `$position` are used in direct element constructors.

### Root Cause

The `XPathContext` struct contains a `variables` map that has **lexical scoping semantics** (variables should persist across nested scopes), but it's being managed with **dynamic context semantics** (saved and restored by `push_context()`/`pop_context()`).

When `pop_context()` restores the context:

```cpp
context = context_stack.back();  // Line 162 in xpath_evaluator_context.cpp
```

This **wholesale replacement** wipes out all variables that were added after the corresponding `push_context()` call, even though those variables should still be in scope.

### Execution Flow Showing the Bug

```
1. push_context() is called
   - Saves entire context (including variables map with 0 variables) to stack

2. VariableBindingGuard objects are created
   - Add variables like $genre, $position to context.variables

3. Expression evaluation begins
   - Variables ARE visible at this point

4. pop_context() is called during evaluation
   - Restores saved context from stack
   - WIPES OUT all variables! (restores the map with 0 variables)

5. AVT expressions try to use variables
   - Variables not found
   - Error: "Attribute value template expression could not be evaluated"
```

### Evidence from Logs

```
XPath [XML:559] VariableBindingGuard: Added variable 'genre' to context @00000090B298EC18 (size now: 1)
XPath [XML:559] VariableBindingGuard: Added variable 'book' to context @00000090B298EC18 (size now: 2)
XPath [XML:559] pop_context: Current context @00000090B298EC18 has 2 variables
XPath [XML:559] pop_context: Restoring context from stack
XPath [XML:559] pop_context: Restored context @00000090B298EC18 now has 0 variables  ← BUG!
```

## Semantic Analysis

The `XPathContext` struct contains fields with different lifetime semantics:

### Dynamic Evaluation Context (should be stack-scoped)
- `context_node` - Current node being evaluated
- `attribute_node` - Current attribute being evaluated
- `position` - Position in current node-set (for `position()` function)
- `size` - Size of current node-set (for `last()` function)

These change frequently during tree traversal and should be saved/restored.

### Lexical Variable Scope (should NOT be stack-scoped)
- `variables` - Variable bindings following lexical scoping rules

Variables follow nested scope semantics: inner scopes can shadow outer scopes, but restoring a context shouldn't remove them. Variables should only be removed when the `VariableBindingGuard` that added them destructs.

### Global/Semi-Global State
- `document` - Root XML document
- `expression_unsupported` - Error flag
- `schema_registry` - Schema type registry

## Solution: Make Variables a Pointer

Change `context.variables` from an owned map to a pointer to storage owned by the evaluator.

### Architecture

**Before:**
```
XPathContext {
   variables: Map<string, XPathVal>  ← Owned, copied by push/pop
}
```

**After:**
```
XPathEvaluator {
   variable_storage: Map<string, XPathVal>  ← Owned by evaluator
}

XPathContext {
   variables: *Map<string, XPathVal>  ← Pointer, not copied by push/pop
}
```

### Why This Works

1. When `push_context()` does `context_stack.push_back(context)`, it only copies the **pointer**, not the map
2. When `pop_context()` does `context = context_stack.back()`, the restored **pointer** still points to the same storage
3. Multiple `VariableBindingGuard` objects all modify the same `variable_storage` via the pointer
4. Clear ownership: Evaluator owns storage, context just references it
5. Variables persist across `push_context()`/`pop_context()` calls as required

## Implementation Plan

### File Changes

#### 1. `src/xpath/xpath_functions.h`

**Change the `variables` field to a pointer:**

```cpp
struct XPathContext
{
   XMLTag * context_node = nullptr;
   const XMLAttrib * attribute_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   ankerl::unordered_dense::map<std::string, XPathVal> * variables = nullptr;  // ← Changed to pointer
   extXML * document = nullptr;
   bool * expression_unsupported = nullptr;
   xml::schema::SchemaTypeRegistry * schema_registry = nullptr;
};
```

**Update `VariableBindingGuard` to use pointer dereference:**

```cpp
class VariableBindingGuard
{
   private:
   XPathContext & context;
   std::string variable_name;
   std::optional<XPathVal> previous_value;
   bool had_previous_value = false;

   public:
   VariableBindingGuard(XPathContext &Context, std::string Name, XPathVal Value)
      : context(Context), variable_name(std::move(Name))
   {
      auto existing = context.variables->find(variable_name);  // ← Use ->
      had_previous_value = (existing != context.variables->end());  // ← Use ->
      if (had_previous_value) previous_value = existing->second;

      (*context.variables)[variable_name] = std::move(Value);  // ← Use (*ptr)[]
   }

   ~VariableBindingGuard()
   {
      if (had_previous_value) (*context.variables)[variable_name] = *previous_value;  // ← Use (*ptr)[]
      else context.variables->erase(variable_name);  // ← Use ->
   }

   VariableBindingGuard(const VariableBindingGuard &) = delete;
   VariableBindingGuard & operator=(const VariableBindingGuard &) = delete;

   VariableBindingGuard(VariableBindingGuard &&) = default;
   VariableBindingGuard & operator=(VariableBindingGuard &&) = default;
};
```

#### 2. `src/xpath/xpath_evaluator.h`

**Add variable storage member:**

```cpp
class XPathEvaluator {
   private:
   extXML * xml;
   XPathContext context;
   XPathArena arena;
   AxisEvaluator axis_evaluator;
   bool expression_unsupported = false;
   bool trace_xpath_enabled = false;
   VLF trace_detail_level = VLF::API;
   VLF trace_verbose_level = VLF::DETAIL;

   // Variable storage owned by the evaluator
   ankerl::unordered_dense::map<std::string, XPathVal> variable_storage;  // ← Add this

   // ... rest of members
};
```

#### 3. `src/xpath/xpath_evaluator.cpp`

**Initialize `context.variables` pointer in constructor:**

```cpp
XPathEvaluator::XPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML, arena)
{
   trace_xpath_enabled = env_flag_enabled(std::getenv("PARASOL_TRACE_XPATH"));
   auto trace_levels = parse_trace_levels(std::getenv("PARASOL_TRACE_XPATH_LEVEL"));
   trace_detail_level = trace_levels.detail;
   trace_verbose_level = trace_levels.verbose;
   context.document = XML;
   context.expression_unsupported = &expression_unsupported;
   context.schema_registry = &xml::schema::registry();
   context.variables = &variable_storage;  // ← Add this
}
```

#### 4. `src/xpath/xpath_evaluator_values.cpp`

**Update variable lookup to use pointer:**

```cpp
if (ExprNode->type IS XPathNodeType::VARIABLE_REFERENCE) {
   auto local_variable = context.variables->find(ExprNode->value);  // ← Use ->
   if (local_variable != context.variables->end()) {  // ← Use ->
      return local_variable->second;
   }

   // Look up variable in the XML object's variable storage
   auto it = xml->Variables.find(ExprNode->value);
   if (it != xml->Variables.end()) {
      return XPathVal(it->second);
   }
   else {
      // Variable not found - XPath 1.0 spec requires this to be an error
      expression_unsupported = true;
      return XPathVal();
   }
}
```

#### 5. Remove Debug Logging

After verifying the fix works, remove the temporary logging added during investigation:

- Remove `log.warning()` calls from `VariableBindingGuard` constructor/destructor
- Remove `log.warning()` calls from `push_context()` and `pop_context()`
- Remove `log.warning()` calls from variable lookup code

### Testing

After implementing the changes:

1. **Build the modules:**
   ```bash
   cmake --build build/agents --config Release --target xpath --parallel
   cmake --build build/agents --config Release --target xml --parallel
   cmake --install build/agents
   ```

2. **Run the FLWOR tests:**
   ```bash
   cd src/xml/tests
   ../../../install/agents/parasol.exe ../../../tools/flute.fluid \
      file=E:/parasol/src/xml/tests/test_xpath_flwor_clauses.fluid \
      --gfx-driver=headless --log-warning
   ```

3. **Expected results:**
   - All 8 tests should pass
   - Tests 4, 5, 6 (GROUP BY and COUNT with AVT) should succeed
   - Test 2 (ORDER BY ascending) may still have ordering issues (separate bug)

### Verification

The fix is correct if:

1. Variables added by `VariableBindingGuard` persist across `push_context()`/`pop_context()` calls
2. AVT expressions can successfully look up variables like `$genre` and `$position`
3. No regressions in other XPath functionality

## Alternative Considered

**Fix `pop_context()` to preserve variables:** Could modify `pop_context()` to save and restore variables separately. This is less clean because it requires special-case handling and doesn't address the semantic mismatch.

## Benefits of Pointer Solution

1. ✅ **Architecturally correct** - Separates dynamic context from lexical scope
2. ✅ **Clear ownership** - Evaluator owns storage, context references it
3. ✅ **No special cases** - Generic copy works correctly
4. ✅ **Minimal changes** - Just change access syntax from `.` to `->`
5. ✅ **Type safe** - Compiler enforces pointer semantics
6. ✅ **Future-proof** - Makes the architectural intent explicit

## References

- **Bug Location:** `src/xpath/xpath_evaluator_context.cpp:162`
- **Affected Code:** `TupleScope` struct in `src/xpath/xpath_evaluator_values.cpp:1893-1911`
- **Test File:** `src/xml/tests/test_xpath_flwor_clauses.fluid`
- **Test Cases:** testGroupByAggregatesSequences, testGroupBySequenceAccess, testCountProvidesPosition
