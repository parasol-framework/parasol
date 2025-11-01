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

## Miscellaneous Gotchas
- Check that any new compile-time constants or flags (e.g. `#define`s) do
  not collide with upstream naming; we will eventually rebase to newer
  LuaJIT drops.
- Generated build outputs under `build/agents/` can be removed safely; do not
  store investigation artefacts there long-term.
- Keep an eye on Fluid tests after modifying LuaJIT semantics—failures often
  surface as subtle script regressions rather than outright crashes.

_Last updated: 2025-10-29_
