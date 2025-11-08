# Alternative Defer Keyword Implementation Plan

## Status & Scope

**Status:** 📝 Draft ready for implementation review  
**Priority:** ⭐⭐⭐⭐ High  
**Estimated Effort:** 10–14 hours (parser + VM + tests)

This plan replaces the earlier heavy-weight proposal for a Fluid `defer`
keyword with a keep-it-simple approach that relies entirely on LuaJIT's
existing anonymous function machinery. Each `defer` statement is lowered
into an ordinary closure stored in a synthetic local. Scope management code
records those locals and emits standard `function` creation/call bytecode to
execute them when the scope unwinds. No bespoke opcodes or exotic runtime
subsystems are introduced.

## Investigation Summary

- **Scope tracking:** `FuncScope` in `lj_parse.c` already captures scope depth
  and is unwound via `fscope_begin()`/`fscope_end()`, giving us a natural hook
  for registering and releasing deferred closures without reworking parser
  control flow.【F:src/fluid/luajit-2.1/src/lj_parse.c†L1650-L1687】
- **Local storage:** `var_new()`/`var_add()` (same file) allocate locals and
  keep metadata in `LexState->vstack`, so synthetic locals can piggy-back on
  the same infrastructure without affecting register allocation rules.【F:src/fluid/luajit-2.1/src/lj_parse.c†L1404-L1460】
- **Function literals:** `parse_body()` produces closures through the regular
  `BC_FNEW` path; reusing it keeps closure semantics and GC integration
  consistent with user-authored anonymous functions.【F:src/fluid/luajit-2.1/src/lj_parse.c†L2358-L2408】
- **Control-transfer exits:** `parse_return()` and `parse_stmt()` are the
  choke points for generating `return`, `break`, and `continue` bytecode, so
  inserting defer unwinding there covers all explicit exits.【F:src/fluid/luajit-2.1/src/lj_parse.c†L3333-L3413】【F:src/fluid/luajit-2.1/src/lj_parse.c†L3648-L3711】
- **Error unwinding:** Runtime upvalue closure logic (`BC_UCLO` handling in
  the interpreter/JIT back-ends) already runs on both normal and exceptional
  paths, providing a single place to trigger deferred calls once the parser
  guarantees a `BC_UCLO` before every scope exit that owns defers.【F:src/fluid/luajit-2.1/src/lj_parse.c†L1671-L1687】【F:src/fluid/luajit-2.1/src/lj_dispatch.c†L392-L421】

## Guiding Principles (KISS)

1. **Reuse closures:** Defer blocks compile to ordinary anonymous functions;
   no new proto flags or GC structures.
2. **Leverage existing bytecode:** Emitted sequences stick to `BC_FNEW`,
   `BC_MOV`, `BC_CALL`, and `BC_UCLO` so the VM and recorder stay untouched.
3. **Minimal metadata:** Track deferred locals alongside existing scope data—
   no auxiliary stacks in the VM unless the parser cannot express the unwind.
4. **One unwind path:** Scope exits, control-flow jumps, and VM errors all
   converge on the same helper so semantics remain predictable.

## Phased Implementation

### Phase 1 – Lexical & Grammar Wiring
1. Extend `TKDEF` in `lj_lex.h` with the reserved word `_ (defer)` so
   the lexer interns it like other keywords; the token tables in `lj_lex.c`
   update automatically.【F:src/fluid/luajit-2.1/src/lj_lex.h†L14-L26】
2. Teach `parse_stmt()` to recognise `TK_defer` and dispatch to a new
   `parse_defer()` helper placed near other statement parsers.【F:src/fluid/luajit-2.1/src/lj_parse.c†L3648-L3711】
3. Add syntax diagnostics in `parse_defer()` to demand `defer ... end`
   blocks (matching existing Fluid keyword error messaging).

**Exit criteria:** Keyword tokenises correctly; trivial script containing
`defer end` is parsed (even if semantics are stubbed).

### Phase 2 – Parser Data Model for Deferred Locals
1. Introduce a lightweight `DeferInfo` (e.g. `{MSize scope_top; BCReg slot;}`)
   stored on a growable array inside `LexState`; snapshot/restore the stack in
   `fs_init()`/`fs_finish()` so nested functions inherit outer state cleanly.【F:src/fluid/luajit-2.1/src/lj_parse.c†L2004-L2030】
2. Extend `FuncScope` with a `defer_top` watermark captured in
   `fscope_begin()`; this marks the first deferred closure owned by the scope
   without touching existing flag bits.【F:src/fluid/luajit-2.1/src/lj_parse.c†L1650-L1669】
3. When `parse_defer()` lowers a block, allocate a synthetic local via
   `var_new_fixed()` / `var_add()`, emit the closure with `parse_body()`, and
   push `{scope_top = fs->bl->defer_top, slot = new_local_slot}` onto the
   `DeferInfo` stack.

**Exit criteria:** Parser state accurately tracks deferred locals, nested
scopes maintain LIFO order, and no bytecode is emitted yet.

### Phase 3 – Lowering `defer` to Closure Storage
1. Inside `parse_defer()`, after parsing the block body:
   - Discharge the resulting closure into the synthetic local register using
     `expr_toreg()`.
   - Prevent user code from reading the internal local by marking the
     corresponding `VarInfo` entry with a new `VSTACK_DEFER` flag bit.
2. Ensure registers stay contiguous: reserve one slot before emitting the
   closure so `fs->freereg` remains consistent with `nactvar`.
3. Emit `var_add()` so lifetime metadata matches the enclosing scope; this
   allows `var_remove()` to retire the local during unwinding without special
   cases.

**Exit criteria:** Compiled chunk shows a stored closure in bytecode (e.g. via
`luajit -bl`) while the helper that actually executes it is still a stub.

### Phase 4 – Unified Scope-Unwind Emission
1. Implement `emit_scope_defers(FuncState *fs, FuncScope *scope)` that pops
   all `DeferInfo` entries newer than `scope->defer_top` and, for each,
   generates:
   - `BC_MOV` into a scratch base register (reuse `fs->freereg`).
   - `BC_CALL` with zero arguments and zero expected results.
   This helper runs *before* `var_remove()` so the registers stay alive.
2. Call the helper from `fscope_end()` when a scope owns at least one defer.
   Emit a guarding `BC_UCLO` (re-using the existing instruction already
   produced for upvalues) so the VM executes the unwind even on error exits.
3. For immediate exits (`return`, `break`, `continue`, tail jumps), invoke the
   same helper prior to emitting their final instruction to cover control flow
   that bypasses `fscope_end()`.
4. Update the goto/break fix-up logic so patched jump targets land *after*
   the emitted defer sequence, preserving ordering guarantees.

**Exit criteria:** Runtime reaches defer closures in normal execution paths,
with bytecode limited to the existing instruction set.

### Phase 5 – Error-Unwind Integration
1. Audit interpreter/JIT `BC_UCLO` handlers (`vm_*.dasc`, `lj_dispatch.c`)
   to confirm they execute on both normal scope exits and VM errors. If a
   scope containing defers lacks an auto-generated `BC_UCLO`, update the
   parser to insert one explicitly when `emit_scope_defers()` runs.【F:src/fluid/luajit-2.1/src/lj_dispatch.c†L392-L421】
2. Add a C helper (e.g. `lj_vm_defer_run(lua_State *L, TValue *base, BCReg n)`) that
   expects the closures to be stored contiguously on the stack and calls them
   via `lj_vm_pcall()` to suppress cascading errors.
3. Adjust each architecture backend’s `BC_UCLO` case to invoke the helper
   immediately before closing upvalues when the compiler signalled pending
   defers (e.g. via a flag stored in `GCproto` or an unused slot in the stack
   frame header). Aim to re-use existing registers so no new bytecode format is
   required.
4. Extend `lj_record.c` to record the helper call as an ordinary function call
   so traces remain valid.

**Exit criteria:** Injected runtime helper runs during `pcall`-captured errors
and during normal unwinds; tests that throw within deferred scopes still see
all defers executed exactly once.

### Phase 6 – Tooling, Tests, and Documentation
1. **Testing:**
   - Create a dedicated Fluid test script exercising ordering, nesting, and
     error propagation. Place it under `src/fluid/tests/` and wire it into the
     CTest suite.
   - Add micro-benchmarks / sanity checks to ensure minimal overhead when no
     defers are present.
2. **Tooling:** Update the bytecode dumper/disassembler paths (if needed) to
   annotate the synthetic locals, making debugging easier.
3. **Documentation:** Refresh the Fluid reference manual with syntax and
   semantics, emphasising the closure-based lowering and error behaviour.

**Exit criteria:** Automated tests demonstrate correctness; developer-facing
materials describe usage clearly.

## Risk Log & Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Missing stack metadata for error unwinds | High | Validate the `BC_UCLO` execution path early; if gaps exist, add an explicit proto flag so the VM can detect scopes with defers. |
| Register pressure in tight scopes | Medium | Always reserve scratch registers via `bcreg_reserve()` and release them promptly; add regression tests with nested defers to monitor stack growth. |
| JIT trace divergence | Medium | Ensure the helper call is traceable and side-effect free beyond invoking closures; extend `lj_record.c` if needed. |
| User-visible synthetic locals | Low | Flag `VarInfo` entries so debugger/introspection tools can hide them. |

## Success Criteria

- `defer` executes closures in strict LIFO order across normal exits and error
  unwinds, matching Go-style semantics.
- Generated bytecode continues to use only existing instructions, verified by
  `luajit -bl` and the recorder.
- Fluid tests covering the new keyword pass alongside the existing suite.
- Documentation clearly explains the sugar and its limitations.

**Last Updated:** 2025-02-15
