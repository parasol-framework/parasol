# Documentation Guide for Claude Sessions

This guide describes the comprehensive documentation available in the `docs/` directories to help Claude sessions navigate and utilize Parasol framework documentation effectively.

## Documentation Structure Overview

The Parasol framework maintains three parallel documentation systems in the `docs/` directory:

- **`docs/wiki/`** - Community-oriented guides and tutorials (Markdown source)
- **`docs/html/`** - Complete website with API references and galleries (HTML output)
- **`docs/xml/`** - Machine-generated API documentation from source code (XML format)

## 📚 docs/wiki/ - Community Guides and Tutorials

**Location:** `docs/wiki/` (source) and `docs/html/wiki/` (rendered HTML)

The wiki contains practical, community-oriented documentation covering:

### Build and Setup
- **`Linux-Builds.md`** - Linux compilation instructions
- **`Windows-Builds.md`** - Windows compilation instructions
- **`Customising-Your-Build.md`** - Build configuration options

### Core Concepts
- **`Home.md`** - Main wiki landing page with navigation
- **`Parasol-Objects.md`** - Object system fundamentals
- **`Parasol-In-Depth.md`** - Advanced framework concepts

### Fluid Scripting Documentation
- **`Fluid-Reference-Manual.md`** - Core Fluid language reference
- **`Fluid-Common-API.md`** - Standard library functions
- **`Fluid-GUI-API.md`** - GUI toolkit APIs and constants
- **`Fluid-JSON-API.md`** - JSON processing utilities
- **`Fluid-VFX-API.md`** - Visual effects and animation APIs
- **`Widgets.md`** - Widget system documentation

### RIPL Document Engine
- **`RIPL-Reference-Manual.md`** - Rich text document formatting system

### Development Tools
- **`Unit-Testing.md`** - Flute test framework documentation
- **`Parasol-Cmd-Tool.md`** - Command-line tool usage
- **`FDL-Tools.md`** - Interface Definition Language tools
- **`FDL-Reference-Manual.md`** - FDL syntax and usage

### Reference Materials
- **`Action-Reference-Manual.md`** - Object action documentation
- **`System-Error-Codes.md`** - Error code reference
- **`Embedded-Document-Formatting.md`** - Documentation markup standards

**Key Usage Notes:**
- These are practical, tutorial-style documents
- Written for developers learning the framework
- Complement the technical API documentation
- Include working examples and best practices

## 🌐 docs/html/ - Complete Website Documentation

**Location:** `docs/html/` (complete self-contained website)

The HTML documentation provides a fully browsable website experience:

### Main Sections
- **`index.html`** - Framework homepage and overview
- **`gallery.html`** - Visual showcase of framework capabilities
- **`modules/api.html`** - API documentation landing page

### API Documentation Structure
- **`modules/`** - Module-level documentation
  - `core.html`, `vector.html`, `display.html`, `network.html`, etc.
- **`modules/classes/`** - Individual class documentation (70+ classes)
  - `vector.html`, `surface.html`, `file.html`, `bitmap.html`, etc.
  - Each class page includes methods, fields, actions, and examples

### Gallery Assets
- **`gallery/`** - Screenshots and demonstrations

### Web Infrastructure
- **`css/`** - Bootstrap-based styling
- **`js/`** - Interactive functionality
- **`images/`** - Framework logos and icons

**Key Usage Notes:**
- Complete offline browsable documentation
- Generated from XML source via XSLT transforms
- Includes visual examples and galleries
- Cross-referenced with hyperlinks between classes and modules

## 📄 docs/xml/ - Machine-Generated API Documentation

**Location:** `docs/xml/` (XML source files)

Raw structured documentation extracted directly from C++ source code:

### Module Documentation
- **`modules/core.xml`** - Core system documentation
- **`modules/vector.xml`** - Vector graphics module
- **`modules/display.xml`** - Display management module
- **`modules/network.xml`** - Network communications module
- And others for each framework module

### Class Documentation
- **`modules/classes/`** - Individual class XML files
  - **`vector.xml`** - Abstract vector graphics base class
  - **`surface.xml`** - Display surface management
  - **`file.xml`** - File system operations
  - **`bitmap.xml`** - Image processing
  - 70+ additional class files covering all framework functionality

### Structure and Content
Each XML file contains:
- **Class metadata** - Name, module, version, description
- **Actions** - Available operations (Draw, Read, Write, etc.)
- **Methods** - Public method interfaces and parameters
- **Fields** - Object properties and their types
- **Constants** - Enumeration values and flags
- **Source references** - Links to implementation files

**Key Usage Notes:**
- Authoritative technical reference
- Generated directly from C++ source via parsing
- Structured data suitable for programmatic processing
- Used to generate the HTML documentation

## 🤖 AI-Optimized Documentation System

**Location:** `docs/xml/ai/` files and `tools/docgen-ai.fluid`

For Claude Code sessions, a specialized condensed documentation format is available:

### AI Documentation Files
- **`ai-condensed.xsd`** - XML Schema for the condensed format
- **`ai-condense.xsl`** - XSLT transformation from full XML to condensed format
- **`../tools/docgen-ai.fluid`** - Build script for AI documentation

### Condensed Format Features
The AI documentation system creates ultra-compact XML files optimized for AI processing:

- **95%+ size reduction** from original XML files
- **Complete API coverage** - all modules, classes, methods, fields
- **Essential information preserved** - signatures, types, constants, errors
- **Abbreviated element names** - `<m>` for modules, `<cl>` for classes, etc.
- **Short comments only** - verbose descriptions removed
- **Machine-readable structure** - consistent XML format

### Generation and Usage
```bash
# Generate AI-optimized documentation
parasol tools/docgen-ai.fluid

# Custom output location
parasol tools/docgen-ai.fluid output=my-docs.xml
```

The generated files in `docs/xml/ai/modules/` and `docs/xml/ai/classes/` provide comprehensive API reference for AI assistants while consuming minimal context window space.

## 📖 How to Use This Documentation Effectively

### For Understanding APIs
1. **Start with wiki guides** for conceptual understanding
4. **Load AI documentation** for comprehensive API coverage in AI context
3. **Check XML files** for extended commentary and examples

### For Fluid Scripting
1. **`Fluid-Reference-Manual.md`** for language fundamentals
2. **`Fluid-GUI-API.md`** for user interface development
3. **XML class pages** for specific object APIs
4. **Examples in `examples/`** for practical patterns

### For C++ Development
1. **`Parasol-Objects.md`** for object system concepts
2. **XML class documentation** for precise API specifications
3. **`FDL-Reference-Manual.md`** for how to write FDL interface definitions
4. **Source code in `src/`** for implementation details

### For Testing and Build
1. **`Unit-Testing.md`** for Flute test framework
2. **Build guide matching your platform** (Linux/Windows)
3. **`Customising-Your-Build.md`** for configuration options

## 🔄 Documentation Generation Process

The documentation follows this generation pipeline:

1. **C++ Source** → **XML** (via FDL parsing)
2. **XML** → **HTML** (via XSLT transformation)
3. **XML** → **AI-optimized XML** (via condensed transformation)
4. **Markdown** → **HTML** (via documentation generator)

Key scripts:

- **`tools/docgen.fluid`** - Main documentation generator
- **`tools/docgen-wiki.fluid`** - Wiki-specific processing
- **`tools/docgen-ai.fluid`** - AI documentation generator

## 💡 Best Practices for Claude Sessions

### When Researching Framework Concepts
- Start with relevant wiki pages for conceptual understanding
- Load AI documentation for complete API coverage
- Reference XML for detailed technical specifications
- **NEVER** read documentation from `docs/html` when writing code, use the XML documentation instead.

### When Writing Code
- Check existing examples in `examples/` directory first
- Use API documentation to understand object interfaces
- Follow patterns established in wiki tutorials
- Reference condensed AI docs for quick API lookup
- If there are discrepencies between documentation and code, bring it to the user's attention

### When Debugging Issues
- Consult error codes in `docs/wiki/System-Error-Codes.md`
- Review action documentation for correct usage patterns
- Check class field documentation for property requirements

### When Working with Fluid Scripts
- Always study existing `.fluid` files for patterns
- Use GUI API documentation for interface constants
- Reference class XML pages for object method signatures
- Use AI documentation for complete method/field reference

### For AI Documentation Maintenance
The AI documentation should be regenerated when:
- New classes or modules are added to the framework
- API signatures change in existing code
- Field definitions or access patterns are modified
- Constants or enumerations are updated

This documentation ecosystem provides comprehensive coverage from high-level concepts to low-level implementation details, with specialized AI-optimized formats for efficient context usage, enabling effective development with the Parasol framework.