# Fluid DebugLog Enhancement Plan

**Status:** Phase 1 Complete - Phase 2 Pending
**Priority:** Medium
**Estimated Effort:** 2-4 hours for Phase 1, 4-6 hours for Phase 2
**Target:** Fluid module (src/fluid/fluid_class.cpp)

## Executive Summary

The Fluid class's `DebugLog()` method provides runtime introspection capabilities for debugging Fluid scripts. This plan outlines enhancements to leverage LuaJIT's built-in analytical functions to provide more comprehensive, efficient, and useful debugging information.

## Current Implementation Status

### ✅ Completed (Working)
- Basic stack traces with function names and line numbers
- Local variables inspection with type information
- Upvalues (closure variables) inspection
- Global variables inspection (fixed to access Fluid's protected globals storage table)
- Memory statistics (Lua heap usage)
- State information (stack top, protected globals status)
- Multiple output options (stack, locals, upvalues, globals, memory, state, all)
- Compact mode for condensed output
- String truncation (40 char limit)
- Various data types display (nil, boolean, number, string, table, function)

### 🔧 Current Limitations
1. **Stack traces** are manually formatted - inefficient and verbose code
2. **No function metadata** - missing parameter counts, bytecode size, complexity metrics
3. **Generic variable names** - doesn't use semantic slot names
4. **No bytecode position info** - can't correlate with profiling data
5. **Limited C function info** - only shows `<function>` for C functions
6. **No closure relationship analysis** - upvalue chains not visualized
7. **No performance hints** - can't identify problematic patterns

## Available LuaJIT Analytical Functions

### From lj_debug.c (C API - Directly Accessible)

| Function | Purpose | Usefulness |
|----------|---------|------------|
| `lj_debug_frame()` | Get frame at stack level | ⭐⭐⭐ Already using via lua_getstack |
| `lj_debug_line()` | Get line for bytecode position | ⭐⭐⭐⭐ For bytecode correlation |
| `lj_debug_funcname()` | Extract function name | ⭐⭐⭐ Already using via lua_getinfo |
| `lj_debug_dumpstack()` | **Format stack with custom fmt** | ⭐⭐⭐⭐⭐ **HIGH VALUE** |
| `lj_debug_getinfo()` | Extended debug info | ⭐⭐⭐⭐ More fields than lua_getinfo |
| `lj_debug_slotname()` | Get semantic slot names | ⭐⭐⭐⭐ Better variable names |
| `lj_debug_uvname()` | Get upvalue names | ⭐⭐⭐ Already accessible |
| `lj_debug_pushloc()` | Push source location | ⭐⭐⭐ For location formatting |

### From lib_jit.c (jit.util module - NOT Accessible)

The `jit.util` module provides powerful reflection APIs but is **not accessible** in Fluid due to module naming restrictions (only alphanumeric names permitted). These functions include:
- `funcinfo()` - Function metadata (params, bytecodes, upvalues)
- `funcbc()` - Bytecode inspection
- `funck()` - Function constants
- `traceinfo()` - JIT trace information

**Workaround:** Access the underlying C functions directly or via GCproto structures.

### From Standard debug Library (Accessible)

All standard Lua `debug.*` functions are available:
- `debug.getinfo()` - Stack frame information
- `debug.getlocal()` - Local variables
- `debug.getupvalue()` - Upvalues
- `debug.traceback()` - Stack traceback
- And others...

## Enhancement Plan

### Phase 1: Core Improvements (High Priority)

**Completion:** ✅ Implemented 2025-11-10

**Highlights:**
- Stack traces now rely on LuaJIT's native dumpstack helper (`luaJIT_profile_dumpstack`) with compact/non-compact formatting parity.
- Added the `funcinfo` option using LuaJIT proto metadata to expose Lua function details (parameters, stack slots, bytecodes, constants, and upvalues).

#### 1.1 Replace Manual Stack Formatting with lj_debug_dumpstack()

**Current Code:** 145 lines of manual stack formatting (lines 620-658 in fluid_class.cpp)

**Benefits:**
- Reduce code by ~100 lines
- More robust edge case handling
- Consistent formatting
- Better performance

**Implementation:**
```cpp
if (show_stack) {
   if (not compact) buf << "=== CALL STACK ===\n";

   // Use LuaJIT's optimized stack dumper
   SBuf sb;
   lj_buf_init(prv->Lua, &sb);
   lj_debug_dumpstack(prv->Lua, &sb, compact ? "F\n" : "Fl\n", 50);
   buf << std::string_view((const char*)sb.b, sbuflen(&sb));
   lj_buf_free(G(prv->Lua), &sb);

   if (not compact) buf << "\n";
}
```

**Format String Options:**
- `'F'` - Full module:name for functions
- `'f'` - Function name only
- `'l'` - module:line location
- `'p'` - Preserve full paths (no stripping)
- `'Z'` - Zap trailing separator

**Testing:** All existing stack trace tests should pass unchanged.

#### 1.2 Add Function Metadata via lj_debug_getinfo() Extended Mode

**New Option:** `funcinfo`

**Output Example:**
```
=== FUNCTION INFORMATION ===
Function: calculateTotal (example.lua:42-78)
  Parameters: 3
  Vararg: false
  Stack slots: 12
  Bytecodes: 156
  Constants: 8 (numeric), 4 (objects)
  Upvalues: 2
  Source: example.fluid
```

**Implementation:**
```cpp
if (show_funcinfo) {
   if (not compact) buf << "=== FUNCTION INFORMATION ===\n";

   lua_Debug ar;
   if (lua_getstack(prv->Lua, 1, &ar)) {
      if (lj_debug_getinfo(prv->Lua, "nSluf", &ar, 1)) {  // Extended mode = 1
         GCfunc *fn = frame_func(lj_debug_frame(prv->Lua, 1, NULL));
         if (isluafunc(fn)) {
            GCproto *pt = funcproto(fn);
            buf << "Function: ";
            if (ar.name) buf << ar.name;
            else buf << "<anonymous>";
            buf << " (" << ar.short_src << ":" << ar.linedefined
                << "-" << (ar.linedefined + pt->numline) << ")\n";
            buf << "  Parameters: " << (int)pt->numparams << "\n";
            buf << "  Vararg: " << ((pt->flags & PROTO_VARARG) ? "true" : "false") << "\n";
            buf << "  Stack slots: " << (int)pt->framesize << "\n";
            buf << "  Bytecodes: " << (int)pt->sizebc << "\n";
            buf << "  Constants: " << (int)pt->sizekn << " (numeric), "
                << (int)pt->sizekgc << " (objects)\n";
            buf << "  Upvalues: " << (int)pt->sizeuv << "\n";
         }
      }
   }
   if (not compact) buf << "\n";
}
```

**New Test Required:** `testFuncInfo()` in test_debuglog.fluid

#### 1.3 Enhance Variable Names with lj_debug_slotname()

**Current:** Shows variable names from bytecode (which are correct)

**Enhancement:** Add semantic information for special variables

**Implementation:**
```cpp
// In locals display loop
while ((name = lua_getlocal(prv->Lua, &ar, idx))) {
   // Get semantic slot name if available
   const char *slot_name = lj_debug_slotname(pt, frame_pc(nextframe), idx-1);

   if (compact) buf << name;
   else {
      buf << name;
      if (slot_name and strcmp(slot_name, name) != 0) {
         buf << " [" << slot_name << "]";  // e.g., "i [iterator]"
      }
   }
   buf << " = ";
   // ... rest of formatting ...
}
```

**Benefit:** Shows semantic context like "iterator", "loop counter", etc.

#### 1.4 Add Bytecode Position Information

**New Option:** `bytecode` (or add to existing `stack` option)

**Output Example:**
```
[1] calculateTotal (example.lua:55 @bc:42) - Lua function
[2] processOrder (example.lua:120 @bc:89) - Lua function
```

**Implementation:**
```cpp
// In stack trace display
if (isluafunc(fn)) {
   GCproto *pt = funcproto(fn);
   BCPos pc = debug_framepc(prv->Lua, fn, nextframe);  // Internal function

   buf << " (" << ar.short_src << ":" << ar.currentline;
   if (pc != NO_BCPOS) {
      buf << " @bc:" << pc;
   }
   buf << ")";
}
```

**Use Case:** Correlating with profiling tools, understanding execution flow.

### Phase 2: Advanced Features (Medium Priority)

#### 2.1 Closure Relationship Analysis

**New Option:** `closures`

**Output Example:**
```
=== CLOSURE ANALYSIS ===
[Level 1] calculateTotal
  Upvalues: taxRate (number), config (table)

[Level 2] formatResult
  Upvalues: precision (number), format (function)
  Closes over: calculateTotal.config

Chain: formatResult → calculateTotal → <global>
```

**Implementation:**
- Track upvalue references across stack frames
- Build closure chain from inner to outer
- Show which upvalues are shared between functions

#### 2.2 C Function Enhanced Information

**Enhancement:** Better display for C functions

**Current:** `<function>`

**Enhanced:**
```cpp
if (iscfunc(fn)) {
   buf << "<C function";
   if (isffunc(fn)) {
      buf << " builtin#" << fn->c.ffid;
   } else {
      buf << " @" << (void*)fn->c.f;
   }
   buf << ">";
}
```

**Output Example:**
- `<C function builtin#42>` for fast functions
- `<C function @0x7fff1234abcd>` for regular C functions

#### 2.3 Performance Hints

**New Option:** `hints`

**Output Example:**
```
=== PERFORMANCE HINTS ===
⚠️ Deep call stack (18 levels) - consider refactoring
⚠️ Large function at example.lua:42 (512 bytecodes)
ℹ️ Hot path: calculateTotal called from 3 different locations
```

**Detection Rules:**
- Stack depth > 15: Deep recursion warning
- Function bytecode size > 256: Large function warning
- Upvalue count > 8: Excessive closure warning

#### 2.4 Coroutine State Information

**Enhancement to `state` option:**

```cpp
if (show_state) {
   // ... existing state info ...

   // Add coroutine info
   if (lua_isyieldable(prv->Lua)) {
      buf << "Coroutine: yieldable\n";
   } else {
      buf << "Coroutine: main thread\n";
   }
}
```

### Phase 3: Advanced Debugging Features (Medium-Low Priority)

#### 3.1 Bytecode Disassembly

**New Option:** `disasm`

**Implementation:** Custom bytecode disassembler using LuaJIT internals

**Rationale:**
- `lj_bcwrite()` dumps binary bytecode (not human-readable)
- `jit.bc` module provides nice disassembly but requires `require()` and `package` global (not available in Fluid)
- `jit.util` module functions are also inaccessible for same reason
- **Solution:** Build custom disassembler directly in C++ using available LuaJIT APIs

**Available Functions:**
- `proto_bc(pt)` - Get bytecode instruction array
- `lj_bc_mode[]` - Instruction format modes
- `lj_bc_names` - Opcode name strings
- `lj_debug_line()` - Map bytecode PC to source line
- `bc_op()`, `bc_a()`, `bc_d()` - Decode instruction fields

**Implementation:**
```cpp
if (show_disasm) {
   lua_Debug ar;
   if (lua_getstack(prv->Lua, 1, &ar)) {
      lua_getinfo(prv->Lua, "f", &ar);
      GCfunc *fn = funcV(prv->Lua->top - 1);
      prv->Lua->top--;

      if (isluafunc(fn)) {
         GCproto *pt = funcproto(fn);
         if (not compact) buf << "=== BYTECODE DISASSEMBLY ===\n";

         buf << "Function: " << ar.short_src << ":" << ar.linedefined << "\n";
         buf << "Bytecodes: " << pt->sizebc << ", Constants: "
             << pt->sizekn << " (num), " << pt->sizekgc << " (gc)\n\n";

         // Disassemble each instruction
         const BCIns *bc = proto_bc(pt);
         for (BCPos pc = 0; pc < pt->sizebc; pc++) {
            BCIns ins = bc[pc];
            BCOp op = bc_op(ins);

            // Format: [PC] OPCODE  A D  ; line
            if (compact) {
               buf << std::setw(4) << std::setfill('0') << pc << " ";
            } else {
               buf << "[" << std::setw(4) << std::setfill('0') << pc << "] ";
            }

            // Opcode name (from lj_bc.h: bc_op extracts opcode)
            const char *op_name = lj_bc_names + (op * 6);  // Names are 6 chars each
            buf << std::string_view(op_name, 6) << " ";

            // Operands
            int a = bc_a(ins);
            int d = bc_d(ins);

            if (not compact) {
               buf << "A=" << std::setw(3) << a << " D=" << std::setw(5) << d;
            } else {
               buf << a << " " << d;
            }

            // Show source line correlation
            BCLine line = lj_debug_line(pt, pc);
            if (line >= 0) {
               buf << (compact ? " ;" : "  ; line ") << line;
            }

            buf << "\n";
         }
         buf << "\n";
      } else {
         buf << "Cannot disassemble: not a Lua function\n\n";
      }
   }
}
```

**Output Example:**
```
=== BYTECODE DISASSEMBLY ===
Function: example.lua:42
Bytecodes: 12, Constants: 3 (num), 2 (gc)

[0000] GGET   A=  0 D=    1  ; line 43
[0001] TGETV  A=  0 D=    2  ; line 43
[0002] ADDVN  A=  0 D=    3  ; line 43
[0003] MULVN  A=  0 D=    4  ; line 43
[0004] RET1   A=  0 D=    2  ; line 43
...
```

**Compact Output:**
```
0000 GGET   0 1 ;43
0001 TGETV  0 2 ;43
0002 ADDVN  0 3 ;43
0003 MULVN  0 4 ;43
0004 RET1   0 2 ;43
```

**Enhanced Version (with constant decoding):**
Could be extended to show constant values inline:
```cpp
// After showing operands, decode constants
uint16_t mode = lj_bc_mode[op];
if ((mode & BCMstr) and d < pt->sizekgc) {
   GCobj *kgc = proto_kgc(pt, d);
   if (kgc->gch.gct == ~LJ_TSTR) {
      GCstr *str = gco2str(kgc);
      buf << "  ; \"" << std::string_view(strdata(str),
                       std::min(str->len, (MSize)20)) << "\"";
   }
}
```

**Use Cases:**
- Understanding compilation of complex expressions
- Debugging closure creation and upvalue handling
- Performance analysis (identifying expensive operations)
- Educational (learning how Lua compiles to bytecode)

**Required Headers:**
```cpp
#include "lj_obj.h"     // For GCproto, BCIns, etc.
#include "lj_bc.h"      // For bc_op, bc_a, bc_d, lj_bc_mode, lj_bc_names
#include "lj_debug.h"   // For lj_debug_line
```

**New Test Required:** `testDisassembly()` in test_debuglog.fluid

#### 3.2 Binary Bytecode Dump

**New Option:** `dump` (optional, lower priority than `disasm`)

**Implementation:** Use `lj_bcwrite()` to dump binary bytecode

**Use Case:** Exporting bytecode for external analysis tools

**Note:** Less useful than `disasm` for interactive debugging, but useful for:
- Saving compiled bytecode to file
- Analyzing with external tools
- Binary diffing between versions

#### 3.3 JIT Trace Information

**New Option:** `jit` (conditional on JIT being active)

**Implementation:** Check if function has been JIT-compiled, show trace info

**Example:**
```cpp
#if LJ_HASJIT
if (show_jit and isluafunc(fn)) {
   jit_State *J = L2J(prv->Lua);
   GCproto *pt = funcproto(fn);

   buf << "=== JIT INFORMATION ===\n";
   buf << "JIT status: " << ((J->flags & JIT_F_ON) ? "enabled" : "disabled") << "\n";

   // Check if function has been compiled
   // (This requires accessing trace information, which is complex)
   buf << "Function compilation: " << (pt->flags & PROTO_ILOOP ? "contains loops" : "no loops") << "\n";
}
#endif
```

**Use Case:** Performance optimization, understanding JIT behavior

**Note:** Complex to implement properly; requires deep JIT state inspection

## Implementation Strategy

### Step 1: Refactor Existing Code
1. Add necessary LuaJIT headers (`lj_debug.h`, `lj_buf.h`, `lj_bc.h`)
2. Create helper functions for common operations
3. Ensure backward compatibility with all existing tests

### Step 2: Implement Phase 1 Features
1. Replace stack formatting with `lj_debug_dumpstack()` (1.1)
2. Add function metadata option (1.2)
3. Enhance variable names (1.3)
4. Add bytecode positions (1.4)
5. Write comprehensive tests for each feature

### Step 3: Document New Features
1. Update embedded documentation in fluid_class.cpp
2. Add examples to test_debuglog.fluid
3. Update Fluid reference manual if needed

### Step 4: Implement Phase 2 (Optional)
- Based on user feedback and actual debugging needs
- Can be done incrementally

### Step 5: Implement Phase 3 (Advanced Features)
1. Implement bytecode disassembler (3.1)
2. Add constant value decoding for enhanced output
3. Optional: Add binary dump capability (3.2)
4. Optional: Add JIT trace info (3.3)
5. Write tests for disassembly functionality

## Testing Strategy

### Existing Tests (Must Pass)
All 12 current tests in `test_debuglog.fluid` must continue to pass:
- ✅ testDefaultStackTrace
- ✅ testLocals
- ✅ testUpvalues
- ✅ testGlobals
- ✅ testMemory
- ✅ testState
- ✅ testMultipleOptions
- ✅ testAllOption
- ✅ testCompactMode
- ✅ testLongString
- ✅ testVariousTypes
- ✅ testNestedStack

### New Tests Required

#### For Phase 1:
- `testFuncInfo()` - Verify function metadata display
- `testBytecodePositions()` - Verify BC position in stack traces
- `testEnhancedVarNames()` - Verify semantic slot names (if different from variable names)
- `testStackDumpQuality()` - Verify improved stack formatting

#### For Phase 2:
- `testClosureAnalysis()` - Verify closure chain tracking
- `testCFunctionInfo()` - Verify C function display
- `testPerformanceHints()` - Verify warning detection
- `testCoroutineState()` - Verify coroutine info display

#### For Phase 3:
- `testDisassembly()` - Verify bytecode disassembly output
- `testDisassemblyFormat()` - Verify instruction formatting and line correlation
- `testDisassemblyCompact()` - Verify compact disassembly mode
- `testDisassemblyWithConstants()` - Verify constant value display (if implemented)

## Code Structure Changes

### New Includes Required
```cpp
#include "lj_debug.h"   // For lj_debug_dumpstack, lj_debug_getinfo, etc.
#include "lj_buf.h"     // For SBuf (string buffer) operations
#include "lj_bc.h"      // For bytecode definitions (BCPos, NO_BCPOS)
#include "lj_obj.h"     // For GCproto, GCfunc structures
```

### New Option Flags
```cpp
bool show_funcinfo = false;    // Function metadata
bool show_closures = false;    // Closure relationships
bool show_hints = false;       // Performance hints
bool show_disasm = false;      // Bytecode disassembly
bool show_jit = false;         // JIT trace information
// bytecode positions integrated into show_stack
```

### Helper Functions to Add
```cpp
static void format_function_info(std::ostringstream &buf, GCproto *pt, const char *name, const char *source, bool compact);
static void analyze_closure_chain(std::ostringstream &buf, lua_State *L, int max_depth, bool compact);
static void check_performance_hints(std::ostringstream &buf, lua_State *L, GCproto *pt, int stack_depth);
static void disassemble_bytecode(std::ostringstream &buf, GCproto *pt, lua_Debug *ar, bool compact);
static const char* decode_constant(GCproto *pt, BCOp op, int d, char *temp_buf, size_t buf_size);
```

## Risks and Mitigations

### Risk 1: Breaking Existing Functionality
**Mitigation:** Comprehensive test suite, incremental changes, backward compatibility

### Risk 2: Performance Overhead
**Mitigation:** Only compute expensive info when explicitly requested, use LuaJIT's optimized functions

### Risk 3: Platform-Specific Issues
**Mitigation:** Test on Windows, Linux, and macOS builds

### Risk 4: LuaJIT Internal API Changes
**Mitigation:** Document LuaJIT version dependency, use stable API functions only

## Success Criteria

### Phase 1 Complete When:
- ✅ All 12 existing tests pass
- ✅ New tests for Phase 1 features pass
- ✅ Code is cleaner and more maintainable (< 800 lines vs current ~850)
- ✅ Stack traces use lj_debug_dumpstack()
- ✅ Function metadata is available via `funcinfo` option
- ✅ Documentation updated

### Phase 2 Complete When:
- ✅ Closure analysis implemented and tested
- ✅ C function info enhanced
- ✅ Performance hints working
- ✅ User feedback positive

## Timeline Estimate

- **Phase 1.1** (Stack refactor): 2 hours
- **Phase 1.2** (Function metadata): 2 hours
- **Phase 1.3** (Variable names): 1 hour
- **Phase 1.4** (Bytecode positions): 1 hour
- **Testing & Documentation**: 2 hours
- **Total Phase 1**: 8 hours

- **Phase 2** (Advanced analysis): 4-6 hours (if needed)

- **Phase 3.1** (Bytecode disassembly): 3-4 hours
  - Basic disassembler: 2 hours
  - Constant decoding: 1 hour
  - Testing: 1 hour
- **Phase 3.2** (Binary dump): 1 hour (optional)
- **Phase 3.3** (JIT info): 2 hours (optional)
- **Total Phase 3**: 3-7 hours (depending on features implemented)

## References

- **LuaJIT Source:** `src/fluid/luajit-2.1/src/lj_debug.c`, `lib_jit.c`, `lj_bc.h`
- **Current Implementation:** `src/fluid/fluid_class.cpp` lines 568-850
- **Tests:** `src/fluid/tests/test_debuglog.fluid`
- **LuaJIT Documentation:** http://luajit.org/ext_jit.html

## Appendix A: Format String Reference for lj_debug_dumpstack()

| Char | Meaning | Example Output |
|------|---------|----------------|
| `F` | Full module:name | `example.fluid:calculateTotal` |
| `f` | Function name only | `calculateTotal` |
| `l` | module:line | `example.fluid:42` |
| `p` | Preserve full paths | `/full/path/to/file.fluid` |
| `Z` | Zap trailing separator | (removes last separator) |
| Other | Literal character | Any other char printed as-is |

**Common Patterns:**
- `"Fl\n"` - Module:line functionname (one per line)
- `"F\n"` - Full module:name (one per line)
- `"f l\n"` - Function name, then module:line
- `"pFl\n"` - Same as Fl but with full paths

## Appendix B: Bytecode Instruction Access

### Accessing Bytecode Instructions

```cpp
#include "lj_obj.h"    // For GCproto, BCIns types
#include "lj_bc.h"     // For bytecode operations

// Get prototype from function
GCfunc *fn = funcV(L->top - 1);
GCproto *pt = funcproto(fn);

// Access bytecode array
const BCIns *bc = proto_bc(pt);
BCPos pc = 0;
BCIns ins = bc[pc];

// Decode instruction
BCOp op = bc_op(ins);     // Extract opcode
int a = bc_a(ins);         // Extract A operand (8 bits)
int d = bc_d(ins);         // Extract D operand (16 bits)
int b = bc_b(ins);         // Extract B operand (8 bits)
int c = bc_c(ins);         // Extract C operand (8 bits)
```

### Bytecode Instruction Format

LuaJIT bytecode instructions are 32-bit values with this format:

```
32-bit instruction: [C:8][B:8][A:8][OP:8]
                     ^     ^    ^    ^
                     |     |    |    +-- Opcode (0-255)
                     |     |    +------- A operand (0-255)
                     |     +------------ B operand (0-255)
                     +------------------ C operand (0-255)

Alternative view: [D:16][A:8][OP:8]
                   ^      ^    ^
                   |      |    +-- Opcode
                   |      +------- A operand
                   +-------------- D operand (16-bit, combines B and C)
```

### Opcode Name Table

```cpp
// From lj_bc.h - opcode names are packed as 6-character strings
extern const char lj_bc_names[];

// Get opcode name
BCOp op = bc_op(ins);
const char *op_name = lj_bc_names + (op * 6);  // Each name is 6 chars

// Example: "GGET  ", "TGETV ", "ADDVN ", etc.
std::string_view name(op_name, 6);
```

### Instruction Mode Table

```cpp
// From lj_bc.h - instruction format modes
extern const uint16_t lj_bc_mode[];

// Get mode for opcode
uint16_t mode = lj_bc_mode[op];

// Mode bits indicate operand types:
// BCMnone  - No operands
// BCMdst   - Has destination
// BCMbase  - Base register access
// BCMvar   - Variable slot access
// BCMjump  - Jump instruction (D is offset)
// BCMnum   - Numeric constant (D is index)
// BCMstr   - String constant (D is index)
// BCMuv    - Upvalue access
// BCMfunc  - Function constant
```

### Accessing Constants

```cpp
// Numeric constants
if (d >= 0 and d < pt->sizekn) {
   cTValue *knum = proto_knumtv(pt, d);
   lua_Number num = numV(knum);
}

// GC constants (strings, functions, tables)
if (d >= 0 and d < pt->sizekgc) {
   GCobj *kgc = proto_kgc(pt, d);
   if (kgc->gch.gct == ~LJ_TSTR) {
      GCstr *str = gco2str(kgc);
      const char *data = strdata(str);
      MSize len = str->len;
   }
}
```

### Example: Simple Disassembler

```cpp
void simple_disasm(GCproto *pt) {
   const BCIns *bc = proto_bc(pt);

   for (BCPos pc = 0; pc < pt->sizebc; pc++) {
      BCIns ins = bc[pc];
      BCOp op = bc_op(ins);

      const char *op_name = lj_bc_names + (op * 6);
      printf("[%04d] %.6s A=%d D=%d\n",
             pc, op_name, bc_a(ins), bc_d(ins));
   }
}
```

## Approval and Sign-off

**Author:** AI Analysis Session
**Date:** 2025-01-10 (Updated)
**Status:** Ready for Review
**Reviewer:** [Pending]
**Implementation Start:** [Pending Approval]

**Recent Updates:**
- Completed Phase 1 implementation (stack formatter replacement and function metadata option)
- Added Phase 3.1: Bytecode Disassembly with full implementation details
- Documented bytecode access methods and instruction format
- Added Appendix B with comprehensive bytecode API reference
- Explained why `jit.bc` and `jit.util` modules are not accessible in Fluid
- Provided custom disassembler implementation strategy using LuaJIT internals

---

**Next Steps:**
1. Review this plan with maintainer
2. Plan Phase 2 scope and obtain approval
3. Optionally review Phase 3 bytecode disassembly approach
4. Implement Phase 2 enhancements after validation feedback
5. Iterate based on feedback and user needs
