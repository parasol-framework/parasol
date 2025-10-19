# XPath Module Test Failures (ctest `-L xpath`)

## Overview

Running `ctest --test-dir build/agents -L xpath --output-on-failure` shows two failing suites:

* **`xpath_xquery_prolog`** – fails `testModuleImportDeclaration` because the assertion still expects the pre-module-loader diagnostic.
* **`xpath_xpath_module_loading`** – every positive module-loading scenario fails because the queries run without a usable base URI, so the loader cannot resolve the location hints.

See the full console output in the test log for context.【7494f4†L1-L111】

## Failure: `xpath_xquery_prolog`

* The Fluid test still checks for the legacy message `"Module function resolution is not implemented for namespace …"` and is marked with a TODO to update the expectation now that module importing exists.【F:src/xpath/tests/test_prolog.fluid†L213-L258】
* The runtime now reports `Module '…' could not be loaded for function '…'` when resolution fails, because `evaluate_user_defined_function` relies on the module cache and emits that message when the loader cannot supply a compiled module.【F:src/xpath/eval/eval_values.cpp†L261-L303】

### Recommended fix

1. Replace the outdated equality assertion with a check for the new diagnostic (for example, assert that the message contains `"could not be loaded"`).
2. Update the namespace-normalisation test to assert against the canonicalised URI that the loader reports.
3. Remove the stale TODO comments once the new assertions are in place.

## Failure: `xpath_xpath_module_loading`

* Every test case constructs an `xml` object from an inline `statement`, which leaves both the document `Path` and the prolog `static_base_uri` blank. The module cache therefore falls back to using the raw hint text (for example `modules/math_utils.xq`) as a direct filesystem path.【F:src/xpath/tests/test_module_loading.fluid†L8-L224】
* The loader needs either a `static_base_uri` or an object `Path` to resolve those hints to absolute locations.【F:src/xpath/api/xquery_prolog.cpp†L88-L109】 Without either, the lookup cannot find or compile the modules that live alongside the test script.
* Consequently the evaluator records `Module '…' could not be loaded for function '…'` for each attempted call, and the assertions fail when they expect successful evaluation.【7494f4†L63-L109】

### Recommended fix

1. Use the `init(ScriptFolder)` hook to compute a `file://` base URI for the `modules/` directory and cache it globally.
2. Wrap each query string in a helper that prepends `declare base-uri "file:///…/src/xpath/tests/";` (or another appropriate declaration) so that relative hints resolve to the packaged module files.
   * Alternatively, assign `xml.path = ScriptFolder .. 'modules/dummy.xq'` before calling `mtEvaluate()` so that `xp::Compile()` inherits a static base URI automatically.
3. Once the base URI is in place, the module cache will load, cache, and validate the supporting `.xq` files as designed, letting the positive-path assertions pass while still exercising the negative-path tests (namespace mismatch, circular dependencies, etc.).

