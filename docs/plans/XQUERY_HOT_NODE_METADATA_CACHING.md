# XQuery Hot Node Metadata Caching

**Status**: Proposed  
**Priority**: 4  
**Complexity**: Medium  
**Estimated Effort**: 4-6 days (one developer)  
**Risk Level**: Low

---

## Executive Summary

The XQuery evaluator still performs string-to-enum mapping for every binary and unary expression node at runtime. This plan introduces cached operator metadata during parsing so the evaluator can bypass repeated string comparisons without altering the existing dispatch table or abstract syntax tree layout. The proposal improves hot-path performance while keeping structural changes minimal and backward compatible.

---

## Objectives

1. Measure current dispatch overhead for operator-heavy workloads.
2. Extend `XPathNode` to store optional cached operator kinds.
3. Populate the cached metadata during parsing and when unit tests manufacture nodes directly.
4. Teach the evaluator to rely on the cached enums, falling back to legacy string mapping only when necessary.
5. Validate behaviour and performance through builds, tests, and micro-benchmarks.

---

## Scope and Constraints

- No introduction of `std::variant` or other intrusive AST storage mechanisms.
- No grammar changes; only metadata recorded alongside existing nodes.
- Preserve all public interfaces, serialisation routines, and cloning helpers.
- Maintain British English terminology across comments and documentation.

---

## Stage 0: Verification (0.5 day)

1. Instrument `XPathEvaluator::dispatch_metrics()` to capture counts during representative workloads (internal unit tests, QT3-derived suites, benchmark scripts).  
2. Profile operator-heavy queries with the existing benchmarking utilities to quantify the share of time spent on dispatch and string comparisons.  
3. Exit Criteria: Confirm that operator dispatch remains a measurable cost (≥3 % of total evaluation time for targeted workloads). If not, archive findings in this document and halt the plan.

---

## Stage 1: Parser Metadata Enrichment (1-2 days)

1. **Node Structure Updates** (`src/xquery/xquery.h`)  
   - Add optional fields for cached operator kinds: `std::optional<BinaryOperationKind> cached_binary_kind;` and `std::optional<UnaryOperationKind> cached_unary_kind;`.  
   - Provide accessor and mutator helpers that mirror existing naming conventions (`set_cached_binary_kind`, `get_cached_binary_kind`, etc.).  
   - Document the new fields inline, noting that they are hints and not required for correctness.

2. **Parser Population** (`src/xquery/parse/xquery_parser.cpp`)  
   - Introduce constexpr lookup tables translating operator tokens to `BinaryOperationKind`/`UnaryOperationKind`.  
   - Update `create_binary_op()` and `create_unary_op()` to populate the cached enums when constructing nodes.  
   - Ensure helper builders inside the parser (e.g. attribute tests, content predicates) also set the cached values.

3. **Test Fixtures and Utilities**  
   - Sweep unit tests, sample data builders, and any custom node factories to set cached kinds when they create operator nodes directly.  
   - Add a guard test that asserts parser-produced nodes always populate the caches for recognised operators.

---

## Stage 2: Evaluator Consumption (1-2 days)

1. **Evaluator Path** (`src/xquery/eval/eval_expression.cpp`)  
   - Modify `handle_binary_op()` to consult `Node->get_cached_binary_kind()` first, using the existing string-based `map_binary_operation()` only when the cache is missing.  
   - Apply the same logic to `handle_unary_op()`.  
   - Add counters (guarded by debug or trace builds) to record fallback usage so unexpected misses are easy to diagnose.

2. **Telemetry and Logging**  
   - When a fallback occurs, emit a trace log summarising the operator text to aid future clean-up efforts.  

3. **Documentation**  
   - Update evaluator header comments to explain the cached metadata flow.  
   - Expand any relevant developer notes under `src/xquery/eval/` to mention the new hints and fallback behaviour.

---

## Stage 3: Validation (1 day)

1. **Build & Test**  
   - `cmake --build build/agents --config FastBuild --target xquery --parallel`  
   - `cmake --install build/agents`  
   - `ctest --build-config FastBuild --test-dir build/agents -L xquery`

2. **Benchmarking**  
   - Re-run the Stage 0 workloads and benchmark scripts, comparing total runtime, instruction counts, and cache miss metrics.  
   - Target ≥1 % improvement on binary-heavy scenarios with no observable regressions elsewhere.  
   - Record fallback counter values; the expectation is near-zero under parser-generated workloads.

3. **Documentation Updates**  
   - Append the measurement results and any notable observations to this plan file.  
   - Update evaluator developer documentation (for example `src/xquery/eval/README.md` if present) with a short summary of the change and maintenance guidance.

---

## Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
| --- | --- | --- | --- |
| Legacy code constructs operator nodes without caches | Medium | Low | Retain string-based fallback and log occurrences to aid follow-up work. |
| Parser helper misses a new operator form | Low | Low | Add unit coverage and ensure constexpr lookup tables stay in sync with token definitions. |
| Performance gain is negligible | Medium | Low | If Stage 0 profiling shows dispatch cost below threshold, record findings and abort prior to code changes. |

---

## Exit Criteria

- Cached enums exist on `XPathNode` and are populated by the parser for all recognised operator tokens.  
- `handle_binary_op()` and `handle_unary_op()` prefer cached enums and only fall back to string comparisons when caches are absent.  
- All automated tests and benchmarks pass without regression.  
- Benchmark summary and fallback metrics are documented in this file, providing future maintainers with context and evidence of the optimisation’s value.

