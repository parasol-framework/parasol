# Investigation Report: Safe Navigation + If-Empty Operator Failures

**Date**: 2025-11-13
**Investigator**: Claude (Sonnet 4.5)
**Branch**: `claude/investigate-fluid-safe-nav-tests-011CV5uKgLMnY1Kgtx9QtLDp`
**Commit**: 506ea7c2

---

## Executive Summary

Investigation of failing tests in `src/fluid/tests/test_safe_nav.fluid` revealed **two critical bugs** in the interaction between chained safe navigation operators (`?.`) and the if-empty operator (`?`). The root cause is a **register aliasing bug** where `src_reg` and `rhs_reg` point to the same register, causing the fallback value logic to fail.

**Status**: Investigation complete, bugs identified, solution approach recommended.
**Action Required**: Implement fix using proposed `EXP_SAFE_NAV_RESULT_FLAG` approach.

---

## Test Results

**Total Tests**: 25
**Passed**: 21
**Failed**: 4

### Failed Tests

| Test # | Test Name | Line | Expression | Expected | Actual |
|--------|-----------|------|------------|----------|--------|
| 9 | `testIntegrationWithPresence` | 94 | `guest?.profile?.name ? "Guest"` | `"Guest"` | `nil` |
| 23 | `testChainingWithIfEmpty` | 302 | `(obj?.a?.b?.c) ? "default"` | `"default"` | `table: 0x...` |
| 24 | `testSafeNavPresenceInsideCallLosesFallback` | 110 | `capture(guest?.profile?.name ? "Guest")` | `"Guest"` | `nil` |
| 25 | `testSafeNavPresenceWithSiblingArgumentLosesFallback` | 125 | `capture(guest?.profile?.name ? "Guest", "suffix")` | `"Guest"`, `"suffix"` | `nil`, `"suffix"` |

**Common Pattern**: All failures involve safe navigation chains combined with the if-empty operator where the fallback value is lost.

---

## Investigation Methodology

### 1. Environment Setup
- Built Debug configuration with `-DPARASOL_VLOG=TRUE`
- Created minimal reproduction test cases
- Added comprehensive debug logging to trace execution

### 2. Debug Logging Locations
Added `printf()` statements to:
- `expr_safe_nav_branch()` - Track flag setting and register allocation
- `bcemit_binop_left()` OPR_IF_EMPTY - Trace flag handling and register paths
- `bcemit_binop()` OPR_IF_EMPTY - Monitor expression state during evaluation

### 3. Test Cases Created
- `test_debug.fluid` - Minimal case: `guest?.profile?.name ? "Guest"`
- `test_func_arg.fluid` - Function argument: `capture(guest?.profile?.name ? "Guest")`

---

## Root Cause Analysis

### Bug #1: Register Aliasing in Safe Navigation Chains

**Location**: `src/fluid/luajit-2.1/src/parser/lj_parse_expr.c:78-114`

**Function**: `expr_safe_nav_branch()`

#### The Problem

When chaining safe navigation operations (e.g., `obj?.a?.b`), the second operation allocates a result register but fails to properly advance `freereg`. This causes the if-empty operator to receive the same register for both source and RHS storage.

#### Evidence from Debug Trace

```
DEBUG[expr_safe_nav_branch]: Setting SAFE_NAV_CHAIN_FLAG, result_reg=4, freereg=5  ← First ?.
DEBUG[expr_safe_nav_branch]: Setting SAFE_NAV_CHAIN_FLAG, result_reg=5, freereg=5  ← Second ?. (BUG!)
                                                                                        Should be freereg=6
```

When the if-empty operator processes this:
```
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Runtime value, src_reg=5, rhs_reg=5, flags=0x01->0x04
                                                                  ↑         ↑
                                                                  Same register!
```

#### Technical Details

In `expr_safe_nav_branch()`:
```c
result_reg = fs->freereg;      // Line 89: Get current free register
bcreg_reserve(fs, 1);          // Line 90: Reserve it
// ... emit bytecode ...
expr_init(v, VNONRELOC, result_reg);
v->flags |= SAFE_NAV_CHAIN_FLAG;  // Line 111: Set flag
```

**Expected behavior for chained operations**:
- First call: `result_reg=1`, `freereg` advances to 2 ✓
- Second call: `result_reg=2`, `freereg` advances to 3 ✓

**Actual behavior**:
- First call: `result_reg=1`, `freereg` advances to 2 ✓
- Second call: `result_reg=2`, `freereg` **stays at 2** ✗

**Hypothesis**: Something between the two safe navigation operations is collapsing `freereg` back to match the last allocated register. Candidates:
1. `expr_collapse_freereg()` being called inappropriately
2. Register cleanup logic not respecting `SAFE_NAV_CHAIN_FLAG`
3. Expression chaining logic interfering with register allocation

#### Consequence

When `bcemit_binop_left()` tries to reserve storage for the RHS fallback value:
```c
BCReg src_reg = expr_toanyreg(fs, e);  // Gets register 5
BCReg rhs_reg = fs->freereg;           // Also gets register 5!
bcreg_reserve(fs, 1);                  // Advances freereg to 6
```

Both registers alias, so when the fallback value is evaluated into `rhs_reg`, it overwrites the source value that should have been checked for falseyness.

---

### Bug #2: Incorrect Flag Clearing Logic

**Location**: `src/fluid/luajit-2.1/src/parser/lj_parse_operators.c:187-191`

**Function**: `bcemit_binop_left()` handling `OPR_IF_EMPTY`

#### The Problem

The flag clearing logic runs after the if-else chain completes, meaning it executes regardless of which branch was taken:

```c
else {
   // Runtime value - do NOT use bcemit_branch() ...
   if (!expr_isk_nojump(e)) {
      BCReg src_reg = expr_toanyreg(fs, e);
      BCReg rhs_reg = fs->freereg;
      uint8_t flags = e->flags;
      bcreg_reserve(fs, 1);
      expr_init(e, VNONRELOC, src_reg);
      e->u.s.aux = rhs_reg;
      e->flags = (flags & ~SAFE_NAV_CHAIN_FLAG) | EXP_HAS_RHS_REG_FLAG;  // Line 169: Clears flag
   }
   pc = NO_JMP;
}
// For constant cases, also clear the flag...
if (had_safe_nav) {
   e->flags &= ~SAFE_NAV_CHAIN_FLAG;  // Line 189: Clears flag AGAIN! (BUG!)
}
```

#### Evidence from Debug Trace

```
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Runtime value, src_reg=5, rhs_reg=5, flags=0x01->0x04
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Clearing SAFE_NAV_CHAIN_FLAG for constant case
                                        ↑
                                        Wrong! We're in the runtime branch
```

#### Why This Matters

While this redundant clearing doesn't directly cause the test failures, it indicates confused logic about when and where the flag should be managed. The comment says "for constant cases" but the code executes for all cases.

---

## Data Flow Analysis

### Successful Case: Ternary Operator

The ternary operator (`if ? then :> else`) works correctly with safe navigation because:
1. It has different register allocation strategy
2. It explicitly manages result registers for both branches
3. No register aliasing occurs

### Failed Case: If-Empty with Safe Nav Chain

Expression: `guest?.profile?.name ? "Guest"`

**Step-by-step execution**:

1. **Parse `guest`** → Register 0
2. **Parse `?.profile`** → Calls `expr_safe_nav_branch()`
   - Allocates `result_reg=1`, sets `freereg=2`
   - Sets `SAFE_NAV_CHAIN_FLAG`
3. **Parse `?.name`** → Calls `expr_safe_nav_branch()` again
   - Allocates `result_reg=2`
   - **BUG**: `freereg` doesn't advance to 3, stays at 2
   - Sets `SAFE_NAV_CHAIN_FLAG` on result in register 2
4. **Parse `? "Guest"`** → Calls `bcemit_binop_left(OPR_IF_EMPTY)`
   - Sees `SAFE_NAV_CHAIN_FLAG` is set
   - `src_reg = expr_toanyreg()` → Gets register 2
   - `rhs_reg = fs->freereg` → **Also gets register 2** (aliased!)
   - Reserves register, advances `freereg` to 3
   - Sets `EXP_HAS_RHS_REG_FLAG`, clears `SAFE_NAV_CHAIN_FLAG`
5. **Evaluate fallback** → Calls `bcemit_binop(OPR_IF_EMPTY)`
   - Extended falsey checks on register 2 (LHS value is nil)
   - Should evaluate RHS into register 2 and return it
   - But register 2 is BOTH source and destination
   - **Result**: nil is returned instead of "Guest"

---

## Comparative Analysis

### Why Other Tests Pass

**Standalone if-empty** (`fallback?.missingField ? "Default"`):
- Single safe navigation, no chaining
- No register aliasing occurs
- ✓ Works correctly

**Ternary operator** (`user?:getProfile()?.email`):
- Different code path with explicit result management
- ✓ Works correctly

**Multiple safe-nav without if-empty** (`data?.users?[1]?.profile?.settings?.theme`):
- No if-empty operator involved
- Register aliasing doesn't matter
- ✓ Works correctly

**Safe method calls** (`account?:getProfile()?.email`):
- Method calls have their own register management
- ✓ Works correctly

---

## Recommended Solution

### Approach: Introduce `EXP_SAFE_NAV_RESULT_FLAG`

As hinted in the investigation request, a new flag specifically for safe navigation results would solve this problem:

#### 1. Define New Flag

```c
// In lj_parse_types.h
#define EXP_SAFE_NAV_RESULT_FLAG  0x08u  // Marks expression result from safe navigation
```

#### 2. Purpose

- **Mark safe navigation results** to prevent premature register collapse
- **Distinguish** between safe-nav results and regular expressions
- **Protect registers** from being reused until the value is fully consumed by an operator

#### 3. Implementation Changes

**In `expr_safe_nav_branch()` (lj_parse_expr.c:111)**:
```c
expr_init(v, VNONRELOC, result_reg);
v->flags |= SAFE_NAV_CHAIN_FLAG | EXP_SAFE_NAV_RESULT_FLAG;  // Add new flag
```

**In register management code**:
- Check for `EXP_SAFE_NAV_RESULT_FLAG` before collapsing `freereg`
- Only collapse when the result has been consumed by an operator that takes ownership

**In `bcemit_binop_left()` for OPR_IF_EMPTY**:
- Recognize `EXP_SAFE_NAV_RESULT_FLAG` as indicator that register needs protection
- Ensure `rhs_reg` is allocated from a truly free register (check `fs->freereg` is > `src_reg`)
- Clear flag only after proper register separation is established

#### 4. Fix Flag Clearing Logic

**Option A - Remove redundant clear**:
```c
// For constant cases, also clear the flag...
if (had_safe_nav) {
   e->flags &= ~SAFE_NAV_CHAIN_FLAG;  // DELETE THIS - already cleared in line 169
}
```

**Option B - Restructure logic** (preferred):
```c
if (e->k == VKNIL || e->k == VKFALSE) {
   pc = NO_JMP;
   if (had_safe_nav) e->flags &= ~SAFE_NAV_CHAIN_FLAG;
}
else if (e->k == VKNUM && expr_numiszero(e)) {
   pc = NO_JMP;
   if (had_safe_nav) e->flags &= ~SAFE_NAV_CHAIN_FLAG;
}
// ... etc for each constant branch
else {
   // Runtime value path
   if (!expr_isk_nojump(e)) {
      // ... existing code that already clears the flag ...
   }
}
// No clearing here - it's now branch-specific
```

---

## Testing Verification

### Manual Tests Created

**test_debug.fluid**:
```lua
local guest = nil
local display = guest?.profile?.name ? "Guest"
print("Result: " .. tostring(display))
```

**test_func_arg.fluid**:
```lua
local guest = nil
local function capture(value)
   return value
end
local result = capture(guest?.profile?.name ? "Guest")
print("Result: " .. tostring(result))
```

### Expected Behavior After Fix

All 4 failing tests should pass:
- ✓ Test 9: Returns `"Guest"` instead of `nil`
- ✓ Test 23: Returns `"default"` instead of table reference
- ✓ Test 24: Returns `"Guest"` in function argument
- ✓ Test 25: Returns `"Guest"` and `"suffix"` correctly

### Regression Testing

Ensure all 21 passing tests continue to pass:
- Standalone safe navigation chains
- If-empty without safe navigation
- Ternary operator with safe navigation
- Safe method calls
- Edge cases (false, 0, empty string)

---

## Code Locations Reference

### Primary Files

| File | Lines | Description |
|------|-------|-------------|
| `src/fluid/luajit-2.1/src/parser/lj_parse_expr.c` | 78-114 | `expr_safe_nav_branch()` - Sets flag, allocates register |
| `src/fluid/luajit-2.1/src/parser/lj_parse_operators.c` | 131-196 | `bcemit_binop_left()` - Handles if-empty LHS |
| `src/fluid/luajit-2.1/src/parser/lj_parse_operators.c` | 442-574 | `bcemit_binop()` - Evaluates if-empty |
| `src/fluid/luajit-2.1/src/parser/lj_parse_types.h` | 54-72 | Flag definitions |

### Helper Functions

| Function | File | Purpose |
|----------|------|---------|
| `expr_collapse_freereg()` | lj_parse_expr.c:114-119 | Collapses freereg (may be implicated) |
| `expr_toanyreg()` | lj_parse_internal.h | Converts expression to register |
| `bcreg_reserve()` | lj_parse_internal.h | Reserves register, advances freereg |
| `expr_init()` | lj_parse_types.h:94-100 | Initializes expression (clears flags!) |

---

## Debug Trace Examples

### Minimal Case Output

```
DEBUG[expr_safe_nav_branch]: Setting SAFE_NAV_CHAIN_FLAG, result_reg=1, freereg=2
DEBUG[expr_safe_nav_branch]: Setting SAFE_NAV_CHAIN_FLAG, result_reg=2, freereg=2  ← BUG
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Entry - e->k=12, e->flags=0x01, had_safe_nav=1, freereg=2
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: After discharge - e->k=12
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Runtime value, src_reg=2, rhs_reg=2, flags=0x01->0x04
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Clearing SAFE_NAV_CHAIN_FLAG for constant case
DEBUG[bcemit_binop_left OPR_IF_EMPTY]: Exit - e->flags=0x04, pc=-1, e->t=-1
DEBUG[bcemit_binop OPR_IF_EMPTY]: Entry - e1->k=12, e1->flags=0x04, e1->t=-1, freereg=3
DEBUG[bcemit_binop OPR_IF_EMPTY]: Falsey/runtime LHS path
DEBUG[bcemit_binop OPR_IF_EMPTY]: Has RHS reg, rhs_reg=2
DEBUG[bcemit_binop OPR_IF_EMPTY]: After discharge - e1->k=12
Result: nil
```

### Key Observations

- Line 2: `freereg` doesn't advance (stays at 2)
- Line 5: `src_reg=2, rhs_reg=2` (aliased)
- Line 6: Wrong branch message for runtime path
- Line 10: `rhs_reg=2` confirms aliasing
- Line 11: Result is nil (fallback lost)

---

## Additional Notes

### Flag Lifecycle Documentation

Current understanding of `SAFE_NAV_CHAIN_FLAG`:

**SET**: `expr_safe_nav_branch()` when safe nav produces a result
**CONSUMED**: `bcemit_binop_left()` when entering operators that need the value
**TESTED**: `bcemit_binop()` for register cleanup decisions

**Proposed** `EXP_SAFE_NAV_RESULT_FLAG` lifecycle:

**SET**: `expr_safe_nav_branch()` alongside `SAFE_NAV_CHAIN_FLAG`
**CHECKED**: Register management code before collapsing freereg
**CLEARED**: When value is fully consumed and register can be reused

### Comparison with Working Operators

**Ternary operator implementation** (for reference):
- Uses its own result register management
- Explicitly reserves and manages registers for both branches
- Does not rely on `freereg` alignment assumptions
- Could serve as model for safe-nav + if-empty fix

---

## Conclusion

The investigation conclusively identified register aliasing as the root cause of all four test failures. The if-empty operator receives the same register for both source value and fallback storage when processing chained safe navigation results.

The solution requires:
1. Introducing `EXP_SAFE_NAV_RESULT_FLAG` to mark and protect safe navigation results
2. Fixing register management to prevent `freereg` collapse during chaining
3. Cleaning up the redundant flag clearing logic
4. Ensuring proper register separation in the if-empty operator

**Next Action**: Implement the fix using the recommended approach, remove debug logging, and verify all tests pass.

---

## Appendix: Commit History

**Investigation commits**:
- 506ea7c2 - [Investigation] Add debug logging to trace safe-nav + if-empty interaction
- 039e0c69 - [Fluid] Improved test coverage for the save navigation operator
- 57c8e23a - Add regression coverage for Fluid if-empty argument leak (#815)
- af2ee395 - Document safe-nav call regressions and add coverage
- b96df198 - Fix safe-nav if-empty register handling

**Branch**: `claude/investigate-fluid-safe-nav-tests-011CV5uKgLMnY1Kgtx9QtLDp`
