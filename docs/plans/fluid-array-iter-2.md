# Plan: Optimised Array Iteration Bytecodes (BC_ISARR/BC_ITERA)

## Summary

Add dedicated bytecodes for native array iteration to eliminate iterator call overhead. This follows the
established pattern of BC_ISNEXT/BC_ITERN for table iteration optimisation.

## Background

Generic-for over arrays goes through BC_ITERC (iterator call). Confirm that arrays are callable via `__call`
in this build; if not, add/enable the iterator entry point first so this optimisation can trigger. Table
iteration has dedicated BC_ISNEXT/BC_ITERN bytecodes that bypass call overhead by directly accessing table
data in the VM.

Arrays are simpler than tables (contiguous memory, no hash part, single element type), making them ideal
candidates for bytecode optimisation.

## Implementation Steps

### Step 0: Confirm Iteration Semantics

- Verify that `for i, v in arr do` is valid today and identify the iterator entry point it uses.
- Confirm expected semantics for nil elements in table/string arrays (continue vs stop) before hard-coding
  iteration termination rules.

### Step 1: Define New Bytecodes

**File:** [lj_bc.h](src/fluid/luajit-2.1/src/bytecode/lj_bc.h)

Add two new bytecodes to the BCOp enum near the existing iteration opcodes (keep them adjacent to
BC_ISNEXT/BC_ITERN and renumber accordingly):
- `BC_ISARR` - Runtime guard that validates array iteration setup (analogous to BC_ISNEXT)
- `BC_ITERA` - Specialised array iterator (analogous to BC_ITERN)

Update BCDEF macro with operand modes (match BC_ISNEXT/BC_ITERN operand styles):
```cpp
_(ISARR,    base,   ___,    jump,   ___) \
_(ITERA,    base,   lit,    lit,    call)
```

Notes:
- Update `BC__MAX` and static_asserts that assume contiguous return/loop ranges.
- Audit range checks like `op >= BC_CALLM and op <= BC_ITERN` and extend them to include BC_ITERA.

### Step 2: Implement VM Assembly

**File:** [vm_x64.dasc](src/fluid/luajit-2.1/src/jit/vm_x64.dasc)

#### BC_ISARR Implementation (~line 4486, after BC_ISNEXT)

- Follow the BC_ISNEXT pattern: check R[A-2] is LJ_TARRAY and R[A-1] is nil, then patch to BC_ITERA and
  branch to the iterator target.
- On failure, patch BC_ISARR to BC_JMP and patch BC_ITERA to BC_ITERC (plus JIT unpatch handling, mirroring
  the BC_ISNEXT logic).

#### BC_ITERA Implementation (~line 4453, after BC_ITERN)

- Add a `hotloop` + `->vm_IITERA` entry like BC_ITERN so dispatch can skip hotcount when needed.
- Use the control var as the last index: if nil, start at 0; else increment by 1.
- Guard `idx < arr->len`; on end, set R[A] to nil and fall through to ITERL.
- Load the value into R[A+1] by calling `lj_arr_getidx` (same helper used by BC_AGET*), avoiding manual
  element-type dispatch.
- Store the index into R[A] and the control var slot R[A-1] using integer tags (respect DUALNUM).

### Step 3: Modify Bytecode Emission

**File:** [ir_emitter.cpp](src/fluid/luajit-2.1/src/parser/ir_emitter.cpp)

#### Add Array Detection Function (~line 428, after predict_next)

```cpp
// Detect if iterator expression is a bare variable suitable for array specialisation
static int predict_array_iter(LexState& lex_state, FuncState &func_state, BCPos pc)
{
   BCIns ins = func_state.bcbase[pc].ins;
   BCOp op = bc_op(ins);

   // The variable needs to be checked at runtime via BC_ISARR
   return (op IS BC_MOV or op IS BC_UGET or op IS BC_GGET) ? 1 : 0;
}
```

#### Modify emit_generic_for_stmt (~line 1050)

```cpp
// In emit_generic_for_stmt(), after predict_next check:
int isnext = (nvars <= 5) ? predict_next(this->lex_state, *fs, exprpc) : 0;
int isarr = 0;

// Check for direct array iteration: `for i, v in array_var do`
// This pattern has exactly 1 iterator expression that's a variable
if (isnext IS 0 and iterator_count IS BCREG(1) and nvars <= 5) {
   isarr = predict_array_iter(this->lex_state, *fs, exprpc);
}

// Emit appropriate setup bytecode
if (isnext) {
   // existing path
} else if (isarr) {
   // use BC_ISARR
} else {
   // use BC_JMP
}

// ... later in the function ...

// Emit appropriate iterator bytecode
if (isnext) {
   // BC_ITERN
} else if (isarr) {
   // BC_ITERA
} else {
   // BC_ITERC
}
```

### Step 4: Update Trace/Dispatch/Recorder for New Opcode

- **`lj_dispatch.cpp`**: treat BC_ITERA like BC_ITERN for hotcounting and dispatch entry; add a VM entry
  symbol if needed (`lj_vm_IITERA`).
- **`lj_trace.cpp`**: include BC_ITERA anywhere BC_ITERN is checked (unpatch, blacklist, trace start/stop).
- **`lj_snap.cpp`**: extend range checks to include BC_ITERA (e.g. call/iter slot handling).
- **`lj_record.cpp`**: add BC_ISARR handling (guard array type + nil control), add BC_ITERA to loop handling
  and instruction recording paths, and update root-trace start rules if needed.

### Step 5: Add JIT Recording (Optional - Can Be Deferred)

**File:** [lj_record.cpp](src/fluid/luajit-2.1/src/lj_record.cpp)

Add `rec_itera()` function following the `rec_itern()` pattern:

```cpp
static LoopEvent rec_itera(jit_State *J, BCREG ra, BCREG rb)
{
   // Load array from ra-2 and guard array type (follow rec_isnext/rec_itern style)
   // Compute next index: nil -> 0, else control + 1
   // Guard idx < len
   // Load value via IRCALL_lj_arr_getidx (reuse the same helper as rec_array_op)
   // Update slots: control (ra-1), index (ra), value (ra+1)
}
```

Add dispatch cases in `rec_loop_op()` and `lj_record_ins()`; ensure trace start/stop handling
considers BC_ITERA similarly to BC_ITERN.

### Step 6: Confirm IR Field Definitions

**File:** [lj_ir.h](src/fluid/luajit-2.1/src/lj_ir.h)

Array IR fields already exist (ARRAY_LEN, ARRAY_ELEMSIZE, ARRAY_ELEMTYPE, ARRAY_STORAGE). No new fields
should be required if `lj_arr_getidx` is used in the VM/JIT paths.

### Step 7: Update Bytecode Documentation

**File:** [BYTECODE.md](src/fluid/luajit-2.1/BYTECODE.md)

Add documentation for new bytecodes in section 6 (Iteration):

```markdown
### 6.4 Array Iteration (BC_ISARR, BC_ITERA)

Optimised bytecodes for native array iteration, analogous to BC_ISNEXT/BC_ITERN for tables.

**BC_ISARR (base, _, jump)**
- Checks if R[base-2] is an array and R[base-1] is nil
- If valid: patches self to BC_ITERA and continues
- If invalid: patches to BC_JMP for fallback to BC_ITERC

**BC_ITERA (base, lit, lit)**
- Specialised array iterator (no function call overhead)
- Returns (index, value) pairs; termination is based on index >= array.len
- Uses 0-based indexing consistent with array access
```

## Files to Modify

| File | Changes |
|------|---------|
| `src/fluid/luajit-2.1/src/bytecode/lj_bc.h` | Add BC_ISARR, BC_ITERA definitions |
| `src/fluid/luajit-2.1/src/jit/vm_x64.dasc` | Implement BC_ISARR, BC_ITERA handlers |
| `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` | Add predict_array_iter(), modify emit_generic_for_stmt() |
| `src/fluid/luajit-2.1/src/lj_record.cpp` | Add rec_itera() and dispatch cases (optional) |
| `src/fluid/luajit-2.1/src/lj_dispatch.cpp` | Treat BC_ITERA like BC_ITERN for dispatch/hotcount |
| `src/fluid/luajit-2.1/src/lj_trace.cpp` | Include BC_ITERA in trace patching/blacklisting logic |
| `src/fluid/luajit-2.1/src/lj_snap.cpp` | Extend slot/range logic to include BC_ITERA |
| `src/fluid/luajit-2.1/src/lj_ir.h` | Confirm IRFL_ARRAY_* fields (no changes expected) |
| `src/fluid/luajit-2.1/BYTECODE.md` | Document new bytecodes |
| `src/fluid/tests/test_array.fluid` | Add performance/correctness tests |

## Implementation Order

Implement interpreter steps first for x64; add JIT support as a follow-up phase:

1. Step 0: Confirm current array iteration entry point and semantics
2. Step 1: Define bytecodes in lj_bc.h (and update range/static_assert users)
3. Step 2: Implement VM assembly in vm_x64.dasc
4. Step 3: Modify bytecode emission in ir_emitter.cpp (automatic detection from `for i,v in arr do`)
5. Step 4: Update dispatch/trace/snap/recorder for BC_ITERA
6. Step 5: Add JIT recording in lj_record.cpp (optional)
7. Step 6: Confirm IR field definitions
8. Step 7: Update documentation
9. Add tests and verify

## Testing Strategy

1. **Correctness Tests:**
   - All existing array iteration tests must pass
   - Verify index/value pairs match current implementation
   - Test all element types (int, float, string, table, etc.)

2. **Edge Cases:**
   - Empty arrays (len=0)
   - Single element arrays
   - Large arrays (1000+ elements)
   - Arrays with nil elements (table/string arrays)
   - Break/continue within loops

3. **Fallback Tests:**
   - BC_ISARR fallback when not an array
   - BC_ISARR fallback when control var not nil
   - Mixed iteration (array then table in same function)

4. **Performance Benchmarks:**
   - Compare iteration time vs current iterator-call path
   - Measure improvement for different array sizes and types

## Build and Test Commands

```bash
# Build
cmake --build build/agents --config Debug --target fluid --parallel
cmake --install build/agents --config Debug

# Test array iteration
build/agents-install/parasol tools/flute.fluid file=src/fluid/tests/test_array.fluid --log-warning

# Run all Fluid tests
ctest --build-config Debug --test-dir build/agents --output-on-failure -L fluid
```

## Scope Decisions

- **Platforms:** x64 only (vm_x64.dasc)
- **JIT Support:** Optional (phase after interpreter path stabilises)
- **Syntax:** Automatic detection - `for i,v in arr do` emits BC_ISARR when iterator is a bare array variable

## Complexity Assessment

- **BC_ISARR**: Low complexity - simple type checks and bytecode patching
- **BC_ITERA**: Medium complexity - reuse `lj_arr_getidx` to avoid manual type dispatch
- **Bytecode emission**: Low complexity - follows existing predict_next pattern
- **JIT recording**: High complexity - requires understanding IR emission deeply

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Element type dispatch complexity in VM | Avoid dispatch; reuse `lj_arr_getidx` helper |
| GC safety for string/table elements | Follow existing patterns in lib_array.cpp |
| Breaking existing iteration | BC_ISARR falls back to BC_ITERC on failure |
| JIT recording complexity | Defer Phase 2 until Phase 1 is stable |

## Notes

- Arrays use 0-based indexing
- The iterator returns both index and value (termination based on index >= len)
- Struct arrays (AET::_STRUCT) are out of scope; decide whether to fallback or return nil values
- The existing iterator path remains as fallback

## Status

- Step 0: Confirmed array iteration runs through the `__call` metamethod (`array_call`/`array_iterator_next`), starting at index 0 when the control var is nil and continuing until `idx >= len`; nil elements are returned, not treated as termination.
- Step 1: Added `BC_ISARR`/`BC_ITERA` to `lj_bc.h`, renumbering opcode ranges to keep iteration opcodes contiguous.
- Step 2: Implemented x64 VM handlers: `BC_ITERA` loads elements via `lj_arr_getidx`, updates control/index slots with integer tags, and terminates by niling the loop value; `BC_ISARR` guards array type + nil control and patches to the iterator/fallback bytecodes.
- Step 3: Parser now detects bare array iterators in generic-for loops and emits `BC_ISARR`/`BC_ITERA` accordingly.
- Pending: Step 4+ (dispatch/trace/recorder updates, documentation, and tests).
