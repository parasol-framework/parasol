# Alternative Defer Keyword Implementation Plan

## Overview

This plan replaces the previous `defer` keyword strategy with a lightweight approach that reuses
Fluid's existing anonymous function support. Instead of introducing bespoke bytecode and runtime
plumbing, the parser lowers each `defer` block into an anonymous function that is stored in an
internally generated local variable flagged for deferred execution. When the variable leaves scope
its flagged closure runs automatically, preserving the familiar scope-based, LIFO, and error-safe
semantics expected of `defer`.

**Status:** 📝 Draft proposal – awaiting implementation sign-off

**Priority:** ⭐⭐⭐⭐ High

**Estimated Effort:** 10–14 hours (including parser, VM, and tests)

## Goals and Non-Goals

### Goals

- Provide scope-based deferred execution with LIFO ordering, guaranteed invocation, and standard Lua
  closure capture semantics.
- Minimise new infrastructure by mapping `defer` to anonymous functions managed like existing local
  variables.
- Maintain compatibility with existing tooling (bytecode dumper, recorder, etc.) by avoiding new
  opcodes where possible.
- Keep the implementation maintainable and easy to reason about for contributors familiar with Lua
  function handling.

### Non-Goals

- Extending the syntax beyond `defer()` blocks (e.g. `defer call()` inline forms).
- Altering function calling conventions or closure representation.
- Introducing new bytecode instructions or VM subsystems unless absolutely unavoidable.

## High-Level Design

1. **Parser desugaring:**
   - On encountering `defer() ... end`, emit bytecode equivalent to declaring an anonymous function
     and storing it in a fresh internal local (e.g. `local @defer#1 = function() ... end`).
   - Mark the created prototype (or closure) with a new `PROTOTYPE_FLAG_DEFER` (boolean field if no
     spare flags exist).
   - Record the synthetic local in the current scope's defer stack to retain LIFO ordering.

2. **Scope unwinding:**
   - Augment scope exit logic to iterate recorded deferred locals in reverse order, invoking any
     closure whose prototype carries the defer flag.
   - Ensure all exit paths trigger the same unwinding: normal scope end, `return`, `break`,
     `continue`, and error unwinds.

3. **Runtime integration:**
   - Reuse existing function call helpers (`lj_vm_call`, etc.) when executing deferred closures.
   - Guarantee execution even on error by chaining to the existing error unwinder; deferred calls run
     in protected mode and propagate the first failure after all handlers execute.

4. **State representation:**
   - Introduce a lightweight `DeferInfo` stack per scope/function to track synthetic locals (register
     indices or slots) and their counts for reverse traversal.
   - No new garbage-collected structures are required because closures already participate in GC via
     existing locals/upvalues.

## Detailed Implementation Steps

### Step 1 – Parser Support

**Files:** `src/fluid/luajit-2.1/src/lj_parse.c`, `src/fluid/luajit-2.1/src/lj_parse.h`

1. Add a `defer_depth` (or vector) field to scope metadata to record the number of synthetic defer
   locals currently active.
2. Extend `parse_defer()` to:
   - Allocate a fresh internal local (reusing existing helpers for temporary locals).
   - Parse the block body as a standard anonymous function.
   - Mark the resulting function prototype with a `PROTO_DFLAG` or new boolean (see Step 2).
   - Push the local's register index onto the scope's defer stack.
3. Ensure nested scopes inherit outer defer stacks while maintaining their own counts for LIFO
   unwinding.

### Step 2 – Prototype Flagging

**Files:** `src/fluid/luajit-2.1/src/lj_proto.h`, `src/fluid/luajit-2.1/src/lj_parse.c`

1. Add a `PROTO_HAS_DEFER` flag (or reuse a spare bit) to `GCproto` to mark compiled closures created
   from `defer` blocks.
2. Set the flag when lowering defer functions in the parser.
3. Provide a helper macro/function `proto_is_defer(proto)` for readability.

### Step 3 – Scope Exit Execution

**Files:** `src/fluid/luajit-2.1/src/lj_parse.c`, `src/fluid/luajit-2.1/src/lj_record.c`,
`src/fluid/luajit-2.1/src/lj_vm.h`, `src/fluid/luajit-2.1/src/lj_vm*.c`

1. Extend `fscope_end()`, `parse_return()`, `parse_break()`, and `parse_continue()` to emit a helper
   that will call `lj_vm_defer_unwind()` before releasing locals.
2. Implement `lj_vm_defer_unwind(FuncState *fs, BCReg base, uint8_t count)` (exact signature TBD)
   that:
   - Iterates deferred locals from newest to oldest.
   - For each flagged closure, executes it via the existing call mechanism, passing zero arguments.
   - Runs handlers in protected mode; if a handler errors, store the error and continue unwinding,
     rethrowing after completion.
3. Ensure error unwinds (longjmp paths) also trigger the helper by hooking into existing panic/error
   paths that already iterate active locals.

### Step 4 – Bytecode & VM Adjustments

**Files:** `src/fluid/luajit-2.1/src/lj_bcemit.c`, `src/fluid/luajit-2.1/src/lj_vm*.c`

1. Verify that storing the anonymous function in a local reuses existing bytecode (`BC_FNEW`,
   `BC_MOV`, etc.) without modification.
2. If a dedicated unwind helper is required, implement it in C and call from generated bytecode using
   existing call patterns (e.g. through fast functions or helper stubs) to avoid new opcodes.
3. Confirm that tail returns and VARG clean-up paths also run `lj_vm_defer_unwind()` before
   completion.

### Step 5 – Tooling and Debugging Support

**Files:** `src/fluid/luajit-2.1/src/lj_bcwrite.c`, `src/fluid/luajit-2.1/src/lj_disasm.c`,
`docs/wiki/Fluid-Reference-Manual.md`

1. Ensure bytecode writers/disassemblers annotate deferred closures (e.g. comment or flag) using the
   new prototype bit.
2. Update documentation to describe the lowered form and execution semantics.

### Step 6 – Testing

**Files:** `src/fluid/tests/test_defer.fluid` (new or existing), regression harness scripts

1. Add Fluid tests verifying:
   - Basic scope defer execution order.
   - Nested scopes with defer.
   - Interaction with `return`, `break`, `continue`, and errors.
   - Closure capture of mutable variables.
   - Multiple defers ensuring LIFO behaviour.
2. Include VM error propagation tests (handler failure followed by successful handlers).

## Potential Risks & Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Missing flag bits on `GCproto` | Medium | Introduce a dedicated boolean field if no flag bit is available. |
| Error unwinding gaps | High | Audit all scope-exit paths and share a single `defer_unwind` helper invoked everywhere. |
| Interaction with existing optimisations (JIT) | Medium | Update recorder to treat flagged closures like standard functions; add guard tests. |
| Performance overhead | Low | Only synthetic locals with defer flag incur the extra check; ensure helper returns quickly when no defer entries exist. |

## Rollout Strategy

1. Implement parser and runtime changes behind a compile-time flag if incremental rollout is
   preferred (optional).
2. Land feature behind hidden config until documentation and tests are complete.
3. Remove old plan references and update developer documentation upon completion.

## Open Questions

- Should deferred closures accept arguments? The sugar can evaluate arguments immediately and store
  them in local temporaries, matching Go semantics; confirm whether this is required for initial
  rollout.
- Is additional tooling needed to surface deferred closures in stack traces?

## Success Criteria

- All semantic guarantees (scope-based execution, LIFO ordering, guaranteed execution on all exit
  paths, and closure capture) pass the new test suite.
- No new bytecode instructions are introduced, and existing tooling continues to function without
  modification.
- Implementation complexity is markedly lower than the previous plan, facilitating easier long-term
  maintenance.

## References

- Existing `function` and closure implementation in LuaJIT (`lj_parse.c`, `lj_vm*.c`).
- Go `defer` semantics for behavioural comparison.
- Current `defer-keyword.md` plan (superseded by this simplified design).

**Last Updated:** 2025-02-XX

