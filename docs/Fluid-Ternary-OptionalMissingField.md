# Fluid ternary optional missing-field investigation

## Summary

The optional ternary regression is not limited to a single jump target mistake.
Re-running the Fluid tests confirms that `testOptionalMissingField` still fails,
and deeper instrumentation shows that the false branch *does* load the fallback
string, but subsequent register moves overwrite it before the surrounding code
can observe the value. The issue manifests both in simple assignments and when
the result participates in multi-value returns, so the fix must address register
ownership for the entire optional pipeline, not just the initial branch jump.

## Evidence

* `ctest --test-dir build/agents -R fluid_ternary --output-on-failure` continues
  to fail only `testOptionalMissingField`, keeping the regression isolated to
  optional access with a missing table field.【c38937†L1-L63】
* A minimal Fluid script that evaluates `t.missing ? "default"` still prints
  `nil`, confirming the interpreter reproduces the failure outside the test
  harness.【55aa8a†L1-L2】
* Disassembling that script shows the false branch loading `KSTR "default"` into
  `R2`, but the bytecode never copies it back to the local slot (`R1`). The
  subsequent `print` call reads from `R1`, so it logs `nil` even though the
  fallback string exists in `R2`.【8f7e65†L7-L18】
* A standalone function that returns both a present and missing field highlights
  a second failure mode: after the fallback string is loaded into `R4`, a later
  `MOV A=R4 D=R2` (emitted while arranging return registers) overwrites the
  fallback with the earlier truthy value. The return therefore exposes the wrong
  value even though the false branch executed correctly.【35d222†L7-L31】
* By comparison, a runtime falsey number (`x ? 100` with `x` supplied at
  runtime) emits the same guard sequence but successfully keeps the fallback in
  the destination register, demonstrating that the register shuffling regression
  is specific to values produced by table field lookups (which reuse the source
  register in later MOVs).【ee3266†L3-L24】
* The bytecode for a minimal helper that returns `t.missing ? "default"`
  clarifies the intent of the current emitter: it emits the fallback load at
  slot `R2` followed by a `RET1` that returns that register. The problem is that
  higher-level contexts (assignments, returns) continue to reference the
  original slot (`R1`), so they never observe the fallback unless an explicit
  copy occurs.【bb6494†L1-L23】
* A disassembly that contrasts a local assignment with a global assignment shows
  the divergence clearly: the local helper leaves the fallback in `R1` but the
  subsequent `tostring` still consumes `R0`, so it prints `local:nil`. The
  global helper, however, copies `R1` into the environment via `GSET` and then
  fetches it again with `GGET`, yielding `global:default`. This explains why
  rewriting the test to use a global causes it to pass.【00bb23†L1-L54】
* Comparing optional assignments with the ternary operator inside a helper
  script highlights why the ternary form keeps working. Optional locals still
  stash the fallback in `R2` while the return path reloads `R1`, so the helper
  prints `local presence result:nil`. The ternary form writes both branches into
  `R1`, so `local ternary result:missing` and both globals succeed because
  `GSET` stores whichever register currently owns the fallback.【166002†L1-L84】

## Reassessment

The earlier report attributed the failure solely to the truthy-path `MOV`
running after the false branch. The new disassembly traces refine that picture:

1. **Result slot mismatch.** The optional operator now materialises the
   fallback in a fresh register (`dest_reg`, typically `R2` or higher) while the
   original expression stays in its source register (`R1`). When the caller is a
   simple assignment, `bcemit_store()` still copies from `R1`, so the variable
   ends up as `nil`. The fallback never reaches the LHS unless an explicit
   `MOV` bridges the registers.
   * The ternary path avoids this by forcing both truthy and falsey results to
     occupy the original condition register: `expr_binop()` sets
     `result_reg = cond_reg`, then both branches call `expr_toreg(...,
     result_reg)` before collapsing temporaries.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L1002-L1068】
   * The optional path leaves the fallback in the reserved RHS slot. Even after
     `expr_init(e1, VNONRELOC, dest_reg)` the caller keeps reading `reg` because
     `bcemit_binop()` preserves the original source in a MOV before evaluating
     the RHS.【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L433-L535】
2. **Multi-result contamination.** Functions that return multiple values trigger
   additional `MOV` instructions to pack the return slots. Those moves blindly
   reuse the old source registers (`R2`, `R3`, …), clobbering the fallback that
   was parked in the reserved destination (`R4`). This mirrors the simple
   assignment problem but shows that the register ownership bug cascades into
   later stages of code generation.

These two issues explain why other optional scenarios still pass: constants and
plain locals never trigger the extra `MOV` because the value already lives in
the register expected by the caller. Missing table fields are the only case that
forces the optional operator to borrow a new register and therefore the only one
exposed to the mismatch.

The global variant provides a useful control: `GSET` consumes the fallback
register immediately, so even though the optional operator still leaves the
result in `R1`, the store path copies the correct value into the global table
before later MOVs can trample it. Locals lack this protective copy, so they
continue to observe the stale `R0`.

## Updated Plan

1. **Audit optional emission for register contracts.** Trace how
   `bcemit_binop_left()` and `bcemit_binop()` communicate the result register
   back to callers. Ensure that, after the false branch runs, the `ExpDesc`
   handed to `bcemit_store()` (and to return packing) points at the register that
   actually contains the fallback. Pay special attention to the `VLOCAL` path,
   which currently reloads from the stale source slot, and compare it with the
   `VGLOBAL` store that succeeds because `GSET` consumes the fallback register
   immediately. Bring the optional operator back into line with the ternary code
   by either keeping the result in the original `cond_reg` (as the ternary does)
   or by updating every consumer to honour the `dest_reg` allocated by
   `bcemit_binop()` before evaluating the RHS.【F:src/fluid/luajit-2.1/src/parser/lj_parse_expr.c†L1002-L1068】【F:src/fluid/luajit-2.1/src/parser/lj_parse_operators.c†L433-L535】
2. **Re-run the targeted Fluid tests and helper scripts** after adjusting the
   emitter to confirm that both assignments and multi-return helpers now expose
   the fallback string. Use `DebugLog('disasm')` on the same scripts above to
   verify that no stray `MOV` overwrites the false-branch result and that the
   return packing honours the updated register ownership.
3. **Guard against regressions.** Extend `test_ternary.fluid` with a regression
   assertion that exercises a missing field in both assignment and return
   contexts (mirroring the helper script) so that future register changes cannot
   silently reintroduce the bug.
