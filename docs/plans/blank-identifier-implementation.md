# Blank Identifier (`_`) Implementation Plan

## Overview

This document provides a detailed step-by-step implementation plan for the blank identifier `_` in Fluid/LuaJIT. The `_` identifier allows explicitly ignoring return values, making code intent clear and preventing accidental storage of unwanted values.

**Status:** 📋 **Implementation Plan** - Not yet started

**Priority:** ⭐⭐⭐ **Medium**

**Estimated Effort:** 6-10 hours

## Feature Specification

### Syntax

```lua
-- In assignments
_, err := openFile("data.txt")
_, _, result := someFunction()

-- In for loops
for _, value in ipairs(t) do
   print(value)
end

for _, v in pairs(t) do
   print(v)
end

-- With regular assignment
local _, status = process()
```

### Semantics

- **No storage**: `_` does not allocate a variable slot or register
- **Explicit intent**: Makes it clear that a value is intentionally ignored
- **Value consumption**: Still consumes/ignores the value from RHS (for proper stack management)
- **All contexts**: Works in assignments, `:=` declarations, and `for` loops
- **Reusable**: Can be used multiple times in the same statement
- **Not a variable**: Cannot be referenced later (not stored, so reading it would be an error)

### Examples

```lua
-- Ignore error, keep file
file, _ := openFile("data.txt")

-- Ignore first two values
_, _, result := someFunction()

-- Loop without index
for _, value in ipairs(t) do
   print(value)
end

-- Multiple ignores
function process()
   _, _, status, _ := complexOperation()
   return status
end

-- Error: cannot read _
local x = _  -- Error: _ is not a variable
```

## Implementation Steps

### Step 1: Define Blank Identifier Constant

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add constant for blank identifier name (around line 114-115):
   ```c
   #define NAME_BREAK		((GCstr *)(uintptr_t)1)
   #define NAME_CONTINUE	((GCstr *)(uintptr_t)2)
   #define NAME_BLANK		((GCstr *)(uintptr_t)3)
   ```

**Alternative Approach:** Instead of a special pointer value, check the string name directly:
```c
/* Check if name is blank identifier */
#define is_blank_name(name) \
   ((name) != NULL && (name)->len == 1 && *(strdata(name)) == '_')
```

**Recommendation:** Use string comparison approach - it's more explicit and doesn't require special pointer values.

**Expected Result:**
- Blank identifier can be detected
- Helper function/macro available

---

### Step 2: Create Helper Function to Check Blank Identifier

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add helper function to check if a name is the blank identifier:
   ```c
   /* Check if a string is the blank identifier '_' */
   static int is_blank_identifier(GCstr *name)
   {
     return (name != NULL && name->len == 1 &&
             *(strdata(name)) == '_');
   }
   ```

**Location:** Add near other helper functions (around line 1280-1300).

**Expected Result:**
- Function to detect blank identifier
- Can be used throughout parser

---

### Step 3: Handle Blank Identifier in Variable Creation

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `var_new()` or create wrapper to skip blank identifier:
   ```c
   /* Define a new local variable (or skip if blank identifier). */
   static int var_new_or_blank(LexState *ls, BCReg n, GCstr *name)
   {
     if (is_blank_identifier(name)) {
        /* Don't create variable for blank identifier */
        return 0;  /* Indicate no variable created */
     }
     var_new(ls, n, name);
     return 1;  /* Variable created */
   }
   ```

**Note:** Actually, we might not want to modify `var_new()` directly. Instead, check before calling it.

**Better Approach:** Check for blank identifier before calling `var_new()` in all places where variables are created.

**Expected Result:**
- Blank identifier doesn't create variables
- Normal variables still work

---

### Step 4: Handle Blank Identifier in Local Declarations

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_local()` to skip blank identifiers:
   ```c
   /* Parse 'local' statement. */
   static void parse_local(LexState *ls)
   {
     if (lex_opt(ls, TK_function)) {  /* Local function declaration. */
        /* ... existing code ... */
     } else {  /* Local variable declaration. */
        ExpDesc e;
        BCReg nexps, nvars = 0, blank_count = 0;
        do {  /* Collect LHS. */
           GCstr *name = lex_str(ls);
           if (is_blank_identifier(name)) {
              blank_count++;
              /* Don't create variable */
           } else {
              var_new(ls, nvars++, name);
           }
        } while (lex_opt(ls, ','));

        if (lex_opt(ls, '=')) {  /* Optional RHS. */
           nexps = expr_list(ls, &e);
        } else {  /* Or implicitly set to nil. */
           e.k = VVOID;
           nexps = 0;
        }

        /* Adjust for blank identifiers - they consume values but don't store */
        assign_adjust(ls, nvars + blank_count, nexps, &e);
        var_add(ls, nvars);  /* Only add non-blank variables */

        /* Assign values to non-blank variables */
        /* Need to skip blank positions when assigning */
        if (nvars > 0) {
           /* Assign to variables, skipping blank positions */
           /* This is complex - might need to track which positions are blank */
        }
     }
   }
   ```

**Challenge:** Handling mixed blank and non-blank variables: `local x, _, y = 1, 2, 3`

**Simpler Approach:** Track blank positions separately and adjust assignment accordingly.

**Expected Result:**
- `local _, x = f()` works correctly
- Blank identifiers don't create variables
- Values are still consumed

---

### Step 5: Handle Blank Identifier in Short Declarations (`:=`)

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_short_decl()` (from short declaration plan) to handle blank identifiers:
   ```c
   /* Parse short variable declaration: name := value */
   static void parse_short_decl(LexState *ls, GCstr *first_name)
   {
     FuncState *fs = ls->fs;
     ExpDesc e;
     BCReg nexps, nvars = 0, blank_count = 0;

     /* Process first name */
     if (is_blank_identifier(first_name)) {
        blank_count++;
     } else {
        BCReg reg = var_lookup_local(fs, first_name);
        if ((int32_t)reg >= 0) {
           lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(first_name));
        }
        var_new(ls, nvars++, first_name);
     }

     /* Collect remaining names */
     while (lex_opt(ls, ',')) {
        GCstr *name = lex_str(ls);
        if (is_blank_identifier(name)) {
           blank_count++;
        } else {
           BCReg reg = var_lookup_local(fs, name);
           if ((int32_t)reg >= 0) {
              lj_lex_error(ls, ls->tok, LJ_ERR_XREDECL, strdata(name));
           }
           var_new(ls, nvars++, name);
        }
     }

     /* Require := token */
     lex_check(ls, TK_colon_eq);

     /* Parse RHS */
     nexps = expr_list(ls, &e);

     /* Adjust for blank identifiers */
     assign_adjust(ls, nvars + blank_count, nexps, &e);
     var_add(ls, nvars);  /* Only add non-blank variables */

     /* Assign values (similar to parse_local) */
   }
   ```

**Expected Result:**
- `_, err := openFile()` works correctly
- Blank identifiers don't create variables
- Values consumed properly

---

### Step 6: Handle Blank Identifier in Regular Assignments

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_assignment()` to handle blank identifiers:
   ```c
   /* Recursively parse assignment statement. */
   static void parse_assignment(LexState *ls, LHSVarList *lh, BCReg nvars)
   {
     ExpDesc e;

     /* Check if current LHS is blank identifier */
     if (lh->v.k == VGLOBAL) {
        GCstr *name = lh->v.u.sval;
        if (is_blank_identifier(name)) {
           /* Blank identifier - don't assign, but consume value */
           if (lex_opt(ls, ',')) {
              /* More LHS variables */
              LHSVarList vl;
              vl.prev = lh;
              expr_primary(ls, &vl.v);
              parse_assignment(ls, &vl, nvars+1);
              /* Skip assignment for blank */
              return;
           } else {
              /* Parse RHS */
              lex_check(ls, '=');
              nexps = expr_list(ls, &e);
              /* Consume first value but don't store */
              if (nexps > 0) {
                 /* Drop first value from stack */
                 fs->freereg--;  /* Or adjust appropriately */
              }
              return;
           }
        }
     }

     checkcond(ls, VLOCAL <= lh->v.k && lh->v.k <= VINDEXED, LJ_ERR_XSYNTAX);
     /* ... rest of existing code ... */
   }
   ```

**Challenge:** Blank identifier appears as `VGLOBAL` when parsed with `expr_primary()`. Need to detect it before processing.

**Better Approach:** Check for blank identifier in `expr_primary()` or handle it specially when parsing LHS.

**Expected Result:**
- `_, x = f()` works correctly
- Blank identifiers don't cause assignment errors

---

### Step 7: Handle Blank Identifier in For Loops

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_for_iter()` to handle blank identifiers:
   ```c
   /* Parse 'for' iterator. */
   static void parse_for_iter(LexState *ls, GCstr *indexname)
   {
     FuncState *fs = ls->fs;
     ExpDesc e;
     BCReg nvars = 0, blank_count = 0;
     BCLine line;
     BCReg base = fs->freereg + 3;
     BCPos loop, loopend, iter, exprpc = fs->pc;
     FuncScope bl;
     int isnext;

     /* Hidden control variables. */
     var_new_fixed(ls, nvars++, VARNAME_FOR_GEN);
     var_new_fixed(ls, nvars++, VARNAME_FOR_STATE);
     var_new_fixed(ls, nvars++, VARNAME_FOR_CTL);

     /* Visible variables returned from iterator. */
     if (is_blank_identifier(indexname)) {
        blank_count++;
        /* Don't create variable */
     } else {
        var_new(ls, nvars++, indexname);
     }

     while (lex_opt(ls, ',')) {
        GCstr *name = lex_str(ls);
        if (is_blank_identifier(name)) {
           blank_count++;
           /* Don't create variable */
        } else {
           var_new(ls, nvars++, name);
        }
     }

     lex_check(ls, TK_in);
     line = ls->linenumber;
     assign_adjust(ls, 3, expr_list(ls, &e), &e);
     bcreg_bump(fs, 3+LJ_FR2);
     isnext = (nvars <= 5 && predict_next(ls, fs, exprpc));
     var_add(ls, 3);  /* Hidden control variables. */
     lex_check(ls, TK_do);
     loop = bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP);
     fscope_begin(fs, &bl, 0);  /* Scope for visible variables. */
     var_add(ls, nvars-3);  /* Only add non-blank variables */
     bcreg_reserve(fs, nvars-3);
     parse_block(ls);
     fscope_end(fs);

     /* Adjust iterator to account for blank identifiers */
     /* The iterator should still return all values, but we only store non-blank */
     iter = bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base,
                       (nvars-3)+1+blank_count, 2+1);
     loopend = bcemit_AJ(fs, BC_ITERL, base, NO_JMP);
     /* ... rest of existing code ... */
   }
   ```

**Challenge:** Iterator returns values that need to be consumed even if not stored. Need to ensure proper stack management.

**Expected Result:**
- `for _, value in ipairs(t) do` works correctly
- Blank identifiers don't create loop variables
- Iterator values are consumed properly

---

### Step 8: Handle Blank Identifier in Numeric For Loops

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `parse_for_num()` - actually, numeric for loops only have one variable, so blank identifier doesn't make sense:
   ```c
   /* Parse numeric 'for'. */
   static void parse_for_num(LexState *ls, GCstr *varname, BCLine line)
   {
     /* Check if blank identifier - error or allow? */
     if (is_blank_identifier(varname)) {
        /* Numeric for with blank identifier doesn't make sense */
        /* Could error or allow (index still needed for loop control) */
        /* Recommendation: Allow but warn or document */
     }
     /* ... rest of existing code ... */
   }
   ```

**Decision:** Allow blank identifier in numeric for loops? It doesn't make much sense, but could be allowed for consistency. Recommend allowing it (the index is still used internally).

**Expected Result:**
- `for _ = 1, 10 do` works (index not accessible in body)
- Or error if not allowed

---

### Step 9: Prevent Reading Blank Identifier

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `var_lookup()` or `expr_primary()` to error on reading blank identifier:
   ```c
   /* Lookup variable name. */
   #define var_lookup(ls, e) \
     var_lookup_((ls)->fs, lex_str(ls), (e), 1)

   /* In var_lookup_() or expr_primary(): */
   static MSize var_lookup_(FuncState *fs, GCstr *name, ExpDesc *e, int first)
   {
     /* Check if trying to read blank identifier */
     if (is_blank_identifier(name)) {
        /* Error: cannot read blank identifier */
        lj_lex_error(ls, ls->tok, LJ_ERR_XSYNTAX, "cannot read blank identifier '_'");
     }
     /* ... rest of existing code ... */
   }
   ```

**Location:** Add check in `expr_primary()` when parsing `TK_name` (around line 2278).

**Expected Result:**
- `local x = _` causes error
- Blank identifier cannot be read

---

### Step 10: Handle Stack Management for Blank Identifiers

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Ensure proper stack management when blank identifiers consume values:
   - Values from RHS are still evaluated and placed on stack
   - Blank identifiers consume (drop) values without storing
   - Need to adjust `freereg` appropriately

**Implementation:**
```c
/* When encountering blank identifier in assignment: */
/* Values are on stack at positions: freereg - n, freereg - n+1, ... */
/* Blank identifier at position i means: drop value at freereg - (total - i) */
/* Then assign remaining values to non-blank variables */

/* Helper to drop a value from stack */
static void drop_stack_value(FuncState *fs)
{
   /* Just adjust freereg - value will be overwritten */
   fs->freereg--;
}
```

**Expected Result:**
- Stack management correct
- No memory leaks or incorrect values

---

### Step 11: Create Test Suite

**File:** `src/fluid/tests/test_blank_identifier.fluid`

**Action:**
1. Create comprehensive test suite:
   ```lua
   -- Basic assignment with blank
   function testBasicAssignment()
      local x, _ = 10, 20
      assert(x is 10, "Failed: expected 10")
      -- _ should not be accessible
   end

   -- Multiple blanks
   function testMultipleBlanks()
      local _, _, x = 1, 2, 3
      assert(x is 3, "Failed: expected 3")
   end

   -- Short declaration with blank
   function testShortDecl()
      x, _ := 10, 20
      assert(x is 10, "Failed: expected 10")
   end

   -- For loop with blank
   function testForLoop()
      local t = {10, 20, 30}
      local sum = 0
      for _, value in ipairs(t) do
         sum += value
      end
      assert(sum is 60, "Failed: expected 60")
   end

   -- Multiple blanks in for loop
   function testForLoopMultiple()
      local t = {a=1, b=2, c=3}
      local count = 0
      for _, _, v in pairs(t) do
         count += 1
      end
      assert(count is 3, "Failed: expected 3")
   end

   -- Error: reading blank identifier
   function testReadError()
      -- This should error: local x = _
      -- Cannot read blank identifier
   end

   -- Function return with blank
   function testFunctionReturn()
      function getValues()
         return 1, 2, 3
      end
      local _, _, x = getValues()
      assert(x is 3, "Failed: expected 3")
   end

   -- All blanks (edge case)
   function testAllBlanks()
      _, _, _ = getValues()
      -- Should work but do nothing
   end

   return {
      tests = { 'testBasicAssignment', 'testMultipleBlanks', 'testShortDecl',
                'testForLoop', 'testForLoopMultiple', 'testReadError',
                'testFunctionReturn', 'testAllBlanks' }
   }
   ```

---

### Step 12: Update Documentation

**File:** `docs/wiki/Fluid-Reference-Manual.md`

**Action:**
1. Add blank identifier section:
   - Syntax and semantics
   - Examples in assignments, for loops, short declarations
   - Use cases (ignoring errors, skipping indices, etc.)
   - Restrictions (cannot read blank identifier)

---

## Implementation Challenges

### Challenge 1: Stack Management

**Issue:** Blank identifiers consume values but don't store them. Need proper stack management.

**Solution:**
- Values are evaluated and placed on stack
- Blank identifiers cause values to be dropped (not stored)
- Adjust `freereg` appropriately
- Use `assign_adjust()` with correct counts (including blanks)

---

### Challenge 2: Mixed Blank and Non-Blank Variables

**Issue:** `local x, _, y = 1, 2, 3` requires tracking which positions are blank.

**Solution:**
- Track blank count separately from variable count
- When assigning, skip blank positions
- Use array or bitmask to track blank positions if needed
- Or simpler: assign sequentially, skipping blanks

---

### Challenge 3: For Loop Iterator Values

**Issue:** Iterator returns values that need to be consumed even if not stored.

**Solution:**
- Iterator still returns all values
- Blank identifiers cause values to be dropped from stack
- Adjust iterator instruction to account for blank count
- Ensure proper register management

---

### Challenge 4: Detection in expr_primary()

**Issue:** Blank identifier appears as `VGLOBAL` when parsed with `expr_primary()`.

**Solution:**
- Check string name when `k == VGLOBAL`
- Detect blank identifier before processing
- Handle specially in assignment/declaration contexts

---

## Testing Checklist

- [ ] Basic assignment: `local x, _ = 10, 20`
- [ ] Multiple blanks: `local _, _, x = 1, 2, 3`
- [ ] Short declaration: `x, _ := 10, 20`
- [ ] For loop: `for _, value in ipairs(t) do`
- [ ] Multiple blanks in for: `for _, _, v in pairs(t) do`
- [ ] Function return: `_, _, x = getValues()`
- [ ] All blanks: `_, _, _ = getValues()`
- [ ] Error: reading blank identifier `local x = _`
- [ ] Mixed positions: `local x, _, y = 1, 2, 3`
- [ ] Numeric for loop: `for _ = 1, 10 do` (if allowed)
- [ ] Stack management (no leaks)
- [ ] Interaction with existing code

---

## Key Files to Modify

1. **`src/fluid/luajit-2.1/src/lj_parse.c`**
   - Add `is_blank_identifier()` helper
   - Modify `parse_local()` to handle blanks
   - Modify `parse_short_decl()` to handle blanks (when implemented)
   - Modify `parse_assignment()` to handle blanks
   - Modify `parse_for_iter()` to handle blanks
   - Modify `parse_for_num()` to handle blanks (if allowed)
   - Add error check for reading blank identifier

2. **`src/fluid/tests/test_blank_identifier.fluid`**
   - Create test suite

3. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add documentation

---

## Implementation Order

1. **Phase 1: Core Infrastructure** (1-2 hours)
   - Step 1-2: Helper function, detection
   - Basic detection working

2. **Phase 2: Assignments** (2-3 hours)
   - Step 4-6: Local declarations, short declarations, regular assignments
   - Assignments work with blanks

3. **Phase 3: Loops** (2-3 hours)
   - Step 7-8: For loops (iterator and numeric)
   - Loops work with blanks

4. **Phase 4: Error Handling** (1 hour)
   - Step 9: Prevent reading blank identifier

5. **Phase 5: Testing & Documentation** (1-2 hours)
   - Step 11-12: Test suite, documentation

---

## Design Decisions

### Decision 1: Allow Blank Identifier in Numeric For Loops?

**Options:**
- **Allow**: `for _ = 1, 10 do` - index not accessible but loop still works
- **Error**: Require a real variable name

**Recommendation:** Allow it for consistency, but document that the index is not accessible.

---

### Decision 2: Allow All Blanks?

**Options:**
- **Allow**: `_, _, _ = f()` - consumes values but stores nothing
- **Error**: At least one non-blank required

**Recommendation:** Allow it - it's valid and useful for consuming all return values.

---

### Decision 3: Blank Identifier as Variable Name?

**Options:**
- **Special**: `_` is always blank identifier, cannot be used as variable name
- **Context-dependent**: `_` can be a normal variable unless used in assignment/loop context

**Recommendation:** Special - `_` is always the blank identifier (simpler and clearer).

---

## References

### Similar Implementations
- Go's blank identifier: `_ = value`
- Python's convention: `_` for unused variables
- Rust's `_` pattern: Explicitly ignore values

### Related Code
- Variable creation: `var_new()` (line 1288)
- Local declarations: `parse_local()` (line 2831)
- For loops: `parse_for_iter()` (line 3128)
- Assignment: `parse_assignment()` (line 2771)

---

**Last Updated:** 2025-01-XX
**Status:** Implementation Plan - Ready for implementation
**Related:** Short variable declaration (`:=`), local declarations, for loops
