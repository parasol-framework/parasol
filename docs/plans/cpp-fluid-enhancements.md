# C++20 Enhancements for LuaJIT Source Code

## Overview
This plan outlines a phased approach to upgrade the LuaJIT source code in `src/fluid/luajit-2.1/src` to leverage modern C++20 features. The focus is on low-effort, high-impact changes that improve code clarity and maintainability without introducing breaking changes.

**Total files to upgrade:** 78 headers and C files
**Estimated effort:** Low to Medium (incremental, file-by-file approach)

---

## Phase 1: Simple Numeric Constants (HIGH IMPACT, LOW RISK)

### Goal
Convert simple numeric `#define` constants to `constexpr` declarations for type safety and scoping benefits.

### Files & Scope

#### `lj_def.h` (Priority: High)
- Convert limit constants:
  - `#define LJ_MAX_MEM32` → `constexpr uint32_t LJ_MAX_MEM32 = 0x7fffff00u;`
  - `#define LJ_MAX_MEM64` → `constexpr uint64_t LJ_MAX_MEM64 = ((uint64_t)1 << 47);`
  - `#define LJ_MAX_STRTAB` → `constexpr uint32_t LJ_MAX_STRTAB = (1u << 26);`
  - `#define LJ_MAX_HBITS` → `constexpr int LJ_MAX_HBITS = 26;`
  - `#define LJ_MAX_ABITS` → `constexpr int LJ_MAX_ABITS = 28;`
  - `#define LJ_MAX_ASIZE` → `constexpr uint32_t LJ_MAX_ASIZE = ((1u << (LJ_MAX_ABITS - 1)) + 1);`
  - `#define LJ_MAX_XLEVEL` → `constexpr int LJ_MAX_XLEVEL = 200;`
  - `#define LJ_MAX_BCINS` → `constexpr uint32_t LJ_MAX_BCINS = (1u << 26);`
  - `#define LJ_MAX_SLOTS` → `constexpr int LJ_MAX_SLOTS = 250;`
  - `#define LJ_MAX_LOCVAR` → `constexpr int LJ_MAX_LOCVAR = 200;`
  - `#define LJ_MAX_UPVAL` → `constexpr int LJ_MAX_UPVAL = 60;`
  - `#define LJ_MAX_IDXCHAIN` → `constexpr int LJ_MAX_IDXCHAIN = 100;`
  - `#define LJ_NUM_CBPAGE` → `constexpr int LJ_NUM_CBPAGE = 1;`
  - `#define LJ_MIN_GLOBAL` → `constexpr int LJ_MIN_GLOBAL = 6;`
  - `#define LJ_MIN_REGISTRY` → `constexpr int LJ_MIN_REGISTRY = 2;`
  - `#define LJ_MIN_STRTAB` → `constexpr int LJ_MIN_STRTAB = 256;`

#### `lua.h` (Priority: High)
- Convert version and status constants:
  - `#define LUA_VERSION_NUM 501` → `constexpr int LUA_VERSION_NUM = 501;`
  - `#define LUA_MULTRET (-1)` → `constexpr int LUA_MULTRET = -1;`
  - `#define LUA_REGISTRYINDEX (-10000)` → `constexpr int LUA_REGISTRYINDEX = -10000;`
  - `#define LUA_ENVIRONINDEX (-10001)` → `constexpr int LUA_ENVIRONINDEX = -10001;`
  - `#define LUA_GLOBALSINDEX (-10002)` → `constexpr int LUA_GLOBALSINDEX = -10002;`
  - Status codes: `LUA_OK`, `LUA_YIELD`, `LUA_ERRRUN`, `LUA_ERRSYNTAX`, `LUA_ERRMEM`, `LUA_ERRERR`
  - Type constants: `LUA_TNONE`, `LUA_TNIL`, `LUA_TBOOLEAN`, etc.

#### `luaconf.h` (Priority: Medium)
- Convert tunable constants:
  - `#define LUAI_MAXSTACK 65500` → `constexpr int LUAI_MAXSTACK = 65500;`
  - `#define LUAI_MAXCSTACK 8000` → `constexpr int LUAI_MAXCSTACK = 8000;`
  - `#define LUAI_GCPAUSE 200` → `constexpr int LUAI_GCPAUSE = 200;`
  - `#define LUAI_GCMUL 200` → `constexpr int LUAI_GCMUL = 200;`
  - `#define LUA_MAXCAPTURES 32` → `constexpr int LUA_MAXCAPTURES = 32;`
  - `#define LUA_MAXINPUT 512` → `constexpr int LUA_MAXINPUT = 512;`

#### `lj_arch.h` (Priority: Medium)
- Convert architecture and OS definitions:
  - `#define LUAJIT_LE 0` → `constexpr int LUAJIT_LE = 0;`
  - `#define LUAJIT_BE 1` → `constexpr int LUAJIT_BE = 1;`
  - Architecture constants (`LUAJIT_ARCH_X86`, `LUAJIT_ARCH_X64`, etc.)
  - OS constants (`LUAJIT_OS_WINDOWS`, `LUAJIT_OS_LINUX`, etc.)
  - Number mode constants (`LJ_NUMMODE_*`)

#### `lj_target.h` (Priority: Medium)
- Convert register/allocation constants:
  - `#define RID_NONE 0x80` → `constexpr uint8_t RID_NONE = 0x80;`
  - `#define RID_MASK 0x7f` → `constexpr uint8_t RID_MASK = 0x7f;`
  - `#define SPS_NONE 0` → `constexpr uint8_t SPS_NONE = 0;`

### Notes for Phase 1
- Keep macros that are used in conditional compilation directives (`#if`, `#ifdef`)
- Keep conditional defines that detect platform-specific constants
- These changes provide type safety and scoping benefits with minimal risk

---

## Phase 2: Type Aliases and Function Pointers (MEDIUM IMPACT, LOW RISK)

### Goal
Modernize type declarations using C++20 `using` syntax for cleaner, more readable declarations.

### Files & Scope

#### Simple Type Aliases
Convert throughout the codebase:
- `typedef uint32_t Reg;` → `using Reg = uint32_t;`
- `typedef uint32_t RegSP;` → `using RegSP = uint32_t;`
- `typedef uint64_t RegSet;` → `using RegSet = uint64_t;`
- `typedef uint32_t MSize;` → `using MSize = uint32_t;`
- `typedef uint32_t GCSize;` → `using GCSize = uint32_t;`
- `typedef uint32_t RegCost;` → `using RegCost = uint32_t;`
- `typedef uint16_t VarIndex;` → `using VarIndex = uint16_t;`
- `typedef intptr_t GPRArg;` → `using GPRArg = intptr_t;`

**Files affected:**
- `lj_target.h`
- `lj_obj.h`
- `lj_ccall.h`
- `parser/lj_parse_types.h`

#### Function Pointer Type Aliases
Convert in public/interface headers:

**`lua.h`:**
- `typedef int (*lua_CFunction) (lua_State *L);`
  → `using lua_CFunction = int(*)(lua_State *L);`
- `typedef const char * (*lua_Reader) (lua_State *L, void *ud, size_t *sz);`
  → `using lua_Reader = const char*(*)(lua_State *L, void *ud, size_t *sz);`
- `typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);`
  → `using lua_Writer = int(*)(lua_State *L, const void* p, size_t sz, void* ud);`
- `typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);`
  → `using lua_Alloc = void*(*)(void *ud, void *ptr, size_t osize, size_t nsize);`
- `typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);`
  → `using lua_Hook = void(*)(lua_State *L, lua_Debug *ar);`

**`lj_vm.h`:**
- `typedef TValue *(*lua_CPFunction)(lua_State *L, lua_CFunction func, void *ud);`
  → `using lua_CPFunction = TValue*(*)(lua_State *L, lua_CFunction func, void *ud);`

**`lj_obj.h`:**
- `typedef void (*ASMFunction)(void);`
  → `using ASMFunction = void(*)(void);`

**Other headers (MCLabel):**
- Various files have: `typedef MCode* MCLabel;`
  → `using MCLabel = MCode*;`

### Notes for Phase 2
- Test public API compatibility after changes
- Focus on user-facing headers first (`lua.h`, `lualib.h`)
- Internal implementation headers can be updated incrementally

---

## Phase 3: String Constants and Macro Functions (MEDIUM IMPACT, MEDIUM RISK)

### Goal
Upgrade string constants and simple macros for improved type safety and compile-time evaluation.

### Files & Scope

#### String Constants → constexpr std::string_view
**`lua.h` (public API strings):**
- `#define LUA_VERSION "Lua 5.1"` → `constexpr std::string_view LUA_VERSION = "Lua 5.1";`
- `#define LUA_RELEASE "Lua 5.1.4"` → `constexpr std::string_view LUA_RELEASE = "Lua 5.1.4";`
- `#define LUA_COPYRIGHT "Copyright (C) 1994-2008 Lua.org, PUC-Rio"`
  → `constexpr std::string_view LUA_COPYRIGHT = "Copyright (C) 1994-2008 Lua.org, PUC-Rio";`
- `#define LUA_AUTHORS "R. Ierusalimschy, L. H. de Figueiredo & W. Celes"`
  → `constexpr std::string_view LUA_AUTHORS = "R. Ierusalimschy, L. H. de Figueiredo & W. Celes";`
- `#define LUA_SIGNATURE "\033Lua"` → `constexpr std::string_view LUA_SIGNATURE = "\033Lua";`

**`lualib.h` (library names):**
- `#define LUA_COLIBNAME "coroutine"` → `constexpr std::string_view LUA_COLIBNAME = "coroutine";`
- `#define LUA_MATHLIBNAME "math"` → `constexpr std::string_view LUA_MATHLIBNAME = "math";`
- Similar conversions for: `LUA_STRLIBNAME`, `LUA_TABLIBNAME`, `LUA_DBLIBNAME`, `LUA_BITLIBNAME`, `LUA_JITLIBNAME`, `LUA_FFILIBNAME`

**`luaconf.h` (path strings):**
- `#define LUA_DIRSEP "\\"` or `"/"` → `constexpr std::string_view LUA_DIRSEP = ...;`
- Similar for: `LUA_PATHSEP`, `LUA_PATH_MARK`, `LUA_EXECDIR`, `LUA_IGMARK`, `LUA_PROGNAME`, `LUA_PROMPT`, `LUA_PROMPT2`

#### Simple Accessor Macros → constexpr inline functions
**`lj_target.h`:**
- `#define ra_noreg(r) ((r) & RID_NONE)`
  → `constexpr inline bool ra_noreg(Reg r) { return r & RID_NONE; }`
- `#define ra_hasreg(r) (!((r) & RID_NONE))`
  → `constexpr inline bool ra_hasreg(Reg r) { return !(r & RID_NONE); }`
- `#define ra_hashint(r) ((r) < RID_SUNK)`
  → `constexpr inline bool ra_hashint(Reg r) { return r < RID_SUNK; }`
- `#define rset_test(rs, r) ((int)((rs) >> (r)) & 1)`
  → `constexpr inline int rset_test(RegSet rs, Reg r) { return ((int)(rs >> r)) & 1; }`
- `#define regsp_reg(rs) ((rs) & 255)`
  → `constexpr inline uint8_t regsp_reg(RegSP rs) { return rs & 255; }`
- `#define regsp_spill(rs) ((rs) >> 8)`
  → `constexpr inline uint8_t regsp_spill(RegSP rs) { return rs >> 8; }`

**`lj_obj.h`:**
- Similar conversions for GC reference and memory reference macros that are simple accessors

### Notes for Phase 3
- Prioritize conversion of frequently-used, simple macros
- Test on multiple platforms due to potential bitwise operation differences
- Some macros like `lua_upvalueindex(i)` require inline functions with parameters
- Be cautious with macros that embed side effects

---

## Phase 4: Enum Classes and Advanced Features (LOWER PRIORITY, HIGHER RISK)

### Goal
Leverage C++20 features like `enum class` for better type safety and scoping.

### Files & Scope
- Conversion of `enum` to `enum class` where appropriate (requires API compatibility review)
- Use of `std::bit_cast` for type-punning operations (C++20)
- Use of C++20 concepts for template metaprogramming if applicable
- Structured bindings for complex types

### Notes for Phase 4
- This phase requires careful API review and testing
- May introduce breaking changes if not handled carefully
- Focus on internal implementation headers first
- Provides incremental modernization benefits

---

## Implementation Strategy

### Per-File Workflow
1. **Create branch** for each file or logical group of related files
2. **Apply conversions** according to phase guidelines
3. **Compile and verify** using:
   ```bash
   cmake --build build/agents --config Release --target fluid --parallel
   cmake --install build/agents
   ```
4. **Run Flute tests** to ensure no regressions:
   ```bash
   cd src/fluid/tests && ../../../build/agents-install/parasol /path/to/tools/flute.fluid file=/absolute/path/to/test.fluid --gfx-driver=headless
   ```
5. **Commit with clear message** describing changes
6. **Push to feature branch**

### Testing Requirements
- Verify code compiles cleanly with no warnings
- Run full integration test suite: `ctest --build-config Release --test-dir build/agents --output-on-failure`
- Verify `parasol --version` reports correct build
- Test on both Windows and Linux platforms if possible

### Code Style Checklist
Before committing, verify:
- [ ] No `&&` used (should be `and`)
- [ ] No `||` used (should be `or`)
- [ ] No `==` used (should be `IS` macro)
- [ ] No `static_cast` used (use C-style casts)
- [ ] No C++ exceptions
- [ ] No trailing whitespace
- [ ] Code compiles successfully
- [ ] Proper indentation (3 spaces for tabs)
- [ ] British English spelling in comments
- [ ] Max column width: 120 characters

---

## Benefits of This Upgrade

1. **Type Safety**: `constexpr` and `using` provide better type checking at compile time
2. **Scoping**: `constexpr` variables have proper scope vs global macros
3. **Readability**: `using` syntax is more modern and explicit than `typedef`
4. **Maintainability**: `constexpr inline` functions can be inlined by the compiler
5. **Documentation**: Modern syntax makes intent clearer to readers
6. **Performance**: Compiler can optimize `constexpr` expressions more aggressively
7. **Debugging**: Constexpr values are easier to inspect in debuggers

---

## Risk Assessment

- **Phase 1 (Numeric Constants)**: Very Low Risk - straightforward conversion, no behavior changes
- **Phase 2 (Type Aliases)**: Low Risk - purely syntactic changes
- **Phase 3 (Strings & Macros)**: Medium Risk - requires testing, potential edge cases with stringification
- **Phase 4 (Enum Classes)**: High Risk - may affect binary compatibility and API

---

## Timeline Estimate

- **Phase 1**: 2-3 hours (high-impact, quick wins)
- **Phase 2**: 1-2 hours (straightforward substitution)
- **Phase 3**: 2-4 hours (requires more testing)
- **Phase 4**: 4-6 hours (needs careful review and testing)

**Total estimated effort**: 9-15 hours

---

## Files by Priority Order

### High Priority (Phase 1 & 2)
1. `lj_def.h` - Core constants and limits
2. `lua.h` - Public API, version and status constants
3. `lj_target.h` - Register and allocation constants
4. `lualib.h` - Library names and function pointers

### Medium Priority
5. `luaconf.h` - Configuration constants
6. `lj_arch.h` - Architecture constants
7. `lj_obj.h` - Object system type aliases and macros
8. `lj_vm.h` - VM-related function pointers

### Lower Priority
- All other header files (can be batched by module)

---

## Success Criteria

✅ All Phase 1 changes compile cleanly
✅ All Phase 2 changes compile cleanly
✅ Full test suite passes (both local and CI)
✅ No performance regressions
✅ No API breakage
✅ Code follows Parasol style guidelines
✅ Changes documented in commit messages

---

## File Upgrade Tracking (185 files total)

### Legend
- `[  ]` - Not yet processed
- `[P1]` - Phase 1 complete (constexpr numeric constants)
- `[P2]` - Phase 2 complete (typedef to using syntax)
- `[P3]` - Phase 3 complete (string constants & macro functions)
- `[DONE]` - All applicable phases completed

### Public API Headers (Priority: HIGH)

```
[P1][P2] lua.h                      ✅ COMPLETED - Version/status codes, function pointers
[P1][P2] luaconf.h                  ✅ COMPLETED - Configuration constants
[P1][P2] lualib.h                   ⏳ PENDING - Library constants and function declarations
[  ] lauxlib.h                      - Auxiliary library interface
```

### Core System Headers (Priority: HIGH)

```
[P1][P2] lj_def.h                   ✅ COMPLETED - VM limits, type definitions
[P1][P2] lj_obj.h                   ✅ COMPLETED - Object system, bytecode types, memory refs
[P1][P2] lj_arch.h                  ✅ REVIEWED - Architecture detection (macros required)
[P1][P2] lj_target.h                ✅ COMPLETED - Register allocations, target CPU defs
[P1][P2] lj_vm.h                    ✅ COMPLETED - VM function pointers
```

### Register & Target Architecture Headers

```
[P1][P2] lj_target_x86.h            - X86 target definitions
[P1][P2] lj_target_arm.h            - ARM target definitions
[P1][P2] lj_target_arm64.h          - ARM64 target definitions
[P1][P2] lj_target_ppc.h            - PowerPC target definitions
[P1][P2] lj_target_mips.h           - MIPS target definitions
```

### Assembly & Code Generation Headers

```
[  ] lj_asm.h                       - Assembler interface
[  ] lj_asm_x86.h                   - X86 assembler
[  ] lj_asm_arm.h                   - ARM assembler
[  ] lj_asm_arm64.h                 - ARM64 assembler
[  ] lj_asm_ppc.h                   - PowerPC assembler
[  ] lj_asm_mips.h                  - MIPS assembler
[  ] lj_emit_x86.h                  - X86 emitter
[  ] lj_emit_arm.h                  - ARM emitter
[  ] lj_emit_arm64.h                - ARM64 emitter
[  ] lj_emit_ppc.h                  - PowerPC emitter
[  ] lj_emit_mips.h                 - MIPS emitter
```

### Bytecode & Serialization

```
[  ] lj_bc.h                        - Bytecode instruction definitions
[  ] lj_bcdump.h                    - Bytecode dump format
```

### Data Type Definitions

```
[  ] lj_buf.h                       - String buffer definitions
[  ] lj_tab.h                       - Table definitions
[  ] lj_str.h                       - String definitions
[  ] lj_func.h                      - Function definitions
[  ] lj_udata.h                     - Userdata definitions
[  ] lj_cdata.h                     - FFI C data definitions
[  ] lj_state.h                     - Lua state definitions
```

### Parser & Compilation

```
[  ] lj_parse.h                     - Parser interface
[  ] parser/lj_parse_types.h        - Parser type definitions
[  ] parser/lj_parse_internal.h     - Parser internal definitions
```

### IR & Optimization

```
[  ] lj_ir.h                        - IR instruction definitions
[  ] lj_ircall.h                    - IR call definitions
[  ] lj_iropt.h                     - IR optimization definitions
```

### JIT Compiler

```
[  ] lj_jit.h                       - JIT compiler definitions
[  ] lj_record.h                    - Recording interface
[  ] lj_trace.h                     - Trace definitions
[  ] lj_snap.h                      - Snapshot definitions
```

### String & Number Formatting

```
[  ] lj_strfmt.h                    - String formatting definitions
[  ] lj_strscan.h                   - String scanning definitions
```

### FFI & C Interop

```
[  ] lj_ccall.h                     - C function call interface
[  ] lj_ccallback.h                 - C callback interface
[  ] lj_cparse.h                    - C type parser interface
[  ] lj_ctype.h                     - C type definitions
[  ] lj_cconv.h                     - C conversion interface
[  ] lj_carith.h                    - C arithmetic interface
[  ] lj_clib.h                      - C library interface
[  ] lj_crecord.h                   - FFI recording interface
```

### Memory & GC

```
[  ] lj_alloc.h                     - Memory allocator interface
[  ] lj_gc.h                        - Garbage collector interface
[  ] lj_mcode.h                     - Machine code interface
```

### Debugging & Profiling

```
[  ] lj_debug.h                     - Debug interface
[  ] lj_gdbjit.h                    - GDB JIT interface
[  ] lj_profile.h                   - Profiling interface
[  ] lj_vmevent.h                   - VM event interface
```

### Miscellaneous Headers

```
[  ] lj_dispatch.h                  - VM dispatch definitions
[  ] lj_meta.h                      - Metamethod interface
[  ] lj_lib.h                       - Library interface
[  ] lj_char.h                      - Character class definitions
[  ] lj_errmsg.h                    - Error message definitions
[  ] lj_ff.h                        - Fast function definitions
[  ] lj_frame.h                     - Stack frame definitions
[  ] lj_lex.h                       - Lexer interface
[  ] lj_load.h                      - Chunk loading interface
[  ] lj_prng.h                      - PRNG interface
[  ] lj_serialize.h                 - Serialization interface
[  ] lj_traceerr.h                  - Trace error definitions
[  ] luajit.h                       - LuaJIT public header
[  ] lualib.h                       - Lua library header
```

### Implementation Files (CPP/C)

```
[  ] ljamalg.cpp                    - Amalgamation source
[  ] lua.cpp                        - Lua core (if present)
[  ] luajit.cpp                     - LuaJIT main
```

#### Core Implementation

```
[  ] lj_alloc.cpp                   - Memory allocation
[  ] lj_api.cpp                     - Lua C API implementation
[  ] lj_assert.cpp                  - Assertions
[  ] lj_bc.cpp                      - Bytecode
[  ] lj_bcread.cpp                  - Bytecode reader
[  ] lj_bcwrite.cpp                 - Bytecode writer
[  ] lj_buf.cpp                     - String buffer
[  ] lj_carith.cpp                  - C arithmetic
[  ] lj_ccall.cpp                   - C function calls
[  ] lj_ccallback.cpp               - C callbacks
[  ] lj_cconv.cpp                   - C conversion
[  ] lj_cdata.cpp                   - FFI C data
[  ] lj_char.cpp                    - Character operations
[  ] lj_clib.cpp                    - C library loading
[  ] lj_cmath.cpp                   - C math
[  ] lj_cparse.cpp                  - C type parser
[  ] lj_crecord.cpp                 - FFI recording
[  ] lj_ctype.cpp                   - C types
[  ] lj_debug.cpp                   - Debugging
[  ] lj_dispatch.cpp                - VM dispatch
[  ] lj_err.cpp                     - Error handling
[  ] lj_ffrecord.cpp                - Fast function recording
[  ] lj_func.cpp                    - Functions
[  ] lj_gc.cpp                      - Garbage collection
[  ] lj_gdbjit.cpp                  - GDB JIT support
[  ] lj_ir.cpp                      - IR generation
[  ] lj_lex.cpp                     - Lexer
[  ] lj_lib.cpp                     - Library base
[  ] lj_load.cpp                    - Chunk loading
[  ] lj_mcode.cpp                   - Machine code
[  ] lj_meta.cpp                    - Metamethods
[  ] lj_obj.cpp                     - Core objects
[  ] lj_parse.cpp                   - Parser
[  ] lj_prng.cpp                    - PRNG
[  ] lj_profile.cpp                 - Profiling
[  ] lj_record.cpp                  - Trace recording
[  ] lj_serialize.cpp               - Serialization
[  ] lj_snap.cpp                    - Snapshots
[  ] lj_state.cpp                   - Lua state
[  ] lj_str.cpp                     - Strings
[  ] lj_strfmt.cpp                  - String formatting
[  ] lj_strfmt_num.cpp              - Number formatting
[  ] lj_strscan.cpp                 - String scanning
[  ] lj_tab.cpp                     - Tables
[  ] lj_trace.cpp                   - Tracing
[  ] lj_udata.cpp                   - Userdata
[  ] lj_vmevent.cpp                 - VM events
[  ] lj_vmmath.cpp                  - VM math
[  ] lj_asm.cpp                     - Assembler
```

#### Library Implementation

```
[  ] lib_aux.cpp                    - Auxiliary library
[  ] lib_base.cpp                   - Base library
[  ] lib_bit.cpp                    - Bitwise operations
[  ] lib_debug.cpp                  - Debug library
[  ] lib_init.cpp                   - Library initialization
[  ] lib_jit.cpp                    - JIT library
[  ] lib_math.cpp                   - Math library
[  ] lib_string.cpp                 - String library
[  ] lib_table.cpp                  - Table library
```

#### Optimization Passes

```
[  ] lj_opt_dce.cpp                 - Dead code elimination
[  ] lj_opt_fold.cpp                - Constant folding
[  ] lj_opt_loop.cpp                - Loop optimization
[  ] lj_opt_mem.cpp                 - Memory optimization
[  ] lj_opt_narrow.cpp              - Value narrowing
[  ] lj_opt_sink.cpp                - Allocation sinking
[  ] lj_opt_split.cpp               - Register splitting
```

#### Parser Components (parser/)

```
[  ] parser/lj_parse_constants.cpp  - Parser constants
[  ] parser/lj_parse_core.cpp       - Parser core
[  ] parser/lj_parse_expr.cpp       - Expression parsing
[  ] parser/lj_parse_operators.cpp  - Operator parsing
[  ] parser/lj_parse_regalloc.cpp   - Register allocation
[  ] parser/lj_parse_scope.cpp      - Scope handling
[  ] parser/lj_parse_stmt.cpp       - Statement parsing
```

#### Build System (host/)

```
[  ] host/buildvm.cpp               - Build VM generator
[  ] host/buildvm.h                 - Build VM header
[  ] host/buildvm_asm.cpp           - ASM generation
[  ] host/buildvm_fold.cpp          - Fold generation
[  ] host/buildvm_lib.cpp           - Library generation
[  ] host/buildvm_libbc.h           - Library bytecode header
[  ] host/buildvm_peobj.cpp         - PE object generation
[  ] host/minilua.c                 - Minimal Lua for building
```

---

### Progress Summary

**Completed: 5 files** (Phase 1 & 2)
- ✅ lua.h
- ✅ luaconf.h
- ✅ lj_def.h
- ✅ lj_obj.h
- ✅ lj_target.h
- ✅ lj_vm.h (P2 only)

**Remaining: 180 files** across 4 phases

**Recommended Priority Order for Remaining Work:**
1. Public API headers (lualib.h, lauxlib.h)
2. Core type headers (lj_tab.h, lj_str.h, lj_func.h)
3. Bytecode/IR headers (lj_bc.h, lj_ir.h)
4. Implementation files (can be processed in parallel)
5. Platform-specific headers (last, as they have conditional compilation)
