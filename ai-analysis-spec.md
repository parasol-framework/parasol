# AI-Enhanced Function Documentation Specification

This specification defines the XML schema for AI-optimized function documentation generated from Parasol C++ source code.

## Overview

The AI analysis system parses C++ source files looking for `-FUNCTION-` markers and generates comprehensive XML documentation that includes both human-readable information and machine-readable metadata for AI consumption.

## XML Schema

### Root Element: `<functions>`
Container for all function documentation in a module.

**Attributes:**
- `module`: Name of the module (e.g., "core", "vector", "svg")
- `generated`: ISO 8601 timestamp of generation

### Function Element: `<function>`
Each function documented with `-FUNCTION-` marker.

**Required child elements:**
- `<source_file>`: Path to the source file relative to module root
- `<name>`: Function name
- `<category>`: Functional category
- `<comment>`: Brief description
- `<prototype>`: Function signature
- `<input>`: Parameters section
- `<description>`: Detailed description
- `<result>`: Return value and error conditions
- `<ai>`: AI-specific metadata

### Input Parameters: `<input>`
Contains `<param>` elements for each parameter.

**Param attributes:**
- `type`: C++ type (e.g., "INT", "MEM", "APTR *")
- `name`: Parameter name
- `lookup` (optional): Reference to type definition

### Result Section: `<result>`
**Attributes:**
- `type`: Return type (typically "ERR")

Contains `<error>` elements with:
- `code`: Error code name
- Content: Description of when this error occurs

### AI Section: `<ai>`
Machine-readable metadata for AI consumption.

#### Semantics: `<semantics>`
Boolean and categorical attributes describing function behavior:
- `side_effects`: true/false
- `zero_initialized`: true/false (for memory functions)
- `tracked`: true/false (for resource functions)
- `ownership`: "contextual", "caller", "global", "none"
- `lock_required`: "always", "conditional", "never"
- `thread_safe`: true/false
- `idempotent`: true/false
- `automatic_cleanup`: true/false

#### Constraints: `<constraints>`
Parameter validation rules as `<param>` elements with attributes:
- `name`: Parameter name
- `required`: true/false
- `min`: Minimum value (for numeric types)
- `max`: Maximum value (for numeric types)
- `required_if`: Conditional requirement expression
- `mutually_exclusive_with`: Other parameter names

#### Flags: `<flags>`
For functions accepting flag parameters, each `<flag>` element has:
- `name`: Flag constant name
- `effect`: Description of flag's behavior
- `compatibility`: Compatible/incompatible flags
- `default`: true/false if this is a default behavior

#### Locking: `<locking>`
Describes locking behavior:
- `<condition>`: When locking occurs
- `<failure_code>`: Error returned if locking fails
- `<mechanism>`: Internal locking mechanism used

#### Workflows: `<workflows>`
Common usage patterns as `<pattern>` elements:
- `name`: Pattern identifier
- `<step>`: Sequence of function calls
- `<description>`: When to use this pattern
- `<preconditions>`: Required state before pattern
- `<postconditions>`: Resulting state after pattern

#### Performance: `<performance>`
- `<complexity>`: time and space complexity
- `<cost_factors>`: Factors affecting performance
- `<optimization_hints>`: Performance improvement suggestions

#### Anti-patterns: `<antipatterns>`
Common mistakes as `<mistake>` elements with descriptions.

#### Relationships: `<relationships>`
Function dependencies and effects:
- `<creates>`: Resources created
- `<requires>`: Prerequisites
- `<cleanup_trigger>`: What triggers cleanup
- `<related_functions>`: Other functions with relationships

#### Metadata: `<metadata>`
Internal implementation details:
- `<internal>`: Implementation-specific information
- `<trackers>`: Global variables used for tracking
- `<locking_mechanism>`: Mutex/lock names
- `<allocation_source>`: System calls used
- `<id_generation>`: ID counter mechanisms

#### Examples: `<example>`
Code examples with:
- `language`: Programming language
- `<code>`: Example code block
- `<description>`: Explanation of example

## Parsing Rules

### Source Code Analysis
1. Locate `-FUNCTION-` markers in C++ source files
2. Extract documentation block until `-END-` marker
3. Parse existing documentation sections:
   - Basic info (name, description)
   - `-INPUT-` parameters
   - `-ERRORS-` return values
   - Extended sections (semantics, constraints, etc.)

### Code Analysis
1. Extract function signature after documentation block
2. Analyze parameter types and names
3. Identify system calls and global variable usage
4. Detect locking mechanisms and resource management patterns
5. Infer semantic properties from code patterns

### XML Generation
1. Create well-formed XML following this specification
2. Include both parsed documentation and inferred analysis
3. Validate against schema before writing
4. Output to `ai-analysis.xml` in module source directory

## Example Output Structure

```xml
<?xml version="1.0" encoding="UTF-8"?>
<functions module="core" generated="2025-01-25T12:00:00Z">
  <function>
    <source_file>lib_memory.cpp</source_file>
    <name>AllocMemory</name>
    <category>Memory</category>
    <comment>Allocates a managed memory block on the heap.</comment>
    <prototype>ERR AllocMemory(INT Size, MEM Flags, APTR * Address, MEMORYID * ID)</prototype>
    
    <input>
      <param type="INT" name="Size">The size of the memory block in bytes. Must be greater than zero.</param>
      <param type="MEM" name="Flags" lookup="MEM">Optional allocation flags controlling behavior and ownership.</param>
      <param type="APTR *" name="Address">Pointer to store the address of the allocated memory block.</param>
      <param type="MEMORYID *" name="ID">Pointer to store the unique identifier of the allocated memory block.</param>
    </input>

    <description>
      <!-- Human-readable description -->
    </description>

    <result type="ERR">
      <error code="Okay">Memory block successfully allocated.</error>
      <error code="Args">Invalid parameters.</error>
      <!-- Additional error codes -->
    </result>

    <ai>
      <semantics>
        <memory side_effects="true" zero_initialized="true" tracked="true" ownership="contextual" lock_required="conditional"/>
        <concurrency thread_safe="true"/>
        <cleanup automatic="true"/>
        <idempotent>false</idempotent>
      </semantics>
      
      <!-- Additional AI sections as specified above -->
    </ai>
  </function>
  
  <!-- Additional functions -->
</functions>
```

## Validation

Generated XML must:
1. Be well-formed XML
2. Include all required elements
3. Have consistent attribute naming
4. Validate parameter references
5. Include meaningful AI analysis beyond basic documentation

## Usage

The generated `ai-analysis.xml` files enable:
- AI systems to understand function behavior and relationships
- Automated code generation and validation
- Enhanced documentation tools
- API usage pattern analysis
- Error handling verification