# LuaJIT Parser C++20 Modernization and Optimization Plan

**Created:** 2025-11-16
**Status:** Proposed
**Target:** `src/fluid/luajit-2.1/src/parser/`
**Scope:** C++20 modernization, performance optimization, code quality improvements

## Executive Summary

This plan identifies opportunities to modernize and optimize the Fluid/LuaJIT parser implementation using C++20 features while maintaining compatibility with the existing architecture and Parasol coding standards.

## Overview

The parser code in `src/fluid/luajit-2.1/src/parser/` is well-structured and shows evidence of recent modernization efforts (C++20 concepts, RAII guards, `std::span`, `std::string_view`). However, there remain several opportunities for further improvement in terms of performance, safety, and maintainability.

## Current State Analysis

### Strengths

1. **Already uses modern C++ features:**
   - C++20 concepts (`parse_concepts.h`)
   - RAII guards for scope/register management (`parse_raii.h`)
   - `std::span`, `std::string_view`, `std::optional`
   - `[[nodiscard]]`, `[[likely]]`, `[[unlikely]]` attributes
   - Range-based for loops with structured bindings

2. **Clean architecture:**
   - Well-separated concerns (lexer, parser, operators, statements)
   - Clear file organization
   - Good use of forward declarations

3. **Performance-conscious:**
   - Uses `LJ_AINLINE`, `LJ_LIKELY`/`LJ_UNLIKELY` macros
   - Manual memory management where appropriate
   - Efficient bytecode generation

### Areas for Improvement

The following sections detail specific modernization and optimization opportunities.

---

## Category 1: C++20 Feature Adoption

### 1.1 Constexpr and Compile-Time Evaluation

**Current State:**
- Many helper functions that could be `constexpr` are not marked as such
- Some array initializations could be done at compile-time

**Opportunities:**

**File:** `parse_types.h`
```cpp
// Current
static constexpr bool vkisvar(ExpKind k) {
   return ExpKind::Local <= k and k <= ExpKind::Indexed;
}

// Already constexpr ✓ - Good example

// Could add:
static constexpr bool expr_is_constant(ExpKind k) {
   return k <= ExpKind::Last;
}

static constexpr ExpKind next_expkind(ExpKind k) {
   return static_cast<ExpKind>(static_cast<uint8_t>(k) + 1);
}
```

**File:** `lj_parse.cpp` (priority table)
```cpp
// Current: Runtime initialization
static const struct {
   uint8_t left, right;
   const char* name;
   uint8_t name_len;
} priority[] = { ... };

// Proposed: Make fully constexpr
struct OperatorPriority {
   uint8_t left, right;
   std::string_view name;

   constexpr OperatorPriority(uint8_t l, uint8_t r, std::string_view n = {})
      : left(l), right(r), name(n) {}
};

static constexpr std::array<OperatorPriority, OPR_NOBINOPR> priority = {
   {6, 6},        // OPR_ADD
   {6, 6},        // OPR_SUB
   // ... etc
   {7, 5, "lshift"},  // OPR_SHL
   {7, 5, "rshift"},  // OPR_SHR
   // ...
};
```

**Benefits:**
- Compile-time computation reduces runtime overhead
- Better optimization opportunities for the compiler
- Type safety with `std::array` over C arrays
- Eliminates runtime string length calculation

**Estimated Impact:** Low (mostly code quality)

---

### 1.2 Enhanced use of `std::span`

**Current State:**
- Some array access patterns still use raw pointers with manual bounds tracking
- `std::span` is used in some places (e.g., `parse_scope.cpp:99`)

**Opportunities:**

**File:** `parse_scope.cpp`
```cpp
// Current (lines 540-560)
for (ve = vs + this->vtop, vs += fs->vbase; vs < ve; vs++) {
   if (!gola_is_jump_or_target(vs)) {
      GCstr* s = strref(vs->name);
      // ... process variable
   }
}

// Proposed
auto var_range = std::span(this->vstack + fs->vbase, this->vtop - fs->vbase);
for (auto& var : var_range) {
   if (!gola_is_jump_or_target(&var)) {
      GCstr* s = strref(var.name);
      // ... process variable
   }
}
```

**File:** `parse_constants.cpp`
```cpp
// Add span-based iteration for constant table nodes
static void process_constant_table(FuncState* fs, GCproto* pt, void* kptr) {
   GCtab* kt = fs->kt;
   auto array_span = std::span(tvref(kt->array), kt->asize);

   for (size_t i = 0; auto& tv : array_span) {
      if (tvhaskslot(&tv)) {
         // ... process constant
      }
      i++;
   }
}
```

**Benefits:**
- Improved bounds safety
- More expressive code
- Better iterator invalidation semantics
- Easier to reason about data access patterns

**Estimated Impact:** Medium (safety + readability)

---

### 1.3 Designated Initializers

**Current State:**
- Structure initialization uses positional arguments
- Makes code less maintainable when structures change

**Opportunities:**

**File:** `parse_expr.cpp`
```cpp
// Current (line 173)
expr_init(&key, ExpKind::Num, 0);
setintV(&key.u.nval, int(narr));

// Proposed
ExpDesc key {
   .k = ExpKind::Num,
   .flags = 0,
   .t = NO_JMP,
   .f = NO_JMP,
   .u = { .nval = {} }
};
setintV(&key.u.nval, int(narr));
```

**Benefits:**
- More explicit initialization
- Catches missing field initialization at compile-time
- Self-documenting code

**Estimated Impact:** Low (code quality)

---

### 1.4 `std::bit_cast` for Type Punning

**Current State:**
- Union-based type punning for bytecode manipulation
- Some pointer casts that could be replaced

**Opportunities:**

**File:** `parse_operators.cpp`
```cpp
// Current (line 623)
o->u64 ^= U64x(80000000, 00000000);

// Could be more explicit with bit_cast in some contexts
// (though union access is valid in C++, bit_cast may be clearer)

// Example for BCIns manipulation:
template<typename T>
requires (sizeof(T) == sizeof(BCIns))
T extract_bcins_field(BCIns ins) {
   return std::bit_cast<T>(ins);
}
```

**Benefits:**
- More explicit type conversions
- Better semantics for bit-level operations
- May improve optimizer understanding

**Estimated Impact:** Low (mostly documentation value)

---

## Category 2: Performance Optimizations

### 2.1 String Handling Optimization

**Current State:**
- `std::string_view` is used in many places
- Some string operations could be more efficient

**Opportunities:**

**File:** `parse_operators.cpp`
```cpp
// Current (lines 224-268)
static void bcemit_shift_call_at_base(FuncState* fs, std::string_view fname,
   ExpDesc* lhs, ExpDesc* rhs, BCReg base)
{
   // ... multiple keepstr calls
   callee.u.sval = fs->ls->keepstr("bit");
   key.u.sval = fs->ls->keepstr(fname);
}

// Proposed: String interning optimization
// Pre-intern common strings at startup
namespace InternedStrings {
   static GCstr* bit_str = nullptr;
   static GCstr* lshift_str = nullptr;
   static GCstr* rshift_str = nullptr;
   // ... etc

   static void initialize(lua_State* L) {
      bit_str = lj_str_newlit(L, "bit");
      fixstring(bit_str);
      // ... etc
   }
}
```

**Benefits:**
- Eliminates repeated string lookups
- Reduces allocator pressure
- Faster bitwise operator handling

**Estimated Impact:** Low-Medium (common operation optimization)

---

### 2.2 Small Object Optimization for ExpDesc

**Current State:**
- `ExpDesc` is 40 bytes (estimated)
- Frequently passed by pointer

**Opportunities:**

**Analysis:**
```cpp
// Current structure size analysis
struct ExpDesc {
   union {
      struct { uint32_t info; uint32_t aux; } s;  // 8 bytes
      TValue nval;                                 // 16 bytes (largest)
      GCstr* sval;                                 // 8 bytes
   } u;                                           // 16 bytes total
   ExpKind k;                                     // 1 byte
   uint8_t flags;                                 // 1 byte
   BCPos t;                                       // 4 bytes
   BCPos f;                                       // 4 bytes
   // + padding = ~32 bytes
};
```

**Proposed:**
- Keep current design (passing by pointer is appropriate for this size)
- Consider adding `[[trivially_copyable]]` assertion
- Optimize flag usage (currently only 2 flags used out of 8 bits)

```cpp
static_assert(std::is_trivially_copyable_v<ExpDesc>,
   "ExpDesc must be trivially copyable for efficient passing");

// Flag consolidation opportunities
inline constexpr uint8_t EXP_FLAG_POSTFIX_INC  = 0x01;
inline constexpr uint8_t EXP_FLAG_HAS_RHS_REG  = 0x02;
inline constexpr uint8_t EXP_FLAG_RESERVED_1   = 0x04;  // Available
inline constexpr uint8_t EXP_FLAG_RESERVED_2   = 0x08;  // Available
// ... etc - document all 8 bits
```

**Benefits:**
- Better documentation of structure layout
- Compile-time validation
- Space for future flags

**Estimated Impact:** Low (documentation + future-proofing)

---

### 2.3 Branch Prediction Optimization

**Current State:**
- Good use of `[[likely]]` and `[[unlikely]]`
- Some opportunities for additional hints

**Opportunities:**

**File:** `parse_scope.cpp`
```cpp
// Current (line 26)
if (vtop >= this->sizevstack) [[unlikely]] {
   // Growth code
}

// Good example ✓

// Additional opportunities:
if (fmt IS STRSCAN_ERROR) [[unlikely]] {
   // Error handling
}

// In hot loops:
while (condition) {
   if (common_case) [[likely]] {
      // Fast path
   } else [[unlikely]] {
      // Slow path
   }
}
```

**File:** `parse_expr.cpp`
```cpp
// Token identification in hot path
static inline bool is_likely_identifier(LexChar c) [[likely]] {
   return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or c == '_';
}
```

**Benefits:**
- Better instruction cache utilization
- Improved branch predictor performance
- Faster common paths

**Estimated Impact:** Low-Medium (micro-optimization)

---

### 2.4 Loop Optimization

**Current State:**
- Most loops are well-written
- Some opportunities for range-based iteration

**Opportunities:**

**File:** `parse_scope.cpp`
```cpp
// Current (line 81-89)
for (i = fs->nactvar - 1; i >= 0; i--) {
   GCstr* varname = strref(var_get(fs->ls, fs, i).name);
   if (varname == NAME_BLANK) [[unlikely]]
      continue;
   if (n == varname) [[likely]]
      return BCReg(i);
}

// Could use reverse iterator (though current is fine for performance)
auto var_range = std::span(fs->ls->vstack, fs->nactvar);
for (auto it = var_range.rbegin(); it != var_range.rend(); ++it) {
   // ...
}

// Benchmark needed to determine if this is actually faster
```

**Benefits:**
- Potentially more optimizer-friendly
- Standard iterator semantics
- Better intent expression

**Estimated Impact:** Low (need benchmarking)

---

## Category 3: Code Quality and Safety

### 3.1 Consistent use of `[[nodiscard]]`

**Current State:**
- Many functions properly marked `[[nodiscard]]`
- Some query functions missing the attribute

**Opportunities:**

**File:** `parse_types.h`
```cpp
// Current - good examples:
[[nodiscard]] static constexpr bool vkisvar(ExpKind k) { ... }
[[nodiscard]] static inline bool expr_hasjump(const ExpDesc* e) { ... }

// Add to:
[[nodiscard]] static inline ExpKind const_pri(const ExpDesc* e) { ... }
[[nodiscard]] static inline bool tvhaskslot(const TValue* o) { ... }
[[nodiscard]] static inline uint32_t tvkslot(const TValue* o) { ... }
```

**File:** `parse_internal.h`
```cpp
// Add nodiscard to all query functions
[[nodiscard]] static int is_blank_identifier(GCstr* name);
[[nodiscard]] static std::optional<BCReg> var_lookup_local(FuncState* fs, GCstr* n);
[[nodiscard]] static int parse_is_end(LexToken tok);
[[nodiscard]] static int predict_next(LexState* ls, FuncState* fs, BCPos pc);
```

**Benefits:**
- Prevents accidental value discarding
- Better compiler warnings
- Self-documenting code

**Estimated Impact:** Medium (safety)

---

### 3.2 Enhanced Type Safety with Strong Typedefs

**Current State:**
- Typedefs like `BCReg`, `BCPos` are basic types
- No compile-time prevention of type mixing

**Opportunities:**

**File:** `parse_types.h`
```cpp
// Current
typedef uint8_t BCReg;
typedef uint32_t BCPos;

// Proposed strong types (in separate header to avoid conflicts)
namespace StrongTypes {
   template<typename Tag, typename T>
   struct StrongTypedef {
      T value;

      constexpr explicit StrongTypedef(T v) : value(v) {}
      constexpr operator T() const { return value; }

      constexpr auto operator<=>(const StrongTypedef&) const = default;
   };

   struct BCRegTag {};
   struct BCPosTag {};

   using StrongBCReg = StrongTypedef<BCRegTag, uint8_t>;
   using StrongBCPos = StrongTypedef<BCPosTag, uint32_t>;
}

// Note: This would require extensive refactoring and may not be worth it
// given the existing codebase maturity. Consider for new code only.
```

**Benefits:**
- Prevents type confusion bugs
- Better code documentation
- Compile-time error detection

**Estimated Impact:** Low (too invasive for existing code)
**Recommendation:** Use for new features only

---

### 3.3 Improved Const Correctness

**Current State:**
- Generally good const usage
- Some function parameters could be const

**Opportunities:**

**File:** `parse_operators.cpp`
```cpp
// Current (line 12)
[[nodiscard]] static int foldarith(BinOpr opr, ExpDesc* e1, ExpDesc* e2)

// Proposed (opr is read-only)
[[nodiscard]] static int foldarith(const BinOpr opr, ExpDesc* e1, ExpDesc* e2)

// Additional opportunities in helper functions
[[nodiscard]] static constexpr bool is_arithmetic_op(const BinOpr op) {
   return op >= OPR_ADD and op <= OPR_POW;
}

[[nodiscard]] static constexpr bool is_bitwise_op(const BinOpr op) {
   return op >= OPR_BAND and op <= OPR_SHR;
}
```

**Benefits:**
- Clearer function contracts
- Better optimization opportunities
- Prevents accidental modifications

**Estimated Impact:** Low (code quality)

---

### 3.4 Replace Magic Numbers with Named Constants

**Current State:**
- Some magic numbers in bytecode manipulation
- Most constants are well-named

**Opportunities:**

**File:** `parse_expr.cpp`
```cpp
// Current (line 223)
en.u.nval.u32.hi = 0x43300000;  // Biased integer to avoid denormals.

// Proposed
inline constexpr uint32_t BIASED_INTEGER_HI = 0x43300000;
en.u.nval.u32.hi = BIASED_INTEGER_HI;

// Current (various files)
if (narr > 256) ...
if (narr > 0x7ff) narr = 0x7ff;

// Proposed
inline constexpr BCReg MAX_TABLE_NARR_INLINE = 256;
inline constexpr BCReg MAX_TABLE_NARR_ENCODED = 0x7ff;
```

**Benefits:**
- Self-documenting code
- Easier to maintain
- Single source of truth for constants

**Estimated Impact:** Low (readability)

---

## Category 4: Modern Algorithm Usage

### 4.1 `std::ranges` and Algorithm Replacements

**Current State:**
- Hand-written loops for many operations
- C++20 ranges not utilized

**Opportunities:**

**File:** `parse_scope.cpp`
```cpp
// Current (line 523-534)
auto uvmap_range = std::span(fs->uvmap.data(), fs->nuv);
for (auto uv_idx : uvmap_range) {
   GCstr* s = strref(vs[uv_idx].name);
   MSize len = s->len + 1;
   char* p = lj_buf_more(&this->sb, len);
   p = lj_buf_wmem(p, strdata(s), len);
   this->sb.w = p;
}

// Could use:
#include <algorithm>
std::ranges::for_each(uvmap_range, [&](auto uv_idx) {
   GCstr* s = strref(vs[uv_idx].name);
   // ... processing
});

// Though the current form is already clear and efficient
```

**File:** `parse_scope.cpp`
```cpp
// Current (line 99-104)
auto uvmap_view = std::span(fs->uvmap.data(), n);
for (MSize i = 0; auto uv_idx : uvmap_view) {
   if (uv_idx == vidx)
      return i;
   i++;
}

// Could use ranges::find with projection
auto it = std::ranges::find(uvmap_view, vidx);
if (it != uvmap_view.end())
   return std::distance(uvmap_view.begin(), it);
```

**Benefits:**
- More expressive code
- Potentially better optimization
- Standard algorithm guarantees

**Estimated Impact:** Low (marginal benefit, current code is clear)

---

### 4.2 `std::erase_if` for Container Management

**Current State:**
- Manual iteration for conditional removal
- Jump list management uses custom logic

**Opportunities:**

```cpp
// Generally not applicable - the parser's data structures are
// specialized (variable stacks, jump lists) and hand-optimized.
// Standard algorithms would likely be slower.
```

**Recommendation:** Keep current approach

---

## Category 5: Memory Management

### 5.1 Custom Allocator Hints

**Current State:**
- Uses LuaJIT's garbage collector
- Manual memory management for parser structures

**Opportunities:**

```cpp
// Use C++20 [[assume]] attribute for optimization hints
[[assume(fs->freereg <= fs->framesize)]];
[[assume(fs->nactvar <= LJ_MAX_LOCVAR)]];

// In hot paths where invariants are known to hold
static void bcreg_reserve(FuncState* fs, BCReg n) {
   [[assume(fs != nullptr)]];
   [[assume(n < LJ_MAX_SLOTS)]];
   bcreg_bump(fs, n);
   fs->freereg += n;
}
```

**Benefits:**
- Better optimization opportunities
- Documents invariants
- No runtime cost

**Estimated Impact:** Low (compiler-dependent)

---

### 5.2 Stack Allocation Optimization

**Current State:**
- Good use of stack allocation for temporary structures
- RAII guards prevent leaks

**Opportunities:**

```cpp
// Current approach is good. Consider documenting size limits:
static_assert(sizeof(ExpDesc) <= 64, "ExpDesc size threshold");
static_assert(sizeof(FuncScope) <= 32, "FuncScope size threshold");
static_assert(sizeof(LHSVarList) <= 16, "LHSVarList size threshold");

// This documents that these are small enough for stack allocation
// and alerts if they grow unexpectedly
```

**Benefits:**
- Documents design decisions
- Prevents accidental bloat
- Compile-time validation

**Estimated Impact:** Low (documentation)

---

## Category 6: Template Improvements

### 6.1 Enhanced Template Usage

**Current State:**
- Some template helpers exist (`bcemit_ABC`, `bcemit_AD`, `bcemit_AJ`)
- Could be expanded

**Opportunities:**

**File:** `parse_internal.h`
```cpp
// Current (lines 88-101)
template<typename Op>
static inline BCPos bcemit_ABC(FuncState* fs, Op o, BCReg a, BCReg b, BCReg c) {
   return bcemit_INS(fs, BCINS_ABC(o, a, b, c));
}

// Good! Could add concepts for safety:
template<BytecodeOpcode Op>
static inline BCPos bcemit_ABC(FuncState* fs, Op o, BCReg a, BCReg b, BCReg c) {
   return bcemit_INS(fs, BCINS_ABC(o, a, b, c));
}

// Additional helpers:
template<BytecodeOpcode Op, RegisterType... Regs>
   requires (sizeof...(Regs) <= 3)
static inline BCPos bcemit_multi_reg(FuncState* fs, Op op, Regs... regs) {
   // Compile-time dispatch based on register count
}
```

**Benefits:**
- Type safety
- Compile-time validation
- Better error messages

**Estimated Impact:** Low (mostly safety)

---

### 6.2 Constexpr Templates for Compile-Time Computation

**Current State:**
- Runtime table lookups (operator priorities)
- Some computation could be compile-time

**Opportunities:**

```cpp
// Constexpr priority lookup
template<BinOpr Op>
constexpr uint8_t get_left_priority() {
   static_assert(Op >= 0 and Op < OPR_NOBINOPR, "Invalid operator");
   return priority[Op].left;
}

template<BinOpr Op>
constexpr uint8_t get_right_priority() {
   static_assert(Op >= 0 and Op < OPR_NOBINOPR, "Invalid operator");
   return priority[Op].right;
}

// Usage:
if constexpr (get_left_priority<OPR_ADD>() > limit) {
   // Compile-time decision
}
```

**Benefits:**
- Compile-time validation
- Potential performance improvement
- Better constant folding

**Estimated Impact:** Low (limited applicability)

---

## Category 7: Error Handling

### 7.1 Structured Error Information

**Current State:**
- Uses `lj_lex_error` with varargs
- Error messages are string literals

**Opportunities:**

```cpp
// Error information structure
struct ParseError {
   LexToken tok;
   ErrMsg msg;
   BCLine line;
   std::string_view context;

   [[noreturn]] void throw_error(LexState* ls) const {
      lj_lex_error(ls, tok, msg, context.data());
   }
};

// Usage
if (error_condition) {
   ParseError{
      .tok = this->tok,
      .msg = LJ_ERR_XSYNTAX,
      .line = this->linenumber,
      .context = "invalid operator"
   }.throw_error(this);
}
```

**Benefits:**
- More structured error handling
- Easier to add error context
- Better error messages

**Estimated Impact:** Low-Medium (error quality)
**Note:** Requires careful integration with existing error system

---

## Category 8: Documentation and Maintainability

### 8.1 Improved Inline Documentation

**Current State:**
- Good comments in complex sections
- Some functions lack detailed documentation

**Opportunities:**

```cpp
/// @brief Emit bytecode for chained bitwise shift operations
/// @param fs Function state for bytecode generation
/// @param fname Bit library function name ("lshift", "rshift", etc.)
/// @param lhs Left-hand side expression (value to shift)
/// @param rhs Right-hand side expression (shift count)
/// @param base Base register for the call (allows register reuse)
/// @details Implements left-associative chaining for expressions like
///          `x << 2 << 3` by reusing the base register for intermediate results.
///          Handles multi-return functions by using only the first return value.
/// @see expr_shift_chain for the chaining logic
static void bcemit_shift_call_at_base(FuncState* fs, std::string_view fname,
   ExpDesc* lhs, ExpDesc* rhs, BCReg base)
```

**Benefits:**
- Better code understanding
- Easier onboarding
- IDE integration

**Estimated Impact:** High (maintainability)

---

### 8.2 Static Assertions for Invariants

**Current State:**
- Runtime assertions for debug builds
- Some compile-time checks exist

**Opportunities:**

```cpp
// Add more compile-time validations
static_assert(sizeof(ExpDesc) < 64, "ExpDesc size reasonable");
static_assert(alignof(ExpDesc) <= 8, "ExpDesc alignment reasonable");
static_assert(std::is_trivially_destructible_v<ExpDesc>, "ExpDesc trivially destructible");

// Validate enum sizes
static_assert(sizeof(ExpKind) == 1, "ExpKind is uint8_t");
static_assert(sizeof(BinOpr) == sizeof(int), "BinOpr enum size");

// Validate structure layouts
static_assert(offsetof(FuncState, freereg) < 64, "Hot fields in first cache line");

// Validate register limits
static_assert(LJ_MAX_SLOTS <= 256, "BCReg can represent all slots");
static_assert(BCMAX_C <= 255, "C field fits in uint8_t");
```

**Benefits:**
- Compile-time validation
- Documents assumptions
- Catches regressions

**Estimated Impact:** Medium (safety + documentation)

---

## Implementation Priority

### High Priority (Immediate Value)

1. **Add `[[nodiscard]]` consistently** (Category 3.1)
   - Low effort, high safety benefit
   - Estimated time: 2-4 hours

2. **Add static assertions** (Category 8.2)
   - Documents invariants
   - Catches regressions
   - Estimated time: 4-6 hours

3. **Enhanced inline documentation** (Category 8.1)
   - Improves maintainability
   - Estimated time: 8-12 hours

4. **Replace magic numbers** (Category 3.4)
   - Improves readability
   - Estimated time: 4-6 hours

### Medium Priority (Good Value)

5. **Constexpr priority table** (Category 1.1)
   - Modern C++ usage
   - Small performance gain
   - Estimated time: 3-4 hours

6. **Enhanced `std::span` usage** (Category 1.2)
   - Safety improvement
   - Estimated time: 6-8 hours

7. **Branch prediction optimization** (Category 2.3)
   - Micro-optimization
   - Requires profiling
   - Estimated time: 4-6 hours

8. **String interning optimization** (Category 2.1)
   - Reduces allocator pressure
   - Estimated time: 3-4 hours

### Low Priority (Marginal Value)

9. **Designated initializers** (Category 1.3)
   - Code quality
   - Estimated time: 2-3 hours

10. **Const correctness improvements** (Category 3.3)
    - Code quality
    - Estimated time: 3-4 hours

11. **Template enhancements** (Category 6.1)
    - Type safety
    - Estimated time: 4-6 hours

### Not Recommended

- **Strong typedefs** (Category 3.2) - Too invasive
- **Ranges algorithms** (Category 4.1) - Current code is clear
- **Structured error handling** (Category 7.1) - Integration complexity

---

## Testing Strategy

1. **Regression Testing:**
   - Run full Flute test suite after each change
   - Verify bytecode generation consistency
   - Test error handling paths

2. **Performance Testing:**
   - Benchmark parser performance before/after changes
   - Focus on common cases (simple expressions, function definitions)
   - Use production Fluid scripts for realistic testing

3. **Static Analysis:**
   - Run with `-Werror -Wall -Wextra -Wpedantic`
   - Use clang-tidy with C++20 checks
   - Verify no new warnings

4. **Code Review:**
   - Ensure changes follow Parasol coding standards
   - Verify `and`/`or` instead of `&&`/`||`
   - Check `IS` macro usage instead of `==`
   - Confirm British English spelling

---

## Compilation Verification

All changes must be verified with:
```bash
cmake --build build/agents --config [BuildType] --target fluid --parallel
```

And tested with:
```bash
cd src/fluid/tests && ../../../build/agents-install/parasol.exe \
   ../../../tools/flute.fluid file=E:/parasol/src/fluid/tests/test_coalesce.fluid \
   --gfx-driver=headless
```

---

## Risks and Mitigations

### Risk: Performance Regression
**Mitigation:**
- Benchmark before/after all optimizations
- Keep current implementation if new approach is slower
- Profile hot paths with realistic workloads

### Risk: Breaking Changes
**Mitigation:**
- Thorough regression testing
- Incremental changes with verification
- Maintain bytecode compatibility

### Risk: Increased Compilation Time
**Mitigation:**
- Avoid excessive template instantiation
- Use `extern template` where appropriate
- Monitor build times

### Risk: Reduced Portability
**Mitigation:**
- Only use features in C++20 standard
- Avoid compiler-specific extensions
- Test on multiple compilers (MSVC, GCC, Clang)

---

## Conclusion

The Fluid/LuaJIT parser is already well-written and shows evidence of thoughtful C++ modernization. The opportunities identified in this plan are primarily incremental improvements focused on:

1. **Safety** - Additional `[[nodiscard]]`, static assertions
2. **Maintainability** - Better documentation, named constants
3. **Performance** - Minor optimizations in hot paths
4. **Modernization** - Leveraging C++20 features where beneficial

None of the changes are critical, but implementing the high-priority items would provide good value for relatively low effort. The most impactful changes are those that improve code documentation and compile-time safety rather than runtime performance, as the parser is already quite efficient.

---

**Next Steps:**
1. Review this plan with the team
2. Prioritize based on project needs
3. Implement high-priority items incrementally
4. Measure impact and adjust priorities
5. Update this plan based on findings
