# Phase 3B – Stage 4 Integration Implementation Plan

## Context
This plan details the implementation of Stage 4 from PARSER_P3.md, which integrates the RegisterAllocator, ExpressionValue wrapper, and ControlFlowGraph abstractions into the parser emission pipeline.

## Current State (from Phase 3)
- **Stage 1 ✓**: RegisterAllocator with RAII handles implemented (parse_regalloc.h/cpp)
- **Stage 2 ⬜**: ExpressionValue wrapper created but not integrated (parse_value.h/cpp)
- **Stage 3 ✓**: ControlFlowGraph implemented (parse_control_flow.h/cpp)
- **Stage 4 ⬜**: Integration into IrEmitter and parsing paths (THIS PLAN)

## Scope Analysis

### Files Requiring Modification
1. **ir_emitter.cpp** (1892 lines) - Primary integration target
2. **parse_stmt.cpp** (1010 lines) - Statement emission
3. **parse_regalloc.cpp** (564 lines) - Already has RegisterAllocator
4. **parse_expr.cpp** - Expression parsing
5. **parse_operators.cpp** - Operator emission
6. **parse_scope.cpp** - Scope management

### Integration Points
- **185 occurrences** of legacy expr_discharge/expr_toreg/bcreg_* functions to migrate
- **3 files** with duplicate table operand logic to consolidate
- **IrEmitter** entry points need RegisterAllocator and ControlFlowGraph threading
- Expression lowering paths need ExpressionValue API adoption

## Implementation Strategy

### Phase 1: Foundation (Low Risk, High Value)
Create wrapper utilities to enable gradual migration without breaking existing code.

#### 1.1 Add Helper Functions to ExpressionValue
**File**: `parse_value.h/.cpp`
**Goal**: Provide convenience methods that mirror legacy APIs

```cpp
// Add to ExpressionValue class:
void to_val();                              // Like expr_toval
void discharge_nobranch();                  // Like expr_discharge without jumps
void store_to(ExpressionValue& Target);     // Like bcemit_store
BCReg discharge_to_any_reg(RegisterAllocator& Alloc);  // Combined discharge + to_any_reg
```

**Rationale**: Allows incremental adoption without rewriting entire functions at once.

#### 1.2 Create IrEmitter Context Object
**File**: `ir_emitter.h/.cpp`
**Goal**: Bundle RegisterAllocator, ControlFlowGraph, and FuncState together

```cpp
class IrEmissionContext {
public:
   IrEmissionContext(FuncState* State);

   RegisterAllocator& allocator();
   ControlFlowGraph& cfg();
   FuncState* state() const;

private:
   FuncState* func_state;
   RegisterAllocator register_allocator;
   ControlFlowGraph control_flow_graph;
};
```

**Rationale**: Simplifies function signatures and ensures all three components stay synchronized.

### Phase 2: Entry Point Threading (Medium Risk)
Thread the new context through IrEmitter and key parsing functions.

#### 2.1 Update IrEmitter Class
**File**: `ir_emitter.h/.cpp`
**Changes**:
- Add `IrEmissionContext` member to `IrEmitter` class
- Update all emit_* methods to use context instead of raw FuncState
- Replace direct bcreg_* calls with allocator methods
- Replace JumpListView with ControlFlowEdge where appropriate

**Priority Functions**:
1. `emit_binary_expr()` - High usage, representative pattern
2. `emit_assignment()` - Uses duplicate_table_operands pattern
3. `emit_if_statement()` - Control flow heavy
4. `emit_loop_statement()` - Break/continue edges

#### 2.2 Migrate Compound Assignment Logic
**File**: `parse_stmt.cpp`, function `assign_compound()`
**Current Issue**: Lines 184-212 manually duplicate table base/index
**Solution**: Replace with `RegisterAllocator::duplicate_table_operands()`

**Before**:
```cpp
// Manual duplication at parse_stmt.cpp:189-211
BCReg new_base = fs->freereg;
bcemit_AD(fs, BC_MOV, new_base, lhv.u.s.info);
bcreg_reserve(fs, 1);
// ... manual index duplication ...
```

**After**:
```cpp
RegisterAllocator allocator(fs);
TableOperandCopies copies = allocator.duplicate_table_operands(*lh);
// Use copies.duplicated for RHS evaluation
// copies.reserved auto-releases via RAII
```

### Phase 3: Expression Lowering Migration (High Risk, High Value)
Systematically replace legacy expr_* functions with ExpressionValue methods.

#### 3.1 Create Migration Checklist
For each file, identify and categorize all expr_* usage:

**parse_regalloc.cpp** (33 occurrences):
- [x] Already uses RegisterAllocator internally
- [ ] Wrap ExpDesc in ExpressionValue at function boundaries

**ir_emitter.cpp** (33 occurrences):
- [ ] emit_binary_expr: 8 calls to expr_discharge/expr_toanyreg
- [ ] emit_assignment: 4 calls + manual table duplication
- [ ] emit_conditional: 12 calls for branch generation
- [ ] emit_table_constructor: 5 calls for field emission

**parse_stmt.cpp** (29 occurrences):
- [ ] assign_compound: Replace manual table duplication (priority)
- [ ] parse_return_stmt: 6 calls to expr_tonextreg
- [ ] parse_local_declaration: 4 calls to expr_toreg

**parse_operators.cpp** (49 occurrences):
- [ ] bcemit_arith: 12 calls - fold constant optimization
- [ ] bcemit_comp: 8 calls - comparison emission
- [ ] bcemit_binop: 15 calls - general binary operations
- [ ] bcemit_unop: 6 calls - unary operations

**parse_expr.cpp** (34 occurrences):
- [ ] expr_primary: 8 calls for literal/identifier handling
- [ ] expr_table_constructor: 10 calls for field values
- [ ] expr_function_call: 6 calls for argument passing

#### 3.2 Migration Pattern Template
**Standard transformation**:

```cpp
// BEFORE (legacy):
void emit_foo(FuncState* fs, ExpDesc* e) {
   expr_discharge(fs, e);
   BCReg reg = expr_toanyreg(fs, e);
   bcreg_free(fs, reg);
}

// AFTER (new API):
void emit_foo(IrEmissionContext& ctx, ExpressionValue& value) {
   value.discharge();
   BCReg reg = value.to_any_reg(ctx.allocator());
   value.release(ctx.allocator());
}
```

#### 3.3 Jump Patching Migration
Replace all JumpListView usage with ControlFlowEdge:

```cpp
// BEFORE:
BCPos true_list = e->t;
JumpListView(fs, true_list).patch_to_here();

// AFTER:
ControlFlowEdge true_edge = value.true_jumps(ctx.cfg());
true_edge.patch_here();
```

### Phase 4: Control Flow Edge Integration (Medium Risk)
Replace remaining JumpListView usage with ControlFlowGraph methods.

#### 4.1 Loop Statement Integration
**Files**: `ir_emitter.cpp`, `parse_stmt.cpp`
**Pattern**: Replace break/continue jump lists with CFG break/continue edges

```cpp
// Store break/continue edges in FuncScope or IrEmissionContext
ControlFlowEdge break_edge = ctx.cfg().make_break_edge();
ControlFlowEdge continue_edge = ctx.cfg().make_continue_edge();

// At break statement:
break_edge.append(bcemit_jmp(fs));

// At loop end:
break_edge.patch_here();
```

#### 4.2 Conditional Statement Integration
**Files**: `ir_emitter.cpp`, `parse_stmt.cpp`
**Pattern**: True/false branches become ControlFlowEdge objects

```cpp
ExpressionValue condition(fs, cond_desc);
ControlFlowEdge true_branch = condition.true_jumps(ctx.cfg());
ControlFlowEdge false_branch = condition.false_jumps(ctx.cfg());

// Emit then block
true_branch.patch_here();
// ... then code ...

// Emit else block
false_branch.patch_here();
// ... else code ...
```

### Phase 5: Cleanup and Validation (Low Risk)
Remove legacy function wrappers and validate the migration.

#### 5.1 Deprecate Legacy Functions
**File**: `parse_internal.h`
Mark legacy functions as deprecated or remove entirely:
- `expr_discharge()` → Use `ExpressionValue::discharge()`
- `expr_toreg()` → Use `ExpressionValue::to_reg()`
- `expr_toanyreg()` → Use `ExpressionValue::to_any_reg()`
- `expr_tonextreg()` → Use `ExpressionValue::to_next_reg()`
- `bcreg_reserve()` → Use `RegisterAllocator::reserve()`
- `bcreg_free()` → Use `RegisterAllocator::release()`

#### 5.2 Add CFG Finalization Calls
At function compilation completion, call `cfg.finalize()` to assert all edges are resolved:

```cpp
// In fs_prep_line or similar function end:
ctx.cfg().finalize();  // LJ_DEBUG assertion that no dangling edges
```

#### 5.3 Testing Strategy
1. **Unit level**: Compile debug build and check assertions
2. **Integration**: Run existing Flute test suite
3. **Regression**: Compare bytecode output before/after (should be identical)

## Implementation Order

### Week 1: Foundation
- [x] Day 1: Implement Phase 1.1 (ExpressionValue helpers)
- [x] Day 2: Implement Phase 1.2 (IrEmissionContext)
- [x] Day 3: Test foundation with simple example migration

### Week 2: Entry Points
- [ ] Day 1: Phase 2.1 - IrEmitter threading (structure only)
- [ ] Day 2: Phase 2.2 - Migrate compound assignment (first real usage)
- [ ] Day 3: Compile and validate compound assignment works

### Week 3: Expression Migration (Part 1)
- [ ] Day 1-2: Migrate parse_regalloc.cpp (already uses RegisterAllocator)
- [ ] Day 3: Migrate ir_emitter.cpp binary operations

### Week 4: Expression Migration (Part 2)
- [ ] Day 1: Migrate parse_stmt.cpp
- [ ] Day 2: Migrate parse_operators.cpp
- [ ] Day 3: Migrate parse_expr.cpp

### Week 5: Control Flow
- [ ] Day 1-2: Phase 4.1 - Loop edges
- [ ] Day 3: Phase 4.2 - Conditional edges

### Week 6: Cleanup
- [ ] Day 1: Phase 5.1 - Remove legacy wrappers
- [ ] Day 2: Phase 5.2 - Add finalization
- [ ] Day 3: Phase 5.3 - Full testing pass

## Risk Mitigation

### High Risk Areas
1. **Expression discharge with jumps** - Complex branching logic
   - Mitigation: Migrate non-jump cases first, validate, then add jump handling

2. **Table constructor emission** - Multiple register allocations
   - Mitigation: Use RegisterSpan for array portions, validate LIFO ordering

3. **For-loop iterator protocol** - Specific register layout requirements
   - Mitigation: Preserve exact register positions, add assertions

### Validation Criteria
- [ ] Debug build compiles without warnings
- [ ] All existing Flute tests pass
- [ ] LJ_DEBUG assertions catch any LIFO violations
- [ ] Bytecode output comparison shows no regressions
- [ ] Memory usage unchanged (RAII should be zero-overhead)

## Success Metrics
1. **Zero legacy expr_* calls** in hot paths (ir_emitter, parse_stmt)
2. **All table duplication** uses RegisterAllocator utility
3. **All jump patching** uses ControlFlowGraph
4. **CFG finalization** catches dangling edges in debug builds
5. **Test suite passes** with no regressions

## Notes
- This is a large refactoring but provides significant benefits:
  - LIFO register discipline enforcement
  - Jump edge tracking and validation
  - Clearer ownership semantics
  - Foundation for future AST-only pipeline
- Migration can be done incrementally without breaking existing code
- Each phase provides value independently

## Progress Tracking

### Phase 1: Foundation - ✅ COMPLETE (2025-11-22)

**Phase 1.1 - ExpressionValue Helper Methods:**
- ✅ Implemented `to_val()` - Partial discharge (mirrors `expr_toval`)
- ✅ Implemented `discharge_nobranch()` - Discharge without jumps (mirrors `expr_toreg_nobranch`)
- ✅ Implemented `store_to()` - Store value to target (mirrors `bcemit_store`)
- ✅ Implemented `discharge_to_any_reg()` - Combined discharge + register allocation

**Files Modified:**
- `src/fluid/luajit-2.1/src/parser/parse_value.h` - Added 4 helper method declarations
- `src/fluid/luajit-2.1/src/parser/parse_value.cpp` - Implemented 4 helper methods

**Phase 1.2 - IrEmissionContext Class:**
- ✅ Created `IrEmissionContext` class bundling RegisterAllocator, ControlFlowGraph, and FuncState
- ✅ Implemented `allocator()`, `cfg()`, and `state()` accessors
- ✅ Added include for `parse_regalloc.h` in ir_emitter.h

**Files Modified:**
- `src/fluid/luajit-2.1/src/parser/ir_emitter.h` - Added IrEmissionContext class declaration
- `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` - Implemented IrEmissionContext methods

**Testing:**
- ✅ All 25 fluid tests pass (100% success rate)
- ✅ No regressions detected
- ✅ Debug build compiles cleanly

**Commit:** `0aff1243` - "Implement Phase 1: Foundation for stage 4 integration"

### Phase 2: Entry Point Threading - ✅ COMPLETE (2025-11-22)

**Phase 2.2 - Migrate Compound Assignment:**
- ✅ Removed `duplicate_index_base()` helper function
- ✅ Updated `emit_compound_assignment()` to use `RegisterAllocator::duplicate_table_operands()`
- ✅ Updated `emit_if_empty_assignment()` to use `RegisterAllocator::duplicate_table_operands()`
- ✅ Updated `emit_update_expr()` to use `RegisterAllocator::duplicate_table_operands()`

**Files Modified:**
- `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` - Replaced 3 usages of manual table duplication

**Testing:**
- ✅ All 25 fluid tests pass (100% success rate)
- ✅ Specifically tested compound assignments (fluid_compound test)
- ✅ Update expressions (++/--) work correctly
- ✅ If-empty assignments (?=) work correctly
- ✅ No regressions detected

**Notes:**
- Phase 2.1 (threading IrEmissionContext) was deferred as IrEmitter already has ControlFlowGraph member
- Can be added incrementally later when needed for more complex migrations
- RegisterAllocator::duplicate_table_operands() successfully replaces all manual duplication logic
- RAII TableOperandCopies::reserved auto-releases, simplifying register management
