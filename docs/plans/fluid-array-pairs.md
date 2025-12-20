# Plan: Add ipairs() and pairs() Support for Arrays

## Overview

Add native support for `ipairs()` and `pairs()` iterators on the `array` type in LuaJIT. Since arrays have only sequential integer indices, both functions should return the same sequence. Implement this by teaching the existing `next` and `ipairs_aux` fast paths (C fallback, assembly, and JIT recorder) to accept arrays, keeping the current upvalue structure intact.

## Current State

### Existing Array Support
- Array type (`LJ_TARRAY`, `GCarray` struct) fully implemented
- Array indexing works at VM assembly level (`BC_AGETV`, `BC_AGETB`, `BC_ASETV`, `BC_ASETB`)
- `checkarray` macro exists in vm_x64.dasc (line 293)
- `.type ARRAY, GCarray` defined in vm_x64.dasc (line 100)
- `values()` iterator already supports arrays (lib_base.cpp:192-244)
- Helper functions: `lj_arr_getidx()`, `lj_arr_setidx()` in lj_vmarray.cpp

### Current Iterator Implementation (Tables Only)
- `pairs(tbl)` returns: `(next, tbl, nil)` and uses `lj_tab_next` for traversal
- `ipairs(tbl)` returns: `(ipairs_aux, tbl, -1)` - state starts at -1, incremented to 0
- `next`, `pairs`, `ipairs`, and `ipairs_aux` fast paths are table-only in vm_x64.dasc
- `ffh_pairs()` enforces `LUA_TTABLE` and rejects arrays
- `recff_xpairs`, `recff_next`, and `recff_ipairs_aux` only accept tables

### GCarray Layout (from lj_obj.h:599-658)
```
[10] elemtype    - Element type (AET enum)
[11] flags       - Array flags (including ARRAY_READONLY)
[16] storage     - Pointer to element data (matches GCtab.array offset)
[32] metatable   - Optional metatable
[40] len         - Number of elements
[44] capacity    - Allocated capacity
[48] elemsize    - Size of each element in bytes
```

## Implementation Plan

### Phase 1: C Fallback Updates (lib_base.cpp)

#### 1.1 Accept Arrays in ffh_pairs()
Use the existing upvalue for the iterator function. For `pairs` it is `next`; for `ipairs` it is `ipairs_aux`.

```cpp
static int ffh_pairs(lua_State* L, MMS mm)
{
   TValue* o = lj_lib_checkany(L, 1);
   cTValue* mo = lj_meta_lookup(L, o, mm);
   if (not tvisnil(mo)) {
      L->top = o + 1;
      copyTV(L, L->base - 2, mo);
      return FFH_TAILCALL;
   }

   if (tvisarray(o)) {
      copyTV(L, o - 1, o);
      o--;
      setfuncV(L, o - 1, funcV(lj_lib_upvalue(L, 1)));  // next or ipairs_aux
      if (mm IS MM_pairs) setnilV(o + 1);
      else setintV(o + 1, -1);
      return FFH_RES(3);
   }

   LJ_CHECK_TYPE(L, 1, o, LUA_TTABLE);
   copyTV(L, o - 1, o);
   o--;
   setfuncV(L, o - 1, funcV(lj_lib_upvalue(L, 1)));
   if (mm IS MM_pairs) setnilV(o + 1);
   else setintV(o + 1, -1);
   return FFH_RES(3);
}
```

#### 1.2 Allow Arrays in next/ipairs_aux C Fallbacks
Keep the fast path in assembly, but accept arrays in type checks so fallback errors are correct.

```cpp
LJLIB_ASM(next) LJLIB_REC(.)
{
   TValue* o = lj_lib_checkany(L, 1);
   if (!(tvistab(o) or tvisarray(o))) lj_err_argt(L, 1, LUA_TTABLE);
   lj_err_msg(L, ErrMsg::NEXTIDX);
   return FFH_UNREACHABLE;
}

LJLIB_NOREGUV LJLIB_ASM(ipairs_aux)   LJLIB_REC(.)
{
   TValue* o = lj_lib_checkany(L, 1);
   if (!(tvistab(o) or tvisarray(o))) lj_err_argt(L, 1, LUA_TTABLE);
   lj_lib_checkint(L, 2);
   return FFH_UNREACHABLE;
}
```

### Phase 2: Assembly Implementation (vm_x64.dasc)

#### 2.1 Extend `.ffunc_1 next` for Arrays
Add an array path before `checktab`, using `checktptp` or an equivalent tag check. Implement array iteration as:
- If key is nil, start at index 0
- If key is integer, increment to next index
- Bounds check with `ARRAY->len`
- Use `lj_arr_getidx` to load the element and return `(key, value)`
- If out of bounds, return `nil`

#### 2.2 Extend `.ffunc_2 ipairs_aux` for Arrays
Add an array path before `checktab`:
- Validate integer key (0-based)
- Increment key by 1
- Bounds check with `ARRAY->len` using `jae` (unsigned compare)
- Call `lj_arr_getidx` to store value at `[BASE-8]`
- Return `(index, value)` on success, or 0 results on end

#### 2.3 Keep `pairs` and `ipairs` Fast Functions Table-Only
The array case can fall back to C for the initial iterator setup. This keeps the current metatable handling intact while the iterator loop itself is fast via `next` and `ipairs_aux`.

### Phase 3: JIT Recording (lj_ffrecord.cpp)

#### 3.1 Update `recff_xpairs()` for Arrays
Allow `tref_isarray(tr)` and return the existing upvalue as the iterator function:
- `pairs`: `(next, arr, nil)`
- `ipairs`: `(ipairs_aux, arr, -1)`

#### 3.2 Update `recff_next()` for Arrays
Add an array path similar to the table path:
- If key is nil, start index at 0
- If key is a number, narrow to int and add 1
- Guard `idx < len` using `IRFL_ARRAY_LEN`
- Call `IRCALL_lj_arr_getidx` and load the result with `recff_tmpref` and `lj_record_vload`
- Return `(idx, value)` or 0 results at end

#### 3.3 Update `recff_ipairs_aux()` for Arrays
Add array handling analogous to the table path:
- Use `lj_opt_narrow_toint` on the index
- Increment and guard against `IRFL_ARRAY_LEN`
- Use `IRCALL_lj_arr_getidx` to fetch the value and `lj_record_vload` for the result
- Set `rd->nres` to 2 on success or 0 on end

### Phase 4: Supporting Changes

- `lj_lib_checkarray()` already exists but is not required for mixed table/array acceptance.
- `IRFL_ARRAY_LEN` and `tref_isarray()` already exist and can be reused.
- Use `IRCALL_lj_arr_getidx` (no new IR calls needed).

## Files to Modify

| File | Changes |
|------|---------|
| [lib_base.cpp](src/fluid/luajit-2.1/src/lib/lib_base.cpp) | Accept arrays in `ffh_pairs`, `next`, and `ipairs_aux` fallbacks |
| [vm_x64.dasc](src/fluid/luajit-2.1/src/jit/vm_x64.dasc) | Add array paths to `next` and `ipairs_aux` fast functions |
| [lj_ffrecord.cpp](src/fluid/luajit-2.1/src/lj_ffrecord.cpp) | Extend `recff_xpairs`, `recff_next`, and `recff_ipairs_aux` for arrays |
| [test_array.fluid](src/fluid/tests/test_array.fluid) | Add Flute tests for array `pairs` and `ipairs` |

## Testing

Add Flute tests to `src/fluid/tests/test_array.fluid` (review at least 3 existing Flute tests first). Example tests:

```lua
@Test function ArrayPairsIpairs()
   arr = array.of('int', 10, 20, 30)
   sum = 0
   indices = {}
   for i, v in ipairs(arr) do
      indices[#indices] = i
      sum += v
   end
   assert(#indices is 3)
   assert(indices[0] is 0 and indices[1] is 1 and indices[2] is 2)
   assert(sum is 60)

   sum = 0
   for i, v in pairs(arr) do
      sum += v
   end
   assert(sum is 60)
end

@Test function ArrayPairsEmpty()
   empty = array.new(0, 'int')
   count = 0
   for i, v in pairs(empty) do
      count += 1
   end
   assert(count is 0)
end

@Test function ArrayPairsStrings()
   str_arr = array.of('string', 'a', 'b', 'c')
   concat = ''
   for i, s in ipairs(str_arr) do
      concat ..= s
   end
   assert(concat is 'abc')
end
```

Build, install, and run the test:

```bash
cmake --build build/agents --config Debug --parallel
cmake --install build/agents --config Debug
build/agents-install/parasol tools/flute.fluid file=src/fluid/tests/test_array.fluid --log-warning --gfx-driver=headless
```

## Implementation Order

1. Update `ffh_pairs`, `next`, and `ipairs_aux` fallbacks to accept arrays.
2. Extend `next` and `ipairs_aux` assembly fast paths for arrays.
3. Update JIT recorders: `recff_xpairs`, `recff_next`, and `recff_ipairs_aux`.
4. Add Flute tests to `test_array.fluid`.
5. Build, install, and run the Flute tests.

## Key Design Decisions

1. **Reuse existing upvalues**: `pairs` and `ipairs` keep their single upvalue; array support is provided by extending `next` and `ipairs_aux`.
2. **Array iteration semantics**: arrays are 0-based, so `pairs` and `ipairs` both iterate indices 0..len-1.
3. **Bounds checks**: use unsigned comparison (`jae`) for efficient 0 <= idx < len checks.
4. **JIT calls**: use the existing `IRCALL_lj_arr_getidx` helper and load results via TMPREF and `lj_record_vload`.

## Status

### Completed (2025-12-20)

**Interpreter Support - WORKING**
- ✅ Implemented array support in `ffh_pairs`, `next`, and `ipairs_aux` C fallbacks (lib_base.cpp)
- ✅ Extended `next` and `ipairs_aux` assembly fast paths for arrays (vm_x64.dasc)
- ✅ Fixed Windows x64 register aliasing bugs in vm_x64.dasc - see [AGENTS.md](../../src/fluid/luajit-2.1/src/jit/AGENTS.md) for details
- ✅ Added Flute tests for array `pairs`/`ipairs` in `src/fluid/tests/test_array.fluid`
- ✅ All 155 array Flute tests pass

**JIT Recording Support - NYI (Not Yet Implemented)**
- ⚠️ JIT recording for array iteration encounters issues with `lj_ir_call` variadic argument handling on Windows x64
- ⚠️ The third variadic argument to `lj_ir_call` reads garbage (value 13432 instead of valid TRef)
- 🔄 Workaround: Array iteration falls back to interpreter when JIT is active
- 📍 Affected functions set to NYI: `rec_itera`, `rec_array_op`, `recff_ipairs_aux` (array path), `recff_next` (array path)

**Files Modified:**
- `lib_base.cpp` - C fallback for pairs/ipairs/next accepting arrays
- `vm_x64.dasc` - Assembly fast paths with Windows x64 register fixes
- `lj_record.cpp` - NYI for BC_ITERA and array indexing JIT recording
- `lj_ffrecord.cpp` - NYI for array ipairs_aux and next fast function recording

### Future Work

To enable JIT compilation of array iteration:
1. Investigate why `lj_ir_call` variadic args fail on Windows x64 (possibly MSVC calling convention issue)
2. Consider alternative approaches: manually emit IR_CARG chain instead of using lj_ir_call
3. Or implement direct IR patterns for array element access without using helper calls
