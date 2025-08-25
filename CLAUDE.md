# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System and Common Commands

Parasol uses CMake as its primary build system. The framework can be built as either modular (shared libraries) or static libraries.

### Essential Build Commands

**Configure build:**
- Release: `cmake -S . -B build/claude -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install/claude -DRUN_ANYWHERE=TRUE -DPARASOL_STATIC=OFF -DBUILD_DEFS=ON`
- Debug: `cmake -S . -B build/claude-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install/claude-debug -DRUN_ANYWHERE=TRUE -DPARASOL_STATIC=OFF -DPARASOL_VLOG=TRUE`
- Static build: Add `-DPARASOL_STATIC=ON` to any configuration

**Build and install:**
- Build: `cmake --build build/claude --config Release -j 8`
- Install: `cmake --build build/claude --config Release -j 8 && cmake --install build/claude`
- To build an individual module, append `--target [module]` to the build command, e.g. `--target network`.

**Testing:**
- **ALWAYS** install your latest build before running `ctest`.
- Run all integration tests: `ctest --build-config Release --test-dir build/claude`
- Run single integration test: `ctest --build-config Release --test-dir build/claude -L TEST_LABEL`
- **ALWAYS** write Fluid tests using Flute unless instructed otherwise (see Flute Testing section below)

### CMake Configuration Options

Key build options (use with `-D` flag):
- `PARASOL_STATIC=ON/OFF` - Build as static libraries instead of modules
- `BUILD_TESTS=ON/OFF` - Enable/disable test building
- `BUILD_DEFS=ON/OFF` - Auto-generate C/C++ headers from FDL files
- `RUN_ANYWHERE=ON/OFF` - Build for local folder execution
- `DISABLE_*=ON/OFF` - Disable specific modules (SSL, X11, AUDIO, etc.)
- `PARASOL_VLOG=ON/OFF` - Enables trace level log messages in debug builds (has no effect on release builds).

## Architecture Overview

### Core Framework Structure

**Parasol Framework** is a vector graphics engine and application framework designed for creating scalable user interfaces. The framework automatically handles display resolution and scaling concerns, allowing developers to focus on application logic rather than display technicalities. Key architectural components include:

1. **Core System** (`src/core/`) - Base object system, memory management, filesystem, and module loading
2. **Vector Graphics Engine** (`src/vector/`) - Main graphics rendering system with scene graphs, filters, and painters
3. **SVG Support** (`src/svg/`) - W3C-compliant SVG parsing and rendering with SMIL animation
4. **Display Management** (`src/display/`) - Cross-platform window management, surfaces, and input handling
5. **Fluid Scripting** (`src/fluid/`) - Lua-based scripting environment built on LuaJIT
6. **Document Engine** (`src/document/`) - RIPL text layout engine for rich document rendering

### Module System

The framework uses a modular architecture where each major feature is implemented as a separate module:
- Each module is in `src/[module_name]/` with its own `CMakeLists.txt`
- Static builds link all modules into the core, while modular builds load them dynamically
- Module definitions are stored in `.fdl` files which generate C headers and IDL strings

### Object System and FDL Files

Parasol uses Interface Definition Language (IDL) files with `.fdl` extension:
- FDL files define classes, methods, fields, and constants
- Build system generates C/C++ headers from FDL using tools in `tools/idl/`
- Class implementations are in `class_*.cpp` files
- Generated headers go to `include/parasol/` directory

### Scripting Integration

**Fluid** is the integrated Lua-based scripting language:
- Built on LuaJIT 2.1 for performance
- Provides high-level access to all framework APIs
- GUI toolkit available through `scripts/gui/` modules (modular widget system)
- Test scripts use `.fluid` extension
- Declarative UI creation with automatic scaling and layout management
- Callback-driven architecture for event handling
- Fluid APIs and reference manuals are available in multiple files at `docs/wiki/Fluid-*.md`.
- The Fluid object interface is case sensitive.  Object fields are accessed as lower snake-case names, e.g. `netlookup.hostName`

#### Fluid Script Execution Model

**CRITICAL: Fluid scripts execute top-to-bottom with NO entry point function**
- Always study existing `.fluid` files (like `docs/generate.fluid`, `examples/*.fluid`) to understand patterns
- API documentation in `docs/xml/modules` and `docs/xml/modules/classes` can be utilised to understand class and module interfaces in detail.

#### Fluid Coding Patterns

**Always study existing examples first:**
```bash
# Key example files to examine:
examples/*.fluid          # Application examples
docs/generate.fluid       # Process execution example
tools/*.fluid             # Build and utility scripts
scripts/gui/*.fluid       # GUI component examples
scripts/*.fluid           # APIs
```

## Key Development Patterns

### Flute Testing

Tests are written in Fluid and executed with the Flute test runner:
- Test files are typically named `test_*.fluid` in module directories.
- Read at least 3 Flute test files to learn the patterns before writing your first test file.
- Use `flute_test()` CMake function to register tests
- Tests run post-install against the installed framework
- Always use `--gfx-driver=headless` for CI/automated testing
- You can append `--log-api` to the runner to see log messages

**Proper Flute Test Command Format:**

**For Windows (use relative paths to avoid path separator issues):**
```bash
cd src/network/tests && ../../../install/claude/parasol.exe ../../../tools/flute.fluid file=E:/parasol/src/network/tests/test_bind_address.fluid --gfx-driver=headless
```

**For Linux:**
```bash
cd "path/to/module/directory" && /path/to/parasol.exe /path/to/tools/flute.fluid file=/absolute/path/to/test.fluid --gfx-driver=headless
```

**Example - Running SVG tests:**
```bash
cd "src/svg/tests" && ../../../install/claude/parasol.exe ../../../tools/flute.fluid file=/full/path/to/src/svg/tests/test_svg.fluid --gfx-driver=headless
```

**Key Requirements for Flute Tests:**
- Must `cd` to the directory containing the test file
- **Windows:** Use relative paths for executables (e.g., `../../../install/claude/parasol.exe`) to avoid Windows path separator issues in Bash
- **Linux:** Use absolute paths for executables
- Use absolute path for the test file parameter (`file=...`) - this works cross-platform
- This ensures proper variable initialization (e.g., `glSVGFolder` for SVG tests)

### Code Generation

The build system heavily uses code generation:
- FDL files are processed by `idl-c.fluid` to generate C headers
- `idl-compile.fluid` generates IDL definition strings
- Generated files are created in build directories and copied to `include/`
- Use `BUILD_DEFS=OFF` to skip generation if no Parasol executable is available

### Multi-Platform Considerations

- Core code is cross-platform (Windows/Linux/MacOS)
- Platform-specific code is in subdirectories (`win32/`, `x11/`, etc.)
- Build system auto-detects platform capabilities (X11, SSL, etc.)
- Windows builds support both MSVC and MinGW toolchains

**Windows-Specific Notes:**
- Use forward slashes `/` in CMake paths and absolute file paths
- In Bash commands, quote paths with spaces: `"path with spaces"`
- For Flute tests, use relative paths for executables to avoid path separator issues

## Working with Vector Graphics

The vector graphics system is the core of Parasol and provides unique capabilities:
- **API-accessible scene graphs** - Hierarchical scene graphs specifically for vector graphics with full programmatic access
- **SVG-to-scene graph integration** - SVG files are parsed directly into manipulable scene graphs for real-time modification
- **Resolution independence** - All graphics scale automatically across display resolutions and DPI settings
- Filters provide effects like blur, lighting, and color manipulation
- Painters handle gradients, patterns, and image fills
- Animation support includes SMIL (SVG animation) and custom VFX
- Real-time manipulation of individual scene graph nodes for dynamic graphics

**Distinctive Features:**
Unlike most vector graphics libraries that use immediate-mode rendering, Parasol maintains retained scene graphs that can be modified at runtime. This enables dynamic, scalable graphics where individual elements can be manipulated programmatically.

## Development Guidelines

### ⚠️ CRITICAL PROJECT REQUIREMENTS (Override Standard C++ Practices)

**These rules MUST be followed and override common C++ conventions:**

- **NEVER use `static_cast`** - Use C-style casting instead, e.g. `int(variable)` NOT `static_cast<int>(variable)`
- **NEVER use `&&`** - Use `and` instead of `&&`
- **NEVER use `||`** - Use `or` instead of `||`
- **NEVER use `==`** - Use the `IS` macro instead of `==`
- **NEVER use C++ exceptions** - Error management relies on checking function results

### 📋 MANDATORY CODING CHECKLIST

Before considering ANY C++ code changes complete, verify:

- [ ] All `&&` replaced with `and`
- [ ] All `||` replaced with `or`
- [ ] All `==` replaced with `IS` macro
- [ ] All `static_cast` replaced with C-style casts
- [ ] No C++ exceptions used
- [ ] All trailing whitespace removed
- [ ] Code compiles successfully
- [ ] Follows formatting standards below

For Fluid code, verify:

- [ ] All `~=` replaced with `!=`
- [ ] All trailing whitespace removed

### Additional Code Style Standards

- Always use upper camel-case for the names of function arguments in C++ code.
- Always use lower snake_case for the names of variables inside C++ functions.
- Use three spaces for tabulation in C++ and Fluid code.
- C++ functions that use global variables must be written with thread safety in mind.
- Use modern C++ conventions where possible, up to and including C++20 features.

### Testing

**MANDATORY: Always compile after making C++ changes**
- After making changes to C++ source files, you MUST verify compilation by building the affected module(s)
- This is required before considering any code changes complete
- Use the module-specific build target to test individual modules quickly

**Module Build Commands:**
```bash
# Build specific module (e.g., network, vector, svg, etc.)
cmake --build build/claude --config Release --target [module_name] -j 8

# Examples:
cmake --build build/claude --config Release --target network -j 8    # For network changes
cmake --build build/claude --config Release --target vector -j 8     # For vector changes
cmake --build build/claude --config Release --target svg -j 8        # For SVG changes
```

**Full Build Commands:**
```bash
# Build everything
cmake --build build/claude --config Release -j 8

# Install after successful build
cmake --install build/claude
```

**SSL-Specific Testing:**
- If working with SSL code, ensure build is configured with SSL support

### Documentation

- API documentation is embedded within comment sections of the C++ source files.
- Embedded documentation is identified with markers in the format `-MARKER-` whereby `MARKER` describes a section for the document generator to parse.
- Marked document sections are terminated at the end of their comment or when `-END-` is encountered.
- Embedded documentation for each function is identified by the `-FUNCTION-` marker.
- Embedded documentation for each class is identified by the `-CLASS-` marker.
- Classes can export actions and methods, which are identified the `-ACTION-` and `-METHOD-` markers.
- Embedded documentation for class fields are identified by the `-FIELD-` marker.
- Always use British English spelling in documentation, comments and variable names.

## Module Dependencies

Key dependencies between modules:
- Most graphics modules require `vector`, `display`, and `font`
- `document` requires `vector`, `display`, and `font` for rich text layout
- `svg` requires `vector` for scene graph rendering
- `scintilla` requires `vector` and `font` for text editing widgets
- `fluid` is independent but provides scripting access to all modules

## File Organization

- `src/` - All source code organized by module
- `include/parasol/` - Public API headers (many auto-generated)
- `scripts/` - Fluid standard library and GUI toolkit
- `tools/` - Build tools and utilities (IDL processors, test runner)
- `examples/` - Example applications and demonstrations (examine git-tracked .fluid files for current examples)
- `data/` - Icons, fonts, styles, and configuration files
- `docs/wiki/` - Markdown files for the GitHub Wiki, includes practical tutorials and guides on how to use Parasol.
- `docs/html/` - Contains the entire Parasol website for offline viewing.
- `docs/xml/` - Auto-generated API documentation in XML format.  This content is sourced from the Parasol C++ files.

### Key Examples for Learning

- **`examples/widgets.fluid`** - Primary showcase of Parasol's GUI capabilities, demonstrates standard widgets and UI patterns
- **`examples/vue.fluid`** - File viewer supporting SVG, RIPL, JPEG, PNG - shows document and graphics integration
- **`examples/gradients.fluid`** - Interactive gradient editor demonstrating real-time vector graphics manipulation
- **`tools/http_server.fluid`** - HTTP server implementation with NetSocket usage patterns
- **`tools/idl/idl-c.fluid`** - Extensive file I/O and general API usage

## Agentic Behaviour

- Always give an honest, balanced opinion in your responses
- Always use the fluid-code-expert agent for Fluid coding
