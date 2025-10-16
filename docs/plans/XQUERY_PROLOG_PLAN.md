# XQuery 1.0 Prolog State Implementation Plan

## Assessment of the Existing Implementation

* `xp::Compile()` drives the compilation pipeline by instantiating `XPathTokeniser` and `XPathParser`, then storing the resulting `XPathNode` AST inside a managed resource. The compiled artefact is simply the AST; no additional wrapper object or document-level state exists today.【F:src/xpath/xpath.cpp†L231-L283】
* The parser is exposed through `XPathParser::parse(const std::vector<XPathToken>&)` and only returns the main expression tree. There is no helper such as `startsWithProlog()` nor any prolog-specific entry points.【F:src/xpath/parse/xpath_parser.h†L20-L101】
* The `XPathNode` structure is defined in `src/xpath/xpath.h` and already owns optional metadata fields. It can be extended to hang additional compilation artefacts such as a prolog descriptor by adding new fields.【F:src/xpath/xpath.h†L10-L119】
* At evaluation time `XPathEvaluator::evaluate_function_call()` immediately delegates to the built-in `XPathFunctionLibrary` after evaluating the argument expressions; there is currently no hook for user-defined functions or prolog-driven lookup paths.【F:src/xpath/eval/eval_values.cpp†L2043-L2063】
* `XPathContext` is defined in `src/xpath/api/xpath_functions.h` and is already shared between the evaluator and the function library. Adding an optional pointer to prolog metadata here will make the information visible to every place that needs it.【F:src/xpath/api/xpath_functions.h†L43-L74】

## Implementation Plan

### Guiding Principles

1. Preserve the contract that compiled queries are standalone artefacts reusable across documents.
2. Keep all prolog metadata co-located with the compiled AST so that it survives resource sharing and does not depend on mutable document state.
3. Stage the work so that new parsing capabilities land alongside focused evaluator hooks, keeping regression risk manageable.

### Data Model – `XQueryProlog`

Introduce a dedicated descriptor that lives beside the AST, plus a thin cache wrapper that can live on `extXML` when available:

```cpp
// src/xpath/api/xquery_prolog.h

struct XQueryFunction {
   std::string qname;                                  // full QName, prefix preserved
   std::vector<std::string> parameter_names;           // lexical parameter names
   std::vector<std::string> parameter_types;           // optional sequence types
   std::optional<std::string> return_type;             // optional result sequence type
   std::unique_ptr<XPathNode> body;                    // AST for the function body
   bool is_external = false;                           // reserved for module imports

   [[nodiscard]] std::string signature() const;
};

struct XQueryVariable {
   std::string qname;                                  // includes prefix when present
   std::unique_ptr<XPathNode> initializer;             // null when declared external
   bool is_external = false;
};

struct XQueryModuleImport {
   std::string target_namespace;
   std::vector<std::string> location_hints;
};

struct XQueryModuleCache {
   std::shared_ptr<extXML> owner;                                 // optional document-scoped cache host
   ankerl::unordered_dense::map<std::string, std::shared_ptr<extXML>> modules; // module URI -> compiled module document

   [[nodiscard]] std::shared_ptr<extXML> fetch_or_load(std::string_view uri,
         const XQueryProlog &prolog, XPathErrorReporter &reporter) const;
};

struct XQueryProlog {
   ankerl::unordered_dense::map<std::string, uint32_t> declared_namespaces;  // prefix -> URI hash
   std::optional<uint32_t> default_element_namespace;
   std::optional<uint32_t> default_function_namespace;

   ankerl::unordered_dense::map<std::string, XQueryVariable> variables;      // signature by QName
   ankerl::unordered_dense::map<std::string, XQueryFunction> functions;      // signature "QName/arity"
   std::vector<XQueryModuleImport> module_imports;                           // recorded during parsing

   std::string static_base_uri;
   std::string default_collation;

   enum class BoundarySpace { Preserve, Strip } boundary_space = BoundarySpace::Strip;
   enum class ConstructionMode { Preserve, Strip } construction_mode = ConstructionMode::Strip;
   enum class OrderingMode { Ordered, Unordered } ordering_mode = OrderingMode::Ordered;
   enum class EmptyOrder { Greatest, Least } empty_order = EmptyOrder::Greatest;

   struct CopyNamespaces {
      bool preserve = true;
      bool inherit = true;
   } copy_namespaces;

   ankerl::unordered_dense::map<std::string, DecimalFormat> decimal_formats; // default entry stored with empty key
   ankerl::unordered_dense::map<std::string, std::string> options;

   [[nodiscard]] const XQueryFunction * find_function(std::string_view qname, size_t arity) const;
   [[nodiscard]] const XQueryVariable * find_variable(std::string_view qname) const;
   uint32_t resolve_prefix(std::string_view prefix, const extXML *document) const;
   void bind_module_cache(std::shared_ptr<XQueryModuleCache> cache);

   // Convenience methods for declaration management
   void declare_namespace(std::string_view prefix, std::string_view uri, extXML *document);
   void declare_variable(std::string_view qname, XQueryVariable variable);
   void declare_function(XQueryFunction function);
};
```

Key points:

* Definitions own their AST fragments using `std::unique_ptr<XPathNode>` so they share the same arena and lifetime as the compiled query.
* Namespace hashes continue to use `pf::strhash()` via the existing `extXML` helpers when we need to populate `declared_namespaces`.
* Decimal format handling is captured explicitly so that later work on `format-number()` can reference the declaration set.
* Module imports are retained even when the document cache is absent, ensuring queries compiled without a document still convey intent once run with a cache. When an `extXML` is available the cache wrapper simply fronts the existing `DocumentCache` map so imported modules stay warm for subsequent expressions.【F:src/xml/xml.h†L166-L168】

### Attaching Prolog Data to the AST

Extend `src/xpath/xpath.h` so that `XPathNode` gains optional shared pointers to `XQueryProlog` and the module cache plus helpers:

```cpp
std::shared_ptr<XQueryProlog> prolog; // new field
std::shared_ptr<XQueryModuleCache> module_cache; // optional document-scoped cache

inline void set_prolog(std::shared_ptr<XQueryProlog> value) { prolog = std::move(value); }
[[nodiscard]] inline std::shared_ptr<XQueryProlog> get_prolog() const { return prolog; }
inline void set_module_cache(std::shared_ptr<XQueryModuleCache> value) { module_cache = std::move(value); }
[[nodiscard]] inline std::shared_ptr<XQueryModuleCache> get_module_cache() const { return module_cache; }
```

Shared pointers keep the metadata and cache handles alive even when multiple `XPathNode` copies or slices exist (e.g., when function bodies are cloned during parsing).

### Parser and Tokeniser Refactor

1. Extend `XPathTokeniser` to recognise the prolog keywords (`declare`, `namespace`, `default`, `function`, `variable`, `option`, `boundary-space`, `base-uri`, `construction`, `ordering`, `empty`, `copy-namespaces`, `decimal-format`, `import`). Tokens for these entries should be added to `XPathTokenType` so the parser can branch cleanly.【F:src/xpath/parse/xpath_tokeniser.h†L17-L44】【F:src/xpath/parse/xpath_ast.h†L17-L98】
2. Introduce `struct ParseResult { std::unique_ptr<XPathNode> expression; std::shared_ptr<XQueryProlog> prolog; std::shared_ptr<XQueryModuleCache> module_cache; };` and change `XPathParser::parse()` to return this result. When no prolog is present `prolog` remains `nullptr` and the cache pointer is empty, allowing existing callers to continue to work after a small call-site adjustment.
3. Implement dedicated parsing routines for each prolog construct (`parse_prolog()`, `parse_namespace_decl()`, `parse_function_decl()`, etc.) that populate the shared `XQueryProlog` instance. These routines should reuse the existing expression parser to build the AST fragments used by function bodies and variable initialisers, and they should record module imports (URI plus optional location hints) in `module_imports`.
4. Make name resolution consistent by recording namespace bindings in `XQueryProlog` as soon as they are parsed. The parser must apply `default function namespace` when constructing function QNames so that lookup can rely on canonical signatures, and it should normalise module import namespaces using the same tables so cache lookups are deterministic.

### Compilation Pipeline Updates

* Adjust `xp::Compile()` to consume the new `ParseResult`. After tokenisation it should:
  * Create the `ParseResult`.
  * Move the expression AST into the managed `XPathNode` resource as today.
  * Call `compiled_node->set_prolog(result.prolog);` before returning so evaluation can see the metadata.
  * When `objXML *XML` is an `extXML`, obtain or create a shared `XQueryModuleCache` rooted in the document's existing `DocumentCache` field and call `compiled_node->set_module_cache(cache);`. When the pointer is null the cache remains empty, mirroring today's behaviour where document-dependent features are disabled.【F:src/xml/xml.h†L166-L168】
* Error reporting remains the same, but when prolog parsing fails the aggregated error messages should surface both the declaration type and the offending QName to aid debugging.

### Runtime Integration

1. Add `std::shared_ptr<XQueryProlog> prolog;` and `std::shared_ptr<XQueryModuleCache> module_cache;` to `XPathContext`. The constructor in `xpath_functions.h` should accept extra optional arguments so that evaluators can share the metadata and cache handle with the function library.【F:src/xpath/api/xpath_functions.h†L43-L74】
2. In `XPathEvaluator::XPathEvaluator` set `context.prolog = query_root.get_prolog();` and `context.module_cache = query_root.get_module_cache();` whenever evaluation begins. This requires threading the compiled query pointer into the evaluator entry points (`evaluate_xpath_expression` and `find_tag`) so that the context is initialised before expression dispatch.【F:src/xpath/eval/eval.cpp†L21-L76】【F:src/xpath/eval/eval_context.cpp†L1-L120】
3. Extend `evaluate_function_call()` so it performs lookup in the prolog before falling back to the built-in library:
   * Resolve the lexical QName using the namespace table from `XQueryProlog`.
   * Materialise argument values (already done today).
   * When a user-defined function is found, create a small evaluation frame: push a context, bind parameters using `VariableBindingGuard`, and evaluate the stored body AST.
   * On `import module` declarations, resolve or load the referenced module using `context.module_cache` so that repeated queries benefit from the per-document cache originally envisaged. When no cache is available, emit the current unsupported feature diagnostics.
   * Enforce `is_external` and arity checks up front. When unsupported constructs are encountered, call `record_error()` to retain current diagnostics behaviour.
4. Implement a helper for variable lookup that first checks the dynamic environment (`context.variables`) and then consults the prolog’s `variables` map for initialised declarations. When module variables are requested, resolve the owning module through `context.module_cache`. External variables remain unresolved and should trigger the existing unsupported-path error until a binding API is introduced.
5. Honour prolog settings where the runtime already exposes hooks:
   * `boundary-space`, `construction`, `ordering`, and `empty order` affect constructors and sequence operators handled in `eval_expression.cpp` and `eval_values.cpp`.
   * `default collation` and decimal formats should be plumbed into the relevant formatting helpers during later phases, but the settings should be stored now so that the evaluator can read them once support is implemented.

### Helper Method Implementations

The following helper methods should be implemented in `src/xpath/api/xquery_prolog.cpp` to provide a clean API for prolog management:

#### Namespace Resolution with Fallback

```cpp
// XQueryProlog::resolve_prefix() - Two-tier lookup strategy
uint32_t XQueryProlog::resolve_prefix(std::string_view prefix, const extXML *document) const
{
   // First check prolog-declared namespaces (query-specific bindings)
   auto it = declared_namespaces.find(std::string(prefix));
   if (it != declared_namespaces.end()) {
      return it->second;
   }

   // Fall back to document-level namespace registry (inherited bindings)
   if (document) {
      auto doc_it = document->Prefixes.find(std::string(prefix));
      if (doc_it != document->Prefixes.end()) {
         return doc_it->second;
      }
   }

   return 0; // No namespace found
}
```

This two-tier lookup ensures that prolog-declared namespaces override document namespaces whilst still allowing queries to use existing document bindings, providing correct XQuery semantics.

#### Declaration Convenience Methods

```cpp
void XQueryProlog::declare_namespace(std::string_view prefix, std::string_view uri, extXML *document)
{
   uint32_t uri_hash = pf::strhash(uri);

   // Register in prolog state
   declared_namespaces[std::string(prefix)] = uri_hash;

   // Optionally register in document namespace registry for consistency
   if (document) {
      document->registerNamespace(std::string(uri));
      document->Prefixes[std::string(prefix)] = uri_hash;
   }
}

void XQueryProlog::declare_variable(std::string_view qname, XQueryVariable variable)
{
   variables[std::string(qname)] = std::move(variable);
}

void XQueryProlog::declare_function(XQueryFunction function)
{
   auto signature = function.signature();
   functions[signature] = std::move(function);
}
```

These convenience methods encapsulate the logic for registering declarations and maintaining consistency between the prolog state and document registries when a document is available.

#### Function and Variable Lookup

```cpp
const XQueryFunction * XQueryProlog::find_function(std::string_view qname, size_t arity) const
{
   // Build lookup signature: "QName/arity"
   std::string signature = std::string(qname) + "/" + std::to_string(arity);

   auto it = functions.find(signature);
   if (it != functions.end()) {
      return &it->second;
   }

   return nullptr;
}

const XQueryVariable * XQueryProlog::find_variable(std::string_view qname) const
{
   auto it = variables.find(std::string(qname));
   if (it != variables.end()) {
      return &it->second;
   }

   return nullptr;
}
```

### User-Defined Function Evaluation

Extend `XPathEvaluator` with a method to evaluate user-defined functions:

```cpp
// xpath_evaluator.cpp

XPathVal XPathEvaluator::evaluate_function_call(const XPathNode *func_node, uint32_t current_prefix)
{
   const auto &func_name = func_node->function_call.name;
   const auto &arguments = func_node->function_call.arguments;

   // First: Try user-defined functions if prolog state exists
   if (context.prolog) {
      auto udf = context.prolog->find_function(func_name, arguments.size());
      if (udf) {
         return evaluate_user_defined_function(*udf, arguments, current_prefix);
      }
   }

   // Second: Fall back to built-in function library
   auto &library = XPathFunctionLibrary::instance();
   if (library.has_function(func_name)) {
      std::vector<XPathVal> arg_values;
      arg_values.reserve(arguments.size());
      for (const auto* arg_node : arguments) {
         arg_values.push_back(evaluate_expression(arg_node, current_prefix));
      }
      return library.call_function(func_name, arg_values, context);
   }

   // Function not found
   record_error("Unknown function: " + std::string(func_name), func_node);
   return XPathVal::empty_sequence();
}

XPathVal XPathEvaluator::evaluate_user_defined_function(
   const XQueryFunction &udf,
   const std::vector<const XPathNode *> &arguments,
   uint32_t current_prefix)
{
   if (arguments.size() != udf.parameter_names.size()) {
      record_error("Function " + udf.qname + " expects " +
                   std::to_string(udf.parameter_names.size()) + " arguments, got " +
                   std::to_string(arguments.size()));
      return XPathVal::empty_sequence();
   }

   // Create local variable bindings for parameters
   std::vector<std::unique_ptr<VariableBindingGuard>> bindings;
   bindings.reserve(udf.parameter_names.size());

   for (size_t i = 0; i < arguments.size(); ++i) {
      XPathVal arg_value = evaluate_expression(arguments[i], current_prefix);

      // TODO: Type checking if parameter_types[i] is specified

      bindings.push_back(std::make_unique<VariableBindingGuard>(
         context, udf.parameter_names[i], std::move(arg_value)
      ));
   }

   // Evaluate function body
   XPathVal result = evaluate_expression(udf.body.get(), current_prefix);

   // TODO: Type checking if return_type is specified

   return result;
   // Bindings automatically restored by VariableBindingGuard destructors
}
```

### Usage Examples

#### Example 1: Basic User-Defined Function

```xquery
declare function local:square($x as xs:integer) as xs:integer {
   $x * $x
};

local:square(5)
```

**Prolog Processing:**
During parsing, the `local:square` function declaration is captured:
- Function name: `"local:square"`
- Parameter: `"x"` with type `"xs:integer"`
- Return type: `"xs:integer"`
- Body: AST representing `$x * $x`

Stored in prolog with signature `"local:square/1"`.

**Evaluation:**
When evaluating `local:square(5)`:
1. `XPathEvaluator::evaluate_function_call()` checks `context.prolog`
2. Finds UDF with signature `"local:square/1"`
3. Binds parameter `$x = 5` using `VariableBindingGuard`
4. Evaluates body expression `$x * $x`
5. Returns `25`

#### Example 2: Namespace and Variable Declarations

```xquery
declare namespace ex = "http://example.com/ns";
declare variable $threshold := 100;

//ex:item[@value > $threshold]
```

**Prolog Processing:**
- Namespace declaration: Registers prefix `"ex"` → URI hash via `declare_namespace()`
- Variable declaration: Creates `XQueryVariable` with initializer AST for `100`
- Variable stored with QName `"threshold"`

**Evaluation:**
During XPath evaluation:
- Prefix `"ex"` resolved via `prolog->resolve_prefix("ex", document)`
- Variable `$threshold` looked up via `prolog->find_variable("threshold")`
- Comparison uses initialised value `100`

#### Example 3: Boundary-Space Declaration

```xquery
declare boundary-space preserve;

<result>{
   <item>text</item>
}</result>
```

**Prolog Processing:**
Parser sets `prolog->boundary_space = BoundarySpace::Preserve`

**Effect:**
Whitespace in constructed elements is preserved rather than stripped during normalisation, affecting the element constructor's behavior in the evaluator.

#### Example 4: Recursive Function

```xquery
declare function local:factorial($n as xs:integer) as xs:integer {
   if ($n <= 1) then 1
   else $n * local:factorial($n - 1)
};

local:factorial(5)
```

**Evaluation Process:**
Call stack demonstrates proper scoping with `VariableBindingGuard`:
```
local:factorial(5)
  -> 5 * local:factorial(4)
      -> 4 * local:factorial(3)
          -> 3 * local:factorial(2)
              -> 2 * local:factorial(1)
                  -> returns 1
              -> returns 2
          -> returns 6
      -> returns 24
  -> returns 120
```

Each recursive call creates new parameter bindings that are automatically cleaned up when the call frame exits, ensuring proper variable scoping without manual stack management.

### Migration & Staging

1. **Phase 1 – Infrastructure**
   * Introduce `XQueryProlog` and `XQueryModuleCache`, extend `XPathNode`, teach `xp::Compile()` to attach the descriptors, and modify `XPathContext` to store the shared pointers. No prolog declarations are yet parsed; the structures simply travel alongside the AST and the module cache hooks into `extXML::DocumentCache` when available.【F:src/xml/xml.h†L166-L168】
2. **Phase 2 – Parsing & Storage**
   * Add tokeniser and parser support for namespace, variable, function, and module declarations. Populate `XQueryProlog`, thread cache bindings when an `extXML` is supplied, and write unit tests that confirm parsed metadata is preserved across compilation.
3. **Phase 3 – Evaluation**
   * Extend the evaluator to resolve variables and functions from the prolog. Start with inline functions, non-external variables, and module lookups via the cache; defer decimal formats and options to subsequent iterations.

Each phase should land with targeted regression tests using existing Fluid-based XPath suites or new C++ unit tests in `src/xpath/tests/` to confirm behaviour.

### Task Checklist

#### Phase 1: Core Infrastructure

**Data Structures:**
- [ ] Create `src/xpath/api/xquery_prolog.h` with structure definitions:
  - [ ] Define `XQueryFunction` struct with signature generation
  - [ ] Define `XQueryVariable` struct with initializer support
  - [ ] Define `XQueryModuleImport` struct
  - [ ] Define `XQueryModuleCache` struct with fetch/load capability
  - [ ] Define `DecimalFormat` struct
  - [ ] Define `XQueryProlog` struct with all declaration maps and settings

**Helper Methods (xquery_prolog.cpp):**
- [ ] Implement `XQueryProlog::find_function(qname, arity)`
- [ ] Implement `XQueryProlog::find_variable(qname)`
- [ ] Implement `XQueryProlog::resolve_prefix(prefix, document)` with fallback logic
- [ ] Implement `XQueryProlog::declare_namespace(prefix, uri, document)`
- [ ] Implement `XQueryProlog::declare_variable(qname, variable)`
- [ ] Implement `XQueryProlog::declare_function(function)`
- [ ] Implement `XQueryProlog::bind_module_cache(cache)`
- [ ] Implement `XQueryFunction::signature()` method
- [ ] Implement `XQueryModuleCache::fetch_or_load(uri, prolog, reporter)`

**AST Integration:**
- [ ] Extend `src/xpath/xpath.h` to add `std::shared_ptr<XQueryProlog> prolog` field
- [ ] Extend `src/xpath/xpath.h` to add `std::shared_ptr<XQueryModuleCache> module_cache` field
- [ ] Add `set_prolog()` / `get_prolog()` inline helpers to `src/xpath/xpath.h`
- [ ] Add `set_module_cache()` / `get_module_cache()` inline helpers to `src/xpath/xpath.h`
- [ ] Verify build succeeds with updated `XPathNode` definition

**Context Threading:**
- [ ] Add `std::shared_ptr<XQueryProlog> prolog` to `XPathContext` structure
- [ ] Add `std::shared_ptr<XQueryModuleCache> module_cache` to `XPathContext` structure
- [ ] Update `XPathContext` constructor to accept optional prolog and cache parameters

**Compilation Pipeline:**
- [ ] Update `xp::Compile()` to work with `ParseResult` instead of raw AST
- [ ] Implement prolog attachment: `compiled_node->set_prolog(result.prolog)`
- [ ] Implement module cache creation from `extXML::DocumentCache` when document provided
- [ ] Implement module cache attachment: `compiled_node->set_module_cache(cache)`
- [ ] Update error reporting to surface prolog parsing failures with declaration context

#### Phase 2: Parsing Support

**Tokeniser Extensions:**
- [ ] Add `DECLARE` token type to `XPathTokenType`
- [ ] Add `NAMESPACE` token type
- [ ] Add `FUNCTION` token type
- [ ] Add `VARIABLE` token type
- [ ] Add `EXTERNAL` token type
- [ ] Add `BOUNDARY_SPACE` token type
- [ ] Add `BASE_URI` token type
- [ ] Add `CONSTRUCTION` token type
- [ ] Add `ORDERING` token type
- [ ] Add `DEFAULT` token type
- [ ] Add `COLLATION` token type
- [ ] Add `COPY_NAMESPACES` token type
- [ ] Add `DECIMAL_FORMAT` token type
- [ ] Add `OPTION` token type
- [ ] Add `IMPORT` token type
- [ ] Add `MODULE` token type
- [ ] Add `SCHEMA` token type
- [ ] Update `XPathTokeniser` keyword recognition logic

**Parser Structure:**
- [ ] Define `ParseResult` structure with expression, prolog, and cache fields
- [ ] Change `XPathParser::parse()` return type to `ParseResult`
- [ ] Update all parse call sites to handle `ParseResult`
- [ ] Implement `parse_prolog()` main entry point
- [ ] Implement prolog detection logic (check for `declare` keyword)

**Parser Declaration Handlers:**
- [ ] Implement `parse_namespace_decl()` - `declare namespace prefix = "uri"`
- [ ] Implement `parse_default_namespace_decl()` - `declare default element/function namespace`
- [ ] Implement `parse_variable_decl()` - `declare variable $name := expr` or `external`
- [ ] Implement `parse_function_decl()` - `declare function name($params) { body }`
- [ ] Implement `parse_boundary_space_decl()` - `declare boundary-space preserve|strip`
- [ ] Implement `parse_base_uri_decl()` - `declare base-uri "uri"`
- [ ] Implement `parse_construction_decl()` - `declare construction preserve|strip`
- [ ] Implement `parse_ordering_decl()` - `declare ordering ordered|unordered`
- [ ] Implement `parse_empty_order_decl()` - `declare default order empty greatest|least`
- [ ] Implement `parse_copy_namespaces_decl()` - `declare copy-namespaces preserve|no-preserve, inherit|no-inherit`
- [ ] Implement `parse_decimal_format_decl()` - `declare decimal-format ...`
- [ ] Implement `parse_option_decl()` - `declare option name "value"`
- [ ] Implement `parse_import_module_decl()` - `import module namespace prefix = "uri" at "location"`
- [ ] Implement `parse_import_schema_decl()` - `import schema ...` (stub for future)

**Name Resolution During Parsing:**
- [ ] Apply namespace bindings immediately when parsed
- [ ] Normalise function QNames using `default function namespace`
- [ ] Normalise module import namespaces for cache lookup consistency
- [ ] Record base URI in prolog when inherited from document

#### Phase 3: Evaluation Integration

**Evaluator Threading:**
- [ ] Update `XPathEvaluator` constructor to accept compiled query root
- [ ] Set `context.prolog = query_root.get_prolog()` in evaluator initialisation
- [ ] Set `context.module_cache = query_root.get_module_cache()` in evaluator initialisation
- [ ] Thread compiled query pointer through `evaluate_xpath_expression()` entry point
- [ ] Thread compiled query pointer through `find_tag()` entry point

**Function Resolution:**
- [ ] Update `evaluate_function_call()` to check prolog before built-in library
- [ ] Implement `evaluate_user_defined_function()` method
- [ ] Implement parameter binding with `VariableBindingGuard`
- [ ] Implement function body evaluation with proper scoping
- [ ] Add arity checking and error reporting
- [ ] Add external function detection and error handling
- [ ] Support recursive function calls

**Variable Resolution:**
- [ ] Implement variable lookup helper checking dynamic context first
- [ ] Extend variable lookup to consult prolog declared variables
- [ ] Evaluate variable initialiser expressions when needed
- [ ] Add external variable detection and error handling
- [ ] Integrate module variable resolution via cache

**Module Resolution:**
- [ ] Implement module cache lookup in `evaluate_function_call()`
- [ ] Implement deferred module loading via `fetch_or_load()`
- [ ] Add module import error handling
- [ ] Add unsupported feature diagnostics when cache unavailable

**Prolog Settings Enforcement:**
- [ ] Implement `boundary-space` behavior in element constructors
- [ ] Implement `construction` mode in element constructors
- [ ] Implement `ordering` mode in sequence operations
- [ ] Implement `empty order` in sorting operations
- [ ] Store `default collation` for future use
- [ ] Store decimal formats for future `format-number()` support

#### Phase 4: Testing and Validation

**Unit Tests (src/xpath/tests/ or C++):**
- [ ] Test prolog structure creation and destruction
- [ ] Test namespace declaration and resolution
- [ ] Test variable declaration and initialisation
- [ ] Test function declaration and signature generation
- [ ] Test helper methods (find_function, find_variable, resolve_prefix)
- [ ] Test convenience methods (declare_namespace, declare_variable, declare_function)

**Flute Integration Tests (test_xquery_prolog.fluid):**
- [ ] Test basic user-defined function (Example 1: square function)
- [ ] Test namespace and variable declarations (Example 2)
- [ ] Test boundary-space preservation (Example 3)
- [ ] Test recursive functions (Example 4: factorial)
- [ ] Test function overloading by arity
- [ ] Test namespace fallback from document to prolog
- [ ] Test variable shadowing (prolog vs dynamic context)
- [ ] Test external variable error handling
- [ ] Test external function error handling
- [ ] Test module import declarations (structural test, not loading)

**Regression Tests:**
- [ ] Verify existing XPath expressions continue to work
- [ ] Verify queries without prolog incur no overhead
- [ ] Verify compiled queries work across multiple documents
- [ ] Verify module cache is document-scoped when provided
- [ ] Verify queries compiled without document work correctly

**Error Handling Tests:**
- [ ] Test duplicate namespace declarations
- [ ] Test duplicate variable declarations
- [ ] Test duplicate function declarations (same signature)
- [ ] Test invalid prolog syntax
- [ ] Test missing required parameters in function calls
- [ ] Test type annotation parsing (even if validation deferred)

#### Documentation

- [ ] Document `XQueryProlog` API in embedded comments
- [ ] Document helper method usage patterns
- [ ] Add usage examples to API documentation
- [ ] Update XPath module documentation to mention XQuery prolog support
- [ ] Document limitations (external functions/variables, module loading)
