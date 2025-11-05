# Implementing `__close` Metamethod for To-Be-Closed Variables

## Overview

This document outlines what would be required to implement Lua 5.4's `<close>` attribute and `__close` metamethod in Fluid/LuaJIT. This feature provides deterministic resource cleanup similar to RAII in C++.

## Feature Description

The `<close>` attribute marks local variables for automatic cleanup when they go out of scope:

```lua
local f <close> = io.open("file.txt")
-- f is automatically closed via __close metamethod when scope ends
-- even if an error occurs
```

## Benefits for Parasol/Fluid

- **Deterministic resource cleanup**: Critical for file handles, network sockets, graphics resources
- **Error safety**: Resources are cleaned up even in error paths
- **Better than `__gc`**: Immediate cleanup instead of waiting for garbage collection
- **Developer experience**: Prevents resource leaks, reduces boilerplate

## Implementation Requirements

### 1. Metamethod Registration

**Location**: `src/fluid/luajit-2.1/src/lj_obj.h`

**Current metamethod definition** (line 556):
```c
#define MMDEF(_) \
  _(index) _(newindex) _(gc) _(mode) _(eq) _(len) \
  /* Only the above (fast) metamethods are negative cached (max. 8). */ \
  _(lt) _(le) _(concat) _(call) \
  /* The following must be in ORDER ARITH. */ \
  _(add) _(sub) _(mul) _(div) _(mod) _(pow) _(unm) \
  /* The following are used in the standard libraries. */ \
  _(metatable) _(tostring) MMDEF_FFI(_) MMDEF_PAIRS(_)
```

**Required change**: Add `_(close)` to the metamethod list (likely after `_(gc)` for logical grouping).

**Impact**: This automatically generates:
- `MM_close` enum value in the `MMS` enum
- Metamethod name string in global roots
- Fast metamethod lookup infrastructure

### 2. Variable Attribute Tracking

**Location**: `src/fluid/luajit-2.1/src/lj_lex.h`

**Current VarInfo structure** (line 46):
```c
typedef struct VarInfo {
  GCRef name;		/* Local variable name or goto/label name. */
  BCPos startpc;	/* First point where the local variable is active. */
  BCPos endpc;		/* First point where the local variable is dead. */
  uint8_t slot;		/* Variable slot. */
  uint8_t info;		/* Variable/goto/label info. */
} VarInfo;
```

**Required changes**:
1. Add new flags to the `info` field:
   ```c
   #define VSTACK_CLOSE  0x80  /* Variable has <close> attribute */
   ```
2. The existing `info` byte has bits available (currently uses: GOTO, LABEL, VAR_RW flags)

### 3. Parser Changes for `<close>` Syntax

**Location**: `src/fluid/luajit-2.1/src/lj_parse.c`

**Function**: `parse_local()` at line 2735

**Required changes**:
1. After parsing variable name (line 2755), check for `<` token
2. If found, parse attribute name and verify it's "close"
3. Set `VSTACK_CLOSE` flag in the variable's info field
4. Example parsing logic:
   ```c
   var_new(ls, nvars++, lex_str(ls));
   if (lex_opt(ls, '<')) {
     GCstr *attr = lex_str(ls);
     if (strcmp(strdata(attr), "close") == 0) {
       // Mark this variable for closing
       VarInfo *v = &ls->vstack[ls->vtop - 1];
       v->info |= VSTACK_CLOSE;
     } else {
       lj_lex_error(ls, 0, LJ_ERR_XSYNTAX);  // Unknown attribute
     }
     lex_check(ls, '>');
   }
   ```

### 4. Scope Exit Handling

**Location**: `src/fluid/luajit-2.1/src/lj_parse.c`

**Function**: `fscope_end()` at line 1459

**Current behavior**: Emits `BC_UCLO` (upvalue close) when needed

**Required changes**:
1. Before emitting `BC_UCLO`, scan variables going out of scope
2. For each variable with `VSTACK_CLOSE` flag:
   - Generate code to call the `__close` metamethod
   - Handle errors (emit warning on error, don't propagate)
3. Close variables in reverse order of declaration (LIFO)

**Pseudo-code**:
```c
static void fscope_end(FuncState *fs) {
  FuncScope *bl = fs->bl;
  LexState *ls = fs->ls;

  // Close to-be-closed variables before removing them
  for (BCReg i = fs->nactvar - 1; i >= bl->nactvar; i--) {
    VarInfo *v = &var_get(ls, fs, i);
    if (v->info & VSTACK_CLOSE) {
      // Emit code to call __close metamethod
      bcemit_close_var(fs, i);
    }
  }

  // ... existing scope end logic ...
}
```

### 5. Bytecode Emission for __close Calls

**Location**: New function in `src/fluid/luajit-2.1/src/lj_parse.c`

**Required**: Create `bcemit_close_var()` function

**Implementation approach**:
```c
static void bcemit_close_var(FuncState *fs, BCReg reg) {
  // Similar to how __gc is called in lj_gc.c:gc_call_finalizer()
  // 1. Check if variable's metatable has __close
  // 2. If yes, emit call to __close(variable, error_object_or_nil)
  // 3. Protect against errors (catch and emit warning)
  // 4. Continue scope exit even if __close fails
}
```

**Key considerations**:
- Must work at parse/compile time, not runtime
- May need new bytecode instruction or reuse existing call mechanism
- Error handling must be non-propagating (similar to `__gc` finalizers)

### 6. Runtime Support

**Location**: `src/fluid/luajit-2.1/src/lj_meta.c`

**Required**: Helper function for calling `__close`

**Implementation**:
```c
LJ_FUNC void lj_meta_close(lua_State *L, TValue *o, TValue *err) {
  // 1. Look up __close metamethod
  cTValue *mo = lj_meta_lookup(L, o, MM_close);
  if (mo) {
    // 2. Call __close(o, err)
    // 3. Catch errors and emit warning (don't propagate)
    // 4. Similar to gc_call_finalizer() but with error parameter
  }
}
```

### 7. Error Propagation

**Location**: Error handling paths in VM

**Required changes**:
- When unwinding stack due to error, call `__close` on marked variables
- Pass error object as second parameter to `__close`
- This is complex as it requires VM-level integration

### 8. Testing

**Location**: `src/fluid/tests/`

**Required tests**:
1. Basic close on normal scope exit
2. Close on early return
3. Close on error/exception
4. Multiple close variables in same scope (LIFO order)
5. Close in loops, conditionals, function calls
6. Error in `__close` doesn't prevent other closes
7. Close with nil/false values (should work)

## Implementation Complexity

### Estimated Effort: **Medium to High**

**Breakdown**:
- **Parser changes**: Low complexity (2-4 hours)
  - Add `<close>` syntax parsing
  - Track attribute in VarInfo

- **Scope handling**: Medium complexity (8-12 hours)
  - Modify `fscope_end()` to call close handlers
  - Emit bytecode for __close calls
  - Handle reverse order (LIFO)

- **Error handling integration**: High complexity (16-24 hours)
  - Integrate with VM error unwinding
  - Pass error objects to __close
  - Prevent error propagation from __close itself

- **Testing**: Medium complexity (8-12 hours)
  - Comprehensive test coverage
  - Edge cases and error paths

**Total estimate**: 34-52 hours of development time

## Challenges

1. **VM Integration**: LuaJIT's VM is complex; integrating scope-based cleanup with error unwinding is non-trivial

2. **Bytecode Generation**: May need new bytecode instruction or clever reuse of existing instructions

3. **Performance**: Must not significantly impact normal execution paths (variables without `<close>`)

4. **Error Semantics**: Lua 5.4 has specific rules about when/how errors in `__close` are handled

5. **JIT Compilation**: LuaJIT's trace compiler would need to understand `<close>` semantics

## Comparison with __gc

### Current `__gc` Implementation

**Advantages**:
- Already implemented and working
- Automatic, no special syntax needed
- Well-tested

**Disadvantages**:
- Non-deterministic timing (depends on GC cycles)
- Doesn't guarantee cleanup (objects might never be collected)
- No error context passed to finalizer
- Can't be used for critical resources

### Proposed `__close` Implementation

**Advantages**:
- Deterministic cleanup at scope exit
- Works even if GC never runs
- Error-safe (cleans up in error paths)
- Explicit intent (developer marks variables)
- Error object passed to handler

**Disadvantages**:
- Requires explicit syntax
- More complex implementation
- Slight performance overhead for marked variables

## Recommendation

**Priority**: Medium-High

**Reasoning**:
- Would significantly improve resource management in Fluid applications
- Particularly valuable for Parasol's file/network/graphics resources
- Industry-standard feature (Lua 5.4, Python context managers, C++ RAII)
- Implementation is feasible but requires careful VM integration

**Suggested Approach**:
1. Start with parser changes and basic scope-exit handling (no error integration)
2. Test with simple cases (normal scope exit, early return)
3. Add error unwinding integration in phase 2
4. Extensive testing and performance validation
5. Consider JIT compiler integration as phase 3 (or mark as NYI in traces)

## Alternative: Simpler Scope-Guard Pattern

If full `__close` implementation proves too complex, consider a simpler library-based approach:

```lua
function with_resource(resource, handler, func)
  local ok, result = pcall(func, resource)
  handler(resource, ok and nil or result)
  if not ok then error(result) end
  return result
end

-- Usage:
with_resource(io.open("file.txt"), function(f) f:close() end, function(f)
  -- use file
end)
```

This provides similar safety but with more verbose syntax and no compiler integration.

## References

- Lua 5.4 Reference Manual: https://www.lua.org/manual/5.4/manual.html#3.3.8
- Lua 5.4 to-be-closed variables: https://www.lua.org/manual/5.4/manual.html#2.5.7
- LuaJIT source code: `lj_parse.c`, `lj_gc.c`, `lj_meta.c`

---

**Last Updated**: 2025-10-30
**Author**: Analysis based on LuaJIT 2.1 source code inspection
