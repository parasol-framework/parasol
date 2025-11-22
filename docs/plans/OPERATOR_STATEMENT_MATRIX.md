# Operator and Statement Capability Matrix

This document catalogues all AST operator and statement kinds exercised in `IrEmitter`, identifies places that still call legacy helpers, and defines expected value-category inputs/outputs for Phase 4 modernisation.

**Generated:** 2025-11-22
**Branch:** test/jit-phase4
**Status:** Step 1 of Phase 4 implementation complete

---

## Executive Summary

The AST pipeline in `IrEmitter` currently handles all major Fluid language constructs but relies heavily on legacy helper functions from `parse_operators.cpp` and `parse_stmt.cpp`. This matrix serves as a roadmap for Phase 4 refactoring, which aims to eliminate these dependencies and modernise operator/statement emission through:

1. Value-category-aware operator emission
2. ControlFlowGraph-based jump management
3. RegisterAllocator-managed temporaries
4. Elimination of direct `freereg` manipulation

**Legacy Helper Instrumentation:** All legacy helper calls are now tracked via `glLegacyHelperCalls` recorder (see `ir_emitter.cpp:138-180`). This allows monitoring refactor progress and identifying remaining gaps.

---

## Expression Operators

### Unary Operators

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| Negate (-) | `UnaryExpr` | `bcemit_unop()` | `ir_emitter.cpp:1369` | **Input:** Any value → **Output:** Relocatable numeric temp |
| Not (not) | `UnaryExpr` | `bcemit_unop()` | `ir_emitter.cpp:1372` | **Input:** Any value → **Output:** Relocatable boolean temp |
| Length (#) | `UnaryExpr` | `bcemit_unop()` | `ir_emitter.cpp:1375` | **Input:** Table/string value → **Output:** Relocatable numeric temp |
| BitNot (~) | `UnaryExpr` | `bcemit_unary_bit_call()` | `ir_emitter.cpp:1378` | **Input:** Numeric value → **Output:** Call result |

**Current Implementation:**
- All unary operators call legacy `bcemit_unop()` or `bcemit_unary_bit_call()`
- Direct `ExpDesc` manipulation for operand discharge
- No RegisterAllocator integration for temporary management

**Target Implementation:**
- Create `OperatorEmitter::emit_unary()` accepting `ExpressionValue`/`ValueSlot` wrappers
- Use `RegisterAllocator::allocate_temp()` for result storage
- Constant folding for numeric literals (already exists in `lj_parse.cpp:foldarith()` but not unified)

**Tracking:** `LegacyHelperKind::Unop` in `emit_unary_expr`

---

### Binary Operators

#### Arithmetic Operators

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| Add (+) | `BinaryExpr` | `bcemit_binop_left()`, `bcemit_binop()` → `bcemit_arith()` | `ir_emitter.cpp:1446-1452` | **Left:** Numeric value → **Right:** Numeric value → **Output:** Relocatable numeric temp |
| Subtract (-) | `BinaryExpr` | Same as Add | Same | Same |
| Multiply (*) | `BinaryExpr` | Same as Add | Same | Same |
| Divide (/) | `BinaryExpr` | Same as Add | Same | Same |
| Modulo (%) | `BinaryExpr` | Same as Add | Same | Same |
| Power (^) | `BinaryExpr` | Same as Add | Same | **Note:** Right-associative, special register ordering |
| Concat (..) | `BinaryExpr` | Same as Add | Same | **Left:** String/numeric → **Right:** String/numeric → **Output:** Relocatable string |

**Current Implementation:**
- `bcemit_binop_left()` handles short-circuit setup and left operand discharge (see `parse_operators.cpp:168-225`)
- `bcemit_binop()` dispatches to `bcemit_arith()` for arithmetic ops (see `parse_operators.cpp:34-88`)
- Direct constant folding via `foldarith()` for compile-time evaluation
- Manual register allocation and `freereg` manipulation
- Special handling for POW operator (right operand discharged first)

**Target Implementation:**
- `OperatorEmitter::emit_binary_arith(lhs: ValueUse, rhs: ValueUse) -> ValueSlot`
- Unified constant folding through `ConstantFolder` utility
- RegisterAllocator manages all temporaries via RAII `RegisterReservation`
- No direct `freereg` writes

**Tracking:** `LegacyHelperKind::BinopLeft` and `LegacyHelperKind::Binop` in `emit_binary_expr`

---

#### Comparison Operators

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| Equal (is) | `BinaryExpr` | `bcemit_binop_left()`, `bcemit_binop()` → `bcemit_comp()` | `ir_emitter.cpp:1446-1452` | **Left:** Any value → **Right:** Any value → **Output:** Jump (true/false edges) |
| NotEqual (!=) | `BinaryExpr` | Same | Same | Same |
| LessThan (<) | `BinaryExpr` | Same | Same | **Note:** Numeric/string comparison only |
| LessEqual (<=) | `BinaryExpr` | Same | Same | Same |
| GreaterThan (>) | `BinaryExpr` | Same | Same | **Note:** Swaps to LT in bytecode |
| GreaterEqual (>=) | `BinaryExpr` | Same | Same | **Note:** Swaps to LE in bytecode |

**Current Implementation:**
- `bcemit_comp()` generates comparison bytecode with jump (see `parse_operators.cpp:93-163`)
- Result is `ExpKind::Jmp` requiring manual patch via `jmp_tohere()` / `jmp_append()`
- Optimised constants (ISEQP for nil/bool, ISEQS for strings, ISEQN for numbers)
- GT/GE swap operands to reuse LT/LE bytecode

**Target Implementation:**
- `OperatorEmitter::emit_comparison(op, lhs, rhs) -> ControlFlowEdge`
- Returns structured CFG edges (true/false branches) instead of raw jump
- ControlFlowGraph manages edge resolution and patching
- Preserve constant optimisations through `ValueUse` abstraction

**Tracking:** `LegacyHelperKind::BinopLeft` and `LegacyHelperKind::Binop` in `emit_binary_expr`

---

#### Bitwise Operators

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| BitAnd (&) | `BinaryExpr` | `bcemit_binop_left()`, `bcemit_binop()` | `ir_emitter.cpp:1446-1452` | **Left:** Integer → **Right:** Integer → **Output:** Relocatable integer |
| BitOr (\|) | `BinaryExpr` | Same | Same | Same |
| BitXor (^) | `BinaryExpr` | Same | Same | Same |
| ShiftLeft (<<) | `BinaryExpr` | Same | Same | Same |
| ShiftRight (>>) | `BinaryExpr` | Same | Same | Same |

**Current Implementation:**
- Treated as arithmetic operators in `bcemit_arith()` path
- Same register allocation and folding logic

**Target Implementation:**
- Can reuse arithmetic emission logic
- Ensure integer coercion in ValueUse layer

**Tracking:** Same as arithmetic operators

---

#### Logical Operators

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| LogicalAnd (and) | `BinaryExpr` | `bcemit_binop_left()` → `bcemit_branch_t()`, `bcemit_binop()` | `ir_emitter.cpp:1446-1452` | **Left:** Any value → **Right:** Any value (short-circuit) → **Output:** Value or jump |
| LogicalOr (or) | `BinaryExpr` | `bcemit_binop_left()` → `bcemit_branch_f()`, `bcemit_binop()` | Same | Same |
| IfEmpty (??) | `BinaryExpr` | `bcemit_binop_left()` (extended falsey), `bcemit_binop()` | Same | **Left:** Any value (falsey check) → **Right:** Any value (short-circuit) → **Output:** Value |

**Current Implementation:**
- `bcemit_binop_left()` sets up short-circuit jumps (see `parse_operators.cpp:172-225`)
- For AND: branches if left is true (skip right), evaluates right otherwise
- For OR: branches if left is false (skip right), evaluates right otherwise
- For IfEmpty (??): extended falsey check (nil, false, 0, "") with special constant handling
- Manual jump list management via `jmp_append()`

**Target Implementation:**
- `OperatorEmitter::emit_logical_short_circuit(op, lhs, rhs) -> ControlFlowEdge`
- CFG records true/false edges explicitly
- IfEmpty falsey check factored into separate `ValueUse::is_falsey()` predicate
- No manual jump patching

**Tracking:** `LegacyHelperKind::BinopLeft` and `LegacyHelperKind::Binop` in `emit_binary_expr`

---

### Update Operators (Postfix/Prefix)

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| Increment (++) | `UpdateExpr` | `bcemit_binop_left()`, `bcemit_binop()`, `bcemit_store()` | `ir_emitter.cpp:1418-1424` | **Target:** L-value (local/upvalue/indexed) → **Output:** Numeric value (pre: new value, post: old value) |
| Decrement (--) | `UpdateExpr` | Same | Same | Same |

**Current Implementation:**
- Duplicates table operands if target is indexed (via `RegisterAllocator::duplicate_table_operands()`)
- Reads target into working register
- For postfix: saves old value to separate register
- Performs `target = target +/- 1` via `bcemit_binop_left()` and `bcemit_binop()`
- Stores result back to target via `bcemit_store()`
- Returns saved register for postfix, new value for prefix

**Target Implementation:**
- `OperatorEmitter::emit_update(op, target: LValue, is_postfix) -> ValueSlot`
- Introduce `LValue` descriptor struct for locals/upvalues/indexed/member
- Reuse binary operator emission for `+/-` operation
- RAII-managed temporary for postfix save
- Centralised hazard detection for table operand duplication

**Tracking:** `LegacyHelperKind::BinopLeft`, `LegacyHelperKind::Binop`, `LegacyHelperKind::Store` in `emit_update_expr`

---

### Ternary Operator

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| Ternary (cond ? true :> false) | `TernaryExpr` | Manual CFG + jump patching | `ir_emitter.cpp:1458-1513` | **Condition:** Any value (extended falsey) → **True/False:** Any value → **Output:** Unified value in single register |

**Current Implementation:**
- Discharges condition to register
- Emits 4 comparison checks (nil, false, 0, empty string) with jumps to false branch
- Evaluates true branch, materialises to condition register
- Emits jump to skip false branch
- Patches all false-branch jumps to false branch start
- Evaluates false branch, materialises to same condition register
- Patches skip jump to merge point
- Uses `ControlFlowEdge` for structured edge tracking (partial modernisation)

**Target Implementation:**
- `OperatorEmitter::emit_ternary(cond, if_true, if_false) -> ValueSlot`
- Factor extended falsey check into reusable `emit_extended_falsey_check() -> ControlFlowEdge`
- CFG manages all edge resolution automatically
- Introduce `ValueSlot::merge(other)` to handle phi-like register unification

**Tracking:** No legacy helpers (already uses CFG), but manual jump patching tracked as `LegacyHelperKind::ManualJump`

---

### Presence Operator

| Operator | AST Kind | Legacy Helpers | Location | Value Categories |
|----------|----------|----------------|----------|------------------|
| Presence (??) | `PresenceExpr` | `bcemit_presence_check()` | `ir_emitter.cpp:1529` | **Input:** Any value → **Output:** Boolean value (true if not nil/false/0/"") |

**Current Implementation:**
- `bcemit_presence_check()` generates extended falsey check with boolean result
- Similar to ternary condition logic but returns boolean instead of jump

**Target Implementation:**
- `OperatorEmitter::emit_presence_check(value) -> ValueSlot<boolean>`
- Reuse extended falsey check logic from ternary
- Return structured boolean result

**Tracking:** `LegacyHelperKind::PresenceCheck` in `emit_presence_expr`

---

## Statement Forms

### Assignment Statements

#### Plain Assignment

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| Plain assignment (a = b) | `AssignmentStmt` | `bcemit_store()`, `assign_adjust()` | `ir_emitter.cpp:1066-1119` | **Targets:** L-value list → **Values:** Expression list → **Output:** Void |

**Current Implementation:**
- Evaluates all value expressions to stack
- If value count matches target count exactly: stores last value to last target, then pops stack for remaining
- If counts differ: calls `assign_adjust()` to pad with nil or truncate multi-return values
- `bcemit_store()` handles locals/upvalues/indexed/global stores
- Manual stack manipulation via `assign_from_stack` lambda

**Target Implementation:**
- `StatementEmitter::emit_assignment(targets: LValue[], values: ExprNode[]) -> IrEmitUnit`
- Introduce `LValue` descriptor for all assignment targets
- Replace `assign_adjust()` with structured multi-value handling in RegisterAllocator
- RAII stack frame management via `StackFrame` helper
- No direct `freereg` manipulation

**Tracking:**
- `LegacyHelperKind::Store` in `emit_plain_assignment/exact`, `/stack`
- `LegacyHelperKind::AssignAdjust` in `emit_plain_assignment/adjust`

---

#### Compound Assignment

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| Add assign (+=) | `AssignmentStmt` | `bcemit_binop_left()`, `bcemit_binop()`, `bcemit_store()` | `ir_emitter.cpp:1122-1192` | **Target:** Single L-value → **Value:** Single expression → **Output:** Void |
| Subtract (-=) | `AssignmentStmt` | Same | Same | Same |
| Multiply (*=) | `AssignmentStmt` | Same | Same | Same |
| Divide (/=) | `AssignmentStmt` | Same | Same | Same |
| Modulo (%=) | `AssignmentStmt` | Same | Same | Same |
| Concat (..=) | `AssignmentStmt` | Same (special path) | Same | **Note:** Concat has special left-associative path |

**Current Implementation:**
- Restricted to single target and single value
- Duplicates table operands if target is indexed
- For concat: uses `bcemit_binop_left()` on target, then `bcemit_binop()` with value
- For arithmetic: materialises target to register, uses binop emission, stores result
- Manual register guard management

**Target Implementation:**
- `StatementEmitter::emit_compound_assignment(op, target: LValue, value: ExprNode) -> IrEmitUnit`
- Reuse `OperatorEmitter::emit_binary_arith()` for operator logic
- LValue descriptor handles table operand duplication automatically
- RAII register guards via `RegisterAllocator::reserve_scope()`

**Tracking:**
- `LegacyHelperKind::BinopLeft`, `Binop`, `Store` in `emit_compound_assignment/concat`, `/arith`

---

#### If-Empty Assignment

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| If-empty assign (?=) | `AssignmentStmt` | Manual CFG + jump patching, `bcemit_store()` | `ir_emitter.cpp:1195-1249` | **Target:** Single L-value → **Value:** Single expression → **Output:** Void |

**Current Implementation:**
- Duplicates table operands
- Reads current target value to register
- Emits 4 falsey checks (nil, false, 0, empty string) with jumps to assignment
- Emits skip jump if all checks fail (target is truthy)
- Patches falsey jumps to assignment block
- Evaluates value expression and stores to target
- Patches skip jump to merge point
- Uses `ControlFlowEdge` for structured tracking

**Target Implementation:**
- `StatementEmitter::emit_if_empty_assignment(target: LValue, value: ExprNode) -> IrEmitUnit`
- Reuse extended falsey check logic (factor out from ternary/presence)
- CFG manages all edge resolution
- No manual jump patching

**Tracking:** No direct legacy helpers, but manual jump patching tracked as `LegacyHelperKind::ManualJump`

---

### Declaration Statements

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| Local declaration (local a, b = ...) | `LocalDeclStmt` | `assign_adjust()` | `ir_emitter.cpp:527-556` | **Names:** Identifier list → **Values:** Expression list → **Output:** Registers allocated at `nactvar` |
| Local function (local function f() end) | `LocalFunctionStmt` | None (direct emission) | `ir_emitter.cpp:561-586` | **Name:** Identifier → **Function:** Function literal → **Output:** Register for function |
| Function (function f() end) | `FunctionStmt` | `bcemit_store()` | `ir_emitter.cpp:589-617` | **Name:** Member/index chain → **Function:** Function literal → **Output:** Stored to l-value |

**Current Implementation:**
- Local decl uses `assign_adjust()` for value list handling (same as plain assignment)
- Registers allocated sequentially at `nactvar`
- Local bindings updated in `binding_table` for name resolution

**Target Implementation:**
- `StatementEmitter::emit_local_decl(names, values) -> IrEmitUnit`
- Replace `assign_adjust()` with structured multi-value handling
- LocalBindingTable integration remains unchanged (already modern)

**Tracking:** `LegacyHelperKind::AssignAdjust` in `emit_local_decl_stmt`

---

### Control Flow Statements

#### If Statement

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| If/elseif/else | `IfStmt` | Manual CFG + jump patching | `ir_emitter.cpp:645-714` | **Conditions:** Boolean values → **Blocks:** Statement lists → **Output:** Void |

**Current Implementation:**
- For each condition: evaluates, discharges to `Jmp`, emits jump to next condition on false
- Emits then-block, emits jump to end
- Patches false jumps to next condition/else block
- Uses `ControlFlowEdge` for structured tracking but still manual patching

**Target Implementation:**
- `StatementEmitter::emit_if(branches: IfBranch[]) -> IrEmitUnit`
- CFG manages all edge lists automatically
- IfBranch struct contains condition + block
- No manual jump patching

**Tracking:** Manual jump patching tracked as `LegacyHelperKind::ManualJump`

---

#### Loop Statements

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| While loop | `WhileStmt` | Manual CFG + jump patching | `ir_emitter.cpp:716-749` | **Condition:** Boolean value → **Block:** Statement list → **Output:** Void |
| Repeat-until loop | `RepeatStmt` | Manual CFG + jump patching | `ir_emitter.cpp:751-781` | Same | Same |
| Numeric for loop | `NumericForStmt` | Manual CFG + jump patching | `ir_emitter.cpp:783-881` | **Start/Stop/Step:** Numeric values → **Block:** Statement list → **Output:** Void |
| Generic for loop | `GenericForStmt` | Manual CFG + jump patching | `ir_emitter.cpp:883-968` | **Iterator/State/Control:** Call values → **Block:** Statement list → **Output:** Void |

**Current Implementation:**
- All loops use FuncScope with Loop flag for break/continue tracking
- Break/continue managed via `gola_new()` jump lists
- Condition/iterator evaluation uses standard expression emission
- Loop body wrapped in scope with bindings for loop variables
- Manual jump patching for loop back-edge and exit edges

**Target Implementation:**
- `StatementEmitter::emit_loop(kind, setup, condition, body) -> IrEmitUnit`
- CFG maintains loop entry/exit/continue edges in structured form
- LoopScope abstraction manages break/continue lists via CFG
- No manual `gola_new()` usage

**Tracking:** Manual jump patching tracked as `LegacyHelperKind::ManualJump`

---

#### Defer Statement

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| Defer block | `DeferStmt` | Manual defer list + CFG | `ir_emitter.cpp:620-643` | **Block:** Statement list → **Output:** Executed on scope exit |

**Current Implementation:**
- Uses FuncState defer chain (linked list of BCPos)
- Block emitted immediately at defer site, marked as deferred code
- Return statements execute all defers in chain
- Break/continue execute defers up to loop scope
- Manual depth tracking and chain traversal

**Target Implementation:**
- `StatementEmitter::emit_defer(block) -> IrEmitUnit`
- CFG maintains defer list per scope
- Structured exit point injection via CFG finalisation
- Defer execution still via runtime mechanism (defer chain)

**Tracking:** Manual defer chain tracked as `LegacyHelperKind::ManualJump`

---

#### Break/Continue/Return

| Statement | AST Kind | Legacy Helpers | Location | Value Categories |
|-----------|----------|----------------|----------|------------------|
| Break | `BreakStmt` | `gola_new()` for jump list | `ir_emitter.cpp:970-1001` | **Output:** Jump to loop exit |
| Continue | `ContinueStmt` | `gola_new()` for jump list | `ir_emitter.cpp:1003-1034` | **Output:** Jump to loop continue point |
| Return | `ReturnStmt` | Defer execution, manual jump | `ir_emitter.cpp:474-522` | **Values:** Expression list → **Output:** Function exit |

**Current Implementation:**
- Break/continue find enclosing loop scope, append jump to `gola` list
- Loop finalisation patches all breaks to loop exit, continues to loop start
- Return executes all defers via `execute_defers()`, emits return bytecode
- Manual jump list management via FuncScope

**Target Implementation:**
- `StatementEmitter::emit_break/continue/return() -> IrEmitUnit`
- CFG tracks break/continue edges per loop scope
- Defer execution remains unchanged (runtime mechanism)
- No manual `gola` usage

**Tracking:** Manual jump lists tracked as `LegacyHelperKind::ManualJump`

---

## Value Category Definitions

The target implementation will use these value category abstractions to replace raw `ExpDesc` manipulation:

### ValueUse (Read-Only)

Represents a value being read for consumption in an operation.

**Variants:**
- `ConstantValue`: Compile-time constant (nil, boolean, number, string, cdata)
- `RegisterValue`: Value already in a register (local, temp, upvalue slot)
- `IndexedValue`: Table slot (requires table and key registers)
- `GlobalValue`: Global variable (requires name constant)

**Operations:**
- `discharge_to_reg(allocator) -> BCReg`: Force value into a register
- `to_constant() -> std::optional<TValue>`: Extract constant if available
- `is_falsey() -> bool`: Extended falsey check (nil/false/0/"")

### ValueSlot (Write Target)

Represents a destination for storing a computed value.

**Variants:**
- `LocalSlot`: Local variable register
- `TempSlot`: Temporary register (RAII-released)
- `UpvalueSlot`: Upvalue index
- `IndexedSlot`: Table slot with table+key registers
- `GlobalSlot`: Global variable name constant

**Operations:**
- `assign(value: ValueUse, allocator)`: Store value to slot
- `merge(other: ValueSlot) -> ValueSlot`: Unify two slots (for ternary/if-else)

### LValue (Assignment Target)

Represents an assignable location for statements.

**Variants:**
- `LocalLValue`: Local variable
- `UpvalueLValue`: Upvalue
- `IndexedLValue`: Table slot (table expression + key expression)
- `MemberLValue`: Table member (table expression + constant key)
- `GlobalLValue`: Global variable

**Operations:**
- `read(allocator) -> ValueUse`: Load current value
- `write(value: ValueUse, allocator)`: Store new value
- `duplicate_hazards(allocator) -> LValue`: Duplicate table/key regs if needed (for +=, ++, etc.)

---

## Legacy Helper Call Tracking

The `LegacyHelperRecorder` (`ir_emitter.cpp:138-180`) tracks all calls to legacy helpers. Statistics can be dumped via `glLegacyHelperCalls.dump_statistics()`.

**Tracked Helper Kinds:**
1. `BinopLeft`: `bcemit_binop_left()` - Left operand setup for binary operators
2. `Binop`: `bcemit_binop()` - Binary operator emission (arith/comp)
3. `Unop`: `bcemit_unop()` - Unary operator emission
4. `PresenceCheck`: `bcemit_presence_check()` - Presence operator (??)
5. `Store`: `bcemit_store()` - Assignment to l-values
6. `AssignAdjust`: `assign_adjust()` - Multi-value list adjustment
7. `BranchTrue`: `bcemit_branch_t()` - Short-circuit true branch
8. `BranchFalse`: `bcemit_branch_f()` - Short-circuit false branch
9. `ManualJump`: Manual jump patching (ternary, if, loops, defer)

**Instrumentation Points:**
- `emit_unary_expr`: Records `Unop` before operator dispatch
- `emit_binary_expr`: Records `BinopLeft` and `Binop` for each binary operation
- `emit_update_expr`: Records `BinopLeft`, `Binop`, and `Store` for increment/decrement
- `emit_presence_expr`: Records `PresenceCheck`
- `emit_plain_assignment`: Records `Store` (multiple contexts) and `AssignAdjust`
- `emit_compound_assignment`: Records `BinopLeft`, `Binop`, `Store` (separate paths for concat vs arith)

---

## Next Steps for Phase 4

Based on this matrix, the following refactoring steps should be executed in order:

### Step 2: Extract OperatorEmitter Facade
- Create `OperatorEmitter` class owned by `IrEmissionContext`
- Migrate `foldarith()` constant folding to `OperatorEmitter::fold_constant()`
- Implement `emit_unary(op, operand: ValueUse) -> ValueSlot`
- Implement `emit_binary_arith(op, lhs: ValueUse, rhs: ValueUse) -> ValueSlot`
- Implement `emit_comparison(op, lhs: ValueUse, rhs: ValueUse) -> ControlFlowEdge`
- Implement `emit_logical_short_circuit(op, lhs: ValueUse, rhs: ValueUse) -> ControlFlowEdge`

### Step 3: Rework Binary/Unary/Ternary Emission
- Define `ValueUse`, `ValueSlot`, and `LValue` structs
- Adapt `emit_unary_expr()` to use `OperatorEmitter::emit_unary()`
- Adapt `emit_binary_expr()` to use `OperatorEmitter` methods
- Adapt `emit_ternary_expr()` to use CFG-based edge management (no manual patching)
- Adapt `emit_presence_expr()` to use shared falsey check logic

### Step 4: Modernise Statement Emission
- Introduce `LValue` descriptor for assignment targets
- Replace `assign_adjust()` with structured multi-value handling in RegisterAllocator
- Implement `StatementEmitter::emit_assignment()` using LValue and OperatorEmitter
- Implement `StatementEmitter::emit_compound_assignment()` using LValue and OperatorEmitter
- Implement `StatementEmitter::emit_if_empty_assignment()` using CFG edges

### Step 5: Retire Legacy Helpers
- Move `parse_operators.cpp` and `parse_stmt.cpp` behind `#ifdef LEGACY_PARSER` guard
- Ensure legacy parser path still compiles for `--ast-legacy` mode
- Remove redundant `ExpDesc` plumbing from `IrEmitter` methods
- Update `IrEmitter` to use new APIs exclusively

### Step 6: Add Targeted Tests
- Expand `tests/parser/test_operators.fluid` to cover all operator forms
- Add bytecode pattern checks for short-circuit behaviour
- Add tests for compound assignments on indexed targets (table operand duplication)
- Add tests for ternary operator with all falsey value types
- Add tests for `??` operator and `?=` assignment

### Step 7: Migration Validation
- Run full Fluid test suite with AST pipeline
- Compare bytecode dumps between legacy and modern paths
- Verify `glLegacyHelperCalls` counters drop to zero for modernised paths
- Update `LUAJIT_PARSER_REDESIGN.md` with extension guidelines

---

## References

**Source Files:**
- `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp` - Main IR emission implementation
- `src/fluid/luajit-2.1/src/parser/ir_emitter.h` - IR emission API
- `src/fluid/luajit-2.1/src/parser/parse_operators.cpp` - Legacy operator helpers
- `src/fluid/luajit-2.1/src/parser/parse_stmt.cpp` - Legacy statement helpers
- `src/fluid/luajit-2.1/src/parser/ast_nodes.h` - AST node definitions
- `src/fluid/luajit-2.1/src/parser/parse_value.h` - ExpressionValue/RegisterAllocator

**Plan Documents:**
- `docs/plans/PARSER_P4.md` - Phase 4 implementation plan
- `docs/plans/PARSER_P2.md` - Phase 2 AST pipeline (background)
- `docs/plans/LUAJIT_PARSER_REDESIGN.md` - Overall parser redesign strategy

**Legacy Helper Locations:**
- `bcemit_binop_left()`: `parse_operators.cpp:168-225`
- `bcemit_binop()`: Dispatches to `bcemit_arith()` (L34-88) or `bcemit_comp()` (L93-163)
- `bcemit_unop()`: `lj_parse.cpp` (legacy parser only)
- `bcemit_presence_check()`: `parse_operators.cpp` (legacy parser only)
- `bcemit_store()`: `parse_stmt.cpp` (legacy parser only)
- `assign_adjust()`: `lj_lex.cpp` (shared between legacy and AST)

---

**Document Status:** ✅ Complete
**Implementation Status:** 🟡 Step 1 complete (tracking infrastructure), ready for Step 2
