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
  - Application screenshots (`widgets.png`, `vue.png`, etc.)
  - Vector graphics examples (`supershapes.svg`, `wave.svg`, etc.)
  - Interactive demos and transitions

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

## 📖 How to Use This Documentation Effectively

### For Understanding APIs
1. **Start with wiki guides** for conceptual understanding
2. **Use HTML class pages** for detailed API reference
3. **Check XML files** for precise technical specifications

### For Fluid Scripting
1. **`Fluid-Reference-Manual.md`** for language fundamentals
2. **`Fluid-GUI-API.md`** for user interface development
3. **HTML class pages** for specific object APIs
4. **Examples in `examples/`** for practical patterns

### For C++ Development
1. **`Parasol-Objects.md`** for object system concepts
2. **XML class documentation** for precise API specifications
3. **`FDL-Reference-Manual.md`** for interface definitions
4. **Source code in `src/`** for implementation details

### For Testing and Build
1. **`Unit-Testing.md`** for Flute test framework
2. **Build guide matching your platform** (Linux/Windows)
3. **`Customising-Your-Build.md`** for configuration options

## 🔄 Documentation Generation Process

The documentation follows this generation pipeline:

1. **C++ Source** → **XML** (via FDL parsing)
2. **XML** → **HTML** (via XSLT transformation)
3. **Markdown** → **HTML** (via documentation generator)

Key scripts:
- **`docs/generate.fluid`** - Main documentation generator
- **`docs/generate-wiki.fluid`** - Wiki-specific processing
- **Saxon XSLT processor** in `docs/saxon/` for XML→HTML transformation

## 💡 Best Practices for Claude Sessions

### When Researching Framework Concepts
- Start with relevant wiki pages for conceptual understanding
- Use HTML documentation for comprehensive API browsing
- Reference XML for precise technical specifications

### When Writing Code
- Check existing examples in `examples/` directory first
- Use API documentation to understand object interfaces
- Follow patterns established in wiki tutorials

### When Debugging Issues
- Consult error codes in `System-Error-Codes.md`
- Review action documentation for correct usage patterns
- Check class field documentation for property requirements

### When Working with Fluid Scripts
- Always study existing `.fluid` files for patterns
- Use GUI API documentation for interface constants
- Reference class HTML pages for object method signatures

This documentation ecosystem provides comprehensive coverage from high-level concepts to low-level implementation details, enabling effective development with the Parasol framework.