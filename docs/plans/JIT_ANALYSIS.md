# Fluid parser JIT analysis (19 Nov 2025)

## 1. Closure/upvalue binding not propagated into nested functions (critical)
*Tests: fluid_catch, fluid_defer, fluid_debuglog, fluid_io, fluid_processing, fluid_compound, fluid_stress, fluid_unit_tests*

### Symptoms
- Catch handlers, defer blocks, IO callbacks, thread closures and helper lambdas all reference locals defined in their enclosing scopes, yet every failure log shows the same pattern: the nested function sees the captured local as `nil`, so table mutations raise `bad argument #1 to 'insert' (table expected, got nil)` and guard flags like `exception_raised` never flip, causing the assertions to trip.【F:build/agents/Testing/Temporary/LastTest.log†L10-L69】【F:build/agents/Testing/Temporary/LastTest.log†L702-L736】【F:build/agents/Testing/Temporary/LastTest.log†L769-L778】【F:build/agents/Testing/Temporary/LastTest.log†L369-L383】
- Top-level locals that should be visible to later chunk statements (e.g. `subjectReturn`, `signal_a`, `fluid = mod.load('fluid')`) are also treated as globals from within other functions, producing errors such as `attempt to call global 'subjectReturn' (a nil value)` and `attempt to index global 'signal_a' (a nil value)`.【F:build/agents/Testing/Temporary/LastTest.log†L37-L44】【F:build/agents/Testing/Temporary/LastTest.log†L376-L383】

### Code hotspot
- `IrEmitter::emit_function_expr` constructs a brand new `ParserContext` and `IrEmitter` every time we compile a function literal (lines 1817-1861). The freshly created emitter owns an empty `LocalBindingTable`, so it never sees bindings that were registered in the parent scope, and thus it cannot resolve upvalues or chunk-level locals when `emit_identifier_expr` runs later.【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1817-L1861】【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1335-L1355】
- Only formal parameters are registered in the child emitter (`child_emitter.update_local_binding(...)`), so closures lose visibility into every other local or top-level symbol.

### Impacted behaviour
- Exception filters (`catch`), `defer`, IO iterators, processing callbacks and embedded tests all rely on closures. Without upvalue resolution they either leave guard variables untouched (failures claiming “Failed to catch the raised exception”) or crash when captured tables are `nil`.
- The same bug explains `fluid_processing` (thread callback cannot reach `signal_a/b`), `fluid_io` (helper functions can’t reach `result` tables), `fluid_compound` (fallback closures can’t see `calls`) and `fluid_unit_tests` (chunk-level `fluid` handle is `nil` inside `testUnitTests`).

### Required follow-up
- Seed each nested `IrEmitter` with the parent emitter’s binding table (or a snapshot) before compiling the body so identifier lookups can classify locals vs upvalues.
- When closing over a symbol, emit the right `ExpKind::Upval` by walking the binding table stack instead of defaulting to globals. This will eliminate the wave of `nil` captures across the suite.

## 2. Extended falsey branching is inverted for ternary, presence and `?=` (high)
*Tests: fluid_presence, fluid_if_empty, fluid_ternary*

### Symptoms
- `value?` and ternary expressions return `false`/`nil` for truthy inputs and `true` for falsey ones. The presence tests complain that “Truthy string must return true” and “Should return true, not the value”, while bytecode-level tests warn that every BC_ISEQP/ISEQN check is firing on the wrong values.【F:build/agents/Testing/Temporary/LastTest.log†L835-L879】
- `count ?= expr` executes its RHS even when `count` already holds a truthy value, and the RHS closure cannot see `count` (due to issue #1), producing “attempt to perform arithmetic on global 'count' (a nil value)” and similar crashes.【F:build/agents/Testing/Temporary/LastTest.log†L769-L778】
- Ternary runtime cases fail with “attempt to compare string with number” and return `false` instead of branch results because the wrong branch executes for truthy conditions.【F:build/agents/Testing/Temporary/LastTest.log†L983-L1031】

### Code hotspot
- Both `LexState::assign_if_empty` and `IrEmitter::emit_ternary_expr` emit `BC_ISEQP/ISEQN/ISEQS` followed by a `JMP`, then patch those jumps to the false branch (lines 109-141 in `parse_stmt.cpp` and 1489-1513 in `ir_emitter.cpp`). However the LuaJIT “IS” opcodes *skip* the following instruction when the comparison succeeds. That means falsey values skip the jump (and fall through to the true path) while truthy values take the jump into the false branch, exactly the opposite of what `?`, `?:>`, and `?=` require.【F:src/fluid/luajit-2.1/src/parser/parse_stmt.cpp†L109-L141】【F:src/fluid/luajit-2.1/src/parser/ir_emitter.cpp†L1489-L1513】
- `bcemit_presence_check` repeats the same pattern, so the standalone postfix `value?` operator also reports inverted truthiness.【F:src/fluid/luajit-2.1/src/parser/parse_operators.cpp†L316-L401】

### Required follow-up
- Use the correct branch pattern: emit `BC_ISNEP/BC_ISNEN/BC_ISNES` (skip when unequal) or invert the patching so equality drives the false branch. After fixing the branching, rerun the presence, ternary and compound assignment suites to verify that truthy inputs stop tripping the “false” path and that RHS expressions short-circuit again.

## 3. Bitwise/shift precedence and chaining regressions (medium)
*Tests: fluid_bitshift, fluid_bitwise*

### Symptoms
- `a << b << c` evaluates as though only the first shift ran, and `1 << 2 & 3` groups left-to-right instead of respecting C-style precedence. The bitwise regression suite reports failures such as “single << should match nested bit.lshift calls, got 0x10, expected 0x20” and “AND should bind tighter than OR: 1 | 2 & 3 should be 1 | (2 & 3)”.【F:build/agents/Testing/Temporary/LastTest.log†L666-L736】
- Complex precedence cases (`8 | 4 ~ 2 & 1`, `1 << 2 & 3`) fail because the parser treats all bitwise tokens as the same precedence and chains them strictly left-to-right.【F:build/agents/Testing/Temporary/LastTest.log†L720-L732】

### Code hotspot
- Inside `LexState::expr_binop` the parser forcibly zeroes the left-priority (`lpri = 0`) whenever the recursion limit equals the operator’s right priority for any shift/bitwise operator (lines 930-934). That shortcut breaks the precedence table from `lj_parse.cpp` by allowing lower-precedence operators (e.g. `|`) to consume operands that should belong to higher-precedence ones (e.g. `&`).【F:src/fluid/luajit-2.1/src/parser/parse_expr.cpp†L930-L944】【F:src/fluid/luajit-2.1/src/parser/lj_parse.cpp†L31-L40】
- The custom `expr_shift_chain` then reserves call slots blindly even when the “base” register is still in use, so chained shifts reuse stale operands and emit `bit.lshift`/`bit.rshift` calls with truncated arguments when the base register is not actually at the top of the stack (lines 803-864).【F:src/fluid/luajit-2.1/src/parser/parse_expr.cpp†L803-L864】

### Required follow-up
- Honour the precedence table by removing the unconditional `lpri = 0` hack and instead limit chaining to operators that truly share the same left priority. This will let `&` bind tighter than `|` and restore the documented C-style rules.
- Revisit `expr_shift_chain`’s register-allocation strategy so that the base register chosen for chaining is guaranteed to be at the current top of stack before reserving the callee/argument slots; otherwise chained shifts will continue to reuse stale arguments, which is what the bitshift test detects.
