# LuaJIT 2.1 Integration Notes

This file captures practices and gotchas observed while maintaining the
LuaJIT 2.1 sources that ship inside Parasol. Use it as a quick orientation
before diving into changes.

## Parser File Structure (src/parser/)

The parser has been modernized to C++20 and refactored into focused modules:

**Core Infrastructure:**
- `parse_types.h` - Type definitions, enums, and constexpr helper functions for expressions and scopes
- `parse_concepts.h` - C++20 concepts for compile-time type validation (15+ concepts)
- `parse_raii.h` - RAII guards (ScopeGuard, RegisterGuard, VStackGuard) with move semantics
- `parse_internal.h` - Internal function declarations and template helpers for bytecode emission

**Lexer:**
- `lj_lex.h` / `lj_lex.cpp` - Tokenization and lexical analysis

**Parser Core:**
- `lj_parse.h` / `lj_parse.cpp` - Main parser entry point and binary operator precedence handling
- `parse_core.cpp` - Core utilities, error handling, and token checking functions

**Parser Components:**
- `parse_constants.cpp` - Constant table management and jump list handling with iterator support
- `parse_regalloc.cpp` - Register allocation, bytecode emission, and expression discharge logic
- `parse_scope.cpp` - Scope management, variable resolution, and upvalue tracking
- `parse_expr.cpp` - Expression parsing (primary, suffix, constructor, function calls)
- `parse_operators.cpp` - Operator implementation (arithmetic folding, bitwise, unary, logical)
- `parse_stmt.cpp` - Statement parsing (assignments, control flow, declarations)

## Repository Layout Highlights
- `src/`: Upstream LuaJIT sources (parser, VM, JIT engine) with Parasol-specific modernizations
- `src/parser/`: C++20 refactored parser (see structure above)
- `src/fluid/tests/`: Fluid regression tests exercise the embedded LuaJIT runtime
- CMake drops generated headers, the VM object/assembly, and host helpers
  under `build/agents/src/fluid/luajit-generated/`. The final static
  library ends up in `build/agents/luajit-2.1/lib/`. None of these artefacts
  should be committed.

## Key Implementation Patterns

**Type Safety with Concepts:**
```cpp
// Use concepts for compile-time validation
template<BytecodeOpcode Op>
static inline BCPos bcemit_ABC(FuncState* fs, Op o, BCReg a, BCReg b, BCReg c);
```

**RAII for Resource Management:**
```cpp
// Automatic scope cleanup
FuncScope bl;
ScopeGuard scope_guard(fs, &bl, FuncScopeFlag::None);
// ... parse statements ...
// Automatic cleanup on scope exit
```

**Constexpr Expression Builders:**
```cpp
// Compile-time expression construction
auto expr = make_nil_expr();
auto bool_expr = make_bool_expr(true);
```

**Modern Container Usage:**
```cpp
// Prefer std::span for array access
auto uvmap_range = std::span(fs->uvmap.data(), fs->nuv);
for (auto uv_idx : uvmap_range) { ... }

// Use std::string_view for string parameters
static GCstr* keepstr(std::string_view str);
```

## Integration & Build Tips
- Always rebuild via CMake (e.g. `cmake --build build/agents --config <BuildType>`)
  after touching LuaJIT or Fluid sources so the static library target is
  regenerated and relinked into the Fluid module.
- CMake drives three build strategies, matching the logic in
  `src/fluid/CMakeLists.txt`:
  - **MSVC**: `msvcbuild_codegen.bat` produces generated headers and
    `lj_vm.obj`, and CMake links `lua51.lib` next to the upstream sources.
  - **Unix-like toolchains**: CMake builds the host tools (`minilua` and
    `buildvm`), generates assembly with DynASM, then archives
    `lj_vm.o` + `ljamalg.o` into `libluajit-5.1.a`.
- Install (`cmake --install build/agents --config <BuildType>`) before running
  tests so the freshly built `parasol` binary (or `parasol.exe` on Windows)
  and scripts land in `build/agents-install/`.

## Error Handling Configuration
- **Windows (MSVC)**: Must NOT define `LUAJIT_NO_UNWIND`. MSVC always uses
  Structured Exception Handling (SEH) via `RaiseException()` and `lj_err_unwind_win()`.
  There is no "internal unwinding" implementation for MSVC - SEH is the only
  viable mechanism. Setting `LJ_NO_UNWIND` for MSVC breaks exception handling
  and causes catch() tests to fail with "attempt to call a nil value" errors.
- The `LJ_NO_UNWIND` flag results in broken code that corrupts memory if used in GCC builds.

## Testing
- Use `ctest --build-config <BuildType> --test-dir build/agents -R <label>` to run subsets, or omit `-R` for the full suite. Fluid regression tests are under
  `src/fluid/tests/` and catch most parser/VM regressions.
- For quick manual checks, launch `parasol` (or `parasol.exe` on Windows) from `build/agents-install/bin/` with `--no-crash-handler --log-warning`
  so failures bubble out as exit codes.
- **Critical**: After touching LuaJIT C sources, rebuild both the Fluid module and `parasol_cmd`, then reinstall:
  ```bash
  cmake --build build/agents --config <BuildType> --parallel
  cmake --install build/agents --config <BuildType>
  ```
- When debugging parser issues, create minimal Fluid scripts to isolate the behaviour before running the full test suite.
- Unit tests are managed by `MODTests()` in `src/fluid/fluid.cpp`.
- To run the compiled-in unit tests, run `src/fluid/tests/test_unit_tests.fluid` with the `--log-xapi` option to view the output from stderr.
- Run `parasol` with `--jit-options` to pass JIT engine flags as a CSV list.  Available options are:
  - `trace` Enable tracing JIT
  - `diagnose` Enable diagnostic mode
  - `ast-pipeline` Use the new AST-based parser (default)
  - `ast-legacy` Use the legacy parser
  - `trace-boundary` Trace boundary crossings between interpreted and JIT code
  - `trace-bytecode` Trace bytecode execution
  - `profile` Use timers to profile JIT execution

## Troubleshooting Register Allocation
- LuaJIT's parser (`lj_parse.cpp`) heavily relies on `freereg`, `nactvar`, and
  expression kinds (`ExpKind`). When changing emission logic:
  - Never reduce `fs->freereg` below `fs->nactvar`; locals are stored there.
  - Ensure every path that creates a `VCALL` either converts it to
    `VNONRELOC` or signals to assignment helpers how many results the call
    should return.
  - The helper `expr_discharge()` is frequently used to normalise expressions
    before storage; inspect current usage before inventing new patterns.

### Register Management for Unary/Postfix Operators
When implementing operators that transform a value in-place (like unary `-`, `not`, or
custom postfix operators):

1. **Modifying the Same Register (In-Place):**
   ```c
   BCReg reg = expr_toanyreg(fs, e);
   // Emit bytecode that modifies reg in-place
   bcemit_AD(fs, BC_UNM, reg, reg);  // Example: negate the value
   // Expression still points to same register
   ```
   **Issue:** If the original variable is referenced again later in the same expression,
   it will get the modified value, not the original. Example: `x? and x` would see
   boolean for both uses of `x`.

2. **Allocating a New Register (Recommended for Value-Transforming Operators):**
   ```c
   BCReg src_reg = expr_toanyreg(fs, e);
   expr_free(fs, e);              // CRITICAL: Free source register first
   BCReg dest_reg = fs->freereg;
   bcreg_reserve(fs, 1);          // Allocate new register for result
   // Emit bytecode that reads src_reg, writes to dest_reg
   bcemit_AD(fs, BC_KPRI, dest_reg, VKTRUE);  // Example
   expr_init(e, VNONRELOC, dest_reg);
   ```
   **Benefits:** Original value remains accessible; no multi-value return issues.

3. **Using VRELOCABLE (For Simple Transformations):**
   ```c
   expr_toanyreg(fs, e);
   expr_free(fs, e);                            // Free old register
   e->u.s.info = bcemit_AD(fs, op, 0, e->u.s.info);  // Emit instruction
   e->k = VRELOCABLE;                           // Mark as relocatable
   ```
   This pattern (from `bcemit_unop()`) defers register allocation until later.

**Common Pitfall:** Forgetting `expr_free()` before allocating a new register causes
the function to return multiple values (the original value AND the transformed value),
which manifests as extra arguments in function calls or assignment contexts.

### CALL Instructions and Base Registers
- After a `BC_CALL` instruction executes, the result(s) are placed starting at
  the base register, **overwriting the function** that was there.
- When emitting multiple calls that reuse the same base register, always move
  values to their destination positions **before** loading the next function
  into the base register. Otherwise, the previous result will be overwritten.
- Example pattern for chained operations:
  ```c
  // CORRECT: Move result to argument position before reloading function
  expr_toreg(fs, previous_result, arg_position);
  expr_toreg(fs, new_function, base_register);

  // INCORRECT: Loading function first overwrites the result
  expr_toreg(fs, new_function, base_register);  // Overwrites result!
  expr_toreg(fs, previous_result, arg_position);  // Too late
  ```

### Controlling VCALL Result Counts
- By default, `VCALL` expressions can return multiple values, which may leak
  into assignment contexts. To restrict a call to a single result:
  - Set `VCALL_SINGLE_RESULT_FLAG` in `expr->u.s.aux` (bitwise OR with base reg)
  - Call `expr_discharge()` to convert the `VCALL` to `VNONRELOC`
  - The flag is automatically handled in `expr_discharge()` and `assign_adjust()`
- This pattern ensures chained operations don't expose multi-value semantics
  to the assignment machinery.

### Preventing Orphaned Registers in Chained Operations
When implementing operators that can chain across precedence boundaries (e.g.,
operators with C-style precedence), be careful to avoid orphaning intermediate
results on the register stack, which manifests as expressions returning multiple
values instead of one.

**The Problem Pattern:**
1. First operation completes, stores result in register N, sets `freereg = N+1`
2. Control returns to the expression parser to handle the next operator
3. Parser allocates a NEW base register (often `freereg`) for the next operation
4. Register N is left orphaned on the stack, becoming an extra return value

**The Solution:**
Before allocating a base register for an operation, check if the LHS operand
(which may be the previous operation's result) is already at the top of the
stack. The check pattern is:
```c
if (lhs->k == VNONRELOC && lhs->u.s.info >= fs->nactvar &&
    lhs->u.s.info + 1 == fs->freereg) {
   // LHS is at the top - reuse its register to avoid orphaning
   base_reg = lhs->u.s.info;
}
```

This commonly occurs when chaining across precedence levels where the parser
returns control between operations rather than handling the entire chain in
one function call.

**Debugging Orphaned Registers:**
- Symptom: Expressions return multiple values when they should return one
- Use printf debugging to trace `lhs->k`, `lhs->u.s.info`, `freereg`, and
  `nactvar` through operation sequences
- Check that `fs->freereg` is correctly adjusted after each operation completes
- Verify that base register reuse logic considers all stack-top scenarios

### Presence Operator Register Discipline

Investigations into the optional `?` operator highlighted several register-handling
rules that apply to any parser feature borrowing temporary slots:

1. **Surface the final value from the RHS register.**
   - After emitting the fallback value, update the expression descriptor to point at
     the RHS slot (e.g. `expr_init(e1, VNONRELOC, rhs_reg);`).
   - This mirrors the ternary operator contract and keeps concatenation chains over
     contiguous registers.

2. **Clamp `fs->freereg` using the borrowed register, not the assignment target.**
   - Collapse with `fs->freereg = rhs_reg + 1;` (or via `bcreg_free`) once the borrowed
     slot is written back.
   - Deriving the collapse point from the destination register leaves the borrowed
     slot marked live, causing `freereg` to drift upwards across chained operators.

3. **Maintain CAT chain contiguity.**
   - `BC_CAT` expects source registers to be consecutive. If an operator copies a
     fallback result into a non-contiguous slot, concatenations may emit duplicate
     values or raise "function or expression too complex" during parsing.
   - When debugging, dump the bytecode to verify CAT operands stay compact (for
     example `R3`, `R4`, `R5`).

4. **Use logical operators as a template.**
   - The `and` / `or` implementations demonstrate how to preserve operand slots across
     chained expressions with short-circuit semantics.

### Implementing Binary Logical Operators with Short-Circuiting

When implementing binary logical operators (like `or`, `and`, or custom variants like the `?`
if-empty operator), follow a two-phase pattern: setup in `bcemit_binop_left()` and completion in `bcemit_binop()`.

**The Pattern:**
1. **In `bcemit_binop_left()`**: Set up jumps in `e->t` for truthy LHS (to skip RHS) or
   `NO_JMP` for falsey LHS (to evaluate RHS). For truthy constants, you must allocate a
   register before loading them:
   ```c
   if (e->k == VKSTR || e->k == VKNUM || e->k == VKTRUE) {
     bcreg_reserve(fs, 1);  // CRITICAL: Allocate register first
     expr_toreg_nobranch(fs, e, fs->freereg-1);
     pc = bcemit_jmp(fs);  // Jump to skip RHS
   }
   ```
   **Never use `NO_REG` as a register number** - it causes runtime crashes in `lj_BC_MOV`.

2. **In `bcemit_binop()`**: Check if `e1->t != NO_JMP` to determine if LHS is truthy:
   ```c
   if (e1->t != NO_JMP) {
     // LHS is truthy - patch jumps to skip RHS, return LHS
     jmp_patch(fs, e1->t, fs->pc);
     e1->t = NO_JMP;
     // Ensure LHS is in a register (already done in bcemit_binop_left() for constants)
   } else {
     // LHS is falsey - evaluate RHS
   }
   ```

**Critical Gotcha:**
- After `expr_toreg_nobranch()` with a proper register, the expression becomes `VNONRELOC`
- Check `e1->k == VNONRELOC` before trying to load it again in `bcemit_binop()`
- If the constant was already loaded in `bcemit_binop_left()`, it's already in a register

**Pattern for Extended Falsey Checks** (as used in `?` if-empty operator and `??` presence check):

The pattern chains multiple equality checks, where each check's JMP is patched to the same "falsey branch". The logic works as follows:

```c
// Check for nil
bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
check_nil = bcemit_jmp(fs);  // Skipped when reg == nil
// Check for false
bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
check_false = bcemit_jmp(fs);  // Skipped when reg == false
// Check for zero
bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
check_zero = bcemit_jmp(fs);  // Skipped when reg == 0
// Check for empty string
bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
check_empty = bcemit_jmp(fs);  // Skipped when reg == ""

// Patch all JMPs to the falsey branch (e.g., RHS evaluation)
jmp_patch(fs, check_nil, falsey_branch_pc);
jmp_patch(fs, check_false, falsey_branch_pc);
jmp_patch(fs, check_zero, falsey_branch_pc);
jmp_patch(fs, check_empty, falsey_branch_pc);
```

**How This Works**:
- **When value is falsey** (e.g., `reg == nil`): The matching `BC_ISEQP` (e.g., `BC_ISEQP reg, VKNIL`) finds equality → skips its JMP → execution continues past all checks → eventually reaches code that handles the falsey case (e.g., load false, evaluate RHS)
- **When value is truthy** (e.g., `reg == "hello"`): ALL `BC_ISEQP` checks find inequality (reg != nil, reg != false, reg != 0, reg != "") → NONE skip their JMPs → the first JMP executes and jumps to the falsey branch → result is correct (e.g., load true, skip RHS)

**Note**: This pattern works because we chain multiple checks and patch all JMPs to the same location. If the value matches any falsey check, that check's JMP is skipped and execution continues. If the value matches none of the checks (is truthy), all JMPs execute and the first one jumps to the falsey branch. The exact behavior depends on how the falsey branch is structured.

### Implementing Operators with Extended Falsey Semantics

When implementing operators with custom falsey semantics (e.g., treating `0` and `""` as falsey
in addition to `nil` and `false`):

1. **Handle compile-time constants separately** in `bcemit_binop()`:
   ```c
   if (e1->k == VKNIL || e1->k == VKFALSE) {
     // Definitely falsey - evaluate RHS
   } else if (e1->k == VKNUM && expr_numiszero(e1)) {
     // Zero is falsey - evaluate RHS
   } else if (e1->k == VKSTR && e1->u.sval && e1->u.sval->len == 0) {
     // Empty string is falsey - evaluate RHS
   }
   ```

2. **For runtime values**, emit a chain of comparison instructions:
   ```c
   // Check for nil
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
   check_nil = bcemit_jmp(fs);
   // Check for false
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
   check_false = bcemit_jmp(fs);
   // Check for zero
   bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
   check_zero = bcemit_jmp(fs);
   // Check for empty string
   bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
   check_empty = bcemit_jmp(fs);

   // All falsey checks jump to RHS evaluation
   jmp_patch(fs, check_nil, fs->pc);
   jmp_patch(fs, check_false, fs->pc);
   // ... etc
   ```

   **How This Works**:
   - When `reg == nil`: `BC_ISEQP` skips the `JMP` → execution continues past all checks
   - When `reg != nil`: `BC_ISEQP` doesn't skip → `JMP` executes → jumps to RHS evaluation
   - Since we want to evaluate RHS when ANY falsey value is found, we patch all jumps to the RHS evaluation point
   - If the value is truthy, ALL checks skip their jumps, and execution continues (returning the truthy value)

3. **Use `lj_parse_keepstr()` for string constants** to ensure they're anchored and not GC'd:
   ```c
   emptyv.u.sval = lj_parse_keepstr(fs->ls, "", 0);
   ```

### Single-Character Token Recognition

When adding single-character operators (like `?`):

1. **Add the token** to `TKDEF` in `lj_lex.h` using the `T2` macro:
   ```c
   __(if_empty, ?)
   ```

2. **Handle recognition in the lexer** (`lj_lex.cpp`) in the switch statement:
   ```c
   case '?':
     lex_next(ls);
     return TK_if_empty;
   ```

### Multi-Character Token Recognition

When adding operators that extend reserved words:

1. **Add the token** to `TKDEF` in `lj_lex.h` using the `T2` macro.

2. **Handle recognition in the lexer** (`lj_lex.cpp`) after identifying a reserved word by checking the next character.
   **Important**: Check the next character (`ls->c`) **after** recognizing the reserved word,
   not in the character switch statement.

3. **Map the token to an operator** in `token2binop()` in `lj_parse.cpp`

## Debugging Parser Issues

When adding new operators or modifying expression parsing, use these strategies:

### Printf Debugging for Token Flow
Add temporary printf statements to trace token values through parsing:
```c
// In expr_primary() suffix loop or similar:
printf("[DEBUG] tok=%d ('%c' if printable)\n", ls->tok,
       (ls->tok >= 32 && ls->tok < 127) ? ls->tok : '?');
```
Remember to remove these before committing. Token values < 256 are ASCII characters;
values >= 256 are token type constants from `lj_lex.h`.

### Disassembling Generated Bytecode

The `DebugLog()` method can be used to dump bytecode instructions during script execution, or after
the script has been compiled with the Activate() action.  For example:

```lua
function runtime_disassembly()
   local self = obj.find('self')
   local scale = b * 2
   local total = a + scale
   local err, result = self.mtDebugLog('disasm')
   print(result)
end

function compiled_disassembly()
   -- Create an independent script object with sample code
   local script = obj.new('script', { statement = [[
function sampleFunction(a, b)
   local scale = b * 2
   local total = a + scale
   return result
end
]]
   })

   script.acActivate()
   local err, result = script.mtDebugLog('disasm')
   print(result)
end
```

Example output:

```
0000       0 FUNCV     A=R4
0001       5 FNEW      A=R0 D=K<func 1-5>
  --- lines 1-5, 5 bytecodes ---
  0000       1 FUNCF     A=R5
  0001       2 MULVN     A=R2 B=R1 C=#2
  0002       3 ADDVV     A=R3 B=R0 C=R2
  0003       4 GGET      A=R4 D=K"result"
  0004       4 RET1      A=R4 D=#2
```

### Diagnosing Multi-Value Return Issues
If an operator returns multiple values instead of one:
- **Symptom**: Expressions like `(x + y)?` produce two values in varargs contexts
- **Common cause**: Allocating a new register without calling `expr_free()` first
- **Fix pattern**:
  ```c
  BCReg src_reg = expr_toanyreg(fs, e);
  expr_free(fs, e);           // Free the source register
  BCReg dest_reg = fs->freereg;
  bcreg_reserve(fs, 1);       // Allocate result register
  ```
- **Test with**: Create a function that counts return values using `{...}` and `#`

### Incremental Testing Strategy
When implementing new operators:
1. Start with compile-time constants (`5`, `nil`) - test constant folding path
2. Test simple variables (`x`) - test basic runtime path
3. Test field access (`t.field`) - test suffix loop integration
4. Test parenthesized expressions (`(x + y)`) - test complex expressions
5. Test function calls that return a single result and those that return multiple results
   (`f(x)`) - test VCALL handling
6. Test in various contexts (assignments, function arguments, conditionals)
7. If issues arise, use DebugLog('disasm') as a source of truth rather than guessing the
   logic of emitted bytecode.

## Miscellaneous Gotchas
- Check that any new compile-time constants or flags (e.g. `#define`s) do
  not collide with upstream naming; we will eventually rebase to newer
  LuaJIT drops.
- Generated build outputs under `build/agents/` can be removed safely; do not
  store investigation artefacts there long-term.
- Keep an eye on Fluid tests after modifying LuaJIT semantics—failures often
  surface as subtle script regressions rather than outright crashes.

---

## Quick Reference

**File Locations:**
- Parser source: `src/fluid/luajit-2.1/src/parser/`
- Fluid tests: `src/fluid/tests/`
- Modernization plan: `docs/plans/parser-cpp20-modernisation.md`

**Common Tasks:**
- Adding an operator: See "Implementing Binary Logical Operators" and "Single-Character Token Recognition" sections
- Register allocation: See "Troubleshooting Register Allocation" and "Register Management" sections
- Debugging bytecode: Use `script.mtDebugLog('disasm')` (see "Disassembling Generated Bytecode" section)
- Understanding concepts: See `parse_concepts.h` for type constraints
- Expression builders: See `parse_types.h` for constexpr factories

**Key Headers:**
- `parse_types.h` - Core types and expression builders
- `parse_concepts.h` - C++20 type constraints
- `parse_raii.h` - RAII guards
- `parse_internal.h` - Function declarations and templates
