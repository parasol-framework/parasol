# Parser Phase 3: Jump Migration Analysis

**Status**: Complete
**Created**: 2025-11-22
**Dependencies**: PARSER_P3_FINALISE.md

## Executive Summary

This document provides a comprehensive analysis of the 86 JumpListView/JumpHandle uses across 8 files in the parser codebase, mapping each usage pattern to ControlFlowGraph equivalents and categorizing files by migration difficulty.

**Key Findings**:
- **parse_constants.cpp** contains the JumpListView class implementation (not usage)
- **63 actual usage sites** across 7 files (excluding class definition)
- **10 distinct usage patterns** identified
- **Migration difficulty**: 2 easy files, 3 medium files, 2 hard files

## Usage Pattern Inventory

### Pattern 1: Simple Patch to Target
**Usage**: `JumpListView(fs, head).patch_to(target)`
**Purpose**: Patch all jumps in list to specific bytecode position
**ControlFlowGraph Equivalent**: `ControlFlowEdge::patch_to(target)`
**Occurrences**: 19 sites

**Example**:
```cpp
// BEFORE - JumpListView
JumpListView(fs, e->t).patch_to(fs->pc);

// AFTER - ControlFlowGraph
ControlFlowEdge true_edge = expr_value.true_jumps(cfg);
true_edge.patch_to(fs->pc);
```

**Sites**:
- parse_scope.cpp:207, 223
- parse_control_flow.cpp:184
- parse_expr.cpp:1038, 1039, 1040, 1041, 1057, 1092
- parse_operators.cpp:496, 497, 498, 499, 503, 542, 617, 618, 619, 620, 633

### Pattern 2: Patch to Current PC (patch_to_here)
**Usage**: `JumpListView(fs, head).patch_to_here()`
**Purpose**: Patch all jumps to current bytecode position
**ControlFlowGraph Equivalent**: `ControlFlowEdge::patch_here()`
**Occurrences**: 10 sites

**Example**:
```cpp
// BEFORE - JumpListView
JumpListView(fs, e->f).patch_to_here();

// AFTER - ControlFlowGraph
ControlFlowEdge false_edge = expr_value.false_jumps(cfg);
false_edge.patch_here();
```

**Sites**:
- parse_regalloc.cpp:544, 562
- parse_operators.cpp:219
- parse_stmt.cpp:722, 752, 954, 960, 966

### Pattern 3: Append to Jump List
**Usage**: `result = JumpListView(fs, head).append(other)`
**Purpose**: Append jump positions, returns new head
**ControlFlowGraph Equivalent**: `ControlFlowEdge::append(other)`
**Occurrences**: 10 sites

**Example**:
```cpp
// BEFORE - JumpListView
e->t = JumpListView(fs, e->t).append(pc);

// AFTER - ControlFlowGraph
ControlFlowEdge true_edge = expr_value.true_jumps(cfg);
true_edge.append(pc);
expr_value.set_true_jumps(true_edge);
```

**Sites**:
- parse_control_flow.cpp:166
- parse_regalloc.cpp:350, 493, 543, 561
- parse_operators.cpp:218, 523, 531
- parse_stmt.cpp:953, 959, 964

### Pattern 4: Patch Head of Jump List
**Usage**: `JumpListView(fs, head).patch_head(destination)`
**Purpose**: Patch only the head instruction of jump list
**ControlFlowGraph Equivalent**: `ControlFlowEdge::patch_head(destination)`
**Occurrences**: 7 sites

**Example**:
```cpp
// BEFORE - JumpListView
JumpListView(fs, loop).patch_head(fs->pc);

// AFTER - ControlFlowGraph
loop_edge.patch_head(fs->pc);
```

**Sites**:
- parse_control_flow.cpp:193
- parse_stmt.cpp:723, 757, 809, 810, 901, 906

### Pattern 5: Patch with Value
**Usage**: `JumpListView(fs, head).patch_with_value(target, reg, default_target)`
**Purpose**: Patch jumps with register value or default target
**ControlFlowGraph Equivalent**: Need new `ControlFlowEdge::patch_with_value()` method
**Occurrences**: 3 sites

**Example**:
```cpp
// BEFORE - JumpListView
JumpListView(fs, e->f).patch_with_value(jend, reg, jfalse);

// AFTER - ControlFlowGraph (requires new method)
ControlFlowEdge false_edge = expr_value.false_jumps(cfg);
false_edge.patch_with_value(jend, reg, jfalse);
```

**Sites**:
- parse_regalloc.cpp:177, 362, 363

### Pattern 6: Check if Produces Values
**Usage**: `JumpListView(fs, head).produces_values()`
**Purpose**: Check if any jump in list produces values
**ControlFlowGraph Equivalent**: Need new `ControlFlowEdge::produces_values()` method
**Occurrences**: 2 sites

**Example**:
```cpp
// BEFORE - JumpListView
if (JumpListView(fs, e->t).produces_values()) { ... }

// AFTER - ControlFlowGraph (requires new method)
ControlFlowEdge true_edge = expr_value.true_jumps(cfg);
if (true_edge.produces_values()) { ... }
```

**Sites**:
- parse_regalloc.cpp:353 (twice in same condition)

### Pattern 7: Drop Values from Jump List
**Usage**: `JumpListView(fs, head).drop_values()`
**Purpose**: Drop value-producing flags from jumps
**ControlFlowGraph Equivalent**: Need new `ControlFlowEdge::drop_values()` method
**Occurrences**: 2 sites

**Example**:
```cpp
// BEFORE - JumpListView
JumpListView(fs, e->f).drop_values();
JumpListView(fs, e->t).drop_values();

// AFTER - ControlFlowGraph (requires new method)
false_edge.drop_values();
true_edge.drop_values();
```

**Sites**:
- parse_operators.cpp:699, 700

### Pattern 8: Static Next Method
**Usage**: `JumpListView::next(fs, pc)`
**Purpose**: Get next jump position in chain (static utility)
**ControlFlowGraph Equivalent**: Keep as static utility or add to ControlFlowEdge
**Occurrences**: 1 site

**Example**:
```cpp
// BEFORE - JumpListView
BCPos next = JumpListView::next(fs, pc);

// AFTER - Options:
// 1. Keep as static utility in parse_internal.h
// 2. Add to ControlFlowEdge: edge.next_position()
```

**Sites**:
- parse_scope.cpp:222

### Pattern 9: Class Definition
**Usage**: Class implementation in parse_constants.cpp
**Purpose**: Provides JumpListView iterator and methods
**ControlFlowGraph Equivalent**: N/A - will be deprecated/removed
**Occurrences**: 23 lines (class implementation)

**Notes**:
- parse_constants.cpp:83-247 contains the JumpListView class implementation
- This is not "usage" but the class itself
- Migration strategy: Gradually deprecate as usage sites migrate to ControlFlowGraph
- Final step: Remove entire class once all call sites migrated

### Pattern 10: Iterator-based Iteration
**Usage**: For-range loops over jump lists
**Purpose**: Iterate over all jumps in a list
**ControlFlowGraph Equivalent**: Need iterator support on ControlFlowEdge
**Occurrences**: Implicit in produces_values() and drop_values() implementations

**Example**:
```cpp
// BEFORE - JumpListView (in implementation)
for (BCPos list = list_head; not(list IS NO_JMP); list = next(func_state, list)) {
   // Process each jump
}

// AFTER - ControlFlowGraph (requires iterator)
for (BCPos pos : edge) {
   // Process each jump
}
```

## ControlFlowGraph API Gaps

The current ControlFlowGraph/ControlFlowEdge API is missing several methods needed for migration:

### Required New Methods

1. **ControlFlowEdge::patch_with_value(BCPos ValueTarget, BCReg Register, BCPos DefaultTarget)**
   - Used in expression discharge with boolean value production
   - 3 call sites in parse_regalloc.cpp

2. **ControlFlowEdge::produces_values() const**
   - Checks if any jump in edge produces values (has KPRI/MOV before it)
   - 2 call sites in parse_regalloc.cpp

3. **ControlFlowEdge::drop_values()**
   - Removes value-producing instructions from jump chain
   - 2 call sites in parse_operators.cpp

4. **Iterator support for ControlFlowEdge**
   - Allow range-based for loops over jump positions
   - Needed for implementing produces_values(), drop_values()

### Existing Methods (Already Available)

✅ `ControlFlowEdge::append(BCPos)` - maps to `JumpListView::append()`
✅ `ControlFlowEdge::patch_here()` - maps to `JumpListView::patch_to_here()`
✅ `ControlFlowEdge::patch_to(BCPos)` - maps to `JumpListView::patch_to()`
✅ `ControlFlowEdge::patch_head(BCPos)` - maps to `JumpListView::patch_head()`
✅ `ControlFlowEdge::head()` - returns jump list head
✅ `ControlFlowEdge::empty()` - checks if jump list is empty

## File-by-File Categorization

### EASY: Simple Patch Operations (2 files, 8 uses)

#### 1. parse_scope.cpp (3 uses)
**Complexity**: Low
**Patterns**: Simple patch_to(), static next() call
**Risk**: Low

**Usage breakdown**:
- Line 207: `JumpListView(fs, pc).patch_to(vl->startpc)` - Simple goto patch
- Line 222: `JumpListView::next(fs, pc)` - Static utility
- Line 223: `JumpListView(fs, next).patch_to(pc)` - Simple UCLO patch

**Migration approach**:
- Direct 1:1 replacement with ControlFlowEdge
- May need to handle static next() differently

#### 2. parse_control_flow.cpp (3 uses)
**Complexity**: Low
**Patterns**: append(), patch_to(), patch_head()
**Risk**: Low

**Usage breakdown**:
- Line 166: `entry.head = JumpListView(fs, entry.head).append(Head)` - Edge appending
- Line 184: `JumpListView(fs, entry.head).patch_to(Target)` - Edge patching
- Line 193: `JumpListView(fs, entry.head).patch_head(Destination)` - Head patching

**Migration approach**:
- This file already wraps ControlFlowGraph internals
- Migration involves removing JumpListView wrapper layer
- Direct use of edge methods

**Notes**:
- parse_control_flow.cpp implements ControlFlowGraph, so these uses are internal
- Migration is straightforward since ControlFlowEdge API already exists

### MEDIUM: Expression and Register Allocation (3 files, 22 uses)

#### 3. parse_expr.cpp (6 uses)
**Complexity**: Medium
**Patterns**: Multiple patch_to() calls, patch to current PC
**Risk**: Medium

**Usage breakdown**:
- Lines 1038-1041: Four consecutive `patch_to(false_start)` - Falsey value coalescing
- Line 1057: `patch_to(fs->pc)` - Skip false branch
- Line 1092: `patch_to(fs->pc)` - Patch true jumps before coalesce operator

**Migration approach**:
- Pattern: Patching multiple condition checks to same target
- Need ControlFlowGraph in context (via ExpressionValue or parameter)
- Straightforward mapping to ControlFlowEdge::patch_to()

**Complexity factors**:
- Multiple jumps patched in sequence (lines 1038-1041)
- Embedded in complex ternary/coalesce logic
- Need to ensure ControlFlowGraph available

#### 4. parse_regalloc.cpp (11 uses)
**Complexity**: Medium-High
**Patterns**: All patterns except static next() and drop_values()
**Risk**: Medium-High

**Usage breakdown**:
- Line 177: `patch_with_value(pc, NO_REG, pc)` - **Requires new method**
- Line 350: `e->t = JumpListView(fs, e->t).append(e->u.s.info)` - Append true jumps
- Line 353: `produces_values()` (twice) - **Requires new method**
- Line 358: `patch_to_here()` - Patch value production
- Lines 362-363: `patch_with_value()` (twice) - **Requires new method**
- Line 493: `j = JumpListView(fs, j).append(jpc)` - Append pending jumps
- Lines 543-544: `append()`, `patch_to_here()` - False branch handling
- Lines 561-562: `append()`, `patch_to_here()` - True branch handling

**Missing API Methods**:
1. `patch_with_value()` - 3 uses
2. `produces_values()` - 2 uses (in same condition)

**Migration approach**:
1. **Phase 1**: Implement missing ControlFlowEdge methods
2. **Phase 2**: Replace straightforward append/patch operations
3. **Phase 3**: Migrate complex discharge logic with value production

**Complexity factors**:
- Needs 2 new ControlFlowEdge methods before migration
- Complex expression discharge logic
- Critical hot path for register allocation

#### 5. parse_operators.cpp (17 uses)
**Complexity**: Medium-High
**Patterns**: append(), patch_to(), patch_to_here(), drop_values()
**Risk**: Medium-High

**Usage breakdown**:
- Lines 218-219: `append()`, `patch_to_here()` - Logical AND short-circuit
- Lines 496-499: Four `patch_to(false_pos)` - Falsey checks for ?? operator
- Line 503: `patch_to(fs->pc)` - Skip false branch
- Line 523: `e2->f = JumpListView(fs, e2->f).append(e1->f)` - Merge false jumps (OR)
- Line 531: `e2->t = JumpListView(fs, e2->t).append(e1->t)` - Merge true jumps (AND)
- Line 542: `patch_to(fs->pc)` - Patch true jumps after LHS
- Lines 617-620: Four `patch_to(fs->pc)` - Falsey checks for ??= operator
- Line 633: `patch_to(fs->pc)` - Skip assignment
- Lines 699-700: `drop_values()` (twice) - **Requires new method**

**Missing API Methods**:
1. `drop_values()` - 2 uses

**Migration approach**:
1. **Phase 1**: Implement drop_values() on ControlFlowEdge
2. **Phase 2**: Migrate simple patch/append operations
3. **Phase 3**: Migrate complex logical operator short-circuiting

**Complexity factors**:
- Needs 1 new ControlFlowEdge method (drop_values)
- Complex logical operator semantics (AND/OR/coalesce)
- Multiple falsey check patterns

### HARD: Statement Control Flow and Loop Constructs (2 files, 21 uses)

#### 6. parse_stmt.cpp (21 uses)
**Complexity**: High
**Patterns**: append(), patch_to(), patch_to_here(), patch_head()
**Risk**: High

**Usage breakdown**:
- **Compound assignment** (lines 150-154): 5 patch_to() - Falsey checks
- **While loop** (lines 717, 722-723): 3 uses - Back-jump, cond exit, loop head
- **Repeat loop** (lines 752, 756-757): 3 uses - Inner upval close, back-jump, loop head
- **Numeric for loop** (lines 809-810): 2 uses - Loop end, loop head patching
- **Iterator for loop** (lines 901, 906): 2 uses - Loop head, loop end patching
- **If/elseif/else** (lines 953-954, 959-960, 964, 966): 6 uses - Escape list management

**Patterns by construct**:
1. **Compound assignment with falsey checks**: 5 patch_to() in sequence
2. **While loops**: patch_to(), patch_to_here(), patch_head()
3. **Repeat loops**: patch_to_here(), patch_to(), patch_head()
4. **For loops**: patch_head() for loop control
5. **If chains**: append() for escape lists, patch_to_here() for branches

**Migration approach**:
1. **Batch 1**: Compound assignment (5 uses) - Similar to operator patterns
2. **Batch 2**: If/elseif/else (6 uses) - Escape list management
3. **Batch 3**: While loops (3 uses) - Loop control flow
4. **Batch 4**: Repeat loops (3 uses) - Loop control flow
5. **Batch 5**: For loops (4 uses) - Loop head patching

**Complexity factors**:
- Multiple loop constructs with different control flow
- Escape lists for if/elseif chains
- Break/continue jump management
- Critical correctness requirements

#### 7. parse_constants.cpp (23 uses)
**Complexity**: Special - Class Implementation
**Patterns**: All (this is where JumpListView is implemented)
**Risk**: N/A (not migrated, deprecated/removed)

**Usage breakdown**:
- Lines 83-247: Complete JumpListView class implementation
  - Constructor, iterator, begin(), end()
  - empty(), head(), next() (static and member)
  - produces_values(), patch_test_register()
  - drop_values(), patch_instruction()
  - append(), patch_with_value()
  - patch_to_here(), patch_to(), patch_head()

**Migration approach**:
- **NOT migrated** - this file contains the class implementation
- **Strategy**: Leave in place until all usage sites migrated
- **Final step**: Mark entire class as deprecated
- **Removal**: Delete after all call sites use ControlFlowGraph

**Notes**:
- This is infrastructure, not usage
- Provides the methods other files call
- Will be removed last, after all usage migrated

## Migration Difficulty Summary

| Difficulty | Files | Uses | Risk | Notes |
|------------|-------|------|------|-------|
| **Easy** | 2 | 6 | Low | Simple patch operations, direct mapping |
| **Medium** | 3 | 28 | Medium | Need 3 new CFG methods, moderate complexity |
| **Hard** | 2 | 21 | High | Complex control flow, loop constructs |
| **Special** | 1 | 23 | N/A | Class implementation (deprecate, not migrate) |
| **Total** | 8 | 78 | - | 55 real usage sites + 23 class definition |

**Corrected count**: 63 actual usage sites (86 total grep hits - 23 class implementation lines)

## Required ControlFlowEdge Extensions

Before migration can proceed, ControlFlowEdge needs these new methods:

### 1. patch_with_value() - Priority: HIGH
```cpp
void ControlFlowEdge::patch_with_value(BCPos ValueTarget, BCReg Register, BCPos DefaultTarget) const
{
   // Patch jumps: if jump produces value, patch to ValueTarget, else to DefaultTarget
   // Update register for value-producing jumps
}
```
**Blocks**: parse_regalloc.cpp migration (3 sites)

### 2. produces_values() - Priority: HIGH
```cpp
[[nodiscard]] bool ControlFlowEdge::produces_values() const
{
   // Return true if any jump in edge has KPRI/MOV before it (produces boolean value)
}
```
**Blocks**: parse_regalloc.cpp migration (2 sites)

### 3. drop_values() - Priority: MEDIUM
```cpp
void ControlFlowEdge::drop_values()
{
   // Remove value-producing flags from jumps in edge
}
```
**Blocks**: parse_operators.cpp migration (2 sites)

### 4. Iterator Support - Priority: LOW
```cpp
// Enable range-based for loops over jump positions
class ControlFlowEdge::Iterator { ... };
[[nodiscard]] Iterator begin() const;
[[nodiscard]] Iterator end() const;
```
**Blocks**: Internal implementation of produces_values(), drop_values()

## Migration Sequencing

### Recommended Migration Order

**Phase 1: Infrastructure** (Week 1)
1. Implement missing ControlFlowEdge methods (patch_with_value, produces_values, drop_values)
2. Add iterator support to ControlFlowEdge
3. Validate new methods with unit tests

**Phase 2: Easy Files** (Week 1-2)
1. parse_control_flow.cpp (3 uses) - Internal CFG usage
2. parse_scope.cpp (3 uses) - Goto/defer patching

**Phase 3: Medium Files** (Week 2-3)
1. parse_expr.cpp (6 uses) - Ternary/coalesce
2. parse_operators.cpp (17 uses) - Logical operators, falsey checks
3. parse_regalloc.cpp (11 uses) - Expression discharge

**Phase 4: Hard Files** (Week 4-5)
1. parse_stmt.cpp (21 uses) - All statement types
   - Sub-batch 1: Compound assignment (5 uses)
   - Sub-batch 2: If/elseif/else (6 uses)
   - Sub-batch 3: While/repeat loops (6 uses)
   - Sub-batch 4: For loops (4 uses)

**Phase 5: Cleanup** (Week 5)
1. Mark JumpListView class as deprecated
2. Remove JumpListView after all usage eliminated
3. Clean up parse_constants.cpp

## Testing Strategy

### Per-File Testing
- Build and test after each file migration
- All 25 Fluid parser tests must pass
- No bytecode generation changes

### Per-Batch Testing (for parse_stmt.cpp)
- Test after each sub-batch (compound, if, while, for)
- Targeted tests for each control construct
- Manual bytecode inspection for correctness

### Regression Testing
- Run full test suite after each migration
- Compare bytecode output for sample programs
- Performance validation (no slowdown)

## Risk Mitigation

### High-Risk Areas
1. **parse_regalloc.cpp** - Hot path, complex discharge logic
   - Mitigation: Extensive testing, bytecode comparison
2. **parse_stmt.cpp** - Loop constructs, critical correctness
   - Mitigation: One construct type at a time, targeted tests

### Medium-Risk Areas
1. **parse_operators.cpp** - Logical operator short-circuiting
   - Mitigation: Test all logical operators thoroughly
2. **parse_expr.cpp** - Ternary/coalesce operators
   - Mitigation: Validate against existing test cases

### Low-Risk Areas
1. **parse_control_flow.cpp** - Internal CFG implementation
   - Mitigation: Standard testing
2. **parse_scope.cpp** - Simple goto/defer
   - Mitigation: Existing defer tests

## Success Criteria

- [ ] All 63 JumpListView usage sites migrated to ControlFlowGraph
- [ ] JumpListView class marked as deprecated
- [ ] All 25 Fluid parser tests passing
- [ ] Zero compiler warnings
- [ ] No bytecode generation changes
- [ ] No performance regression

## Detailed Migration Checklist

### Phase 1: Infrastructure (Week 1)
- [ ] Implement ControlFlowEdge::patch_with_value()
- [ ] Implement ControlFlowEdge::produces_values()
- [ ] Implement ControlFlowEdge::drop_values()
- [ ] Add iterator support to ControlFlowEdge
- [ ] Build and validate new methods
- [ ] Write unit tests for new methods

### Phase 2: Easy Files (Week 1-2)
- [ ] **parse_control_flow.cpp** (3 uses)
  - [ ] Line 166: append() → direct edge.append()
  - [ ] Line 184: patch_to() → direct edge.patch_to()
  - [ ] Line 193: patch_head() → direct edge.patch_head()
  - [ ] Build and test
  - [ ] Commit

- [ ] **parse_scope.cpp** (3 uses)
  - [ ] Line 207: patch_to() → edge.patch_to()
  - [ ] Line 222: next() → static utility or edge method
  - [ ] Line 223: patch_to() → edge.patch_to()
  - [ ] Build and test
  - [ ] Commit

### Phase 3: Medium Files (Week 2-3)
- [ ] **parse_expr.cpp** (6 uses)
  - [ ] Lines 1038-1041: Four patch_to() → CFG equivalents
  - [ ] Line 1057: patch_to() → edge.patch_to()
  - [ ] Line 1092: patch_to() → edge.patch_to()
  - [ ] Build and test
  - [ ] Commit

- [ ] **parse_operators.cpp** (17 uses)
  - [ ] Lines 218-219: append(), patch_to_here() → CFG
  - [ ] Lines 496-499: Four patch_to() → CFG
  - [ ] Line 503: patch_to() → edge.patch_to()
  - [ ] Line 523: append() → edge.append()
  - [ ] Line 531: append() → edge.append()
  - [ ] Line 542: patch_to() → edge.patch_to()
  - [ ] Lines 617-620: Four patch_to() → CFG
  - [ ] Line 633: patch_to() → edge.patch_to()
  - [ ] Lines 699-700: drop_values() → edge.drop_values()
  - [ ] Build and test
  - [ ] Commit

- [ ] **parse_regalloc.cpp** (11 uses)
  - [ ] Line 177: patch_with_value() → edge.patch_with_value()
  - [ ] Line 350: append() → edge.append()
  - [ ] Line 353: produces_values() → edge.produces_values()
  - [ ] Line 358: patch_to_here() → edge.patch_here()
  - [ ] Lines 362-363: patch_with_value() → edge.patch_with_value()
  - [ ] Line 493: append() → edge.append()
  - [ ] Lines 543-544: append(), patch_to_here() → CFG
  - [ ] Lines 561-562: append(), patch_to_here() → CFG
  - [ ] Build and test
  - [ ] Commit

### Phase 4: Hard Files (Week 4-5)
- [ ] **parse_stmt.cpp** Sub-batch 1: Compound assignment (5 uses)
  - [ ] Lines 150-154: Five patch_to() → CFG
  - [ ] Build and test
  - [ ] Commit

- [ ] **parse_stmt.cpp** Sub-batch 2: If/elseif/else (6 uses)
  - [ ] Line 953: append() → edge.append()
  - [ ] Line 954: patch_to_here() → edge.patch_here()
  - [ ] Line 959: append() → edge.append()
  - [ ] Line 960: patch_to_here() → edge.patch_here()
  - [ ] Line 964: append() → edge.append()
  - [ ] Line 966: patch_to_here() → edge.patch_here()
  - [ ] Build and test
  - [ ] Commit

- [ ] **parse_stmt.cpp** Sub-batch 3: While/repeat loops (6 uses)
  - [ ] Line 717: patch_to() → edge.patch_to()
  - [ ] Lines 722-723: patch_to_here(), patch_head() → CFG
  - [ ] Lines 752, 756-757: Three patches → CFG
  - [ ] Build and test
  - [ ] Commit

- [ ] **parse_stmt.cpp** Sub-batch 4: For loops (4 uses)
  - [ ] Lines 809-810: Two patch_head() → CFG
  - [ ] Lines 901, 906: Two patch_head() → CFG
  - [ ] Build and test
  - [ ] Commit

### Phase 5: Cleanup (Week 5)
- [ ] Mark JumpListView class as deprecated (parse_internal.h)
- [ ] Add deprecation notice to parse_constants.cpp
- [ ] Verify zero JumpListView uses in parser code
- [ ] Remove JumpListView class (parse_constants.cpp, parse_internal.h)
- [ ] Clean up includes
- [ ] Full rebuild and test
- [ ] Commit cleanup

## Conclusion

This analysis provides a comprehensive roadmap for migrating all 63 JumpListView usage sites to ControlFlowGraph. The migration is structured in 5 phases over 5 weeks, progressing from easy to hard files with clear success criteria at each step.

**Key Takeaways**:
1. 3 new ControlFlowEdge methods required before migration can begin
2. 63 real usage sites across 7 files (parse_constants.cpp is implementation, not usage)
3. Clear difficulty categorization guides migration sequencing
4. Incremental approach with testing after each file minimizes risk

**Next Action**: Implement missing ControlFlowEdge methods (Phase 1: Infrastructure)

---

**Document Status**: Complete - Ready for Phase 1 implementation
