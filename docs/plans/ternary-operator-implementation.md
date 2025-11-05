# Ternary Operator Implementation Plan

## Overview

This document provides a detailed step-by-step implementation plan for the C-style ternary conditional operator (`condition ? true_value : false_value`) and its optional form (`condition ? default_value`) in Fluid/LuaJIT. The optional form (`condition ? default_value`) has been implemented as part of renaming `?` to `?`, and provides a default value pattern that returns the condition when truthy.

**Status:** 🔄 **Plan Updated** - Adopt lookahead-based full ternary parsing. Lexer/token work is solid. Current full-ternary emission path will be replaced with a control-flow-first approach; optional form (`x ? default`) remains passing.

**Priority:** ⭐⭐⭐⭐⭐ **Highest**

**Estimated Effort:** 12-18 hours remaining (lookahead + emission rewrite + tests)

## Current Implementation Status (2025-01-05)

### ✅ Completed
- **Step 1**: Token definition (`TK_ternary_sep`) added to lexer (lj_lex.h:21)
- **Step 1**: Lexer recognizes `:>` token correctly (lj_lex.c:403-407)
- **Step 3**: `OPR_TERNARY` added to `BinOpr` enum (lj_parse.c:160)
- **Step 4**: Priority table entry added with {1,1} (lj_parse.c:192)
- **Step 5**: `bcemit_ternary()` function implemented (lj_parse.c:1162-1231)
- **Step 6**: `bcemit_ternary_optional()` function implemented (not used - see issues)
- **Step 7**: `expr_binop()` modified to detect `:>` and call `bcemit_ternary()` (lj_parse.c:2867-2877)
- Builds successfully with no compilation errors
- Basic tests pass for simple values and variables
- Existing `?` operator tests continue to pass (all 11 tests in test_orq.fluid)

### ⚠️ Known Issues - CRITICAL ARCHITECTURAL PROBLEM

#### Issue #1: Current Implementation is Fundamentally Broken
**Severity:** 🔴 **BLOCKER** - Current approach cannot work, requires complete redesign

**Problem Summary:** The ternary operator implementation has a fundamental architectural flaw where both branches are emitted and executed unconditionally, regardless of the condition. This isn't just a "function call" problem - it's a complete failure of conditional control flow.

**Comprehensive Test Results (test_ternary.fluid - 71 tests):**
- ✅ **37 tests PASS** - Simple constants, variables, arithmetic, basic operators
- ❌ **34 tests FAIL** - All tests involving:
  - Function calls in branches (functions not called, return garbage)
  - Method calls in branches (return function object instead of calling)
  - Short-circuit evaluation (false branches execute when they shouldn't)
  - Constant condition optimization (both branches still execute)
  - Nested table access (parser state corruption: "attempt to index field 'user' (a function value)")
  - Tests after first major failure (cascading failures: "attempt to call a nil value")

**Failed Test Categories:**
1. **Short-circuit violations** (Tests 10, 32-34):
   - Test 10: `testShortCircuitFalse` - False branch function called when condition is truthy
   - Tests 32-34: Constant falsey conditions still execute true branch functions

2. **Function/method call corruption** (Tests 28, 45, 48-50):
   - Test 28: Function returns wrong value (0 instead of 10)
   - Tests 45, 48-50: Methods return `function: builtin#19` instead of calling and returning result
   - Indicates VCALL expressions are completely mishandled

3. **Parser state corruption** (Test 61+):
   - Test 61: Table field treated as function
   - Tests 62-71: Cascading "attempt to call a nil value" errors
   - Suggests parser/compiler state becomes corrupted after failed ternary

**Root Cause - Deep Analysis:**

The current implementation is architecturally incompatible with LuaJIT's parsing model:

1. **LuaJIT emits bytecode during parsing** - You cannot parse an expression without emitting its bytecode
2. **Current flow (BROKEN):**
   ```
   1. Parse condition → emit condition bytecode
   2. See '?' token
   3. Call expr_binop() to parse true branch → TRUE BRANCH BYTECODE EMITTED HERE
   4. See ':>' token
   5. Call expr_binop() to parse false branch → FALSE BRANCH BYTECODE EMITTED HERE
   6. Call bcemit_ternary() → emit condition checks
   ```

3. **Resulting bytecode order:**
   ```
   PC 0-7:   [earlier code]
   PC 8:     BC_CALL for getX()        ← True branch, emitted in step 3
   PC 9:     BC_CALL for getY()        ← False branch, emitted in step 5
   PC 10-15: Condition checks           ← Emitted in step 6, TOO LATE
   PC 16:    True branch handling
   PC 17:    False branch handling
   ```

4. **Why this fails:**
   - Both BC_CALL instructions at PC 8-9 execute unconditionally BEFORE condition checks at PC 10-15
   - The condition checks at PC 10-15 are essentially dead code
   - The "branching" at PC 16-17 just decides which already-executed result to use
   - This explains why functions are "called but return garbage" - they're called before we even check the condition

**Why Simple Cases Work:**
- Constants/variables don't emit complex bytecode during parsing
- `expr_discharge()` and `expr_toreg()` handle them after the fact
- No side effects from unconditional emission

**Why Complex Cases Fail:**
- Function calls (BC_CALL), method calls, table access emit bytecode during parsing
- This bytecode executes unconditionally
- By the time we set up conditional jumps, it's too late
- Parser state gets corrupted because expressions are emitted out of order

**Critical Insight from Existing `?` Operator:**

The `?` operator works because:
1. `bcemit_binop_left()` is called BEFORE parsing RHS
2. It sets up conditional jumps that will skip the RHS bytecode
3. When RHS is parsed and emitted, the jumps are ALREADY in place
4. Result: RHS bytecode is conditionally skipped at runtime

**For ternary, we can't do this because:**
- We don't know it's a ternary (vs just `?`) until AFTER parsing the first branch
- By then, the first branch bytecode is already emitted
- We'd need lookahead to see `:>` before parsing, which LuaJIT's parser doesn't support well

**Proposed Solutions (in priority order):**

### Chosen Approach: Lookahead for `:>` (determine full ternary before emitting)
**Why:** LuaJIT emits bytecode during parsing. To preserve short-circuiting, conditional jumps must be in place before either branch is parsed/emitted. This requires deciding “full ternary vs optional form” up-front.

**Strategy:**
- After encountering `?`, perform a lookahead that determines whether a top-level `TK_ternary_sep` (`:>`) appears for this operator (respect parentheses/nesting).
- If found: emit extended-falsey condition checks immediately, then parse/emit true branch to a fixed result register, emit skip-to-end, patch falsey jumps to parse/emit false branch into the same register, then patch skip.
- If not found: fall back to existing optional form (`condition ? default`) emission path, which already passes.

**Notes:**
- Lookahead operates on lexer state without emitting bytecode; no changes to branch expressions during scanning.
- This ensures function/method calls (VCALL) in branches are only emitted in the selected branch.

### Rejected: Hybrid extension of the `?` operator
Parsing and emitting the RHS (true branch) before knowing whether `:>` follows unconditionally emits its bytecode, breaking short-circuiting for full ternary and corrupting VCALL/method semantics. Extending after-the-fact cannot retroactively prevent already-emitted calls.

### 🔧 Implementation Details

**Files Modified:**
- `src/fluid/luajit-2.1/src/lj_lex.h` - Token definition
- `src/fluid/luajit-2.1/src/lj_lex.c` - Lexer recognition
- `src/fluid/luajit-2.1/src/lj_parse.c` - Parser and bytecode emission

**Current Code Location:**
- Lexer: Lines 21 (token def), 403-407 (recognition)
- Parser enum: Line 160
- Priority table: Line 192
- `bcemit_ternary()`: Lines 1162-1231
- `expr_binop()` ternary check: Lines 2867-2877

**Test Results:**
- ✅ Simple constants: `true ? "A" :> "B"` → "A"
- ✅ Extended falsey: `0 ? "A" :> "B"` → "B"
- ✅ Extended falsey: `"" ? "A" :> "B"` → "B"
- ✅ Variables: `true ? x :> "B"` → value of x
- ✅ Runtime conditions: `x > 5 ? "yes" :> "no"` → works correctly
- ✅ Nested ternary: `true ? (false ? "A" :> "B") :> "C"` → "B"
- ✅ Existing `?` operator: All tests pass, no regression
- ❌ Function calls: `true ? getX() :> "B"` → garbage, function not called
- ❌ Method calls: `true ? obj:getName() :> "default"` → garbage, method not called

### 📋 Next Steps — Focused Plan (Lookahead)

#### Immediate Actions

1. Implement lookahead for `:>` at the `?` operator site
   - Scan tokens from just after `?` to find a top-level `TK_ternary_sep` for this operator instance (respect parentheses and nested subexpressions).
   - Operate on lexer state without emitting bytecode; do not mutate parsed expressions during the scan.

2. Rewire `expr_binop()` handling of `OPR_OR_QUESTION`
   - If lookahead says “no :>”: use existing optional form emission (unchanged).
   - If lookahead says “has :>”: emit extended-falsey condition checks first, then:
     - Parse/emit true branch into a fixed result register.
     - Emit skip-to-end.
     - Patch falsey checks to current pc; parse/emit false branch into the same result register.
     - Patch skip to end.

3. Remove the current full-ternary path that emits after parsing both branches
   - Delete the special-case in `expr_binop()` that calls `bcemit_ternary()` after parsing `v2` and `false_expr`.
   - Either delete the old `bcemit_ternary()` or refactor/rename it into an internal helper called only by the new lookahead-based path.

4. Guardrails for VCALL/method calls
   - Ensure `expr_discharge()` is applied to branch expressions so only a single value is produced.
   - Always write branch results to the same destination register via `expr_toreg()`.

5. Test incrementally
   - Constants → variables → functions/methods in branches → nested ternaries.
   - Re-run `src/fluid/tests/test_orq.fluid` to confirm no regression in optional form.
   - Run `src/fluid/tests/test_ternary.fluid` and iterate.

#### Medium-Term Tasks (After Solution 1 Works):

4. **Add compile-time constant optimization**:
   - If condition is constant and branch is constant, fold at compile time
   - Avoid emitting dead branch code
   - Must not break short-circuit evaluation for non-constant branches

5. **Comprehensive testing**:
   - Full test_ternary.fluid suite should pass all 71 tests
   - Add edge cases: deeply nested ternaries, ternary in function arguments, etc.

6. **Documentation**:
   - Update Fluid Reference Manual with ternary operator
   - Document `:>` syntax, extended falsey semantics, precedence
   - Add examples to wiki

#### Alternative Path (If Solution 1 Fails):

7. **Try Solution 2 (Lookahead)**:
   - Implement `lj_lex_lookahead_scan()` to find `:>` token
   - Must handle nested expressions, parentheses, brackets
   - More complex but avoids hybrid approach limitations

8. **Last Resort: Solution 3 (Complete Rewrite)**:
   - Only if both Solution 1 and 2 fail
   - Requires deep LuaJIT parser expertise
   - High risk of breaking existing functionality

### 🎯 Success Criteria

The implementation will be considered complete when:
- ✅ All 71 tests in test_ternary.fluid pass
- ✅ All 11 tests in test_orq.fluid still pass (no regression)
- ✅ Function calls work in both branches with proper short-circuiting
- ✅ Method calls work correctly
- ✅ Nested ternaries work
- ✅ Extended falsey semantics work for all value types
- ✅ No parser state corruption or cascading failures

### 📝 Notes for Next Session

**Key Files to Focus On:**
- `src/fluid/luajit-2.1/src/lj_parse.c` - All the action happens here
- Line 1318-1380: OPR_OR_QUESTION handling in `bcemit_binop()` - study this carefully
- Line 2867: Current `:>` check location - needs to move after bcemit_binop()
- Test files: `src/fluid/tests/test_ternary.fluid` and `test_orq.fluid`

**What Already Works (Don't Break This):**
- Lexer correctly recognizes `:>` token
- Token definitions and priority table are correct
- Existing `?` operator (all 11 tests pass)
- Simple ternary with constants/variables (37/71 tests pass)

**What's Broken (Fix This):**
- Function/method calls in branches (functions never called)
- Short-circuit evaluation (both branches execute unconditionally)
- Parser state corruption with complex expressions

**Debug Strategy:**
- Use printf debugging selectively in `bcemit_binop()` and new `bcemit_ternary_extend()`
- Check jump lists (e->t, e->f) to verify conditional flow
- Verify BC_CALL instructions are emitted AFTER conditional jumps are set up
- Test incrementally with simple cases before complex ones

## Feature Specification

### Syntax
```lua
-- Full ternary (two branches)
local result = condition ? value_if_true :> value_if_false

-- Optional false branch (default value pattern)
local result = condition ? default_value
```

**Note:** The `:>` token is used as the separator for the full ternary form to avoid conflicts with `:` (used for method calls and labels) and `>` (used for comparison operators).

### Semantics
- **Extended falsey semantics**: Both forms treat `nil`, `false`, `0`, and `""` as falsey (consistent with `?` operator)
- **Short-circuit evaluation**: Only one branch (true or false) is evaluated
- **Precedence**: Lower than all arithmetic/relational operators, higher than assignment
- **Associativity**: Right-associative (like assignment)
- **Return value**: Returns the value from the selected branch
- **Full ternary**: If condition is truthy, return true branch; if falsey (nil, false, 0, ""), return false branch
- **Optional false branch**: When `:>` is omitted, if condition is truthy, return condition; if falsey (nil, false, 0, ""), return default value

### Examples
```lua
-- Full ternary (extended falsey semantics)
local msg = error ? "Error occurred" :> "Success"
-- If error is truthy, returns "Error occurred"
-- If error is falsey (nil, false, 0, ""), returns "Success"

local count = x ? 10 :> 0
-- If x is truthy (non-zero, non-empty), returns 10
-- If x is falsey (nil, false, 0, ""), returns 0

local max = a > b ? a :> b
local status = user ~= nil ? user.active ? "online" :> "offline" :> "unknown"

-- Optional false branch (default value pattern)
local msg = possible_msg ? "No message given"
-- If possible_msg is truthy, returns possible_msg
-- If possible_msg is falsey (nil, false, 0, ""), returns "No message given"

local name = user_name ? "Anonymous"
local count = result_count ? 0
```

## Implementation Steps

### Step 1: Add Token Definitions and Lexer Recognition

**File:** `src/fluid/luajit-2.1/src/lj_lex.h` and `src/fluid/luajit-2.1/src/lj_lex.c`

**Action:**
1. Add ternary separator token to `TKDEF` macro in `lj_lex.h`:
   ```c
   __(ternary_sep, :>)
   ```

2. Update lexer in `lj_lex.c` to recognize `:>` token (around line 403-405):
   ```c
   case ':':
     lex_next(ls);
     if (ls->c == '>') { lex_next(ls); return TK_ternary_sep; }
     else if (ls->c == ':') { lex_next(ls); return TK_label; }
     else return ':';
   ```

**Key Points:**
- Check for `>` before checking for `:` to properly recognize `:>` token
- This order ensures `:>` is recognized before the label check (`::`)
- The `>` token parsing is unaffected since it only checks for `=` and `>>`, not `:>`

**Expected Result:**
- `TK_ternary_sep` token enum value created
- Lexer correctly recognizes `:>` as a single token
- Method calls (`obj:method()`) and labels (`::label::`) continue to work correctly
- Comparison operators (`>`, `>=`, `>>`) continue to work correctly

---

### Step 2: Verify Token Conflicts Resolved

**File:** `src/fluid/luajit-2.1/src/lj_lex.c` and `src/fluid/luajit-2.1/src/lj_parse.c`

**Status:** ✅ **No conflicts** - Using `:>` eliminates token conflicts entirely.

**Verification:**
- **Method calls**: Use `:` token (single colon) - no conflict with `:>`
- **Labels**: Use `::` token (double colon) - no conflict with `:>`
- **Comparison operators**:
  - `>` checks for `=` → `>=` or `>>` → `>>` - no conflict with `:>`
  - `>` parser only checks `=` and `>>`, not `:>`
- **Ternary separator**: Uses `:>` token - unique and unambiguous

**Expected Result:**
- Method calls like `obj:method()` work correctly in all contexts
- Ternary expressions like `cond ? obj:method() :> other` parse correctly (method call in true branch)
- Ternary expressions like `cond ? true_val :> obj:method()` parse correctly (method call in false branch)
- Nested ternaries work correctly without any special flag handling
- Labels (`::label::`) continue to work correctly
- Comparison operators (`>`, `>=`, `>>`) continue to work correctly

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
1. Modify `expr_binop()` to handle ternary operator parsing when encountering `OPR_OR_QUESTION` (`?` token):
   - Parse RHS first (true branch or default value)
   - After parsing RHS, check if next token is `TK_ternary_sep` (`:>`) to determine full ternary vs optional form
   - Parse false branch if `:>` is present (full ternary)
   - Emit appropriate bytecode

**Location:** `expr_binop()` function (around line 2700-2710)

**Implementation Approach:**
```c
/* Special handling for ? operator: delay bcemit_binop_left to determine if it's ternary or optional */
if (op == OPR_OR_QUESTION) {
  /* Parse RHS first, then check for :> to determine full ternary vs optional */
  ExpDesc v2;
  BinOpr nextop;
  nextop = expr_binop(ls, &v2, priority[op].right);

  if (ls->tok == TK_ternary_sep) {
    /* Full ternary: condition ? true_expr :> false_expr */
    lj_lex_next(ls);  /* Consume ':>' */

    ExpDesc false_expr;
    nextop = expr_binop(ls, &false_expr, priority[op].right);

    /* Emit full ternary bytecode: v is condition, v2 is true branch, false_expr is false branch */
    bcemit_ternary(ls->fs, v, &v2, &false_expr);
    op = nextop;
  } else {
    /* Optional form: condition ? default_value */
    /* Use bcemit_ternary_optional instead of bcemit_binop */
    bcemit_ternary_optional(ls->fs, v, &v2);
    op = nextop;
  }
}
```

**Key Points:**
- No flag needed - `:>` token is unambiguous and won't conflict with method calls or labels
- After parsing RHS, we check if the next token is `TK_ternary_sep` (`:>`) to distinguish full ternary from optional form
- Method calls work normally in both true and false branches since `:` and `:>` are distinct tokens
- Labels continue to work since they use `::` (double colon)

**Status:** ⏳ **Pending Implementation**

---

### Step 6: Implement full ternary emission (control-first)

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
- In the lookahead-affirmed full-ternary path, emit extended-falsey checks for the condition BEFORE parsing branches.
- Parse/emit the true branch into a fixed destination register, then emit a skip-to-end jump.
- Patch falsey checks to current pc; parse/emit the false branch into the same destination register; patch skip to end.

**Key Considerations:**
- Extended falsey semantics (nil, false, 0, "").
- Ensure `expr_discharge()` restricts VCALLs to single values; use `expr_toreg()` to place results.
- Optimise compile-time constants by selecting the branch without emitting the other.

---

### Step 7: Optional form (`x ? default`) stays as-is

**Action:**
- Retain the existing implementation (already covered by tests in `test_orq.fluid`).
- Ensure the lookahead path does not disturb this emission path.

**Expected Result:**
- Optional form remains fully functional with extended falsey semantics and short-circuiting.

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

**Status:** ✅ **Already Implemented** - Ternary parsing is integrated into `expr_binop()` via special handling for `OPR_OR_QUESTION` operator.

**Implementation Details:**
- Ternary uses the existing `?` operator (`OPR_OR_QUESTION`) token
- When `OPR_OR_QUESTION` is encountered, special logic determines if it's:
  - Full ternary: `condition ? true_expr : false_expr` (when `:` follows the RHS)
  - Optional form: `condition ? default_value` (when `:` is absent)
- The `in_ternary` flag ensures proper disambiguation of `:` token (see Step 2)

**Expected Result:**
- Ternary expressions are parsed correctly ✅
- Precedence is handled correctly ✅
- Right-associativity is maintained ✅
- Method calls work correctly in ternary branches ✅

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

### Step 11: Add Token Mapping

**File:** `src/fluid/luajit-2.1/src/lj_parse.c`

**Action:**
1. Update `token2binop()` to map `TK_ternary_sep` if needed (likely not required since ternary parsing is handled specially):
   ```c
   static BinOpr token2binop(LexToken tok)
   {
     switch (tok) {
     /* ... existing cases ... */
     case '?':  /* Handled separately in expr_binop() */
       return OPR_OR_QUESTION;  /* ? is already mapped */
     case TK_ternary_sep:  /* :> is handled in expr_binop() ternary logic */
       return OPR_NOBINOPR;  /* Not a binary operator itself */
     default:
       return OPR_NOBINOPR;
     }
   }
   ```

**Note:** The `:>` token is checked directly in `expr_binop()` when handling `OPR_OR_QUESTION`, so mapping it in `token2binop()` may not be necessary. However, adding it explicitly prevents accidental misinterpretation.

---

### Step 12: Create Test Suite

DONE

**File:** `src/fluid/tests/test_ternary.fluid`

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

### Challenge 1: Token Conflicts Resolved with `:>` Separator

**Issue:** The `:` token is used for multiple purposes:
- Method calls: `obj:method()` (parsed in `expr_primary()` suffix loop)
- Labels: `::label::` (double colon, parsed separately)
- ~~Ternary operator separator~~ (was proposed, but conflicts)

**Solution:** Use `:>` as the ternary separator token instead of `:`:
- `:>` is a unique two-character token that doesn't conflict with any existing operators
- Method calls continue to use `:` (single colon) - no conflict
- Labels continue to use `::` (double colon) - no conflict
- Comparison operators (`>`, `>=`, `>>`) are unaffected since `>` parser only checks for `=` and `>>`
- No flag-based disambiguation needed - the token itself is unambiguous

**Why This Works:**
- Lexer recognizes `:>` as a single token when `:` is followed by `>`
- Checking for `>` before `:` in the lexer ensures proper token recognition
- Method calls (`obj:method()`) work correctly in all contexts without special handling
- Labels (`::label::`) continue to work correctly
- Ternary expressions like `cond ? obj:method() :> other` parse correctly
- No parser state flags needed - the unambiguous token eliminates the need

✅ **Status:** Solution proposed - eliminates complexity

### Challenge 2: Three-Operand Expression

**Issue:** Ternary has three operands (condition, true, false), unlike binary operators. Optional form has two operands but different semantics.

**Solution:**
- Parse condition as normal expression
- Check for `?` token
- Parse RHS (true branch or default value)
- Check if next token is `TK_ternary_sep` (`:>`) to determine full ternary vs optional
- Full ternary: Require `:>`, parse false branch
- Optional: No `:>` token, treat as optional form
- Emit appropriate bytecode for each form

### Challenge 3: Right-Associativity

**Issue:** Ternary should be right-associative: `a ? b :> c ? d :> e` = `a ? b :> (c ? d :> e)`

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

- [ ] Basic ternary: `true ? "A" :> "B"`
- [ ] False condition: `false ? "A" :> "B"`
- [ ] Short-circuit true branch (RHS not evaluated)
- [ ] Short-circuit false branch (LHS not evaluated)
- [ ] Nested ternary (right-associativity)
- [ ] Runtime conditions: `x > 0 ? "pos" :> "neg"`
- [ ] Compile-time constant optimization
- [ ] Precedence: `a > b ? a :> b`
- [ ] Assignment: `x = cond ? a :> b`
- [ ] Chaining with other operators
- [ ] Extended falsey semantics: `nil ? "A" :> "B"` returns `"B"`
- [ ] Extended falsey semantics: `0 ? "A" :> "B"` returns `"B"`
- [ ] Extended falsey semantics: `false ? "A" :> "B"` returns `"B"`
- [ ] Extended falsey semantics: `"" ? "A" :> "B"` returns `"B"`
- [ ] Error handling: missing `:>` (for full ternary)
- [ ] Optional ternary - truthy: `"msg" ? "default"` returns `"msg"`
- [ ] Optional ternary - falsey (nil): `nil ? "default"` returns `"default"`
- [ ] Optional ternary - falsey (false): `false ? "default"` returns `"default"`
- [ ] Optional ternary - falsey (0): `0 ? 1` returns `1`
- [ ] Optional ternary - falsey (""): `"" ? "default"` returns `"default"`
- [ ] Optional ternary - short-circuit (default not evaluated if truthy)
- [ ] Optional ternary - extended falsey semantics
- [ ] Method calls in true branch: `cond ? obj:method() :> other` works correctly
- [ ] Method calls in false branch: `cond ? true_val :> obj:method()` works correctly
- [ ] Nested ternaries with method calls: `cond1 ? cond2 ? obj:method() :> other :> default` works correctly
- [ ] Comparison operators work correctly: `a > b ? a :> b` (ensures `>` doesn't conflict with `:>`)

---

## Key Files to Modify

1. **`src/fluid/luajit-2.1/src/lj_lex.h`** ⏳
   - Add `TK_ternary_sep` token to `TKDEF` macro

2. **`src/fluid/luajit-2.1/src/lj_lex.c`** ⏳
   - Update `:` case to recognize `:>` token (check for `>` before `:`)

3. **`src/fluid/luajit-2.1/src/lj_parse.c`** ⏳
   - Add `OPR_TERNARY` to enum (if needed - currently using `OPR_OR_QUESTION`)
   - Add priority entry (if needed)
   - Modify `expr_binop()` to handle ternary - check for `TK_ternary_sep` instead of `:`
   - Implement `bcemit_ternary()` function
   - Implement `bcemit_ternary_optional()` function
   - Update `token2binop()` if needed (though `:>` is handled specially)

4. **`src/fluid/tests/test_ternary.fluid`**
   - Update test suite to use `:>` instead of `:` for full ternary
   - Create comprehensive test suite (including method call tests)

5. **`docs/wiki/Fluid-Reference-Manual.md`**
   - Add ternary operator documentation with `:>` syntax

**Note:** The `in_ternary` flag approach is no longer needed since `:>` is an unambiguous token.

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

**Last Updated:** 2025-01-14
**Status:** Implementation Plan - In Progress
**Related:** `?` operator implementation, expression parsing in `expr_binop()`

## Implementation Notes

### `:>` Token Recognition Pattern

The `:>` token approach eliminates the need for parser state flags by using an unambiguous token:

**Lexer Implementation Pattern:**
```c
case ':':
  lex_next(ls);
  if (ls->c == '>') { lex_next(ls); return TK_ternary_sep; }
  else if (ls->c == ':') { lex_next(ls); return TK_label; }
  else return ':';
```

**Parser Implementation Pattern:**
```c
/* When encountering ? operator */
if (op == OPR_OR_QUESTION) {
  /* Parse RHS first, then check for :> to determine full ternary vs optional */
  ExpDesc v2;
  BinOpr nextop;
  nextop = expr_binop(ls, &v2, priority[op].right);

  if (ls->tok == TK_ternary_sep) {
    /* Full ternary: condition ? true_expr :> false_expr */
    lj_lex_next(ls);  /* Consume ':>' */
    ExpDesc false_expr;
    nextop = expr_binop(ls, &false_expr, priority[op].right);
    bcemit_ternary(ls->fs, v, &v2, &false_expr);
    op = nextop;
  } else {
    /* Optional form: condition ? default_value */
    bcemit_ternary_optional(ls->fs, v, &v2);
    op = nextop;
  }
}
```

**Key Benefits:**
- No parser state flags needed - token is unambiguous
- Method calls (`obj:method()`) work correctly in all contexts without special handling
- Labels (`::label::`) continue to work correctly
- Comparison operators (`>`, `>=`, `>>`) are unaffected
- Simpler implementation - no flag management needed
- Works correctly with nested ternaries without flag scoping concerns

**Files Modified:**
- `lj_lex.h` - Add `TK_ternary_sep` to `TKDEF` macro
- `lj_lex.c` - Update `:` case to recognize `:>` token (check `>` before `:`)
- `lj_parse.c` - Check for `TK_ternary_sep` in ternary parsing logic

**Note on Token Conflicts:**
- `:>` is checked before `::` in the lexer, ensuring proper recognition
- The `>` token parser only checks for `=` (`>=`) and `>>` (`>>`), so it won't interfere with `:>`
- Method calls use `:` (single colon), which is distinct from `:>`
- Labels use `::` (double colon), which is distinct from `:>`
- All existing tokens continue to work correctly without modification
