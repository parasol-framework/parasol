# Bytecode-Level Exception Handling for Fluid Try-Except

## Problem Statement

The current try-except implementation wraps try bodies and exception handlers in closures:
```lua
try
   return "value"
except e
   ...
end
```
Transforms to:
```lua
__try(0, function() return "value" end, function(e) ... end, filter)
```

This causes `return`, `break`, and `continue` to only affect the inner closure, not the enclosing function/loop.

## Solution Overview

Emit try body code **inline** (not in closures) with bytecode markers that call C++ runtime functions for exception frame management. The C++ runtime handles:
- Exception frame stack management
- Error matching against filters during unwinding
- Jumping to appropriate handlers

### Progress

- Inline emission now records try blocks and handlers directly in `FuncState`/`GCproto`, including packed filters.
- Runtime filter matching and try-frame cleanup are implemented; BC_RET paths trim stale frames on return.
- Handler re-entry in `err_unwind()`/`lj_err_throw()` is wired to jump into handler PCs with exception tables, and break/continue now emit `BC_TRYLEAVE` before exiting try scopes.

## Implementation Checklist

- [x] **Step 1**: Add BC_TRYENTER, BC_TRYLEAVE to BCDEF macro in `lj_bc.h`
- [x] **Step 2**: Add TryHandlerDesc/TryBlockDesc, TryFrame/TryFrameStack, proto fields, and `lua_State::try_handler_pc`
- [x] **Step 3**: Implement try_stack lifecycle management in `lj_state.cpp` (alloc/free)
- [x] **Step 4**: Add `lj_try_enter()`, `lj_try_leave()`, `lj_try_find_handler()` in `fluid_functions.cpp`
- [x] **Step 5**: Add `cleanup_try_frames_to_base()` and integrate into BC_RET* handlers
- [x] **Step 6**: Modify `err_unwind()` and `lj_err_throw()` to re-enter handlers; define `ERR_TRYHANDLER`
- [x] **Step 7**: Implement handler entry stack restoration and exception table placement
- [x] **Step 8**: Implement `filter_matches()` for exception filtering
- [x] **Step 9**: Rewrite `emit_try_except_stmt()` and record handler metadata in proto
- [x] **Step 10**: Insert `BC_TRYLEAVE` before break/continue/goto jumps that exit try scopes
- [x] **Step 11**: Add VM handlers in buildvm and regenerate
- [ ] **Test**: Enable and run control flow tests (return/break/continue)
- [ ] **Test**: Run full test_try_except.fluid test suite

## Files to Modify

| File | Purpose |
|------|---------|
| `src/fluid/luajit-2.1/src/bytecode/lj_bc.h` | Add BC_TRYENTER, BC_TRYLEAVE opcodes |
| `src/fluid/luajit-2.1/src/runtime/lj_obj.h` | Add TryFrame struct and try_stack to lua_State |
| `src/fluid/luajit-2.1/src/debug/lj_err.h` | Define ERR_TRYHANDLER sentinel |
| `src/fluid/luajit-2.1/src/debug/lj_err.cpp` | Modify err_unwind() to check try frames |
| `src/fluid/luajit-2.1/src/parser/ir_emitter/emit_try.cpp` | Replace closure emission with inline bytecode |
| `src/fluid/luajit-2.1/src/parser/ir_emitter/emit_function.cpp` | Insert TRYLEAVE before break/continue/return jumps that exit a try scope |
| `src/fluid/fluid_functions.cpp` | Add lj_try_enter/lj_try_leave runtime functions |
| `src/fluid/luajit-2.1/src/host/buildvm*.cpp` | Add VM handlers for new opcodes |
| `src/fluid/luajit-2.1/src/lj_state.cpp` | Allocate/free try_stack for threads |

## Implementation Steps

### Step 1: Add Bytecode Instructions (lj_bc.h)

Add to BCDEF macro (after FUNCCW, ~line 206):
```cpp
_(TRYENTER, base, ___, lit, ___) \
_(TRYLEAVE, base, ___, ___, ___)
```

- **BC_TRYENTER**: A=base_reg, D=try_block_index (see Step 2). Calls C++ to push exception frame.
- **BC_TRYLEAVE**: A=base_reg. Calls C++ to pop exception frame.

**Note**: Use consistent naming throughout: BC_TRYENTER and BC_TRYLEAVE (not BC_TRY/BC_TRYEND).

### Step 2: Add Try Metadata and Exception Frame Structure (lj_obj.h)

Add before lua_State struct (~line 962):
```cpp
// Handler metadata stored per-proto
struct TryHandlerDesc {
   uint64_t filter_packed;
   BCPos    handler_pc;     // Bytecode position for handler entry
   BCReg    exception_reg;  // 0xFF when no exception variable
};

struct TryBlockDesc {
   uint16_t first_handler;
   uint8_t  handler_count;
};

// Exception frame for try-except blocks
struct TryFrame {
   uint16_t try_block_index;
   TValue  *frame_base;     // L->base when BC_TRYENTER executed
   TValue  *saved_top;      // L->top when BC_TRYENTER executed
   BCReg    saved_nactvar;  // L->top - L->base at entry
   GCfunc  *func;           // Function containing the try block
   uint8_t  depth;          // Nesting depth for validation
};

constexpr int LJ_MAX_TRY_DEPTH = 32;

struct TryFrameStack {
   TryFrame frames[LJ_MAX_TRY_DEPTH];
   int depth;
};
```

Use `saved_top` as the authoritative restore point on handler entry; keep `saved_nactvar` for
asserts or additional validation only.

Add to GCproto struct (near other metadata pointers):
```cpp
TryBlockDesc  *try_blocks;
TryHandlerDesc *try_handlers;
uint16_t       try_block_count;
uint16_t       try_handler_count;
```

Allocate `try_blocks` and `try_handlers` alongside the proto (or in its GC-managed extra data)
so they are freed with the proto; set counts to zero when a function has no try blocks.

Add to lua_State struct (~line 982):
```cpp
TryFrameStack *try_stack;      // Exception frame stack (lazily allocated)
const BCIns   *try_handler_pc; // Handler PC for error re-entry
```

### Step 3: Try Stack Lifecycle Management

**Allocation points:**
- `lj_state_newthread()`: Initialize `L->try_stack = nullptr` (lazy allocation)
- `lj_state_newthread()`: Initialise `L->try_handler_pc = nullptr`
- First `BC_TRYENTER`: Allocate via `lj_mem_new(L, sizeof(TryFrameStack))`

**Deallocation points:**
- `lj_state_free()` / `lua_close()`: Free try_stack if non-null
- Thread cleanup: Free in the thread free path (e.g. `lj_state_free()` for coroutines or the thread GC finaliser, if present)

```cpp
// In lj_state.cpp lua_close() or lj_state_free():
if (L->try_stack) {
   lj_mem_free(G(L), L->try_stack, sizeof(TryFrameStack));
   L->try_stack = nullptr;
}
```

**Coroutine reuse**: Reset `try_stack->depth = 0` and clear `try_handler_pc` when a coroutine is
re-initialised for reuse.

### Step 4: Add Runtime Functions (fluid_functions.cpp)

```cpp
// Called by BC_TRYENTER handler
extern "C" void lj_try_enter(lua_State *L, BCReg Base, uint16_t TryBlockIndex)
{
   // Lazy allocation
   if (!L->try_stack) {
      L->try_stack = (TryFrameStack *)lj_mem_new(L, sizeof(TryFrameStack));
      L->try_stack->depth = 0;
   }

   if (L->try_stack->depth >= LJ_MAX_TRY_DEPTH) {
      lj_err_msg(L, LJ_ERR_XNEST);  // "try blocks nested too deeply"
   }

   TryFrame *try_frame = &L->try_stack->frames[L->try_stack->depth++];
   try_frame->try_block_index = TryBlockIndex;
   try_frame->frame_base = L->base;
   try_frame->saved_top = L->top;
   try_frame->saved_nactvar = (BCReg)(L->top - L->base);
   try_frame->func = curr_func(L);
   try_frame->depth = (uint8_t)L->try_stack->depth;
}

// Called by BC_TRYLEAVE handler
extern "C" void lj_try_leave(lua_State *L)
{
   if (L->try_stack and L->try_stack->depth > 0) {
      L->try_stack->depth--;
   }
}

// Called during exception to find matching handler
// Returns handler PC or nullptr if no match
extern "C" bool lj_try_find_handler(lua_State *L, const TryFrame *Frame,
                                    TValue *ErrorObj, ERR ErrorCode,
                                    const BCIns **HandlerPc, BCReg *ExceptionReg);
```

### Step 5: Non-Local Exit Handling (CRITICAL)

**Problem**: `return`, `break`, `continue` can bypass BC_TRYLEAVE, leaving stale try frames.

**Solution**: Combine emitter-level cleanup for intra-function jumps with runtime cleanup for returns/unwinding.

#### Emitter-Level Cleanup (Required for `break`/`continue`)

Track try depth in FuncState and insert `BC_TRYLEAVE` before any jump that exits a try scope
(break/continue/goto out of a try). The emitter already knows the target scope depth, so it can
compute the number of try frames to pop and emit that many `BC_TRYLEAVE` instructions before the
actual jump.

#### Runtime Cleanup (Required for `return`/unwind)

Use a helper to pop try frames by base pointer during returns and unwinding:

```cpp
// In err_unwind() or a new cleanup function:
void cleanup_try_frames_to_base(lua_State *L, TValue *TargetBase)
{
   if (!L->try_stack) return;

   // Pop all try frames whose frame_base is at or above target_base
   while (L->try_stack->depth > 0) {
      TryFrame *try_frame = &L->try_stack->frames[L->try_stack->depth - 1];
      if (try_frame->frame_base < TargetBase) break;  // This frame is in an outer scope
      L->try_stack->depth--;
   }
}
```

`TryBlockIndex` indexes into `GCproto::try_blocks` for the current function; `lj_try_enter()` stores
it on the TryFrame so `lj_try_find_handler()` can resolve handlers later.

Example lookup logic:
```cpp
bool lj_try_find_handler(lua_State *L, const TryFrame *Frame,
                         TValue *ErrorObj, ERR ErrorCode,
                         const BCIns **HandlerPc, BCReg *ExceptionReg)
{
   GCfunc *func = curr_func(L);
   GCproto *proto = funcproto(func);
   const TryBlockDesc *try_block = &proto->try_blocks[Frame->try_block_index];

   for (uint8_t index = 0; index < try_block->handler_count; ++index) {
      const TryHandlerDesc *handler =
         &proto->try_handlers[try_block->first_handler + index];

      if (!filter_matches(handler->filter_packed, ErrorCode, ErrorObj)) continue;

      *HandlerPc = proto_bcpos(proto, handler->handler_pc);
      *ExceptionReg = handler->exception_reg;
      return true;
   }

   return false;
}
```

**Integration points:**
- `BC_RET*` handlers: Before returning, call `cleanup_try_frames_to_base(L, return_target_base)`
- `lj_func_closeuv()`: Call cleanup when closing upvalues during unwind
- Frame unwinding in `err_unwind()`: Already handled by exception path

This ensures `break`/`continue` get explicit cleanup, while `return` and exception unwinds stay
robust even if a direct `BC_TRYLEAVE` is bypassed.

### Step 6: Modify Error Unwinding (lj_err.cpp)

In `err_unwind()` (~line 213), add try frame handling:

```cpp
extern void *err_unwind(lua_State *L, void *StopCf, int ErrCode)
{
   // Check for try frames that should handle this error
   if (L->try_stack and L->try_stack->depth > 0) {
      TryFrame *try_frame = &L->try_stack->frames[L->try_stack->depth - 1];

      // Verify try frame is in current call chain (not from a returned function)
      TValue *frame = L->base - 1;
      if (try_frame->frame_base >= frame and try_frame->func IS curr_func(L)) {
         // Get error object and code
         TValue *error_obj = L->top - 1;
         ERR err_code = /* extract from prvFluid or error object */;

         const BCIns *handler_pc = nullptr;
         BCReg exception_reg = 0xFF;

         if (lj_try_find_handler(L, try_frame, error_obj, err_code, &handler_pc, &exception_reg)) {
            // Restore stack state
            L->base = try_frame->frame_base;
            L->top = try_frame->saved_top;
            lj_func_closeuv(L, try_frame->saved_top);

            // Pop try frame
            L->try_stack->depth--;

            // Build exception table and place in handler's register
            // ... build_exception_table(L, error_obj, err_code, exception_reg) ...

            // Stash handler PC so lj_err_throw can re-enter the VM at the handler
            L->try_handler_pc = handler_pc;
            return (void *)ERR_TRYHANDLER;
         }
      }
   }

   // ... existing err_unwind logic ...
}
```

Update `lj_err_throw()` (or the shared error dispatch path) to recognise `ERR_TRYHANDLER` and
resume execution at `L->try_handler_pc` via `lj_vm_run()` or an equivalent VM re-entry helper,
then clear `try_handler_pc` once control transfers to the handler.
Define `ERR_TRYHANDLER` as a private sentinel (e.g. in `lj_err.h`) so it cannot collide with
regular error codes.

### Step 7: Handler Entry Stack Restoration

When jumping to an exception handler, restore VM state:

```cpp
void handle_try_exception(lua_State *L, TryFrame *Frame,
                          TValue *ErrorObj, ERR ErrorCode, BCReg ExceptionReg)
{
   // 1. Restore stack pointers
   L->base = Frame->frame_base;
   L->top = Frame->saved_top;

   // 2. Close any open upvalues in the abandoned scope
   lj_func_closeuv(L, Frame->saved_top);

   // 3. Build exception table
   lua_createtable(L, 0, 4);
   // Set e.code, e.message, e.line, e.trace

   // 4. Place exception table in handler's expected register
   // (Register determined by TryHandlerDesc.exception_reg)
   // If ExceptionReg IS 0xFF, discard the table instead

}
```

### Step 8: Filter Matching and Error Object Binding

**Filter semantics:**

| Error Type | `e.code` | `e.message` | Filter Match |
|------------|----------|-------------|--------------|
| `raise(ERR_xxx)` | ERR code (integer) | GetErrorMsg(code) | Match packed filter codes |
| `error("msg")` | `nil` | Error string | Match catch-all only (filter=0) |
| Lua runtime error | `nil` | Error message | Match catch-all only |

**Filter matching logic:**
```cpp
bool filter_matches(uint64_t PackedFilter, ERR ErrorCode, TValue *ErrorObj)
{
   if (PackedFilter IS 0) return true;  // Catch-all

   // Only ERR codes can match specific filters
   if (ErrorCode < ERR::ExceptionThreshold) return false;

   // Unpack and check each 16-bit code
   for (int shift = 0; shift < 64; shift += 16) {
      uint16_t filter_code = (PackedFilter >> shift) & 0xFFFF;
      if (filter_code IS 0) break;
      if (filter_code IS (uint16_t)ErrorCode) return true;
   }
   return false;
}
```

**Exception variable binding:**
- Handler receives the exception table in the register specified by `TryHandlerDesc.exception_reg`
- If no exception variable (`except` without `e`), use `exception_reg = 0xFF` and discard the table

### Step 9: Rewrite IR Emitter (emit_try.cpp)

Replace closure-based emission with inline bytecode:

```cpp
ParserResult<IrEmitUnit> IrEmitter::emit_try_except_stmt(const TryExceptPayload &Payload)
{
   FuncState *func_state = &this->func_state;
   BCReg base_reg = BCReg(func_state->freereg);

   size_t first_handler_index = func_state->try_handlers.size();
   uint16_t try_block_index = (uint16_t)func_state->try_blocks.size();
   func_state->try_blocks.push_back(TryBlockDesc{
      (uint16_t)first_handler_index,
      (uint8_t)Payload.except_clauses.size()
   });

   // 1. Emit BC_TRYENTER with try block index
   bcemit_AD(func_state, BC_TRYENTER, base_reg, try_block_index);

   // 2. Emit try body INLINE (not in closure!)
   emit_block(*Payload.try_block, FuncScopeFlag::None);

   // 3. Emit BC_TRYLEAVE after try body
   bcemit_AD(func_state, BC_TRYLEAVE, base_reg, 0);

   // 4. Jump over handlers (successful completion)
   BCPos exit_jmp = bcemit_jmp(func_state);

   // 5. Emit handlers inline and record metadata
   std::vector<BCPos> handler_exits;
   for (const auto &clause : Payload.except_clauses) {
      uint64_t packed_filter = pack_filter_codes(clause.filter_codes);
      BCReg exception_reg = allocate_exception_var(func_state, clause.exception_var);
      // allocate_exception_var returns 0xFF when the clause has no exception variable
      BCPos handler_pc = func_state->pc;

      func_state->try_handlers.push_back(TryHandlerDesc{
         packed_filter,
         handler_pc,
         exception_reg
      });

      // Handler entry: exception table placed in exception_reg by runtime
      emit_block(*clause.block, FuncScopeFlag::None);

      // Jump to exit
      handler_exits.push_back(bcemit_jmp(func_state));
   }

   // 7. Patch all exits
   BCPos try_end = func_state->pc;
   jmp_patch(func_state, exit_jmp, try_end);
   for (BCPos exit : handler_exits) {
      jmp_patch(func_state, exit, try_end);
   }

   return success();
}
```

During function finalisation, copy `FuncState::try_blocks` and `FuncState::try_handlers` into the
new `GCproto` fields so `lj_try_find_handler()` can resolve handler PCs and exception registers
at runtime.

Validate `try_block_index` and `handler_count` fit their storage widths (e.g. 16-bit block index,
8-bit handler count) and raise a parse error if exceeded.

### Step 10: VM Handler Integration (buildvm)

**This requires explicit buildvm modification** - it is NOT automatic.

1. **Update opcode count** in `lj_bc.h`: Increment `BC__MAX` to include new opcodes

2. **Add VM handlers** in the appropriate `vm_*.dasc` file or interpreter:

```cpp
// For interpreted mode, add to vm_x64.dasc or equivalent:
|.macro ins_TRYENTER, base, try_block
|  // Call C++ runtime
|  mov CARG1, L:RB
|  movzx CARG2d, byte [PC-4+OFS_RA]  // base register
|  movzx CARG3d, word [PC-4+OFS_RD]   // try_block_index
|  call extern lj_try_enter
|  ins_next
|.endmacro

|.macro ins_TRYLEAVE, base
|  mov CARG1, L:RB
|  call extern lj_try_leave
|  ins_next
|.endmacro
```

3. **Regenerate buildvm output**:
```bash
# After modifying lj_bc.h and vm_*.dasc
rm -f build/agents/src/fluid/luajit-generated/lj_bcdef.h
rm -f build/agents/src/fluid/luajit-generated/buildvm*
cmake --build build/agents --config Debug --parallel
```

4. **Register handlers** in `lj_dispatch.cpp` if needed for dispatch table updates.

## Exception Handling Flow

1. **Normal execution**: BC_TRYENTER pushes frame, body executes inline, BC_TRYLEAVE pops frame
2. **Exception thrown**: `lj_err_throw()` -> `err_unwind()` checks try_stack
3. **Handler matched**: Restore stack, pop frame, build exception table, set `try_handler_pc`, return `ERR_TRYHANDLER`
   so `lj_err_throw()` re-enters the VM at the handler PC
4. **No match**: Exception propagates to outer try or default error handling
5. **Non-local exit**: Frame cleanup via `cleanup_try_frames_to_base()` before exit

## Control Flow Correctness

- **return**: BC_RET triggers `cleanup_try_frames_to_base()` before returning
- **break/continue**: Emitter inserts `BC_TRYLEAVE` for each exited try scope before the jump
- **Nested try**: Multiple TryFrame entries, innermost first; cleanup respects nesting

## Testing

Enable the disabled tests in `test.fluid`:
- `return_from_try()` - return from try block
- `return_from_except()` - return from except block
- `break_from_try()` - break from loop inside try
- `continue_from_try()` - continue from loop inside try

Run with:
```bash
build/agents-install/parasol tools/flute.fluid file=test.fluid --log-warning
```

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| JIT compatibility | New opcodes bypass JIT (interpreter only for now) |
| Stack corruption | Restore base/top from TryFrame and close upvalues at saved_top on handler entry |
| Memory leak | Free try_stack in lua_close() and thread GC |
| Stale try frames | Emitter inserts `BC_TRYLEAVE` on scope-exiting jumps plus `cleanup_try_frames_to_base()` on returns/unwind |
| Nested exceptions | Track exception-in-handler state; re-throw propagates |

## Expression Form (Deferred)

The expression form (`ex, a, b = try ... except ... end`) is NOT covered by this plan. Tests for expression form remain disabled:
- `ExpressionSuccess`
- `ExpressionException`
- `ExpressionSingleResult`
- `ExpressionNoReturn`

Expression form would require tracking result count and managing return values differently. This can be addressed in a future iteration after statement form works.

## Test Coverage Summary

**Statement form tests (24 tests)**: All covered by this plan
**Control flow tests (4 tests)**: The primary goal - will be enabled after implementation
**Expression form tests (4 tests)**: Deferred to future work

## Future Enhancements

- Expression form support (`ex = try ... end`)
- JIT trace recording for try blocks
- `finally` clause support via BC_TRYFINALLY
- Performance optimisation of filter matching
