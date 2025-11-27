# Bytecode Semantics Reference

## 1. Introduction and Scope
Parasol integrates a heavily modified LuaJIT 2.1 VM. This note captures the control-flow semantics of its bytecode so parser and emitter changes do not regress short-circuiting or extended-falsey behaviour. It answers questions such as "when does `BC_ISEQP` skip the next instruction?" and "how do `??` and `?` wire their jumps?" and is aimed at maintainers working on `IrEmitter`/`OperatorEmitter` or debugging logical and ternary operators.

## 2. Notation, Conventions, and Versioning
- Registers are shown as `R0`, `R1`, etc. Fields A/B/C/D follow LuaJIT encoding: `A` is usually a destination or base, `B`/`C` are sources, `D` is a constant or split field. `base` is the current stack frame start.
- Conditions are expressed as "condition true → skip next instruction; condition false → execute next instruction (normally a `JMP`)." "Next instruction" means the sequential `BCIns`; a taken `JMP` applies its offset from the following instruction.
- Version: LuaJIT 2.1 with Parasol patches, assuming the `LJ_FR2` two-slot frame layout used by all supported platforms.
- Keep this file aligned with changes in `src/fluid/luajit-2.1/src/parser/parse_operators.cpp`, `operator_emitter.cpp`, and `ir_emitter.cpp` whenever bytecode emission patterns change.

## 3. Bytecode Overview
### 3.1 High-Level Structure
- Each instruction is a 32-bit `BCIns` packed with opcode and A/B/C/D fields. Prototypes (`GCproto`) hold the instruction stream, constants, and line table.
- This document focuses on conditional and control-flow opcodes that affect short-circuiting; arithmetic and data-movement opcodes are intentionally omitted.

### 3.2 Resources and Cross-References
- Opcode definitions and metadata (including the `BCDEF` macro): `src/fluid/luajit-2.1/src/bytecode/lj_bc.h`.
- Parser emission sites: `parse_operators.cpp` (operator lowering), `operator_emitter.cpp` (register-aware helpers), `ir_emitter.cpp` (control-flow builders).
- Behavioural context and patterns: `src/fluid/luajit-2.1/src/parser/operator_emitter.cpp`, `src/fluid/luajit-2.1/src/parser/ir_emitter.cpp`, and `src/fluid/luajit-2.1/src/parser/parse_control_flow.cpp` (parser wiring and control-flow emission patterns).
- Tests exercising these paths live under `src/fluid/tests/`.

## 4. Conditional and Comparison Bytecodes
### 4.1 Summary Table: Conditional Bytecode Matrix
| Opcode | Condition | Condition true | Condition false | Typical follower |
| --- | --- | --- | --- | --- |
| `BC_ISEQP` | `R(A) == pri(D)` | Skip next | Execute next | `JMP` to false/alternate path |
| `BC_ISEQN` | `R(A) == num(D)` | Skip next | Execute next | `JMP` |
| `BC_ISEQS` | `R(A) == str(D)` | Skip next | Execute next | `JMP` |
| `BC_ISEQV` | `R(A) == R(D)` | Skip next | Execute next | `JMP` |
| `BC_ISNEP` | `R(A) != pri(D)` | Skip next | Execute next | `JMP` |
| `BC_ISNEV` | `R(A) != R(D)` | Skip next | Execute next | `JMP` |
| `BC_ISLT` | `R(A) < R(D)` | Skip next | Execute next | `JMP` |
| `BC_ISGE` | `R(A) >= R(D)` | Skip next | Execute next | `JMP` |
| `BC_ISLE` | `R(A) <= R(D)` | Skip next | Execute next | `JMP` |
| `BC_ISGT` | `R(A) > R(D)` | Skip next | Execute next | `JMP` |

### 4.2 Equality-with-Constant Opcodes (`BC_ISEQP`, `BC_ISEQN`, `BC_ISEQS`, `BC_ISNEP`, `BC_ISNEV`)
- `BC_ISEQP A, pri(D)`: compare register with primitive constant (nil/false/true). Equal → skip next; not equal → execute next.
- `BC_ISEQN A, num(D)`: compare with numeric constant; same skip/execute behaviour.
- `BC_ISEQS A, str(D)`: compare with interned string constant; same skip/execute.
- `BC_ISNEP A, pri(D)` / `BC_ISNEV A, R(D)`: not-equal variants; condition true (not equal) skips next.
- Canonical branching: to branch on equality, place `JMP` immediately after the compare. Example (branch when value is nil):
  ```
  ISEQP   A, nil    ; nil → skip JMP, fall through to nil path
  JMP     target    ; non-nil → jump to alternate path
  ; nil path continues here
  ```
  For inverted sense (branch when not nil), use `ISNEP` with the same layout.

### 4.3 Generic Comparison Opcodes (`BC_ISLT`, `BC_ISGE`, `BC_ISLE`, `BC_ISGT`)
- These compare two registers (or a register and constant folded into D). Condition true skips the next instruction; condition false executes it.
- Pattern for `if a < b then ... else ... end`:
  ```
  ISLT    A, D      ; a < b → skip JMP → enter then-branch
  JMP     else      ; a >= b → jump to else-branch
  ```
- The parser uses the same idiom for numeric `for` bounds and relational operators in expressions.

### 4.4 Interaction with `BC_JMP` and Jump Lists
- Comparison + `JMP` encodes "if condition holds, skip the jump; otherwise execute the jump". A taken jump applies its signed offset from the instruction after the `JMP`.
- `jmp_patch` links jumps into singly linked lists so multiple comparisons can target a shared label. Patching a list fixes all pending offsets at once (e.g. chained falsey checks all landing on the RHS evaluation).
- Disassembly shows skipped jumps as `----` offsets; when reading dumps from `mtDebugLog('disasm')`, identify compare/JMP pairs and the final patched destination to understand flow.

## 5. Control-Flow and Short-Circuit Patterns
### 5.1 Logical Operators (`and`, `or`)
- `a and b`: evaluate `a`; emit compare + `JMP` that jumps past `b` when `a` is falsey. True `a` skips the `JMP`, so `b` is evaluated and its result replaces/occupies the same slot.
- `a or b`: evaluate `a`; emit compare + `JMP` that jumps into `b` only when `a` is falsey. Truthy `a` skips the `JMP` and becomes the result; `b` is untouched.
- Registers are normalised so the resulting value lives in the LHS register; `freereg` collapses after RHS evaluation to avoid leaks.

### 5.2 Ternary Operator (`cond ? true_val :> false_val`)
- Only one branch executes. The condition is evaluated in place; compare + `JMP` sends control to the false branch when the condition is extended-falsey.
- `IrEmitter::emit_ternary_expr` places both branches so the result lands in the condition register. Each branch frees temporaries before convergence, and `freereg` is patched back to the condition’s base to guarantee a single-slot result.
- Example sketch:
  ```
  <eval cond in RA>
  ISEQP  RA, nil ; skip if nil
  JMP    false   ; non-nil → jump if needed depending on extended checks
  ; true branch emits true_val into RA
  JMP    end
false:
  ; false branch emits false_val into RA
end:
  ```

### 5.3 Presence Operator (`x?`) – Extended Falsey Check
- Falsey set: `nil`, `false`, numeric zero, empty string. If operand is in this set, result is `false` and RHS (if any) is skipped.
- Emission: chain `BC_ISEQP`/`BC_ISEQN`/`BC_ISEQS` comparisons, each followed by a `JMP` to the truthy path. Equality skips its `JMP`, advancing to the next check; if none match, the first `JMP` executes to bypass falsey handling. This preserves short-circuiting without running RHS helpers.
- Jump lists patch the truthy exit after the chain; falsey fallthrough sets the result and collapses `freereg`.

### 5.4 If-Empty Operator (`lhs ?? rhs`) – Short-Circuiting with Extended Falsey Semantics
- Evaluate `lhs`; if it is nil/false/0/"", evaluate `rhs` and return it; otherwise return `lhs` without touching `rhs`.
- Implemented in `IrEmitter::emit_if_empty_expr` plus helper routines in `operator_emitter.cpp`. The compare chain mirrors the presence operator: each equality skips its `JMP`, so a matching falsey value falls through into RHS evaluation; a truthy value triggers the first `JMP` and skips RHS entirely.
- The result register is the original `lhs` slot; RHS evaluation reuses it and collapses `freereg` afterward to avoid leaked arguments or vararg tails.

## 6. Register Semantics and Multi-Value Behaviour
### 6.1 Register Lifetimes, `freereg`, and `nactvar`
- `nactvar` tracks active local slots; `freereg` is the first free slot above them. Temporaries are allocated above `nactvar` and must be reclaimed when no longer needed.
- Helpers such as `expr_free`, `RegisterGuard`, and `ir_collapse_freereg` ensure temporaries are released so following expressions do not see spurious stack entries.
- Failing to collapse `freereg` manifests as leaked arguments in calls or extra return values in vararg contexts.

### 6.2 `ExpKind::Call` and Multi-Return Semantics
- A freshly emitted `BC_CALL` yields an `ExpKind::Call` tied to its base register and may return multiple values (`BC_CALLM`) if left uncapped.
- Binary operators and control-flow constructs force single-value semantics: they convert call expressions to registers (`expr_toanyreg`), then free the call expression to cap results at one slot.
- Multi-return propagation is allowed only when explicitly constructing vararg lists; otherwise collapse to a single register before further comparisons or jumps.

### 6.3 Preventing Register Leaks in Chained Operations
- Reuse the LHS register when chaining operators at the same precedence; allocate new temporaries only when operands cannot share.
- After emitting a RHS, immediately free or collapse temporaries so the stack height matches `freereg` expectations before emitting the next operator.
- Always call `expr_free` before reserving new registers when an operand might still own a multi-return slot.

## 7. Common Emission Patterns and Anti-Patterns
### 7.1 Canonical Patterns (Do This)
- Compare + `JMP` for "branch on not-equal": `ISEQP A,const; JMP target` with true skipping the jump; see `operator_emitter::emit_compare_const`.
- Presence / if-empty chains: sequential `ISEQP/ISEQN/ISEQS` with shared jump list patched to the truthy or RHS path; see `operator_emitter.emit_presence_check` (called from `IrEmitter::emit_presence_expr`) and `IrEmitter::emit_if_empty_expr`.
- Logical short-circuit: `a or b` uses compare + `JMP` into RHS only when falsey; `a and b` jumps over RHS when falsey. Implemented in `emit_or_expr` / `emit_and_expr`.
- Ternary layout: condition in place, compare chain, then true/false blocks writing back into the same register, with end jump to merge; see `emit_ternary_expr`.

### 7.2 Typical Mistakes (Do NOT Do This)
- Wiring compare + `JMP` backwards (treating "skip on equal" as "jump on equal"), which executes the wrong branch.
- Failing to collapse `freereg` after evaluating RHS, causing leaked stack slots that appear as extra arguments or returns.
- Allocating a new register for an operand without freeing the previous expression, allowing multi-return values to flow into subsequent operators.
- Regression tests that catch these issues: `src/fluid/tests/test_if_empty.fluid`, `test_presence.fluid`, logical operator suites, and ternary-focused cases; add new ones when patterns change.

## 8. Testing, Debugging, and Tooling
### 8.1 Using Flute and Fluid Tests
- Run the Fluid regression tests under `src/fluid/tests/` (e.g. `test_if_empty.fluid`, `test_presence.fluid`, logical/ternary suites) to validate control-flow changes.
- When adding coverage, use side effects (counters, print hooks) on RHS expressions to prove short-circuiting, and capture varargs with `{...}` to detect leaked registers.

### 8.2 Disassembly and Bytecode Inspection
- Obtain bytecode via `mtDebugLog('disasm')` on a `fluid` object or run scripts with `--jit-options dump-bytecode,diagnose`.
- Map disassembly back to source by matching instruction order to expression evaluation order, then locate emission sites in `ir_emitter.cpp` or `operator_emitter.cpp`.
- Treat disassembly as the source of truth for branch direction when debugging control flow.

## 9. Maintenance Guidelines
- When touching conditional emission or short-circuit logic, update the opcode matrix and the relevant sections here.
- Add or adjust regression tests in `src/fluid/tests/` to cover new control-flow behaviours; rerun tests after installing a fresh build.
- Re-generate disassembly for representative snippets (logical ops, ternary, `??`, `?`) to verify register collapse and branch wiring.
- Reviewers should confirm emitted patterns match the documented skip/execute semantics and that test coverage exercises both true and false paths.

## 10. Glossary and Quick Reference
- `BCIns`/`BCOp`: packed bytecode word and opcode enum.
- `freereg`: first free stack slot above active locals; must be collapsed after temporaries.
- `nactvar`: count of active local variables.
- `ExpKind`: parser expression classification (`VNONRELOC`, `VRELOCABLE`, `VCALL`, etc.).
- Extended falsey: `nil`, `false`, numeric zero, empty string.
- Short-circuit: using compare+`JMP` so RHS executes only when needed.
- Cheat sheet: `ISEQ*`/`ISNE*`/`IS*` comparisons — true skips next, false executes next; `and` skips RHS on falsey, `or` skips RHS on truthy; `x?` and `lhs ?? rhs` use chained equality tests with shared jump lists; ternary writes result back into the condition register with one branch skipped.
