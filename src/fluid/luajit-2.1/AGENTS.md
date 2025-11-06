# LuaJIT 2.1 Integration Notes

This file captures practices and gotchas observed while maintaining the
LuaJIT 2.1 sources that ship inside Parasol. Use it as a quick orientation
before diving into changes.

## Repository Layout Highlights
- `src/`: Upstream LuaJIT sources (parser, VM, JIT engine) live here;
  Parasol-specific tweaks are kept minimal to ease rebases.
- `src/fluid/tests/`: Fluid regression tests exercise the embedded LuaJIT
  runtime. If you change parser or VM behaviour, run the Fluid suite.
- Generated artefacts are staged under `build/agents/src/fluid/luajit-generated/`
  during a CMake build; they do not belong in git.

## Integration & Build Tips
- Always rebuild via CMake (`cmake --build build/agents --config Release`)
  after touching C/C++ files so the LuaJIT static library is regenerated.
- On Windows, CMake rebuilds regenerate the LuaJIT VM object using the
  bundled `minilua.exe`. Expect console noise about setting `LJLINK`; it is
  harmless.
- Install (`cmake --install build/agents --config Release`) before running
  tests; the integrator copies `parasol.exe` and scripts to `install/agents/`.

## Testing
- Use `ctest --build-config Release --test-dir build/agents -R <label>`
  to run a subset. Full Fluid test runs ensure parser and VM changes do not
  regress hosted scripts.
- For quick manual checks, launch `parasol.exe` from `install/agents/` with
  `--no-crash-handler --gfx-driver=headless` so failures return exit codes.
- **Critical**: For static builds, after modifying LuaJIT C sources, you must
  rebuild BOTH the fluid module AND parasol_cmd, then reinstall:
  ```bash
  cmake --build build/agents --config Release --parallel
  cmake --install build/agents --config Release
  ```
- When debugging parser issues, create minimal test scripts to isolate the
  problem before running the full test suite.

## Coding Conventions & Constraints
- LuaJIT is an upstream C project; follow its existing style (tabs, K&R,
  use of `&&`/`||`, traditional casts, etc.). Parasol’s stricter C++ rules
  (e.g. mandatory `and`/`or`, `IS`, no exceptions) do **not** apply here.
- When touching Parasol-owned C++ files that interact with LuaJIT, switch
  back to the repository’s standard requirements.
- Temporary logging is acceptable during investigations but remove or guard
  it before committing. Windows builds collect logs under `build/agents/`.

## Troubleshooting Register Allocation
- LuaJIT's parser (`lj_parse.c`) heavily relies on `freereg`, `nactvar`, and
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

### Understanding Comparison Bytecode Semantics

**CRITICAL**: LuaJIT's comparison bytecode instructions (`BC_ISEQP`, `BC_ISEQN`, `BC_ISEQS`, `BC_ISNEP`, `BC_ISNEN`, `BC_ISNES`) have specific semantics that are essential to understand:

- **`BC_ISEQP/BC_ISEQN/BC_ISEQS`** (Is Equal): When the values **ARE equal**, the instruction **modifies the program counter (PC) to skip the next instruction**. When values are **NOT equal**, execution continues to the next instruction normally.

- **`BC_ISNEP/BC_ISNEN/BC_ISNES`** (Is Not Equal): When the values **are NOT equal**, the instruction **modifies PC to skip the next instruction**. When values **ARE equal**, execution continues normally.

**Key Point**: "Skip the next instruction" means the instruction immediately following the comparison is conditionally skipped. This is typically a `BC_JMP` instruction that is patched later.

**Example Pattern**:
```c
// Check if reg == nil
bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
check_nil = bcemit_jmp(fs);  // This JMP is SKIPPED when reg == nil
// Behavior:
//   - If reg == nil: BC_ISEQP skips the JMP → execution continues to next instruction
//   - If reg != nil: BC_ISEQP doesn't skip → JMP executes → jumps to its target
```

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

**Important**: The PR comment that claimed "BC_ISEQP skips when comparison succeeds" was **ambiguous** because "succeeds" could mean either:
1. "The comparison operation completes successfully" (always true - incorrect interpretation)
2. "The comparison condition is true" (values ARE equal - correct interpretation)

The correct interpretation is #2: `BC_ISEQP` skips the next instruction when the comparison condition is **true** (values ARE equal).

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
   __(if_null, ?)
   ```

2. **Handle recognition in the lexer** (`lj_lex.c`) in the switch statement:
   ```c
   case '?':
     lex_next(ls);
     return TK_if_empty;
   ```

### Multi-Character Token Recognition

When adding operators that extend reserved words:

1. **Add the token** to `TKDEF` in `lj_lex.h` using the `T2` macro.

2. **Handle recognition in the lexer** (`lj_lex.c`) after identifying a reserved word by checking the next character.
   **Important**: Check the next character (`ls->c`) **after** recognizing the reserved word,
   not in the character switch statement.

3. **Map the token to an operator** in `token2binop()` in `lj_parse.c`

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
1. Start with compile-time constants (`5?`, `nil?`) - test constant folding path
2. Test simple variables (`x?`) - test basic runtime path
3. Test field access (`t.field?`) - test suffix loop integration
4. Test parenthesized expressions (`(x + y)?`) - test complex expressions
5. Test in various contexts (assignments, function arguments, conditionals)

## Miscellaneous Gotchas
- Check that any new compile-time constants or flags (e.g. `#define`s) do
  not collide with upstream naming; we will eventually rebase to newer
  LuaJIT drops.
- Generated build outputs under `build/agents/` can be removed safely; do not
  store investigation artefacts there long-term.
- Keep an eye on Fluid tests after modifying LuaJIT semantics—failures often
  surface as subtle script regressions rather than outright crashes.

_Last updated: 2025-11-04_
