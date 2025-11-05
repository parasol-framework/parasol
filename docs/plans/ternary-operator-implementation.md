# Ternary Operator Implementation Plan

## Overview

This document provides a detailed step-by-step implementation plan for the C-style ternary conditional operator (`condition ? true_value : false_value`) and its optional form (`condition ? default_value`) in Fluid/LuaJIT. The optional form (`condition ? default_value`) has been implemented as part of renaming `?` to `?`, and provides a default value pattern that returns the condition when truthy.

**Status:** 🔄 **In Progress** - Optional form (`condition ? default_value`) implemented by renaming `?` to `?`. Full ternary (`condition ? true_value : false_value`) still pending.

**Priority:** ⭐⭐⭐⭐⭐ **Highest**

**Estimated Effort:** 18-26 hours (increased due to optional form support)

## Feature Specification

### Syntax
```lua
-- Full ternary (two branches)
local result = condition ? value_if_true : value_if_false

-- Optional false branch (default value pattern)
local result = condition ? default_value
```

### Semantics
- **Extended falsey semantics**: Both forms treat `nil`, `false`, `0`, and `""` as falsey (consistent with `?` operator)
- **Short-circuit evaluation**: Only one branch (true or false) is evaluated
- **Precedence**: Lower than all arithmetic/relational operators, higher than assignment
- **Associativity**: Right-associative (like assignment)
- **Return value**: Returns the value from the selected branch
- **Full ternary**: If condition is truthy, return true branch; if falsey (nil, false, 0, ""), return false branch
- **Optional false branch**: When `:` is omitted, if condition is truthy, return condition; if falsey (nil, false, 0, ""), return default value

### Examples
```lua
-- Full ternary (extended falsey semantics)
local msg = error ? "Error occurred" : "Success"
-- If error is truthy, returns "Error occurred"
-- If error is falsey (nil, false, 0, ""), returns "Success"

local count = x ? 10 : 0
-- If x is truthy (non-zero, non-empty), returns 10
-- If x is falsey (nil, false, 0, ""), returns 0

local max = a > b ? a : b
local status = user ~= nil ? user.active ? "online" : "offline" : "unknown"

-- Optional false branch (default value pattern)
local msg = possible_msg ? "No message given"
-- If possible_msg is truthy, returns possible_msg
-- If possible_msg is falsey (nil, false, 0, ""), returns "No message given"

local name = user_name ? "Anonymous"
local count = result_count ? 0
```

## Implementation Steps

### Step 1: Add Token Definitions

**File:** `src/fluid/luajit-2.1/src/lj_lex.h`

**Action:**
1. Add ternary token to `TKDEF` macro:
   ```c
   __(ternary, ?:)
   ```
   Note: The `:` token is already used for labels, so we need to handle `?` followed by `:` specially.

**Expected Result:**
- `TK_ternary` token enum value created
- Token name string available for error messages

---

### Step 2: Update Lexer for Ternary Recognition

**File:** `src/fluid/luajit-2.1/src/lj_lex.c`

**Action:**
1. Modify the `?` case in `lex_scan()` (around line 416-419):
   ```c
   case '?':
     lex_next(ls);
     if (ls->c == ':') {  // Check for ?: (ternary)
       lex_next(ls);
       return TK_ternary;
     }
     // Check for ? - existing code
     return '?';
   ```

**Note:** However, `?:` as a single token won't work because we need to parse `condition ? true_expr : false_expr` where `?` and `:` are separate tokens. We need a different approach.

**Revised Approach:**
- `?` is already handled in the lexer (returns `'?'` token)
- `:` is already handled (returns `':'` token)
- We don't need a combined token - we'll parse `?` and `:` separately

**Action:**
1. No lexer changes needed - `?` and `:` are already valid tokens
2. The parser will recognize the ternary pattern: `expr ? expr : expr`

---

### Step 3: Add Ternary Operator to Parser Enum

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add `OPR_TERNARY` to the `BinOpr` enum (around line 154-161):
   ```c
   typedef enum BinOpr {
     OPR_ADD, OPR_SUB, OPR_MUL, OPR_DIV, OPR_MOD, OPR_POW,  /* ORDER ARITH */
     OPR_CONCAT,
     OPR_NE, OPR_EQ,
     OPR_LT, OPR_GE, OPR_LE, OPR_GT,
     OPR_BAND, OPR_BOR, OPR_BXOR, OPR_SHL, OPR_SHR,
     OPR_AND, OPR_OR, OPR_COALESCE, OPR_OR_QUESTION,
     OPR_TERNARY,
     OPR_NOBINOPR
   } BinOpr;
   ```

**Expected Result:**
- `OPR_TERNARY` enum value available for parser logic

---

### Step 4: Add Ternary to Priority Table

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Add priority entry for ternary operator (around line 210-216):
   ```c
   static const struct {
     uint8_t left;		/* Left priority. */
     uint8_t right;	/* Right priority. */
     const char* name;	/* Name for bitlib function (if applicable). */
     uint8_t name_len;	/* Cached name length for bitlib lookups. */
   } priority[] = {
     {6,6,NULL,0}, {6,6,NULL,0}, {7,7,NULL,0}, {7,7,NULL,0}, {7,7,NULL,0},	/* ADD SUB MUL DIV MOD */
     {10,9,NULL,0}, {5,4,NULL,0},					/* POW CONCAT (right associative) */
     {3,3,NULL,0}, {3,3,NULL,0},					/* EQ NE */
     {3,3,NULL,0}, {3,3,NULL,0}, {3,3,NULL,0}, {3,3,NULL,0},		/* LT GE GT LE */
     {5,4,"band",4}, {3,2,"bor",3}, {4,3,"bxor",4}, {7,5,"lshift",6}, {7,5,"rshift",6},	/* BAND BOR BXOR SHL SHR */
     {2,2,NULL,0}, {1,1,NULL,0}, {1,1,NULL,0}, {1,1,NULL,0},			/* AND OR COALESCE OR_QUESTION */
     {1,1,NULL,0}			/* TERNARY */
   };
   ```

**Note:** Ternary has same priority as `or`, `and`, `?` (lowest priority). It's right-associative.

**Expected Result:**
- Ternary operator has correct precedence (lowest, same as logical operators)

---

### Step 5: Parse Ternary Expression in Parser

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `expr_binop()` to handle ternary operator parsing:
   - After parsing a binary expression, check if current token is `?`
   - If yes, check if next token is `:` (full ternary) or something else (optional false branch)
   - Parse accordingly: `condition ? true_expr : false_expr` or `condition ? default_value`

**Location:** `expr_binop()` function (around line 2511)

**Implementation Approach:**
```c
/* After parsing binary expression, check for ternary */
if (ls->tok == '?') {
  lj_lex_next(ls);  // Consume '?'

  /* Check if this is full ternary (?:) or optional false branch (?) */
  LexToken lookahead = lj_lex_lookahead(ls);

  if (lookahead == ':') {
    /* Full ternary: condition ? true_expr : false_expr */
    ExpDesc true_expr, false_expr;

    /* Parse true branch with current limit (right-associative) */
    expr_binop(ls, &true_expr, limit);

    /* Require ':' token */
    lex_check(ls, ':');

    /* Parse false branch with same limit */
    expr_binop(ls, &false_expr, limit);

    /* Emit ternary bytecode */
    bcemit_ternary(ls->fs, v, &true_expr, &false_expr);
  } else {
    /* Optional false branch: condition ? default_value */
    /* If condition is truthy, return condition; if falsey, return default */
    ExpDesc default_expr;

    /* Parse default value with current limit */
    expr_binop(ls, &default_expr, limit);

    /* Emit optional ternary bytecode */
    bcemit_ternary_optional(ls->fs, v, &default_expr);
  }

  /* Continue with next operator */
  op = token2binop(ls->tok);
  continue;
}
```

**Alternative Approach:** Handle ternary in `expr()` function after `expr_binop()` returns, since ternary has lowest priority and is right-associative.

**Better Approach:** Add special handling in `expr_binop()` after parsing the condition expression, check for `?` token.

---

### Step 6: Implement Ternary Bytecode Emission

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `bcemit_ternary()` function with extended falsey semantics:
   ```c
   static void bcemit_ternary(FuncState *fs, ExpDesc *cond, ExpDesc *true_expr, ExpDesc *false_expr)
   {
     /* Handle compile-time constant conditions first */
     expr_discharge(fs, cond);

     /* Check for compile-time truthy constants */
     if (cond->k == VKTRUE || (cond->k == VKNUM && !expr_numiszero(cond)) ||
         (cond->k == VKSTR && cond->u.sval->len > 0)) {
        /* Condition is always truthy - only evaluate true branch */
        expr_discharge(fs, true_expr);
        *cond = *true_expr;
        return;
     }

     /* Check for compile-time falsey constants */
     if (cond->k == VKFALSE || cond->k == VKNIL ||
         (cond->k == VKNUM && expr_numiszero(cond)) ||
         (cond->k == VKSTR && cond->u.sval->len == 0)) {
        /* Condition is always falsey - only evaluate false branch */
        expr_discharge(fs, false_expr);
        *cond = *false_expr;
        return;
     }

     /* Runtime condition - emit bytecode with extended falsey semantics */
     /* Ensure condition is in a register */
     BCReg cond_reg = expr_toanyreg(fs, cond);
     BCReg result_reg = cond_reg;

     /* Emit checks for extended falsey semantics (nil, false, 0, "") */
     /* Similar to ? operator implementation */
     BCPos check_nil = NO_JMP, check_false = NO_JMP;
     BCPos check_zero = NO_JMP, check_empty = NO_JMP;
     BCPos use_false_branch = NO_JMP;

     /* Check for nil */
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, cond_reg, const_pri(fs, &nilv)));
     check_nil = bcemit_jmp(fs);

     /* Check for false */
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, cond_reg, const_pri(fs, &falsev)));
     check_false = bcemit_jmp(fs);

     /* Check for zero */
     bcemit_INS(fs, BCINS_AD(BC_ISEQN, cond_reg, const_num(fs, &zerov)));
     check_zero = bcemit_jmp(fs);

     /* Check for empty string */
     bcemit_INS(fs, BCINS_AD(BC_ISEQS, cond_reg, const_str(fs, &emptyv)));
     check_empty = bcemit_jmp(fs);

     /* All falsey checks jump to false branch */
     use_false_branch = fs->pc;
     jmp_patch(fs, check_nil, use_false_branch);
     jmp_patch(fs, check_false, use_false_branch);
     jmp_patch(fs, check_zero, use_false_branch);
     jmp_patch(fs, check_empty, use_false_branch);

     /* Condition is truthy - use true branch */
     BCPos true_start = fs->pc;
     expr_discharge(fs, true_expr);
     expr_toreg(fs, true_expr, result_reg);

     /* Jump to end (after false branch) */
     BCPos skip_false = bcemit_jmp(fs);

     /* Condition is falsey - use false branch */
     BCPos false_start = fs->pc;
     expr_discharge(fs, false_expr);
     expr_toreg(fs, false_expr, result_reg);

     /* Patch skip to end */
     BCPos end = fs->pc;
     jmp_patch(fs, skip_false, end);

     /* Result is in result_reg */
     cond->u.s.info = result_reg;
     cond->k = VNONRELOC;
   }
   ```

**Key Considerations:**
- **Extended falsey semantics**: nil, false, 0, "" are all treated as falsey
- Short-circuit: only one branch executes
- Both branches write to same register
- Jump patching ensures correct flow
- Handle compile-time constant conditions
- Runtime checks for all falsey values (nil, false, 0, "")

---

### Step 7: Implement Optional Ternary Bytecode Emission

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Create `bcemit_ternary_optional()` function for optional false branch:
   ```c
   static void bcemit_ternary_optional(FuncState *fs, ExpDesc *cond, ExpDesc *default_expr)
   {
     /* Handle compile-time constant conditions */
     expr_discharge(fs, cond);

     if (cond->k == VKTRUE || (cond->k == VKNUM && !expr_numiszero(cond)) ||
         (cond->k == VKSTR && cond->u.sval->len > 0)) {
        /* Condition is truthy - return condition itself */
        /* Condition is already in correct form, just return it */
        return;  /* cond already contains the result */
     }

     if (cond->k == VKFALSE || cond->k == VKNIL ||
         (cond->k == VKNUM && expr_numiszero(cond)) ||
         (cond->k == VKSTR && cond->u.sval->len == 0)) {
        /* Condition is falsey - return default value */
        expr_discharge(fs, default_expr);
        *cond = *default_expr;
        return;
     }

     /* Runtime condition - emit bytecode */
     /* Ensure condition is in a register */
     BCReg cond_reg = expr_toanyreg(fs, cond);
     BCReg result_reg = cond_reg;

     /* Check if condition is truthy (extended falsey semantics) */
     /* Emit checks for nil, false, 0, "" */

     /* Jump to default if condition is falsey */
     BCPos check_nil = NO_JMP, check_false = NO_JMP;
     BCPos check_zero = NO_JMP, check_empty = NO_JMP;
     BCPos use_default = NO_JMP;

     /* Check for nil */
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, cond_reg, const_pri(fs, &nilv)));
     check_nil = bcemit_jmp(fs);

     /* Check for false */
     bcemit_INS(fs, BCINS_AD(BC_ISEQP, cond_reg, const_pri(fs, &falsev)));
     check_false = bcemit_jmp(fs);

     /* Check for zero (if numeric) */
     /* Only check if condition might be a number */
     /* For simplicity, always check */
     bcemit_INS(fs, BCINS_AD(BC_ISEQN, cond_reg, const_num(fs, &zerov)));
     check_zero = bcemit_jmp(fs);

     /* Check for empty string */
     bcemit_INS(fs, BCINS_AD(BC_ISEQS, cond_reg, const_str(fs, &emptyv)));
     check_empty = bcemit_jmp(fs);

     /* All falsey checks jump to default */
     use_default = fs->pc;
     jmp_patch(fs, check_nil, use_default);
     jmp_patch(fs, check_false, use_default);
     jmp_patch(fs, check_zero, use_default);
     jmp_patch(fs, check_empty, use_default);

     /* Condition is truthy - use condition value */
     /* Condition is already in cond_reg, so result is ready */
     BCPos skip_default = bcemit_jmp(fs);

     /* Condition is falsey - use default value */
     BCPos default_start = fs->pc;
     expr_discharge(fs, default_expr);
     expr_toreg(fs, default_expr, result_reg);

     /* Patch skip to end */
     BCPos end = fs->pc;
     jmp_patch(fs, skip_default, end);

     /* Result is in result_reg */
     cond->u.s.info = result_reg;
     cond->k = VNONRELOC;
   }
   ```

**Key Considerations:**
- Extended falsey semantics: nil, false, 0, ""
- If truthy, return condition itself
- If falsey, return default value
- Short-circuit: default value only evaluated if condition is falsey
- Handle compile-time constants

**Note:** This is similar to `?` operator logic but returns condition when truthy instead of skipping to RHS.

**Expected Result:**
- Optional ternary works correctly
- Proper short-circuit behavior
- Extended falsey semantics

---

### Step 8: Handle Compile-Time Constants

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Note: Compile-time constant handling is already included in `bcemit_ternary()` (Step 6). The function checks for:
   - Truthy constants: `true`, non-zero numbers, non-empty strings
   - Falsey constants: `false`, `nil`, zero, empty strings

   For constants, only the selected branch is evaluated, providing optimal performance.

**Expected Result:**
- Compile-time constant conditions are optimized
- Only the selected branch is evaluated for constants
- Extended falsey semantics properly handled for constants

---

### Step 9: Integrate Ternary into Expression Parsing

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Modify `expr_binop()` to check for ternary after parsing an expression:
   ```c
   static BinOpr expr_binop(LexState *ls, ExpDesc *v, uint32_t limit)
   {
     BinOpr op;
     synlevel_begin(ls);
     expr_unop(ls, v);

     /* Check for ternary operator */
     if (ls->tok == '?') {
       /* Parse ternary: condition ? true_expr : false_expr */
       ExpDesc true_expr, false_expr;

       lj_lex_next(ls);  // Consume '?'

       /* Parse true branch - use current limit (ternary has lowest priority) */
       expr_binop(ls, &true_expr, limit);

       /* Require ':' token */
       checkcond(ls, ls->tok == ':', LJ_ERR_XTOKEN);
       lj_lex_next(ls);  // Consume ':'

       /* Parse false branch - use current limit */
       expr_binop(ls, &false_expr, limit);

       /* Emit ternary bytecode */
       bcemit_ternary(ls->fs, v, &true_expr, &false_expr);

       /* Check for next operator */
       op = token2binop(ls->tok);
     } else {
       op = token2binop(ls->tok);
     }

     /* ... rest of existing expr_binop() logic ... */
   }
   ```

**Expected Result:**
- Ternary expressions are parsed correctly
- Precedence is handled correctly
- Right-associativity is maintained

---

### Step 10: Handle Right-Associativity

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Ensure ternary is right-associative:
   - `a ? b : c ? d : e` should parse as `a ? b : (c ? d : e)`
   - Not as `(a ? b : c) ? d : e`
   - This is handled by the priority table (right priority same as left)
   - The parsing logic should allow nested ternaries

**Implementation:**
- When parsing false branch, use same limit (allows nested ternary)
- Right-associativity is automatic with correct priority

---

### Step 11: Add Token Mapping (if needed)

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Update `token2binop()` if ternary needs special handling:
   ```c
   static BinOpr token2binop(LexToken tok)
   {
     switch (tok) {
     /* ... existing cases ... */
     case '?':  /* Handled separately in expr_binop() */
       return OPR_TERNARY;  // Or return OPR_NOBINOPR, handled in expr_binop()
     default:
       return OPR_NOBINOPR;
     }
   }
   ```

**Note:** Ternary is handled specially in `expr_binop()`, so this may not be needed.

---

### Step 12: Create Test Suite

**File:** `src/fluid/tests/test_ternary.fluid`

**Action:**
1. Create comprehensive test suite:
   ```lua
   -- Basic ternary
   function testBasic()
      local v = true ? "A" : "B"
      assert(v is "A", "Failed basic: expected 'A'")

      local v2 = false ? "A" : "B"
      assert(v2 is "B", "Failed basic false: expected 'B'")
   end

   -- Short-circuit true branch
   function testShortCircuitTrue()
      local function false_branch()
         error("Should not be called")
      end
      local v = true ? "A" : false_branch()
      assert(v is "A", "Failed short-circuit true")
   end

   -- Short-circuit false branch
   function testShortCircuitFalse()
      local function true_branch()
         error("Should not be called")
      end
      local v = false ? true_branch() : "B"
      assert(v is "B", "Failed short-circuit false")
   end

   -- Nested ternary (right-associative)
   function testNested()
      local v = true ? false ? "A" : "B" : "C"
      -- Parses as: true ? (false ? "A" : "B") : "C"
      -- Result: false ? "A" : "B" = "B"
      assert(v is "B", "Failed nested: expected 'B'")
   end

   -- Runtime condition
   function testRuntime()
      local x = 5
      local v = x > 0 ? "positive" : "non-positive"
      assert(v is "positive", "Failed runtime: expected 'positive'")
   end

   -- Compile-time constant optimization
   function testConstantTrue()
      local v = true ? "A" : expensive()
      assert(v is "A", "Failed constant true")
   end

   -- Extended falsey semantics (full ternary)
   function testExtendedFalseyNil()
      local v = nil ? "A" : "B"
      assert(v is "B", "Failed nil: expected 'B'")
   end

   function testExtendedFalseyZero()
      local v = 0 ? "A" : "B"
      assert(v is "B", "Failed zero: expected 'B'")
   end

   function testExtendedFalseyEmptyString()
      local v = "" ? "A" : "B"
      assert(v is "B", "Failed empty string: expected 'B'")
   end

   function testExtendedFalseyFalse()
      local v = false ? "A" : "B"
      assert(v is "B", "Failed false: expected 'B'")
   end

   -- Precedence
   function testPrecedence()
      local a, b = 5, 3
      local v = a > b ? a : b
      assert(v is 5, "Failed precedence: expected 5")
   end

   -- Optional false branch - truthy condition
   function testOptionalTruthy()
      local msg = "Hello" ? "No message"
      assert(msg is "Hello", "Failed optional truthy: expected 'Hello'")

      local count = 5 ? 0
      assert(count is 5, "Failed optional truthy number: expected 5")
   end

   -- Optional false branch - falsey condition (nil)
   function testOptionalNil()
      local msg = nil ? "No message given"
      assert(msg is "No message given", "Failed optional nil: expected default")
   end

   -- Optional false branch - falsey condition (false)
   function testOptionalFalse()
      local msg = false ? "No message given"
      assert(msg is "No message given", "Failed optional false: expected default")
   end

   -- Optional false branch - falsey condition (0)
   function testOptionalZero()
      local count = 0 ? 1
      assert(count is 1, "Failed optional zero: expected default 1")
   end

   -- Optional false branch - falsey condition (empty string)
   function testOptionalEmptyString()
      local name = "" ? "Anonymous"
      assert(name is "Anonymous", "Failed optional empty string: expected default")
   end

   -- Optional false branch - short-circuit
   function testOptionalShortCircuit()
      local function expensive()
         error("Should not be called")
      end
      local msg = "Hello" ? expensive()
      assert(msg is "Hello", "Failed optional short-circuit: expected 'Hello'")
   end

   -- Optional false branch - runtime condition
   function testOptionalRuntime()
      local possible_msg = getUserMessage()
      local msg = possible_msg ? "No message given"
      -- If possible_msg is truthy, use it; otherwise use default
   end

   return {
      tests = { 'testBasic', 'testShortCircuitTrue', 'testShortCircuitFalse',
                'testNested', 'testRuntime', 'testConstantTrue', 'testPrecedence',
                'testExtendedFalseyNil', 'testExtendedFalseyZero',
                'testExtendedFalseyEmptyString', 'testExtendedFalseyFalse',
                'testOptionalTruthy', 'testOptionalNil', 'testOptionalFalse',
                'testOptionalZero', 'testOptionalEmptyString', 'testOptionalShortCircuit',
                'testOptionalRuntime' }
   }
   ```

---

### Step 13: Update Documentation

**File:** `docs/wiki/Fluid-Reference-Manual.md`

**Action:**
1. Add ternary operator section:
   - Syntax and semantics (full and optional forms)
   - Examples for both forms
   - Precedence information
   - Short-circuit behavior
   - **Extended falsey semantics**: Both forms treat `nil`, `false`, `0`, and `""` as falsey (consistent with `?` operator)
   - Comparison with if-else statements
   - Comparison with `?` operator (both forms use same falsey semantics)

---

## Implementation Challenges

### Challenge 1: Token Conflicts

**Issue:** `?` is already used for:
- `?` (extended falsey)
- Potential future operators

**Solution:**
- Check for `?` token in `expr_binop()` after parsing expression
- Handle `?` first in lexer
- Ternary is distinguished by following `:`

### Challenge 2: Three-Operand Expression

**Issue:** Ternary has three operands (condition, true, false), unlike binary operators. Optional form has two operands but different semantics.

**Solution:**
- Parse condition as normal expression
- Check for `?` token
- Use lookahead to determine if `:` follows (full ternary) or not (optional)
- Full ternary: Parse true branch, require `:`, parse false branch
- Optional: Parse default value only
- Emit appropriate bytecode for each form

### Challenge 3: Right-Associativity

**Issue:** Ternary should be right-associative: `a ? b : c ? d : e` = `a ? b : (c ? d : e)`

**Solution:**
- Use same priority for left and right (already done)
- When parsing false branch, use same limit (allows nested ternary)
- Right-associativity is automatic

### Challenge 4: Register Management

**Issue:** Both branches write to same register, need proper cleanup. Optional form returns condition when truthy.

**Solution:**
- Use same register for both branches (full ternary)
- Optional form: Condition register is result register when truthy
- Ensure proper register allocation
- Handle register cleanup after branches

### Challenge 5: Optional Form Semantics

**Issue:** Optional form needs extended falsey semantics (nil, false, 0, "").

**Solution:**
- Reuse logic from `?` operator for falsey checks
- Check for nil, false, zero, and empty string
- If any falsey check passes, use default value
- If all falsey checks fail, use condition value

---

## Testing Checklist

- [ ] Basic ternary: `true ? "A" : "B"`
- [ ] False condition: `false ? "A" : "B"`
- [ ] Short-circuit true branch (RHS not evaluated)
- [ ] Short-circuit false branch (LHS not evaluated)
- [ ] Nested ternary (right-associativity)
- [ ] Runtime conditions: `x > 0 ? "pos" : "neg"`
- [ ] Compile-time constant optimization
- [ ] Precedence: `a > b ? a : b`
- [ ] Assignment: `x = cond ? a : b`
- [ ] Chaining with other operators
- [ ] Extended falsey semantics: `nil ? "A" : "B"` returns `"B"`
- [ ] Extended falsey semantics: `0 ? "A" : "B"` returns `"B"`
- [ ] Extended falsey semantics: `false ? "A" : "B"` returns `"B"`
- [ ] Extended falsey semantics: `"" ? "A" : "B"` returns `"B"`
- [ ] Error handling: missing `:` (for full ternary)
- [ ] Optional ternary - truthy: `"msg" ? "default"` returns `"msg"`
- [ ] Optional ternary - falsey (nil): `nil ? "default"` returns `"default"`
- [ ] Optional ternary - falsey (false): `false ? "default"` returns `"default"`
- [ ] Optional ternary - falsey (0): `0 ? 1` returns `1`
- [ ] Optional ternary - falsey (""): `"" ? "default"` returns `"default"`
- [ ] Optional ternary - short-circuit (default not evaluated if truthy)
- [ ] Optional ternary - extended falsey semantics

---

## Key Files to Modify

1. **`src/fluid/luajit-2.1/src/lj_parse.c`**
   - Add `OPR_TERNARY` to enum
   - Add priority entry
   - Modify `expr_binop()` to handle ternary
   - Implement `bcemit_ternary()` function

2. **`src/fluid/tests/test_ternary.fluid`**
   - Create comprehensive test suite

3. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add ternary operator documentation

4. **`src/fluid/luajit-2.1/AGENTS.md`** (if needed)
   - Add implementation notes if complex patterns emerge

---

## Implementation Order

1. **Phase 1: Core Implementation** (8-12 hours)
   - Steps 3-6: Add enum, priority, parsing, bytecode emission
   - Basic functionality working

2. **Phase 2: Optimization** (4-6 hours)
   - Step 7: Compile-time constant handling
   - Performance optimization

3. **Phase 3: Testing** (4-6 hours)
   - Step 11: Comprehensive test suite
   - Edge case testing

4. **Phase 4: Documentation** (2-4 hours)
   - Step 12: User documentation
   - Implementation notes

---

## References

### Similar Implementations
- `?` operator: `lj_parse.c:910-1147` (short-circuit pattern, extended falsey semantics)
- Binary operators: `lj_parse.c:1065-1118` (operator patterns)
- Optional ternary is similar to `?` but returns condition when truthy instead of skipping to RHS

### External References
- C/C++ ternary operator specification
- JavaScript ternary operator behavior
- Lua expression evaluation rules

---

**Last Updated:** 2025-01-XX
**Status:** Implementation Plan - Ready for implementation
**Related:** `?` operator implementation, expression parsing in `expr_binop()`
