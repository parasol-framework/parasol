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

## Current Parser Architecture

The Fluid parser has been modernised to C++20 with an AST-based pipeline:

1. **Lexer** (`src/fluid/luajit-2.1/src/parser/lexer.cpp`) - Tokenisation
2. **AstBuilder** (`src/fluid/luajit-2.1/src/parser/ast_builder.cpp`) - Builds AST from tokens
3. **IrEmitter** (`src/fluid/luajit-2.1/src/parser/ir_emitter.cpp`) - Transforms AST to bytecode

Key files:
- `parse_types.h` - Type definitions, enums, flags
- `parse_scope.cpp` - Scope management, variable tracking, defer execution
- `lexer.h` - VarInfo structure, token definitions
- `ast_nodes.h` - AST node definitions

## Implementation Requirements

### 1. Metamethod Registration

**Location**: `src/fluid/luajit-2.1/src/runtime/lj_obj.h`

**Current metamethod definition** (line 553):
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

**Required change**: Add `_(close)` to the metamethod list (after `_(gc)` for logical grouping).

**Impact**: This automatically generates:
- `MM_close` enum value in the `MMS` enum
- Metamethod name string in global roots
- Fast metamethod lookup infrastructure

### 2. Variable Attribute Tracking

**Location**: `src/fluid/luajit-2.1/src/parser/lexer.h`

**Current VarInfo structure** (line 196):
```cpp
typedef struct VarInfo {
   GCRef name;        //  Local variable name.
   BCPOS startpc;     //  First point where the local variable is active.
   BCPOS endpc;       //  First point where the local variable is dead.
   uint8_t slot;      //  Variable slot.
   VarInfoFlag info;  //  Variable info flags.
} VarInfo;
```

**Location**: `src/fluid/luajit-2.1/src/parser/parse_types.h`

**Current VarInfoFlag enum** (line 61):
```cpp
enum class VarInfoFlag : uint8_t {
   None = 0x00u,
   VarReadWrite = 0x01u,
   Jump = 0x02u,
   JumpTarget = 0x04u,
   Defer = 0x08u,
   DeferArg = 0x10u
};
```

**Required change**: Add new flag:
```cpp
enum class VarInfoFlag : uint8_t {
   None = 0x00u,
   VarReadWrite = 0x01u,
   Jump = 0x02u,
   JumpTarget = 0x04u,
   Defer = 0x08u,
   DeferArg = 0x10u,
   Close = 0x20u       // Variable has <close> attribute
};
```

### 3. Lexer Changes for `<close>` Token

**Location**: `src/fluid/luajit-2.1/src/parser/lexer.h`

The lexer already handles `<` and `>` as individual tokens. No new token types are needed since `<close>` is parsed as `<` followed by identifier `close` followed by `>`.

### 4. AST Node Changes

**Location**: `src/fluid/luajit-2.1/src/parser/ast_nodes.h`

**Current Identifier structure** (line 179):
```cpp
struct Identifier {
   GCstr* symbol = nullptr;
   SourceSpan span{};
   bool is_blank = false;
};
```

**Required change**: Extend to track close attribute:
```cpp
struct Identifier {
   GCstr* symbol = nullptr;
   SourceSpan span{};
   bool is_blank = false;
   bool has_close = false;  // Variable has <close> attribute
};
```

**Current LocalDeclStmtPayload** (line 425):
```cpp
struct LocalDeclStmtPayload {
   std::vector<Identifier> names;
   ExprNodeList values;
   ~LocalDeclStmtPayload();
};
```

The `Identifier` structure change automatically propagates the `<close>` attribute through the AST.

### 5. Parser Changes for `<close>` Syntax

**Location**: `src/fluid/luajit-2.1/src/parser/ast_builder.cpp`

Find the local declaration parsing (likely in a function like `parse_local_stmt()` or similar).

**Required changes**:
1. After parsing variable name, check for `<` token
2. If found, parse attribute name and verify it's "close"
3. Set `has_close = true` in the Identifier
4. Example parsing logic:
```cpp
Identifier name = parse_identifier();
if (this->check(TokenKind::LessThanToken)) {
   this->advance();  // consume '<'
   Token attr = this->expect_identifier();
   if (strdata(attr.string_value()) != std::string_view("close")) {
      this->emit_error(ErrorCode::UnknownAttribute, attr, "unknown attribute");
   } else {
      name.has_close = true;
   }
   this->expect(TokenKind::GreaterThanToken);  // consume '>'
}
```

### 6. IR Emitter Changes

**Location**: `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp`

Modify `emit_local_decl_stmt()` to mark variables with `VarInfoFlag::Close`:

```cpp
ParserResult<IrEmitUnit> IrEmitter::emit_local_decl_stmt(const LocalDeclStmtPayload& payload)
{
   // ... existing code to emit local declarations ...

   // After var_add(), mark close variables
   for (size_t i = 0; i < payload.names.size(); ++i) {
      if (payload.names[i].has_close) {
         VarInfo* info = &fs->var_get(fs->nactvar - payload.names.size() + i);
         info->info |= VarInfoFlag::Close;
      }
   }

   // ... rest of function ...
}
```

### 7. Scope Exit Handling

**Location**: `src/fluid/luajit-2.1/src/parser/parse_scope.cpp`

The existing `execute_defers()` function provides the template for scope-exit handling. Create a similar function for `__close`:

**Required**: Create `execute_closes()` function:
```cpp
static void execute_closes(FuncState* fs, BCREG limit)
{
   BCREG i = fs->nactvar;
   BCREG oldfreereg = fs->freereg;

   // Process variables in reverse order (LIFO)
   while (i > limit) {
      VarInfo *v = &fs->var_get(--i);
      if (has_flag(v->info, VarInfoFlag::Close)) {
         // Emit code to call __close metamethod
         // 1. Check if variable's metatable has __close
         // 2. If yes, emit call to __close(variable, nil)
         // 3. Wrap in protected call to prevent error propagation
         bcemit_close_var(fs, v->slot);
      }
   }

   fs->freereg = oldfreereg;
}
```

**Modify `fscope_end()`** to call close handlers:
```cpp
static void fscope_end(FuncState* fs)
{
   if (not fs) return;

   FuncScope* bl = fs->bl;
   LexState* ls = fs->ls;
   fs->bl = bl->prev;

   // Execute closes before defers (closes are innermost)
   execute_closes(fs, bl->nactvar);
   execute_defers(fs, bl->nactvar);

   ls->var_remove(bl->nactvar);
   // ... rest of existing code ...
}
```

### 8. Bytecode Emission for __close Calls

**Location**: New function in `src/fluid/luajit-2.1/src/parser/parse_scope.cpp`

**Required**: Create `bcemit_close_var()` function:
```cpp
static void bcemit_close_var(FuncState* fs, BCREG slot)
{
   // This is similar to how defer calls work, but:
   // 1. Must lookup __close metamethod at runtime
   // 2. Pass (object, nil) as arguments (nil = no error during normal exit)
   // 3. Errors in __close should be caught and reported as warnings

   // Implementation approach:
   // Emit bytecode that:
   // 1. Gets metatable of variable in slot
   // 2. Gets __close field from metatable
   // 3. If not nil, calls __close(variable, nil)

   BCREG callbase = fs->freereg;
   RegisterAllocator allocator(fs);

   // Reserve registers for: metatable lookup, __close function, args
   allocator.reserve(BCReg(4 + LJ_FR2));

   // Load the variable
   bcemit_AD(fs, BC_MOV, callbase, slot);

   // Get metatable (requires runtime check)
   // This will need a new helper or bytecode pattern
   // ... implementation details depend on available bytecode ops ...

   // Call __close(obj, nil)
   bcemit_ABC(fs, BC_CALL, callbase, 1, 2 + LJ_FR2);
}
```

**Note**: The exact bytecode sequence depends on what instructions are available for metatable lookup. This may require:
- Using `BC_TGETV` with metatable access
- Or emitting a call to a runtime helper function

### 9. Runtime Support

**Location**: `src/fluid/luajit-2.1/src/runtime/lj_meta.c`

**Required**: Helper function for calling `__close`:
```c
LJ_FUNC void lj_meta_close(lua_State *L, TValue *o, TValue *err)
{
   // 1. Look up __close metamethod
   cTValue *mo = lj_meta_lookup(L, o, MM_close);
   if (mo and not tvisnil(mo)) {
      // 2. Set up protected call
      TValue *top = L->top;
      copyTV(L, top++, mo);      // Push __close function
      copyTV(L, top++, o);       // Push object
      copyTV(L, top++, err);     // Push error (nil for normal exit)
      L->top = top;

      // 3. Call with error protection
      int status = lj_vm_pcall(L, top - 3, 0, 0);
      if (status != LUA_OK) {
         // Log warning but don't propagate error
         // Similar to __gc finalizer behavior
      }
   }
}
```

### 10. Error Propagation

**Location**: Error handling paths in VM

**Required changes**:
- When unwinding stack due to error, call `__close` on marked variables
- Pass error object as second parameter to `__close`
- This requires VM-level integration in `src/fluid/luajit-2.1/src/runtime/lj_err.c`

**Approach**:
```c
// In error unwinding code:
void lj_err_unwind_close(lua_State *L, TValue *frame, TValue *errobj)
{
   // Walk frame's locals, call __close for marked variables
   // Pass errobj as second argument
}
```

### 11. Testing

**Location**: `src/fluid/tests/`

Create `test_close.fluid` with comprehensive tests:

```lua
-- test_close.fluid

local function testBasicClose()
   local closed = false
   local mt = { __close = function(self, err) closed = true end }
   do
      local obj <close> = setmetatable({}, mt)
   end
   assert(closed, "basic close failed")
end

local function testCloseOnReturn()
   local closed = false
   local mt = { __close = function(self, err) closed = true end }

   local function inner()
      local obj <close> = setmetatable({}, mt)
      return 42
   end

   local result = inner()
   assert(closed, "close on return failed")
   assert(result is 42, "return value wrong")
end

local function testCloseOnError()
   local closed = false
   local received_error = nil
   local mt = {
      __close = function(self, err)
         closed = true
         received_error = err
      end
   }

   local ok = pcall(function()
      local obj <close> = setmetatable({}, mt)
      error("test error")
   end)

   assert(not ok, "error should propagate")
   assert(closed, "close on error failed")
   assert(received_error != nil, "error not passed to __close")
end

local function testMultipleClosesLIFO()
   local order = {}
   local function makeMt(n)
      return {
         __close = function(self, err)
            table.insert(order, n)
         end
      }
   end

   do
      local a <close> = setmetatable({}, makeMt(1))
      local b <close> = setmetatable({}, makeMt(2))
      local c <close> = setmetatable({}, makeMt(3))
   end

   assert(order[1] is 3, "LIFO order wrong: first")
   assert(order[2] is 2, "LIFO order wrong: second")
   assert(order[3] is 1, "LIFO order wrong: third")
end

local function testCloseErrorNonPropagating()
   local second_closed = false
   local mt1 = { __close = function() error("close error") end }
   local mt2 = { __close = function() second_closed = true end }

   do
      local a <close> = setmetatable({}, mt1)
      local b <close> = setmetatable({}, mt2)
   end

   -- Despite error in first __close, second should still run
   assert(second_closed, "second close should still run")
end

local function testCloseWithNilFalse()
   -- nil and false values should not crash
   do
      local a <close> = nil
      local b <close> = false
   end
   -- Should complete without error
end

-- Run tests
testBasicClose()
testCloseOnReturn()
testCloseOnError()
testMultipleClosesLIFO()
testCloseErrorNonPropagating()
testCloseWithNilFalse()

print("All __close tests passed!")
```

## Implementation Complexity

### Estimated Effort: **Medium**

The existing `defer` implementation provides much of the infrastructure needed.

**Breakdown**:
- **AST/Parser changes**: Low complexity (2-4 hours)
  - Add `has_close` to Identifier
  - Parse `<close>` syntax in AstBuilder
  - Mark VarInfo in IrEmitter

- **Scope handling**: Low-Medium complexity (4-8 hours)
  - Create `execute_closes()` modelled on `execute_defers()`
  - Integrate into `fscope_end()` and return paths
  - Handle LIFO ordering

- **Bytecode emission**: Medium complexity (8-12 hours)
  - Emit metamethod lookup and call
  - Handle nil/false values gracefully
  - Protected call wrapper

- **Error handling integration**: Medium complexity (8-12 hours)
  - Pass error object to `__close`
  - Prevent error propagation from `__close` itself
  - Integration with VM error unwinding

- **Metamethod registration**: Low complexity (1 hour)
  - Add `_(close)` to MMDEF

- **Testing**: Medium complexity (4-8 hours)
  - Comprehensive test coverage
  - Edge cases and error paths

**Total estimate**: 27-45 hours of development time

## Comparison with Existing `defer`

### Existing `defer` Implementation

The `defer` feature already implemented in Fluid provides:
- Scope-based cleanup via `execute_defers()`
- VarInfoFlag tracking (`Defer`, `DeferArg`)
- Integration with loops, returns, and control flow
- LIFO execution order

### Differences for `__close`

| Aspect | `defer` | `<close>` |
|--------|---------|-----------|
| Syntax | `defer func(args)` | `local x <close> = value` |
| Cleanup function | Specified at declaration | Looked up via metamethod |
| Arguments | User-specified | `(self, error_or_nil)` |
| Error context | Not passed | Error object passed on unwind |
| nil/false handling | N/A | Must be graceful |

### Key Implementation Pattern

The `defer` implementation in `parse_scope.cpp` shows the exact pattern needed:

```cpp
// From execute_defers() - this pattern applies to __close
while (i > limit) {
   VarInfo *v = &fs->var_get(--i);
   if (has_flag(v->info, VarInfoFlag::Defer)) {
      // Emit call bytecode
      bcemit_ABC(fs, BC_CALL, callbase, 1, argc + 1);
   }
}
```

For `__close`, the same loop structure works, but the call must:
1. Look up `__close` metamethod at runtime
2. Pass `(self, nil)` or `(self, error)` as arguments

## Challenges

1. **Metamethod Lookup at Compile Time vs Runtime**: Unlike `defer` where the function is known, `__close` must be looked up at runtime. This requires emitting bytecode for metatable access.

2. **nil/false Values**: Variables declared with `<close>` may be nil or false. The implementation must handle this gracefully without crashing.

3. **Error Object Passing**: When unwinding due to an error, the error object must be captured and passed to `__close`. This requires integration with the VM's error handling.

4. **JIT Compilation**: LuaJIT's trace compiler would need to understand `<close>` semantics, or mark as NYI in traces.

## Recommendation

**Priority**: Medium-High

**Reasoning**:
- Existing `defer` provides 70% of the infrastructure
- Would significantly improve resource management in Fluid applications
- Particularly valuable for Parasol's file/network/graphics resources
- Industry-standard feature (Lua 5.4, Python context managers, C++ RAII)

**Suggested Approach**:
1. Start with metamethod registration and VarInfoFlag
2. Implement AST parsing for `<close>` syntax
3. Implement `execute_closes()` modelled on `execute_defers()`
4. Add bytecode emission for metamethod lookup and call
5. Test basic scope-exit handling (no error integration)
6. Add error unwinding integration in phase 2
7. Extensive testing and edge case handling
8. Consider JIT integration as phase 3 (or mark as NYI)

## Alternative: Enhanced `defer`

If full `__close` implementation proves too complex, consider enhancing `defer` to support metamethod-based cleanup:

```lua
-- Hypothetical syntax
local f = io.open("file.txt")
defer f:close()  -- Already supported

-- Or auto-defer based on metamethod
local f <defer> = io.open("file.txt")  -- Calls __close automatically
```

This would reuse all existing `defer` infrastructure while providing similar ergonomics.

## References

- Lua 5.4 Reference Manual: https://www.lua.org/manual/5.4/manual.html#3.3.8
- Lua 5.4 to-be-closed variables: https://www.lua.org/manual/5.4/manual.html#2.5.7
- Fluid parser source: `src/fluid/luajit-2.1/src/parser/`
- Existing defer implementation: `parse_scope.cpp`, `ir_emitter.cpp`

---

**Last Updated**: 2025-11-26
**Author**: Analysis based on modernised Fluid/LuaJIT parser inspection
