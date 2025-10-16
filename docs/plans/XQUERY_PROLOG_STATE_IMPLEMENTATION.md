# XQuery 1.0 Prolog State Implementation Plan

## Overview

This document outlines the implementation plan for adding XQuery 1.0 prolog support to the Parasol XML module through a `PrologState` structure embedded within the `extXML` class. The approach maintains backward compatibility with pure XPath expressions whilst providing comprehensive prolog declaration support for full XQuery functionality.

## Design Principles

1. **Backward Compatibility**: Existing XPath code continues to work unchanged
2. **Optional Activation**: Prolog state is only created when XQuery features are detected
3. **Leveraging Existing Infrastructure**: Reuses extXML's namespace and variable systems where appropriate
4. **Clear Separation**: Compilation-time state (prolog) distinct from runtime state (XPathContext)
5. **Standard Compliance**: Full support for XQuery 1.0 prolog declarations per W3C specification

## Architecture Context

### Current System Components

The existing XPath/XQuery infrastructure consists of:

- **`extXML`** (src/xml/xml.h) - Document-level state with namespace registries, variables, caches
- **`XPathContext`** (src/xpath/xpath_functions.h) - Runtime evaluation context
- **`XPathEvaluator`** (src/xpath/xpath_evaluator.h) - Expression evaluation engine
- **`XPathFunctionLibrary`** (src/xpath/xpath_functions.h) - Singleton for built-in functions

### Why PrologState in extXML

Prolog declarations are **compilation-time metadata** that affect query semantics:
- Namespace bindings influence QName resolution during parsing
- Variable declarations define the static context
- Function declarations extend the function library
- Settings like boundary-space affect constructor behaviour

Placing `PrologState` in `extXML` provides:
- Natural lifetime management (created with document, destroyed with document)
- Access to existing namespace infrastructure
- Separation from per-evaluation runtime context
- Clear ownership model for user-defined functions

## PrologState Structure Definition

### Core Structure in xml.h

```cpp
//********************************************************************************************************************
// XQuery Prolog State - Compilation-time declarations

struct UserDefinedFunction {
   std::string name;                                // Qualified function name (may include prefix)
   std::vector<std::string> parameter_names;        // Parameter names in order
   std::vector<std::string> parameter_types;        // Optional type annotations (empty string = untyped)
   std::string return_type;                         // Optional return type (empty = untyped)
   std::unique_ptr<XPathNode> body;                 // AST of function body expression
   bool is_external = false;                        // Reserved for module imports

   // Function signature for lookup: "localname/arity" or "prefix:localname/arity"
   [[nodiscard]] std::string signature() const {
      return name + "/" + std::to_string(parameter_names.size());
   }
};

struct DecimalFormat {
   std::string name;                     // Format name (empty for default)
   char decimal_separator = '.';
   char grouping_separator = ',';
   std::string infinity = "Infinity";
   char minus_sign = '-';
   std::string NaN = "NaN";
   char percent = '%';
   char per_mille = '\u2030';            // ‰ character
   char zero_digit = '0';
   char digit = '#';
   char pattern_separator = ';';
};

struct ModuleImport {
   std::string namespace_uri;            // Target namespace URI
   std::string location_hints;           // Optional location URI
   bool is_schema_import = false;        // true for schema import, false for module import

   // Deferred loading support
   bool loaded = false;
   std::string error_message;            // If loading failed
};

struct PrologState {
   // Namespace declarations (declare namespace prefix = "uri")
   ankerl::unordered_dense::map<std::string, uint32_t> declared_namespaces;

   // Default namespace declarations
   std::optional<uint32_t> default_element_namespace;     // declare default element namespace
   std::optional<uint32_t> default_function_namespace;    // declare default function namespace

   // Variable declarations (declare variable $name := value or external)
   ankerl::unordered_dense::map<std::string, XPathVal> declared_variables;
   std::unordered_set<std::string> external_variables;    // Variables marked as external

   // User-defined functions (declare function name($params) { body })
   ankerl::unordered_dense::map<std::string, UserDefinedFunction> user_functions;

   // Function scope stack for temporary/local function definitions
   std::vector<std::string> function_scope_stack;

   // Module imports (import module namespace "uri" at "location")
   std::vector<ModuleImport> imported_modules;

   // Prolog settings with W3C defaults
   enum class BoundarySpacePolicy { PRESERVE, STRIP } boundary_space = BoundarySpacePolicy::STRIP;

   std::string default_collation = "http://www.w3.org/2005/xpath-functions/collation/codepoint";

   std::string base_uri;                              // Static base URI for the query

   enum class ConstructionMode { PRESERVE, STRIP } construction_mode = ConstructionMode::STRIP;

   enum class OrderingMode { ORDERED, UNORDERED } ordering_mode = OrderingMode::ORDERED;

   enum class EmptyOrderSpec { GREATEST, LEAST } default_empty_order = EmptyOrderSpec::GREATEST;

   struct CopyNamespacesPolicy {
      bool preserve = true;                           // preserve vs no-preserve
      bool inherit = true;                            // inherit vs no-inherit
   } copy_namespaces;

   // Decimal formats for format-number() function
   DecimalFormat default_decimal_format;
   ankerl::unordered_dense::map<std::string, DecimalFormat> named_decimal_formats;

   // Option declarations (declare option name "value") - implementation-defined
   ankerl::unordered_dense::map<std::string, std::string> options;

   // Helper methods
   void pushFunction(std::string Name, UserDefinedFunction Func);
   void popFunction(const std::string &Name);
   [[nodiscard]] const UserDefinedFunction* findFunction(std::string_view Name, size_t Arity) const;

   [[nodiscard]] uint32_t resolveNamespacePrefix(const std::string &Prefix, extXML *XML) const;
   [[nodiscard]] std::string resolveFunctionName(const std::string &Name) const;

   void declareNamespace(const std::string &Prefix, const std::string &URI, extXML *XML);
   void declareVariable(const std::string &Name, XPathVal Value);
   void declareFunction(UserDefinedFunction Function);

   PrologState() = default;
};
```

## extXML Integration

### Modified extXML Structure (xml.h)

```cpp
class extXML : public objXML {
   public:
   // Existing fields remain unchanged...
   ankerl::unordered_dense::map<int, XMLTag *> Map;
   ankerl::unordered_dense::map<int, std::string> BaseURIMap;
   std::string ErrorMsg;
   std::string Statement;
   std::string Attrib;
   bool ReadOnly;
   bool StaleMap;

   TAGS *CursorParent;
   TAGS *CursorTags;
   CURSOR Cursor;
   FUNCTION Callback;

   std::shared_ptr<xml::schema::SchemaContext> SchemaContext;
   ankerl::unordered_dense::map<std::string, std::string> Variables;
   ankerl::unordered_dense::map<std::string, std::string> Entities;
   ankerl::unordered_dense::map<std::string, std::string> ParameterEntities;
   ankerl::unordered_dense::map<std::string, std::string> Notations;

   ankerl::unordered_dense::map<uint32_t, std::string> NSRegistry;
   ankerl::unordered_dense::map<std::string, uint32_t> Prefixes;

   ankerl::unordered_dense::map<std::string, std::shared_ptr<extXML>> DocumentCache;
   ankerl::unordered_dense::map<std::string, std::shared_ptr<std::string>> UnparsedTextCache;
   ankerl::unordered_dense::map<const XMLTag *, std::weak_ptr<extXML>> DocumentNodeOwners;

   // NEW: Optional XQuery prolog state
   std::optional<PrologState> prolog_state;

   extXML() : ReadOnly(false), StaleMap(true) { }

   // NEW: Prolog state management
   [[nodiscard]] inline bool hasProlog() const { return prolog_state.has_value(); }

   inline PrologState& ensureProlog() {
      if (not prolog_state.has_value()) {
         prolog_state.emplace();
         // Inherit base URI from document if not explicitly set
         if (prolog_state->base_uri.empty() and not CurrentBase.empty()) {
            prolog_state->base_uri = CurrentBase;
         }
      }
      return *prolog_state;
   }

   inline void clearProlog() { prolog_state.reset(); }

   // Existing methods remain unchanged...
   [[nodiscard]] ankerl::unordered_dense::map<int, XMLTag *> & getMap();
   [[nodiscard]] inline XMLTag * getTag(int ID) noexcept;
   // ... etc
};
```

## XPathContext Integration

### Updated XPathContext Structure (xpath_functions.h)

```cpp
struct XPathContext
{
   XMLTag * context_node = nullptr;
   const XMLAttrib * attribute_node = nullptr;
   size_t position = 1;
   size_t size = 1;
   ankerl::unordered_dense::map<std::string, XPathVal> * variables = nullptr;
   extXML * document = nullptr;
   bool * expression_unsupported = nullptr;
   xml::schema::SchemaTypeRegistry * schema_registry = nullptr;

   // NEW: Link to prolog state for UDF lookups
   PrologState * prolog_state = nullptr;

   XPathContext() = default;

   XPathContext(XMLTag *Node, size_t cursor = 1, size_t Sz = 1, const XMLAttrib *Attribute = nullptr,
                extXML *Document = nullptr, bool *UnsupportedFlag = nullptr,
                xml::schema::SchemaTypeRegistry *Registry = nullptr,
                ankerl::unordered_dense::map<std::string, XPathVal> *Variables = nullptr,
                PrologState *Prolog = nullptr)
      : context_node(Node), attribute_node(Attribute), position(cursor), size(Sz), variables(Variables),
        document(Document), expression_unsupported(UnsupportedFlag), schema_registry(Registry),
        prolog_state(Prolog) {}
};
```

## XPathEvaluator Integration

### Updated XPathEvaluator Constructor

```cpp
// xpath_evaluator.cpp

XPathEvaluator::XPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML)
{
   // Existing initialization...
   context.document = XML;
   context.variables = &variable_storage;
   context.expression_unsupported = &expression_unsupported;

   if (XML and XML->SchemaContext) {
      context.schema_registry = &xml::schema::registry();
   }

   // NEW: Link prolog state if present
   if (XML and XML->hasProlog()) {
      context.prolog_state = &XML->prolog_state.value();
   }
}
```

### Function Call Resolution with UDFs

```cpp
// xpath_evaluator.cpp

XPathVal XPathEvaluator::evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix)
{
   const auto &func_name = FuncNode->function_call.name;
   const auto &arguments = FuncNode->function_call.arguments;

   // First: Try user-defined functions if prolog state exists
   if (context.prolog_state) {
      auto udf = context.prolog_state->findFunction(func_name, arguments.size());
      if (udf) {
         return evaluate_user_defined_function(*udf, arguments, CurrentPrefix);
      }
   }

   // Second: Fall back to built-in function library
   auto &library = XPathFunctionLibrary::instance();
   if (library.has_function(func_name)) {
      // Evaluate arguments
      std::vector<XPathVal> arg_values;
      arg_values.reserve(arguments.size());
      for (const auto* arg_node : arguments) {
         arg_values.push_back(evaluate_expression(arg_node, CurrentPrefix));
      }

      return library.call_function(func_name, arg_values, context);
   }

   // Function not found
   record_error("Unknown function: " + std::string(func_name), FuncNode);
   return XPathVal::empty_sequence();
}

XPathVal XPathEvaluator::evaluate_user_defined_function(
   const UserDefinedFunction &UDF,
   const std::vector<const XPathNode *> &Arguments,
   uint32_t CurrentPrefix)
{
   if (Arguments.size() != UDF.parameter_names.size()) {
      record_error("Function " + UDF.name + " expects " +
                   std::to_string(UDF.parameter_names.size()) + " arguments, got " +
                   std::to_string(Arguments.size()));
      return XPathVal::empty_sequence();
   }

   // Create local variable bindings for parameters
   std::vector<std::unique_ptr<VariableBindingGuard>> bindings;
   bindings.reserve(UDF.parameter_names.size());

   for (size_t i = 0; i < Arguments.size(); ++i) {
      XPathVal arg_value = evaluate_expression(Arguments[i], CurrentPrefix);

      // TODO: Type checking if parameter_types[i] is specified

      bindings.push_back(std::make_unique<VariableBindingGuard>(
         context, UDF.parameter_names[i], std::move(arg_value)
      ));
   }

   // Evaluate function body
   XPathVal result = evaluate_expression(UDF.body.get(), CurrentPrefix);

   // TODO: Type checking if return_type is specified

   return result;
   // Bindings automatically restored by VariableBindingGuard destructors
}
```

## Prolog Parsing Integration

### Parser Modification (xpath_parser.h/cpp)

```cpp
// New parser method for prolog processing

class XPathParser {
   // ... existing members ...

   public:
   // NEW: Parse XQuery prolog and populate PrologState
   ERR parseProlog(extXML *XML);

   private:
   ERR parsePrologDeclaration(PrologState &Prolog, extXML *XML);
   ERR parseNamespaceDecl(PrologState &Prolog, extXML *XML);
   ERR parseDefaultNamespaceDecl(PrologState &Prolog, extXML *XML);
   ERR parseVariableDecl(PrologState &Prolog, extXML *XML);
   ERR parseFunctionDecl(PrologState &Prolog, extXML *XML);
   ERR parseBoundarySpaceDecl(PrologState &Prolog);
   ERR parseBaseURIDecl(PrologState &Prolog);
   ERR parseConstructionDecl(PrologState &Prolog);
   ERR parseOrderingDecl(PrologState &Prolog);
   ERR parseEmptyOrderDecl(PrologState &Prolog);
   ERR parseCopyNamespacesDecl(PrologState &Prolog);
   ERR parseDecimalFormatDecl(PrologState &Prolog);
   ERR parseOptionDecl(PrologState &Prolog);
   ERR parseImportDecl(PrologState &Prolog, extXML *XML);
};
```

### Usage in xp::Compile

```cpp
// xml_functions.cpp - Updated xp::Compile method

ERR xp::Compile(extXML *Self, std::shared_ptr<CompiledXPath> &Result)
{
   pf::Log log(__FUNCTION__);

   // Tokenize the XPath/XQuery expression
   XPathTokenizer tokenizer(Self->Statement);
   auto tokens = tokenizer.tokenize();
   if (tokens.empty()) return log.warning(ERR::Failed, "Failed to tokenize XPath expression");

   // Create parser
   XPathParser parser(tokens, Self->Statement);

   // NEW: Check if expression starts with prolog declarations
   if (parser.startsWithProlog()) {
      // Ensure prolog state exists
      auto &prolog = Self->ensureProlog();

      // Parse prolog declarations
      if (auto error = parser.parseProlog(Self); error != ERR::Okay) {
         Self->ErrorMsg = "Prolog parsing failed: " + parser.getErrorMessage();
         return log.warning(error, "Failed to parse XQuery prolog");
      }
   }

   // Parse main query expression
   auto ast_root = parser.parse();
   if (not ast_root) {
      Self->ErrorMsg = parser.getErrorMessage();
      return log.warning(ERR::Failed, "Failed to parse XPath expression");
   }

   // Create compiled result
   Result = std::make_shared<CompiledXPath>();
   Result->root = std::move(ast_root);
   Result->original_expression = Self->Statement;

   return ERR::Okay;
}
```

## PrologState Helper Method Implementations

### Function Management (src/xml/xml.cpp or xml_prolog.cpp)

```cpp
void PrologState::pushFunction(std::string Name, UserDefinedFunction Func)
{
   auto signature = Func.signature();

   // Store function
   user_functions[signature] = std::move(Func);

   // Push to scope stack for later removal
   function_scope_stack.push_back(signature);
}

void PrologState::popFunction(const std::string &Name)
{
   if (function_scope_stack.empty()) return;

   auto signature = function_scope_stack.back();
   function_scope_stack.pop_back();

   user_functions.erase(signature);
}

const UserDefinedFunction* PrologState::findFunction(std::string_view Name, size_t Arity) const
{
   // Build lookup signature
   std::string signature = std::string(Name) + "/" + std::to_string(Arity);

   auto it = user_functions.find(signature);
   if (it != user_functions.end()) {
      return &it->second;
   }

   return nullptr;
}
```

### Namespace Resolution

```cpp
uint32_t PrologState::resolveNamespacePrefix(const std::string &Prefix, extXML *XML) const
{
   // First check prolog-declared namespaces
   auto it = declared_namespaces.find(Prefix);
   if (it != declared_namespaces.end()) {
      return it->second;
   }

   // Fall back to document-level namespace registry
   if (XML) {
      auto doc_it = XML->Prefixes.find(Prefix);
      if (doc_it != XML->Prefixes.end()) {
         return doc_it->second;
      }
   }

   return 0; // No namespace
}

std::string PrologState::resolveFunctionName(const std::string &Name) const
{
   // If name contains colon, it's already qualified
   if (Name.find(':') != std::string::npos) return Name;

   // If default function namespace is set, prefix with it
   if (default_function_namespace.has_value()) {
      // Would need to reverse-lookup namespace URI to prefix
      // For now, return as-is (this is a simplification)
      return Name;
   }

   return Name;
}
```

### Declaration Methods

```cpp
void PrologState::declareNamespace(const std::string &Prefix, const std::string &URI, extXML *XML)
{
   uint32_t uri_hash = pf::strhash(URI);

   // Register in prolog state
   declared_namespaces[Prefix] = uri_hash;

   // Also register in document namespace registry for consistency
   if (XML) {
      XML->registerNamespace(URI);
      XML->Prefixes[Prefix] = uri_hash;
   }
}

void PrologState::declareVariable(const std::string &Name, XPathVal Value)
{
   declared_variables[Name] = std::move(Value);
}

void PrologState::declareFunction(UserDefinedFunction Function)
{
   auto signature = Function.signature();
   user_functions[signature] = std::move(Function);
}
```

## Usage Examples

### Example 1: Basic User-Defined Function

```xquery
declare function local:square($x as xs:integer) as xs:integer {
   $x * $x
};

local:square(5)
```

**Prolog Processing:**
```cpp
// Parser creates UserDefinedFunction:
UserDefinedFunction square_func;
square_func.name = "local:square";
square_func.parameter_names = {"x"};
square_func.parameter_types = {"xs:integer"};
square_func.return_type = "xs:integer";
square_func.body = /* AST of "$x * $x" */;

// Stored in prolog state:
xml->ensureProlog().declareFunction(std::move(square_func));
```

**Evaluation:**
```cpp
// When evaluating "local:square(5)":
// 1. XPathEvaluator::evaluate_function_call() checks context.prolog_state
// 2. Finds UDF with signature "local:square/1"
// 3. Binds parameter $x = 5
// 4. Evaluates body expression $x * $x
// 5. Returns 25
```

### Example 2: Namespace and Variable Declarations

```xquery
declare namespace ex = "http://example.com/ns";
declare variable $threshold := 100;

//ex:item[@value > $threshold]
```

**Prolog Processing:**
```cpp
auto &prolog = xml->ensureProlog();

// Namespace declaration
prolog.declareNamespace("ex", "http://example.com/ns", xml);

// Variable declaration
XPathVal threshold_value(100.0);
prolog.declareVariable("threshold", threshold_value);
```

**Evaluation:**
```cpp
// During XPath evaluation:
// 1. Prefix "ex" resolved via prolog.resolveNamespacePrefix()
// 2. Variable $threshold looked up in prolog.declared_variables
// 3. Comparison uses value 100
```

### Example 3: Boundary-Space Declaration

```xquery
declare boundary-space preserve;

<result>{
   <item>text</item>
}</result>
```

**Prolog Processing:**
```cpp
auto &prolog = xml->ensureProlog();
prolog.boundary_space = PrologState::BoundarySpacePolicy::PRESERVE;
```

**Effect:**
Whitespace in constructed elements is preserved rather than stripped during normalisation.

### Example 4: Recursive Function

```xquery
declare function local:factorial($n as xs:integer) as xs:integer {
   if ($n <= 1) then 1
   else $n * local:factorial($n - 1)
};

local:factorial(5)
```

**Evaluation Process:**
```cpp
// Call stack:
// local:factorial(5)
//   -> 5 * local:factorial(4)
//       -> 4 * local:factorial(3)
//           -> 3 * local:factorial(2)
//               -> 2 * local:factorial(1)
//                   -> returns 1
//               -> returns 2
//           -> returns 6
//       -> returns 24
//   -> returns 120
```

Each recursive call creates new parameter bindings using `VariableBindingGuard`, ensuring proper scoping.

## Migration Path and Backward Compatibility

### Phase 1: Core Infrastructure (Current Proposal)
- Add `PrologState` structure to xml.h
- Modify `extXML` to include `std::optional<PrologState>`
- Update `XPathContext` to link prolog state
- Extend `XPathEvaluator` constructor

**Impact:** Zero - changes are additive, `std::optional` defaults to empty

### Phase 2: Basic Prolog Parsing
- Implement namespace and variable declarations
- Add boundary-space and base-uri declarations
- Basic function declaration parsing (no recursion yet)

**Impact:** Minimal - only affects expressions starting with "declare"

### Phase 3: User-Defined Functions
- Complete UDF evaluation with parameter binding
- Recursive function support
- Type annotation parsing (validation optional initially)

**Impact:** Low - only affects queries using UDFs

### Phase 4: Advanced Features
- Module imports (with timeout protection)
- Decimal formats
- Construction mode and copy-namespaces policies

**Impact:** Minimal - advanced features used rarely

### Pure XPath Expressions

```cpp
// Expression: "//book[@price < 20]"
// No "declare" keyword detected
// xml->hasProlog() returns false
// Evaluation proceeds as before with zero overhead
```

### Mixed XPath in Documents

```cpp
// XML document may have inline XPath in attributes
// These are pure XPath expressions, not XQuery
// No prolog state created
// Existing behaviour preserved
```

## Implementation Checklist

### Structural Changes
- [ ] Add `UserDefinedFunction` struct to xml.h
- [ ] Add `DecimalFormat` struct to xml.h
- [ ] Add `ModuleImport` struct to xml.h
- [ ] Add `PrologState` struct to xml.h
- [ ] Add `std::optional<PrologState> prolog_state` to `extXML`
- [ ] Add `PrologState* prolog_state` to `XPathContext`
- [ ] Implement `extXML::hasProlog()`, `ensureProlog()`, `clearProlog()`

### Parser Extensions
- [ ] Add prolog detection logic to `XPathParser`
- [ ] Implement `parseProlog()` method
- [ ] Implement individual declaration parsers (namespace, variable, function, etc.)
- [ ] Add `startsWithProlog()` check to xp::Compile

### Evaluator Extensions
- [ ] Update `XPathEvaluator` constructor to link prolog state
- [ ] Modify `evaluate_function_call()` for UDF lookup
- [ ] Implement `evaluate_user_defined_function()`
- [ ] Ensure `VariableBindingGuard` works with UDF parameters

### Helper Methods
- [ ] Implement `PrologState::pushFunction()`
- [ ] Implement `PrologState::popFunction()`
- [ ] Implement `PrologState::findFunction()`
- [ ] Implement `PrologState::resolveNamespacePrefix()`
- [ ] Implement `PrologState::resolveFunctionName()`
- [ ] Implement `PrologState::declareNamespace()`
- [ ] Implement `PrologState::declareVariable()`
- [ ] Implement `PrologState::declareFunction()`

### Testing
- [ ] Unit tests for prolog parsing
- [ ] Tests for namespace declarations
- [ ] Tests for variable declarations
- [ ] Tests for user-defined functions (including recursion)
- [ ] Tests for boundary-space and other settings
- [ ] Backward compatibility tests (pure XPath expressions)
- [ ] Integration tests with real XQuery expressions

### Documentation
- [ ] Update XML module API documentation
- [ ] Add XQuery prolog examples
- [ ] Document UDF syntax and limitations
- [ ] Update Fluid XPath API documentation

## Implementation Benefits

### Alignment with XQuery 1.0 Standard
- Full W3C XQuery 1.0 prolog support
- Standard-compliant namespace handling
- Proper static context management

### Leveraging Existing Infrastructure
- Reuses `extXML` namespace registry
- Integrates with existing variable system
- Extends function library architecture cleanly

### Minimal Overhead
- `std::optional` ensures zero cost when not used
- Prolog state only created for XQuery expressions
- Pure XPath expressions unaffected

### Clear Separation of Concerns
- Compilation-time state (prolog) in `extXML`
- Runtime state (context) in `XPathContext`
- Evaluation logic in `XPathEvaluator`

### Extensibility
- Easy to add new prolog declarations
- Module import system extensible
- Future XQuery 3.0 features can build on this foundation

### Developer Experience
- Intuitive structure mirrors W3C specification
- Helper methods provide clean API
- Clear documentation and examples

## Related Issues

This implementation plan addresses requirements from:
- [#560](https://github.com/parasol-framework/parasol/issues/560) - Complete XQuery Support

## Future Considerations

### Module System Enhancement
Once basic prolog support is complete, the module import system can be enhanced with:
- Asynchronous module loading with timeout protection
- Module caching to avoid redundant parsing
- Module version management

### Type System Integration
The type annotations in UDFs can be integrated with:
- Schema-aware validation
- Static type checking during compilation
- Runtime type coercion

### XQuery 3.0 Features
This PrologState foundation enables future:
- Higher-order functions
- Try-catch expressions
- Group by with aggregation
- Window clauses

This implementation plan provides a robust, standards-compliant foundation for XQuery 1.0 prolog support while maintaining full backward compatibility with existing XPath functionality.
