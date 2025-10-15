# XPath FLWOR Clause Status (FastBuild XML Suite)

## Build Summary
- Built the FastBuild configuration via `cmake --build build/agents --config FastBuild --parallel` and installed to `install/agents`.
- Installation step reports expected missing X11 runtime libraries (libXrandr) because the headless cloud environment excludes desktop dependencies.

## Test Execution
- Ran `ctest --build-config FastBuild --test-dir build/agents -L xml`.
- Only `xml_xpath_flwor_clauses` failed; all other XML-labelled suites passed.
- Re-ran the failing target with `--output-on-failure` to capture detailed assertions.

## Observed Failures
- **Ordering assertions**: ascending string order returns `bk-3,bk-1,bk-4,bk-2`; descending numeric order returns `bk-1,bk-2,bk-3,bk-4`.
- **Grouping**: both aggregate and sequence-access FLWOR variants abort with `General failure` before callbacks fire.
- **COUNT clause**: evaluation halts with the same general failure, preventing counter verification.
- **Error reporting**: invalid `group by` expression returns `Group key expression could not be evaluated.` rather than the expected guidance about referencing bound variables.

## Instrumentation Progress
- Added a `TraceCategory::XPath` toggle on `XPathEvaluator` so instrumentation can be enabled only when the XML suites are under investigation.
- Extended `evaluate_flwor_pipeline` to log order-by metadata (collation URI, direction, empty ordering) and capture each tuple's bindings and derived key values before the comparator runs.
- Instrumented group-by evaluation to emit key materialisation per tuple, record whether groups are created or merged, and snapshot the merged bindings to highlight where `ERR::Failed` is tripping.

## Completed Diagnostics
- ✅ Implemented the FLWOR order-by logging hook and trace toggle without altering release behaviour.
- ✅ Captured group-by execution paths inside `evaluate_flwor_pipeline`, exposing tuple keys and merged bindings for `ERR::Failed` tracking.
- ✅ Added an environment-controlled trace level override (`PARASOL_TRACE_XPATH_LEVEL`) so FLWOR instrumentation can emit at warning/info when `--log-api` crashes in the headless runner.【F:src/xpath/xpath_evaluator.cpp†L60-L130】【F:src/xpath/xpath_evaluator_values.cpp†L1938-L1957】

## Diagnostic Findings (15 Oct)
- Instrumented run (`PARASOL_TRACE_XPATH=1 PARASOL_TRACE_XPATH_LEVEL=warning`) confirms order-by stages derive the correct key material and report sorted indices `1,3,2,0` (ascending) and `1,3,0,2` (descending), yet `mtFindTag` still returns the original unsorted sequence—indicating the post-sort tuple materialisation step is discarding the comparator output.【e199d0†L1-L33】
- Group-by traces show tuple merges functioning for both aggregate and sequence-access cases, but the return clause aborts with `ERR::Failed` (`FLWOR return expression could not be evaluated.`), implying the grouped bindings or result-construction path is dereferencing missing data after grouping succeeds.【e199d0†L34-L63】
- COUNT pipeline reproduces the same return-clause failure once order-by has run, highlighting that positional binding synthesis is not completing when group/order clauses precede the return expression.【e199d0†L64-L75】
- Direct `--log-api` execution of the Fluid suite segfaults prior to evaluation; using the new trace-level override keeps logging actionable while avoiding the crash (follow-up issue to catalogue separately).【fca96e†L1-L40】【787a0b†L1-L43】

## Next Diagnostic Steps
1. Trace the tuple materialisation path after sorting to see why the sorted order `(1,3,2,0)` is discarded—inspect the vector that feeds `invoke_callback` and how `original_index` is applied when emitting nodes.【e199d0†L1-L33】
2. Step through `FLWOR return` evaluation when `group by` is active to locate the `ERR::Failed` source; verify grouped bindings survive `merge_binding_maps` and that constructed element builders tolerate node-set aggregates.【e199d0†L34-L63】
3. Audit COUNT handling immediately after order-by to ensure positional counters are initialised from the sorted tuple list rather than the pre-sort sequence.【e199d0†L64-L75】
4. Capture and document the `--log-api` segmentation fault so we can either guard the logger or file a separate stability fix for the diagnostics harness.【fca96e†L1-L40】
