# Compound assignment register leak investigation

## Reproduction
- Build/install debug tree, then run `parasol --log-warning src/fluid/tests/test_compound.fluid`.
- Warnings report leaked registers before `emit_compound_assignment` runs and `RegisterSpan` depth mismatches while cleaning up compound assignments on table indexes.
- A minimal repro is available at `src/fluid/tests/test_compound_minimal.fluid` (indexed `+=` feeding from the same slot).

## Observations
- `emit_assignment_stmt` prepares targets via `prepare_assignment_targets`, which duplicates table operands and reserves registers, leaving `freereg` above `nactvar` by design.
- `emit_compound_assignment` saves that register level, then duplicates the base/key again via `duplicate_table_operands`, creating another `RegisterSpan`.
- Cleanup currently releases spans in the order `target.reserved` then `copies.reserved` after resetting `freereg` to the saved value.
- When the original `target.reserved` span is released first, it drops `freereg` below the expected top for `copies.reserved`, triggering the depth mismatch warnings logged in `RegisterAllocator::release_span_internal`.

## Root cause
- Register spans are being released out of LIFO order. The span allocated last (`copies.reserved`) expects `freereg` to match its own `expected_top`, but releasing the earlier span first lowers `freereg`, so the second release sees a mismatch (e.g., expected 6 vs actual 2).

## Fix and validation
- Implemented LIFO cleanup in `emit_compound_assignment` and `emit_if_empty_assignment` by releasing table-operand duplicates before the prepared target spans, restoring `freereg` to each span's expected top prior to release, and dropping the noisy entry leak warning.
- Ensured update expressions (`++`/`--`) also release duplicated table operands once the store is emitted to avoid stale spans.
- Result: `src/fluid/tests/test_compound.fluid`, `src/fluid/tests/test_compound_minimal.fluid`, and `ctest -L fluid` now run cleanly without register depth warnings.
