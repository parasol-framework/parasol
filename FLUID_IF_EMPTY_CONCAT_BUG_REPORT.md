# Fluid IF_EMPTY (?) Operator Bug Report - Concatenation Context

**Date:** 2025-11-12
**Branch:** `test/fluid-if-empty`
**Reporter:** Claude (AI Assistant)
**Status:** Investigation Complete - Bug Identified

## Executive Summary

The Fluid `?` operator (IF_EMPTY) contains a critical register allocation bug when used in combination with:
1. Function call arguments: `msg(Number ? 2)`
2. String concatenation: `'<' .. (Number ? 2) .. '>'`
3. Ternary operator with concatenation: `'' .. (nil ? func() :> func())`

The bug causes incorrect bytecode generation leading to runtime errors ("attempt to call local 'Number' (a number value)") and segmentation faults.

## Test Failures

From `ctest --test-dir build/agents -L fluid`:

- **fluid_if_empty** (Test #36): **SEGFAULT**
  - `testConcatMsg` fails at line 81

- **fluid_ternary** (Test #40): **Failed**
  - `testConcat` fails at line 521 with "attempt to concatenate a boolean value"

- **fluid_safe_nav** (Test #39): **Failed**
  - 2 tests fail related to presence/if-empty operator interaction

## Root Cause

### Location
`src/fluid/luajit-2.1/src/parser/lj_parse_operators.c`

### Functions Affected
1. `bcemit_binop_left()` - lines 131-180 (IF_EMPTY operator handling)
2. `bcemit_binop()` - lines 425-520 (IF_EMPTY operator completion)

### The Bug

**Step 1: Register Reservation (bcemit_binop_left, line 167)**
```c
if (!expr_isk_nojump(e)) {
   BCReg src_reg = expr_toanyreg(fs, e);
   BCReg rhs_reg = fs->freereg;
   uint8_t flags = e->flags;
   bcreg_reserve(fs, 1);  // ← RESERVES REGISTER FOR RHS
   expr_init(e, VNONRELOC, src_reg);
   e->u.s.aux = rhs_reg;
   e->flags = (flags & ~SAFE_NAV_CHAIN_FLAG) | EXP_HAS_RHS_REG_FLAG;
}
```

**Step 2: Incorrect Register Cleanup (bcemit_binop, lines 508-512)**
```c
if (rhs_reg > reg && !(saved_flags & SAFE_NAV_CHAIN_FLAG)) {
   BCReg target_free = (reg >= fs->nactvar) ? reg + 1 : fs->nactvar;
   if (fs->freereg > target_free)
      fs->freereg = target_free;  // ← CLEANUP LOGIC FAILS IN CONCAT CONTEXT
}
```

**Problem:** The register cleanup logic attempts to collapse `fs->freereg` to drop stale registers, but fails to account for concatenation and function argument contexts where the register state must be preserved differently.

**Step 3: Concatenation Failure**

When the IF_EMPTY result is used in concatenation:
```c
// bcemit_binop_left for CONCAT (line 181-183)
else if (op == OPR_CONCAT) {
   expr_tonextreg(fs, e);  // ← EXPECTS CORRECT fs->freereg
}
```

The `expr_tonextreg()` expects `fs->freereg` to be correct, but IF_EMPTY left it in an incorrect state. This causes:
- Wrong register indices in BC_CAT instruction
- Orphaned registers containing intermediate values
- Runtime attempts to use wrong register contents

## Evidence

### Test Case 1: testConcatMsg (line 81)
```fluid
function testConcatMsg()
   local function fakeFunction()
      return nil
   end
   local Number = 1
   fakeFunction(Number ? 2) -- ← FAILS HERE
   msg(Number ? 2)
   msg('<' .. (Number ? 2) .. '>')  -- ← SEGFAULT
end
```

**Error:** `attempt to call local 'Number' (a number value)`

**Explanation:** Bytecode treats `Number` as a function to call instead of the LHS of the `?` operator due to incorrect register indices.

### Test Case 2: testConcat (line 521)
```fluid
function testConcat()
   local function returnString(Arg)
      if Arg then return Arg else return "Result" end
   end

   local v = '' .. (nil ? returnString('true') :> returnString('false')) .. ''
   -- ← FAILS: "attempt to concatenate a boolean value"
end
```

**Error:** `attempt to concatenate a boolean value`

**Explanation:** The ternary result (should be string `"false"`) is treated as boolean, indicating wrong register is being concatenated.

## Register Lifecycle Issue

1. **Parse `Number ? 2` in function argument**:
   - `Number` → register 2
   - `bcemit_binop_left` reserves register (fs->freereg becomes 5)
   - RHS `2` should use reserved register

2. **Register cleanup**:
   - Attempts to collapse `fs->freereg`
   - Condition `rhs_reg > reg` satisfied incorrectly
   - `fs->freereg` set to wrong value

3. **Concatenation/function call**:
   - Expects `fs->freereg` to indicate next available register
   - Uses incorrect indices due to wrong `fs->freereg`
   - Generates invalid bytecode

4. **Runtime**:
   - Bytecode references wrong registers
   - **Error:** Type mismatch or segfault

## Why Concatenation is Particularly Affected

BC_CAT (concatenation) requires consecutive registers:
```
BC_CAT dest, start_reg, end_reg
```

If `fs->freereg` is incorrect:
- `start_reg` and `end_reg` indices are wrong
- Concatenates unrelated register values
- Runtime type errors or memory access violations

## Recommended Fix

The register cleanup logic in `bcemit_binop()` (lines 505-512) must be revised to:

1. Properly account for concatenation and function argument contexts
2. Ensure `fs->freereg` correctly reflects available registers after IF_EMPTY
3. Handle the lifetime of the reserved RHS register correctly
4. Avoid premature register collapsing that orphans intermediate values

## Files to Fix

- `src/fluid/luajit-2.1/src/parser/lj_parse_operators.c`
  - Lines 131-180 (`bcemit_binop_left` IF_EMPTY handling)
  - Lines 425-520 (`bcemit_binop` IF_EMPTY completion)

## Test Files with Failures

- `src/fluid/tests/test_if_empty.fluid:81` (testConcatMsg)
- `src/fluid/tests/test_ternary.fluid:521` (testConcat)
- `src/fluid/tests/test_safe_nav.fluid` (multiple tests)

## Investigation Methodology

1. Ran `ctest --test-dir build/agents -L fluid` to identify failures
2. Created minimal test cases to reproduce the issue
3. Added debug printf() statements to track:
   - Expression parsing flow
   - Register allocation state
   - IF_EMPTY operator processing
4. Analyzed bytecode generation patterns
5. Traced register lifecycle through concatenation context
6. Identified register cleanup logic as root cause

## Conclusion

This is a **register allocation/cleanup bug** in the IF_EMPTY operator implementation. The bug manifests specifically when IF_EMPTY results are used in concatenation or function arguments due to incorrect `fs->freereg` management. The fix requires careful revision of the register cleanup logic to properly handle all usage contexts.
