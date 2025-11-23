# Logical Operators Migration Plan
## Migrate AND/OR/IF_EMPTY to CFG-Based Implementation

**Status: ALL STAGES COMPLETE** ✅ (Stages 1-4)

**Goal:** Replace legacy jump-based short-circuit operators with modern ControlFlowGraph-based implementation

**Operators to Migrate:**
- ✅ `and` (OPR_AND) - Skip RHS evaluation if left is false - **COMPLETE (Stage 2)**
- ✅ `or` (OPR_OR) - Skip RHS evaluation if left is true - **COMPLETE (Stage 3)**
- ✅ `??` (OPR_IF_EMPTY) - Skip RHS evaluation if left is truthy (extended falsey: nil, false, 0, "") - **COMPLETE (Stage 4)**

**Original Implementation:** `ir_emitter.cpp:1480-1488` calls `bcemit_binop_left()` → `bcemit_binop()`

**Current Implementation:** All logical operators (AND, OR, IF_EMPTY) use CFG-based OperatorEmitter methods

---

## Stage 1: Add OperatorEmitter Facade Methods ✅ COMPLETE

**Duration:** ~1 hour
**Complexity:** Low
**Risk:** Minimal (no behavior change)
**Status:** COMPLETED - All tests passing (23/25 baseline maintained)

### Work Items

1. **Add method declarations to `operator_emitter.h`:**
   ```cpp
   // Logical short-circuit operators (CFG-based)
   void emit_logical_and(ExpDesc* left, ExpDesc* right);
   void emit_logical_or(ExpDesc* left, ExpDesc* right);
   void emit_if_empty(ExpDesc* left, ExpDesc* right);
   ```

2. **Add method implementations to `operator_emitter.cpp`:**
   - Initially call legacy helpers (wrapper pattern)
   - Add TODO comments noting future CFG implementation

3. **Update `ir_emitter.cpp` to route through new methods:**
   - Replace direct `bcemit_binop_left`/`bcemit_binop` calls
   - Keep same legacy behavior temporarily

### Testing

**Test Script:** `/tmp/test_logical_stage1.fluid`
```lua
-- AND operator tests
local a = true and "yes"
assert(a == "yes")
local b = false and "no"
assert(b == false)

-- OR operator tests
local c = true or "fallback"
assert(c == true)
local d = false or "fallback"
assert(d == "fallback")

-- IF_EMPTY (??) tests
local e = nil ?? "default"
assert(e == "default")
local f = 0 ?? "default"
assert(f == "default")
local g = "value" ?? "default"
assert(g == "value")

print("Stage 1: All legacy behavior preserved ✓")
```

**Success Criteria:**
- ✅ Code compiles without errors
- ✅ All existing logical operator tests pass
- ✅ No bytecode changes (verify with bytecode dump comparison)
- ✅ Legacy helper call counts unchanged

---

## Stage 2: Implement CFG-Based AND Operator ✅ COMPLETE

**Duration:** ~2-3 hours
**Complexity:** Medium
**Risk:** Medium (behavior change, control flow)
**Status:** COMPLETED - CFG-based implementation working, all tests passing

### Work Items

1. **Implement `emit_logical_and()` in `operator_emitter.cpp`:**
   ```cpp
   void OperatorEmitter::emit_logical_and(ExpDesc* left, ExpDesc* right)
   {
      // Create CFG edges for short-circuit behavior
      // - If left is false, skip RHS and return left (false path)
      // - If left is true, evaluate RHS and return RHS result (true path)

      ControlFlowEdge false_edge = this->cfg->make_false_edge();

      // Discharge left to test condition
      ExpressionValue left_val(this->func_state, *left);
      left_val.discharge();
      *left = left_val.legacy();

      // Branch if left is false (skip RHS)
      // Implementation details based on bcemit_branch_t pattern

      // Evaluate RHS on true path
      // ...

      // Merge paths with result
      // ...
   }
   ```

2. **Handle constant folding optimization:**
   - If left is constant false → return false immediately
   - If left is constant true → return right result

3. **Remove legacy helper calls for AND:**
   - Update tracking in `emit_binary_expr()`

### Testing

**Test Script:** `/tmp/test_and_cfg.fluid`
```lua
-- Basic AND tests
assert((true and true) == true)
assert((true and false) == false)
assert((false and true) == false)
assert((false and false) == false)

-- Value propagation
assert((true and 42) == 42)
assert((false and 42) == false)
assert((nil and "never") == nil)

-- Short-circuit verification (RHS should not evaluate if LHS is false)
local called = false
local function side_effect()
   called = true
   return "result"
end

local result = false and side_effect()
assert(result == false)
assert(called == false, "RHS should not be evaluated when LHS is false")

-- Complex expressions
local x = 5
assert((x > 3 and x < 10) == true)
assert((x > 10 and x < 20) == false)

-- Table access short-circuit
local t = { field = "value" }
assert((t and t.field) == "value")
assert((nil and t.field) == nil)

print("Stage 2: AND operator CFG implementation ✓")
```

**Success Criteria:**
- ✅ All AND operator tests pass
- ✅ Short-circuit behavior verified (RHS not evaluated when unnecessary)
- ✅ Bytecode matches expected CFG pattern
- ✅ Legacy helper calls for AND eliminated
- ✅ OR and IF_EMPTY still use legacy (verify separation)

---

## Stage 3: Implement CFG-Based OR Operator ✅ COMPLETE

**Duration:** ~2 hours
**Complexity:** Medium
**Risk:** Medium
**Status:** COMPLETED - CFG-based implementation working, all tests passing

### Work Items

1. **Implement `emit_logical_or()` in `operator_emitter.cpp`:**
   - Mirror AND logic but invert short-circuit condition
   - If left is true, skip RHS and return left
   - If left is false, evaluate RHS and return RHS result

2. **Handle constant folding:**
   - If left is constant true → return true immediately
   - If left is constant false → return right result

3. **Remove legacy helper calls for OR**

### Testing

**Test Script:** `/tmp/test_or_cfg.fluid`
```lua
-- Basic OR tests
assert((true or true) == true)
assert((true or false) == true)
assert((false or true) == true)
assert((false or false) == false)

-- Value propagation
assert((true or 42) == true)
assert((false or 42) == 42)
assert((nil or "fallback") == "fallback")

-- Short-circuit verification
local called = false
local function side_effect()
   called = true
   return "result"
end

local result = true or side_effect()
assert(result == true)
assert(called == false, "RHS should not be evaluated when LHS is true")

-- Complex expressions
local x = 15
assert((x < 10 or x > 20) == false)
assert((x < 10 or x > 5) == true)

-- Fallback patterns
local config = nil
local default_config = { mode = "default" }
local active = config or default_config
assert(active == default_config)

print("Stage 3: OR operator CFG implementation ✓")
```

**Success Criteria:**
- ✅ All OR operator tests pass
- ✅ Short-circuit behavior verified
- ✅ Bytecode matches expected CFG pattern
- ✅ Legacy helper calls for OR eliminated
- ✅ IF_EMPTY still uses legacy

---

## Stage 4: Implement CFG-Based IF_EMPTY Operator ✅ COMPLETE

**Duration:** ~3-4 hours
**Complexity:** High
**Risk:** High (complex falsey semantics)
**Status:** COMPLETED - CFG-based implementation with extended falsey checks working perfectly

### Work Items

1. **Implement `emit_if_empty()` in `operator_emitter.cpp`:**
   - Use extended falsey check: nil, false, 0, ""
   - Leverage `ValueUse::is_falsey()` for constant optimization
   - Handle runtime falsey checks for non-constants

2. **Constant optimization:**
   - Use `ValueUse` wrapper to check if left is compile-time falsey
   - If left is constant truthy → return left immediately
   - If left is constant falsey → return right result

3. **Runtime falsey handling:**
   - For non-constants, emit bytecode to check extended falsey conditions
   - Follow pattern from `bcemit_binop_left()` lines 178-226
   - Properly handle table operand duplication (stored in ExpDesc.aux with HasRhsReg flag)

4. **Remove legacy helper calls for IF_EMPTY**

### Testing

**Test Script:** `/tmp/test_if_empty_cfg.fluid`
```lua
-- Extended falsey: nil is falsey
assert((nil ?? "default") == "default")

-- Extended falsey: false is falsey
assert((false ?? "default") == "default")

-- Extended falsey: 0 is falsey
assert((0 ?? "default") == "default")

-- Extended falsey: "" is falsey
assert(("" ?? "default") == "default")

-- Truthy values (non-falsey)
assert((true ?? "default") == true)
assert((1 ?? "default") == 1)
assert((-1 ?? "default") == -1)
assert(("text" ?? "default") == "text")
assert(({} ?? "default").constructor == "table")

-- Short-circuit verification
local called = false
local function side_effect()
   called = true
   return "result"
end

local result = "truthy" ?? side_effect()
assert(result == "truthy")
assert(called == false, "RHS should not be evaluated when LHS is truthy")

-- Reset and test falsey path
called = false
result = nil ?? side_effect()
assert(result == "result")
assert(called == true, "RHS should be evaluated when LHS is falsey")

-- Chaining
assert((nil ?? 0 ?? "") == "")
assert((nil ?? 0 ?? "final") == "final")
assert((5 ?? 0 ?? "never") == 5)

-- Table member fallback pattern
local options = { timeout = 0 }  -- 0 is falsey!
local timeout = options.timeout ?? 30
assert(timeout == 30, "0 should be treated as falsey")

options.timeout = 60
timeout = options.timeout ?? 30
assert(timeout == 60)

-- Complex expressions
local function get_value() return nil end
local x = (get_value() ?? 10) + 5
assert(x == 15)

print("Stage 4: IF_EMPTY (??) operator CFG implementation ✓")
```

**Success Criteria:**
- ✅ All extended falsey cases handled correctly (nil, false, 0, "")
- ✅ All truthy cases return left value
- ✅ Short-circuit behavior verified
- ✅ Constant optimization working (compile-time falsey detection)
- ✅ Runtime falsey checks emit correct bytecode
- ✅ Bytecode matches expected CFG pattern
- ✅ Legacy helper calls for IF_EMPTY eliminated

---

## Stage 5: Cleanup and Comprehensive Testing

**Duration:** ~1-2 hours
**Complexity:** Low
**Risk:** Minimal

### Work Items

1. **Remove legacy helper dependencies:**
   - Verify `bcemit_binop_left()` is no longer called from AST pipeline for AND/OR/IF_EMPTY
   - Update legacy helper tracking statistics
   - Add comments noting legacy helpers are for old parser only

2. **Update documentation:**
   - Update `PARSER_P4.md` to mark logical operators as migrated
   - Update `OPERATOR_STATEMENT_MATRIX.md` with CFG implementation notes

3. **Code cleanup:**
   - Remove temporary TODO comments
   - Ensure consistent code style
   - Add comprehensive code comments for complex CFG patterns

### Testing

**Comprehensive Test Script:** `/tmp/test_logical_comprehensive.fluid`
```lua
-- Mix of all three logical operators
assert((true and "a" or "b") == "a")
assert((false and "a" or "b") == "b")
assert((nil and "a" or "b") == "b")

-- Combining with ??
assert((nil ?? false and "never") == false)
assert((nil ?? true and "yes") == "yes")
assert((false or nil ?? "default") == "default")

-- Precedence tests
assert((true or false and false) == true)  -- or has lower precedence
assert(((true or false) and false) == false)

-- Complex nested short-circuits
local a, b, c = false, false, false
local result = (a and (b = true)) or (c = true)
assert(result == true)
assert(b == false, "b should not be assigned (short-circuit)")
assert(c == true, "c should be assigned")

-- Real-world pattern: config with fallbacks
local user_config = nil
local default_config = { theme = "dark", timeout = 30 }
local config = user_config ?? default_config
assert(config.theme == "dark")

-- Function call short-circuiting
local call_count = 0
local function increment()
   call_count = call_count + 1
   return call_count
end

result = (true and increment()) or increment()
assert(result == 1)
assert(call_count == 1, "Second increment should not be called")

-- Ternary-like pattern with ??
local mode = nil
local display = mode ?? "auto"
assert(display == "auto")

print("Stage 5: Comprehensive logical operator tests ✓")
```

**Run Full Test Suite:**
```bash
# Run all Fluid integration tests
ctest --build-config Debug --test-dir build/agents --output-on-failure -R fluid

# Verify bytecode regression tests pass
# Compare bytecode dumps for sample scripts before/after migration
```

**Success Criteria:**
- ✅ All comprehensive tests pass
- ✅ Full Fluid test suite passes
- ✅ No bytecode regressions in existing code
- ✅ Documentation updated
- ✅ Legacy helper tracking shows zero calls for AND/OR/IF_EMPTY from AST pipeline
- ✅ Code review ready

---

## Risk Mitigation

1. **Incremental Approach:** Each stage is independently testable
2. **Legacy Preservation:** Old parser path remains unchanged
3. **Bytecode Verification:** Compare dumps at each stage
4. **Rollback Plan:** Each stage is a separate commit, easy to revert
5. **Test Coverage:** Dedicated test scripts for each operator and edge cases

## Success Metrics

- **Code Quality:** All logical operators use structured CFG edges
- **Performance:** No regression in bytecode size or execution speed
- **Correctness:** 100% test pass rate
- **Maintainability:** Clear separation from legacy code paths
- **Documentation:** Updated plan documents and code comments

## Estimated Total Time

- Stage 1: 1 hour
- Stage 2: 2-3 hours
- Stage 3: 2 hours
- Stage 4: 3-4 hours
- Stage 5: 1-2 hours

**Total: 9-12 hours of focused development work**

Split across multiple sessions with testing and validation between stages.
