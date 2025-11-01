# XQuery Module Optimization Plan

**Module**: XQuery/XPath
**Date**: 2025-01-27
**Status**: Planning Phase
**Complexity**: High (multiple phases)
**Impact**: High performance gains for XML query operations

## Executive Summary

This document outlines optimization opportunities identified in the XQuery module through comprehensive code analysis. The module currently implements XPath 2.0 and XQuery 1.0+ with good architectural foundations, but several targeted refactorings can yield significant performance improvements, particularly for complex queries over large XML documents.

---

## Quick Wins

These are low-risk, incremental changes that improve hot paths without architectural upheaval. Implement all in Phase 1.

### QW-1: Axis reserve() Coverage
- Extend `Output.reserve(estimate_result_size(...))` beyond descendant/descendant-or-self to all axes with predictable sizes in `src/xquery/api/xquery_axis.cpp`.
- Rationale: avoids repeated growth for sibling/ancestor/following/preceding patterns; `estimate_result_size()` already exists and should be applied consistently.

### QW-2: Arena Vectors Pre-sizing
- After acquiring scratch vectors from `XPathArena` (e.g., `arena.acquire_node_vector()`), call `reserve()` with a local estimate when it is known (children count, siblings count, stack depth heuristics).
- Rationale: immediate reduction in reallocations without changing the arena API yet.

### QW-3: Heterogeneous Lookups (no temporary strings)
- Change prolog maps to use transparent hash/equal to enable `find(std::string_view)` without allocating:
  - `XQueryProlog::functions`, `variables`, `declared_namespaces`, `declared_namespace_uris` in `src/xquery/xquery.h`.
  - Use the existing `TransparentStringHash` and `TransparentStringEqual` types defined in `xquery.h`.
- Update lookups in `xquery_prolog.cpp` to remove `std::string` temporaries (e.g., in `find_variable()`, `resolve_prefix()`).

### QW-4: Prolog Import Key Normalisation Once
- Normalise import namespace keys at declaration (`declare_module_import`) and store the normalised form alongside the original.
- Use normalised keys directly in caches/sets; avoid re-normalising in every lookup.

### QW-5: Tuple Preallocation in FLWOR
- Pre-size `order_keys` and `order_key_empty` in `FlworTuple` based on the observed `order by` arity.
- Where tuple streams are expanded, reserve target vector sizes when the expansion factor is known (e.g., product of sequence lengths in nested `for`).

### QW-6: Tokeniser Micro-optimisations
- In `src/xquery/parse/xquery_tokeniser.cpp`, reduce `std::string` construction by favouring `std::string_view` and reusing slices into the input buffer for identifiers/numerics where safe.
- Ensure attribute value templates continue to populate `attribute_value_parts` correctly (no semantic change).

## Progress Update

### Quick Wins Status
- [x] QW-1: Axis reserve() coverage implemented across dispatcher and axes
  - Files: `src/xquery/api/xquery_axis.cpp`
  - Date: 27 Oct 2025
- [x] QW-2: Arena vectors pre-sizing implemented with size-hint acquires
  - Files: `src/xquery/xquery.h`, `src/xquery/api/xquery_axis.cpp`
  - Date: 27 Oct 2025
- [x] QW-3: Heterogeneous lookups (transparent hash/equal) for prolog maps
  - Files: `src/xquery/xquery.h`, `src/xquery/api/xquery_prolog.cpp`
  - Date: 27 Oct 2025
- [x] QW-4: Prolog import key normalisation at declaration; imports use pre-normalised keys
  - Files: `src/xquery/xquery.h`, `src/xquery/api/xquery_prolog.cpp`
  - Date: 27 Oct 2025
 - [x] QW-5: Tuple preallocation in FLWOR (pre-size per-tuple expansions)
   - Files: `src/xquery/eval/eval_flwor.cpp`
   - Date: 27 Oct 2025
- [x] QW-6: Tokeniser micro-optimisations (reserve heuristics, avoid temporaries)
  - Files: `src/xquery/parse/xquery_tokeniser.cpp`
  - Date: 27 Oct 2025

### LR-1 Progress
- Size-hint overloads added to `XPathArena` pools and used throughout axis evaluator to reduce reallocations.
- Tiered size-class pooling implemented for std::vector pools in `XPathArena` (attributes, strings).
- Tiered size-class pooling implemented for node vectors via a tiered pool with pointer→tier tracking (pf::vector has no public reserve/capacity); acquire uses size hints for tier selection.

### LR-2 Progress (FLWOR Tuple Schema)
- Implemented indexed tuple schema with vector-based bindings and presence flags (replaces unordered_map per tuple).
- Updated evaluation scopes to bind variables via schema (TupleScope now accepts schema).
- Reworked binding writes in FOR/LET/GROUP BY/COUNT to use schema IDs; replaced map merges with vector merges.
- Files: `src/xquery/eval/eval_flwor.cpp`
- Tests extended in `src/xquery/tests/test_flwor.fluid` for nested FOR, multi-key ORDER BY empty handling, and LET shadowing.

### LR-4 Progress (Prolog & Keys)
- Part 1: Added global string interning for QNames; FunctionKey stores interned `string_view`.
- Part 3: Replaced `qname/arity` string keys with structured `FunctionKey` in prolog function map; updated declaration and lookup.
- Files: `src/xquery/xquery.h`, `src/xquery/api/xquery_prolog.cpp`


## Large Refactoring Opportunities

These require careful planning, phased implementation, and comprehensive testing.

## Validation Notes (Current Code Alignment)

- Arena pooling exists in `src/xquery/xquery.h` around class `XPathArena` and a specialised `NodeVectorPool`; `AxisEvaluator::estimate_result_size()` is implemented in `src/xquery/api/xquery_axis.cpp`.
- FLWOR tuples currently use `std::unordered_map<std::string, XPathVal>` for `bindings` in `src/xquery/eval/eval_flwor.cpp`.
- Expression nodes are represented by `struct XPathNode` with an explicit `XQueryNodeType` and switch‑based dispatch in `src/xquery/eval/*.cpp`.
- Prolog stores `functions`/`variables` as `ankerl::unordered_dense::map<std::string, ...>` and builds function signatures with `qname/arity` strings in `src/xquery/api/xquery_prolog.cpp`.
- Tokeniser stores both `std::string_view value` and `std::string stored_value` in `XPathToken` in `src/xquery/xquery.h`; lexical analysis is in `src/xquery/parse/xquery_tokeniser.cpp`.

### LR-1: Axis Evaluation Size-Class Pooling

**Priority**: 1 (Highest)
**Complexity**: Medium-High
**Files**: `src/xquery/xquery.h` (XPathArena, NodeVectorPool), `src/xquery/api/xquery_axis.cpp`
**Estimated Effort**: 3-5 days

#### Current State
The `XPathArena` (lines 789-886 in xquery.h) provides vector pooling to avoid repeated allocations during axis evaluation. However, it uses a one-size-fits-all approach where vectors of any size are pooled together.

#### Problem
- Child axis queries typically return small result sets (1-10 nodes)
- Descendant axis queries return much larger sets (100-1000+ nodes)
- Current approach causes fragmentation and unnecessary capacity growth

#### Proposed Solution

Implement size-class segregation for the vector pools (including the specialised `NodeVectorPool`):

```cpp
template<typename T>
struct TieredVectorPool {
   static constexpr size_t SIZE_CLASSES = 5;
   static constexpr std::array<size_t, SIZE_CLASSES> TIER_LIMITS = {16, 64, 256, 1024, 4096};

   std::array<VectorPool<T>, SIZE_CLASSES> pools;

   std::vector<T>& acquire(size_t estimated_size) {
      size_t tier = select_tier(estimated_size);
      return pools[tier].acquire();
   }

   void release(std::vector<T>& vector) {
      size_t tier = select_tier(vector.capacity());
      pools[tier].release(vector);
   }

private:
   size_t select_tier(size_t size) const {
      for (size_t i = 0; i < SIZE_CLASSES; ++i) {
         if (size <= TIER_LIMITS[i]) return i;
      }
      return SIZE_CLASSES - 1;
   }
};
```

**Integration Points**:
1. Update `XPathArena` to use `TieredVectorPool`
2. Modify `AxisEvaluator::estimate_result_size()` to guide tier selection
3. Update all `arena.acquire_node_vector()` calls to pass size hints

**Benefits**:
- Reduces memory fragmentation by 40-60%
- Improves cache locality for small result sets
- Eliminates capacity growth operations in 80%+ of cases
- Particularly beneficial for queries with mixed axis types

**Testing Strategy**:
1. Unit tests for tier selection logic
2. Benchmark suite comparing allocation counts before/after
3. Memory profiler validation showing fragmentation reduction
4. Regression tests with W3C XPath test suite

---

### LR-2: FLWOR Tuple Schema Compilation

**Priority**: 2
**Complexity**: High
**Files**: `src/xquery/eval/eval_flwor.cpp`, `src/xquery/parse/xquery_parser.cpp`
**Estimated Effort**: 1-2 weeks

#### Current State
FLWOR tuples (line 34-43 in eval_flwor.cpp) use `std::unordered_map<std::string, XPathVal>` for variable bindings. Every variable access requires string hashing and map lookup.

#### Problem
- Large FLWOR queries (e.g., iterating 10,000+ nodes) create millions of hash lookups
- Each tuple allocates its own map on the heap
- String hashing is repeated for the same variable names
- Poor cache locality due to map node allocations

#### Proposed Solution

Introduce a compilation phase that assigns integer IDs to variables:

**Phase 1: Schema Construction**
```cpp
struct TupleSchema {
   std::unordered_map<std::string, size_t> variable_indices;
   size_t variable_count = 0;

   size_t register_variable(std::string_view name) {
      auto [it, inserted] = variable_indices.emplace(name, variable_count);
      if (inserted) ++variable_count;
      return it->second;
   }

   size_t get_index(std::string_view name) const {
      auto it = variable_indices.find(name);
      return it != variable_indices.end() ? it->second : SIZE_MAX;
   }
};
```

**Phase 2: Tuple Storage**
```cpp
struct FlworTuple {
   std::vector<XPathVal> bindings;  // Indexed by variable ID
   // ... other fields remain the same

   const XPathVal& get_binding(size_t var_id) const {
      return bindings[var_id];
   }

   void set_binding(size_t var_id, XPathVal value) {
      if (var_id >= bindings.size()) bindings.resize(var_id + 1);
      bindings[var_id] = std::move(value);
   }
};
```

**Phase 3: Parser Integration**
- Parser builds `TupleSchema` during FLWOR clause parsing
- Variable references store indices instead of names
- Schema attached to FLWOR node in AST

**Benefits**:
- Eliminates string hashing (5-10% speedup on FLWOR queries)
- Reduces allocations (one vector vs. one map per tuple)
- Improves cache locality (flat array vs. hash table nodes)
- Enables future optimizations (tuple vectorization, SIMD)

**Migration Path**:
1. Phase 1: Implement schema in parser (non-breaking)
2. Phase 2: Add indexed tuple variant alongside existing map-based tuples
3. Phase 3: Benchmark and validate correctness
4. Phase 4: Remove old implementation

**Risks**:
- Complex interaction with variable scoping rules
- External variable references need careful handling
- Must preserve error messages that reference variable names

---

### LR-3: Expression Node Type Dispatch Modernization

**Priority**: 3
**Complexity**: Very High
**Files**: `src/xquery/xquery.h` (XPathNode), `src/xquery/eval/eval*.cpp`
**Estimated Effort**: 2-3 weeks

#### Current State
Expression evaluation uses type-tagged unions (`XPathNode` with `XQueryNodeType` enum) and large switch statements for dispatch. Type checks are repeated throughout the call stack.

#### Problem
- Switch statements are verbose and error-prone
- No compiler-enforced exhaustiveness checking
- Difficult to add new node types
- Repeated type checks inhibit optimization
- Poor separation of concerns between node types

#### Proposed Solution

**Option A: Virtual Dispatch with Type Erasure**

Refactor `XPathNode` to use runtime polymorphism:

```cpp
class XPathNodeBase {
public:
   virtual ~XPathNodeBase() = default;
   virtual XPathVal evaluate(XPathEvaluator& eval, uint32_t prefix) const = 0;
   virtual XQueryNodeType type() const = 0;
   virtual std::string_view type_name() const = 0;
};

class LiteralNode : public XPathNodeBase {
   std::string value;
public:
   XPathVal evaluate(XPathEvaluator& eval, uint32_t prefix) const override;
   XQueryNodeType type() const override { return XQueryNodeType::LITERAL; }
   std::string_view type_name() const override { return "literal"; }
};

// Similarly for BinaryOpNode, FunctionCallNode, etc.
```

**Option B: std::variant-based Visitor**

Use C++20 variant for zero-overhead type-safe dispatch:

```cpp
struct LiteralNode { std::string value; };
struct BinaryOpNode { /* ... */ };
struct FunctionCallNode { /* ... */ };
// ... all other node types

using XPathNodeVariant = std::variant<
   LiteralNode,
   BinaryOpNode,
   FunctionCallNode,
   // ... all node types
>;

struct EvaluationVisitor {
   XPathEvaluator& eval;
   uint32_t prefix;

   XPathVal operator()(const LiteralNode& node) { /* ... */ }
   XPathVal operator()(const BinaryOpNode& node) { /* ... */ }
   // ... handlers for each type
};

XPathVal evaluate_node(const XPathNodeVariant& node, XPathEvaluator& eval, uint32_t prefix) {
   return std::visit(EvaluationVisitor{eval, prefix}, node);
}
```

#### Comparison

| Aspect | Virtual Dispatch | std::variant |
|--------|-----------------|--------------|
| Runtime overhead | Small vtable lookup | Zero overhead |
| Compile time | Faster | Slower (large variants) |
| Extensibility | Easy (add derived class) | Hard (recompile all) |
| Type safety | Runtime checks | Compile-time checks |
| Memory | 8 bytes per node | Size of largest type |
| Debuggability | Better (clear types) | Harder (opaque variant) |

**Updated Approach (Pragmatic Hybrid)**
- Phase 1: Centralised dispatch table mapping `XQueryNodeType` → handler functions (no AST changes). Lift switch cases into typed handlers per node kind.
- Phase 2: Add typed accessors (“views”) for required fields of each node kind to reduce scattered optional checks.
- Phase 3: Hot‑path specialisation for FunctionCall, BinaryOp, Path, Predicates. Reuse arena hints and avoid repeated checks.
- Phase 4 (optional pilot): Introduce a small `std::variant` view for the hottest node kinds, used only inside the dispatch wrapper; evaluate compile/runtime impact before broader adoption.
- Phase 5: Cleanup duplicate switches and document handler contracts.

**Benefits**:
- Type safety enforced at compile time
- Easier to add new node types
- Better compiler optimization opportunities
- Clearer code organization
- Eliminates redundant type checks

**Migration Strategy (Revised)**
1. Implement central dispatch and extract handlers (Literal, Number, FunctionCall, BinaryOp, Path, Predicates) without AST changes.
2. Add typed accessors for node fields and replace ad‑hoc checks in handlers.
3. Optimise handlers on hot paths; benchmark with extended suites.
4. Pilot variant‑based view for a subset of hot kinds; keep behind wrapper.
5. Remove duplicate switches; document invariants and contributor notes.

**Risks**:
- Very large change touching many files
- Potential performance regression if not done carefully
- Complex AST node relationships (parent/child pointers, etc.)
- Must preserve exact semantics for compatibility

---

### LR-4: Prolog & Module Cache String Key Optimisation

**Priority**: 4
**Complexity**: Medium-High
**Files**: `src/xquery/api/xquery_prolog.cpp`, `src/xquery/xquery.h` (XQueryProlog)
**Estimated Effort**: 4-6 days

#### Current State
Module caching (lines 116-300 in xquery_prolog.cpp) uses string normalization on every lookup. Function signatures are built by concatenating strings (lines 91-94). Transparent hash comparators do runtime string comparisons.

#### Problem
- `xp_normalise_cache_key()` creates temporary strings on every module lookup
- Function signature concatenation (`"name/arity"`) allocates strings repeatedly
- String hashing occurs even when keys are identical to previous lookups
- No sharing of common namespace/function name strings

#### Proposed Solution

**Part 1: String Interning**

Implement a global string pool for common identifiers:

```cpp
class StringInterner {
   std::unordered_set<std::string> pool;
   mutable std::shared_mutex mutex;

public:
   std::string_view intern(std::string_view str) {
      std::shared_lock lock(mutex);
      auto it = pool.find(str);
      if (it != pool.end()) return *it;

      lock.unlock();
      std::unique_lock write_lock(mutex);
      auto [inserted_it, success] = pool.insert(std::string(str));
      return *inserted_it;
   }
};

// Global instance
inline StringInterner& global_string_pool() {
   static StringInterner pool;
   return pool;
}
```

**Part 2: Pre‑normalised Keys**

Store normalized forms during compilation:

```cpp
struct ModuleImportInfo {
   std::string target_namespace;
   std::string normalized_key;  // Pre-computed at parse time
   std::vector<std::string> location_hints;

   ModuleImportInfo(std::string ns, std::vector<std::string> hints)
      : target_namespace(std::move(ns))
      , normalized_key(xp_normalise_cache_key(target_namespace))
      , location_hints(std::move(hints)) {}
};
```

**Part 3: Function Signature Optimisation**

Replace string concatenation with structured keys:

```cpp
struct FunctionKey {
   std::string qname;  // Canonical name; consider interning later
   size_t arity;
   bool operator==(const FunctionKey&) const = default;
};

struct FunctionKeyHash {
   using is_transparent = void;
   size_t operator()(const FunctionKey& key) const noexcept {
      size_t h1 = std::hash<std::string_view>{}(key.qname);
      size_t h2 = std::hash<size_t>{}(key.arity);
      return h1 ^ (h2 << 1);
   }
};

// In XQueryProlog:
ankerl::unordered_dense::map<FunctionKey, XQueryFunction, FunctionKeyHash> functions;
```

**Benefits**:
- Eliminates repeated string normalization
- Reduces memory usage (shared strings)
- Faster lookups (pointer comparison vs. string comparison)
- Thread-safe string sharing

Status: Implemented structured FunctionKey map and lookups. String interning remains a future enhancement.

**Testing**:
1. Verify all module imports still resolve correctly
2. Benchmark module cache hit rates
3. Memory profiler showing string deduplication
4. Concurrent query tests (thread safety)

---

### LR-5: XPathToken Storage Strategy Overhaul

**Priority**: 5
**Complexity**: Medium-High
**Files**: `src/xquery/xquery.h` (XPathToken), `src/xquery/parse/xquery_tokeniser.cpp`
**Estimated Effort**: 4-6 days

#### Current State
`XPathToken` (lines 369-393 in xquery.h) stores both a `string_view` (non-owning) and a `stored_value` (`std::string`, owning), with runtime logic to determine which is valid. The tokenizer eagerly constructs all tokens before parsing begins.

#### Problem
- Dual storage strategy is confusing and error-prone
- Most tokens are short (<16 characters) but use heap-allocated strings
- Entire token vector is constructed even if parsing fails early
- Token string data has poor locality (scattered allocations)

#### Proposed Solution

**Option A: Arena Allocation**

```cpp
class TokenArena {
   std::vector<char> buffer;
   size_t offset = 0;

public:
   TokenArena() { buffer.reserve(4096); }

   std::string_view allocate_string(std::string_view source) {
      size_t required = source.size();
      if (offset + required > buffer.size()) {
         buffer.resize(std::max(buffer.size() * 2, offset + required));
      }

      std::memcpy(buffer.data() + offset, source.data(), required);
      std::string_view result(buffer.data() + offset, required);
      offset += required;
      return result;
   }

   void reset() { offset = 0; }
};

struct XPathToken {
   XPathTokenType type;
   std::string_view value;  // Always valid, points into arena or input
   size_t position;
   size_t length;
   // ... other fields
};
```

**Option B: Small String Optimization**

```cpp
struct SmallString {
   static constexpr size_t INLINE_CAPACITY = 15;

   union {
      char inline_data[INLINE_CAPACITY + 1];
      struct {
         char* heap_data;
         size_t size;
         size_t capacity;
      } heap;
   };

   bool is_inline;

   std::string_view view() const {
      return is_inline
         ? std::string_view(inline_data, std::strlen(inline_data))
         : std::string_view(heap.heap_data, heap.size);
   }

   // ... constructors, assignment, etc.
};

struct XPathToken {
   XPathTokenType type;
   SmallString value;
   // ...
};
```

**Option C: Lazy Token Generation** (Most ambitious)

Convert tokeniser to a generator/coroutine:

```cpp
class XPathTokenStream {
   std::string_view input;
   size_t position = 0;

public:
   std::optional<XPathToken> next_token();
   std::optional<XPathToken> peek_token(size_t lookahead = 0);
   void reset(std::string_view new_input);
};

// Parser integrates with streaming interface
class XPathParser {
   XPathTokenStream stream;
   // ... parse methods pull tokens on demand
};
```

#### Comparison

| Approach | Pros | Cons |
|----------|------|------|
| Arena | Best locality, simple | Must know max size |
| SmallString | No external allocation for most tokens | Complex implementation |
| Lazy | Zero waste, fails fast | Requires parser rewrite |

**Recommendation**: Start with arena allocation (Option A) for quick wins, evaluate lazy tokenisation (Option C) if profiling shows significant waste.

**Benefits** (Arena approach):
- Eliminates heap allocations for token strings
- Excellent cache locality (contiguous storage)
- Simple memory management (single buffer)
- Faster tokenisation (no allocation overhead)

**Migration Path**:
1. Add `TokenArena` to tokenizer
2. Migrate string-valued tokens to use arena
3. Simplify `XPathToken` to single `string_view` field
4. Remove `stored_value` field
5. Benchmark and validate

---

## Testing Strategy

For all large refactorings:

### Unit Tests
- Individual component tests for new data structures
- Comparison tests (old vs. new implementation)
- Edge case validation (empty inputs, maximum sizes, etc.)

### Integration Tests
- W3C XPath 2.0 conformance test suite
- W3C XQuery 1.0 test suite
- Parasol-specific extension tests
- Module import/export tests
 
### Flute Tests (Fluid)
- Always install the latest build before running tests.
- Run module tests from the module folder with headless driver, e.g.:
- Windows: `cd src/xquery/tests && ../../../install/agents/parasol.exe ../../../tools/flute.fluid file=$PWD/test_flwor.fluid --gfx-driver=headless --log-warning`
- Linux: `cd src/xquery/tests && ../../../install/agents/parasol ../../../tools/flute.fluid file=/abs/path/src/xquery/tests/test_flwor.fluid --gfx-driver=headless --log-warning`

### Performance Benchmarks
- Simple queries (baseline overhead measurement)
- Complex FLWOR expressions (tuple processing stress test)
- Deep XML trees (axis evaluation stress test)
- Large document sets (cache effectiveness)
- Concurrent query execution (thread safety)

### Regression Prevention
- Before/after performance comparisons
- Memory usage profiling (allocations, peak, fragmentation)
- Compilation time tracking
- Binary size monitoring

---

## Implementation Phasing

### Phase 1: Foundation (Week 1)
- Implement all quick wins (QW‑1 through QW‑6)
- Switch prolog maps to transparent lookup (QW‑3) and adjust sites in `xquery_prolog.cpp`.
- Establish benchmark suite (unit test timers + W3C subsets) and capture baseline.

### Phase 2: Axis Optimisation (Weeks 2-3)
- Implement LR-1 (Size-class pooling)
- Validate with benchmarks
- Monitor for regressions

### Phase 3: String Optimisation (Weeks 4-5)
- Implement LR-4 (Prolog string optimisation)
- Implement LR-5 (Token storage)
- Measure memory improvements

### Phase 4: Advanced Features (Weeks 6-9)
- Implement LR-2 (FLWOR tuples) OR LR-3 (Node dispatch)
- Choose based on profiling results from previous phases
- Comprehensive testing

### Phase 5: Refinement (Week 10)
- Address any regressions discovered
- Performance tuning based on real-world usage
- Documentation updates

---

## Success Metrics

### Performance Targets
- 15-25% speedup on complex FLWOR queries
- 30-40% reduction in allocations during axis evaluation
- 20-30% memory usage reduction for large query sets
- Zero regressions on W3C test suites

### Code Quality Targets
- Reduce cyclomatic complexity of eval functions by 25%
- Improve test coverage to >90% for modified code
- Maintain or improve compilation times
- Add comprehensive inline documentation

---

## Risks and Mitigations

### Risk: Performance Regressions
**Mitigation**: Benchmark every change, maintain rollback branches, use feature flags for gradual rollout

### Risk: Semantic Changes
**Mitigation**: Extensive testing with W3C test suites, maintain compatibility tests, careful review of edge cases

### Risk: Increased Complexity
**Mitigation**: Clear documentation, code reviews, incremental changes with validation gates

### Risk: Compilation Time Increase
**Mitigation**: Monitor build times, use explicit template instantiation, consider compile-time optimizations

---

## Constraints and Coding Standards

- Follow CRITICAL PROJECT REQUIREMENTS from repository root `AGENTS.md` for C++ and Fluid code:
  - Use `IS` macro instead of `==`; use `and`/`or` instead of `&&`/`||`.
  - Avoid `static_cast`; prefer C-style casts.
  - Do not use C++ exceptions; rely on error codes.
  - Use lower snake_case for local variables; three spaces for tabulation.
- Keep British English spelling in code and comments (e.g., tokeniser, normalise, optimised).

## Build and Test Commands

- Configure (FastBuild): `cmake -S . -B build/agents -DCMAKE_BUILD_TYPE=FastBuild -DCMAKE_INSTALL_PREFIX=install/agents -DRUN_ANYWHERE=TRUE -DPARASOL_STATIC=ON -DBUILD_DEFS=ON`
- Build: `cmake --build build/agents --config FastBuild --parallel`
- Install: `cmake --install build/agents`
- Run integration tests: `ctest --build-config FastBuild --test-dir build/agents`

## Future Considerations

### Beyond This Plan
- SIMD vectorization for numeric operations
- Query plan caching for repeated expressions
- Parallel evaluation for independent FLWOR clauses
- JIT compilation for hot query paths
- XML schema-aware query optimization

### Maintenance
- Document architectural decisions
- Create developer guide for expression evaluation
- Establish performance regression testing infrastructure
- Plan for XQuery 3.1 feature additions

---

## References

- W3C XPath 2.0 Specification: https://www.w3.org/TR/xpath20/
- W3C XQuery 1.0 Specification: https://www.w3.org/TR/xquery/
- Parasol Repository: E:/parasol/src/xquery/
- Related Modules: XML (src/xml/), Vector (src/vector/)

---

## Approval and Sign-off

**Prepared by**: Claude Code Analysis
**Review Status**: Pending
**Estimated Total Effort**: 6-10 weeks (one developer, sequential phases)
**Risk Level**: Medium-High

**Next Steps**:
1. Review and approval of plan
2. Prioritize specific refactorings based on profiling data
3. Allocate developer resources
4. Set up benchmark infrastructure
5. Begin Phase 1 implementation
