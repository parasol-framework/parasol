# Safe Navigation Operator Implementation Plan (Revised)

## Overview

This document provides a revised implementation plan for the Safe Navigation Operator (`?.`) in Fluid/LuaJIT using a simpler approach that leverages the existing if-empty operator (`?`) mechanism.

**Status:** 📋 **Planning** - Not yet started

**Priority:** ⭐⭐⭐ **Medium**

**Estimated Effort:** 8-16 hours

## Core Strategy

Instead of implementing custom bytecode generation for safe navigation, we leverage the existing if-empty operator which already handles nil checks correctly:

- `obj?.field` is equivalent to `obj ? obj.field`
- `obj?.method()` is equivalent to `obj ? obj:method()`
- `obj?[key]` is equivalent to `obj ? obj[key]`

The key insight is that the if-empty operator already:
1. Checks if a value is nil (among other falsey checks)
2. Returns the RHS when LHS is falsey
3. Handles expression evaluation correctly without side effects

For safe navigation, we only need to check for **nil specifically** (not false/zero/empty string like the full if-empty operator does).

## Feature Specification

### Syntax
```lua
obj?.field
obj?.method()
obj?[index]
obj?.field?.subfield
```

### Semantics
- **Nil-only check**: If object is `nil`, returns `nil` without accessing field/method
- **Field access**: `obj?.field` returns `nil` if `obj` is `nil`, otherwise returns `obj.field`
- **Method calls**: `obj?.method()` returns `nil` if `obj` is `nil`, otherwise calls `obj:method()`
- **Index access**: `obj?[key]` returns `nil` if `obj` is `nil`, otherwise returns `obj[key]`
- **Chaining**: Multiple safe navigation operators can chain: `obj?.field?.subfield`
- **Integration**: Works seamlessly with `?` (if-empty) for default values

### Examples
```lua
local name = user?.profile?.name ? "Guest"
local result = obj?.method()
local value = table?[key] ? 0
```

## Implementation Steps

### Step 1: Add Token Definitions

**File:** `src/fluid/luajit-2.1/src/lj_lex.h`

**Action:**
Add safe navigation tokens to the `TKDEF` macro:
```c
__(safe_field, ?.) __(safe_method, ?:)
```

**Expected Result:**
- `TK_safe_field` and `TK_safe_method` tokens are defined
- Token enum values are automatically generated

---

### Step 2: Update Lexer to Emit Safe Navigation Tokens

**File:** `src/fluid/luajit-2.1/src/lj_lex.c`

**Action:**
Modify the `?` case in `lex_scan()` to recognize `?.` and `?:`:
```c
case '?':
  lex_next(ls);
  if (ls->c == '.') { lex_next(ls); return TK_safe_field; }
  if (ls->c == ':') { lex_next(ls); return TK_safe_method; }
  if (ls->c != '?') return TK_if_empty;
  else { lex_next(ls); return TK_presence; }
```

**Note:** For `?[`, we'll handle it as `TK_if_empty` followed by `[` in the parser.

**Expected Result:**
- Lexer correctly tokenizes `?.` as `TK_safe_field`
- Lexer correctly tokenizes `?:` as `TK_safe_method`
- Solitary `?` returns `TK_if_empty` as before

---

### Step 3: Implement Safe Field Access Helper

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
Create `expr_safe_field()` function that uses the if-empty operator pattern:

```c
static void expr_safe_field(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  ExpDesc key, field_result;
  BCReg obj_reg, result_reg;
  BCPos check_nil, skip_field;

  /* Compile-time nil optimization */
  expr_discharge(fs, v);
  if (v->k == VKNIL) {
    expr_str(ls, &key);  /* Consume field name token */
    expr_init(v, VKNIL, 0);
    return;
  }

  /* Get object into a register */
  obj_reg = expr_toanyreg(fs, v);
  result_reg = obj_reg;  /* Reuse same register for result */

  /* Emit nil check (similar to if-empty operator logic in bcemit_binop) */
  ExpDesc nilv;
  expr_init(&nilv, VKNIL, 0);
  bcemit_INS(fs, BCINS_AD(BC_ISEQP, obj_reg, const_pri(&nilv)));
  check_nil = bcemit_jmp(fs);

  /* Not nil path: skip field access and return nil */
  /* (BC_ISEQP skips next instruction when equal) */
  skip_field = bcemit_jmp(fs);

  /* Nil path: return nil */
  jmp_patch(fs, check_nil, fs->pc);
  bcemit_AD(fs, BC_KPRI, result_reg, VKNIL);
  BCPos skip_nil = bcemit_jmp(fs);

  /* Field access path */
  jmp_patch(fs, skip_field, fs->pc);
  expr_str(ls, &key);
  expr_index(fs, v, &key);
  expr_toreg(fs, v, result_reg);

  /* Merge point */
  jmp_patch(fs, skip_nil, fs->pc);
  expr_init(v, VNONRELOC, result_reg);
}
```

**Key Points:**
- Follows the pattern from `bcemit_binop` for `OPR_IF_EMPTY` (lines 1213-1249)
- Only checks for nil (not false/zero/empty like full if-empty)
- Both nil and non-nil paths write to the same register
- Handles compile-time nil optimization

---

### Step 4: Implement Safe Method Call Helper

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
Create `expr_safe_method()` function:

```c
static void expr_safe_method(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  ExpDesc key, nilv;
  BCReg obj_reg, base;
  BCPos check_nil, skip_method, skip_nil;

  /* Get object into a register */
  obj_reg = expr_toanyreg(fs, v);
  base = fs->freereg;

  /* Emit nil check */
  expr_init(&nilv, VKNIL, 0);
  bcemit_INS(fs, BCINS_AD(BC_ISEQP, obj_reg, const_pri(&nilv)));
  check_nil = bcemit_jmp(fs);

  /* Not nil path: skip method call */
  skip_method = bcemit_jmp(fs);

  /* Nil path: set result to nil */
  jmp_patch(fs, check_nil, fs->pc);
  bcemit_AD(fs, BC_KPRI, base, VKNIL);
  if (fs->freereg <= base) fs->freereg = base+1;
  skip_nil = bcemit_jmp(fs);

  /* Method call path */
  jmp_patch(fs, skip_method, fs->pc);
  fs->freereg = base;
  expr_str(ls, &key);
  bcemit_method(fs, v, &key);
  parse_args(ls, v);

  /* Preserve VCALL for multi-return semantics */
  jmp_patch(fs, skip_nil, fs->pc);
  if (v->k == VCALL) {
    /* Keep as VCALL - method may return multiple values */
    v->u.s.aux = base;
  } else {
    expr_discharge(fs, v);
    expr_toreg(fs, v, base);
    expr_init(v, VNONRELOC, base);
  }
}
```

**Key Points:**
- Preserves `VCALL` for multi-return semantics (`local a, b = obj?.method()`)
- Arguments are only evaluated on the non-nil path
- Nil path returns single nil value

---

### Step 5: Implement Safe Index Access Helper

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
Create `expr_safe_bracket()` function (similar to `expr_safe_field`):

```c
static void expr_safe_bracket(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  ExpDesc key, nilv;
  BCReg obj_reg, result_reg;
  BCPos check_nil, skip_index, skip_nil;

  /* Get object into a register */
  obj_reg = expr_toanyreg(fs, v);
  result_reg = obj_reg;

  /* Emit nil check */
  expr_init(&nilv, VKNIL, 0);
  bcemit_INS(fs, BCINS_AD(BC_ISEQP, obj_reg, const_pri(&nilv)));
  check_nil = bcemit_jmp(fs);

  /* Not nil path: skip index access */
  skip_index = bcemit_jmp(fs);

  /* Nil path: return nil */
  jmp_patch(fs, check_nil, fs->pc);
  bcemit_AD(fs, BC_KPRI, result_reg, VKNIL);
  skip_nil = bcemit_jmp(fs);

  /* Index access path */
  jmp_patch(fs, skip_index, fs->pc);
  lj_lex_next(ls);  /* Consume '[' */
  expr(ls, &key);
  expr_toval(fs, &key);
  lex_check(ls, ']');
  expr_index(fs, v, &key);
  expr_toreg(fs, v, result_reg);

  /* Merge point */
  jmp_patch(fs, skip_nil, fs->pc);
  expr_init(v, VNONRELOC, result_reg);
}
```

---

### Step 6: Add Forward Declarations

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
Add forward declaration for `parse_args` since it's called before its definition:
```c
/* Forward declarations */
static void expr(LexState *ls, ExpDesc *v);
static void parse_args(LexState *ls, ExpDesc *e);
```

**Location:** Around line 2025, in the forward declarations section

---

### Step 7: Integrate into Expression Suffix Loop

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Location:** `expr_primary()` function, suffix parsing loop (around line 2503)

**Action:**
Add handling for safe navigation tokens:
```c
for (;;) {  /* Parse multiple expression suffixes. */
  if (ls->tok == TK_safe_field) {
    lj_lex_next(ls);
    expr_safe_field(ls, v);
  } else if (ls->tok == TK_safe_method) {
    lj_lex_next(ls);
    expr_safe_method(ls, v);
  } else if (ls->tok == TK_if_empty && lj_lex_lookahead(ls) == '[') {
    /* Handle ?[ as safe index access */
    lj_lex_next(ls);  /* Consume TK_if_empty */
    expr_safe_bracket(ls, v);
  } else if (ls->tok == '.') {
    expr_field(ls, v);
  } else if (ls->tok == '[') {
    /* ... existing code ... */
  } else if (ls->tok == ':') {
    /* ... existing code ... */
  }
  /* ... rest of existing code ... */
}
```

**Important:** For `?[`, check for `TK_if_empty` (not raw `'?'`) since the lexer returns `TK_if_empty` for solitary `?`.

---

### Step 8: Create Test Suite

**File:** `src/fluid/tests/test_safe_nav.fluid`

DONE

---

### Step 10: Build and Test

**Commands:**
```bash
# Clean LuaJIT artifacts (MSVC only)
cd src/fluid/luajit-2.1/src && rm -f *.o *.lib

# Build
cmake --build build/agents --config Debug --target fluid parasol_cmd --parallel

# Install
cmake --install build/agents --config Debug

# Run tests
cd src/fluid/tests
../../../build/agents-install/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/fluid/tests/test_safe_nav.fluid --gfx-driver=headless
```

---

### Step 11: Update Documentation

**File:** `docs/wiki/Fluid-Reference-Manual.md`

**Action:**
Add section documenting the safe navigation operator:

```markdown
## Safe Navigation Operator

The safe navigation operator (`?.`) provides null-safe access to object fields, methods, and indexes.

### Syntax

```lua
obj?.field           -- Safe field access
obj?.method()        -- Safe method call
obj?[key]            -- Safe index access
obj?.a?.b?.c         -- Chaining
```

### Behavior

If the object is `nil`, the safe navigation operator returns `nil` without attempting to access the field/method/index. This prevents "attempt to index a nil value" errors.

**Important:** The safe navigation operator only checks for `nil`. Other falsey values like `false`, `0`, or `""` are treated as valid objects and field access proceeds normally.

### Examples

```lua
-- Safe field access
local user = nil
local name = user?.name  -- Returns nil instead of error

local user2 = { name = "Alice" }
local name2 = user2?.name  -- Returns "Alice"

-- Chaining
local city = user?.profile?.address?.city  -- Returns nil if any level is nil

-- With default values using if-empty operator
local displayName = user?.name ? "Guest"  -- "Guest" if user or name is nil

-- Safe method calls
local result = obj?.calculate()  -- Returns nil if obj is nil

-- Multiple return values preserved
local a, b = obj?.getTwoValues()  -- Both a and b will be nil if obj is nil

-- Safe index access
local value = table?[key]  -- Returns nil if table is nil
```

### Integration with Other Operators

The safe navigation operator works seamlessly with:
- **If-empty operator (`?`)**: `obj?.field ? "default"`
- **Chaining**: `obj?.a?.b?.c`
- **Method calls**: Return values and multi-return preserved

### Differences from If-Empty Operator

| Operator | Checks | Use Case |
|----------|--------|----------|
| `?` (if-empty) | nil, false, 0, "" | Provide defaults for any falsey value |
| `?.` (safe nav) | nil only | Safe access to potentially nil objects |

```lua
local obj = { count = 0 }

-- If-empty treats 0 as falsey
local v1 = obj.count ? 10  -- Returns 10 (because 0 is falsey)

-- Safe nav treats 0 as valid
local v2 = obj?.count  -- Returns 0 (because obj is not nil)
```
```

---

## Key Differences from Original Plan

### Simpler Implementation
- Leverages existing if-empty operator bytecode patterns
- Only checks for nil (not false/zero/empty like full if-empty)
- Reuses proven register management and jump patching

### Better Integration
- Works naturally with existing `?` operator for defaults
- No need for custom bytecode instructions
- Follows established patterns in the codebase

### Reduced Complexity
- ~100 lines of code vs. ~200+ in original plan
- No need to understand/modify complex ternary operator logic
- Easier to maintain and debug

## Implementation Challenges

### Challenge 1: Token Recognition for `?[`

**Issue:** Lexer returns `TK_if_empty` for solitary `?`, not raw `'?'` character.

**Solution:** In suffix loop, check for `ls->tok == TK_if_empty && lj_lex_lookahead(ls) == '['`

### Challenge 2: Register Management

**Issue:** Both nil and non-nil paths must write to same register.

**Solution:** Follow pattern from if-empty operator - allocate result register upfront, both paths write to it.

### Challenge 3: Multi-Return Method Calls

**Issue:** `local a, b = obj?.method()` should receive multiple returns when obj is not nil.

**Solution:** Check if `v->k == VCALL` after method call and preserve it instead of discharging to single value.

### Challenge 4: Argument Evaluation

**Issue:** Method arguments should not be evaluated when object is nil.

**Solution:** Emit nil check BEFORE calling `parse_args()`, only parse arguments in non-nil branch.

---

## Testing Checklist

- [ ] `nil?.field` returns `nil`
- [ ] `obj?.field` returns field value
- [ ] `nil?.method()` returns `nil`
- [ ] `obj?.method()` calls method
- [ ] `nil?[key]` returns `nil`
- [ ] `obj?[key]` returns indexed value
- [ ] Chaining: `nil?.a?.b` returns `nil`
- [ ] Chaining: `obj?.a?.b` accesses nested fields
- [ ] Arguments not evaluated: `nil?.method(sideEffect())` doesn't call `sideEffect()`
- [ ] Multi-return: `local a, b = obj?.method()` gets both values
- [ ] False/zero/empty not treated as nil: `{flag=false}?.flag` returns `false`
- [ ] Integration with `?`: `nil?.field ? "default"` returns `"default"`
- [ ] Compile-time optimization: `nil?.field` doesn't generate runtime checks

---

## Implementation Order

### Phase 1: Lexer (1-2 hours)
- Step 1-2: Add tokens, modify lexer
- Test: Verify tokens are emitted correctly

### Phase 2: Field Access (2-3 hours)
- Steps 3, 6, 7: Implement safe field access
- Test: `obj?.field` works

### Phase 3: Method and Index (2-3 hours)
- Steps 4-5, 7: Implement safe method and index
- Test: `obj?.method()` and `obj?[key]` work

### Phase 4: Testing (2-3 hours)
- Steps 8-10: Comprehensive test suite
- Fix any issues found

### Phase 5: Documentation (1-2 hours)
- Step 11: User documentation

---

## References

### Code References
- If-empty operator: `bcemit_binop()` handling of `OPR_IF_EMPTY` (lj_parse.c:1190-1256)
- Field access: `expr_field()` (lj_parse.c:2068-2076)
- Method calls: `bcemit_method()` (lj_parse.c:689-735)
- Ternary operator: Extended falsey checks (lj_parse.c:2920-2984)

### External References
- JavaScript: Optional chaining (`?.`)
- Kotlin: Safe call operator (`?.`)
- Swift: Optional chaining (`?.`)
- C#: Null-conditional operator (`?.`)

---

**Last Updated:** 2025-01-07
**Status:** Planning Complete - Ready for Implementation
**Related:** If-empty operator (`?`), ternary operator, expression parsing
