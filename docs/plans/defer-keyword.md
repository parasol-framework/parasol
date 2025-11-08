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

### Step 2: Extend Parser Scope Structures for Defer Tracking

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Extend the parser data structures so each lexical scope knows about the defers it owns:
   ```c
   typedef struct DeferEntry {
      struct DeferEntry *prev;   /* Link to next newest defer in the same scope. */
      BCReg handler_reg;         /* Register holding the closure value. */
      BCReg arg_base;            /* First register that stores evaluated arguments. */
      uint8_t nargs;             /* Number of arguments captured for the handler. */
   } DeferEntry;

   typedef struct FuncScope {
      struct FuncScope *prev;    /* Link to outer scope. */
      MSize vstart;              /* Start of block-local variables. */
      uint8_t nactvar;           /* Number of active vars outside the scope. */
      uint8_t flags;             /* Scope flags. */
      uint16_t defer_base;       /* Total number of defers owned by outer scopes. */
      uint16_t defer_count;      /* Defers registered inside this scope. */
      DeferEntry *defer_top;     /* Stack of defers declared in this scope. */
   } FuncScope;
   ```
2. Add parser helpers (`defer_scope_init`, `defer_scope_push`, `defer_scope_pop_all`) that allocate and recycle `DeferEntry` nodes from `FuncState`. Hosting the helpers alongside the existing scope utilities keeps the control-flow logic focused.

**Expected Result:**
- Each scope records how many defers it owns and where their closures and arguments live.
- Parser helpers let future steps unwind and clean up defers without repeating pointer arithmetic.

---

### Step 3: Add Scope Flag for Defer Handlers

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Introduce a flag for scopes that own defers so control-flow utilities can fast-path the common "no defers" case:
   ```c
   #define FSCOPE_DEFER         0x40    /* Scope has one or more defers. */
   ```
2. Update `fscope_begin()` to copy the parent scope's `defer_base`, reset `defer_count` and `defer_top`, and clear the flag by default.

**Expected Result:**
- Scope metadata can be checked by a single flag test before emitting unwind bytecode.
- Scope creation keeps defer bookkeeping consistent across nested scopes.

---

### Step 4: Parse the `defer` Statement

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Introduce `parse_defer()` and wire it into `parse_stmt()`:
   - Consume the `defer` keyword and capture the current scope pointer (`FuncScope *bl = fs->bl`).
   - Parse an optional parameter list for the handler using the same helpers as anonymous functions (reusing `parse_parlist`). Store the parameter count so the generated prototype mirrors a nested function literal.
   - Require a block terminated by `end`, using `parse_body()` to build the nested prototype and obtain an `ExpDesc` for the closure.
   - Allow an optional trailing expression list `defer (...) ... end (expr1, expr2, ...)` whose values become immediate arguments; reuse `expr_list()` to materialise them in registers.
   - Normalise the resulting closure/argument registers with `expr_toreg()` so the handler lives in a stable slot until we emit unwind code.
2. As soon as the closure has been placed in a register, call `defer_scope_push(fs, bl, handler_reg, arg_base, nargs)` to link a `DeferEntry` into the current scope, increment `defer_count`, and set `FSCOPE_DEFER`.

**Expected Result:**
- `parse_defer()` produces bytecode for the handler function and ensures its closure and arguments occupy known registers.
- Scope metadata reflects the presence of defers immediately, enabling later phases to emit unwind code without re-parsing the AST.

---

### Step 5: Emit Defer Registration Bytecode

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. After `defer_scope_push` returns a populated `DeferEntry`, lower it to bytecode with a new helper `bcemit_defer_register(fs, entry)`:
   - Emit a dedicated `BC_DEFER` instruction that copies the closure (register `entry->handler_reg`) and the eagerly-evaluated argument registers into the runtime defer stack. Encode the argument count in operand `B` so the VM can copy the right number of stack slots, and store the absolute defer index in operand `C` (`bl->defer_base + bl->defer_count - 1`).
   - Immediately release the temporary registers (`fs->freereg = entry->handler_reg`) because the runtime stack now owns the values.
   - Return the `DeferEntry` node to a free list so the parser does not leak allocations.
2. Keep `bl->defer_count` and `fs->total_defer` in sync so later control-flow patches know how many handlers must be unwound.

**Expected Result:**
- Each `defer` statement produces a single bytecode instruction that snapshots the handler and its arguments at the point of declaration.
- Register pressure stays low because the captured values are copied into runtime storage immediately.

---

### Step 6: Provide Runtime Support for Defer Frames

**Files:**
- `src/fluid/luajit-2.1/src/lj_bc.h`
- `src/fluid/luajit-2.1/src/lj_vm.h`
- `src/fluid/luajit-2.1/src/lj_vm.def`
- `src/fluid/luajit-2.1/src/lj_vm.s` (and the portable interpreter fallback)
- `src/fluid/luajit-2.1/src/lj_vmevent.c`
- New files `src/fluid/luajit-2.1/src/lj_defer.c` and `src/fluid/luajit-2.1/src/lj_defer.h`

**Action:**
1. Extend the bytecode definition table with a new `BC_DEFER` opcode (`Amode = base register`, `Bmode = literal argument count`, `Cmode = literal defer index`) and a companion unwind opcode `BC_UNDEFER` (`Amode = literal count to run`, `Dmode = literal base index`). Update the opcode enums, disassembler, and vm name tables accordingly.
2. Implement a lightweight runtime stack structure (`DeferFrame`) stored on the Lua call frame. `BC_DEFER` should:
   - Allocate the frame on first use (growable array of `TValue` pairs `[closure, arg tuple]`).
   - Copy the closure and its `B` arguments from the VM stack into the frame, tagging each entry with the lexical depth passed in operand `C` (so nested scope unwinds can skip entries belonging to outer scopes).
3. Implement `BC_UNDEFER` so it pops entries in reverse order, invoking each closure with the captured argument tuple. If a handler errors, chain the error through the usual VM unwinding while still attempting the remaining handlers. Reuse existing helper code paths for `pcall`/`xpcall` to avoid reimplementing error-state preservation.
4. Update the portable (`lj_vm.c`) and assembly (`lj_vm.S`) interpreters plus the trace recorder (`lj_record.c`) so JIT traces can record and replay the new opcodes. JIT lowering can be deferred to a later change but the recorder must at least abort with a helpful error until support is implemented.

**Expected Result:**
- The VM can store defer handlers efficiently and run them in LIFO order regardless of how the scope exits.
- The new opcodes integrate with the interpreter, the unwinder, and trace recording without regressing existing bytecode.

---

### Step 7: Emit Unwind Bytecode When a Scope Ends

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Update `fscope_end()` so that, before dropping locals, it emits `BC_UNDEFER` whenever `FSCOPE_DEFER` is set:
   ```c
   if ((bl->flags & FSCOPE_DEFER) && bl->defer_count) {
      bcemit_AD(fs, BC_UNDEFER, bl->defer_count, bl->defer_base);
   }
   ```
   Operand `A` carries the number of handlers to run, whilst operand `D` stores the absolute defer index so the VM can skip handlers that belong to outer scopes. After emission, subtract `bl->defer_count` from `fs->total_defer` and clear the scope metadata.
2. Insert the new instruction before any `BC_UCLO` that might close upvalues so the existing jump fix-up code remains valid.

**Expected Result:**
- Normal scope exits now always emit unwind bytecode ahead of the existing clean-up logic.
- Nested scopes unwind only their own defers because operand `D` encodes the scope boundary explicitly.

---

### Step 8: Unwind Defers on `return`

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Extend `parse_return()` so it walks the scope chain and emits cumulative `BC_UNDEFER` instructions for every scope that will be abandoned by the return.
2. Factor this logic into a helper `bcemit_defer_unwind_to(fs, target_level)` that both `parse_return()` and the break/continue paths can reuse. The helper should emit a single `BC_UNDEFER` per contiguous block of scopes instead of one per scope to minimise bytecode size.

**Expected Result:**
- Returning from a function reliably runs all pending defers before control leaves the frame.
- The compiler reuses a single helper for all early-exit paths, reducing maintenance burden.

---

### Step 9: Unwind Defers for `break` and `continue`

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Inside `parse_break()` and `parse_continue()`, compute the loop scope that execution will resume in and call `bcemit_defer_unwind_to()` with that scope as the target.
2. Ensure the helper does nothing when the target scope still owns the defers (e.g., a `continue` inside the same loop body) to avoid double execution.

**Expected Result:**
- Control transfers out of inner scopes never leak defers.
- Code generation stays symmetric between `break` and `continue`.

---

### Step 10: Initialise Defer Accounting in Function Prologues

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Augment `fs_init()` and `parse_body()` so new functions start with `fs->total_defer = 0` and the root scope's `defer_base` set to zero.
2. When a function contains at least one defer, record this in `fs->flags` (e.g., `PROTF_HASDEFER`) so the runtime can lazily allocate the `DeferFrame` on function entry.

**Expected Result:**
- Per-function counters start from a clean slate and the generated prototypes expose whether defer support is required.
- The VM only pays the cost of allocating a defer frame when a function actually uses `defer`.

---

### Step 11: Integrate New Opcodes with Tooling

**Files:**
- `src/fluid/luajit-2.1/src/lj_disasm.c`
- `src/fluid/luajit-2.1/src/lj_load.c`
- `src/fluid/luajit-2.1/src/lj_bcwrite.c`
- `src/fluid/luajit-2.1/src/lj_ffrecord.c`

**Action:**
1. Teach the bytecode writer, reader, and disassembler about `BC_DEFER` and `BC_UNDEFER` so modules compiled offline remain round-trippable.
2. Update the trace recorder to either record the opcodes explicitly or abort traces with a descriptive message until JIT lowering lands. Document any temporary limitations in the developer notes.

**Expected Result:**
- Tooling and debugging aids remain functional when the new opcodes appear in bytecode dumps.
- The JIT subsystem fails gracefully if support for the new instructions is postponed.

---

### Step 12: Statement Parser Integration

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Register the `TK_defer` branch inside `parse_stmt()` and ensure the parser correctly flags a syntax error if the statement appears where it is not permitted (e.g., outside functions when there is no enclosing scope).
2. Add error productions for malformed `defer` syntax so diagnostics mention missing `end` tokens or invalid argument lists explicitly.

**Expected Result:**
- `defer` slots naturally into the statement grammar with precise error handling.
- Users receive actionable diagnostics when they mistype a defer block.

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

### Challenge 1: Runtime Storage for Defers

**Issue:** The VM needs to retain every deferred closure and its eager arguments without interfering with normal stack slots or the GC write barriers.

**Resolution Plan:**
- Implement the dedicated `DeferFrame` container described in Step 6 so each Lua call frame owns its defer stack and can grow it independently of the VM value stack.
- Ensure entries carry both the closure and a packed tuple of arguments so they survive register reuse, and wire the frame into the GC barrier logic so references remain strongly reachable.
- Add amortised growth with explicit upper bounds to avoid unbounded reallocation when thousands of defers are queued.

---

### Challenge 2: Consistent Unwinding Semantics

**Issue:** Every exit path (normal scope end, `return`, `break`, `continue`, and error unwinds) must execute the exact subset of handlers that belong to the abandoned scopes.

**Resolution Plan:**
- Use the `defer_base`/`defer_count` metadata to compute absolute indices and emit the new `BC_UNDEFER` instruction with deterministic arguments.
- Cover the interpreter slow paths and the error unwinder so runtime exceptions trigger the same bytecode sequence the parser would emit for explicit exits.
- Add regression tests for nested loops with mixed `return`/`break` statements to validate the helper introduced in Step 8.

---
### Challenge 3: Interaction with the JIT Trace Recorder

**Issue:** New bytecodes need either native lowering or graceful bail-outs so traces do not produce incorrect results.

**Resolution Plan:**
- Teach `lj_ffrecord.c` to spot `BC_DEFER`/`BC_UNDEFER` and either emit IR that mirrors the interpreter semantics or abort tracing with a precise status code until lowering lands.
- Add trace tests that hit `defer` inside hot loops to confirm the recorder behaviour and avoid accidental trace stitching across unwound scopes.

---

### Challenge 4: Argument Snapshotting

**Issue:** Deferred handlers must observe argument values as they existed when the statement executed, regardless of later mutation.

**Resolution Plan:**
- Reuse the parser logic from Steps 4 and 5 to evaluate and copy arguments into temporary registers before emitting `BC_DEFER`.
- Extend the runtime helper to duplicate the argument tuple into the `DeferFrame`, guaranteeing the captured values survive even if the originating registers are clobbered immediately afterwards.

---

### Challenge 5: Error Propagation During Unwind

**Issue:** A handler may raise an error; the runtime must surface the first error while still attempting the remaining handlers, matching Go's defer semantics.

**Resolution Plan:**
- In the interpreter implementation of `BC_UNDEFER`, catch handler failures and store the pending error object, continue running the remaining handlers, then re-raise once unwinding completes.
- Add tests that combine failing and succeeding handlers to verify the call order and the reported error value.

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
   - Add `defer` to the token table so the lexer recognises the keyword.

2. **`src/fluid/luajit-2.1/src/lj_parse.c` & `src/fluid/luajit-2.1/src/lj_parse.h`**
   - Extend `FuncScope` with defer bookkeeping helpers.
   - Implement `parse_defer()`, `bcemit_defer_register()`, and the shared unwind helpers.
   - Patch `fscope_end()`, `parse_return()`, `parse_break()`, and `parse_continue()` to emit `BC_UNDEFER`.

3. **`src/fluid/luajit-2.1/src/lj_bc.h`** (and generated tables)
   - Introduce `BC_DEFER` and `BC_UNDEFER` entries in `BCDEF`.
   - Regenerate opcode name tables and ensure the disassembler understands the new operands.

4. **Runtime core (`src/fluid/luajit-2.1/src/lj_vm*.{c,s}`, `src/fluid/luajit-2.1/src/lj_vm.h`, new `src/fluid/luajit-2.1/src/lj_defer.c`/`.h`)**
   - Execute the new opcodes, manage `DeferFrame` allocation, and integrate with the GC and error unwinder.

5. **Tooling & JIT (`src/fluid/luajit-2.1/src/lj_disasm.c`, `src/fluid/luajit-2.1/src/lj_bcwrite.c`, `src/fluid/luajit-2.1/src/lj_load.c`, `src/fluid/luajit-2.1/src/lj_ffrecord.c`)**
   - Update bytecode IO utilities and ensure the trace recorder either lowers or rejects the new instructions cleanly.

6. **Tests & Documentation**
   - Add `src/fluid/tests/test_defer.fluid` with the scenarios listed in the checklist.
   - Document the feature in `docs/wiki/Fluid-Reference-Manual.md` and any developer-facing notes if the JIT support is staged.

---
## Implementation Order

1. **Phase 1: Parser Foundations** (6-8 hours)
   - Steps 1-3: Token addition, scope metadata extensions, and flag initialisation.
   - Outcome: the parser can recognise `defer` and track scope-level bookkeeping without emitting bytecode yet.

2. **Phase 2: Statement Parsing & Registration** (6-8 hours)
   - Steps 4-5: Implement `parse_defer()` and emit `BC_DEFER` with captured arguments.
   - Outcome: compiled chunks contain the new bytecode and the runtime stack receives handlers, albeit without unwinding support.

3. **Phase 3: Runtime Execution Support** (10-12 hours)
   - Step 6: Introduce the new opcodes, `DeferFrame`, and interpreter/JIT scaffolding.
   - Outcome: the VM can store and execute handlers while maintaining GC correctness.

4. **Phase 4: Control-Flow Integration** (6-8 hours)
   - Steps 7-10: Emit unwind bytecode for scope exits, returns, breaks, and initialise per-function accounting.
   - Outcome: all exit paths run the appropriate handlers exactly once.

5. **Phase 5: Tooling, Testing, and Documentation** (6-8 hours)
   - Steps 11-15: Update disassembly/serialisation utilities, add the new test suite, and document the feature.
   - Outcome: the change-set is production ready with full coverage and user guidance.

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
