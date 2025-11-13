# Safe navigation with `?` investigation

## Summary
- The Fluid test suite reports failures in `test_safe_nav.fluid` when safe navigation chains feed into the `?` operator (presence/if-empty).
- Manual experiments show that `guest?.profile?.name ? "Guest"` yields `nil` instead of the fallback string, and the truthy branch returns the intermediate table instead of the final value.【0a5ae9†L1-L4】【1c54b2†L1-L5】
- Returning the same expression directly from a function produces the correct values, which indicates the evaluation logic works but the assignment path is wrong.【b89f9a†L1-L24】
- Disassembly of a repro function demonstrates that the final result of the `?` operator resides in register `R2`, yet the assignment to `local display` continues to use `R1`, which still holds the intermediate object from the safe-navigation chain.【d7022b†L9-L65】
- Inspecting locals confirms that `display` remains `nil` despite the fallback being written, proving the register mismatch.【398273†L1-L37】
- New Flute tests (`testSafeNavPresenceInsideCallLosesFallback`, `testSafeNavPresenceWithSiblingArgumentLosesFallback`) capture how the current parser emits bytecode that drops the fallback when the combined expression is used as a function argument, matching the reviewer’s report of corrupted call sites.【F:src/fluid/tests/test_safe_nav.fluid†L103-L130】

## Detailed findings
1. `ctest -L fluid` fails tests 9 and 23 in `test_safe_nav.fluid`, both of which combine safe navigation with the `?` operator.【552fd9†L33-L60】
2. Running the expression in isolation with `parasol` reproduces the issue. The fallback value is not applied when the chain yields `nil`, and the truthy branch returns the parent table instead of the nested value.【0a5ae9†L1-L4】【1c54b2†L1-L5】
3. Evaluating `member?.profile?.name` by itself returns the expected string, so safe navigation alone is correct.【51b8e6†L1-L5】
4. Returning the combined expression from a function yields correct strings (`"Guest"` and `"Chloe"`), which shows the runtime implementation emits the right bytecode for returns.【b89f9a†L16-L31】
5. Disassembly of a minimal repro reveals that register `R2` carries the result of `guest?.profile?.name ? "Guest"`, but the local variable `display` remains bound to register `R1`. The assignment never copies the fallback result back into that original slot, so subsequent loads read the stale intermediate value.【d7022b†L9-L65】
6. Inspecting local variables immediately after the assignment confirms `display = nil`, even though the fallback string is present in register `R2`. This proves the bug is a register bookkeeping error during assignment, not a logic error in the safe navigation operation itself.【398273†L1-L37】
7. Exercising the expression as a function argument shows that the emitted bytecode passes `nil` into the call even after the fallback should have been applied (`result=captured:nil`).【73a345†L1-L4】 The bare `print` case reveals that the call observes both the original `nil` and the fallback (`nilGuest`), demonstrating that the temporary register now leaks into call arguments and corrupts the stack layout.【6a05d4†L1-L4】

## Conclusion
The failure stems from register management when chaining `?.` with the `?` operator. The fallback/string result resides in a temporary register, but the compiler does not move it back to the register associated with the left-hand expression. As a result, locals and subsequent consumers observe the previous intermediate value (either `nil` or the parent table), triggering the Flute assertions.

## Next steps
1. Rework the `OPR_IF_EMPTY` path so that safe-navigation temporaries are moved into the expression’s register without touching `fs->nactvar`, ensuring function calls keep their base registers intact.
2. Validate the fix against the newly added call-site regressions plus the original assignment coverage before landing any parser changes.
