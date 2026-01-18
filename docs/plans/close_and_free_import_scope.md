# Close And Free Import Scope Plan

Status: Implemented steps 1-4; tests pending

## Goal
Replace the current KeepRegs/NAME_BLANK approach with a "close and free" strategy so import-scope locals are
properly closed (heap-upvalued) at scope exit and then removed, allowing registers to be safely reused.

## Proposed Design
- Treat import scopes as explicit close boundaries.
- Emit `BC_UCLO` unconditionally for the import-scope register range at scope end.
- Then call `var_remove()` and reset `freereg` normally (no register pinning).
- Remove `freereg_floor` and the `NAME_BLANK` masking loop.

## Steps
1. Introduce a dedicated scope flag (e.g. `FuncScopeFlag::ForceClose` or `ImportScope`) to mark import scopes. (done)
2. Update `emit_import_stmt()` to use the new flag instead of `KeepRegs`. (done)
3. Update `fscope_end()` to:
   - Always emit `BC_UCLO` for the import scope range.
   - Proceed with `var_remove()` and `reset_freereg()` as normal. (done)
4. Remove `freereg_floor` from `FuncState` and delete related logic in:
   - `reset_freereg()`, `assert_freereg_at_locals()`
   - `IrEmitter::ensure_register_balance()` (done)
   - any other register floor references. (done)
5. Add a Flute test that:
   - imports a file defining a local captured by a nested function,
   - runs code after import that allocates locals/registers,
   - verifies the captured value remains correct after the import scope ends.
6. Build and run tests (module build + install + test).

## Test Sketch (for step 5)
- `import` file: `local x = 41; function get() return x end; x = 42; export get`
- main file: call `get()` after allocating more locals and ensure it returns 42.
