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
