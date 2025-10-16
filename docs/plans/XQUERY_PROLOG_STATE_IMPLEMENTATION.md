# XQuery 1.0 Prolog State Implementation Plan

## Assessment of the Existing Implementation

* `xp::Compile()` drives the compilation pipeline by instantiating `XPathTokeniser` and `XPathParser`, then storing the resulting `XPathNode` AST inside a managed resource. The compiled artefact is simply the AST; no additional wrapper object or document-level state exists today.【F:src/xpath/xpath.cpp†L231-L283】
* The parser is exposed through `XPathParser::parse(const std::vector<XPathToken>&)` and only returns the main expression tree. There is no helper such as `startsWithProlog()` nor any prolog-specific entry points.【F:src/xpath/parse/xpath_parser.h†L20-L101】
* The generated `XPathNode` structure (defined in `xpath.fdl`) already owns optional metadata fields and can be extended, making it the natural place to hang additional compilation artefacts such as a prolog descriptor.【F:src/xpath/xpath.fdl†L9-L116】
* At evaluation time `XPathEvaluator::evaluate_function_call()` immediately delegates to the built-in `XPathFunctionLibrary` after evaluating the argument expressions; there is currently no hook for user-defined functions or prolog-driven lookup paths.【F:src/xpath/eval/eval_values.cpp†L2043-L2063】
* `XPathContext` is defined in `src/xpath/api/xpath_functions.h` and is already shared between the evaluator and the function library. Adding an optional pointer to prolog metadata here will make the information visible to every place that needs it.【F:src/xpath/api/xpath_functions.h†L43-L74】

## Issues in the Original Proposal

1. **Lifetime management** – The compiled artefact still needs to function without a backing document, but we must not lose the opportunity to cache prolog state when `extXML` *is* provided. `xp::Compile()` accepts a nullable `objXML*` and produces ASTs that are documented as reusable across documents. The revised plan therefore keeps the canonical prolog representation on the AST while also defining an opt-in cache that attaches to the supplied `extXML`, allowing future queries against the same document to reuse module imports and other derived artefacts. When compilation happens without a document the cache layer is simply skipped.【F:src/xpath/xpath.cpp†L231-L283】【F:src/xml/xml.h†L137-L228】
2. **Parser entry points** – The proposed `startsWithProlog()` and `parseProlog()` helpers do not exist. The parser currently exposes a single `parse()` method and would need a deliberate refactor to surface prolog handling rather than ad hoc checks at call sites.【F:src/xpath/parse/xpath_parser.h†L20-L101】
3. **Base URI handling** – The suggested `extXML::ensureProlog()` references `CurrentBase`, a field that only exists on the temporary `ParseState` used during XML parsing, not on `extXML` itself. Any base-URI inheritance therefore needs to be recorded explicitly in the prolog object when compilation happens, not by poking at `extXML` internals.【F:src/xml/xml.h†L20-L137】
4. **Evaluator integration** – Updating `XPathEvaluator` via `context.prolog_state = &XML->prolog_state` assumes the state lives on the document. With the AST-centric design this pointer should instead be derived from the compiled `XPathNode` that is being executed.【F:src/xpath/eval/eval.cpp†L28-L61】【F:src/xpath/eval/eval_values.cpp†L2043-L2063】
5. **Tokeniser naming** – The proposal refers to `XPathTokenizer`; the actual class is `XPathTokeniser` (British spelling). Using the existing name avoids churn and keeps the codebase consistent.【F:src/xpath/parse/xpath_tokeniser.h†L1-L45】

## Revised Implementation Plan

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
};
```

Key points:

* Definitions own their AST fragments using `std::unique_ptr<XPathNode>` so they share the same arena and lifetime as the compiled query.
* Namespace hashes continue to use `pf::strhash()` via the existing `extXML` helpers when we need to populate `declared_namespaces`.
* Decimal format handling is captured explicitly so that later work on `format-number()` can reference the declaration set.
* Module imports are retained even when the document cache is absent, ensuring queries compiled without a document still convey intent once run with a cache. When an `extXML` is available the cache wrapper simply fronts the existing `DocumentCache` map so imported modules stay warm for subsequent expressions.【F:src/xml/xml.h†L166-L168】

### Attaching Prolog Data to the AST

Extend `xpath.fdl` so that `XPathNode` gains optional shared pointers to `XQueryProlog` and the module cache plus helpers:

```cpp
std::shared_ptr<XQueryProlog> prolog; // new field
std::shared_ptr<XQueryModuleCache> module_cache; // optional document-scoped cache

inline void set_prolog(std::shared_ptr<XQueryProlog> value) { prolog = std::move(value); }
[[nodiscard]] inline std::shared_ptr<XQueryProlog> get_prolog() const { return prolog; }
inline void set_module_cache(std::shared_ptr<XQueryModuleCache> value) { module_cache = std::move(value); }
[[nodiscard]] inline std::shared_ptr<XQueryModuleCache> get_module_cache() const { return module_cache; }
```

Because the structure is generated, this change happens in `xpath.fdl` and will be reflected in the regenerated header. Shared pointers keep the metadata and cache handles alive even when multiple `XPathNode` copies or slices exist (e.g., when function bodies are cloned during parsing).

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

### Migration & Staging

1. **Phase 1 – Infrastructure**
   * Introduce `XQueryProlog` and `XQueryModuleCache`, extend `XPathNode`, teach `xp::Compile()` to attach the descriptors, and modify `XPathContext` to store the shared pointers. No prolog declarations are yet parsed; the structures simply travel alongside the AST and the module cache hooks into `extXML::DocumentCache` when available.【F:src/xml/xml.h†L166-L168】
2. **Phase 2 – Parsing & Storage**
   * Add tokeniser and parser support for namespace, variable, function, and module declarations. Populate `XQueryProlog`, thread cache bindings when an `extXML` is supplied, and write unit tests that confirm parsed metadata is preserved across compilation.
3. **Phase 3 – Evaluation**
   * Extend the evaluator to resolve variables and functions from the prolog. Start with inline functions, non-external variables, and module lookups via the cache; defer decimal formats and options to subsequent iterations.

Each phase should land with targeted regression tests using existing Fluid-based XPath suites or new C++ unit tests in `src/xpath/tests/` to confirm behaviour.

### Task Checklist

- [ ] Create `src/xpath/api/xquery_prolog.h/.cpp` with the data structures and helper methods described above, including module cache support.
- [ ] Extend `xpath.fdl` and regenerate headers so `XPathNode` stores shared pointers to the prolog metadata and optional module cache.
- [ ] Update `XPathTokeniser` and `XPathParser` to return `ParseResult` and populate the prolog descriptor and cache bindings.
- [ ] Adjust `xp::Compile()` to attach the parsed prolog and cache handles to the compiled `XPathNode`, using the provided `extXML` when present.
- [ ] Thread the prolog and cache pointers through `XPathContext` and `XPathEvaluator`.
- [ ] Implement function and variable lookup helpers that consult `XQueryProlog` before falling back to the built-in library, resolving module references via the cache.
- [ ] Add focused tests covering namespace declarations, inline variables, and user-defined functions once parsing support is complete.

This staged plan keeps the compiled-query contract intact whilst opening the door to full XQuery prolog support.
