# Investigation: `test_if_empty.fluid`

## Summary
- Running the Fluid regression suite reveals that `test_if_empty.fluid` fails at two different stages: an initial parse-time overflow and a runtime argument mismatch in `testInFunction()` once compilation succeeds.
- Instrumentation and bytecode disassembly show that the `?` if-empty operator leaks a spare register when used in a function-call argument, causing LuaJIT to pass an extra value (`'then'`) as the third parameter to `string.find`.
- The register leak also explains the earlier `function or expression too complex` syntax error: repeated calls such as `msg(Number ? 2)` keep bumping `freereg` until the parser hits `LJ_MAX_SLOTS`.

## Reproduction
1. Build and install the Release configuration.
2. Execute `ctest --build-config Release --test-dir build/agents --output-on-failure -L fluid`.
3. Observe the failure for `fluid_if_empty`, initially reporting `function or expression too complex near 'msg'` and, after a full rebuild that ensures the latest Fluid bytecode is linked, the runtime error `bad argument #3 to 'find' (number expected, got string)` inside `testInFunction`.

Key excerpts:
- Parse-time overflow near `msg(Number ? 2)`.【d4cb99†L41-L48】
- Runtime failure showing the third argument to `string.find` is the literal `'then'`.【fb0ff7†L1-L34】

## Root Cause Analysis
`testInFunction()` contains the minimal reproducer. The ternary form works, but the if-empty expression is emitted as an extra argument when compiled inside a function call.【F:src/fluid/tests/test_if_empty.fluid†L203-L211】 Bytecode disassembly of an isolated script confirms that the compiler allocates three arguments for `string.find`: the second parameter is the pattern `'e'`, while the third slot is occupied by the RHS fallback `'then'` even when the LHS is truthy.【d4d338†L1-L27】

Because the fallback register is never released, `fs->freereg` continues to grow across successive arguments. In large call expressions (e.g., consecutive `msg(Number ? 2)` invocations) this runaway allocation eventually reaches `LJ_MAX_SLOTS`, which LuaJIT reports as “function or expression too complex”. The same leaked register is visible in the bytecode (`CALL ... C=#4`), demonstrating that the operator returns two values in vararg contexts instead of collapsing to one.

## Supporting Evidence
- `/tmp/if_empty_debug.log` captured during investigation shows `freereg` monotonically increasing on the lines that compile `msg(Number ? 2)` and `string.find((Function.status ? 'then'), 'e')`, corroborating the register leak hypothesis. (Log not committed; retained locally.)
- Manual disassembly (above) demonstrates that the fallback literal occupies a live register at call time, producing the observed runtime failure.

## Session 2 Reflection
- The follow-up session stalled immediately because the workplan still centred on preparing a fix, while the updated instructions asked for investigative follow-through first. Without an amended plan that prioritised diagnostic test coverage, the agent response defaulted to a refusal and no progress was made.
- We also lacked purpose-built regression tests that exercise the if-empty operator inside different call patterns, which meant there was little new evidence to capture before attempting a code change.

## Session 2 Findings
- Manual stress tests confirm that using `Function.status ? 'then'` as a function argument still produces two values. The vararg capture now shows that LuaJIT duplicates the truthy operand across both slots instead of leaving the fallback literal live.
- Wrapping the `string.find` call in `pcall` consistently surfaces the runtime failure with a “bad argument #3” diagnostic, providing a deterministic signature for the leak without crashing the surrounding test harness.

## Session 3 Implementation Notes
- Normalised the optional operator’s truthy path so that it collapses back to the original register and releases any scratch slot reserved for the fallback expression. This mirrors the ternary operator contract and prevents the extra argument that previously leaked into function calls.
- Updated the regression coverage to assert the fixed behaviour (single argument capture, successful `string.find` invocation, and matching ternary results) so that future changes surface any regressions immediately.

## Next Steps
1. Exercise the updated bytecode with the custom Fluid snippets recorded in `docs/if-empty-custom-tests.md` to confirm there are no remaining edge cases (e.g., concatenation chains or nested optional operators).
2. Run the Fluid suite after rebuilding to ensure no regressions emerge elsewhere (known `test_safe_nav.fluid` issues remain acceptable noise).
3. Trim or relocate any investigation-only artefacts once the fix stabilises to keep the repository tidy.
