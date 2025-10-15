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
- ✅ Instrumented the tuple emission pipeline to record document-order re-sorting, callback indices, and grouped return evaluation failures, enabling step-by-step tracing of order-by and group-by materialisation.【F:src/xpath/xpath_evaluator_values.cpp†L2628-L2721】【393af0†L16-L86】

## Diagnostic Findings (15 Oct)
- Instrumented run (`PARASOL_TRACE_XPATH=1 PARASOL_TRACE_XPATH_LEVEL=warning`) confirms order-by stages derive the correct key material and report sorted indices `1,3,2,0` (ascending) and `1,3,0,2` (descending), yet `mtFindTag` still returns the original unsorted sequence—indicating the post-sort tuple materialisation step is discarding the comparator output.【e199d0†L1-L33】
- Group-by traces show tuple merges functioning for both aggregate and sequence-access cases, but the return clause aborts with `ERR::Failed` (`FLWOR return expression could not be evaluated.`), implying the grouped bindings or result-construction path is dereferencing missing data after grouping succeeds.【e199d0†L34-L63】
- COUNT pipeline reproduces the same return-clause failure once order-by has run, highlighting that positional binding synthesis is not completing when group/order clauses precede the return expression.【e199d0†L64-L75】
- Direct `--log-api` execution of the Fluid suite segfaults prior to evaluation; using the new trace-level override keeps logging actionable while avoiding the crash (follow-up issue to catalogue separately).【fca96e†L1-L40】【787a0b†L1-L43】
- Step 1 tracing shows `process_expression_node_set` reorders the FLWOR output back into document order (`[3,0,2,1]` and `[2,3,0,1]`), so callbacks consume the original sequence despite the comparator returning `[1,3,2,0]` and `[2,3,0,1]`. The fix must preserve the sorted tuple vector when invoking callbacks.【F:src/xpath/xpath_evaluator_values.cpp†L2598-L2694】【393af0†L21-L42】
- Step 2 tracing captures the grouped tuples entering the return clause with combined node-set bindings, yet evaluation fails before emitting any nodes and leaves `xml->ErrorMsg` empty—suggesting the return expression hits an unreported runtime error inside grouped constructors or aggregate helpers.【F:src/xpath/xpath_evaluator_values.cpp†L2661-L2707】【393af0†L58-L82】

## Next Diagnostic Steps
1. Patch `process_expression_node_set` so it honours the sorted tuple order (likely by preserving the sorted iterator vector instead of re-sorting by document order) and re-run the ascending/descending order tests to confirm the callback sequence matches the comparator.【393af0†L21-L42】
2. Investigate the grouped return expression failure by probing `append_constructor_sequence` and related aggregators for unhandled empty node-sets, and ensure `record_error` surfaces a message when `xml->ErrorMsg` remains blank.【F:src/xpath/xpath_evaluator_values.cpp†L2661-L2707】【393af0†L58-L82】
3. Audit COUNT handling immediately after order-by to ensure positional counters are initialised from the sorted tuple list rather than the pre-sort sequence.【e199d0†L64-L75】
4. Capture and document the `--log-api` segmentation fault so we can either guard the logger or file a separate stability fix for the diagnostics harness.【fca96e†L1-L40】

## Diagnostic Findings (18 Oct)
- Order-by instrumentation confirms the callback pipeline now consumes the sorted tuple sequence `[1,3,2,0]`, yielding `bk-1,bk-2,bk-4,bk-3`. The Fluid expectations still anticipate `bk-3` ahead of `bk-4`, so we need to reconcile the fixture with codepoint collation rules before closing out Step 1.【F:src/xpath/xpath_evaluator_values.cpp†L3696-L3734】【f89e16†L16-L46】
- Group-by and count clauses continue to abort before emitting nodes. The new constructor tracing shows attribute value template evaluation failing for grouped tuples, but the AST signatures need deeper inspection to isolate the missing binding or unsupported expression.【F:src/xpath/xpath_evaluator_values.cpp†L1071-L1103】【f89e16†L47-L92】
- Added `record_error` tracing ensures XPath diagnostics populate `xml->ErrorMsg` when attribute or constructor expressions fail, preventing silent `General failure` reports during Fluid execution.【F:src/xpath/xpath_evaluator.cpp†L151-L160】【F:src/xpath/xpath_evaluator_values.cpp†L1094-L1110】

## Current Actions
1. Compare the Fluid order-by expectations with the W3C codepoint collation to decide whether the fixture or comparator requires adjustment now that callback order matches the sorted tuples.【f89e16†L16-L46】
2. Use the new constructor tracing to step through the grouped return clause and identify which attribute expression marks `expression_unsupported`, then patch the evaluator so grouped bindings survive the return clause.【F:src/xpath/xpath_evaluator_values.cpp†L1045-L1132】
3. Keep the count-clause instrumentation enabled while debugging the grouped return fix so we can verify positional variables (`$position`) align with the sorted tuple list once the return clause stops aborting.【F:src/xpath/xpath_evaluator_values.cpp†L2619-L2656】
