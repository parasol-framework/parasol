# LR-3: Expression Node Dispatch Modernisation

**Status**: Planning
**Priority**: 3
**Complexity**: Very High
**Estimated Effort**: 2-3 weeks (one developer, phased implementation)
**Risk Level**: High

---

## Executive Summary

This document provides a comprehensive, step-by-step plan for modernising the XPath/XQuery expression node dispatch system in `src/xquery/eval/eval_expression.cpp`. The current implementation uses a hybrid of switch statements and if-statement chains, resulting in a 2325-line monolithic function that is difficult to maintain, extend, and optimise.

The refactoring follows a **pragmatic hybrid approach** that:
1. Centralises dispatch logic without requiring AST restructuring
2. Extracts type-specific handlers into dedicated functions
3. Adds typed accessor patterns to reduce field checking overhead
4. Enables compiler optimisations through better code organisation
5. Preserves exact semantics for W3C XQuery compliance

---

## Progress Checklist

- [x] Phase 1: Centralised Dispatch Table
  - [x] Step 1.1: Create Handler Type Alias
  - [x] Step 1.2: Declare Handler Methods
  - [x] Step 1.3: Create Dispatch Table *(initial table added with high-impact node types; remaining entries will be populated as handlers are extracted)*
  - [x] Step 1.4: Refactor evaluate_expression() to Use Dispatch Table
  - [x] Step 1.5: Extract Inline Handlers
  - [x] Step 1.6: Testing Strategy for Phase 1
- [ ] Phase 2: Typed Accessors for Node Fields
  - [x] Step 2.1: Design Accessor Pattern
  - [x] Step 2.2: Refactor Handlers to Use Accessors
  - [x] Step 2.3: Testing After Phase 2
- [x] Phase 3: Hot-Path Specialisation
  - [x] Step 3.1: Identify Hot Paths via Profiling
  - [x] Step 3.2: Create Fast-Path Dispatch
  - [x] Step 3.3: Specialise Binary Operator Handler
- [ ] Phase 4: Optional Variant-Based View (Deferred; proceed only if Phase 3 speedup < 5%)
  - [ ] Decision Gate: Confirm need to pursue Phase 4
  - [ ] If pursued: Create HotNodeVariant for hottest node types
  - [ ] If pursued: Add std::variant field to XPathNode
  - [ ] If pursued: Update parser to populate variant for hot types
  - [ ] If pursued: Modify evaluate_expression() to check variant before dispatch
  - [ ] If pursued: Benchmark and justify added complexity
- [ ] Phase 5: Cleanup and Documentation
  - [ ] Step 5.1: Remove Duplicate Switches
  - [ ] Step 5.2: Document Handler Contracts
  - [ ] Step 5.3: Add Contributor Notes
  - [ ] Step 5.4: Update Optimisation Plan

---

### Phase 1 Progress Update (2025-10-31)

- `evaluate_expression()` now dispatches exclusively via the `NODE_HANDLERS` map, logging a trace entry when encountering an unsupported node type instead of falling back to the legacy switch/if chain.
- Extracted dedicated handlers for every previously inline expression (type checks, quantified/filter/path nodes, and the binary operator suite), reusing the existing helper utilities (`evaluate_quantified_binding_recursive`, `compare_xpath_relational`, `evaluate_predicate`, constructor evaluators).
- Verified the refactor by rebuilding the XQuery target with `cmake --build build/agents --config FastBuild --target xquery --parallel`.
- Completed the Phase 1 testing pass by rebuilding the Release configuration, installing the artefacts, and running `ctest --build-config Release --test-dir build/agents -L xquery` (20/20 suites passing).

### Phase 2 Progress Update (2025-11-01)

- Added reusable accessors on `XPathNode` (`get_child_safe`, `get_value_view`, `get_constructor_info`, `child_is_type`, etc.) and migrated all expression handlers to the new helpers.
- Converted handler loops to rely on safe child access and string views, eliminating ad-hoc `child_count()` checks and raw `value` references.
- Updated constructor handlers and binding logic to use accessor-backed lookups, reducing optional field guard boilerplate.
- Rebuilt the `xquery` target in FastBuild mode and reinstalled artefacts before executing `ctest --build-config FastBuild --test-dir build/agents -L xquery` (20/20 passing) to validate the accessor sweep.

### Phase 3 Progress Update (2025-11-01)

- Instrumented `XPathEvaluator` with a per-node dispatch counter array and exposed reset/snapshot helpers to support profiling runs.
- Added a fast-path switch in `evaluate_expression()` for the hottest node kinds (binary/unary operators, function calls, variable references, literals, numbers) ahead of the dispatch map lookup.
- Split `handle_binary_op()` into five focused helpers (`handle_binary_logical`, `handle_binary_comparison`, `handle_binary_arithmetic`, `handle_binary_sequence`, `handle_binary_set_ops`) and extended `BinaryOperationKind` to differentiate value vs. general comparisons.
- Executed the XQuery-labelled CTest suite under the new instrumentation to confirm the hot-path cases are exercised and that the refactored handlers preserve semantics (20/20 passing under FastBuild).

### Testing Notes / Gaps

* Release build/install and the full `xquery` CTest label suite (20 tests) completed successfully; FastBuild remains available for quick recompiles during later phases.
* Edge cases worth re-verifying:
  - Typeswitch clauses that bind variables (ensure the guards added around case/default bindings still match expectations).
  - Quantified expressions with large binding sequences (performance).
  - Binary comparison semantics for mixed node/scalar operands—especially the “lt/le/gt/ge” textual operators.
  - Capture dispatch counter snapshots on the W3C QT3 corpus to quantify hot-path frequency before deciding on Phase 4 variant work.

## Current State Analysis

### File Structure
- **Primary file**: `src/xquery/eval/eval_expression.cpp` (2325 lines)
- **Core function**: `XPathEvaluator::evaluate_expression()` (lines 942-2325, ~1383 lines)
- **Node definition**: `struct XPathNode` in `src/xquery/xquery.h` (lines 356-412)
- **Node types**: 51 enum values defined in `src/xquery/xquery.fdl` (lines 7-75)

### Current Dispatch Pattern

The `evaluate_expression()` function uses three dispatch mechanisms:

#### 1. Switch Statement (lines 1078-1632)
Handles 19 node types with individual case blocks:
- `EMPTY_SEQUENCE` (line 1079) - Returns empty node-set
- `NUMBER` (line 1083) - Parses double value
- `LITERAL`, `STRING` (lines 1089-1090) - Returns string value
- `DIRECT_ELEMENT_CONSTRUCTOR` (line 1093) - Delegates to helper
- `COMPUTED_ELEMENT_CONSTRUCTOR` (line 1096) - Delegates to helper
- `COMPUTED_ATTRIBUTE_CONSTRUCTOR` (line 1099) - Delegates to helper
- `TEXT_CONSTRUCTOR` (line 1102) - Delegates to helper
- `COMMENT_CONSTRUCTOR` (line 1105) - Delegates to helper
- `PI_CONSTRUCTOR` (line 1108) - Delegates to helper
- `DOCUMENT_CONSTRUCTOR` (line 1111) - Delegates to helper
- `LOCATION_PATH` (line 1114) - Delegates to path evaluator
- `CAST_EXPRESSION` (line 1117) - 100-line inline handler with schema validation
- `TREAT_AS_EXPRESSION` (line 1217) - 209-line inline handler with type checking
- `INSTANCE_OF_EXPRESSION` (line 1425) - 26-line inline handler
- `CASTABLE_EXPRESSION` (line 1450) - 57-line inline handler
- `TYPESWITCH_EXPRESSION` (line 1506) - 94-line inline handler
- `UNION` (line 1600) - Delegates to union evaluator
- `CONDITIONAL` (line 1609) - 25-line inline ternary handler

#### 2. Individual If Statements (lines 1634-2296)
Handles 11 node types with dedicated if blocks:
- `LET_EXPRESSION` (line 1634, marked `[[likely]]`) - 54-line handler
- `FLWOR_EXPRESSION` (line 1687, marked `[[likely]]`) - Delegates to FLWOR pipeline
- `FOR_EXPRESSION` (line 1691, marked `[[likely]]`) - 86-line handler with iteration
- `QUANTIFIED_EXPRESSION` (line 1776) - 50-line handler (some/every quantifiers)
- `FILTER` (line 1826) - 85-line predicate application handler
- `PATH` (line 1911) - 54-line path step handler
- `FUNCTION_CALL` (line 1966) - Delegates to function call handler
- `UNARY_OP` (line 1972, marked `[[likely]]`) - 15-line handler (-, not)
- `BINARY_OP` (line 1988, marked `[[likely]]`) - 300-line handler covering:
  - Logical: `and`, `or`
  - Set operations: `union`, `intersect`, `except`
  - Sequence: `,` (comma operator)
  - Comparison: `=`, `!=`, `<`, `<=`, `>`, `>=`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`
  - Arithmetic: `+`, `-`, `*`, `div`, `idiv`, `mod`
  - String: `||` (concatenation)
  - Range: `to`
- `EXPRESSION` (line 2288) - 7-line wrapper unwrapper
- `VARIABLE_REFERENCE` (line 2296) - 26-line variable resolution

#### 3. Default Fallthrough (line 2323)
Sets `expression_unsupported = true` for unhandled types.

### Already-Extracted Handlers

The following handlers already exist as separate member functions (in `eval_values.cpp` and `eval_flwor.cpp`):

**Constructor handlers** (all in `eval_values.cpp`):
- `evaluate_direct_element_constructor()` (line 1615, ~26 lines)
- `evaluate_computed_element_constructor()` (line 1641, ~128 lines)
- `evaluate_computed_attribute_constructor()` (line 1769, ~103 lines)
- `evaluate_text_constructor()` (line 1872, ~37 lines)
- `evaluate_comment_constructor()` (line 1909, ~47 lines)
- `evaluate_pi_constructor()` (line 1956, ~65 lines)
- `evaluate_document_constructor()` (line 2021, ~261 lines)

**Path and set handlers** (all in `eval_values.cpp`):
- `evaluate_path_expression_value()` (line 355, ~219 lines)
- `evaluate_path_from_nodes()` (line 574, ~112 lines)
- `evaluate_union_value()` (line 686, ~131 lines)
- `evaluate_intersect_value()` (line 817, ~151 lines)
- `evaluate_except_value()` (line 968, ~647 lines)

**Function and FLWOR handlers**:
- `evaluate_function_call()` (eval_values.cpp line 2282, ~461 lines)
- `evaluate_user_defined_function()` (eval_values.cpp line 316, ~39 lines)
- `evaluate_flwor_pipeline()` (eval_flwor.cpp line 203, ~575 lines)

### Dependencies and State

Each handler relies on:
- **Evaluator state**: `expression_unsupported`, `arena`, `context`, `query`, `xml`
- **Error reporting**: `record_error()` method with optional node tracing
- **Schema registry**: `xml::schema::registry()` for type validation
- **Recursive evaluation**: Calls back to `evaluate_expression()` for child nodes
- **Variable resolution**: `resolve_variable_value()`, `prolog_variable_cache`
- **Constructor support**: `constructed_nodes`, `next_constructed_node_id`, namespace scopes

---

## Problems with Current Approach

### Maintainability Issues
1. **Monolithic function**: 1383 lines in a single function makes navigation difficult
2. **Mixed dispatch strategies**: Switch + if-statements + default fallthrough lacks consistency
3. **Inline complexity**: Cast/Treat/Typeswitch handlers span 100-209 lines inline
4. **Code duplication**: Schema validation patterns repeated across cast/treat/instance handlers
5. **Unclear boundaries**: Hard to determine which code belongs to which node type

### Performance Issues
1. **Poor code locality**: Related logic scattered across 1300+ lines
2. **Repeated type checks**: Each if-statement repeats `ExprNode->type IS ...` check
3. **Compiler optimisation barriers**: Large function inhibits inlining and branch prediction
4. **Cache unfriendly**: Entire function must fit in instruction cache
5. **No opportunity for specialisation**: Hot paths (BINARY_OP, UNARY_OP) mixed with cold paths

### Extensibility Issues
1. **High change risk**: Adding new node types requires editing monolithic function
2. **No compiler exhaustiveness checks**: Missing case in switch goes undetected
3. **Test isolation difficulty**: Unit testing individual handlers requires full evaluator setup
4. **Documentation fragmentation**: Handler logic not co-located with type definitions

---

## Proposed Solution: Pragmatic Hybrid Approach

### Phase 1: Centralised Dispatch Table (Week 1, Days 1-3)

**Goal**: Replace scattered if-statements with a function pointer dispatch table while preserving exact semantics.

#### Step 1.1: Create Handler Type Alias (1 hour)

**File**: `src/xquery/xquery.h` (after line 1151)

Add handler function pointer type:
```cpp
// Handler for a specific XQuery node type evaluation
using NodeEvaluationHandler = XPathVal (XPathEvaluator::*)(const XPathNode *, uint32_t);
```

#### Step 1.2: Declare Handler Methods (2 hours)

**File**: `src/xquery/xquery.h` (in `XPathEvaluator` class, private section after line 1207)

Add declarations for handlers not yet extracted:
```cpp
// Expression node type handlers
XPathVal handle_empty_sequence(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_number(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_literal(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_cast_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_treat_as_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_instance_of_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_castable_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_typeswitch_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_conditional(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_let_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_for_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_quantified_expression(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_filter(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_path(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_unary_op(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_binary_op(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_expression_wrapper(const XPathNode *Node, uint32_t CurrentPrefix);
XPathVal handle_variable_reference(const XPathNode *Node, uint32_t CurrentPrefix);
```

#### Step 1.3: Create Dispatch Table (4 hours)

**File**: `src/xquery/eval/eval_expression.cpp` (before `evaluate_expression()` function)

Add static dispatch table mapping node types to handlers:
```cpp
// Centralised dispatch table for expression node evaluation.
// Each entry maps an XQueryNodeType to its dedicated handler method.
// Handlers not listed here are either unsupported or handled via special cases.
static const ankerl::unordered_dense::map<XQueryNodeType, XPathEvaluator::NodeEvaluationHandler> NODE_HANDLERS = {
   // Literal values
   {XQueryNodeType::EMPTY_SEQUENCE, &XPathEvaluator::handle_empty_sequence},
   {XQueryNodeType::NUMBER, &XPathEvaluator::handle_number},
   {XQueryNodeType::LITERAL, &XPathEvaluator::handle_literal},
   {XQueryNodeType::STRING, &XPathEvaluator::handle_literal},  // STRING reuses LITERAL handler

   // Type expressions
   {XQueryNodeType::CAST_EXPRESSION, &XPathEvaluator::handle_cast_expression},
   {XQueryNodeType::TREAT_AS_EXPRESSION, &XPathEvaluator::handle_treat_as_expression},
   {XQueryNodeType::INSTANCE_OF_EXPRESSION, &XPathEvaluator::handle_instance_of_expression},
   {XQueryNodeType::CASTABLE_EXPRESSION, &XPathEvaluator::handle_castable_expression},
   {XQueryNodeType::TYPESWITCH_EXPRESSION, &XPathEvaluator::handle_typeswitch_expression},

   // Constructors (delegate to existing extracted functions)
   {XQueryNodeType::DIRECT_ELEMENT_CONSTRUCTOR, &XPathEvaluator::evaluate_direct_element_constructor},
   {XQueryNodeType::COMPUTED_ELEMENT_CONSTRUCTOR, &XPathEvaluator::evaluate_computed_element_constructor},
   {XQueryNodeType::COMPUTED_ATTRIBUTE_CONSTRUCTOR, &XPathEvaluator::evaluate_computed_attribute_constructor},
   {XQueryNodeType::TEXT_CONSTRUCTOR, &XPathEvaluator::evaluate_text_constructor},
   {XQueryNodeType::COMMENT_CONSTRUCTOR, &XPathEvaluator::evaluate_comment_constructor},
   {XQueryNodeType::PI_CONSTRUCTOR, &XPathEvaluator::evaluate_pi_constructor},
   {XQueryNodeType::DOCUMENT_CONSTRUCTOR, &XPathEvaluator::evaluate_document_constructor},

   // Path and set operations (delegate to existing extracted functions)
   {XQueryNodeType::LOCATION_PATH, &XPathEvaluator::evaluate_path_expression_value},
   {XQueryNodeType::UNION, &XPathEvaluator::handle_union_node},  // New wrapper for evaluate_union_value

   // Control flow
   {XQueryNodeType::CONDITIONAL, &XPathEvaluator::handle_conditional},

   // FLWOR and iteration
   {XQueryNodeType::LET_EXPRESSION, &XPathEvaluator::handle_let_expression},
   {XQueryNodeType::FOR_EXPRESSION, &XPathEvaluator::handle_for_expression},
   {XQueryNodeType::FLWOR_EXPRESSION, &XPathEvaluator::evaluate_flwor_pipeline},
   {XQueryNodeType::QUANTIFIED_EXPRESSION, &XPathEvaluator::handle_quantified_expression},

   // Filtering and navigation
   {XQueryNodeType::FILTER, &XPathEvaluator::handle_filter},
   {XQueryNodeType::PATH, &XPathEvaluator::handle_path},

   // Function calls
   {XQueryNodeType::FUNCTION_CALL, &XPathEvaluator::evaluate_function_call},

   // Operations
   {XQueryNodeType::UNARY_OP, &XPathEvaluator::handle_unary_op},
   {XQueryNodeType::BINARY_OP, &XPathEvaluator::handle_binary_op},

   // Wrappers and references
   {XQueryNodeType::EXPRESSION, &XPathEvaluator::handle_expression_wrapper},
   {XQueryNodeType::VARIABLE_REFERENCE, &XPathEvaluator::handle_variable_reference},
};
```

#### Step 1.4: Refactor evaluate_expression() to Use Dispatch Table (3 hours)

**File**: `src/xquery/eval/eval_expression.cpp`

Replace the existing switch + if-statement logic with dispatch table lookup:

```cpp
XPathVal XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix)
{
   if (not ExprNode) [[unlikely]] {
      expression_unsupported = true;
      return XPathVal();
   }

   // Look up handler in dispatch table
   auto handler_it = NODE_HANDLERS.find(ExprNode->type);
   if (handler_it != NODE_HANDLERS.end()) {
      // Invoke member function pointer
      return (this->*(handler_it->second))(ExprNode, CurrentPrefix);
   }

   // Unsupported or unrecognised node type
   if (is_trace_enabled()) {
      log.msg(VLF::TRACE, "Unsupported expression node type: %d", int(ExprNode->type));
   }
   expression_unsupported = true;
   return XPathVal();
}
```

#### Step 1.5: Extract Inline Handlers (Days 2-3, 12 hours)

For each node type currently handled inline, extract to a dedicated method. Use this template:

**Pattern**:
```cpp
XPathVal XPathEvaluator::handle_<node_type>(const XPathNode *Node, uint32_t CurrentPrefix)
{
   // [Copy existing case logic here, preserving all comments and error handling]
}
```

**Priority order** (extract in this sequence):
1. **Simple handlers first** (1 hour each):
   - `handle_empty_sequence()` - 3 lines
   - `handle_number()` - 6 lines
   - `handle_literal()` - 2 lines (shared by STRING)
   - `handle_conditional()` - 25 lines
   - `handle_expression_wrapper()` - 7 lines

2. **Medium complexity** (2 hours each):
   - `handle_unary_op()` - 15 lines
   - `handle_variable_reference()` - 26 lines
   - `handle_instance_of_expression()` - 26 lines
   - `handle_let_expression()` - 54 lines

3. **High complexity** (3-4 hours each):
   - `handle_castable_expression()` - 57 lines
   - `handle_quantified_expression()` - 50 lines
   - `handle_filter()` - 85 lines
   - `handle_for_expression()` - 86 lines
   - `handle_typeswitch_expression()` - 94 lines
   - `handle_path()` - 54 lines (but complex logic)

4. **Very high complexity** (4-6 hours each):
   - `handle_cast_expression()` - 100 lines with schema validation
   - `handle_treat_as_expression()` - 209 lines with type checking
   - `handle_binary_op()` - 300 lines with nested switch for operators

**Extraction process for each handler**:
1. Create function stub in `eval_expression.cpp`
2. Copy case block code exactly (preserve all whitespace, comments, error messages)
3. Replace `ExprNode` with `Node` parameter name (if needed for consistency)
4. Verify all references to evaluator state (`expression_unsupported`, `context`, etc.) remain valid
5. Compile and run tests
6. If tests pass, remove case block from old switch statement
7. Update dispatch table entry

#### Step 1.6: Testing Strategy for Phase 1 (ongoing)

After extracting each group of handlers:

1. **Compilation check**: `cmake --build build/agents --config Release --target xquery --parallel`
2. **Install**: `cmake --install build/agents`
3. **Run XQuery tests**: `ctest --build-config Release --test-dir build/agents -L xquery`
4. **Regression check**: All 20 test suites must pass (maintain 100% pass rate)

If any test fails:
- Compare handler output with original switch case
- Check for missing `expression_unsupported` flag propagation
- Verify error messages remain identical
- Ensure recursive `evaluate_expression()` calls preserved

---

### Phase 2: Typed Accessors for Node Fields (Week 1, Days 4-5)

**Goal**: Reduce optional field checking overhead by providing typed accessor views.

#### Step 2.1: Design Accessor Pattern (2 hours)

**Problem**: Current code repeatedly checks optional fields:
```cpp
if (Node->has_constructor_info()) {
   const auto *info = &(*Node->constructor_info);
   // use info...
}
```

**Solution**: Add accessor methods that encapsulate null-checking and provide direct access:

**File**: `src/xquery/xquery.h` (in `XPathNode` struct after line 411)

```cpp
// Typed accessor methods for optional fields
[[nodiscard]] inline const XPathConstructorInfo * get_constructor_info() const {
   return constructor_info ? &(*constructor_info) : nullptr;
}

[[nodiscard]] inline bool has_attribute_value_parts() const {
   return not attribute_value_parts.empty();
}

[[nodiscard]] inline std::string_view get_value_view() const {
   return std::string_view(value);
}

// Child access with bounds checking (returns nullptr if out of bounds, avoiding repeated child_count() checks)
[[nodiscard]] inline XPathNode * get_child_safe(size_t index) const {
   return index < children.size() ? children[index].get() : nullptr;
}

// Check for specific child type at index
[[nodiscard]] inline bool child_is_type(size_t index, XQueryNodeType Type) const {
   const XPathNode *child = get_child_safe(index);
   return child and (child->type IS Type);
}
```

#### Step 2.2: Refactor Handlers to Use Accessors (6 hours)

Update extracted handlers to use new accessor patterns. Focus on:

1. **Constructor info access** - Update all 7 constructor handlers
2. **Child iteration patterns** - Replace `for (size_t i = 0; i < Node->child_count(); ++i)` with range-based where possible
3. **Value access** - Use `get_value_view()` to avoid string copies in comparisons
4. **Optional field checks** - Replace `has_X()` + dereference with single `get_X()` returning pointer

**Example refactoring**:

**Before**:
```cpp
if (Node->has_constructor_info()) {
   const auto &info = *Node->constructor_info;
   std::string element_name = info.element_name;
   // ...
}
```

**After**:
```cpp
if (const auto *info = Node->get_constructor_info()) {
   std::string element_name = info->element_name;
   // ...
}
```

#### Step 2.3: Testing After Phase 2 (2 hours)

- Full regression test suite
- Verify no semantic changes
- Check compiler warnings for unused accessor methods
- Benchmark selected handlers to confirm no performance regression
- ✅ Built `xquery` target with FastBuild configuration to validate compilation; full regression suite still pending.

---

### Phase 3: Hot-Path Specialisation (Week 2, Days 1-4)

**Goal**: Optimise the most frequently executed node types with specialised fast paths.

#### Step 3.1: Identify Hot Paths via Profiling (4 hours)

**Approach**:
1. Add instrumentation counters to dispatch table lookup
2. Run W3C XQuery test suite
3. Run Parasol's internal XQuery tests
4. Collect node type frequency distribution

**Expected hot paths** (based on `[[likely]]` attributes in current code):
- `BINARY_OP` - Arithmetic and comparison operations
- `UNARY_OP` - Negation and logical not
- `FUNCTION_CALL` - Built-in and user functions
- `VARIABLE_REFERENCE` - Variable lookups
- `PATH` - Path navigation
- `FOR_EXPRESSION` - Iteration
- `LET_EXPRESSION` - Variable binding
- `FLWOR_EXPRESSION` - Complex queries

#### Step 3.2: Create Fast-Path Dispatch (8 hours)

**File**: `src/xquery/eval/eval_expression.cpp`

Add fast-path checks before dispatch table lookup:

```cpp
XPathVal XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix)
{
   if (not ExprNode) [[unlikely]] {
      expression_unsupported = true;
      return XPathVal();
   }

   // Fast path for hottest node types (avoids hash table lookup)
   switch (ExprNode->type) {
      case XQueryNodeType::BINARY_OP: [[likely]]
         return handle_binary_op(ExprNode, CurrentPrefix);

      case XQueryNodeType::UNARY_OP: [[likely]]
         return handle_unary_op(ExprNode, CurrentPrefix);

      case XQueryNodeType::VARIABLE_REFERENCE: [[likely]]
         return handle_variable_reference(ExprNode, CurrentPrefix);

      case XQueryNodeType::FUNCTION_CALL: [[likely]]
         return evaluate_function_call(ExprNode, CurrentPrefix);

      case XQueryNodeType::NUMBER:
         return handle_number(ExprNode, CurrentPrefix);

      case XQueryNodeType::LITERAL:
      case XQueryNodeType::STRING:
         return handle_literal(ExprNode, CurrentPrefix);

      default:
         break;  // Fall through to dispatch table
   }

   // Dispatch table lookup for less common types
   auto handler_it = NODE_HANDLERS.find(ExprNode->type);
   if (handler_it != NODE_HANDLERS.end()) {
      return (this->*(handler_it->second))(ExprNode, CurrentPrefix);
   }

   // Unsupported node type
   if (is_trace_enabled()) {
      log.msg(VLF::TRACE, "Unsupported expression node type: %d", int(ExprNode->type));
   }
   expression_unsupported = true;
   return XPathVal();
}
```

**Rationale**:
- Switch statement with 6-8 cases compiles to jump table or binary search (O(1) or O(log n))
- Hash table lookup is O(1) average but has higher constant factor
- Most queries will hit hot paths 80%+ of the time
- Compiler can inline hot-path handlers more aggressively

#### Step 3.3: Specialise Binary Operator Handler (12 hours)

**File**: `src/xquery/eval/eval_expression.cpp`

The `handle_binary_op()` function is 300+ lines and handles many operation types. Split into sub-handlers:

```cpp
// Forward declarations for binary operation sub-handlers
XPathVal handle_binary_logical(const XPathNode *Node, const XPathNode *Left, const XPathNode *Right,
                                uint32_t CurrentPrefix, BinaryOperationKind OpKind);
XPathVal handle_binary_comparison(const XPathNode *Node, const XPathNode *Left, const XPathNode *Right,
                                   uint32_t CurrentPrefix, BinaryOperationKind OpKind);
XPathVal handle_binary_arithmetic(const XPathNode *Node, const XPathNode *Left, const XPathNode *Right,
                                   uint32_t CurrentPrefix, BinaryOperationKind OpKind);
XPathVal handle_binary_sequence(const XPathNode *Node, const XPathNode *Left, const XPathNode *Right,
                                 uint32_t CurrentPrefix, BinaryOperationKind OpKind);
XPathVal handle_binary_set_ops(const XPathNode *Node, const XPathNode *Left, const XPathNode *Right,
                                uint32_t CurrentPrefix, BinaryOperationKind OpKind);

XPathVal XPathEvaluator::handle_binary_op(const XPathNode *Node, uint32_t CurrentPrefix)
{
   if (Node->child_count() < 2) [[unlikely]] {
      expression_unsupported = true;
      return XPathVal();
   }

   const XPathNode *left_node = Node->get_child(0);
   const XPathNode *right_node = Node->get_child(1);
   const std::string &operation = Node->value;
   BinaryOperationKind op_kind = map_binary_operation(operation);

   // Dispatch to specialised sub-handler based on operation category
   switch (op_kind) {
      case BinaryOperationKind::AND:
      case BinaryOperationKind::OR:
         return handle_binary_logical(Node, left_node, right_node, CurrentPrefix, op_kind);

      case BinaryOperationKind::EQ:
      case BinaryOperationKind::NE:
      case BinaryOperationKind::LT:
      case BinaryOperationKind::LE:
      case BinaryOperationKind::GT:
      case BinaryOperationKind::GE:
      case BinaryOperationKind::VALUE_EQ:
      case BinaryOperationKind::VALUE_NE:
      case BinaryOperationKind::VALUE_LT:
      case BinaryOperationKind::VALUE_LE:
      case BinaryOperationKind::VALUE_GT:
      case BinaryOperationKind::VALUE_GE:
         return handle_binary_comparison(Node, left_node, right_node, CurrentPrefix, op_kind);

      case BinaryOperationKind::ADD:
      case BinaryOperationKind::SUB:
      case BinaryOperationKind::MUL:
      case BinaryOperationKind::DIV:
      case BinaryOperationKind::IDIV:
      case BinaryOperationKind::MOD:
         return handle_binary_arithmetic(Node, left_node, right_node, CurrentPrefix, op_kind);

      case BinaryOperationKind::COMMA:
      case BinaryOperationKind::TO:
      case BinaryOperationKind::CONCAT:
         return handle_binary_sequence(Node, left_node, right_node, CurrentPrefix, op_kind);

      case BinaryOperationKind::UNION:
      case BinaryOperationKind::INTERSECT:
      case BinaryOperationKind::EXCEPT:
         return handle_binary_set_ops(Node, left_node, right_node, CurrentPrefix, op_kind);

      default:
         expression_unsupported = true;
         return XPathVal();
   }
}
```

**Benefits**:
- Smaller functions easier to optimise
- Better instruction cache utilisation
- Clearer code organisation by operation category
- Enables future SIMD optimisation for arithmetic operations

#### Step 3.4: Optimise Common Sub-Expressions (4 hours)

Identify repeated patterns across handlers and extract to helper functions:

**Example**: Schema type descriptor lookup (repeated in cast/treat/instance/castable handlers):

**File**: `src/xquery/eval/eval_expression.cpp` (at file scope)

```cpp
// Helper: Resolve schema type descriptor with standard error reporting
static std::shared_ptr<xml::schema::SchemaTypeDescriptor>
resolve_schema_type(XPathEvaluator *Eval, std::string_view TypeName, const XPathNode *Node)
{
   auto &registry = xml::schema::registry();
   auto descriptor = registry.find_descriptor(TypeName);
   if (not descriptor) {
      auto message = std::format("XPST0052: Type '{}' is not defined.", TypeName);
      Eval->record_error(message, Node, true);
   }
   return descriptor;
}
```

Then update cast/treat/instance handlers to use this helper instead of duplicating the lookup + error logic.

#### Step 3.5: Add Inline Hints for Compiler (2 hours)

Mark small, frequently-called handlers for aggressive inlining:

**File**: `src/xquery/xquery.h`

```cpp
// Mark hot-path handlers for inlining (header-only or force inline)
[[gnu::always_inline]] inline XPathVal handle_number(const XPathNode *Node, uint32_t CurrentPrefix);
[[gnu::always_inline]] inline XPathVal handle_literal(const XPathNode *Node, uint32_t CurrentPrefix);
```

Move implementations to header or ensure compiler can inline them.

#### Step 3.6: Benchmark Phase 3 (4 hours)

Compare performance before/after hot-path optimisation:

**Benchmark suite**:
1. Simple arithmetic queries (`1 + 2 * 3`)
2. Variable-heavy queries (`let $x := 5 return $x * $x`)
3. Path-heavy queries (`//book[@price < 10]/title`)
4. FLWOR queries (nested iterations)
5. Complex binary operations (mixed comparisons and arithmetic)

**Metrics**:
- Query execution time (µs)
- Instructions per query (perf stat)
- Branch mispredictions
- L1 instruction cache misses

**Expected improvements**:
- 5-10% faster for simple queries (less dispatch overhead)
- 10-15% faster for variable-heavy queries (inlined lookups)
- Minimal change for constructor-heavy queries (not on hot path)

---

### Phase 4: Optional Variant-Based View (Week 2-3, Days 5-10)

**Goal**: Pilot `std::variant` for a subset of hot node types to evaluate compile-time/runtime tradeoffs.

**Status**: **DEFERRED** - Only proceed if Phase 3 benchmarks show dispatch overhead remains significant.

**Rationale**: The pragmatic approach focuses on incremental improvements without AST restructuring. Variant-based dispatch requires:
- Significant changes to node storage
- Re-parsing of all queries
- Risk of ABI breakage
- Longer compile times for template instantiation

**If pursued**:
1. Create wrapper type `HotNodeVariant` containing only 6-8 hottest types
2. Add `std::variant<...>` field to `XPathNode` (opt-in, not replacing existing fields)
3. Update parser to populate variant for hot types
4. Modify `evaluate_expression()` to check variant presence before dispatch table
5. Benchmark to justify complexity

**Decision gate**: Proceed only if Phase 3 achieves < 5% speedup. Otherwise, declare victory and move to Phase 5.

---

### Phase 5: Cleanup and Documentation (Week 3, Days 1-5)

#### Step 5.1: Remove Duplicate Switches (4 hours)

**File**: Search `src/xquery/eval/*.cpp` for remaining switch statements on `node->type`.

Check:
- `eval_context.cpp` - May have dispatch logic for context management
- `eval_navigation.cpp` - May have node test switches
- `eval_predicates.cpp` - May have predicate-specific switches

For each switch found:
1. Determine if it's redundant with centralised dispatch
2. If yes, replace with handler method call
3. If no (e.g., different dispatch purpose), add comment explaining why it's separate

#### Step 5.2: Document Handler Contracts (8 hours)

For each handler method in `XPathEvaluator`, add comprehensive doc comment:

**Template**:
```cpp
//********************************************************************************************************************
// Evaluates a <NODE_TYPE> node and returns the computed value.
//
// The <NODE_TYPE> node represents [semantic description from XQuery spec].
//
// **Preconditions**:
// - Node must be non-null
// - Node type must match <NODE_TYPE>
// - [Any additional requirements, e.g., "Must have exactly 2 children"]
//
// **Behaviour**:
// - [Describe evaluation logic in 1-3 sentences]
// - Sets `expression_unsupported` to true if evaluation fails
// - Records errors via `record_error()` for invalid structures
//
// **Returns**:
// - On success: XPathVal containing [describe result type]
// - On failure: Empty XPathVal with `expression_unsupported` set
//
// **Examples**:
// - `<example XQuery expression>` → <result>
//
// **Related**:
// - XQuery spec section [X.Y]
// - Called from: evaluate_expression()
// - May recursively call: evaluate_expression() for child nodes

XPathVal XPathEvaluator::handle_<node_type>(const XPathNode *Node, uint32_t CurrentPrefix)
{
   // ...
}
```

#### Step 5.3: Add Contributor Notes (2 hours)

**File**: Create `src/xquery/eval/README.md`

Document the dispatch architecture for future contributors:

```markdown
# XQuery Expression Evaluator Architecture

## Overview
The XPath/XQuery expression evaluator is responsible for traversing the AST produced by the parser and computing values for each expression node.

## Dispatch Mechanism
Expression evaluation uses a centralised dispatch table in `eval_expression.cpp`:

1. **Fast path** (lines XXX-YYY): Inline switch for 6-8 hottest node types
2. **Dispatch table** (line ZZZ): Hash map lookup for less common types
3. **Handler methods**: Each node type has a dedicated `handle_<type>()` method

## Adding a New Node Type
To add support for a new XQuery expression node type:

1. Define the enum value in `src/xquery/xquery.fdl`
2. Add a handler method declaration in `src/xquery/xquery.h` (XPathEvaluator class)
3. Implement the handler in `eval_expression.cpp` or related file
4. Add entry to `NODE_HANDLERS` dispatch table
5. Write Flute tests in `src/xquery/tests/`
6. Run full test suite to ensure no regressions

## Handler Method Conventions
All handlers follow this signature:
```cpp
XPathVal handle_<type>(const XPathNode *Node, uint32_t CurrentPrefix);
```

- **Node**: AST node to evaluate (never null when called from dispatch)
- **CurrentPrefix**: Namespace prefix index for QName resolution
- **Returns**: Computed XPath value, or empty value if unsupported

Handlers must:
- Set `expression_unsupported = true` on unrecoverable errors
- Call `record_error()` for user-facing error messages
- Recursively call `evaluate_expression()` for child nodes
- Preserve `expression_unsupported` flag from recursive calls

## Performance Considerations
- Hot path contains node types accounting for 80%+ evaluations
- Handler methods are small (< 100 lines ideal) for inlining
- Binary operation dispatch uses nested switch for operation kind
- Avoid allocations in hot paths (use arena for temporary vectors)

## Testing
- Unit tests: Individual handler methods
- Integration tests: W3C XQuery conformance suite
- Regression tests: Parasol's Flute test suite (src/xquery/tests/)

Run tests after any dispatch changes:
```bash
cmake --build build/agents --config Release --target xquery --parallel
cmake --install build/agents
ctest --build-config Release --test-dir build/agents -L xquery
```

#### Step 5.4: Update Optimisation Plan (2 hours)

**File**: `docs/plans/XQUERY_OPTIMIZATION_PLAN.md`

Update LR-3 section with:
- **Status**: Completed
- **Date completed**: [Date]
- **Files modified**: List of all changed files
- **Performance impact**: Benchmark results summary
- **Breaking changes**: None (or list if any)
- **Migration notes**: N/A (internal refactoring)

#### Step 5.5: Create Developer Presentation (4 hours)

**File**: Create `docs/plans/XQUERY_DISPATCH_ARCHITECTURE.md`

Visual documentation of the new dispatch flow:

# XQuery Dispatch Architecture

## Before Refactoring

```
evaluate_expression()
├─ switch (node->type)  [19 cases]
│  ├─ case EMPTY_SEQUENCE: [inline]
│  ├─ case NUMBER: [inline]
│  ├─ case CAST_EXPRESSION: [100 lines inline]
│  ├─ case TREAT_AS_EXPRESSION: [209 lines inline]
│  └─ ...
├─ if (node->type IS LET_EXPRESSION)  [54 lines inline]
├─ if (node->type IS FLWOR_EXPRESSION) [delegate]
├─ if (node->type IS PATH) [54 lines inline]
├─ if (node->type IS BINARY_OP) [300 lines inline]
└─ default: unsupported
```

**Problems**: 1383 lines, poor locality, hard to maintain

## After Refactoring

```
evaluate_expression()
├─ Fast-path switch [6-8 hot types]
│  ├─ BINARY_OP → handle_binary_op()
│  │               ├─ handle_binary_logical()
│  │               ├─ handle_binary_comparison()
│  │               └─ handle_binary_arithmetic()
│  ├─ UNARY_OP → handle_unary_op()
│  ├─ VARIABLE_REFERENCE → handle_variable_reference()
│  └─ FUNCTION_CALL → evaluate_function_call()
├─ Dispatch table lookup [remaining types]
│  ├─ CAST_EXPRESSION → handle_cast_expression()
│  ├─ TREAT_AS_EXPRESSION → handle_treat_as_expression()
│  ├─ CONSTRUCTOR_* → evaluate_*_constructor()
│  └─ ...
└─ Default: unsupported
```
**Benefits**: Modular, testable, optimizable, maintainable

## Dispatch Table Structure

```cpp
static const map<XQueryNodeType, NodeEvaluationHandler> NODE_HANDLERS = {
   {EMPTY_SEQUENCE, &XPathEvaluator::handle_empty_sequence},
   {NUMBER, &XPathEvaluator::handle_number},
   // ... 30+ entries
};
```

## Handler Method Pattern

```cpp
XPathVal XPathEvaluator::handle_<type>(const XPathNode *Node, uint32_t CurrentPrefix)
{
   // 1. Validate preconditions
   if (Node->child_count() < expected) {
      expression_unsupported = true;
      return XPathVal();
   }

   // 2. Evaluate child nodes
   XPathVal child_value = evaluate_expression(Node->get_child(0), CurrentPrefix);
   if (expression_unsupported) return XPathVal();

   // 3. Compute result
   // ... type-specific logic ...

   return result;
}
```

## Performance Impact

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| evaluate_expression() size | 1383 lines | ~50 lines | 96% reduction |
| Largest handler | 300 lines (inline) | 80 lines (separate) | 73% reduction |
| Hot-path overhead | Hash lookup | Switch jump | 5-10% faster |
| Code locality | Poor | Excellent | Better caching |

---

## Testing Strategy

### Unit Testing Approach

For each extracted handler, create isolated unit tests:

**File**: `src/xquery/tests/test_dispatch_handlers.fluid`

```fluid
-- Test individual node type handlers
require('flute')

glTestCount = 0

-- Helper: Create minimal evaluator context
local function createEvaluator()
   local xml = obj.new('xml', { statement='<root/>' })
   local xquery = obj.new('xquery', { xml=xml, expression='1' })
   return xquery
end

-- Test: NUMBER node handler
glTestCount = glTestCount + 1
flute.test(function()
   local xq = createEvaluator()
   local result = xq.mtEvaluate('42')
   flute.assert(result.value == 42, 'NUMBER handler should parse integer')
end, 'Number node handler')

-- Test: BINARY_OP arithmetic
glTestCount = glTestCount + 1
flute.test(function()
   local xq = createEvaluator()
   local result = xq.mtEvaluate('5 + 3')
   flute.assert(result.value == 8, 'BINARY_OP handler should evaluate addition')
end, 'Binary operation addition')

-- ... additional handler tests ...

print(string.format('Ran %d handler unit tests', glTestCount))
```

### Integration Testing

Ensure W3C XQuery conformance suite still passes:

**Process**:
1. After each phase, run full W3C test suite
2. Compare pass/fail counts with baseline
3. For any new failures, diff the error messages with baseline
4. Fix handler if semantic difference found

### Regression Testing

Maintain 100% pass rate on Parasol's XQuery tests:

**Command**:
```bash
ctest --build-config Release --test-dir build/agents -L xquery --output-on-failure
```

**Expectations**:
- All 20 test suites pass
- No change in test execution time (within 5% variance)
- No change in error message text (except intentional improvements)

### Performance Regression Prevention

**Benchmark suite**: `src/xquery/tests/benchmark_dispatch.fluid`

```fluid
-- Dispatch performance benchmarks
require('flute')

local function measureQueryTime(xq, expression, iterations)
   local start = os.clock()
   for i = 1, iterations do
      xq.mtEvaluate(expression)
   end
   local elapsed = os.clock() - start
   return elapsed / iterations  -- Average time per query
end

local xml = obj.new('xml', { statement='<root><item>1</item><item>2</item></root>' })
local xq = obj.new('xquery', { xml=xml })

-- Baseline: Simple arithmetic (hot path)
local arithTime = measureQueryTime(xq, '1 + 2 * 3', 10000)
print(string.format('Arithmetic (hot path): %.6f ms', arithTime * 1000))

-- Variable reference (hot path)
xq.acQuery({ statement='declare variable $x := 5; $x * $x', resulttype='value' })
local varTime = measureQueryTime(xq, '$x * $x', 10000)
print(string.format('Variable reference: %.6f ms', varTime * 1000))

-- Path expression (warm path)
local pathTime = measureQueryTime(xq, '//item', 1000)
print(string.format('Path expression: %.6f ms', pathTime * 1000))

-- Cast expression (cold path)
local castTime = measureQueryTime(xq, '(5.5 cast as xs:integer)', 1000)
print(string.format('Cast expression: %.6f ms', castTime * 1000))
```

**Acceptance criteria**:
- Hot-path queries must not regress by > 5%
- Warm-path queries must not regress by > 10%
- Cold-path queries may regress up to 15% (acceptable for cleaner code)

---

## Risk Mitigation

### Risk: Semantic Regression

**Likelihood**: Medium
**Impact**: High (breaks XQuery compliance)
**Mitigation**:
- Extract handlers line-by-line, preserving all comments
- Do not "improve" error messages during extraction
- Test after every handler extraction, not at end of phase
- Keep old code commented out until phase completes
- Use `git diff` to verify only structural changes, no logic changes

**Detection**:
- W3C test suite catches spec violations
- Parasol test suite catches internal contract violations
- `ctest` must pass 100% before proceeding to next handler

### Risk: Performance Regression

**Likelihood**: Low (dispatch table should be similar speed)
**Impact**: Medium (slower queries)
**Mitigation**:
- Benchmark before refactoring (establish baseline)
- Benchmark after Phase 1 (dispatch table)
- Benchmark after Phase 3 (hot-path optimisation)
- If regression > 10%, revert and analyse

**Detection**:
- `benchmark_dispatch.fluid` script (created in Phase 1)
- Manual profiling with `perf stat` on Linux
- Query execution time logging in evaluator (add counters)

### Risk: Compilation Time Increase

**Likelihood**: Low (not using heavy templates)
**Impact**: Low (slightly slower builds)
**Mitigation**:
- Avoid `std::variant` unless Phase 4 is pursued
- Keep handler methods in `.cpp` files, not headers
- Use forward declarations where possible

**Detection**:
- Measure build time before refactoring: `time cmake --build ...`
- Measure after each phase
- Acceptable: < 10% increase
- Unacceptable: > 20% increase (investigate split into multiple translation units)

### Risk: Merge Conflicts

**Likelihood**: High (eval_expression.cpp is large and active)
**Impact**: Medium (time spent resolving conflicts)
**Mitigation**:
- Complete refactoring in dedicated branch
- Regularly rebase onto master
- Communicate refactoring scope to team (freeze eval_expression.cpp changes)
- Complete all phases within 3 weeks to minimise drift

**Detection**:
- Daily `git fetch && git rebase origin/master`
- If conflicts arise, resolve immediately (don't accumulate)

### Risk: Increased Complexity for New Contributors

**Likelihood**: Low (new architecture is clearer)
**Impact**: Low (slower onboarding)
**Mitigation**:
- Create comprehensive README.md (Phase 5)
- Add inline comments explaining dispatch flow
- Provide examples of adding new node types
- Document testing requirements

**Detection**:
- Solicit feedback from team member unfamiliar with refactoring
- If documentation is unclear, improve before marking complete

---

## Success Criteria

### Functional Requirements

**Must achieve**:
- [ ] All 20 XQuery test suites pass (100% pass rate maintained)
- [ ] W3C XQuery conformance suite shows no new failures
- [ ] Error messages remain identical (or improved with explicit approval)
- [ ] No breaking changes to public APIs (`objXQuery` interface unchanged)

### Code Quality Requirements

**Must achieve**:
- [ ] `evaluate_expression()` reduced to < 100 lines
- [ ] No handler method exceeds 150 lines
- [ ] All handlers have doc comments documenting preconditions/behaviour
- [ ] Dispatch table documents all 30+ node type mappings
- [ ] No `TODO` or `FIXME` comments remain in refactored code

### Performance Requirements

**Must achieve**:
- [ ] Hot-path queries (arithmetic, variables) within 5% of baseline
- [ ] Warm-path queries (paths, functions) within 10% of baseline

**Nice to have**:
- [ ] Hot-path queries 5-10% faster after Phase 3 optimisation
- [ ] Instruction cache misses reduced by 10%+

### Documentation Requirements

**Must achieve**:
- [ ] `README.md` created in `src/xquery/eval/` explaining architecture
- [ ] LR-3 section in optimisation plan updated with completion status
- [ ] Each handler method has comprehensive doc comment
- [ ] Developer presentation document created (XQUERY_DISPATCH_ARCHITECTURE.md)

---

## Phased Rollout Schedule

### Week 1: Foundation

| Day | Phase | Tasks | Hours | Deliverables |
|-----|-------|-------|-------|--------------|
| Mon | 1.1-1.3 | Handler type alias, declarations, dispatch table | 7 | Dispatch table scaffolding |
| Tue | 1.4-1.5 | Refactor `evaluate_expression()`, extract simple handlers | 8 | 5 handlers extracted, tests pass |
| Wed | 1.5 | Extract medium complexity handlers | 8 | 9 handlers extracted total, tests pass |
| Thu | 1.5 | Extract high complexity handlers | 8 | 15 handlers extracted total, tests pass |
| Fri | 1.5, 2.1-2.2 | Finish handler extraction, start typed accessors | 8 | Phase 1 complete, accessor design done |

**Checkpoint**: Phase 1 complete, all tests passing, 100% regression-free.

### Week 2: Optimisation

| Day | Phase | Tasks | Hours | Deliverables |
|-----|-------|-------|-------|--------------|
| Mon | 2.2-2.3 | Complete accessor refactoring, test | 8 | Phase 2 complete, accessor tests pass |
| Tue | 3.1-3.2 | Profile hot paths, implement fast-path dispatch | 8 | Hot-path dispatch implemented |
| Wed | 3.3 | Specialise binary operator handler | 8 | Binary op sub-handlers extracted |
| Thu | 3.4-3.5 | Common sub-expression extraction, inline hints | 6 | Helper functions extracted |
| Fri | 3.6 | Benchmark Phase 3, analyse results | 8 | Phase 3 complete, performance report |

**Checkpoint**: Phase 3 complete, performance targets met (5-10% hot-path speedup).

### Week 3: Polish and Documentation

| Day | Phase | Tasks | Hours | Deliverables |
|-----|-------|-------|-------|--------------|
| Mon | 5.1 | Remove duplicate switches | 4 | All redundant switches removed |
| Tue | 5.2 | Document handler contracts | 8 | All handlers documented |
| Wed | 5.3 | Create contributor README | 8 | README.md complete |
| Thu | 5.4-5.5 | Update optimisation plan, create presentation | 6 | Documentation complete |
| Fri | Review | Code review, final testing, merge preparation | 8 | Ready for merge |

**Checkpoint**: LR-3 complete, documentation approved, code reviewed.

---

## Rollback Plan

If critical issues arise during implementation:

### Phase 1 Rollback
- Revert commits in `eval_expression.cpp`
- Restore original switch/if-statement dispatch
- Remove handler method stubs from header
- Expected downtime: < 1 hour

### Phase 2 Rollback
- Revert accessor method additions to `XPathNode`
- Restore direct field access in handlers
- Phase 1 remains intact (handlers still extracted)
- Expected downtime: < 30 minutes

### Phase 3 Rollback
- Remove fast-path switch from `evaluate_expression()`
- Remove binary operation sub-handler splits
- Fall back to dispatch table for all types
- Phase 1-2 remain intact
- Expected downtime: < 1 hour

### Complete Rollback
If fundamental issues discovered:
1. Create rollback branch from pre-refactoring commit
2. Cherry-pick any unrelated bug fixes from refactoring branch
3. Abandon refactoring branch
4. Schedule retrospective to analyse failure
5. Expected downtime: 2-4 hours

**Rollback triggers**:
- More than 5 W3C test failures
- More than 2 Parasol test suite failures
- Performance regression > 15% on hot paths
- Compilation time increase > 25%
- Unresolvable merge conflicts with critical fixes

---

## Appendix A: Node Type Inventory

Complete list of 51 XQueryNodeType values and their dispatch status:

| Node Type | Current Handler | Extraction Status | Priority |
|-----------|----------------|------------------|----------|
| EMPTY_SEQUENCE | switch case | Simple (3 lines) | High |
| NUMBER | switch case | Simple (6 lines) | High |
| LITERAL | switch case | Simple (2 lines) | High |
| STRING | switch case (shared with LITERAL) | Simple | High |
| DIRECT_ELEMENT_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| COMPUTED_ELEMENT_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| COMPUTED_ATTRIBUTE_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| TEXT_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| COMMENT_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| PI_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| DOCUMENT_CONSTRUCTOR | switch case → extracted | Already extracted | N/A |
| LOCATION_PATH | switch case → extracted | Already extracted | N/A |
| CAST_EXPRESSION | switch case | Very high complexity (100 lines) | Medium |
| TREAT_AS_EXPRESSION | switch case | Very high complexity (209 lines) | Medium |
| INSTANCE_OF_EXPRESSION | switch case | Medium (26 lines) | Medium |
| CASTABLE_EXPRESSION | switch case | High (57 lines) | Medium |
| TYPESWITCH_EXPRESSION | switch case | High (94 lines) | Low |
| UNION | switch case → extracted (wrapper) | Already extracted | N/A |
| CONDITIONAL | switch case | Medium (25 lines) | High |
| LET_EXPRESSION | if statement | Medium (54 lines) | High |
| FOR_EXPRESSION | if statement | High (86 lines) | High |
| FLWOR_EXPRESSION | if statement → extracted | Already extracted | N/A |
| QUANTIFIED_EXPRESSION | if statement | High (50 lines) | Medium |
| FILTER | if statement | High (85 lines) | High |
| PATH | if statement | High (54 lines) | High |
| FUNCTION_CALL | if statement → extracted | Already extracted | N/A |
| UNARY_OP | if statement | Medium (15 lines) | High (hot path) |
| BINARY_OP | if statement | Very high (300 lines) | High (hot path) |
| EXPRESSION | if statement | Simple (7 lines) | High |
| VARIABLE_REFERENCE | if statement | Medium (26 lines) | High (hot path) |
| LOCATION_PATH | (duplicate, handled above) | | |
| STEP | Not directly evaluated (used in path parsing) | N/A | N/A |
| NODE_TEST | Not directly evaluated (used in axis matching) | N/A | N/A |
| PREDICATE | Not directly evaluated (predicate evaluator) | N/A | N/A |
| ROOT | Handled via LOCATION_PATH | N/A | N/A |
| EXPRESSION | (duplicate, handled above) | | |
| FILTER | (duplicate, handled above) | | |
| BINARY_OP | (duplicate, handled above) | | |
| UNARY_OP | (duplicate, handled above) | | |
| FOR_BINDING | Processed within FOR_EXPRESSION | N/A | N/A |
| LET_BINDING | Processed within LET_EXPRESSION | N/A | N/A |
| WHERE_CLAUSE | Processed within FLWOR pipeline | N/A | N/A |
| GROUP_CLAUSE | Processed within FLWOR pipeline | N/A | N/A |
| GROUP_KEY | Processed within GROUP_CLAUSE | N/A | N/A |
| ORDER_CLAUSE | Processed within FLWOR pipeline | N/A | N/A |
| ORDER_SPEC | Processed within ORDER_CLAUSE | N/A | N/A |
| COUNT_CLAUSE | Processed within FLWOR pipeline | N/A | N/A |
| QUANTIFIED_BINDING | Processed within QUANTIFIED_EXPRESSION | N/A | N/A |
| NAME_TEST | Handled in axis matching, not dispatch | N/A | N/A |
| NODE_TYPE_TEST | Handled in axis matching, not dispatch | N/A | N/A |
| PROCESSING_INSTRUCTION_TEST | Handled in axis matching, not dispatch | N/A | N/A |
| WILDCARD | Handled in axis matching, not dispatch | N/A | N/A |
| AXIS_SPECIFIER | Processed within STEP, not dispatch | N/A | N/A |
| CONSTRUCTOR_CONTENT | Processed by constructors, not top-level | N/A | N/A |
| ATTRIBUTE_VALUE_TEMPLATE | Processed by constructors, not top-level | N/A | N/A |
| DIRECT_ATTRIBUTE_CONSTRUCTOR | Processed within element constructors | N/A | N/A |
| DIRECT_TEXT_CONSTRUCTOR | Processed within element constructors | N/A | N/A |
| TYPESWITCH_CASE | Processed within TYPESWITCH_EXPRESSION | N/A | N/A |
| TYPESWITCH_DEFAULT_CASE | Processed within TYPESWITCH_EXPRESSION | N/A | N/A |

**Summary**:
- **Requires extraction**: 18 node types
- **Already extracted**: 12 node types
- **Not dispatched** (processed by parent): 21 node types

---

## Appendix B: BinaryOperationKind Enum

The `map_binary_operation()` function maps XQuery operators to this enum:

```cpp
enum class BinaryOperationKind {
   UNKNOWN,
   AND, OR,                                      // Logical
   EQ, NE, LT, LE, GT, GE,                      // General comparison
   VALUE_EQ, VALUE_NE, VALUE_LT, VALUE_LE, VALUE_GT, VALUE_GE,  // Value comparison
   ADD, SUB, MUL, DIV, IDIV, MOD,               // Arithmetic
   UNION, INTERSECT, EXCEPT,                     // Set operations
   COMMA,                                        // Sequence construction
   TO,                                           // Range
   CONCAT                                        // String concatenation (||)
};
```

**Handler assignment after Phase 3**:
- `handle_binary_logical()`: AND, OR
- `handle_binary_comparison()`: EQ, NE, LT, LE, GT, GE, VALUE_*
- `handle_binary_arithmetic()`: ADD, SUB, MUL, DIV, IDIV, MOD
- `handle_binary_sequence()`: COMMA, TO, CONCAT
- `handle_binary_set_ops()`: UNION, INTERSECT, EXCEPT

---

## Appendix C: Coding Standards for LR-3

All code changes must follow project coding standards:

### Required Conventions
- **Comparison**: Use `IS` macro instead of `==`
- **Logical operators**: Use `and`/`or` instead of `&&`/`||`
- **Casting**: Use C-style casts `int(x)` instead of `static_cast<int>(x)`
- **Indentation**: Three spaces, no tabs
- **Variable names**: Lower snake_case for locals, UpperCamelCase for parameters
- **Spelling**: British English (optimise, colour, behaviour)
- **Column width**: 120 characters maximum
- **Opening brace**: Same line for `if`/`while`/`for`/`switch`/`struct` when no wrap

### Error Handling
- **No exceptions**: Return error codes or set flags
- **Error messages**: Use XQuery error codes (XPST0003, XPTY0004, etc.)
- **Tracing**: Use `is_trace_enabled()` before logging
- **Flag propagation**: Always check `expression_unsupported` after recursive calls

### Example (Before)
```cpp
if (node == nullptr) return XPathVal();  // WRONG: Uses '==' instead of the project-standard null check pattern
```

### Example (After)
```cpp
if (not node) return XPathVal();  // CORRECT: Uses 'not', follows standards
```

---

## Appendix D: Benchmark Reference Values

These are baseline performance metrics measured before LR-3 implementation. Use for Phase 3 comparison.

**System**: Windows 10, Intel Core i7-10700K, 32GB RAM
**Build**: Release, PARASOL_STATIC=ON
**Date**: 2025-10-27

| Query Type | Expression | Iterations | Avg Time (µs) | Std Dev |
|------------|-----------|-----------|---------------|---------|
| Arithmetic (hot) | `1 + 2 * 3` | 10000 | 0.83 | 0.05 |
| Variable (hot) | `$x * $x` | 10000 | 1.21 | 0.08 |
| Unary negation | `-5` | 10000 | 0.92 | 0.06 |
| Binary comparison | `5 > 3` | 10000 | 1.15 | 0.07 |
| Function call | `string-length("test")` | 5000 | 3.45 | 0.21 |
| Path expression | `//item` | 1000 | 15.32 | 1.10 |
| FLWOR simple | `for $i in (1,2,3) return $i * 2` | 1000 | 8.67 | 0.52 |
| Cast expression | `(5.5 cast as xs:integer)` | 1000 | 4.21 | 0.30 |
| Typeswitch | `typeswitch($x) case xs:integer...` | 500 | 12.88 | 0.95 |

**Target improvements after Phase 3**:
- Arithmetic: < 0.80 µs (5% faster)
- Variable: < 1.15 µs (5% faster)
- Other queries: Within 10% of baseline

---

## Appendix E: Related Files Reference

### Primary Files
- `src/xquery/eval/eval_expression.cpp` - Main evaluation logic (2325 lines)
- `src/xquery/eval/eval_values.cpp` - Value constructors and path evaluation
- `src/xquery/eval/eval_flwor.cpp` - FLWOR pipeline evaluation
- `src/xquery/xquery.h` - XPathEvaluator class declaration, XPathNode definition

### Supporting Files
- `src/xquery/eval/eval_detail.h` - Shared utilities and helper declarations
- `src/xquery/eval/eval_common.cpp` - Common evaluation helpers
- `src/xquery/eval/eval_context.cpp` - Context management
- `src/xquery/eval/eval_navigation.cpp` - Axis evaluation
- `src/xquery/eval/eval_predicates.cpp` - Predicate evaluation
- `src/xquery/xquery.fdl` - XQueryNodeType enum definition
- `src/xml/xpath_value.h` - XPathVal type definition

### Test Files
- `src/xquery/tests/test_*.fluid` - Flute test suite (20 test files)
- W3C test suite (external, referenced by tests)

### Build Files
- `src/xquery/CMakeLists.txt` - XQuery module build configuration

---

## Appendix F: Questions for Code Review

Before merging LR-3, reviewers should verify:

### Functional Correctness
1. Do all 20 XQuery test suites still pass?
2. Are error messages identical to before (or explicitly approved changes)?
3. Are W3C conformance results unchanged?
4. Have new handler methods been tested individually?

### Code Quality
5. Does each handler method have a comprehensive doc comment?
6. Are all handlers under 150 lines (excluding comments)?
7. Is the dispatch table complete (no missing entries)?
8. Have all TODO/FIXME comments been resolved?
9. Does the code follow project coding standards (IS macro, and/or, C-casts)?

### Performance
10. Have hot-path benchmarks been run?
11. Is performance within acceptable ranges (±10%)?
12. Has instruction cache impact been measured?
13. Are there opportunities for further optimisation?

### Documentation
14. Is the eval/README.md comprehensive and accurate?
15. Have optimisation plan updates been made?
16. Is the developer presentation document clear?
17. Are handler contracts well-documented?

### Architecture
18. Is the dispatch mechanism easy to understand?
19. Can new node types be added easily?
20. Is the separation of concerns clear (dispatch vs. handlers)?
21. Are there any remaining large inline functions?

### Testing
22. Have benchmark scripts been added to the repository?
23. Is there a clear rollback plan documented?
24. Have edge cases been tested (null nodes, empty children, etc.)?
