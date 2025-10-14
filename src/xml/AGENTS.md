# XML Module - AI Agent Guide

This file provides comprehensive information about the Parasol XML module for AI agents working with the codebase.

## Overview

The XML module provides robust functionality for creating, parsing, and maintaining XML data structures. It serves as Parasol's general-purpose structured data handler, supporting XML, JSON, YAML, and other structured formats with cross-format conversion capabilities.

**Key Features:**
- Full XML parsing and serialisation with flexible parsing modes
- XPath 2.0 query engine with comprehensive function support
- XML Schema (XSD) validation and type system integration
- Namespace processing and management
- Document validation and well-formedness checking
- In-memory XML tree manipulation
- Cross-format structured data conversion
- Thread-safe operations with C++20 features

## Module Architecture

### Core Files Structure

```
src/xml/
├── xml.fdl              # Interface definition (classes, enums, structs)
├── xml.cpp              # Main XML class implementation
├── xml.h                # Internal header definitions
├── xml_def.c            # Generated C definitions
├── xml_functions.cpp    # XML manipulation functions
├── unescape.cpp         # HTML/XML entity handling
├── schema/              # XML Schema validation system
│   ├── schema_parser.cpp/h       # XSD schema parsing
│   ├── schema_types.cpp/h        # Schema type system and validation
│   └── type_checker.cpp/h        # Type validation logic
├── xpath/               # XPath 2.0 engine implementation
│   ├── xpath_ast.cpp/h       # Abstract Syntax Tree
│   ├── xpath_axis.cpp/h      # XPath axis evaluation
│   ├── xpath_evaluator.cpp/h # Main evaluation engine
│   ├── xpath_functions.cpp/h # XPath function library (registry/dispatch)
│   ├── xpath_parser.cpp/h    # XPath expression parser
│   ├── xpath_arena.h         # Memory management
│   └── functions/            # XPath function implementations by category
│       ├── func_booleans.cpp     # Boolean functions (not, true, false, etc.)
│       ├── func_datetimes.cpp    # Date/time functions (current-date, etc.)
│       ├── func_diagnostics.cpp  # Diagnostic functions (error, trace)
│       ├── func_nodeset.cpp      # Node-set functions (name, local-name, etc.)
│       ├── func_numbers.cpp      # Numeric functions (sum, round, etc.)
│       ├── func_sequences.cpp    # Sequence functions (distinct-values, etc.)
│       └── func_strings.cpp      # String functions (concat, substring, etc.)
└── tests/               # Comprehensive test suite
    ├── test_basic.fluid                # Basic XML operations
    ├── test_advanced_features.fluid    # Complex parsing scenarios
    ├── test_schema_validation.fluid    # XML Schema validation tests
    ├── test_xpath_*.fluid              # XPath functionality tests
    └── ...
```

### Dependencies

- **unicode** module (PUBLIC) - For text encoding/decoding
- **link_regex** library (PRIVATE) - For pattern matching in XPath

## Core Classes and Structures

### XML Class (`objXML`)

Primary class for XML document handling with comprehensive parsing and manipulation capabilities.

**Include:** `<parasol/modules/xml.h>`

**Key Fields:**
- `Path` (str) - File system location of XML data
- `Source` (obj) - Alternative object-based data source
- `Flags` (XMF) - Parsing and behavior flags
- `Start` (int) - Starting cursor position for operations
- `Modified` (int) - Modification timestamp
- `Tags` (TAGS) - Hierarchical array of XMLTag structures

**Critical Implementation Notes:**
- C++ developers get direct access to `Tags` field as `pf::vector<XMLTag>`
- Fluid developers should cache `Tags` reads as they create full copies
- Thread-safe due to object locking principles.

### XMLTag Structure

Represents complete XML elements with attributes, content, and hierarchy.

**Include:** `<parasol/modules/xml.h>`

```cpp
struct XMLTag {
   int ID;                             // Unique tag identifier
   int ParentID;                       // Parent tag ID (0 for root)
   int LineNo;                         // Source line number
   XTF Flags;                          // Tag flags (CDATA, INSTRUCTION, etc.)
   uint NamespaceID;                   // Namespace URI hash
   pf::vector<XMLAttrib> Attribs;      // Attributes array
   pf::vector<XMLTag> Children;        // Child elements array
}
```

**Important Methods:**
- `name()` - Returns tag name (first attribute)
- `hasContent()` - Checks for text content
- `isContent()` - Determines if this is content node
- `getContent()` - Extracts all text content
- `attrib(name)` - Gets attribute value by name

### XMLAttrib Structure

Simple name-value pair for XML attributes and content.

**Include:** `<parasol/modules/xml.h>`

```cpp
struct XMLAttrib {
   std::string Name;   // Attribute name (empty for content)
   std::string Value;  // Attribute/content value
}
```

### Key Enumerations and Flags

**XMF Flags** - XML parsing and behavior options
**Include:** `<parasol/modules/xml.h>`
- `WELL_FORMED` - Require well-formed XML structure
- `INCLUDE_COMMENTS` - Preserve XML comments in parsing
- `STRIP_CONTENT` - Remove all text content during parsing
- `READABLE/INDENT` - Format output with indentation
- `NAMESPACE_AWARE` - Enable namespace processing
- `PARSE_HTML` - Handle HTML escape codes
- `INCLUDE_WHITESPACE` - Retain whitespace between tags

**XTF Flags** - XMLTag type indicators
**Include:** `<parasol/modules/xml.h>`
- `CDATA` - Tag represents CDATA section
- `INSTRUCTION` - Processing instruction (<?xml?>)
- `NOTATION` - Notation declaration (<!XML>)
- `COMMENT` - Comment section (<!-- -->)

**XMI Enum** - Tag insertion positions
**Include:** `<parasol/modules/xml.h>`
- `PREV/PREVIOUS` - Insert before target tag
- `CHILD` - Insert as first child
- `NEXT` - Insert after target tag
- `CHILD_END` - Insert as last child

## Core Functionality

### Document Loading and Parsing

**Multiple Input Methods:**
- `Path` field - File system sources with automatic caching
- `Statement` field - Direct XML string parsing
- `Source` field - Object-based input (any object with Read action)

**Parsing Behaviors:**
- Default: Accepts loosely structured XML
- `XMF::WELL_FORMED` - Requires well-formed XML structure
- `XMF::INCLUDE_COMMENTS` - Preserves XML comments
- `XMF::STRIP_CONTENT` - Removes all text content
- `XMF::NAMESPACE_AWARE` - Enables namespace processing

### XML Schema Validation

The XML module now includes comprehensive XML Schema (XSD) validation support for ensuring document conformance to schema definitions.

**Include:** `src/xml/schema/schema_types.h` (internal), access via XML class methods

**Supported Features:**
- Schema parsing from XSD files
- Type validation (built-in and derived types)
- Element and attribute validation
- Namespace-aware schema processing
- Detailed validation error diagnostics
- Schema-aware XPath type comparisons

**Key Methods (objXML):**
- `LoadSchema()` - Load and parse XSD schema definition
- `ValidateDocument()` - Validate XML document against loaded schema
- Schema validation integrates with namespace processing

**Validation Capabilities:**
- Built-in schema types (string, integer, boolean, date, etc.)
- Complex type definitions with sequences and choices
- Attribute presence and type validation
- Element cardinality validation (minOccurs, maxOccurs)
- Namespace-qualified element validation

### XPath 2.0 Engine

Comprehensive XPath 2.0 implementation with full W3C compliance and schema-aware type system.
**Include:** `src/xml/xpath/xpath_evaluator.h` (internal), access via XML class methods

**Supported Features:**
- All standard axes (child, parent, ancestor, descendant, etc.)
- Predicates with complex expressions
- FLWOR expressions (For, Let, Where, Order by, Return)
- Function library (100+ functions including XPath 2.0 extensions)
- Variables and namespace support
- Node-set, string, number, and boolean operations
- Schema-aware type comparisons and operations

**Key XPath Functions:**
- String: `concat()`, `substring()`, `normalize-space()`, `matches()`
- Numeric: `sum()`, `count()`, `round()`, `ceiling()`, `floor()`
- Boolean: `not()`, `true()`, `false()`
- Node: `name()`, `local-name()`, `namespace-uri()`
- Sequence: `distinct-values()`, `index-of()`, `reverse()`
- XPath 2.0: `error()`, `trace()`, `round-half-to-even()`
- QName: `resolve-QName()`, `QName()`, `prefix-from-QName()`
- DateTime: `current-date()`, `current-time()`, `current-dateTime()`

### Content Manipulation

**Core Methods (all in objXML):**
- `InsertXML()` - Insert XML content at specific positions
- `InsertXPath()` - Insert based on XPath expressions
- `RemoveTag()` - Remove specific tags
- `RemoveXPath()` - Remove tags matching XPath
- `SetAttrib()` - Modify/create attributes
- `GetAttrib()` - Retrieve attribute values
- `MoveTags()` - Relocate tags within document
- `Sort()` - Sort elements by attributes

### Namespace Support

**Namespace Management (objXML methods):**
- `RegisterNamespace()` - Define namespace prefixes
- `GetNamespaceURI()` - Resolve namespace URIs
- `SetTagNamespace()` - Assign namespaces to tags
- `ResolvePrefix()` - Convert prefixes to URIs

### Utility Functions

**XML Namespace Functions**
**Include:** `<parasol/modules/xml.h>` (xml namespace)

```cpp
// Direct tag attribute manipulation
void UpdateAttrib(XMLTag &Tag, const std::string Name, const std::string Value, bool CanCreate = false)
void NewAttrib(XMLTag &Tag, const std::string Name, const std::string Value)
std::string GetContent(const XMLTag &Tag)

// Process all attributes in XML tree
void ForEachAttrib(objXML::TAGS &Tags, std::function<void(XMLAttrib &)> &Function)
```

## Testing Framework

### Test Organization

Comprehensive test suite using Flute test runner covering all module features:

**Core XML Tests:**
- `test_basic.fluid` - Core XML operations and tag access
- `test_advanced_features.fluid` - Complex parsing scenarios
- `test_xml_parsing.fluid` - Parser robustness tests
- `test_xml_manipulation.fluid` - Content modification
- `test_data_sources.fluid` - Multiple input source handling
- `test_namespaces.fluid` - Namespace processing
- `test_error_handling.fluid` - Error condition handling

**XPath Tests:**
- `test_xpath_core.fluid` - Core XPath expressions and operators
- `test_xpath_predicates.fluid` - Predicate evaluation
- `test_xpath_axes.fluid` - All XPath axes (child, parent, ancestor, etc.)
- `test_xpath_advanced.fluid` - Complex XPath queries
- `test_xpath_advanced_paths.fluid` - Advanced path expressions
- `test_xpath_flwor.fluid` - FLWOR expressions
- `test_xpath_func_ext.fluid` - Extended function library
- `test_xpath_sequences.fluid` - Sequence operations
- `test_xpath_string_uri.fluid` - String and URI functions
- `test_xpath2_duration.fluid` - Duration type operations
- `test_xpath2_datetime.fluid` - DateTime functions
- `test_xpath2_qname.fluid` - QName operations
- `test_setvariable.fluid` - Variable binding

**Schema Validation Tests:**
- `test_schema_validation.fluid` - XML Schema validation and type checking

**Performance Tests:**
- `benchmark.fluid` - Performance measurement and optimization

### Running Tests

**Individual Test:**
```bash
cd src/xml/tests && ../../../install/agents/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/xml/tests/test_basic.fluid --gfx-driver=headless --log-warning
```

**All XML Tests via CMake:**
```bash
ctest --build-config Release --test-dir build/agents -R xml_
```

## Common Usage Patterns

### Basic XML Parsing

```fluid
local xml = obj.new('xml', { path = 'document.xml' })
local tags = xml.tags
for i, tag in ipairs(tags) do
   if tag.name == 'target_element' then
      local content = tag.content
      -- Process content
   end
end
```

### XPath Queries - Multiple Matches

```fluid
local err, index = xml.mtFindTag('//book[@category="fiction"]',  function(XML, TagID, Attrib)
   local err, tag = XML.mtGetTag(TagID)
   -- Process matching tags
end)
```

### XML Schema Validation

```fluid
-- Load schema definition
local xml = obj.new('xml', { path = 'document.xml', flags = '!NAMESPACE_AWARE' })
local err = xml.mtLoadSchema('schema.xsd')
if err == ERR_Okay then
   -- Validate document against schema
   err = xml.mtValidateDocument()
   if err == ERR_Okay then
      print('Document is valid')
   else
      print('Validation failed: ' .. mSys.GetErrorMsg(err))
   end
end
```

### Content Manipulation

```cpp
for (auto &tag : xml->Tags) {
   if (tag.name() IS "target") {
      xml::UpdateAttrib(tag, "modified", "true", true);
   }
}
```

## Development Guidelines

### Code Style Requirements

**Standard Patterns:**
- Three-space indentation
- Upper camel-case for function arguments
- Lower snake_case for local variables
- British English spelling in all text

### Build Integration

**Module Target:**
```bash
cmake --build build/agents --config Release --target xml --parallel
```

**Test Registration:**
```cmake
flute_test(xml_custom "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_custom.fluid")
```

### Schema System Architecture

The XML Schema validation system is integrated into the module with these components:

**Schema Parser** (`schema/schema_parser.cpp/h`):
- Parses XSD schema definitions
- Builds internal schema type registry
- Handles namespace-qualified schema elements

**Type System** (`schema/schema_types.cpp/h`):
- Defines built-in and derived schema types
- Provides type validation interface
- Supports complex type definitions

**Type Checker** (`schema/type_checker.cpp/h`):
- Validates element and attribute values against schema types
- Checks element cardinality constraints
- Reports detailed validation errors

## Performance Considerations

### Memory Management

- **Tags Array**: Direct C++ access avoids copying, Fluid access creates copies
- **Content Extraction**: Pre-calculated string sizes prevent reallocations
- **XPath Evaluation**: Arena-based memory allocation for expression evaluation
- **Namespace Hashing**: URI hashes for fast namespace lookups
- **Schema Validation**: Schema definitions cached after initial load

### Optimization Strategies

- Cache `Tags` reads in Fluid scripts
- Use specific XPath expressions rather than broad queries
- Enable namespace processing only when required (`XMF::NAMESPACE_AWARE`)
- Consider `XMF::STRIP_CONTENT` for metadata-only operations
- Load schemas once and reuse XML objects for multiple validations
- Schema validation requires namespace awareness for qualified elements

## Error Handling

### Common Error Conditions

- `ERR::BadData` - Malformed XML when `XMF::WELL_FORMED` enabled
- `ERR::Search` - XPath expression matches no nodes
- `ERR::InvalidPath` - Invalid file path in `Path` field
- `ERR::NoSupport` - Unsupported XPath feature
- `ERR::Args` - Invalid arguments to XML methods
- `ERR::Failed` - Schema validation failure (invalid document structure or types)
- `ERR::FileNotFound` - Schema file not found during `LoadSchema()`

### Best Practices

- Always check return codes from XML methods
- Use `xml->ParseError` field for detailed parsing diagnostics
- Enable appropriate logging levels (`--log-warning` minimum)
- Validate XPath expressions before batch processing
- Enable `XMF::NAMESPACE_AWARE` when using schema validation
- Schema validation errors are logged with detailed diagnostics
- Use `ValidateDocument()` after successful `LoadSchema()`

## Advanced Features

### Schema-Aware XPath

The XPath engine integrates with the schema validation system to provide type-aware comparisons and operations. When a schema is loaded, XPath expressions can leverage schema type information for more accurate evaluations.

### Variable Binding

The XPath engine supports variable binding through the `SetVariable()` method, enabling parameterized XPath queries. Variables can be referenced in XPath expressions using the `$variable` syntax.

### Custom Functions

The XPath engine supports variable binding and custom function registration for specialized processing needs through the extensible function registry.

### Cross-Format Support

While primarily XML-focused, the module architecture supports extension to other structured data formats (JSON, YAML) through the same tag-based representation.

## Integration Points

### With Other Modules

- **Document Module**: XML provides structured data for RIPL document processing
- **SVG Module**: XML parsing serves as foundation for SVG document handling
- **Core Module**: Utilizes Core's file system and object management
- **Fluid Module**: Provides scripted access to all XML functionality

### External Dependencies

- Unicode module for text encoding/decoding operations
- SRELL regex library for pattern matching in XPath expressions
- Standard C++ libraries (C++20 features utilized throughout)

This guide provides the essential information needed for AI agents to work effectively with the Parasol XML module, covering architecture, functionality, testing, and integration patterns.