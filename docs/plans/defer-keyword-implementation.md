# Defer Keyword Implementation Plan

## Overview

This document provides a detailed step-by-step implementation plan for the `defer` keyword in Fluid/LuaJIT. The `defer` keyword allows functions to be registered for execution at the end of the scope in which they are created, similar to Go's `defer` statement.

**Status:** 📋 **Implementation Plan** - Not yet started

**Priority:** ⭐⭐⭐⭐ **High**

**Estimated Effort:** 20-30 hours

## Feature Specification

### Syntax

```lua
defer(args...)
   -- function body
end
```

### Semantics

- **Scope-based execution**: Deferred functions execute at the end of the scope (block, function, etc.) in which they are defined
- **LIFO order**: Multiple defer statements execute in reverse order (last defer first)
- **Guaranteed execution**: Deferred functions execute even if the scope exits early via `return`, `break`, `continue`, or error
- **Closure capture**: Deferred functions capture upvalues from their enclosing scope (standard Lua closure semantics)
- **Parameter handling**: Deferred functions can accept parameters, but they are evaluated at defer time, not at execution time

### Examples

```lua
function thing()
   defer()
      print('executes at the end')
   end
   print('Hello world')
   -- Output: "Hello world"
   --         "executes at the end"
end

function file_handler()
   local f = io.open("file.txt")
   defer()
      if f then f:close() end
   end
   -- file operations...
   -- f:close() is automatically called at function exit
end

function multiple_defers()
   defer()
      print('third')
   end
   defer()
      print('second')
   end
   defer()
      print('first')
   end
   -- Output: "first", "second", "third" (LIFO order)
end
```

## Implementation Steps

### Step 1: Add Defer Token Definition

**File:** `src/fluid/luajit-2.1/src/lj_lex.h`

**Action:**
1. Add `defer` to the keyword list in `TKDEF` macro:
   ```c
   #define TKDEF(_, __) \
     _(and) _(break) _(continue) _(defer) _(do) _(else) _(elseif) _(end) _(false) \
     _(for) _(function) _(goto) _(if) _(in) _(is) _(local) _(nil) _(not) _(or) \
     _(repeat) _(return) _(then) _(true) _(until) _(while) \
     __(or_question, or?) \
     ...
   ```

**Expected Result:**
- `TK_defer` token enum value created
- Token name string available for error messages
- Lexer recognizes `defer` as a reserved keyword

---

### Step 2: Extend FuncScope Structure for Defer Tracking

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add defer-related fields to `FuncScope` structure (around line 100-105):
   ```c
   /* Per-function linked list of scope blocks. */
   typedef struct FuncScope {
     struct FuncScope *prev;	/* Link to outer scope. */
     MSize vstart;			/* Start of block-local variables. */
     uint8_t nactvar;		/* Number of active vars outside the scope. */
     uint8_t flags;		/* Scope flags. */
     BCPos defer_start;		/* First bytecode position with defer handlers. */
     BCPos defer_count;		/* Number of defer handlers in this scope. */
   } FuncScope;
   ```

**Alternative Approach:** Store defer handlers in a separate linked list or array. Consider:
- Adding a pointer to a defer handler list in `FuncScope`
- Using a separate structure to track defer handlers

**Recommended:** Add defer tracking directly to `FuncScope` initially, as it's simpler and defer handlers are inherently scope-local.

**Expected Result:**
- `FuncScope` can track defer handlers for each scope
- Defer count and start position stored per scope

---

### Step 3: Add Scope Flag for Defer Handlers

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add defer flag to scope flags (around line 107-112):
   ```c
   #define FSCOPE_LOOP		0x01	/* Scope is a (breakable) loop. */
   #define FSCOPE_BREAK		0x02	/* Break used in scope. */
   #define FSCOPE_GOLA		0x04	/* Goto or label used in scope. */
   #define FSCOPE_UPVAL		0x08	/* Upvalue in scope. */
   #define FSCOPE_NOCLOSE		0x10	/* Do not close upvalues. */
   #define FSCOPE_CONTINUE	0x20	/* Continue used in scope. */
   #define FSCOPE_DEFER		0x40	/* Defer handlers in scope. */
   ```

**Expected Result:**
- Scope flag available to mark scopes with defer handlers
- Can be checked efficiently during scope exit

---

### Step 4: Parse Defer Statement

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add `parse_defer()` function (similar to `parse_func()`):
   ```c
   /* Parse 'defer' statement. */
   static void parse_defer(LexState *ls, BCLine line)
   {
     FuncState *fs = ls->fs;
     ExpDesc b;
     lj_lex_next(ls);  /* Skip 'defer'. */

     /* Parse defer function body (similar to parse_body, but no name) */
     parse_body(ls, &b, 0, line);

     /* Store defer handler - mark scope as having defers */
     fs->bl->flags |= FSCOPE_DEFER;
     fs->bl->defer_count++;

     /* Store defer function in a register or constant table */
     /* We need to keep the function closure for later execution */
     /* This is the tricky part - how to store for later execution */
   }
   ```

**Challenge:** Storing defer handlers for later execution requires:
- Keeping function closures alive until scope exit
- Storing them in a way that allows iteration in reverse order
- Ensuring they execute even on early returns

**Initial Approach:**
- Store defer handlers as function closures in a temporary table or array
- Each scope maintains a list of defer handlers
- At scope exit, iterate in reverse and call each handler

**Expected Result:**
- `defer` statements are parsed correctly
- Defer handlers are registered with their scope

---

### Step 5: Store Defer Handlers Per Scope

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Implement defer handler storage mechanism:
   - Option A: Store in a Lua table accessible from bytecode
   - Option B: Store function references in registers that persist until scope exit
   - Option C: Emit bytecode to push handlers onto a defer stack

**Recommended Approach:** Option C - Emit bytecode to maintain a defer stack:
- Use a special register or upvalue to hold defer handler stack
- Each defer pushes its closure onto the stack
- At scope exit, pop and call handlers in reverse order

**Implementation:**
```c
/* Parse 'defer' statement. */
static void parse_defer(LexState *ls, BCLine line)
{
   FuncState *fs = ls->fs;
   ExpDesc b;
   BCReg defer_reg;

   lj_lex_next(ls);  /* Skip 'defer'. */

   /* Parse defer function body */
   parse_body(ls, &b, 0, line);

   /* Ensure defer handler is in a register */
   expr_discharge(fs, &b);
   if (b.k != VNONRELOC) {
      bcreg_reserve(fs, 1);
      expr_toreg(fs, &b, fs->freereg - 1);
   }

   /* Mark scope as having defers */
   fs->bl->flags |= FSCOPE_DEFER;
   fs->bl->defer_count++;

   /* Emit bytecode to store defer handler */
   /* We'll need a helper function to manage defer stack */
   bcemit_defer_push(fs, b.u.s.info);
}
```

**Expected Result:**
- Defer handlers are stored in a way that persists until scope exit
- Handlers can be retrieved and executed at scope end

---

### Step 6: Implement Defer Stack Management

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create helper functions for defer stack management:
   ```c
   /* Initialize defer stack for a function. */
   static void defer_init(FuncState *fs)
   {
     /* Create a table to hold defer handlers */
     /* Store as upvalue or in a register */
     /* This is called once per function */
   }

   /* Push a defer handler onto the stack. */
   static void bcemit_defer_push(FuncState *fs, BCReg handler_reg)
   {
     /* Emit bytecode to push handler onto defer stack */
     /* Use table.insert or similar */
   }

   /* Pop and execute all defer handlers for current scope. */
   static void bcemit_defer_execute(FuncState *fs, FuncScope *bl)
   {
     /* Emit bytecode to pop handlers in reverse order */
     /* Call each handler */
     /* Execute bl->defer_count handlers */
   }
   ```

**Challenge:** Defer stack management requires:
- Persistent storage across bytecode execution
- Access from any point in the function
- Efficient push/pop operations

**Approach:** Use a Lua table stored in an upvalue or special register:
- Function starts: create empty table for defer handlers
- Each defer: push closure onto table
- Scope exit: iterate table in reverse, call each handler, remove from table

**Expected Result:**
- Defer handlers can be pushed and executed
- Stack management works correctly

---

### Step 7: Emit Defer Execution at Scope Exit

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `fscope_end()` to execute defer handlers:
   ```c
   /* End a scope. */
   static void fscope_end(FuncState *fs)
   {
     FuncScope *bl = fs->bl;
     LexState *ls = fs->ls;

     /* Execute defer handlers if any */
     if ((bl->flags & FSCOPE_DEFER)) {
        bcemit_defer_execute(fs, bl);
     }

     fs->bl = bl->prev;
     var_remove(ls, bl->nactvar);
     fs->freereg = fs->nactvar;
     /* ... rest of existing code ... */
   }
   ```

**Expected Result:**
- Defer handlers execute at normal scope exit
- Handlers execute in reverse order (LIFO)

---

### Step 8: Handle Defer Execution on Early Returns

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_return()` to execute defer handlers before returning:
   ```c
   /* Parse 'return' statement. */
   static void parse_return(LexState *ls)
   {
     FuncState *fs = ls->fs;
     /* Execute all defer handlers in current and enclosing scopes */
     /* (only those scopes that are still active) */
     bcemit_defer_execute_all(fs);

     /* ... rest of existing return logic ... */
   }
   ```

**Challenge:** Need to execute defers from current scope up to function boundary, but not outer scopes.

**Approach:**
- Track which scopes have active defers
- On return, execute defers from current scope up to function scope
- Use scope chain to determine which defers to execute

**Expected Result:**
- Defer handlers execute before function returns
- Only defers from current function scope execute (not outer scopes)

---

### Step 9: Handle Defer Execution on Break/Continue

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_break()` and `parse_continue()` to execute defer handlers:
   ```c
   /* Parse 'break' statement. */
   static void parse_break(LexState *ls)
   {
     FuncState *fs = ls->fs;
     /* Execute defer handlers in scopes being exited */
     /* Need to execute defers from current scope up to target loop scope */
     bcemit_defer_execute_to_scope(fs, target_scope);

     /* ... rest of existing break logic ... */
   }
   ```

**Challenge:** Break/continue may exit multiple scopes. Need to execute defers for all exited scopes.

**Approach:**
- Track target scope for break/continue
- Execute defers from current scope up to (but not including) target scope
- Use existing scope chain traversal

**Expected Result:**
- Defer handlers execute before break/continue
- All relevant scopes' defers execute

---

### Step 10: Initialize Defer Stack in Function Prologue

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify function prologue to initialize defer stack:
   - In `parse_body()` or `fs_init()`, initialize defer stack table
   - Store as upvalue or in a special register
   - Ensure it's accessible throughout the function

**Implementation:**
```c
/* In parse_body() or similar */
static void parse_body(LexState *ls, ExpDesc *e, int needself, BCLine line)
{
   /* ... existing code ... */

   /* Initialize defer stack */
   bcemit_defer_init(&fs);

   /* ... rest of function body parsing ... */
}
```

**Expected Result:**
- Defer stack is initialized when function starts
- Stack is accessible throughout function execution

---

### Step 11: Implement Defer Handler Execution Bytecode

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `bcemit_defer_execute()` function:
   ```c
   /* Execute defer handlers for a scope. */
   static void bcemit_defer_execute(FuncState *fs, FuncScope *bl)
   {
     BCReg defer_stack_reg = get_defer_stack_reg(fs);
     BCReg handler_reg;
     BCPos loop_start, loop_end;

     if (bl->defer_count == 0)
        return;

     /* Emit loop to execute handlers in reverse order */
     /* Loop from end of defer stack backwards */
     loop_start = fs->pc;

     /* Pop handler from stack */
     /* Call handler */
     /* Continue until stack is empty for this scope */

     loop_end = fs->pc;
     /* Patch loop back to start */
   }
   ```

**Key Considerations:**
- Defer handlers must be called as functions (BC_CALL)
- Need to handle function call semantics correctly
- Must execute in reverse order (LIFO)
- Need to clean up stack after execution

**Expected Result:**
- Defer handlers execute correctly via bytecode
- Handlers execute in reverse order

---

### Step 12: Handle Defer Parameters

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_defer()` to handle defer arguments:
   ```c
   /* Parse 'defer' statement. */
   static void parse_defer(LexState *ls, BCLine line)
   {
     FuncState *fs = ls->fs;
     ExpDesc b, args;
     BCReg nargs;

     lj_lex_next(ls);  /* Skip 'defer'. */

     /* Parse argument list (if any) */
     if (ls->tok == '(') {
        nargs = parse_args_list(ls, &args);
     } else {
        nargs = 0;
     }

     /* Parse defer function body */
     parse_body(ls, &b, 0, line);

     /* Create closure with captured arguments */
     /* Arguments are evaluated at defer time, not execution time */
     /* This means we need to capture argument values, not expressions */

     /* Store defer handler with arguments */
   }
   ```

**Semantic Decision:**
- **Option A**: Arguments evaluated at defer time (current value captured)
- **Option B**: Arguments evaluated at execution time (expression evaluated later)

**Recommendation:** Option A (evaluate at defer time) - matches Go's semantics and is more intuitive.

**Expected Result:**
- Defer statements can accept arguments
- Arguments are evaluated when defer is encountered
- Arguments are captured in closure

---

### Step 13: Add Defer to Statement Parser

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add `TK_defer` case to `parse_stmt()`:
   ```c
   /* Parse a statement. Returns 1 if it must be the last one in a chunk. */
   static int parse_stmt(LexState *ls)
   {
     BCLine line = ls->linenumber;
     switch (ls->tok) {
     /* ... existing cases ... */
     case TK_defer:
        parse_defer(ls, line);
        break;
     /* ... rest of cases ... */
     }
     return 0;
   }
   ```

**Expected Result:**
- `defer` statements are recognized and parsed
- Parser handles defer syntax correctly

---

### Step 14: Create Test Suite

**File:** `src/fluid/tests/test_defer.fluid`

**Action:**
1. Create comprehensive test suite:
   ```lua
   -- Basic defer
   function testBasic()
      local output = {}
      function thing()
         defer()
            table.insert(output, "deferred")
         end
         table.insert(output, "normal")
      end
      thing()
      assert(#output is 2, "Expected 2 items")
      assert(output[1] is "normal", "Expected 'normal' first")
      assert(output[2] is "deferred", "Expected 'deferred' second")
   end

   -- Multiple defers (LIFO order)
   function testMultipleDefers()
      local output = {}
      function thing()
         defer()
            table.insert(output, "third")
         end
         defer()
            table.insert(output, "second")
         end
         defer()
            table.insert(output, "first")
         end
      end
      thing()
      assert(output[1] is "first", "Expected 'first'")
      assert(output[2] is "second", "Expected 'second'")
      assert(output[3] is "third", "Expected 'third'")
   end

   -- Defer with early return
   function testEarlyReturn()
      local output = {}
      function thing()
         defer()
            table.insert(output, "deferred")
         end
         table.insert(output, "before")
         return
         table.insert(output, "never")
      end
      thing()
      assert(#output is 2, "Expected 2 items")
      assert(output[1] is "before", "Expected 'before'")
      assert(output[2] is "deferred", "Expected 'deferred'")
   end

   -- Defer in nested scopes
   function testNestedScopes()
      local output = {}
      function outer()
         defer()
            table.insert(output, "outer")
         end
         function inner()
            defer()
               table.insert(output, "inner")
            end
         end
         inner()
      end
      outer()
      assert(output[1] is "inner", "Expected 'inner' first")
      assert(output[2] is "outer", "Expected 'outer' second")
   end

   -- Defer with break
   function testBreak()
      local output = {}
      while true do
         defer()
            table.insert(output, "deferred")
         end
         table.insert(output, "before")
         break
         table.insert(output, "never")
      end
      assert(output[1] is "before", "Expected 'before'")
      assert(output[2] is "deferred", "Expected 'deferred'")
   end

   -- Defer with upvalues
   function testUpvalues()
      local x = 10
      function thing()
         defer()
            assert(x is 10, "Expected x to be 10")
         end
         x = 20
      end
      thing()
      assert(x is 20, "Expected x to be 20 after function")
   end

   return {
      tests = { 'testBasic', 'testMultipleDefers', 'testEarlyReturn',
                'testNestedScopes', 'testBreak', 'testUpvalues' }
   }
   ```

---

### Step 15: Update Documentation

**File:** `docs/wiki/Fluid-Reference-Manual.md`

**Action:**
1. Add defer keyword section:
   - Syntax and semantics
   - Examples
   - Execution order (LIFO)
   - Scope behavior
   - Interaction with return/break/continue
   - Use cases (resource cleanup, etc.)

---

## Implementation Challenges

### Challenge 1: Defer Stack Storage

**Issue:** Need persistent storage for defer handlers that survives bytecode execution.

**Solutions:**
- **Option A**: Use a Lua table stored as an upvalue
- **Option B**: Use a special register that persists throughout function
- **Option C**: Emit bytecode to maintain stack in Lua table

**Recommendation:** Option C - emit bytecode to maintain defer stack in a table. This is most transparent and debuggable.

---

### Challenge 2: Scope Exit Paths

**Issue:** Defer handlers must execute on all scope exit paths (normal, return, break, continue, error).

**Solution:**
- Modify all exit points to call defer execution
- Use existing scope chain to determine which defers to execute
- Ensure defers execute before any exit bytecode

---

### Challenge 3: Nested Scopes

**Issue:** Defer handlers in nested scopes must execute in correct order.

**Solution:**
- Track defer handlers per scope
- On scope exit, execute only that scope's defers
- Use scope chain to determine scope boundaries

---

### Challenge 4: Defer Argument Evaluation

**Issue:** When should defer arguments be evaluated?

**Solution:**
- Evaluate at defer time (when `defer` statement is encountered)
- Capture argument values in closure
- This matches Go's semantics and is more intuitive

---

### Challenge 5: Error Handling

**Issue:** What happens if a defer handler throws an error?

**Solution:**
- Defer handlers execute even if previous handlers error
- Consider: Should we continue executing remaining defers after error?
- Default: Continue execution (matches Go behavior)

---

## Testing Checklist

- [ ] Basic defer execution
- [ ] Multiple defers (LIFO order)
- [ ] Defer with early return
- [ ] Defer in nested scopes
- [ ] Defer with break
- [ ] Defer with continue
- [ ] Defer with upvalues
- [ ] Defer with arguments
- [ ] Defer in loops
- [ ] Defer in conditionals
- [ ] Defer handler errors
- [ ] Defer in recursive functions
- [ ] Defer with closures
- [ ] Resource cleanup patterns (file.close, etc.)

---

## Key Files to Modify

1. **`src/fluid/luajit-2.1/src/lj_lex.h`**
   - Add `defer` to keyword list

2. **`src/fluid/luajit-2.1/src/lj_parse.c`**
   - Extend `FuncScope` structure
   - Add `FSCOPE_DEFER` flag
   - Implement `parse_defer()` function
   - Modify `fscope_end()` to execute defers
   - Modify `parse_return()` to execute defers
   - Modify `parse_break()` and `parse_continue()` to execute defers
   - Add defer stack management functions
   - Add defer execution bytecode emission

3. **`src/fluid/tests/test_defer.fluid`**
   - Create comprehensive test suite

4. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add defer keyword documentation

5. **`src/fluid/luajit-2.1/AGENTS.md`** (if needed)
   - Add implementation notes if complex patterns emerge

---

## Implementation Order

1. **Phase 1: Core Infrastructure** (8-12 hours)
   - Steps 1-3: Token, scope structure, flags
   - Basic infrastructure in place

2. **Phase 2: Parsing** (4-6 hours)
   - Steps 4-5: Parse defer statements, store handlers
   - Defer statements parse correctly

3. **Phase 3: Execution** (6-8 hours)
   - Steps 6-11: Defer stack management, execution bytecode
   - Defers execute at scope exit

4. **Phase 4: Edge Cases** (4-6 hours)
   - Steps 8-9: Handle return/break/continue
   - All exit paths work correctly

5. **Phase 5: Testing & Documentation** (4-6 hours)
   - Steps 14-15: Test suite, documentation
   - Feature complete and documented

---

## References

### Similar Implementations
- Go's `defer` statement (primary inspiration)
- Rust's `Drop` trait (RAII pattern)
- D's `scope(exit)` statement
- Swift's `defer` statement

### Related Code
- Function parsing: `parse_func()`, `parse_body()` (lines 2865-2883, 2125-2155)
- Scope management: `fscope_begin()`, `fscope_end()` (lines 1523-1576)
- Return handling: `parse_return()` (line 2899+)
- Break/continue handling: `parse_break()`, `parse_continue()`

### External References
- Go defer specification: https://go.dev/ref/spec#Defer_statements
- Lua closure semantics
- LuaJIT bytecode instruction set

---

**Last Updated:** 2025-01-XX
**Status:** Implementation Plan - Ready for implementation
**Related:** Function parsing, scope management, closure handling
