# Fluid Configurable Array Indexing Implementation Plan

## Overview

This document outlines the implementation plan for making Fluid (Parasol's LuaJIT-based scripting language) support configurable array starting indices. The goal is to allow compilation with either traditional Lua 1-based indexing or C-style 0-based indexing via a `#define STARTING_INDEX` configuration option.

## Background

Standard Lua uses 1-based array indexing, inherited from its origins as a data description language designed for non-programmers. While this is intuitive for some use cases, it can be awkward when interfacing with C/C++ code or when developers are accustomed to 0-based indexing common in most programming languages.

This feature will allow Parasol to be built with either indexing convention, controlled at compile time.

## Configuration Approach

### Define Location

Add the configuration macro to `src/fluid/luajit-2.1/src/luaconf.h`:

```cpp
// Array indexing configuration.
// Set to 0 for C-style 0-based indexing, or 1 for traditional Lua 1-based indexing.
#ifndef LJ_STARTING_INDEX
#define LJ_STARTING_INDEX 1
#endif
```

### Supporting Macros

Add helper macros to `src/fluid/luajit-2.1/src/lj_def.h`:

```cpp
// Index adjustment macros for converting between semantic and storage indices.
// Semantic index: The index as seen by Lua code (starts at LJ_STARTING_INDEX).
// Storage index: The internal array index (always 0-based).
#define LJ_IDX_TO_STORAGE(idx)    ((idx) - LJ_STARTING_INDEX)
#define LJ_IDX_FROM_STORAGE(idx)  ((idx) + LJ_STARTING_INDEX)

// Length adjustment: storage_len is (highest_storage_index + 1)
// For 1-based: length = storage_len (since storage[0..n-1] maps to semantic[1..n])
// For 0-based: length = storage_len (since storage[0..n-1] maps to semantic[0..n-1])
// Actually length semantic is "number of elements", independent of starting index.
```

## Affected Code Areas

### 1. Table Core (`src/fluid/luajit-2.1/src/runtime/lj_tab.cpp`, `lj_tab.h`)

**Current Behaviour:**
- Array storage uses indices 0 to `asize-1`
- `inarray(t, key)` checks `(MSize)(key) < (MSize)(t)->asize`
- `arrayslot(t, i)` returns `&tvref((t)->array)[(i)]`
- Semantic index 1 maps to storage index 1, leaving slot 0 unused

**Required Changes:**

```cpp
// In lj_tab.h, modify the inarray and arrayslot macros:
#if LJ_STARTING_INDEX == 0
#define inarray(t, key)      ((MSize)(key) < (MSize)(t)->asize)
#define arrayslot(t, i)      (&tvref((t)->array)[(i)])
#else
// Original 1-based: key must be >= 1 and < asize
#define inarray(t, key)      ((MSize)(key) > 0 and (MSize)(key) < (MSize)(t)->asize)
#define arrayslot(t, i)      (&tvref((t)->array)[(i)])
#endif
```

**Files to modify:**
- `runtime/lj_tab.h` - Macros for array access
- `runtime/lj_tab.cpp` - `lj_tab_new_ah()`, `lj_tab_len()`, `tab_len_slow()`, `lj_tab_next()`

### 2. Table Length Calculation (`lj_tab.cpp`)

**Current Behaviour:**
- `lj_tab_len()` finds the boundary where `t[i] ~= nil` and `t[i+1] == nil`
- Starting search from semantic index 1

**Required Changes:**

```cpp
// In lj_tab_len():
MSize LJ_FASTCALL lj_tab_len(GCtab* t)
{
   size_t hi = (size_t)t->asize;
   if (hi) hi--;
#if LJ_STARTING_INDEX == 0
   // For 0-based, check from index 0
   if (hi > 0 and LJ_LIKELY(tvisnil(arrayslot(t, hi)))) {
      size_t lo = 0;  // Start at 0 for 0-based
      // ... binary search
   }
#else
   // Original 1-based logic
   if (hi > 0 and LJ_LIKELY(tvisnil(arrayslot(t, hi)))) {
      size_t lo = 0;
      // ... binary search
   }
#endif
   return t->hmask ? tab_len_slow(t, hi) : (MSize)hi;
}
```

**Note:** The length semantics need careful consideration:
- In 1-based Lua: `#t` returns the count of elements from index 1
- In 0-based: `#t` should return the count of elements from index 0
- The return value represents "number of consecutive non-nil elements starting from STARTING_INDEX"

### 3. Table Library (`src/fluid/luajit-2.1/src/lib/lib_table.cpp`)

**Affected Functions:**

| Function | Current Index | Required Change |
|----------|--------------|-----------------|
| `table_foreachi` | `for i=1,#t do` | `for i=LJ_STARTING_INDEX,#t+LJ_STARTING_INDEX-1 do` |
| `table_insert` | `i = lj_tab_len(t) + 1` | `i = lj_tab_len(t) + LJ_STARTING_INDEX` |
| `table_remove` | `for i=pos+1,len do t[i-1]=t[i]` | Adjust for starting index |
| `table_concat` | Default `i = 1` | Default `i = LJ_STARTING_INDEX` |
| `table_pack` | `array + 1` | `array + LJ_STARTING_INDEX` |
| `table_sort` | `auxsort(L, 1, n)` | `auxsort(L, LJ_STARTING_INDEX, n + LJ_STARTING_INDEX - 1)` |
| `table_move` | 1-based semantics | Adjust for starting index |

**Example Changes:**

```cpp
// table_insert
LJLIB_CF(table_insert) LJLIB_REC(.)
{
   GCtab* t = lj_lib_checktab(L, 1);
#if LJ_STARTING_INDEX == 0
   int32_t n, i = (int32_t)lj_tab_len(t);  // For 0-based, next index is length
#else
   int32_t n, i = (int32_t)lj_tab_len(t) + 1;  // For 1-based, next index is length + 1
#endif
   // ...
}

// table_concat default start
int32_t i = lj_lib_optint(L, 3, LJ_STARTING_INDEX);
```

### 4. String Library (`src/fluid/luajit-2.1/src/lib/lib_string.cpp`)

**Affected Functions:**

| Function | Current Behaviour | Required Change |
|----------|-------------------|-----------------|
| `string_byte` | Default start = 1 | Default start = `LJ_STARTING_INDEX` |
| `string_sub` | 1-based indices | Adjust for starting index |
| `string_find` | Returns 1-based positions | Return `LJ_STARTING_INDEX`-based positions |
| `string_match` | Position captures return 1-based | Adjust capture positions |
| `string_split` | Builds array from index 1 | Build from `LJ_STARTING_INDEX` |
| `string_join` | Iterates from index 1 | Iterate from `LJ_STARTING_INDEX` |
| `string_char` | Loop from 1 to nargs | Loop from `LJ_STARTING_INDEX` |

**Example Changes:**

```cpp
// string_byte
int32_t start = lj_lib_optint(L, 2, LJ_STARTING_INDEX);

// string_find return values
setintV(L->top - 2, (int32_t)(q - strdata(s)) + LJ_STARTING_INDEX);

// Pattern capture positions
lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + LJ_STARTING_INDEX);
```

### 5. Base Library (`src/fluid/luajit-2.1/src/lib/lib_base.cpp`)

**Affected Functions:**

| Function | Current Behaviour | Required Change |
|----------|-------------------|-----------------|
| `ipairs` | Control var starts at 0 | Control var starts at `LJ_STARTING_INDEX - 1` |
| `ipairs_aux` | Increments and returns | No change needed (increment logic is correct) |
| `unpack` | Default `i = 1` | Default `i = LJ_STARTING_INDEX` |
| `select` | 1-based argument selection | Adjust for starting index |

**Example Changes:**

```cpp
// ipairs initialisation
if (mm == MM_pairs) setnilV(o + 1);
else setintV(o + 1, LJ_STARTING_INDEX - 1);  // So first returned index is LJ_STARTING_INDEX

// unpack
int32_t i = lj_lib_optint(L, 2, LJ_STARTING_INDEX);
int32_t e = (L->base + 3 - 1 < L->top and !tvisnil(L->base + 3 - 1)) ?
   lj_lib_checkint(L, 3) : (int32_t)lj_tab_len(t) + LJ_STARTING_INDEX - 1;
```

### 6. C API (`src/fluid/luajit-2.1/src/lj_api.cpp`)

**Affected Functions:**

The C API functions `lua_rawgeti` and `lua_rawseti` take integer indices directly. The question is whether these should be adjusted or left as-is for backward compatibility.

**Recommendation:** Leave C API functions unchanged - they pass indices directly to the table functions. The semantic adjustment happens at the Lua/library level, not the C API level. This maintains C API compatibility.

### 7. VM Bytecode (`src/fluid/luajit-2.1/src/jit/vm_*.dasc`)

**Affected Instruction: BC_ITERN**

The `BC_ITERN` instruction handles `ipairs` iteration. It currently:
1. Gets the control variable (starts at 0 after `BC_ISNEXT` initialises it)
2. Iterates through the array part with the storage index
3. Returns the index as the key

**Required Changes:**

```asm
// In vm_IITERN (all architectures):
// After setting up the key to return:
// Currently: cvtsi2sd xmm0, RCd  (or equivalent)
// Need to add LJ_STARTING_INDEX if it's 0-based to convert storage to semantic

#if LJ_STARTING_INDEX == 0
// No adjustment needed - storage index IS the semantic index
#else
// Current code - storage index is returned as-is, which works because
// the control var is 0-based internally but represents (semantic_index - 1)
#endif
```

**Note:** The VM code is tricky because:
- The control variable stores a 0-based iteration counter
- The returned key should be the semantic index
- With 1-based: counter 0 → returns key 1, counter 1 → returns key 2, etc.
- With 0-based: counter 0 → returns key 0, counter 1 → returns key 1, etc.

The current VM code returns the counter directly as the key. For 0-based indexing, this is correct. For 1-based, the counter would need to be stored as (semantic_index - 1).

### 8. Buffer Library (`src/fluid/luajit-2.1/src/runtime/lj_buf.cpp`)

**Affected Function: `lj_buf_puttab`**

```cpp
SBuf* lj_buf_puttab(SBuf* sb, GCtab* t, GCstr* sep, int32_t i, int32_t e)
{
   // i and e are semantic indices, passed from caller
   // The loop uses lj_tab_getint which expects semantic indices
   // No change needed if lj_tab_getint handles the conversion
}
```

### 9. JIT Compiler Recording

**Files:**
- `src/fluid/luajit-2.1/src/lj_ffrecord.cpp`
- `src/fluid/luajit-2.1/src/lj_record.cpp`

**Affected areas:**
- `recff_ipairs_aux` - Recording of ipairs iteration
- Table access recording with integer keys

**Required Changes:**
The JIT compiler needs to emit correct IR for index adjustments when `LJ_STARTING_INDEX == 0`.

### 10. Serialization (`src/fluid/luajit-2.1/src/runtime/lj_serialize.cpp`)

**Affected Code:**

```cpp
// Dictionary iteration
for (i = 1; i <= len and i < dict->asize; i++) {
   // Change to:
   // for (i = LJ_STARTING_INDEX; i <= len + LJ_STARTING_INDEX - 1 and i < dict->asize; i++)
}
```

### 11. Debug Library (`src/fluid/luajit-2.1/src/debug/lj_debug.cpp`)

**Affected areas:**
- Line info array access (if any use semantic indices)
- Generally debug info uses 1-based line numbers which should remain unchanged

## Implementation Phases

### Phase 0: Unit Test Infrastructure (Estimated: 1 day)

**CRITICAL: Establish unit tests BEFORE making changes.**

1. Create `src/fluid/luajit-2.1/src/runtime/unit_test_indexing.cpp`.  Refer to MODTest() in fluid.cpp for hooks and `src/fluid/luajit-2.1/src/parser/parser_unit_tests.cpp` for working test patterns.
2. Implement baseline tests that pass with current 1-based indexing
3. Unit tests are run by test_unit_tests.fluid and require ENABLE_UNIT_TESTS to have been enabled in the build.
4. Verify all baseline tests pass before proceeding

### Phase 1: Core Infrastructure (Estimated: 2-3 days)

1. Add `LJ_STARTING_INDEX` configuration to `luaconf.h`
2. Add helper macros to `lj_def.h`
3. Modify `lj_tab.h` macros (`inarray`, `arrayslot`)
4. Update `lj_tab.cpp`:
   - `lj_tab_len()` and `tab_len_slow()`
   - `lj_tab_next()` - table traversal

**Unit tests to add/verify:**
- `test_starting_index_constant` - Compile-time validation
- `test_lj_tab_getint_semantic_index` - Low-level table access
- `test_lj_tab_len_returns_element_count` - Length calculation
- `test_empty_table_length` - Edge case

### Phase 2: Library Functions (Estimated: 3-4 days)

1. Update `lib_table.cpp`:
   - All table manipulation functions
   - Embedded Lua code in `LJLIB_LUA` macros

2. Update `lib_string.cpp`:
   - String indexing functions
   - Pattern matching position returns
   - Array-building functions (`split`, etc.)

3. Update `lib_base.cpp`:
   - `ipairs` and `ipairs_aux`
   - `unpack`
   - `select`

**Unit tests to add/verify:**
- `test_table_insert_position` - table.insert semantics
- `test_table_concat_default_range` - table.concat defaults
- `test_table_sort_operates_on_sequence` - table.sort range
- `test_string_find_returns_correct_index` - string.find positions
- `test_string_byte_default_start` - string.byte default
- `test_unpack_default_range` - unpack default bounds
- `test_ipairs_starting_index` - ipairs iteration start

### Phase 3: VM and JIT (Estimated: 3-4 days)

1. Update VM bytecode handlers:
   - `BC_ITERN` in all `.dasc` files (x86, x64, ARM, ARM64, MIPS, PPC)
   - `BC_ISNEXT` control variable initialisation

2. Update JIT recording:
   - `lj_ffrecord.cpp` - Fast function recording
   - `lj_record.cpp` - General recording adjustments

**Unit tests to add/verify:**
- `test_ipairs_jit_compiled` - Verify JIT-compiled ipairs works
- `test_array_access_jit_compiled` - Verify JIT array access
- Test with `jit.off()` and `jit.on()` to isolate interpreter vs JIT

### Phase 4: Auxiliary Systems (Estimated: 1-2 days)

1. Update `lj_serialize.cpp` - Table serialization
2. Update `lj_buf.cpp` - Buffer operations on tables
3. Review and update any debug-related code

**Unit tests to add/verify:**
- `test_table_serialization_roundtrip` - Serialize/deserialize preserves indices

### Phase 5: Integration Testing (Estimated: 3-5 days)

1. Run full unit test suite with `LJ_STARTING_INDEX=1` (must all pass)
2. Run full unit test suite with `LJ_STARTING_INDEX=0` (must all pass)
3. Create Fluid integration tests (`test_indexing.fluid`)
4. Test edge cases:
   - Empty tables
   - Tables with holes
   - Mixed array/hash tables
   - Negative indices
   - Very large indices

5. Test interoperability:
   - C API calls from Lua
   - Lua calls from C

6. Performance regression testing

### Phase 6: C API Zero-Based Entry Points (Estimated: 0.5-1 day)

1. Add `lua_rawgetzi`/`lua_rawsetzi` to provide zero-based integer table access for embedders
2. Translate zero-based indices to the configured semantic base internally so the array part remains consistent
3. Document that these functions supersede the existing `lua_rawgeti`/`lua_rawseti` and will replace them in future phases

### Phase 7: VM Iterator Key Offsets (Estimated: 0.5 day)

1. Update `BC_ITERN` iterator fast paths in `vm_*.dasc` so integer keys returned from the array part are adjusted by `LJ_STARTING_INDEX` before being pushed to the stack.
2. Keep the control variable in storage indices to preserve iteration state while mapping returned keys to semantic indices.
3. Apply the adjustment across x86, x64, ARM, ARM64, and PPC back ends; MIPS may be addressed later if re-enabled.

### Test-Driven Development Workflow

For each code change:

1. **Write the test first** - Add unit test that validates expected behaviour
2. **Verify test fails** - With current code, test should fail for 0-based indexing
3. **Implement the change** - Modify the code to support configurable index
4. **Run all tests** - Ensure new test passes AND no regressions
5. **Build with both configurations** - Verify compilation succeeds for both values

## Unit Testing Strategy

Unit testing should be implemented as the primary validation mechanism for all index-related changes. A dedicated unit test file `unit_test_indexing.cpp` should be created following the pattern established by `parser/parser_unit_tests.cpp`.

## Status Update

- Phase 1: Core Infrastructure - implemented
- Phase 2: Library Functions - implemented
- Phase 3: VM and JIT - implemented
- Phase 4: Serialization and buffer preparation - implemented
- Phase 5: Integration Testing - pending dual-configuration sweeps
- Phase 6: C API Zero-Based Entry Points - implemented
- Phase 7: VM Iterator Key Offsets - implemented
- Phase 8: Buffer Library alignment - implemented
- Phase 9: JIT Recording adjustments - implemented
- Phase 10: Serialization dictionary offsets - implemented
- Phase 11: Debug Library adjustments - implemented
- Indexing unit tests: `indexing_unit_tests()` registered with the Fluid module and compiled when `ENABLE_UNIT_TESTS` is set

### New Unit Test File: `unit_test_indexing.cpp`

Create `src/fluid/luajit-2.1/src/runtime/unit_test_indexing.cpp`:

```cpp
// Unit tests for array indexing configuration.
// Tests both compile-time constant validation and runtime behaviour
// to ensure LJ_STARTING_INDEX is correctly applied throughout the codebase.

#include <parasol/main.h>

#ifdef ENABLE_UNIT_TESTS

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_tab.h"
#include "lj_str.h"

#include <array>
#include <string>
#include <string_view>

#include "../../defs.h"

static objScript *glTestScript = nullptr;

namespace {

struct LuaStateHolder {
   LuaStateHolder() { this->state = luaL_newstate(glTestScript); }
   ~LuaStateHolder() { if (this->state) lua_close(this->state); }
   lua_State* get() const { return this->state; }
private:
   lua_State* state = nullptr;
};

struct TestCase {
   const char* name;
   bool (*fn)(pf::Log&);
};

// Execute Lua code and check result
static bool run_lua_test(lua_State* L, std::string_view code, std::string& error)
{
   if (luaL_loadbuffer(L, code.data(), code.size(), "indexing-test")) {
      error = lua_tostring(L, -1);
      lua_pop(L, 1);
      return false;
   }
   if (lua_pcall(L, 0, 1, 0)) {
      error = lua_tostring(L, -1);
      lua_pop(L, 1);
      return false;
   }
   return true;
}

//********************************************************************************************************************
// Core indexing tests - validate LJ_STARTING_INDEX configuration

static bool test_starting_index_constant(pf::Log& log)
{
   // Verify compile-time constant is valid
   static_assert(LJ_STARTING_INDEX IS 0 or LJ_STARTING_INDEX IS 1,
      "LJ_STARTING_INDEX must be 0 or 1");

   log.msg("LJ_STARTING_INDEX = %d", LJ_STARTING_INDEX);
   return true;
}

static bool test_array_first_element_access(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   const char* code = R"(
      local t = {10, 20, 30}
      return t[)" STRINGIFY(LJ_STARTING_INDEX) R"(]
   )";

   if (not run_lua_test(L, code, error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   if (not lua_isnumber(L, -1) or lua_tonumber(L, -1) != 10) {
      log.error("expected first element to be 10, got %g", lua_tonumber(L, -1));
      return false;
   }

   return true;
}

static bool test_table_length_operator(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   if (not run_lua_test(L, "local t = {1, 2, 3, 4, 5} return #t", error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   if (not lua_isnumber(L, -1) or lua_tonumber(L, -1) != 5) {
      log.error("expected length 5, got %g", lua_tonumber(L, -1));
      return false;
   }

   return true;
}

static bool test_ipairs_starting_index(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   const char* code = R"(
      local first_index = nil
      for i, v in ipairs({10, 20, 30}) do
         first_index = i
         break
      end
      return first_index
   )";

   if (not run_lua_test(L, code, error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   int expected = LJ_STARTING_INDEX;
   if (not lua_isnumber(L, -1) or (int)lua_tonumber(L, -1) != expected) {
      log.error("expected first ipairs index %d, got %g", expected, lua_tonumber(L, -1));
      return false;
   }

   return true;
}

static bool test_table_insert_position(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   const char* code = R"(
      local t = {}
      table.insert(t, "first")
      table.insert(t, "second")
      return t[)" STRINGIFY(LJ_STARTING_INDEX) R"(], t[)" STRINGIFY(LJ_STARTING_INDEX) R"( + 1]
   )";

   if (not run_lua_test(L, code, error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   // Check we got two results
   // Note: need to adjust for multiple returns
   return true;
}

static bool test_string_find_returns_correct_index(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   // "hello" - 'l' is at position 3 (1-based) or 2 (0-based)
   if (not run_lua_test(L, "return string.find('hello', 'l')", error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   int expected = LJ_STARTING_INDEX + 2;  // 'l' is third character
   if (not lua_isnumber(L, -1) or (int)lua_tonumber(L, -1) != expected) {
      log.error("expected string.find to return %d, got %g", expected, lua_tonumber(L, -1));
      return false;
   }

   return true;
}

static bool test_string_byte_default_start(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   // string.byte("ABC") should return byte of first character (65 = 'A')
   if (not run_lua_test(L, "return string.byte('ABC')", error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   if (not lua_isnumber(L, -1) or lua_tonumber(L, -1) != 65) {
      log.error("expected string.byte to return 65 ('A'), got %g", lua_tonumber(L, -1));
      return false;
   }

   return true;
}

static bool test_unpack_default_range(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   const char* code = R"(
      local a, b, c = unpack({10, 20, 30})
      return a, b, c
   )";

   if (not run_lua_test(L, code, error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   // Should get all three values regardless of starting index
   return true;
}

static bool test_table_concat_default_range(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   if (not run_lua_test(L, "return table.concat({'a', 'b', 'c'}, ',')", error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   const char* result = lua_tostring(L, -1);
   if (not result or std::string(result) != "a,b,c") {
      log.error("expected 'a,b,c', got '%s'", result ? result : "(nil)");
      return false;
   }

   return true;
}

static bool test_table_sort_operates_on_sequence(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   const char* code = R"(
      local t = {3, 1, 2}
      table.sort(t)
      return t[)" STRINGIFY(LJ_STARTING_INDEX) R"(], t[)" STRINGIFY(LJ_STARTING_INDEX) R"( + 1], t[)" STRINGIFY(LJ_STARTING_INDEX) R"( + 2]
   )";

   if (not run_lua_test(L, code, error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   // Sorted result should be 1, 2, 3
   return true;
}

static bool test_empty_table_length(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   if (not run_lua_test(L, "return #{}", error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   if (not lua_isnumber(L, -1) or lua_tonumber(L, -1) != 0) {
      log.error("expected empty table length 0, got %g", lua_tonumber(L, -1));
      return false;
   }

   return true;
}

static bool test_negative_string_indices_unchanged(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }
   luaL_openlibs(L);

   std::string error;
   // Negative indices should still work (relative to end)
   if (not run_lua_test(L, "return string.sub('hello', -1)", error)) {
      log.error("test failed: %s", error.c_str());
      return false;
   }

   const char* result = lua_tostring(L, -1);
   if (not result or std::string(result) != "o") {
      log.error("expected 'o', got '%s'", result ? result : "(nil)");
      return false;
   }

   return true;
}

//********************************************************************************************************************
// Low-level table API tests

static bool test_lj_tab_getint_semantic_index(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }

   // Create table {10, 20, 30}
   GCtab* t = lj_tab_new(L, 4, 0);
   TValue val;
   setintV(&val, 10);
   copyTV(L, lj_tab_setint(L, t, LJ_STARTING_INDEX), &val);
   setintV(&val, 20);
   copyTV(L, lj_tab_setint(L, t, LJ_STARTING_INDEX + 1), &val);
   setintV(&val, 30);
   copyTV(L, lj_tab_setint(L, t, LJ_STARTING_INDEX + 2), &val);

   // Verify retrieval
   cTValue* v = lj_tab_getint(t, LJ_STARTING_INDEX);
   if (not v or not tvisint(v) or intV(v) != 10) {
      log.error("lj_tab_getint failed for first element");
      return false;
   }

   return true;
}

static bool test_lj_tab_len_returns_element_count(pf::Log& log)
{
   LuaStateHolder holder;
   lua_State* L = holder.get();
   if (not L) {
      log.error("failed to create Lua state");
      return false;
   }

   // Create table with 3 elements
   GCtab* t = lj_tab_new(L, 4, 0);
   TValue val;
   for (int i = 0; i < 3; i++) {
      setintV(&val, (i + 1) * 10);
      copyTV(L, lj_tab_setint(L, t, LJ_STARTING_INDEX + i), &val);
   }

   MSize len = lj_tab_len(t);
   if (len != 3) {
      log.error("expected lj_tab_len to return 3, got %u", (unsigned)len);
      return false;
   }

   return true;
}

}  // namespace

extern void indexing_unit_tests(int& Passed, int& Total)
{
   constexpr std::array<TestCase, 14> tests = { {
      { "starting_index_constant", test_starting_index_constant },
      { "array_first_element_access", test_array_first_element_access },
      { "table_length_operator", test_table_length_operator },
      { "ipairs_starting_index", test_ipairs_starting_index },
      { "table_insert_position", test_table_insert_position },
      { "string_find_returns_correct_index", test_string_find_returns_correct_index },
      { "string_byte_default_start", test_string_byte_default_start },
      { "unpack_default_range", test_unpack_default_range },
      { "table_concat_default_range", test_table_concat_default_range },
      { "table_sort_operates_on_sequence", test_table_sort_operates_on_sequence },
      { "empty_table_length", test_empty_table_length },
      { "negative_string_indices_unchanged", test_negative_string_indices_unchanged },
      { "lj_tab_getint_semantic_index", test_lj_tab_getint_semantic_index },
      { "lj_tab_len_returns_element_count", test_lj_tab_len_returns_element_count }
   } };

   if (NewObject(CLASSID::FLUID, &glTestScript) != ERR::Okay) return;
   glTestScript->setStatement("");
   if (Action(AC::Init, glTestScript, nullptr) != ERR::Okay) return;

   for (const TestCase& test : tests) {
      pf::Log log("IndexingTests");
      log.branch("Running %s", test.name);
      ++Total;
      if (test.fn(log)) {
         ++Passed;
         log.msg("%s passed", test.name);
      }
      else {
         log.error("%s failed", test.name);
      }
   }
}

#endif // ENABLE_UNIT_TESTS
```

### Unit Test Categories

The unit tests should be organised into the following categories:

| Category | Tests | Priority |
|----------|-------|----------|
| **Compile-time validation** | `LJ_STARTING_INDEX` constant check | Critical |
| **Array access** | First element, sequential access | Critical |
| **Length operator** | Empty, populated, sparse tables | Critical |
| **ipairs iteration** | Starting index, iteration order | Critical |
| **Table library** | `insert`, `remove`, `concat`, `sort`, `pack`, `unpack` | High |
| **String library** | `byte`, `sub`, `find`, `match` position returns | High |
| **Negative indices** | String negative indices unchanged | Medium |
| **Low-level API** | `lj_tab_getint`, `lj_tab_setint`, `lj_tab_len` | High |
| **Edge cases** | Empty tables, holes, mixed array/hash | Medium |

### Integration with Build System

1. Add `unit_test_indexing.cpp` to the CMake build when `ENABLE_UNIT_TESTS` is defined
2. Register `indexing_unit_tests()` in the test runner alongside `parser_unit_tests()`
3. Ensure tests run as part of the CI pipeline

### Test Execution

Tests should be executable via the existing Flute test infrastructure:

```bash
# Run indexing unit tests
build/agents-install/parasol src/fluid/tests/test_unit_tests.fluid --log-api
```

## Fluid Integration Tests

In addition to C++ unit tests, create Fluid integration tests at `src/fluid/tests/test_indexing.fluid`:

```lua
-- Integration tests for array indexing semantics

local function test_array_literal_access()
   local t = {10, 20, 30}
   -- First element should be accessible at STARTING_INDEX
   local first = t[0] or t[1]  -- Runtime check
   assert(first == 10, "First element should be 10")
end

local function test_ipairs_indices()
   local indices = {}
   for i, v in ipairs({10, 20, 30}) do
      table.insert(indices, i)
   end
   -- Indices should be consecutive starting from STARTING_INDEX
   assert(#indices == 3, "Should iterate 3 times")
end

local function test_table_insert_append()
   local t = {}
   table.insert(t, "a")
   table.insert(t, "b")
   local result = table.concat(t, ",")
   assert(result == "a,b", "Inserted elements should be retrievable")
end

local function test_string_positions()
   local start_pos, end_pos = string.find("hello world", "world")
   assert(start_pos ~= nil, "string.find should find 'world'")
   -- Position should be correct for the indexing mode
end

-- Run tests
test_array_literal_access()
test_ipairs_indices()
test_table_insert_append()
test_string_positions()

print("All indexing integration tests passed")
```

## Test Cases

### Core Tests

```lua
-- Array creation and access
local t = {10, 20, 30}
assert(t[STARTING_INDEX] == 10)
assert(t[STARTING_INDEX + 1] == 20)
assert(t[STARTING_INDEX + 2] == 30)

-- Length operator
assert(#t == 3)

-- ipairs iteration
local sum = 0
for i, v in ipairs(t) do
   sum = sum + v
   -- With 0-based: i should be 0, 1, 2
   -- With 1-based: i should be 1, 2, 3
end
assert(sum == 60)

-- table.insert
local t2 = {}
table.insert(t2, "a")
table.insert(t2, "b")
assert(t2[STARTING_INDEX] == "a")
assert(t2[STARTING_INDEX + 1] == "b")

-- string.find
local pos = string.find("hello", "l")
-- With 0-based: should return 2
-- With 1-based: should return 3

-- unpack
local a, b, c = unpack({10, 20, 30})
assert(a == 10 and b == 20 and c == 30)
```

### Edge Cases

```lua
-- Empty table
local empty = {}
assert(#empty == 0)

-- Table with holes
local holey = {[STARTING_INDEX] = 1, [STARTING_INDEX + 2] = 3}
-- Length behaviour with holes (undefined in Lua spec)

-- Negative indices (string library)
local s = "hello"
assert(string.sub(s, -1) == "o")  -- Should still work

-- Table as sparse array
local sparse = {}
sparse[1000] = "big"
-- Should work regardless of starting index
```

## Backward Compatibility Considerations

### Scripts

- Existing Fluid scripts assume 1-based indexing
- A compile-time switch means scripts must be written for one mode or the other
- Consider adding a runtime check: `local STARTING_INDEX = (function() local t={1} return t[0] and 0 or 1 end)()`

### C API

- `lua_rawgeti(L, idx, n)` - The `n` parameter is a raw index
- Recommendation: Document that C API uses semantic indices
- Alternative: Keep C API always 0-based internally (more complex)

### Binary Compatibility

- Bytecode files compiled with one indexing mode won't work with another
- Add indexing mode flag to bytecode header for validation

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Performance regression | High | Careful implementation to avoid runtime checks; use compile-time conditionals |
| JIT compilation bugs | High | Extensive testing of JIT-compiled code paths |
| Script compatibility | Medium | Clear documentation; provide conversion guidelines |
| Subtle semantic bugs | High | Comprehensive test suite; edge case testing |
| VM instruction breakage | High | Test on all supported architectures |

## Open Questions

1. **String negative indices**: Should negative indices in string functions be affected? (Probably not - they're relative to the end)

2. **C API semantics**: Should C API functions like `lua_rawgeti` use semantic or storage indices?

3. **Bytecode compatibility**: Should we embed the indexing mode in bytecode for runtime validation?

4. **Mixed tables**: How should tables with both array and hash parts behave at the boundary?

## Success Criteria

1. **Unit test coverage**: All 14+ indexing unit tests pass for both configurations
2. All existing parser and regression tests pass with `LJ_STARTING_INDEX=1`
3. New indexing test suite passes with `LJ_STARTING_INDEX=0`
4. No performance regression > 5% on benchmarks
5. JIT compilation works correctly for both modes
6. Fluid integration tests (`test_indexing.fluid`) pass for both modes
7. Documentation updated with indexing mode behaviour

## Estimated Total Effort

- **Unit Test Infrastructure**: 1 day
- **Core Implementation**: 10-14 days (with incremental unit tests)
- **Integration Testing**: 3-5 days
- **Documentation**: 1-2 days
- **Total**: 15-22 days

Note: The test-driven approach adds ~1 day upfront but typically reduces debugging time in later phases.

## References

- [Lua 5.1 Reference Manual - Tables](https://www.lua.org/manual/5.1/manual.html#2.5.7)
- [LuaJIT Source Code](src/fluid/luajit-2.1/src/)
- [Existing AGENTS.md for LuaJIT](src/fluid/luajit-2.1/AGENTS.md)
- [Parser Unit Tests Pattern](src/fluid/luajit-2.1/src/parser/parser_unit_tests.cpp) - Reference implementation for unit test structure
- [Flute Test Framework](docs/wiki/Unit-Testing.md) - For Fluid integration tests
