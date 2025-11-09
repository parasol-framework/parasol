# Implementing `defer` Keyword in Fluid

## Overview

This document outlines the implementation strategy for adding a `defer` keyword to Fluid that provides Go-like deferred function execution. The implementation leverages existing LuaJIT infrastructure for functions and closures to achieve simplicity and maintainability.

## Feature Description

The `defer` keyword allows developers to schedule function execution at scope exit, providing deterministic cleanup and resource management:

```lua
function example()
   defer()
      print('executes at the end')
   end
   print('Hello world')
   -- Output: "Hello world"
   --         "executes at the end"
end
```

### Key Semantic Requirements

Based on the test suite at `src/fluid/tests/test_defer.fluid`:

1. **Scope-based execution**: Deferred functions execute at the end of the scope (block, function, etc.) in which they are defined
2. **LIFO order**: Multiple defer statements execute in reverse order (last defer first)
3. **Guaranteed execution**: Deferred functions execute even if the scope exits early via `return`, `break`, `continue`, or error
4. **Closure capture**: Deferred functions capture upvalues from their enclosing scope (standard Lua closure semantics)
5. **Argument snapshot**: Arguments passed to defer capture values at defer registration time, not execution time
6. **Error handling**: Errors in deferred functions propagate but all deferred handlers still execute in order

## Design Philosophy: KISS Implementation

**Critical constraint**: The implementation MUST NOT add new bytecode instructions or modify `.dasc` files. This ensures:

- Minimal complexity and maintenance burden
- Compatibility with existing JIT infrastructure
- Leveraging proven, well-tested closure and scope management code
- No platform-specific assembly code changes

The implementation treats `defer()` as **syntactic sugar** that transforms into standard Lua constructs at parse time.

## Implementation Strategy

### Core Concept: defer() as Syntactic Sugar

The parser transforms this:

```lua
defer()
   print('cleanup')
end
```

Into this conceptual equivalent:

```lua
local __defer_1 = function()
   print('cleanup')
end
-- (marked with VSTACK_DEFER flag for scope-exit execution)
```

The key insight: we create a local variable holding an anonymous function, mark it with a special flag, and execute it when the variable goes out of scope.

## Findings from 2025-02-16 Investigation

The previous implementation attempt (see attached chat log) did not reach a working MVP. A closer inspection of the LuaJIT parser highlighted several concrete pitfalls that must be addressed explicitly in the plan before resuming work:

1. **Scope teardown order in `fscope_end()`** (`src/fluid/luajit-2.1/src/lj_parse.c`, lines ~1671-1695). The original attempt added defer execution *after* the existing `var_remove()` call. Because `var_remove()` shrinks both the register file and the variable metadata, the deferred closures were unreachable by the time execution was attempted. Any helper must therefore run **before** `var_remove()` adjusts `fs->nactvar`/`fs->freereg`.
2. **Implicit returns in `fs_fixup_ret()`** (`src/fluid/luajit-2.1/src/lj_parse.c`, lines ~1913-1947). Functions that end without an explicit `return` emit a trailing `BC_RET0` *after* all scopes have already been closed. Without a hook here, top-level defers never fire. We need a single helper that both `parse_return()` and `fs_fixup_ret()` can call before emitting return bytecode.
3. **Loop semantics and `continue` handling** (`parse_break()`/`parse_continue()` around lines ~3333-3405 and the loop lowering paths around lines ~3450-3600). The failed attempt tried to execute every defer when encountering `continue`, which incorrectly drained outer-scope handlers and broke subsequent iterations. The parser must differentiate between the current loop scope (`FuncScope` with `FSCOPE_LOOP`) and surrounding scopes so that only the innermost active defers execute for `continue`/`break`.
4. **Register reuse across iterations**. Hidden locals for defers reuse the same register slot each time a defer statement is encountered in a loop body. The investigation confirmed that this is safe as long as the helper executes defers before `var_remove()` in every scope exit (including loop bodies). No additional runtime list structure is required once ordering is corrected.
5. **Test suite location**. The plan referenced `src/fluid/tests/test_defer.fluid`, but this file is currently absent from the repository. Phase 1 should provision a minimal smoke test (e.g., temporary Flute script under `src/fluid/tests/`) or update the plan to create it alongside the implementation.

These findings are reflected in the revised breakdown below.

## Detailed Implementation Steps

### 1. Add `defer` Keyword Token

**Location**: `src/fluid/luajit-2.1/src/lj_lex.h`

**Current token definition** (line 14-25):
```c
#define TKDEF(_, __) \
  _(and) _(break) _(continue) _(do) _(else) _(elseif) _(end) _(false) \
  _(for) _(function) _(goto) _(if) _(in) _(is) _(local) _(nil) _(not) _(or) \
  _(repeat) _(return) _(then) _(true) _(until) _(while) \
  ...
```

**Required change**: Add `_(defer)` to the keyword list:

```c
#define TKDEF(_, __) \
  _(and) _(break) _(continue) _(defer) _(do) _(else) _(elseif) _(end) _(false) \
  _(for) _(function) _(goto) _(if) _(in) _(is) _(local) _(nil) _(not) _(or) \
  _(repeat) _(return) _(then) _(true) _(until) _(while) \
  ...
```

This automatically generates `TK_defer` token in the lexer.

**File to modify**: `lj_lex.h`
**Impact**: Minimal - adds one token to enum

### 2. Add Variable Flag for Deferred Functions

**Location**: `src/fluid/luajit-2.1/src/lj_parse.c`

**Current VarInfo flags** (line 124-127):
```c
/* Variable/goto/label info. */
#define VSTACK_VAR_RW    0x01  /* R/W variable. */
#define VSTACK_GOTO      0x02  /* Pending goto. */
#define VSTACK_LABEL     0x04  /* Label. */
```

**Required change**: Add new flag for deferred variables:

```c
/* Variable/goto/label info. */
#define VSTACK_VAR_RW    0x01  /* R/W variable. */
#define VSTACK_GOTO      0x02  /* Pending goto. */
#define VSTACK_LABEL     0x04  /* Label. */
#define VSTACK_DEFER     0x08  /* Deferred function for scope-exit execution. */
```

The `info` field in `VarInfo` is `uint8_t`, so we have plenty of bits available (currently using only 3).

**File to modify**: `lj_parse.c`
**Impact**: Minimal - adds one bit flag definition

### 3. Parse `defer()` Syntax

**Location**: `src/fluid/luajit-2.1/src/lj_parse.c`

**Current statement parsing**: The `parse_stmt()` function (around line 3100-3200) handles all statement types.

**Required changes**:

Add case for `TK_defer` in the statement parser switch:

```c
static void parse_stmt(LexState *ls)
{
  BCLine line = ls->linenumber;
  switch (ls->tok) {
    // ... existing cases ...
    case TK_defer: parse_defer(ls); break;
    // ... rest of cases ...
  }
}
```

**New function**: `parse_defer()`

```c
/* Parse 'defer' statement with optional arguments */
static void parse_defer(LexState *ls)
{
   FuncState *fs = ls->fs;
   ExpDesc v, b;
   BCReg base;
   BCLine line = ls->linenumber;
   int nargs = 0;

   lj_lex_next(ls);  /* Skip 'defer'. */
   lex_check(ls, '(');

   /* Parse optional parameter list */
   if (ls->tok != ')') {
      do {
         expr_tonextreg(fs, &v);
         nargs++;
      } while (lex_opt(ls, ','));
   }

   lex_check(ls, ')');

   /* Create hidden local variable to hold the deferred function */
   GCstr *varname = lj_str_newlit(ls->L, "__defer");  /* Internal name */
   BCReg reg = fs->freereg;
   var_new(ls, 0, varname);  /* Create hidden local */

   /* Parse the function body */
   parse_body(ls, &b, nargs, line);

   /* Assign function to hidden variable */
   expr_init(&v, VLOCAL, reg);
   v.u.s.aux = fs->vbase + fs->nactvar - 1;
   var_new_fixed(fs, 1);

   /* Mark this variable as deferred for scope-exit execution */
   VarInfo *vi = &ls->vstack[fs->vbase + fs->nactvar - 1];
   vi->info |= VSTACK_DEFER;

   /* Store arguments snapshot if provided (for testArgumentSnapshot) */
   if (nargs > 0) {
      /* Arguments are already on stack from expr_tonextreg calls */
      /* The closure will capture them as upvalues */
   }
}
```

**Alternative simpler approach** (if defer takes no arguments):

```c
/* Parse 'defer' statement - creates deferred anonymous function */
static void parse_defer(LexState *ls)
{
   FuncState *fs = ls->fs;
   ExpDesc func;
   BCLine line = ls->linenumber;

   lj_lex_next(ls);  /* Skip 'defer'. */
   lex_check(ls, '(');
   lex_check(ls, ')');

   /* Create anonymous local variable */
   BCReg reg = fs->freereg;
   var_new(ls, 0, lj_str_newlit(ls->L, ""));  /* Empty name = hidden */

   /* Parse function body (zero parameters) */
   parse_body(ls, &func, 0, line);

   /* Assign to local */
   expr_toreg(fs, &func, reg);
   var_new_fixed(fs, 1);
   bcreg_reserve(fs, 1);

   /* Mark as deferred */
   VarInfo *vi = &ls->vstack[fs->vbase + fs->nactvar - 1];
   vi->info |= VSTACK_DEFER;
}
```

**File to modify**: `lj_parse.c`
**Impact**: Medium - new parser function, integrates with existing function parsing

### 4. Execute Deferred Functions at Scope Exit

**Location**: `src/fluid/luajit-2.1/src/lj_parse.c`

**Current scope handling**: The `fscope_end()` function (line 1671-1693) handles scope cleanup.

**Required changes**: Modify `fscope_end()` to execute deferred functions before removing variables:

```c
/* End a scope. */
static void fscope_end(FuncState *fs)
{
   FuncScope *bl = fs->bl;
   LexState *ls = fs->ls;

   fs->bl = bl->prev;

   /* Execute deferred functions in LIFO order (reverse declaration order) */
   /* Scan variables going out of scope from last to first */
   for (BCReg i = fs->nactvar; i > bl->nactvar; ) {
      --i;
      VarInfo *v = &var_get(ls, fs, i);
      if (v->info & VSTACK_DEFER) {
         /* Emit call to the deferred function (zero arguments) */
         /* The function is in register i (variable slot) */
         bcemit_ABC(fs, BC_CALL, i, 1, 1);  /* CALL reg, 1 arg (func), 1 ret */
      }
   }

   /* Standard variable cleanup */
   var_remove(ls, bl->nactvar);
   fs->freereg = fs->nactvar;
   lj_assertFS(bl->nactvar IS fs->nactvar, "bad regalloc");

   /* Handle upvalue closing if needed */
   if ((bl->flags & (FSCOPE_UPVAL|FSCOPE_NOCLOSE)) IS FSCOPE_UPVAL)
      bcemit_AJ(fs, BC_UCLO, bl->nactvar, 0);

   /* Handle break labels */
   if ((bl->flags & FSCOPE_BREAK)) {
      if ((bl->flags & FSCOPE_LOOP)) {
         MSize idx = gola_new(ls, NAME_BREAK, VSTACK_LABEL, fs->pc);
         ls->vtop = idx;
         gola_resolve(ls, bl, idx);
      } else {
         gola_fixup(ls, bl);
         return;
      }
   }
   gola_close(ls, bl);
}
```

**Key considerations**:

1. **LIFO execution**: Loop from `fs->nactvar - 1` down to `bl->nactvar` to ensure last-declared defer executes first
2. **Call emission**: Use `BC_CALL` bytecode with register holding the function
3. **Timing**: Execute defers BEFORE `var_remove()` so variables are still in scope
4. **No new bytecode**: Reuses existing `BC_CALL` instruction

**Alternative for handling arguments**:

If defer accepts arguments (like `defer(arg) ... end(value)`), the arguments need to be stored when defer is created, not when it executes. This requires:

1. Creating closure with captured argument values as upvalues
2. Or storing arguments in hidden locals alongside the function

The simpler approach (no arguments) matches Go's defer more closely and avoids complexity.

**File to modify**: `lj_parse.c`
**Impact**: Medium - modifies critical scope exit path

### 5. Handle Early Exit Paths

**Locations**: Various places in `src/fluid/luajit-2.1/src/lj_parse.c`

**Required changes**: Ensure deferred functions execute on all exit paths:

#### A. `return` statements

**Current**: `parse_return()` function (around line 3155)

**Required**: Before emitting return, trigger defer execution by ending current block scope:

```c
static void parse_return(LexState *ls)
{
   // ... existing parsing ...

   /* Execute deferred functions before return */
   /* Create temporary scope end to trigger defers */
   FuncScope temp_scope;
   temp_scope.nactvar = fs->bl->nactvar;
   temp_scope.flags = FSCOPE_NOCLOSE;  /* Don't emit UCLO */
   temp_scope.prev = fs->bl;
   temp_scope.vstart = fs->bl->vstart;

   /* Emit defer calls manually */
   for (BCReg i = fs->nactvar; i > fs->bl->nactvar; ) {
      --i;
      VarInfo *v = &var_get(ls, fs, i);
      if (v->info & VSTACK_DEFER) {
         bcemit_ABC(fs, BC_CALL, i, 1, 1);
      }
   }

   // ... existing return emission ...
}
```

**Better approach**: Factor out defer execution into helper function:

```c
/* Execute all deferred functions in current scope */
static void execute_defers(FuncState *fs)
{
   LexState *ls = fs->ls;
   FuncScope *bl = fs->bl;

   /* LIFO order: scan from last variable to first in current scope */
   for (BCReg i = fs->nactvar; i > bl->nactvar; ) {
      --i;
      VarInfo *v = &var_get(ls, fs, i);
      if (v->info & VSTACK_DEFER) {
         bcemit_ABC(fs, BC_CALL, i, 1, 1);
      }
   }
}
```

Then call `execute_defers(fs)` from:
- `fscope_end()` (normal scope exit)
- `parse_return()` (early return)
- `parse_break()` (loop break)
- `parse_continue()` (loop continue - note: continue should only execute loop-scope defers)

#### B. `break` statements

**Current**: `parse_break()` function (around line 3070)

**Required**: Execute defers for current loop scope before breaking:

```c
static void parse_break(LexState *ls)
{
   // ... existing code ...
   execute_defers(ls->fs);  /* Run defers before break */
   // ... emit break jump ...
}
```

#### C. `continue` statements

**Current**: `parse_continue()` function (similar to parse_break)

**Required**: Execute defers but only for variables declared within the loop iteration:

This is more complex - need to track which scope level corresponds to the loop body vs outer scopes.

```c
static void parse_continue(LexState *ls)
{
   // ... existing code ...

   /* Execute defers only for variables in innermost scope */
   /* More complex - need to identify loop body scope */
   FuncScope *loop_scope = find_loop_scope(ls->fs);

   for (BCReg i = fs->nactvar; i > loop_scope->nactvar; ) {
      --i;
      VarInfo *v = &var_get(ls, fs, i);
      if (v->info & VSTACK_DEFER) {
         bcemit_ABC(fs, BC_CALL, i, 1, 1);
      }
   }

   // ... emit continue jump ...
}
```

**File to modify**: `lj_parse.c`
**Impact**: Medium - multiple exit paths need modification

### 6. Handle Nested Scopes

**Location**: Existing `fscope_begin()` and `fscope_end()` infrastructure

**Required changes**: None - the implementation naturally handles nested scopes because:

1. Each scope has its own `FuncScope` with `nactvar` tracking
2. `fscope_end()` only processes variables in current scope level
3. Nested scopes create nested `FuncScope` structures
4. LIFO execution order is maintained within each scope level

**Test case** (`testNestedScopes` from test_defer.fluid):

```lua
function subject()
   defer()
      table.insert(order, 'outer')
   end

   do
      defer()
         table.insert(order, 'inner')
      end
      table.insert(order, 'inner-body')
   end

   table.insert(order, 'after-inner')
end

-- Expected: 'inner-body,inner,after-inner,outer'
```

This works automatically because:
- Inner `do...end` block has its own scope
- `fscope_end()` called when exiting `do` block executes inner defer
- `fscope_end()` called when exiting function executes outer defer

**File to modify**: None
**Impact**: Zero - existing infrastructure handles this

### 7. Error Handling and Propagation

**Location**: VM error handling paths and `lj_err.c`

**Requirement**: Deferred functions must execute even when errors occur, similar to Go's defer behavior.

**Current state**: LuaJIT error unwinding uses `lj_err_throw()` which unwinds the stack.

**Challenge**: This is the most complex part and would require VM modifications.

**Pragmatic approach for initial implementation**:

1. **Phase 1 (MVP)**: Defers execute on normal exit paths only (return, break, continue, scope end)
2. **Phase 2**: Add error path handling if critical

**If Phase 2 is required**:

Would need to modify error unwinding to:
1. Walk the call stack during unwinding
2. Identify variables marked with `VSTACK_DEFER`
3. Call those functions before continuing unwind
4. Protect against errors in defer handlers (use pcall internally)

This is complex and may violate the KISS principle. Recommend deferring (pun intended) until proven necessary.

**File to modify**: Potentially `lj_err.c` (Phase 2 only)
**Impact**: High complexity - recommend MVP without error path support initially

### 8. Argument Snapshot Support (Optional)

**Requirement**: Support `defer(arg) ... end(value)` syntax for capturing arguments at defer time.

**Test case** (`testArgumentSnapshot`):

```lua
local x = 5
defer(arg)
   table.insert(snapshots, arg)
end(x)
x = 9
-- Expected: defer executes with arg = 5, not 9
```

**Implementation approach**:

Modify `parse_defer()` to:

1. Parse parameter list in `defer(param1, param2)`
2. Parse argument list after `end(arg1, arg2)`
3. Create closure with arguments as upvalues (captured at defer creation time)

**Pseudo-code**:

```c
static void parse_defer(LexState *ls)
{
   // Parse defer(params)
   int nparams = parse_defer_params(ls);

   // Parse function body
   parse_defer_body(ls, nparams);

   // Parse end(args)
   if (lex_opt(ls, '(')) {
      /* Parse argument expressions */
      for (int i = 0; i < nparams; i++) {
         expr_tonextreg(fs, &e);  /* Evaluate argument */
      }
      lex_check(ls, ')');

      /* Create closure capturing arguments as upvalues */
      /* This requires modifying parse_body to accept initial upvalues */
   }
}
```

**Complexity**: High - requires significant parser modifications.

**Recommendation**: Start with parameter-less defer, add argument support in Phase 2 if needed.

**File to modify**: `lj_parse.c` (parse_defer function)
**Impact**: High if implemented

### 9. Testing Infrastructure

**Location**: `src/fluid/tests/test_defer.fluid`

**Status**: Comprehensive test suite already exists with 15 test cases covering:

- Basic execution order
- LIFO ordering
- Early returns
- Nested scopes
- Break/continue in loops
- Upvalue capture
- Argument snapshot
- Conditional scopes
- Multiple defers in loops
- Error handling
- Recursive functions
- Closures
- Resource cleanup patterns

**Required**: Ensure all tests pass after implementation.

**Additional test considerations**:

1. Performance regression tests
2. JIT compiler compatibility (may need to disable JIT for defer initially)
3. Edge cases like defer in tail-call positions

**File**: To be added at `src/fluid/tests/test_defer.fluid` during Phase 1.6
**Impact**: Medium - new automated test harness required

## Implementation Phases

### Phase Progression Summary

| Phase | Tests Passing | Key Features | Go Ahead Criteria |
|-------|--------------|--------------|-------------------|
| **Phase 1** | 13/13 core | Basic defer, LIFO order, early exits, paren-less syntax | All 13 core tests pass |
| **Phase 2** | 15/15 active | + Argument snapshot support, resource cleanup patterns | All 15 active tests pass |
| **Phase 3** | 16/16 total | + Error path handling | All 16 total tests pass |

**CRITICAL**: Each phase MUST achieve its test passing criteria before proceeding to the next phase. This ensures stability and prevents regressions.

### Phase 1: MVP (Minimal Viable Product)

**Goal**: Basic defer functionality without argument support or specialised error recovery.

**Progression**: Phase 1 is split into six incremental sub-phases so that each behavioural milestone can be validated independently.

#### Phase 1.1 – Lexical and flag scaffolding
- Extend `src/fluid/luajit-2.1/src/lj_lex.h` with `_(defer)`.
- Introduce `VSTACK_DEFER` beside the existing `VSTACK_*` bits in `lj_parse.c`.
- Acceptance: repository builds successfully; lexer recognises `defer` (sanity check using a minimal Fluid script that only tokenises the keyword).

#### Phase 1.2 – Parser recognition for `defer`
- Add a `parse_defer()` prototype near the other statement helpers.
- Hook `parse_stmt()` to dispatch on `TK_defer`.
- Inside `parse_defer()` create a hidden local (name can reuse `NAME_BLANK`) that stores the anonymous closure produced by `parse_body()`; mark its `VarInfo` with `VSTACK_DEFER` and ensure `fs->freereg` advances exactly once.
- Acceptance: AST/bytecode emission should succeed for a script containing a defer with an empty body; no execution yet.

#### Phase 1.3 – Scope-exit execution helper
- Implement `execute_defers(FuncState *fs, BCReg limit)` (or similar) that scans locals from `fs->nactvar-1` down to `limit`, emitting `BC_CALL` for each `VSTACK_DEFER` entry.
- Call this helper **before** `var_remove()` inside `fscope_end()` and adjust `fs->freereg` so that the helper does not clobber live registers.
- Update `fs_fixup_ret()` to run the helper before emitting implicit `BC_RET0` so that bare returns honour defer handlers.
- Acceptance: manually inspect generated bytecode for a simple function containing a defer and confirm a `BC_CALL` precedes the return slot; run a headless script to ensure scope-exit calls trigger on implicit returns.

#### Phase 1.4 – Explicit `return`
- Invoke the helper from `parse_return()` immediately before generating any `BC_RET*` instruction.
- Ensure tail-call optimisation paths still work (verify `BC_CALLT`/`BC_RETM` emission); update helper signature if it needs to temporarily park return registers.
- Acceptance: Fluid snippet with an early `return` inside a function must execute defers once before returning control.

#### Phase 1.5 – Loop control flow (`break`/`continue`)
- Teach the helper to limit execution to the active loop scope: locate the innermost `FuncScope` flagged with `FSCOPE_LOOP` and use its `nactvar` as the lower bound when responding to `continue`.
- Modify `parse_break()` to drain all defers from the current `FuncScope` chain up to (but excluding) the surrounding loop’s outer variables; modify `parse_continue()` to stop at the loop scope.
- Ensure `fscope_loop_continue()` remains a no-op until after defer execution so jump targets stay intact.
- Acceptance: create Fluid snippets covering `break` and `continue` in loops; confirm LIFO ordering and per-iteration execution (e.g., use print statements during manual testing).

#### Phase 1.6 – Minimal regression test harness
- Author `src/fluid/tests/test_defer.fluid` (or a new Flute-based harness if naming conflicts exist) implementing at least the ten Phase 1 scenarios listed below.
- Wire the test into the existing Fluid test runner (update `CMakeLists.txt` or Flute manifest if necessary).
- Acceptance: `parasol --log-warning --gfx-driver=headless` run of the new test passes for sub-phases 1.1–1.5; document the command in this plan for future runs.

**Completion criteria**: After Phase 1.6, the following behavioural checks must pass (automated once the new test exists):
1. ✓ `testBasicExecution`
2. ✓ `testLifoOrder`
3. ✓ `testEarlyReturn`
4. ✓ `testNestedScopes`
5. ✓ `testBreak`
6. ✓ `testContinue`
7. ✓ `testUpvalues`
8. ✓ `testConditionalScopes`
9. ✓ `testLoopMultipleDefers`
10. ✓ `testRecursive`
11. ✓ `testClosureReturn`

**Expected failures** while Phase 1 is in progress:
- ✗ `testArgumentSnapshot`
- ✗ `testResourceCleanupPattern`
- ✗ `testHandlerError`

**Validation command** (to be finalised in Phase 1.6):
```bash
cmake --build build/agents --config Release --parallel
cmake --install build/agents --config Release
build/agents-install/parasol --log-warning --gfx-driver=headless tools/flute.fluid file=src/fluid/tests/test_defer.fluid
```

> Note: Replace the final command with the precise path once the new test harness lands.

**Do NOT proceed to Phase 2 until all 13 Phase 1 tests pass.**

#### Phase 1 delivery report (2025-02-17)

- ✅ Lexer recognises `defer` via `lj_lex.h` and parser dispatch in `parse_stmt()`.
- ✅ `parse_defer()` now creates a hidden local marked with `VSTACK_DEFER`, lowers the anonymous function body via `parse_body()`, and leaves the closure in the scope’s register file.
- ✅ `execute_defers()` walks pending handlers at scope boundaries, snapshots each closure into a scratch register before issuing `BC_CALL`, and is invoked from `fscope_end()`, implicit returns (`fs_fixup_ret()`), explicit `return`, `break`, and `continue` sites. A lightweight `snapshot_return_regs()` helper preserves return values prior to invoking defers.
- ✅ Loop control statements resolve the active loop scope before draining handlers so `continue` only flushes the current iteration while `break` drains the entire loop scope.
- ✅ Added `src/fluid/tests/test_defer.fluid` with thirteen Phase 1 scenarios, including coverage for paren-less `defer` syntax. Phase 2/3 expectations remain documented as placeholder functions and are intentionally excluded from the exported `tests` list until the corresponding semantics land.
- ✅ Command to validate Phase 1: `build/agents-install/parasol --log-warning --gfx-driver=headless tools/flute.fluid file=src/fluid/tests/test_defer.fluid` (passes 13/13 cases after the 2025-02-17 build).

#### Phase 2 delivery report (2025-11-08)

- ✅ `parse_defer()` now snapshots optional argument expressions into hidden locals flagged with `VSTACK_DEFERARG`, ensuring values are captured at registration time.
- ✅ `execute_defers()` drains both handlers and their argument payloads in LIFO order and emits `BC_CALL` with the correct arity while staging calls safely above live locals.
- ✅ Re-enabled `testArgumentSnapshot` and `testResourceCleanupPattern`, and added `testMultiReturnPreservesLocals` plus `testDeferWithoutParens` to guard both multi-value returns and syntax ergonomics; the Flute suite now reports 15/15 passing scenarios with `supportsErrorPropagation` still gating the Phase 3 error test.
- ✅ Validation command: `build/agents-install/parasol --log-warning --gfx-driver=headless tools/flute.fluid file=src/fluid/tests/test_defer.fluid` (passes 15/15 active cases on 2025-11-08).

### Phase 2: Argument Snapshot Support

**Goal**: Support `defer(arg) ... end(value)` syntax.

**Scope**:
- Extend `parse_defer()` to handle parameters
- Create closures with argument upvalues
- Modify function body parsing to accept pre-captured values

**Estimated effort**: 6-10 hours

**Completion criteria**: **15 of 16 tests passing** (all Phase 1 tests + 2 new tests)

The following ADDITIONAL tests must pass before Phase 3:
1. ✓ `testArgumentSnapshot` - Argument passing: defer(arg)...end(value)
2. ✓ `testResourceCleanupPattern` - Practical: resource cleanup with arguments

**All Phase 1 tests must still pass** (regression check).

**Expected failures** (Phase 3 features):
- ✗ `testHandlerError` - Requires error path handling

**Validation command**:
```bash
cd src/fluid/tests && ../../../build/agents-install/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/fluid/tests/test_defer.fluid --gfx-driver=headless
```

**Do NOT proceed to Phase 3 until all 15 tests pass.**

### Phase 3: Error Path Handling (Optional)

**Goal**: Execute defers during error unwinding.

**Scope**:
- Modify `lj_err_throw()` to check for deferred functions
- Walk stack during unwind
- Execute defers with error object as context
- Protect against errors in defer handlers

**Estimated effort**: 16-24 hours

**Completion criteria**: **16 of 16 tests passing** (100% test coverage)

The following FINAL test must pass:
1. ✓ `testHandlerError` - Error handling: defers execute during errors

**All previous tests must still pass** (full regression check).

**Validation command**:
```bash
cd src/fluid/tests && ../../../build/agents-install/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/fluid/tests/test_defer.fluid --gfx-driver=headless
```

**Phase 3 is OPTIONAL.** Consider implementing only if:
- User demand justifies the complexity
- Error-path defer execution is critical for resource safety
- Time budget allows for VM-level integration work

**Recommendation**: Defer Phase 3 (pun intended again) until proven necessary. Many use cases don't require error path execution. Phase 1 and 2 provide over 93% of Go's defer functionality (15 of 16 tests).

### Running Tests for Specific Phases

The test file `src/fluid/tests/test_defer.fluid` is organized with comments indicating which tests belong to each phase. To run tests incrementally during development:

**Run all tests** (shows which phase you're at based on pass/fail counts):
```bash
cd src/fluid/tests && ../../../build/agents-install/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/fluid/tests/test_defer.fluid --gfx-driver=headless
```

**Interpreting results**:
- **13 passing**: Phase 1 complete, ready for Phase 2
- **15 passing**: Phase 2 complete, ready for Phase 3 (or ship!)
- **16 passing**: Phase 3 complete (full implementation)

**Individual test debugging** (if needed):

To debug a specific test, you can temporarily modify the test file to return only that test:
```lua
return {
   tests = {
      'testBasicExecution'  -- Debug single test
   }
}
```

Or use Flute's test filtering if available.

## Technical Considerations

### 1. Bytecode Efficiency

**Concern**: Each defer adds a local variable and function call overhead.

**Mitigation**:
- Hidden locals don't consume register slots unnecessarily
- Function calls use fast path BC_CALL
- JIT compiler can optimize trivial defers

**Measurement**: Add performance tests comparing defer vs manual cleanup.

### 2. JIT Compiler Compatibility

**Concern**: LuaJIT trace compiler may not understand defer semantics.

**Initial approach**: Mark deferred functions as NYI (Not Yet Implemented) in traces.

**Location**: May need to add NYI check in `lj_record.c` for functions marked as deferred.

**Long-term**: JIT compiler could inline trivial defers or optimize them away.

### 3. Upvalue Handling

**Concern**: Deferred functions capture upvalues - need to ensure correct closure semantics.

**Solution**: Leverage existing closure implementation - no special handling needed.

**Test**: `testUpvalues` verifies this works correctly.

### 4. Register Allocation

**Concern**: Hidden locals may interfere with register allocation.

**Solution**:
- Use standard `var_new()` mechanism which handles register allocation
- Empty-string variable names indicate hidden locals
- Existing register allocator handles this correctly

### 5. Scope Flags Interaction

**Concern**: `VSTACK_DEFER` flag may interact with other flags (VAR_RW, GOTO, LABEL).

**Solution**: Flags are independent bits - no conflicts expected.

**Verification**: Check all flag usage to ensure no assumptions about mutual exclusivity.

## Files to Modify

Summary of all file changes:

1. **`src/fluid/luajit-2.1/src/lj_lex.h`**
   - Add `_(defer)` to TKDEF macro
   - Impact: 1 line change

2. **`src/fluid/luajit-2.1/src/lj_parse.c`**
   - Add `#define VSTACK_DEFER 0x08`
   - Add `parse_defer()` function (~40 lines)
   - Add `execute_defers()` helper (~15 lines)
   - Modify `fscope_end()` to call execute_defers (~5 lines)
   - Modify `parse_return()` to call execute_defers (~3 lines)
   - Modify `parse_break()` to call execute_defers (~3 lines)
   - Modify `parse_continue()` to call execute_defers (~10 lines)
   - Add case in `parse_stmt()` switch (~1 line)
   - Impact: ~80 lines of new code, ~20 lines modified

3. **`src/fluid/tests/test_defer.fluid`**
   - Already exists with comprehensive tests
   - Impact: Zero - no changes needed

**Total code changes**: ~100 lines

## Advantages of This Approach

1. **KISS principle**: Leverages existing function/closure infrastructure
2. **No new bytecode**: Reuses BC_CALL - no .dasc file modifications
3. **Maintainable**: All logic in parser, easy to understand and debug
4. **Type safe**: Uses standard Lua function objects
5. **JIT friendly**: Can be optimized by existing JIT infrastructure
6. **Testable**: Comprehensive test suite already exists
7. **Minimal risk**: Changes isolated to parser, doesn't touch VM core

## Disadvantages and Tradeoffs

1. **Not pure Go semantics**: Error path handling requires Phase 3
2. **Hidden locals**: Consumes local variable slots (but LuaJIT has 200+ locals available)
3. **Runtime overhead**: Function call for each defer (but minimal - single BC_CALL)
4. **No JIT initially**: May need NYI marking until JIT support added

## Alternative Approaches Considered

### Alternative 1: New Bytecode Instruction

**Approach**: Add `BC_DEFER` instruction to register deferred functions.

**Rejected because**:
- Violates KISS principle
- Requires .dasc file modifications (explicitly forbidden)
- More complex VM integration
- Higher maintenance burden

### Alternative 2: Metamethod-Based (like `__close`)

**Approach**: Use `__defer` metamethod on special objects.

**Rejected because**:
- Requires metamethod infrastructure
- Less ergonomic syntax
- Doesn't match Go's defer semantics
- More runtime overhead

### Alternative 3: Macro/Preprocessing

**Approach**: Transform defer syntax before parsing.

**Rejected because**:
- Adds preprocessing step
- Harder to debug
- Loses source line information
- Less integrated with language

## Success Metrics

After implementation, success is measured by:

1. **Test coverage**: All 15 active tests in test_defer.fluid pass (Phase 1: 13 core tests, Phase 2: all 15 active cases)
2. **Performance**: < 5% overhead vs manual cleanup in benchmarks
3. **Code quality**: No new compiler warnings, passes static analysis
4. **Compilation**: Builds successfully on Windows/Linux/macOS
5. **Documentation**: Implementation documented in Fluid wiki

## Documentation Updates Required

After implementation:

1. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add defer keyword documentation
   - Include syntax examples
   - Explain LIFO execution order
   - Document limitations (error paths in Phase 1)

2. **`docs/wiki/Fluid-Common-API.md`**
   - Add resource cleanup patterns using defer
   - Show examples with files, sockets, graphics resources

3. **`src/fluid/tests/test_defer.fluid`**
   - Already comprehensive - no changes needed

4. **`readme.md`**
   - Add defer to feature list

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Register allocation conflicts | Low | Medium | Use hidden local mechanism (well-tested) |
| JIT incompatibility | Medium | Low | Mark NYI initially, optimize later |
| Performance regression | Low | Medium | Add benchmarks, optimize if needed |
| Scope tracking bugs | Medium | High | Extensive testing with nested scopes |
| Break/continue edge cases | Medium | Medium | Test all loop types thoroughly |
| Error path complexity | High | Medium | Defer to Phase 3, document limitation |

## Conclusion

This implementation plan provides a pragmatic, KISS-compliant approach to adding Go-style defer to Fluid. By leveraging existing function and closure infrastructure, we avoid complex VM modifications while delivering valuable resource management capabilities.

The phased approach allows delivering basic functionality quickly (Phase 1) while keeping options open for advanced features (Phase 2-3) if user demand justifies the additional complexity.

**Recommended action**: Implement Phase 1 MVP, validate with test suite, gather user feedback, then decide on Phase 2/3 based on actual usage patterns.

---

**Last Updated**: 2025-02-17
**Author**: Strategic analysis based on LuaJIT 2.1 architecture and test_defer.fluid requirements
**Status**: Phase 2 complete (argument snapshots enabled; 15/15 active regression tests passing, including paren-less defer syntax, while error unwinding remains deferred)
