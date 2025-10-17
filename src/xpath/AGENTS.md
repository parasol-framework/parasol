# XPath Module - AI Agent Guide

This file provides comprehensive information about the Parasol XPath module for AI agents working with the codebase.

## Overview

The XPath module provides comprehensive XPath 2.0 and XQuery language support for querying and navigating XML documents. It operates in conjunction with the XML module to deliver a standards-compliant query engine with extensive functionality.

**Key Features:**
- Full XPath 2.0 specification compliance
- XQuery language support with FLWOR expressions
- Comprehensive function library (100+ functions)
- Expression compilation and optimisation
- Thread-safe compiled expressions
- Schema-aware type system integration
- Namespace-aware query processing
- Variable binding and parameterised queries
- Node constructor expressions

## Module Architecture

### Core Files Structure

```
src/xpath/
├── xpath.fdl                  # Interface definition (functions, structs, enums)
├── xpath.cpp                  # Module initialisation and core functions
├── xpath.h                    # Module header
├── xpath_def.c                # Generated C definitions
├── xpath_parser.cpp/h         # XPath/XQuery expression parser
├── xpath_tokeniser.cpp/h      # Lexical analysis and tokenisation
├── eval.cpp/h                 # Main evaluation engine
├── eval_common.cpp            # Common evaluation utilities
├── eval_context.cpp           # Context management
├── eval_navigation.cpp        # Node navigation
├── eval_predicates.cpp        # Predicate evaluation
├── eval_values.cpp            # Value operations
├── eval_expression.cpp        # Expression evaluation
├── eval_flwor.cpp             # FLWOR expression support
├── xpath_axis.cpp/h           # XPath axis evaluation
├── xpath_functions.cpp/h      # Function registry and dispatch
├── xpath_arena.h              # Memory management for evaluation
├── xpath_ast.h                # Abstract Syntax Tree definitions
└── functions/                 # Function implementations by category
    ├── func_accessors.cpp         # Accessor functions (base-uri, data, etc.)
    ├── func_booleans.cpp          # Boolean functions (not, true, false, etc.)
    ├── func_datetimes.cpp         # Date/time functions (current-date, etc.)
    ├── func_diagnostics.cpp       # Diagnostic functions (error, trace)
    ├── func_documents.cpp         # Document functions (doc, collection, etc.)
    ├── func_nodeset.cpp           # Node-set functions (name, local-name, etc.)
    ├── func_numbers.cpp           # Numeric functions (sum, round, etc.)
    ├── func_qnames.cpp            # QName functions
    ├── func_sequences.cpp         # Sequence functions (distinct-values, etc.)
    ├── func_strings.cpp           # String functions (concat, substring, etc.)
    └── accessor_support.cpp/h     # Accessor function utilities
```

### Dependencies

- **unicode** module (PUBLIC) - For text encoding/decoding
- **xml** module - For XML document structures and schema integration
- **regex** module (dynamically loaded) - For pattern matching in XPath functions

## Core Structures and Types

### XPathNode Structure

Represents a compiled XPath or XQuery expression as an Abstract Syntax Tree (AST).

**Include:** `<parasol/modules/xpath.h>`

The XPathNode structure is an opaque type representing compiled XPath expressions. It is created via `xp::Compile()` and used with `xp::Evaluate()` and `xp::Query()`.

**Key Properties:**
- Thread-safe and reusable across multiple XML documents
- Managed as a resource (freed with `FreeResource()`)
- Contains optimised internal representation
- Supports complex XPath 2.0 and XQuery expressions

### XPathValue Structure

Represents the result of XPath expression evaluation, supporting multiple value types.

**Include:** `<parasol/modules/xml.h>` (shared type)

```cpp
struct XPathValue {
   XPVT Type;              // Type identifier (NodeSet, Boolean, Number, String, Date, Time, DateTime)
   double NumberValue;     // For Number and Boolean types
   std::string StringValue; // For String type
   pf::vector<XMLTag *> node_set; // For NodeSet type
}
```

**Type Enumeration (XPVT):**
- `NodeSet` - Collection of XML nodes
- `Boolean` - Boolean value (stored as number)
- `Number` - Numeric value
- `String` - String value
- `Date` - Date value
- `Time` - Time value
- `DateTime` - Date and time value

### XPathNodeType Enumeration

Describes the type of nodes in the XPath Abstract Syntax Tree.

**Include:** `<parasol/modules/xpath.h>`

Key node types include:
- `LOCATION_PATH`, `STEP`, `NODE_TEST`, `PREDICATE`, `ROOT`
- `BINARY_OP`, `UNARY_OP`, `CONDITIONAL`
- `FLWOR_EXPRESSION`, `WHERE_CLAUSE`, `ORDER_CLAUSE`
- `FUNCTION_CALL`, `LITERAL`, `VARIABLE_REFERENCE`
- Constructor types: `DIRECT_ELEMENT_CONSTRUCTOR`, `COMPUTED_ELEMENT_CONSTRUCTOR`, etc.

## Core Functionality

### Expression Compilation

XPath and XQuery expressions must be compiled before evaluation. Compilation validates syntax, builds an optimised AST, and prepares the expression for execution.

**Key Function:**
- `xp::Compile(objXML *XML, CSTRING Query, XPathNode **Result)`

**Compilation Process:**
1. Tokenisation - Break expression into tokens
2. Parsing - Build Abstract Syntax Tree
3. Validation - Check syntax and structure
4. Optimisation - Prepare for efficient evaluation

**Error Handling:**
- Detailed error messages stored in XML object's `ErrorMsg` field
- Returns `ERR::Syntax` on compilation failure
- Validates XPath 2.0 and XQuery syntax

### Expression Evaluation

Compiled expressions can be evaluated against XML documents to produce typed results.

**Key Function:**
- `xp::Evaluate(objXML *XML, XPathNode *Query, XPathValue **Result)`

**Evaluation Modes:**
- **Value Mode**: Returns complete typed result (XPathValue)
- **Node Iteration Mode**: Uses `xp::Query()` with callbacks for streaming

**Return Types:**
- Node sets (collections of XMLTag pointers)
- Strings (concatenated text content)
- Numbers (double precision)
- Booleans (true/false)
- Date/Time values

### Node Iteration with Callbacks

For processing large result sets or when only node matching is needed, use the Query function with callbacks.

**Key Function:**
- `xp::Query(objXML *XML, XPathNode *Query, FUNCTION *Callback)`

**Callback Pattern:**
```cpp
// C++ callback for each matching node
ERR callback(objXML *XML, int TagID, CSTRING Attrib) {
   // Process node...
   return ERR::Okay;
}
```

**Behavior:**
- Invokes callback for each matching node
- Returns `ERR::Search` if no matches found
- Returns `ERR::Okay` if at least one match processed
- Can be called with NULL callback to find first match only

## XPath 2.0 Language Support

### Path Expressions

Full support for XPath 2.0 path expressions including:

**Axes (all 13 standard axes):**
- `child::` - Child elements (default axis)
- `descendant::` - All descendants
- `descendant-or-self::` - Self and all descendants
- `parent::` - Parent element
- `ancestor::` - All ancestors
- `ancestor-or-self::` - Self and all ancestors
- `following::` - All following nodes
- `following-sibling::` - Following siblings only
- `preceding::` - All preceding nodes
- `preceding-sibling::` - Preceding siblings only
- `self::` - Current node
- `attribute::` - Attributes (short form: `@`)
- `namespace::` - Namespace nodes

**Node Tests:**
- Element names: `/root/element`
- Wildcards: `/root/*`
- Attribute selectors: `@attr`, `@*`
- Node type tests: `node()`, `text()`, `comment()`

**Path Types:**
- Absolute paths: `/root/element`
- Relative paths: `element/subelement`
- Recursive descent: `//element`

### Predicates

Complex predicate expressions with full boolean logic:

**Position Predicates:**
- `[1]` - First element
- `[last()]` - Last element
- `[position() < 5]` - First 4 elements

**Comparison Predicates:**
- `[@price < 10]` - Numeric comparison
- `[@category='fiction']` - String equality
- `[@*='value']` - Any attribute matching (Parasol extension)

**Boolean Predicates:**
- `[not(@deprecated)]` - Negation
- `[@a and @b]` - Logical AND
- `[@a or @b]` - Logical OR

**Content Matching:**
- `[=pattern]` - Match on text content (Parasol extension)

## XQuery Language Support

### FLWOR Expressions

Full support for XQuery FLWOR (For, Let, Where, Order by, Return) expressions:

**For Clause:**
```xpath
for $book in //book return $book/title
```

**Let Clause:**
```xpath
let $total := sum(//book/@price) return $total
```

**Where Clause:**
```xpath
for $book in //book where $book/@price < 20 return $book/title
```

**Order By Clause:**
```xpath
for $book in //book order by $book/@price return $book
```

**Return Clause:**
```xpath
for $x in 1 to 10 return $x * $x
```

**Combined Example:**
```xpath
for $book in //book
let $discount := $book/@price * 0.1
where $book/@category = 'fiction'
order by $book/@price descending
return $book/title
```

### Additional XQuery Features

**Group By Clause:**
```xpath
for $book in //book
group by $category := $book/@category
return $category
```

**Count Clause:**
```xpath
for $book in //book
count $index
return concat($index, ': ', $book/title)
```

**Quantified Expressions:**
```xpath
some $book in //book satisfies $book/@price < 10
every $book in //book satisfies $book/@isbn
```

### Node Constructors

XQuery supports both direct and computed constructors for creating new XML nodes:

**Direct Element Constructor:**
```xpath
<result>{//book/title}</result>
```

**Computed Element Constructor:**
```xpath
element book { attribute isbn {'123'}, 'Title' }
```

**Attribute Constructor:**
```xpath
attribute price { $book/@price * 0.9 }
```

**Text, Comment, and PI Constructors:**
```xpath
text { 'Hello' }
comment { 'This is a comment' }
processing-instruction target { 'data' }
```

## Function Library

The XPath module provides an extensive function library organised by category.

### Node Functions

**Include:** `xpath_functions.h` (internal)

- `position()` - Current node position in context
- `last()` - Size of context node set
- `count(node-set)` - Count nodes in set
- `id(string)` - Find element by ID attribute
- `idref(string)` - Find elements by IDREF
- `name([node])` - Qualified name of node
- `local-name([node])` - Local part of node name
- `namespace-uri([node])` - Namespace URI of node
- `root([node])` - Root node of document
- `node-name([node])` - QName of node
- `base-uri([node])` - Base URI of node

### String Functions

**Include:** `functions/func_strings.cpp`

- `string(value)` - Convert to string
- `concat(string, ...)` - Concatenate strings
- `substring(string, start, [length])` - Extract substring
- `substring-before(string, delimiter)` - Text before delimiter
- `substring-after(string, delimiter)` - Text after delimiter
- `string-length([string])` - Length of string
- `normalize-space([string])` - Normalise whitespace
- `normalize-unicode(string, [form])` - Unicode normalisation
- `translate(string, from, to)` - Character translation
- `upper-case(string)` - Convert to uppercase
- `lower-case(string)` - Convert to lowercase
- `contains(string, substring)` - Substring test
- `starts-with(string, prefix)` - Prefix test
- `ends-with(string, suffix)` - Suffix test
- `string-join(sequence, separator)` - Join strings
- `encode-for-uri(string)` - URL encoding
- `iri-to-uri(string)` - IRI to URI conversion
- `escape-html-uri(string)` - HTML URI escaping

### Numeric Functions

**Include:** `functions/func_numbers.cpp`

- `number([value])` - Convert to number
- `sum(node-set)` - Sum numeric values
- `floor(number)` - Round down
- `ceiling(number)` - Round up
- `round(number)` - Round to nearest
- `round-half-to-even(number)` - Banker's rounding
- `abs(number)` - Absolute value
- `min(sequence)` - Minimum value
- `max(sequence)` - Maximum value
- `avg(sequence)` - Average value

### Boolean Functions

**Include:** `functions/func_booleans.cpp`

- `boolean(value)` - Convert to boolean
- `not(boolean)` - Logical negation
- `true()` - Boolean true
- `false()` - Boolean false
- `lang(string)` - Language test

### Sequence Functions

**Include:** `functions/func_sequences.cpp`

- `distinct-values(sequence)` - Remove duplicates
- `index-of(sequence, value)` - Find value positions
- `insert-before(sequence, position, values)` - Insert items
- `remove(sequence, position)` - Remove item
- `reverse(sequence)` - Reverse order
- `subsequence(sequence, start, [length])` - Extract subsequence
- `unordered(sequence)` - Remove ordering
- `exists(sequence)` - Test non-empty
- `empty(sequence)` - Test empty
- `deep-equal(sequence1, sequence2)` - Deep comparison
- `zero-or-one(sequence)` - Cardinality check
- `one-or-more(sequence)` - Cardinality check
- `exactly-one(sequence)` - Cardinality check

### Regular Expression Functions

**Include:** `functions/func_strings.cpp`

- `matches(string, pattern, [flags])` - Pattern matching
- `replace(string, pattern, replacement, [flags])` - Pattern replacement
- `tokenize(string, pattern, [flags])` - Split by pattern
- `analyze-string(string, pattern, [flags])` - Detailed analysis

### Date and Time Functions

**Include:** `functions/func_datetimes.cpp`

**Current Date/Time:**
- `current-date()` - Current date
- `current-time()` - Current time
- `current-dateTime()` - Current date and time
- `implicit-timezone()` - System timezone

**Date/Time Components:**
- `year-from-dateTime(dateTime)` - Extract year
- `month-from-dateTime(dateTime)` - Extract month
- `day-from-dateTime(dateTime)` - Extract day
- `hours-from-dateTime(dateTime)` - Extract hours
- `minutes-from-dateTime(dateTime)` - Extract minutes
- `seconds-from-dateTime(dateTime)` - Extract seconds
- `timezone-from-dateTime(dateTime)` - Extract timezone

**Similar extractors for date and time types**

**Timezone Adjustments:**
- `adjust-dateTime-to-timezone(dateTime, [timezone])` - Adjust timezone
- `adjust-date-to-timezone(date, [timezone])` - Adjust date timezone
- `adjust-time-to-timezone(time, [timezone])` - Adjust time timezone

**Duration Functions:**
- `years-from-duration(duration)` - Extract years
- `months-from-duration(duration)` - Extract months
- `days-from-duration(duration)` - Extract days
- `hours-from-duration(duration)` - Extract hours
- `minutes-from-duration(duration)` - Extract minutes
- `seconds-from-duration(duration)` - Extract seconds

### QName Functions

**Include:** `functions/func_qnames.cpp`

- `QName(namespace, local-name)` - Create QName
- `resolve-QName(string, element)` - Resolve QName from string
- `prefix-from-QName(qname)` - Extract prefix
- `local-name-from-QName(qname)` - Extract local name
- `namespace-uri-from-QName(qname)` - Extract namespace URI
- `namespace-uri-for-prefix(prefix, element)` - Resolve prefix to URI
- `in-scope-prefixes(element)` - Get all prefixes in scope

### Document Functions

**Include:** `functions/func_documents.cpp`

- `doc(uri)` - Load external XML document
- `doc-available(uri)` - Test document availability
- `collection([uri])` - Access document collection
- `uri-collection([uri])` - Get URIs in collection
- `document-uri([node])` - Get document URI
- `unparsed-text(uri, [encoding])` - Read text file
- `unparsed-text-available(uri, [encoding])` - Test text availability
- `unparsed-text-lines(uri, [encoding])` - Read text as line sequence

### Accessor Functions

**Include:** `functions/func_accessors.cpp`

- `data([sequence])` - Extract typed values
- `nilled([node])` - Test for nilled element
- `static-base-uri()` - Get static base URI
- `default-collation()` - Get default collation

### Utility Functions

**Include:** `functions/func_diagnostics.cpp`

- `error([code], [description], [object])` - Raise error
- `trace(value, label)` - Trace value with label

## Variable Binding

The XPath module supports variable binding for parameterised queries.

**Setting Variables (via XML class):**
```cpp
// C++ example
xml->setVariable("threshold", 10.0);
xml->setVariable("category", "fiction");
```

```fluid
-- Fluid example
xml.mtSetVariable('threshold', 10.0)
xml.mtSetVariable('category', 'fiction')
```

**Using Variables in Expressions:**
```xpath
//book[@price < $threshold and @category = $category]
```

**FLWOR Variables:**
Variables defined in `for` and `let` clauses are automatically bound:
```xpath
for $book in //book
let $discount := $book/@price * 0.1
return $discount
```

## Schema-Aware Type System

When an XML document has a loaded schema, the XPath module can leverage schema type information for type-aware operations.

**Schema Integration:**
- Type-aware comparisons
- Type casting and validation
- Schema type information in predicates
- Enhanced type checking in functions

**Example:**
```cpp
// Load schema into XML object
xml->loadSchema("schema.xsd");

// XPath can now use schema types
xp::Compile(xml, "//book[@price cast as xs:decimal > 10.0]", &query);
```

## Namespace Support

The XPath module is fully namespace-aware and integrates with the XML module's namespace system.

**Namespace Prefixes:**
```cpp
// Register namespace prefix in XML object
xml->registerNamespace("bk", "http://example.com/books");

// Use in XPath
xp::Compile(xml, "//bk:book/bk:title", &query);
```

**Default Namespace:**
```xpath
// Access elements in default namespace
//*[local-name()='book']
```

## Performance Considerations

### Expression Compilation

- **Compile Once, Use Many**: Compiled expressions are reusable and thread-safe
- **Compilation Cost**: Upfront cost for complex expressions
- **Memory Management**: Compiled expressions are managed resources

### Memory Management

- **Arena Allocation**: XPath evaluation uses arena-based memory allocation
- **Node Set Efficiency**: Node sets store pointers, not copies
- **String Operations**: Pre-calculated sizes prevent reallocations

### Optimization Strategies

- Compile expressions outside loops for reuse
- Use specific paths rather than `//` when possible
- Leverage predicates to filter early in evaluation
- Consider using `xp::Query()` with callbacks for large result sets
- Cache compiled expressions for frequently used queries

### Function Performance

- String functions pre-calculate buffer sizes
- Numeric functions use optimised algorithms
- Sequence functions minimise allocations
- Regular expression compilation is cached

## Testing Framework

### XPath Tests via XML Module

All XPath tests are located in the XML module's test directory and exercise XPath functionality through the XML class interface:

**XPath Integration Tests (`src/xml/tests/`):**
- `test_xpath_core.fluid` - Core XPath expressions and operators
- `test_xpath_predicates.fluid` - Predicate evaluation
- `test_xpath_axes.fluid` - All 13 XPath axes
- `test_xpath_advanced.fluid` - Complex XPath queries
- `test_xpath_advanced_paths.fluid` - Advanced path expressions
- `test_xpath_flwor.fluid` - FLWOR expressions
- `test_xpath_flwor_clauses.fluid` - Individual FLWOR clauses
- `test_xpath_func_ext.fluid` - Extended function library
- `test_xpath_sequences.fluid` - Sequence operations
- `test_xpath_string_uri.fluid` - String and URI functions
- `test_xpath_duration.fluid` - Duration type operations
- `test_xpath_datetime.fluid` - DateTime functions
- `test_xpath_qname.fluid` - QName operations
- `test_xpath_accessor.fluid` - Accessor functions
- `test_xpath_constructors.fluid` - Node constructors
- `test_xpath_documents.fluid` - Document functions
- `test_setvariable.fluid` - Variable binding

### Running XPath Tests

**Individual Test:**
```bash
cd src/xml/tests && ../../../install/agents/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/xml/tests/test_xpath_core.fluid --gfx-driver=headless --log-warning
```

**All XPath Tests via CMake:**
```bash
ctest --build-config [BuildType] --test-dir build/agents -R xml_xpath
```

### C++ Unit Testing for Internal Components

The XPath module includes a compiled-in unit testing framework for testing internal components that are not easily accessible through the Fluid interface. This is particularly useful for debugging low-level functionality like XQuery prolog integration, parser internals, and data structure integrity.

**Unit Test Infrastructure:**

The module exposes a `xp::UnitTest()` function that can be called to run compiled-in unit tests. This function is defined in:
- **Implementation**: `src/xpath/unit_tests.cpp` - Contains all unit test suites
- **Test Runner**: `src/xpath/tests/test_xpath_unit.cpp` - C++ executable that calls the unit test function

**Creating Unit Tests:**

To add new C++ unit tests for internal XPath components:

1. **Add test functions to `unit_tests.cpp`:**
   ```cpp
   static void test_my_feature() {
      std::cout << "\n--- Testing My Feature ---\n" << std::endl;

      // Test case 1
      {
         // Setup test
         XQueryProlog prolog;
         // ... test code ...

         test_assert(condition, "Test name", "Failure message");
      }

      // Additional test cases...
   }
   ```

2. **Register the test suite in `UnitTest()`:**
   ```cpp
   namespace xp {
   void UnitTest(APTR Meta) {
      reset_test_counters();

      // Run test suites
      test_prolog_api();
      test_my_feature();  // Add your new test

      print_test_summary();
   }
   }
   ```

3. **Build and run the tests:**
   ```bash
   # Build the xpath module (includes unit tests)
   cmake --build build/agents --config Release --target xpath --parallel

   # Install
   cmake --install build/agents

   # Build the test executable
   cmake --build build/agents --config Release --target test_xpath_unit --parallel

   # Copy to install folder and run
   cp build/agents/src/xpath/Release/test_xpath_unit.exe install/agents/
   cd install/agents && ./test_xpath_unit.exe
   ```

**Example Unit Test Structure:**

The current implementation includes tests for XQuery prolog functionality:

```cpp
// Test XQueryProlog API
static void test_prolog_api() {
   // Test function declaration
   XQueryProlog prolog;
   XQueryFunction func;
   func.qname = "local:test";
   func.parameter_names.push_back("x");
   prolog.declare_function(std::move(func));

   auto found = prolog.find_function("local:test", 1);
   test_assert(found not_eq nullptr, "Function declaration",
      "Declared function should be findable");
}
```

**Best Practices for C++ Unit Tests:**

- Test internal data structures and APIs not exposed to Fluid
- Use unit tests for debugging complex integration issues
- Keep tests focused on specific functionality
- Use descriptive test names for easy identification
- Include both positive and negative test cases
- Verify edge cases and boundary conditions
- Test error handling and invalid inputs

**When to Use C++ Unit Tests vs Fluid Tests:**

- **C++ Unit Tests**: Internal APIs, data structures, parser internals, performance-critical code, debugging integration issues
- **Fluid Tests**: End-to-end functionality, user-facing features, XPath expression evaluation, integration with XML module

This dual testing approach ensures comprehensive coverage at both the internal implementation level and the user-facing API level.

## Common Usage Patterns

### Basic Query with Compilation

```cpp
// C++ example
#include <parasol/modules/xml.h>
#include <parasol/modules/xpath.h>

if (auto xml = objXML::create { fl::Path("document.xml") }; xml.ok()) {
   XPathNode *query;
   if (xp::Compile(*xml, "//book[@price < 20]/title", &query) IS ERR::Okay) {
      XPathValue *result;
      if (xp::Evaluate(*xml, query, &result) IS ERR::Okay) {
         // Process result node set
         for (auto *node : result->node_set) {
            log.msg("Found: %s", node->name());
         }
         FreeResource(result);
      }
      FreeResource(query);
   }
}
```

### Callback-Based Node Iteration

```cpp
// C++ example - process each matching node
static ERR process_book(objXML *XML, int TagID, CSTRING Attrib) {
   XMLTag *tag;
   if (XML->getTag(TagID, &tag) IS ERR::Okay) {
      log.msg("Processing book: %s", tag->getContent().c_str());
   }
   return ERR::Okay;
}

XPathNode *query;
if (xp::Compile(xml, "//book[@category='fiction']", &query) IS ERR::Okay) {
   FUNCTION callback = C_FUNCTION(process_book);
   xp::Query(xml, query, &callback);
   FreeResource(query);
}
```

### FLWOR Query

```fluid
-- Fluid example
local xml = obj.new('xml', { path = 'books.xml' })

-- Compile FLWOR expression
local query = 'for $book in //book ' ..
              'where $book/@price < 20 ' ..
              'order by $book/@price ' ..
              'return $book/title'

local err, matches = xml.mtFindTag(query, function(XML, TagID, Attrib)
   local err, tag = XML.mtGetTag(TagID)
   print('Title: ' .. tag.content)
end)
```

### Variable Binding

```cpp
// C++ example with variables
xml->setVariable("minPrice", 10.0);
xml->setVariable("maxPrice", 50.0);
xml->setVariable("category", "fiction");

XPathNode *query;
xp::Compile(xml, "//book[@price >= $minPrice and @price <= $maxPrice "
                 "and @category = $category]", &query);

XPathValue *result;
xp::Evaluate(xml, query, &result);
// Process results...
FreeResource(result);
FreeResource(query);
```

## Error Handling

### Common Error Conditions

- `ERR::Syntax` - XPath expression syntax error
- `ERR::Search` - XPath query matched no nodes (Query function only)
- `ERR::NoData` - XML document is empty
- `ERR::NullArgs` - Required parameter is NULL
- `ERR::NoSupport` - Unsupported XPath feature
- `ERR::AllocMemory` - Memory allocation failure

### Best Practices

- Always check return codes from XPath functions
- Use `xml->ErrorMsg` field for detailed error diagnostics after compilation failures
- Enable appropriate logging levels (`--log-warning` or `--log-api`)
- Validate XPath expressions during development
- Handle `ERR::Search` appropriately (it indicates no matches, not a failure)

## Development Guidelines

### Build Integration

**Module Target:**
```bash
cmake --build build/agents --config [BuildType] --target xpath --parallel
```

**Dependency Note:**
The XPath module depends on the XML module for document structures and the schema system. Always build XML before XPath in dependency order.

## Integration Points

### With XML Module

The XPath module has a tight integration with the XML module:
- Operates on `XMLTag` structures from XML documents
- Uses XML namespace resolution
- Leverages XML schema type system
- Accesses XML document metadata

### With Other Modules

- **Core Module**: Uses Core's object system and memory management
- **Unicode Module**: For text encoding and string operations
- **Regex Module**: Dynamically loaded for pattern matching functions

## Advanced Features

### Custom Function Registration

The XPath function library is extensible through the `XPathFunctionLibrary` class, allowing custom function registration for specialised processing needs.

### Expression Caching

Compiled expressions are expensive to create but cheap to reuse. Consider caching strategies for frequently used expressions.

### Streaming Evaluation

Use the `xp::Query()` function with callbacks for streaming evaluation of large result sets, avoiding memory overhead of materialising all results.

## Related Documentation

- **XML Module**: See `src/xml/AGENTS.md` for XML document handling
- **XPath 2.0 Specification**: W3C XPath 2.0 Recommendation
- **XQuery Specification**: W3C XQuery 1.0 Recommendation
- **API Reference**: See `docs/xml/modules/xpath.xml` for complete API documentation

This guide provides the essential information needed for AI agents to work effectively with the Parasol XPath module, covering architecture, language support, function library, and integration patterns.
