# Safe Navigation Operator Implementation Plan

## Overview

This document provides a detailed step-by-step implementation plan for the Safe Navigation Operator (`?.`) in Fluid/LuaJIT. This operator allows safe access to fields and methods on potentially nil objects, returning `nil` if the object is nil instead of raising an error.

**Status:** 📋 **Implementation Plan** - Not yet started

**Priority:** ⭐⭐⭐ **Medium**

**Estimated Effort:** 24-40 hours

## Feature Specification

### Syntax
```lua
obj?.field
obj?.method()
obj?[index]
obj?.field?.subfield
```

### Semantics
- **Short-circuit evaluation**: If object is `nil`, returns `nil` without accessing field/method
- **Field access**: `obj?.field` returns `nil` if `obj` is `nil`, otherwise returns `obj.field`
- **Method calls**: `obj?.method()` returns `nil` if `obj` is `nil`, otherwise calls `obj:method()`
- **Index access**: `obj?[key]` returns `nil` if `obj` is `nil`, otherwise returns `obj[key]`
- **Chaining**: Multiple safe navigation operators can chain: `obj?.field?.subfield`
- **Integration**: Works seamlessly with `??` and `or?` for default values

### Examples
```lua
local name = user?.profile?.name ?? "Guest"
local result = obj?.method() or? "default"
local value = table?[key] ?? 0
```

## Implementation Steps

### Step 1: Add Token Definition (if needed)

**File:** `src/fluid/luajit-2.1/src/lj_lex.h`

**Action:**
1. Check if `?.` needs a combined token or can be parsed as `?` followed by `.`
2. Since `?` and `.` are already separate tokens, we can parse `?.` as two tokens
3. **Decision:** No new token needed - parse `?` then check for `.`

**Expected Result:**
- Parser can recognize `?.` pattern without new token

---

### Step 2: Add Safe Navigation Flag to Expression Descriptor

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. We need to track whether an expression is safe-navigated (object was checked for nil)
2. **Option A:** Add flag to `ExpDesc` structure (may require struct changes)
3. **Option B:** Use existing expression state to track safe navigation
4. **Option C:** Handle safe navigation inline during field/method access

**Recommended Approach:** Option C - Handle inline, no structure changes needed

**Expected Result:**
- Parser can distinguish between regular field access and safe navigation

---

### Step 3: Modify Field Access Parsing

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Location:** `expr_primary()` function (around line 2268-2308)

**Action:**
1. Modify the suffix parsing loop to handle `?.`:
   ```c
   for (;;) {  /* Parse multiple expression suffixes. */
     if (ls->tok == '?') {
       /* Check for safe navigation ?. */
       LexState *ls_save = ls;
       lex_next(ls);  // Peek at next character
       if (ls->c == '.') {
         /* Safe navigation - emit nil check */
         expr_safe_field(ls, v);
       } else {
         /* Not safe nav - restore and handle as regular ? */
         ls = ls_save;
         break;  // Let ternary or other operators handle it
       }
     } else if (ls->tok == '.') {
       expr_field(ls, v);
     } else if (ls->tok == '[') {
       /* ... existing code ... */
     }
     /* ... rest of existing code ... */
   }
   ```

**Note:** This approach requires lookahead or token buffering. Alternative: check for `?` in lexer and return special token.

**Better Approach:** Handle `?` in `expr_primary()` and check if next token is `.`:
```c
if (ls->tok == '?') {
  lj_lex_next(ls);  // Consume '?'
  if (ls->tok == '.') {
    /* Safe navigation field access */
    expr_safe_field(ls, v);
    continue;
  } else {
    /* Not safe nav - restore ? token or handle as ternary */
    /* This requires token pushback or different approach */
  }
}
```

**Challenge:** Need to handle token pushback or use lookahead mechanism.

---

### Step 4: Implement Safe Field Access

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `expr_safe_field()` function:
   ```c
   static void expr_safe_field(LexState *ls, ExpDesc *v)
   {
     FuncState *fs = ls->fs;
     ExpDesc key;
     BCReg obj_reg, result_reg;
     BCPos nil_check, skip;

     /* Ensure object is in a register */
     obj_reg = expr_toanyreg(fs, v);

     /* Check if object is nil */
     ExpDesc nilv;
     expr_init(&nilv, VKNIL, 0);
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, obj_reg, const_pri(&nilv)));
     nil_check = bcemit_jmp(fs);

     /* Object is not nil - access field */
     lj_lex_next(ls);  // Consume '.'
     expr_str(ls, &key);
     expr_index(fs, v, &key);

     /* Emit field access (standard TGETS/TGETV) */
     expr_toanyreg(fs, v);
     result_reg = v->u.s.info;

     /* Jump to end (skip nil path) */
     skip = bcemit_jmp(fs);

     /* Object was nil - return nil */
     BCPos nil_path = fs->pc;
     expr_init(v, VKNIL, 0);
     bcemit_nil(fs, result_reg, 1);
     v->u.s.info = result_reg;
     v->k = VNONRELOC;

     /* Patch jumps */
     jmp_patch(fs, nil_check, nil_path);  // If nil, jump to nil path
     jmp_patch(fs, skip, fs->pc);         // If not nil, skip nil path
   }
   ```

**Key Considerations:**
- Nil check before field access
- Short-circuit to nil if object is nil
- Both paths must write to same register
- Handle register allocation carefully

---

### Step 5: Implement Safe Method Call

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `expr_safe_method()` function:
   ```c
   static void expr_safe_method(LexState *ls, ExpDesc *v)
   {
     FuncState *fs = ls->fs;
     ExpDesc key;
     BCReg obj_reg, result_reg;
     BCPos nil_check, skip;

     /* Ensure object is in a register */
     obj_reg = expr_toanyreg(fs, v);

     /* Check if object is nil */
     ExpDesc nilv;
     expr_init(&nilv, VKNIL, 0);
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, obj_reg, const_pri(&nilv)));
     nil_check = bcemit_jmp(fs);

     /* Object is not nil - setup method call */
     lj_lex_next(ls);  // Consume ':'
     expr_str(ls, &key);
     bcemit_method(fs, v, &key);

     /* Parse arguments and emit call */
     parse_args(ls, v);
     expr_discharge(fs, v);
     result_reg = v->u.s.info;

     /* Jump to end (skip nil path) */
     skip = bcemit_jmp(fs);

     /* Object was nil - return nil */
     BCPos nil_path = fs->pc;
     bcemit_nil(fs, result_reg, 1);
     expr_init(v, VNONRELOC, result_reg);

     /* Patch jumps */
     jmp_patch(fs, nil_check, nil_path);
     jmp_patch(fs, skip, fs->pc);
   }
   ```

**Key Considerations:**
- Method call setup before nil check
- Method call happens only if object is not nil
- Both paths must have same result register

---

### Step 6: Implement Safe Index Access

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `expr_safe_bracket()` function:
   ```c
   static void expr_safe_bracket(LexState *ls, ExpDesc *v)
   {
     FuncState *fs = ls->fs;
     ExpDesc key;
     BCReg obj_reg, result_reg;
     BCPos nil_check, skip;

     /* Ensure object is in a register */
     obj_reg = expr_toanyreg(fs, v);

     /* Check if object is nil */
     ExpDesc nilv;
     expr_init(&nilv, VKNIL, 0);
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, obj_reg, const_pri(&nilv)));
     nil_check = bcemit_jmp(fs);

     /* Object is not nil - access index */
     expr_bracket(ls, &key);
     expr_index(fs, v, &key);

     /* Emit index access (standard TGETV/TGETS/TGETB) */
     expr_toanyreg(fs, v);
     result_reg = v->u.s.info;

     /* Jump to end (skip nil path) */
     skip = bcemit_jmp(fs);

     /* Object was nil - return nil */
     BCPos nil_path = fs->pc;
     bcemit_nil(fs, result_reg, 1);
     expr_init(v, VNONRELOC, result_reg);

     /* Patch jumps */
     jmp_patch(fs, nil_check, nil_path);
     jmp_patch(fs, skip, fs->pc);
   }
   ```

---

### Step 7: Integrate Safe Navigation into expr_primary()

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Location:** `expr_primary()` function suffix parsing loop

**Action:**
1. Modify the loop to handle `?.`, `?:`, `?[`:
   ```c
   for (;;) {  /* Parse multiple expression suffixes. */
     if (ls->tok == '?') {
       /* Check for safe navigation operators */
       lj_lex_next(ls);  // Consume '?'
       if (ls->tok == '.') {
         /* Safe field access: obj?.field */
         expr_safe_field(ls, v);
       } else if (ls->tok == ':') {
         /* Safe method call: obj?:method() */
         expr_safe_method(ls, v);
       } else if (ls->tok == '[') {
         /* Safe index access: obj?[key] */
         expr_safe_bracket(ls, v);
       } else {
         /* Not safe navigation - restore ? for ternary or other operators */
         /* This requires token pushback - may need different approach */
         break;
       }
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

**Challenge:** Token pushback - if `?` is not followed by `.`, `:`, or `[`, we need to restore it for ternary operator or other uses.

**Solution Options:**
1. **Lookahead mechanism** - Check next token without consuming `?`
2. **Token buffering** - Save and restore tokens
3. **Separate parsing path** - Handle `?` in `expr_simple()` before `expr_primary()`

**Recommended:** Use lookahead by checking `ls->lookahead` or modifying lexer to support peek.

---

### Step 8: Implement Token Lookahead (if needed)

**File:** `src/fluid/luajit-2.1/src/lj_lex.c` and `lj_lex.h`

**Action:**
1. Check if lookahead mechanism exists (already present in `LexState`)
2. Looking at `lj_lex.h`, `LexState` has `lookahead` and `lookaheadval` fields
3. Use `lj_lex_lookahead()` function if available, or implement peek mechanism

**Alternative:** Modify lexer to handle `?.` as a single token:
```c
case '?':
  lex_next(ls);
  if (ls->c == '.') {
    lex_next(ls);
    return TK_safe_field;  // New token
  } else if (ls->c == ':') {
    lex_next(ls);
    return TK_safe_method;  // New token
  } else if (ls->c == '[') {
    // Can't combine with '[', handle separately
    return '?';  // Return '?' and let parser handle '?['
  }
  // ... existing coalesce and or? handling ...
  return '?';
```

**Better Approach:** Add `TK_safe_field` and `TK_safe_method` tokens to lexer.

---

### Step 9: Add Safe Navigation Tokens to Lexer

**File:** `src/fluid/luajit-2.1/src/lj_lex.h`

**Action:**
1. Add tokens to `TKDEF` macro:
   ```c
   __(safe_field, ?.) __(safe_method, ?:)
   ```

**File:** `src/fluid/luajit-2.1/src/lj_lex.c`

**Action:**
1. Modify `?` case in `lex_scan()`:
   ```c
   case '?':
     lex_next(ls);
     if (ls->c == '.') {
       lex_next(ls);
       return TK_safe_field;
     } else if (ls->c == ':') {
       lex_next(ls);
       return TK_safe_method;
     } else if (ls->c == '?') {
       lex_next(ls);
       return TK_coalesce;
     }
     /* Check for or? - existing code in identifier handling */
     return '?';
   ```

**Note:** For `?[`, we'll handle it as `?` followed by `[` in the parser, since `[` is a separate token.

---

### Step 10: Update expr_primary() for Safe Navigation Tokens

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify suffix parsing loop:
   ```c
   for (;;) {  /* Parse multiple expression suffixes. */
     if (ls->tok == TK_safe_field) {
       /* Safe field access: obj?.field */
       lj_lex_next(ls);  // Consume TK_safe_field
       expr_safe_field(ls, v);
     } else if (ls->tok == TK_safe_method) {
       /* Safe method call: obj?:method() */
       lj_lex_next(ls);  // Consume TK_safe_method
       expr_safe_method(ls, v);
     } else if (ls->tok == '?') {
       /* Check for safe index: obj?[key] */
       lj_lex_next(ls);  // Consume '?'
       if (ls->tok == '[') {
         expr_safe_bracket(ls, v);
       } else {
         /* Not safe index - restore for ternary or other operators */
         /* This requires token pushback or different approach */
         break;
       }
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

**Challenge:** For `?[`, we consume `?` but then need to check for `[`. If it's not `[`, we need to restore `?` for ternary operator.

**Solution:** Use a temporary token or check lookahead before consuming `?`.

---

### Step 11: Handle Chaining

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Ensure safe navigation functions handle chaining correctly:
   - `obj?.field?.subfield` should work
   - Each `?.` adds a nil check
   - Chaining should be handled by the loop in `expr_primary()`

**Implementation:**
- The loop in `expr_primary()` already handles multiple suffixes
- Each `?.` will be processed in sequence
- Each adds its own nil check before accessing the next level

**Example Flow:**
```c
obj?.field?.subfield
1. Parse `obj` - VNONRELOC
2. See `TK_safe_field` - emit nil check, access `field`
3. Result is VINDEXED (field access)
4. See `TK_safe_field` again - emit nil check on field result, access `subfield`
5. Final result
```

**Key Consideration:** Each safe navigation operation needs to check if its input is nil before proceeding.

---

### Step 12: Optimize Compile-Time Constants

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add optimization for compile-time nil objects:
   ```c
   static void expr_safe_field(LexState *ls, ExpDesc *v)
   {
     FuncState *fs = ls->fs;

     /* Check if object is compile-time nil */
     expr_discharge(fs, v);
     if (v->k == VKNIL) {
       /* Object is nil - return nil without field access */
       /* v is already nil, just return */
       return;
     }

     /* ... rest of runtime nil check ... */
   }
   ```

**Expected Result:**
- Compile-time nil objects don't generate unnecessary bytecode
- Field access is skipped entirely for known nil values

---

### Step 13: Handle Method Call Arguments

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Ensure `expr_safe_method()` properly handles method arguments:
   - Method call setup happens after nil check
   - Arguments are parsed only if object is not nil
   - Short-circuit ensures arguments are not evaluated if object is nil

**Implementation:**
- Nil check before `bcemit_method()` and `parse_args()`
- If nil, jump to nil path without evaluating arguments
- If not nil, proceed with normal method call

---

### Step 14: Create Test Suite

**File:** `src/fluid/tests/test_safe_nav.fluid`

**Action:**
1. Create comprehensive test suite:
   ```lua
   -- Basic safe field access
   function testSafeField()
      local obj = nil
      local v = obj?.field
      assert(v is nil, "Failed safe field: expected nil")

      local obj2 = { field = "value" }
      local v2 = obj2?.field
      assert(v2 is "value", "Failed safe field: expected 'value'")
   end

   -- Safe method call
   function testSafeMethod()
      local obj = nil
      local v = obj?:method()
      assert(v is nil, "Failed safe method: expected nil")

      local obj2 = { method = function(self) return "result" end }
      local v2 = obj2?:method()
      assert(v2 is "result", "Failed safe method: expected 'result'")
   end

   -- Safe index access
   function testSafeIndex()
      local obj = nil
      local v = obj?[key]
      assert(v is nil, "Failed safe index: expected nil")

      local obj2 = { key = "value" }
      local v2 = obj2?["key"]
      assert(v2 is "value", "Failed safe index: expected 'value'")
   end

   -- Chaining
   function testChaining()
      local obj = nil
      local v = obj?.field?.subfield
      assert(v is nil, "Failed chaining: expected nil")

      local obj2 = { field = { subfield = "value" } }
      local v2 = obj2?.field?.subfield
      assert(v2 is "value", "Failed chaining: expected 'value'")
   end

   -- Short-circuit with arguments
   function testShortCircuitArgs()
      local function expensive()
         error("Should not be called")
      end
      local obj = nil
      local v = obj?:method(expensive())
      assert(v is nil, "Failed short-circuit args")
   end

   -- Integration with coalesce
   function testIntegrationCoalesce()
      local obj = nil
      local v = obj?.field ?? "default"
      assert(v is "default", "Failed coalesce integration")
   end

   -- Integration with or?
   function testIntegrationOrQ()
      local obj = nil
      local v = obj?.field or? "default"
      assert(v is "default", "Failed or? integration")
   end

   return {
      tests = { 'testSafeField', 'testSafeMethod', 'testSafeIndex', 'testChaining',
                'testShortCircuitArgs', 'testIntegrationCoalesce', 'testIntegrationOrQ' }
   }
   ```

---

### Step 15: Update Documentation

**File:** `docs/wiki/Fluid-Reference-Manual.md`

**Action:**
1. Add Safe Navigation Operator section:
   - Syntax and semantics
   - Examples for field, method, and index access
   - Chaining behavior
   - Integration with `??` and `or?`
   - Short-circuit behavior

---

## Implementation Challenges

### Challenge 1: Token Recognition

**Issue:** `?` is used for multiple operators (`??`, `or?`, ternary `? :`, safe nav `?.`)

**Solution:**
- Handle `?.` and `?:` in lexer as combined tokens (`TK_safe_field`, `TK_safe_method`)
- Handle `?[` in parser by checking for `[` after consuming `?`
- Ternary operator checks for `:` after `?` (not `?` then `.`)

### Challenge 2: Token Pushback

**Issue:** When checking `?` for safe navigation, if it's not followed by `.`, `:`, or `[`, we need to restore it for ternary

**Solution:**
- Use lookahead mechanism (`ls->lookahead`)
- Or handle `?` in lexer with multiple checks
- Or use separate parsing path for ternary vs safe navigation

### Challenge 3: Register Management

**Issue:** Both nil and non-nil paths must write to same register

**Solution:**
- Allocate result register before nil check
- Both paths write to same register
- Ensure proper register cleanup

### Challenge 4: Method Call Setup

**Issue:** Method calls require setup before nil check, but arguments should not be evaluated if nil

**Solution:**
- Check nil before method setup
- If nil, skip method setup and argument parsing entirely
- Jump directly to nil result

### Challenge 5: Chaining Complexity

**Issue:** Multiple `?.` operators chain: `obj?.field?.subfield`

**Solution:**
- Each `?.` in the loop processes independently
- Each adds its own nil check before accessing next level
- Result of previous safe navigation becomes input to next

---

## Key Files to Modify

1. **`src/fluid/luajit-2.1/src/lj_lex.h`**
   - Add `TK_safe_field` and `TK_safe_method` tokens

2. **`src/fluid/luajit-2.1/src/lj_lex.c`**
   - Handle `?.` and `?:` in lexer

3. **`src/fluid/luajit-2.1/src/lj_parse.c`**
   - Implement `expr_safe_field()`, `expr_safe_method()`, `expr_safe_bracket()`
   - Modify `expr_primary()` to handle safe navigation tokens
   - Handle `?[` pattern for safe index access

4. **`src/fluid/tests/test_safe_nav.fluid`**
   - Create comprehensive test suite

5. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add safe navigation operator documentation

---

## Implementation Order

### Phase 1: Lexer and Token Recognition (4-6 hours)
- Steps 1, 9: Add tokens, modify lexer
- Basic token recognition working

### Phase 2: Field Access (6-8 hours)
- Steps 2, 4, 10: Implement safe field access
- Basic `obj?.field` working

### Phase 3: Method and Index Access (6-8 hours)
- Steps 5, 6: Implement safe method and index access
- `obj?:method()` and `obj?[key]` working

### Phase 4: Chaining and Integration (4-6 hours)
- Step 11: Handle chaining
- Integration with `??` and `or?`

### Phase 5: Optimization and Testing (4-6 hours)
- Steps 12, 14: Optimize constants, comprehensive testing

### Phase 6: Documentation (2-4 hours)
- Step 15: User documentation

---

## Testing Checklist

- [ ] Basic safe field: `nil?.field` returns `nil`
- [ ] Basic safe field: `obj?.field` returns value
- [ ] Safe method: `nil?:method()` returns `nil`
- [ ] Safe method: `obj?:method()` calls method
- [ ] Safe index: `nil?[key]` returns `nil`
- [ ] Safe index: `obj?[key]` returns value
- [ ] Chaining: `nil?.field?.subfield` returns `nil`
- [ ] Chaining: `obj?.field?.subfield` works correctly
- [ ] Short-circuit: Arguments not evaluated when object is nil
- [ ] Integration: Works with `??` operator
- [ ] Integration: Works with `or?` operator
- [ ] Compile-time optimization: Nil constant doesn't generate bytecode
- [ ] Edge cases: Empty tables, zero, false

---

## Alternative Implementation Approaches

### Approach A: Library Function (Simpler, Less Efficient)

Instead of bytecode-level implementation, could provide a library function:
```lua
function safe_get(obj, key)
   return obj ~= nil and obj[key] or nil
end
```

**Pros:** Easy to implement, no parser changes
**Cons:** Less efficient, no short-circuiting for method arguments, verbose syntax

### Approach B: VM Instruction (Most Complex)

Add new bytecode instruction `BC_SAFE_GET` that handles nil checks at VM level.

**Pros:** Most efficient, clean bytecode
**Cons:** Requires VM changes, most complex

### Approach C: Parser-Level (Recommended)

Implement at parser level with bytecode emission (as outlined in this plan).

**Pros:** Good balance of efficiency and complexity
**Cons:** Moderate complexity, requires careful jump patching

---

## References

### Existing Implementations to Study
- Field access: `expr_field()` function (`lj_parse.c:1951-1959`)
- Method calls: `bcemit_method()` function (`lj_parse.c:689-735`)
- Index access: `expr_index()` function (`lj_parse.c:1919-1948`)
- Nil checks: Coalesce operator (`??`) implementation
- Short-circuit: `or?` operator implementation

### External References
- JavaScript: Optional chaining (`?.`)
- Kotlin: Safe call operator (`?.`)
- Swift: Optional chaining (`?.`)
- Ruby: Safe navigation (`&.`)

---

**Last Updated:** 2025-01-XX
**Status:** Implementation Plan - Ready for implementation
**Related:** `or?` operator implementation, ternary operator plan, expression parsing in `expr_primary()`
