# Detailed XPath Optimization Implementation Plan

## Overview
This plan provides detailed implementation steps for the 12 XPath optimization opportunities identified in the basic plan, progressing from easiest wins to more complex refactors.

## Phase 1: Container Pre-sizing (Quick Wins)

### 1.1 Pre-size vectors during axis traversal
**Files:** `xpath_axis.cpp`, `xpath_axis.h`
**Impact:** Reduce reallocations during node collection
**Implementation:**
- Update `evaluate_child_axis()`: Add `reserve()` call based on child count from `ContextNode`
- Update `evaluate_descendant_axis()`: Add heuristic-based `reserve()` (e.g., depth * average_children)
- Update `evaluate_ancestor_axis()`: Reserve based on estimated tree depth
- Update `evaluate_following_sibling_axis()` and `evaluate_preceding_sibling_axis()`: Reserve based on sibling count
- Add helper method `estimate_result_size()` to predict capacity needs per axis type

### 1.2 Reserve and reuse ID-cache storage
**Files:** `xpath_axis.cpp`, `xpath_axis.h`
**Impact:** Reduce map reallocations and stack rebuilding
**Implementation:**
- Update `build_id_cache()`: Add `id_lookup.reserve()` with estimated node count
- Update `find_tag_by_id()`: Reuse static/member vector for traversal stack instead of local allocation
- Add cache size estimation based on document structure analysis

### 1.3 Reduce allocations in dispatch_axis
**Files:** `xpath_evaluator.cpp`, `xpath_evaluator.h`
**Impact:** Pre-size result containers to avoid push_back reallocations
**Implementation:**
- Update `dispatch_axis()`: Add capacity estimation logic based on axis type and context
- For child axis: Reserve based on `ContextNode->Child` count
- For attribute axis: Reserve based on `ContextNode->Attrib` count
- Eliminate redundant `push_back` operations for singleton results (self, parent axes)

## Phase 2: Namespace Optimization

### 2.1 Tighten namespace-axis bookkeeping
**Files:** `xpath_axis.cpp`, `xpath_axis.h`
**Impact:** Reduce container construction costs for namespace processing
**Implementation:**
- Replace map/set pair in `evaluate_namespace_axis()` with sorted vector approach
- Implement `collect_namespace_declarations()` using flat vector + `std::unique`
- Reuse `namespace_node_storage` objects between evaluations
- Add namespace node pooling mechanism

## Phase 3: Tokenization Improvements

### 3.1 Avoid string churn during tokenisation
**Files:** `xpath_parser.cpp`, `xpath_parser.h`
**Impact:** Reduce string allocations during token parsing
**Implementation:**
- Update `XPathTokenizer` to store `std::string_view` instead of `std::string` for tokens
- Defer string copying until AST construction in `XPathParser`
- Update token structures to support view-based parsing
- Implement string interning for frequently used identifiers

## Phase 4: String Function Optimization

### 4.1 Pre-compute concatenation sizes in string functions
**Files:** `xpath_functions.cpp`, `xpath_functions.h`
**Impact:** Prevent quadratic behavior in string operations
**Implementation:**
- Update `function_concat()`: Pre-calculate total length and use `std::string::reserve()`
- Update `function_translate()`: Pre-size result string based on input length
- Update `function_substring()`: Optimize memory allocation for result strings
- Add size estimation helpers for common string operations

## Phase 5: AST and Union Optimization

### 5.1 Cache parsed ASTs across union branches
**Files:** `xpath_evaluator.cpp`, `xpath_parser.cpp`
**Impact:** Avoid re-parsing identical expressions in unions
**Implementation:**
- Add AST caching mechanism in `SimpleXPathEvaluator`
- Update `find_tag_enhanced_internal()` to cache parsed ASTs per union branch
- Implement AST comparison for cache key generation
- Add cache invalidation strategy

### 5.2 Refactor union handling into parse phase
**Files:** `xpath_parser.cpp`, `xpath_ast.h`, `xpath_evaluator.cpp`
**Impact:** Move union splitting from evaluation to parsing phase
**Implementation:**
- Add explicit `Union` AST node type to `xpath_ast.h`
- Update `XPathParser` to emit `Union` nodes instead of using `split_union_paths`
- Refactor evaluator to handle `Union` nodes directly
- Remove string-based union splitting from evaluation phase

## Phase 6: Advanced Optimizations

### 6.1 Streamline step sequencing to reduce recursion overhead
**Files:** `xpath_evaluator.cpp`, `xpath_evaluator.h`
**Impact:** Reduce stack pressure and allocator churn
**Implementation:**
- Convert `evaluate_step_sequence()` from recursive to iterative approach
- Implement frame object pooling for step evaluation contexts
- Add iterative step walker with explicit stack management
- Optimize context switching between steps

### 6.2 Flatten function lookup tables
**Files:** `xpath_functions.cpp`, `xpath_functions.h`
**Impact:** Speed up function dispatch
**Implementation:**
- Replace `std::map` with `std::unordered_map` in `XPathFunctionLibrary`
- Alternative: Use sorted vector + binary search for small function sets
- Add function pointer caching for frequently used functions
- Implement compile-time function registration where possible

## Phase 7: Memory Management Optimization

### 7.1 Adopt shared arena for transient XPath nodes/values
**Files:** All XPath files, new `xpath_arena.h`
**Impact:** Reduce allocator pressure during evaluation
**Implementation:**
- Create `XPathArena` class for temporary object allocation
- Implement object pooling for `XPathValue` instances
- Add arena-based allocation for temporary vectors during predicate evaluation
- Integrate arena lifecycle with evaluation phases

## Phase 8: Document Order Optimization

### 8.1 Generalise document-order comparisons
**Files:** `xpath_axis.cpp`, `xpath_evaluator.cpp`
**Impact:** Reduce allocations during node ordering
**Implementation:**
- Cache ancestor chains in `build_ancestor_path()` using object pooling
- Implement lightweight iterator/span approach for document order traversal
- Update `normalise_node_set()` and `is_before_in_document_order()` to use cached data
- Add document order comparison caching for frequently compared nodes

## Testing Strategy

### Performance Testing
- Create benchmark suite measuring before/after performance for each optimization
- Use existing XPath test files in `src/xml/tests/` as performance baseline
- Add memory usage profiling to measure allocation reduction

### Regression Testing
- Ensure all existing XPath tests continue to pass
- Add specific tests for edge cases in optimized code paths
- Test memory cleanup and lifecycle management

## Implementation Timeline

**Phase 1 (Week 1):** Container pre-sizing optimizations (items 1.1-1.3)
**Phase 2 (Week 2):** Namespace optimization (item 2.1)
**Phase 3 (Week 3):** Tokenization improvements (item 3.1)
**Phase 4 (Week 4):** String function optimization (item 4.1)
**Phase 5 (Week 5-6):** AST and union optimization (items 5.1-5.2)
**Phase 6 (Week 7-8):** Advanced optimizations (items 6.1-6.2)
**Phase 7 (Week 9):** Memory management optimization (item 7.1)
**Phase 8 (Week 10):** Document order optimization (item 8.1)

Each phase will include implementation, testing, and performance measurement to validate improvements.