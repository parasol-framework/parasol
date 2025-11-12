# Fluid IF_EMPTY Operator Concatenation Bug - Investigation Summary

## Current Status (After Partial Fix)

### Test Results
- ✅ **fluid_safe_nav**: PASSING (fixed by recent code changes)
- ❌ **fluid_if_empty**: FAILING - `testConcat` line 70
- ❌ **fluid_ternary**: FAILING - `testConcat` line 521

### Remaining Failures

#### 1. test_if_empty.fluid:70 - Triple Concatenation
```fluid
local val
local v = '' .. (val ? returnString('ignore'))
-- Expected: "Result"
-- Got: "ResultResultResult"
```

**Problem**: The string "Result" is being concatenated 3 times instead of once. This indicates that the BC_CAT instruction is using incorrect register indices, concatenating the same value multiple times.

#### 2. test_ternary.fluid:521 - Type Error
```fluid
local v = '' .. (nil ? returnString('true') :> returnString('false')) .. ''
-- Expected: "Result" (from returnString('false'))
-- Error: "attempt to concatenate a boolean value"
```

**Problem**: Instead of the string result from the ternary expression, a boolean value is being used in concatenation. This suggests the wrong register is being referenced.

## Root Cause Confirmed

The issue is in **register allocation/cleanup** in `src/fluid/luajit-2.1/src/parser/lj_parse_operators.c`:

### What Was Fixed
Lines 502-504 added register cleanup:
```c
// Free the temporary register if it was allocated for the RHS and is at the top of the register stack.
if (rhs_reg != NO_REG && rhs_reg >= fs->nactvar && rhs_reg + 1 == fs->freereg)
   fs->freereg = rhs_reg;
```

This fixed the `fluid_safe_nav` tests but didn't address all concatenation issues.

### What's Still Broken

The register cleanup logic doesn't properly handle the case where:
1. IF_EMPTY reserves a register for RHS evaluation (line 164: `bcreg_reserve(fs, 1)`)
2. The result is used in a BC_CAT (concatenation) operation
3. `fs->freereg` ends up pointing to the wrong location
4. BC_CAT uses incorrect start/end register indices

### Specific Issues

**Issue 1: Triple Concatenation**
- BC_CAT is given register indices that span 3 consecutive registers instead of 2
- All 3 registers contain the same value ("Result")
- Concatenating them produces "ResultResultResult"

**Issue 2: Wrong Type**
- BC_CAT is referencing a register that contains a boolean (probably from internal ternary evaluation)
- Instead of the register containing the ternary result string

## Technical Analysis

The BC_CAT bytecode format is:
```
BC_CAT dest, start_reg, end_reg
```

When `fs->freereg` is incorrect after IF_EMPTY processing:
- `start_reg` and `end_reg` calculations are wrong
- Concatenates unintended registers
- Results in duplicate values or type errors

## What Needs to Be Fixed

The register cleanup in `bcemit_binop()` (lines 493-504) needs to:

1. **Track register state more carefully** when IF_EMPTY is used in concatenation context
2. **Ensure `fs->freereg` accurately reflects** which registers contain valid values vs. temporaries
3. **Handle the BC_CAT case specially** to ensure correct register indices
4. **Avoid premature register collapsing** that orphans intermediate values

## Next Steps

The fix requires deeper changes to how registers are managed during IF_EMPTY evaluation, specifically:

1. Better coordination between `bcemit_binop_left()` (which reserves the register) and `bcemit_binop()` (which uses it)
2. Proper tracking of which registers are "in use" vs. "free" after IF_EMPTY completes
3. Special handling for concatenation context to ensure BC_CAT gets correct indices

The current partial fix (lines 502-504) is insufficient because it only handles the simple case where the RHS register is at the top of the stack. It doesn't handle cases where concatenation introduces additional complexity in register allocation.
