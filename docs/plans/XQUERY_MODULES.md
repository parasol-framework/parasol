# XQuery Module Loading Implementation Plan

## Overview

This document outlines the implementation plan for dynamic module loading to enable `import module` declarations in XQuery. This builds on the existing XQuery prolog infrastructure (Phase 1-3 complete) by adding actual file loading, library module parsing, and runtime resolution.

## Current Implementation Status

### Already Implemented (Phase 1-3)

- ✅ `XQueryModuleCache` structure with stub `fetch_or_load()`
- ✅ `XQueryModuleImport` parsing and storage
- ✅ Module cache attached to AST and context (`XQueryProlog::bind_module_cache` / `get_module_cache`)
- ✅ Integration with `extXML::DocumentCache`
- ✅ Error handling when cache unavailable (placeholder diagnostics in evaluators)
- ✅ Module import declarations parsed and recorded by `XPathParser::parse_import_statement`
- ✅ Basic validation that module cache exists when imports resolve

### Newly Implemented

- ✅ Phase C runtime integration: evaluator resolves module functions and variables via the shared cache
- ✅ Module variable recursion guard now canonicalises QName lookups so aliased imports detect circular dependencies

### Repository Reality Check

Before expanding the plan, confirm the starting point in the tree:

- `src/xpath/api/xquery_prolog.h/.cpp` currently store modules as `ankerl::unordered_dense::map<std::string, std::shared_ptr<extXML>>` and stub out `fetch_or_load()`.
- `src/xpath/parse/xpath_parser.cpp` has no notion of `module namespace` declarations yet and always treats the parsed file as a main module.
- Runtime lookups (`XPathEvaluator::resolve_user_defined_function` and `XPathEvaluator::resolve_variable_value`) detect imported namespaces but deliberately emit "not implemented" errors.

The steps below extend these specific seams instead of introducing new entry points.

### What Needs Implementation

This plan focuses on making module loading fully functional, transforming the current stub implementation into a complete module system.

## Implementation Phases

### Phase A: Library Module Parser Support

#### 1. Module Declaration Parsing

Library modules in XQuery begin with a module declaration:

```xquery
module namespace prefix = "http://example.com/namespace";

declare function prefix:foo($x) { $x + 1 };
```

Main modules (queries) have an optional prolog followed by a query expression:

```xquery
import module namespace math = "http://example.com/math";

math:square(5)
```

**Parser Changes Required:**

Extend `struct XQueryProlog` with module metadata:

```cpp
struct XQueryProlog {
   // ... existing fields ...

   bool is_library_module = false;
   std::optional<std::string> module_namespace_uri;
   std::optional<std::string> module_namespace_prefix;

   bool validate_library_exports() const;
};
```

Update `src/xpath/parse/xpath_parser.cpp`:

```cpp
bool XPathParser::parse(XPathParseResult &result) {
   // When the very first tokens read "module namespace" invoke parse_module_decl().
   // Library modules stop after the prolog; main modules continue into the query body.
}

bool XPathParser::parse_module_decl(XQueryProlog &prolog) {
   // Expect: module namespace prefix = "uri";
   // Register the namespace immediately via prolog.declare_namespace().
   // Mark prolog.is_library_module and capture prefix/URI for later validation.
}
```

**Validation Rules:**

- Library modules must begin with `module namespace` declaration
- Library modules cannot have a query expression (only prolog)
- All functions and variables in library module must be in the module's target namespace (XQST0048)
- Main modules cannot have `module namespace` declaration
- Main modules must have a query expression after prolog

#### 2. Export Validation

Implement `validate_library_exports()` to ensure XQST0048 compliance:

```cpp
bool XQueryProlog::validate_library_exports() const {
   if (!is_library_module or !module_namespace_uri) return true;

   std::string ns_prefix = *module_namespace_prefix + ":";

   // Check all functions start with module prefix
   for (const auto &[sig, func] : functions) {
      if (!func.qname.starts_with(ns_prefix)) {
         return false; // XQST0048 error
      }
   }

   // Check all variables start with module prefix
   for (const auto &[name, var] : variables) {
      if (!var.qname.starts_with(ns_prefix)) {
         return false; // XQST0048 error
      }
   }

   return true;
}
```

Call this validation after parsing completes and before returning `ParseResult`.

### Phase B: Module Loading Infrastructure

#### 3. Enhanced Module Cache Structure

Update `XQueryModuleCache` (in `src/xpath/api/xquery_prolog.h/.cpp`) to retain additional metadata alongside the cached `extXML`:

```cpp
struct ModuleInfo {
   std::shared_ptr<extXML> document;     // Parsed XML document object
   XPathNode *compiled_query = nullptr;  // Managed resource with compiled prolog
   std::shared_ptr<XQueryProlog> prolog; // Direct access to module's prolog
};

struct XQueryModuleCache {
   std::shared_ptr<extXML> owner;
   mutable ankerl::unordered_dense::map<std::string, ModuleInfo> modules; // keyed by namespace URI
   mutable std::unordered_set<std::string> loading_in_progress;           // circular dependency detection

   [[nodiscard]] std::shared_ptr<extXML> fetch_or_load(std::string_view uri,
      const XQueryProlog &prolog, XPathErrorReporter &reporter) const;

   [[nodiscard]] const ModuleInfo * find_module(std::string_view uri) const;
};
```

#### 4. URI Resolution Helper

Add URI resolution utilities (in `xquery_prolog.cpp` or new `module_loader.cpp`):

```cpp
// Resolve location hints against base URI
std::optional<std::string> resolve_module_location(
   std::string_view target_namespace,
   const std::vector<std::string> &location_hints,
   std::string_view base_uri)
{
   for (const auto &hint : location_hints) {
      std::string resolved;

      // If absolute URI, use as-is
      if (xml::uri::is_absolute_uri(hint)) {
         resolved = hint;
      }
      // Otherwise resolve against base URI
      else if (!base_uri.empty()) {
         resolved = xml::uri::resolve_relative_uri(hint, base_uri);
      }
      else {
         resolved = hint;
      }

      // Normalise separators
      resolved = xml::uri::normalise_uri_separators(resolved);

      // Check if file exists (for file:// URIs)
      // Return first valid location
      if (file_exists_or_accessible(resolved)) {
         return resolved;
      }
   }

   return std::nullopt; // No valid location found
}

// Check if a URI/path is accessible
bool file_exists_or_accessible(std::string_view uri) {
   // Strip file:// prefix if present
   // Use filesystem check or HTTP HEAD request
   // For Phase 1, support only local file:// and plain paths
}

// Helper mirroring xml::uri style for stripping "file:" prefixes
std::string strip_file_scheme(std::string_view uri);
```

#### 5. Complete `fetch_or_load()` Implementation

Replace the stub implementation in `xquery_prolog.cpp`:

```cpp
std::shared_ptr<extXML> XQueryModuleCache::fetch_or_load(
   std::string_view uri,
   const XQueryProlog &prolog,
   XPathErrorReporter &reporter) const
{
   if (uri.empty()) return nullptr;

   std::string uri_key(uri);

   // Step 1: Check if already loaded in module cache
   auto found = modules.find(uri_key);
   if (found != modules.end()) {
      return found->second.document;
   }

   // Step 2: Check document cache (for pre-loaded modules)
   if (owner) {
      auto doc_cache = owner->DocumentCache.find(uri_key);
      if (doc_cache != owner->DocumentCache.end()) {
         // TODO: Extract prolog and store in modules map
         return doc_cache->second;
      }
   }

   // Step 3: Detect circular dependencies
   if (loading_in_progress.contains(uri_key)) {
      reporter.record_error("Circular module dependency detected: " + uri_key);
      return nullptr;
   }

   // Step 4: Find matching import declaration
   const XQueryModuleImport *import_decl = nullptr;
   for (const auto &imp : prolog.module_imports) {
      if (imp.target_namespace == uri_key) {
         import_decl = &imp;
         break;
      }
   }

   if (!import_decl) {
      reporter.record_error("No import declaration found for: " + uri_key);
      return nullptr;
   }

   // Step 5: Resolve location from hints
   auto location = resolve_module_location(
      uri_key,
      import_decl->location_hints,
      prolog.static_base_uri);

   if (!location) {
      reporter.record_error("Cannot resolve module location for: " + uri_key);
      return nullptr;
   }

   // Step 6: Mark as loading to detect cycles
   loading_in_progress.insert(uri_key);

   // Step 7: Load file content
   std::string file_path = strip_file_prefix(*location);
   APTR file_cache = nullptr;
   std::string content;

   if (LoadFile(file_path.c_str(), LDF::NIL, &file_cache) == ERR::Okay) {
      content.assign((char*)file_cache->Data, file_cache->Size);
      UnloadFile(file_cache);
   }
   else {
      loading_in_progress.erase(uri_key);
      reporter.record_error("Cannot load module file: " + file_path);
      return nullptr;
   }

   // Step 8: Create XML document and compile module
   auto module_doc = std::make_shared<extXML>();
   module_doc->Statement = content;

   // Initialize the document (triggers parsing)
   if (InitObject(module_doc.get()) != ERR::Okay) {
      loading_in_progress.erase(uri_key);
      reporter.record_error("Cannot parse module: " + file_path);
      return nullptr;
   }

   // Step 9: Compile the module query
   XPathNode *compiled = nullptr;
   if (xp::Compile(module_doc.get(), content.c_str(), (APTR*)&compiled) != ERR::Okay) {
      loading_in_progress.erase(uri_key);
      reporter.record_error("Cannot compile module: " + file_path);
      return nullptr;
   }

   // Step 10: Validate it's a library module
   auto module_prolog = compiled->get_prolog();
   if (!module_prolog or !module_prolog->is_library_module) {
      loading_in_progress.erase(uri_key);
      FreeResource(compiled);
      reporter.record_error("Module is not a library module: " + uri_key);
      return nullptr;
   }

   // Step 11: Validate namespace matches
   if (module_prolog->module_namespace_uri != uri_key) {
      loading_in_progress.erase(uri_key);
      FreeResource(compiled);
      reporter.record_error("Module namespace mismatch: expected " + uri_key);
      return nullptr;
   }

   // Step 12: Validate exports
   if (!module_prolog->validate_library_exports()) {
      loading_in_progress.erase(uri_key);
      FreeResource(compiled);
      reporter.record_error("Module exports not in target namespace: " + uri_key);
      return nullptr;
   }

   // Step 13: Cache the module
   ModuleInfo info;
   info.document = module_doc;
   info.compiled_query = compiled;
   info.prolog = module_prolog;

   modules[uri_key] = std::move(info);

   // Also cache in document cache if owner exists
   if (owner) {
      owner->DocumentCache[uri_key] = module_doc;
   }

   // Step 14: Done loading
   loading_in_progress.erase(uri_key);

   return module_doc;
}
```

#### 6. Module Prolog Access Helper

`find_module()` (above) exposes both the module prolog and compiled query so runtime lookups can reuse them without duplicating ownership rules.

### Phase C: Runtime Integration

#### 7. Module Function Resolution

Extend `XPathEvaluator::resolve_user_defined_function()` in `src/xpath/eval/eval_values.cpp`:

```cpp
std::optional<XPathVal> XPathEvaluator::resolve_user_defined_function(std::string_view function_name,
   const std::vector<XPathVal> &args, uint32_t current_prefix, const XPathNode *func_node)
{
   // Existing main-prolog lookup and arity diagnostics remain unchanged.

   if (!context.prolog or !context.module_cache) {
      // fall back to existing behaviour when we have no cache available
      return std::nullopt;
   }

   // Determine the namespace URI (expanded Q{uri}local form or prefix via resolve_prefix()).
   // Locate the matching XQueryModuleImport entry.
   // Call module_cache->fetch_or_load() to make sure the module has been parsed and compiled.
   // Retrieve the ModuleInfo via module_cache->find_module() and search its prolog for the function.
   // Evaluate with evaluate_user_defined_function() using the module's declarations.
}
```

#### 8. Module Variable Resolution

Mirror the logic inside `XPathEvaluator::resolve_variable_value()` (`src/xpath/eval/eval_expression.cpp`) so that once the main
prolog lookup fails it:

1. Derives the namespace URI from either a `Q{}` literal or in-scope prefix via `prolog->resolve_prefix()`.
2. Calls `module_cache->fetch_or_load()` to ensure the module is available and short-circuits on circular dependency errors.
3. Retrieves the `ModuleInfo` via `find_module()` and resolves the variable declaration, enforcing the same `is_external`
   and initializer rules as local prolog variables while caching evaluated results.

### Phase D: Testing

DONE: See `src/xpath/tests/test_module_loading.fluid`

### Phase E: Error Handling and Validation ✅ COMPLETE

#### 12. XQuery Error Codes Implementation

**Files Created:**
- `src/xpath/api/xpath_errors.h` - W3C error code constants and message formatters

**Error Codes Implemented:**

- **XQST0047**: Duplicate module imports for same target namespace
- **XQST0048**: Function/variable in library module not in target namespace
- **XQST0059**: Module cannot be located (no valid location hints)
- **XQDY0054**: Circular module dependency (dynamic error)

**Implementation Details:**

1. **Error Code Header (`xpath_errors.h`)**
   - Defined error code constants as `constexpr std::string_view`
   - Created message formatter functions for each error type:
     - `duplicate_module_import(namespace_uri)` - XQST0047
     - `export_not_in_namespace(component_type, qname, expected_namespace)` - XQST0048
     - `module_not_found(namespace_uri)` - XQST0059
     - `module_location_not_found(location)` - XQST0059
     - `circular_module_dependency(namespace_uri)` - XQDY0054
     - `not_library_module(namespace_uri)` - XQST0059
     - `namespace_mismatch(expected, actual)` - XQST0059

2. **Enhanced Export Validation (`xquery_prolog.h/.cpp`)**
   - Added `ExportValidationResult` structure with detailed error information
   - Implemented `validate_library_exports_detailed()` method
   - Returns specific error messages using XQST0048 error formatter
   - Identifies problematic function or variable QName

3. **Duplicate Import Detection (`xquery_prolog.h/.cpp`)**
   - Added `declare_module_import()` helper function
   - Checks for duplicate namespace imports (XQST0047)
   - Normalises URIs before comparison
   - Returns formatted error message

4. **Future Integration Documentation (`xquery_prolog.cpp`)**
   - Added comprehensive comments in `fetch_or_load()`
   - Documents which error codes to use at each validation step
   - Guides future implementation of full module loading

### Phase F: Documentation

#### 13. Update Implementation Plan Document

Update `docs/plans/XQUERY_PROLOG_PLAN.md` checklist:

```markdown
#### Phase 3: Evaluation Integration
...
- [x] Integrate module variable resolution via cache
- [x] Implement deferred module loading via `fetch_or_load()`

#### Phase 5: Module Loading (New Section)

**Library Module Parsing:**
- [ ] Implement module declaration parsing
- [ ] Add is_library_module flag and validation
- [ ] Implement export validation (XQST0048)

**Module Loading:**
- [ ] Implement URI resolution with location hints
- [ ] Implement file loading in fetch_or_load()
- [ ] Add circular dependency detection
- [ ] Implement module compilation and caching
- [ ] Add namespace validation

**Runtime Integration:**
- [ ] Extend function resolution to search modules
- [ ] Extend variable resolution to search modules
- [ ] Handle module context properly

**Testing:**
- [ ] Create library module test files
- [ ] Write comprehensive Flute tests
- [ ] Test error conditions
- [ ] Test module caching behavior
```

#### 14. Code Documentation

Add comprehensive documentation to new functions:

```cpp
/********************************************************************************************************************
** Resolves a module location from import declaration hints.
**
** This function iterates through the location hints provided in an import declaration and attempts to resolve
** each one against the static base URI. The first successfully resolved and accessible location is returned.
**
** Resolution follows XQuery semantics:
** - Absolute URIs are used as-is
** - Relative URIs are resolved against the static base URI
** - File existence is checked before returning
**
** @param target_namespace The target namespace URI of the module being imported
** @param location_hints Vector of location hint strings from the import declaration
** @param base_uri The static base URI for resolving relative paths
** @return The resolved absolute location, or nullopt if no valid location found
********************************************************************************************************************/
```

## Implementation Timeline

### Week 1: Parser and Structure
- Days 1-2: Implement module declaration parsing
- Days 3-4: Add validation and error handling
- Day 5: Test parser with unit tests

### Week 2: Loading Infrastructure
- Days 1-2: Implement URI resolution helpers
- Days 3-4: Complete fetch_or_load() implementation
- Day 5: Test loading in isolation

### Week 3: Runtime Integration
- Days 1-2: Module function resolution
- Days 3-4: Module variable resolution
- Day 5: Integration testing

### Week 4: Testing and Polish
- Days 1-2: Create test modules and Flute tests
- Days 3-4: Error handling and edge cases
- Day 5: Documentation and cleanup

## Dependencies and Risks

### Dependencies
- Existing prolog infrastructure (Phase 1-3) ✅ Complete
- File I/O system (LoadFile, UnloadFile) ✅ Available
- URI resolution utilities ✅ Available
- Error reporting framework ✅ Available

### Risks and Mitigations

**Risk 1: HTTP/HTTPS module loading**
- Mitigation: Phase 1 supports only file:// and local paths
- Future: Add HTTP support with optional libcurl integration

**Risk 2: Module compilation overhead**
- Mitigation: Cache aggressively at document level
- Optimization: Consider pre-compiling standard library modules

**Risk 3: Complex circular dependency scenarios**
- Mitigation: Simple loading_in_progress set handles direct cycles
- Enhancement: Consider stack trace for better error messages

**Risk 4: Memory management with multiple cached modules**
- Mitigation: Use shared_ptr with proper ownership
- Monitoring: Add cache size limits if needed

## Future Enhancements

### Out of Scope for Initial Implementation
- Schema imports (`import schema`)
- HTTP/HTTPS module URIs
- Module versioning
- Module signatures and security
- Pre-compiled module format
- Module search paths / catalog files

### Possible Future Work
- Module repository system
- Standard library module packaging
- Module dependency visualization
- Performance profiling for module loading
- Module hot-reloading for development

## References

- XQuery 1.0 Specification: https://www.w3.org/TR/xquery/
- XQuery Modules: https://www.w3.org/TR/xquery/#id-modules
- Existing implementation plan: `docs/plans/XQUERY_PROLOG_PLAN.md`
- URI utilities: `src/xml/uri_utils.h`
- Document cache: `src/xml/xml.h` (extXML::DocumentCache)
