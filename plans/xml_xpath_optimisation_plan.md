# XPath Optimisation Opportunities

This plan summarises optimisation opportunities in `src/xml/xpath/`, ordered from easiest wins to more complex refactors.

1. **Pre-size vectors during axis traversal**  
   Reserve capacity for node lists in `evaluate_child_axis`, `evaluate_descendant_axis`, and related helpers to avoid repeated reallocations during axis traversal.
2. **Reserve and reuse ID-cache storage**  
   Apply `reserve` to `id_lookup` and reuse traversal stacks in `find_tag_by_id` instead of rebuilding containers on each cache miss.
3. **Reduce allocations in `dispatch_axis`**  
   Pre-size the `matches` container based on child or attribute counts and eliminate redundant `push_back` operations for singleton results.
4. **Tighten namespace-axis bookkeeping**  
   Replace the map/set pair in `evaluate_namespace_axis` with a flat structure (sorted vector plus `std::unique`) and reuse `namespace_node_storage` objects to cut construction costs.
5. **Avoid string churn during tokenisation**  
   Let `XPathTokenizer` store substrings as `std::string_view` and defer copying until AST construction, trimming multiple allocations per token.
6. **Pre-compute concatenation sizes in string functions**  
   Sum operand lengths before building result strings (e.g., `concat`, `translate`) so `std::string::reserve` can prevent quadratic behaviour.
7. **Cache parsed ASTs across union branches**  
   Reuse token streams or parsed ASTs for each branch processed by `find_tag_enhanced_internal` instead of re-parsing identical expressions.
8. **Streamline step sequencing to reduce recursion overhead**  
   Convert `evaluate_step_sequence` to an iterative walker or reuse frame objects to reduce allocator pressure and recursion depth.
9. **Adopt a shared arena for transient XPath nodes/values**  
   Introduce an arena or object pool for temporary `XPathValue` objects and vectors created during predicate evaluation.
10. **Refactor union handling into parse phase**  
    Emit an explicit `Union` AST node rather than using `split_union_paths`, allowing evaluator-side optimisations and removing repeated string scans.
11. **Flatten function lookup tables**  
    Replace `std::map`-based lookups with `std::unordered_map` or sorted vectors plus binary search to speed up repeated function dispatch.
12. **Generalise document-order comparisons**  
    Cache ancestor chains or introduce lightweight iterators/spans in `build_ancestor_path`, reducing allocations during ordering checks in `normalise_node_set` and `is_before_in_document_order`.
