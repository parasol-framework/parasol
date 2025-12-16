# Bytecode Semantics Reference

## 1. Introduction and Scope
Parasol integrates a heavily modified LuaJIT 2.1 VM. This note captures the control-flow semantics of its bytecode so parser and emitter changes do not regress short-circuiting or extended-falsey behaviour. It answers questions such as "when does `BC_ISEQP` skip the next instruction?" and "how do `??` and `?` wire their jumps?" and is aimed at maintainers working on `IrEmitter`/`OperatorEmitter` or debugging logical and ternary operators.

## 2. Notation, Conventions, and Versioning
- Registers are shown as `R0`, `R1`, etc. Fields A/B/C/D follow LuaJIT encoding: `A` is usually a destination or base, `B`/`C` are sources, `D` is a constant or split field. `base` is the current stack frame start.
- Conditions are expressed as "condition true → skip next instruction; condition false → execute next instruction (normally a `JMP`)." "Next instruction" means the sequential `BCIns`; a taken `JMP` applies its offset from the following instruction.
- Version: LuaJIT 2.1 with extensive changes, assuming the `LJ_FR2` two-slot frame layout used by all supported platforms.
- Keep this file aligned with changes in `src/fluid/luajit-2.1/src/parser/*`, whenever bytecode emission patterns change.

## 3. Bytecode Overview
### 3.1 High-Level Structure
- Each instruction is a 32-bit `BCIns` packed with opcode and A/B/C/D fields. Prototypes (`GCproto`) hold the instruction stream, constants, and line table.
- Format ABC: `[B:8][C:8][A:8][OP:8]`, Format AD: `[D:16][A:8][OP:8]`

### 3.2 Complete Bytecode Instruction Matrix

Operand suffixes: V=variable slot, S=string const, N=number const, P=primitive (~itype), B=byte literal, M=multiple args/results.

#### Comparison Ops (condition true → skip next; false → execute next)
| Opcode | Format | Description |
|--------|--------|-------------|
| `ISLT` | A D | R(A) < R(D) |
| `ISGE` | A D | R(A) >= R(D) |
| `ISLE` | A D | R(A) <= R(D) |
| `ISGT` | A D | R(A) > R(D) |
| `ISEQV` | A D | R(A) == R(D) |
| `ISNEV` | A D | R(A) != R(D) |
| `ISEQS` | A D | R(A) == str(D) |
| `ISNES` | A D | R(A) != str(D) |
| `ISEQN` | A D | R(A) == num(D) |
| `ISNEN` | A D | R(A) != num(D) |
| `ISEQP` | A D | R(A) == pri(D) (nil/false/true) |
| `ISNEP` | A D | R(A) != pri(D) |

#### Unary Test and Copy Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `ISTC` | A D | Copy R(D) to R(A) if truthy, else skip next |
| `ISFC` | A D | Copy R(D) to R(A) if falsey, else skip next |
| `IST` | D | Skip next if R(D) is truthy |
| `ISF` | D | Skip next if R(D) is falsey |
| `ISTYPE` | A D | Assert R(A) is type D (debug) |
| `ISNUM` | A D | Assert R(A) is number (debug) |

#### Unary Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `MOV` | A D | R(A) = R(D) |
| `NOT` | A D | R(A) = not R(D) |
| `UNM` | A D | R(A) = -R(D) |
| `LEN` | A D | R(A) = #R(D) |

#### Binary Arithmetic Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `ADDVN` | A B C | R(A) = R(B) + num(C) |
| `SUBVN` | A B C | R(A) = R(B) - num(C) |
| `MULVN` | A B C | R(A) = R(B) * num(C) |
| `DIVVN` | A B C | R(A) = R(B) / num(C) |
| `MODVN` | A B C | R(A) = R(B) % num(C) |
| `ADDNV` | A B C | R(A) = num(C) + R(B) |
| `SUBNV` | A B C | R(A) = num(C) - R(B) |
| `MULNV` | A B C | R(A) = num(C) * R(B) |
| `DIVNV` | A B C | R(A) = num(C) / R(B) |
| `MODNV` | A B C | R(A) = num(C) % R(B) |
| `ADDVV` | A B C | R(A) = R(B) + R(C) |
| `SUBVV` | A B C | R(A) = R(B) - R(C) |
| `MULVV` | A B C | R(A) = R(B) * R(C) |
| `DIVVV` | A B C | R(A) = R(B) / R(C) |
| `MODVV` | A B C | R(A) = R(B) % R(C) |
| `POW` | A B C | R(A) = R(B) ^ R(C) |
| `CAT` | A B C | R(A) = R(B) .. ... .. R(C) (concatenate range) |

#### Constant Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `KSTR` | A D | R(A) = str(D) |
| `KCDATA` | A D | R(A) = cdata(D) (FFI) |
| `KSHORT` | A D | R(A) = signed 16-bit D |
| `KNUM` | A D | R(A) = num(D) |
| `KPRI` | A D | R(A) = pri(D) (nil/false/true) |
| `KNIL` | A D | R(A) ... R(D) = nil (range) |

#### Upvalue and Function Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `UGET` | A D | R(A) = upvalue(D) |
| `USETV` | A D | upvalue(A) = R(D) |
| `USETS` | A D | upvalue(A) = str(D) |
| `USETN` | A D | upvalue(A) = num(D) |
| `USETP` | A D | upvalue(A) = pri(D) |
| `UCLO` | A D | Close upvalues for R >= A; JMP to D |
| `FNEW` | A D | R(A) = new closure from proto(D) |

#### Table Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `TNEW` | A D | R(A) = new table (D encodes array/hash sizes) |
| `TDUP` | A D | R(A) = copy of template table(D) |
| `GGET` | A D | R(A) = _G[str(D)] |
| `GSET` | A D | _G[str(D)] = R(A) |
| `TGETV` | A B C | R(A) = R(B)[R(C)] |
| `TGETS` | A B C | R(A) = R(B)[str(C)] |
| `TGETB` | A B C | R(A) = R(B)[C] (byte index) |
| `TGETR` | A B C | R(A) = R(B)[R(C)] (raw, no metamethod) |
| `TSETV` | A B C | R(B)[R(C)] = R(A) |
| `TSETS` | A B C | R(B)[str(C)] = R(A) |
| `TSETB` | A B C | R(B)[C] = R(A) (byte index) |
| `TSETM` | A D | table[MULTRES...] = R(A)... (multi-set) |
| `TSETR` | A B C | R(B)[R(C)] = R(A) (raw, no metamethod) |

#### Array Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `AGETV` | A B C | R(A) = R(B)[R(C)] (native array get by variable) |
| `AGETB` | A B C | R(A) = R(B)[C] (native array get by byte literal) |
| `ASETV` | A B C | R(B)[R(C)] = R(A) (native array set by variable) |
| `ASETB` | A B C | R(B)[C] = R(A) (native array set by byte literal) |

#### Call and Vararg Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `CALLM` | A B C | R(A)...R(A+B-2) = R(A)(R(A+1)...R(A+C+MULTRES)) |
| `CALL` | A B C | R(A)...R(A+B-2) = R(A)(R(A+1)...R(A+C-1)) |
| `CALLMT` | A D | return R(A)(R(A+1)...R(A+D+MULTRES)) (tail) |
| `CALLT` | A D | return R(A)(R(A+1)...R(A+D-1)) (tail) |
| `ITERC` | A B C | Call iterator: R(A)...R(A+B-2) = R(A-3)(R(A-2), R(A-1)) |
| `ITERN` | A B C | Specialised next() iterator call |
| `VARG` | A B C | R(A)...R(A+B-2) = vararg (C=base offset) |
| `ISNEXT` | A D | Verify iterator is next(); JMP to D if not |

#### Return Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `RETM` | A D | return R(A)...R(A+D+MULTRES-1) |
| `RET` | A D | return R(A)...R(A+D-2) |
| `RET0` | A D | return (no values) |
| `RET1` | A D | return R(A) (single value) |

#### Loop and Branch Ops
| Opcode | Format | Description |
|--------|--------|-------------|
| `FORI` | A D | Numeric for init: R(A) = start, R(A+1) = limit, R(A+2) = step; JMP D |
| `JFORI` | A D | FORI with JIT trace linkage |
| `FORL` | A D | Numeric for loop: R(A) += R(A+2); if step>0 ? R(A)<=R(A+1) : R(A)>=R(A+1) then JMP D |
| `IFORL` | A D | FORL interpreter-only variant |
| `JFORL` | A D | FORL with JIT trace linkage |
| `ITERL` | A D | Iterator for loop: if R(A) != nil then R(A-1) = R(A); JMP D |
| `IITERL` | A D | ITERL interpreter-only variant |
| `JITERL` | A D | ITERL with JIT trace linkage |
| `LOOP` | A D | Loop hint (no-op in interpreter, JIT uses for tracing) |
| `ILOOP` | A D | LOOP interpreter-only variant |
| `JLOOP` | A D | LOOP with JIT trace linkage |
| `JMP` | A D | PC += D - 0x8000 (signed jump offset) |

#### Function Header Ops (internal, not emitted by parser)
| Opcode | Format | Description |
|--------|--------|-------------|
| `FUNCF` | A | Fixed-arg Lua function entry |
| `IFUNCF` | A | FUNCF interpreter-only variant |
| `JFUNCF` | A D | FUNCF with JIT trace linkage |
| `FUNCV` | A | Vararg Lua function entry |
| `IFUNCV` | A | FUNCV interpreter-only variant |
| `JFUNCV` | A D | FUNCV with JIT trace linkage |
| `FUNCC` | A | C function entry |
| `FUNCCW` | A | C function entry (with wrapper) |

### 3.3 Resources and Cross-References
- Opcode definitions and metadata (including the `BCDEF` macro): `src/fluid/luajit-2.1/src/bytecode/lj_bc.h`.
- Parser emission sites: `parse_operators.cpp` (operator lowering), `operator_emitter.cpp` (register-aware helpers), `ir_emitter.cpp` (control-flow builders).
- Behavioural context and patterns: `src/fluid/luajit-2.1/src/parser/operator_emitter.cpp`, `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp`, and `src/fluid/luajit-2.1/src/parser/parse_control_flow.cpp` (parser wiring and control-flow emission patterns).
- Native array implementation: `src/fluid/luajit-2.1/src/runtime/lj_array.cpp`, `lj_array.h` (core array operations), `lj_vmarray.cpp` (bytecode handlers), `lib_array.cpp` (library functions).
- Array bytecode emission: `src/fluid/luajit-2.1/src/parser/ir_emitter/emit_function.cpp`, `parse_regalloc.cpp` (array index expression discharge).
- Tests exercising these paths live under `src/fluid/tests/`.

## 4. Conditional and Comparison Bytecodes
### 4.1 Conditional Bytecode Semantics
All comparison opcodes follow: **condition true → take the following JMP; condition false → fall through to next instruction**.

The comparison opcode and the following `JMP` form a single logical unit. When the condition is true, control branches to the JMP target; when false, execution continues sequentially.

### 4.2 Equality-with-Constant Opcodes (`BC_ISEQP`, `BC_ISEQN`, `BC_ISEQS`, `BC_ISNEP`, `BC_ISNEV`)
- `BC_ISEQP A, pri(D)`: compare register with primitive constant (nil/false/true). Equal → take JMP; not equal → fall through.
- `BC_ISEQN A, num(D)`: compare with numeric constant; same branch/fall-through behaviour.
- `BC_ISEQS A, str(D)`: compare with interned string constant; same branch/fall-through behaviour.
- `BC_ISNEP A, pri(D)` / `BC_ISNEV A, R(D)`: not-equal variants; not equal → take JMP; equal → fall through.
- Canonical branching: to branch on equality, place `JMP` immediately after the compare. Example (branch when value is nil):
  ```
  ISEQP   A, nil    ; nil → take JMP to target
  JMP     target    ; non-nil → fall through to next instruction
  ; non-nil path continues here
  ```
  For inverted sense (branch when not nil), use `ISNEP` with the same layout.

### 4.3 Generic Comparison Opcodes (`BC_ISLT`, `BC_ISGE`, `BC_ISLE`, `BC_ISGT`)
- These compare two registers (or a register and constant folded into D). Condition true takes the following JMP; condition false falls through.
- Pattern for `if a < b then ... else ... end`:
  ```
  ISLT    A, D      ; a < b → take JMP to else-branch
  JMP     else      ; a >= b → fall through to then-branch
  ; then-branch code here
  ```
- The parser uses the same idiom for numeric `for` bounds and relational operators in expressions.

### 4.4 Interaction with `BC_JMP` and Jump Lists
- Comparison + `JMP` encodes "if condition holds, branch to the jump target; otherwise fall through". A taken jump applies its signed offset from the instruction after the `JMP`.
- `jmp_patch` links jumps into singly linked lists so multiple comparisons can target a shared label. Patching a list fixes all pending offsets at once (e.g. chained falsey checks all landing on the RHS evaluation).
- Disassembly shows unpatched jumps as `----` offsets; when reading dumps from `--jit-options dump-bytecode`, identify compare/JMP pairs and the final patched destination to understand flow.

## 5. Native Array Operations

### 5.1 Overview

The native array type (`LJ_TARRAY`) provides a first-class VM type for fixed-size, homogeneous arrays. Unlike Lua tables, native arrays use dedicated bytecodes that bypass metamethod dispatch for element access, enabling better performance through direct memory access and JIT optimisation.

Native arrays support multiple element types: byte, int16, int32, int64, float, double, and pointer. All indexing uses 0-based conventions (Fluid standard).

### 5.2 Array Bytecode Semantics

Array bytecodes mirror the table access pattern but operate on `GCarray` objects:

| Bytecode | Operation | Bounds Check | Read-Only Check |
|----------|-----------|--------------|-----------------|
| `AGETV` | Load element by variable index | Yes | No |
| `AGETB` | Load element by literal index | Yes | No |
| `ASETV` | Store element by variable index | Yes | Yes |
| `ASETB` | Store element by literal index | Yes | Yes |

**Bounds checking**: All array access bytecodes validate `0 <= index < array.len`. Out-of-bounds access raises `LJ_ERR_ARROB`.

**Read-only checking**: Store operations (`ASETV`, `ASETB`) verify the `ARRAY_FLAG_READONLY` flag is not set. Writes to read-only arrays raise `LJ_ERR_ARRRO`.

### 5.3 Type Dispatch and Element Access

Array bytecodes must handle multiple element types. The interpreter dispatches based on `GCarray.elemtype`:

```
1. Type check: Verify operand is LJ_TARRAY
2. Index validation: Check 0 <= idx < len
3. Calculate element address: data + (idx * elemsize)
4. Load/store with type-appropriate width:
   - ARRAY_ELEM_BYTE:   8-bit load/store
   - ARRAY_ELEM_INT16:  16-bit load/store
   - ARRAY_ELEM_INT32:  32-bit load/store
   - ARRAY_ELEM_INT64:  64-bit load/store
   - ARRAY_ELEM_FLOAT:  32-bit float load/store
   - ARRAY_ELEM_DOUBLE: 64-bit double load/store
5. Box/unbox value for Lua stack
```

### 5.4 Fallback to Metamethods

When the base operand is not a native array, array bytecodes fall back to the standard table metamethod handlers (`vmeta_tgetv`, `vmeta_tsetv`). This enables:

- Graceful handling of userdata with `__index`/`__newindex`
- Mixed code that may operate on tables or arrays
- Forward compatibility with future array-like types

### 5.5 JIT Recording

Array bytecodes record to the JIT trace using call-based IR emission:

- `AGETV`/`AGETB` → `IRCALL_lj_arr_getidx`
- `ASETV`/`ASETB` → `IRCALL_lj_arr_setidx`

The recorder emits:
1. Type guard ensuring operand is array
2. Index narrowing (variable indices converted to integers)
3. Call to helper function with bounds checking

Future optimisation (Phase 5b) may inline element access for common types to eliminate function call overhead.

### 5.6 Parser Integration

When the parser can determine at compile time that an expression is an array (via type annotations or `array.new()` tracking), it emits array bytecodes directly:

- Known array + variable index → `BC_AGETV` / `BC_ASETV`
- Known array + literal index (0-255) → `BC_AGETB` / `BC_ASETB`

When the base type is unknown, standard table bytecodes (`TGETV`, `TSETV`, etc.) are emitted, relying on runtime type dispatch.

### 5.7 Example: Array Access Pattern

```lua
local arr = array.new(100, "integer")
arr[0] = 42        -- BC_ASETB if type known, else BC_TSETB
local x = arr[i]   -- BC_AGETV if type known, else BC_TGETV
```

Disassembly for known array type:
```
ASETB   arr, 0, value    ; Store 42 at index 0
AGETV   dest, arr, i     ; Load element at variable index i
```

## 6. Control-Flow and Short-Circuit Patterns
### 6.1 Logical Operators (`and`, `or`)
- `a and b`: evaluate `a`; emit compare + `JMP` that branches past `b` when `a` is falsey. Truthy `a` falls through, so `b` is evaluated and its result replaces/occupies the same slot.
- `a or b`: evaluate `a`; emit compare + `JMP` that branches into `b` only when `a` is falsey. Truthy `a` falls through and becomes the result; `b` is untouched.
- Registers are normalised so the resulting value lives in the LHS register; `freereg` collapses after RHS evaluation to avoid leaks.

### 6.2 Ternary Operator (`cond ? true_val :> false_val`)
- Only one branch executes. The condition is evaluated in place; compare + `JMP` sends control to the false branch when the condition is extended-falsey.
- `IrEmitter::emit_ternary_expr` places both branches so the result lands in the condition register. Each branch frees temporaries before convergence, and `freereg` is patched back to the condition's base to guarantee a single-slot result.
- Example sketch:
  ```
  <eval cond in RA>
  ISEQP  RA, nil ; nil → branch to false
  JMP    false   ; non-nil → fall through to true branch
  ; true branch emits true_val into RA
  JMP    end
false:
  ; false branch emits false_val into RA
end:
  ```

### 6.3 Presence Operator (`x?`) – Extended Falsey Check
- Falsey set: `nil`, `false`, numeric zero, empty string. If operand is in this set, result is `false` and RHS (if any) is skipped.
- Emission: chain `BC_ISEQP`/`BC_ISEQN`/`BC_ISEQS` comparisons, each followed by a `JMP` to the falsey path. A matching equality branches to that path; non-matching comparisons fall through to the next check. If all checks fall through (truthy value), execution continues past the chain.
- Jump lists patch the falsey exit after the chain; truthy fallthrough sets the result and collapses `freereg`.

### 6.4 If-Empty Operator (`lhs ?? rhs`) – Short-Circuiting with Extended Falsey Semantics
- Evaluate `lhs`; if it is nil/false/0/"", evaluate `rhs` and return it; otherwise return `lhs` without touching `rhs`.
- Implemented in `IrEmitter::emit_if_empty_expr` plus helper routines in `operator_emitter.cpp`. The compare chain mirrors the presence operator: a matching falsey value branches to RHS evaluation; a truthy value falls through all checks and skips RHS entirely.
- The result register is the original `lhs` slot; RHS evaluation reuses it and collapses `freereg` afterward to avoid leaked arguments or vararg tails.

## 7. Register Semantics and Multi-Value Behaviour
### 7.1 Register Lifetimes, `freereg`, and `nactvar`
- `nactvar` tracks active local slots; `freereg` is the first free slot above them. Temporaries are allocated above `nactvar` and must be reclaimed when no longer needed.
- Helpers such as `expr_free`, `RegisterGuard`, and `ir_collapse_freereg` ensure temporaries are released so following expressions do not see spurious stack entries.
- Failing to collapse `freereg` manifests as leaked arguments in calls or extra return values in vararg contexts.

### 7.2 `ExpKind::Call` and Multi-Return Semantics
- A freshly emitted `BC_CALL` yields an `ExpKind::Call` tied to its base register and may return multiple values (`BC_CALLM`) if left uncapped.
- Binary operators and control-flow constructs force single-value semantics: they convert call expressions to registers (`expr_toanyreg`), then free the call expression to cap results at one slot.
- Multi-return propagation is allowed only when explicitly constructing vararg lists; otherwise collapse to a single register before further comparisons or jumps.

### 7.3 Preventing Register Leaks in Chained Operations
- Reuse the LHS register when chaining operators at the same precedence; allocate new temporaries only when operands cannot share.
- After emitting a RHS, immediately free or collapse temporaries so the stack height matches `freereg` expectations before emitting the next operator.
- Always call `expr_free` before reserving new registers when an operand might still own a multi-return slot.

## 8. Common Emission Patterns and Anti-Patterns
### 8.1 Canonical Patterns (Do This)
- Compare + `JMP` for "branch on not-equal": `ISEQP A,const; JMP target` with true skipping the jump; see the relevant compare/jump emission logic in `operator_emitter.cpp`.
- Presence / if-empty chains: sequential `ISEQP/ISEQN/ISEQS` with shared jump list patched to the truthy or RHS path; see `operator_emitter.emit_presence_check` (called from `IrEmitter::emit_presence_expr`) and `IrEmitter::emit_if_empty_expr`.
- Logical short-circuit: `a or b` uses compare + `JMP` into RHS only when falsey; `a and b` jumps over RHS when falsey. Implemented via general binary operator emission in `operator_emitter.cpp` and `ir_emitter.cpp`.
- Ternary layout: condition in place, compare chain, then true/false blocks writing back into the same register, with end jump to merge; see `IrEmitter::emit_ternary_expr`.

### 8.2 Typical Mistakes (Do NOT Do This)
- Wiring compare + `JMP` backwards (treating "skip on equal" as "jump on equal"), which executes the wrong branch.
- Failing to collapse `freereg` after evaluating RHS, causing leaked stack slots that appear as extra arguments or returns.
- Allocating a new register for an operand without freeing the previous expression, allowing multi-return values to flow into subsequent operators.
- Regression tests that catch these issues: `src/fluid/tests/test_if_empty.fluid`, `test_presence.fluid`, logical operator suites, and ternary-focused cases; add new ones when patterns change.

## 9. Testing, Debugging, and Tooling
### 9.1 Using Flute and Fluid Tests
- Run the Fluid regression tests under `src/fluid/tests/` (e.g. `test_if_empty.fluid`, `test_presence.fluid`, logical/ternary suites) to validate control-flow changes.
- When adding coverage, use side effects (counters, print hooks) on RHS expressions to prove short-circuiting, and capture varargs with `{...}` to detect leaked registers.

### 9.2 Disassembly and Bytecode Inspection
- Obtain bytecode via `mtDebugLog('disasm')` on a `fluid` object or run scripts with `--jit-options dump-bytecode,diagnose`.
- Map disassembly back to source by matching instruction order to expression evaluation order, then locate emission sites in `ir_emitter.cpp` or `operator_emitter.cpp`.
- Treat disassembly as the source of truth for branch direction when debugging control flow.

## 10. Maintenance Guidelines
- When touching conditional emission or short-circuit logic, update the opcode matrix and the relevant sections here.
- Add or adjust regression tests in `src/fluid/tests/` to cover new control-flow behaviours; rerun tests after installing a fresh build.
- Re-generate disassembly for representative snippets (logical ops, ternary, `??`, `?`) to verify register collapse and branch wiring.
- Reviewers should confirm emitted patterns match the documented skip/execute semantics and that test coverage exercises both true and false paths.

## 11. Glossary and Quick Reference
- `BCIns`/`BCOp`: packed bytecode word and opcode enum.
- `freereg`: first free stack slot above active locals; must be collapsed after temporaries.
- `nactvar`: count of active local variables.
- `ExpKind`: parser expression classification (`VNONRELOC`, `VRELOCABLE`, `VCALL`, etc.).
- Extended falsey: `nil`, `false`, numeric zero, empty string.
- Short-circuit: using compare+`JMP` so RHS executes only when needed.
- `GCarray`: native array object with typed element storage.
- `LJ_TARRAY`: type tag for native arrays (value ~13).
- `ARRAY_FLAG_READONLY`: flag indicating array elements cannot be modified.
- `ARRAY_FLAG_EXTERNAL`: flag indicating array data is not owned by the GC.
- Cheat sheet: `ISEQ*`/`ISNE*`/`IS*` comparisons — true skips next, false executes next; `and` skips RHS on falsey, `or` skips RHS on truthy; `x?` and `lhs ?? rhs` use chained equality tests with shared jump lists; ternary writes result back into the condition register with one branch skipped; `AGETV`/`AGETB`/`ASETV`/`ASETB` provide direct array element access with bounds checking.
