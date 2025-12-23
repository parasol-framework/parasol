# Parser Implementation Notes

This file captures parser-specific practices and gotchas for the LuaJIT 2.1 parser that ships inside Parasol. The parser has been modernised to C++20 and refactored into focused modules.

The parser is built as an amalgamated build, with all source files included in a single compilation unit at `parser.cpp`. This simplifies dependency management and ensures consistent compiler settings across the entire parser codebase.

## Parser File Structure

The parser has been modernised to C++20 and refactored into focused modules:

**Core Infrastructure:**
- `parse_types.h` - Type definitions, enums, and constexpr helper functions for expressions and scopes
- `parse_concepts.h` - C++20 concepts for compile-time type validation (15+ concepts)
- `parse_raii.h` - RAII guards (ScopeGuard, RegisterGuard, VStackGuard) with move semantics
- `parse_internal.h` - Internal function declarations and template helpers for bytecode emission

**Lexer:**
- `lexer.h` / `lexer.cpp` - Tokenisation and lexical analysis

**Parser Core:**
- `parser.h` / `parser.cpp` - Main parser entry point and binary operator precedence handling
- `parse_core.cpp` - Core utilities, error handling, and token checking functions

**Parser Components:**
- `parse_constants.cpp` - Constant table management and jump list handling with iterator support
- `parse_regalloc.cpp` - Register allocation, bytecode emission, and expression discharge logic
- `parse_scope.cpp` - Scope management, variable resolution, and upvalue tracking
- `parse_expr.cpp` - Expression parsing (primary, suffix, constructor, function calls)
- `parse_operators.cpp` - Operator implementation (arithmetic folding, bitwise, unary, logical)
- `parse_stmt.cpp` - Statement parsing (assignments, control flow, declarations)

## Key Implementation Patterns

**Type Safety with Concepts:**
```cpp
// Use concepts for compile-time validation
template<BytecodeOpcode Op>
static inline BCPOS bcemit_ABC(FuncState *, Op, BCREG, BCREG, BCREG);
```

**RAII for Resource Management:**
```cpp
// Automatic scope cleanup
FuncScope bl;
ScopeGuard scope_guard(fs, &bl, FuncScopeFlag::None);
// ... parse statements ...
// Automatic cleanup on scope exit
```

**Constexpr Expression Builders:**
```cpp
// Compile-time expression construction
auto expr = make_nil_expr();
auto bool_expr = make_bool_expr(true);
```

**Modern Container Usage:**
```cpp
// Prefer std::span for array access
auto uvmap_range = std::span(fs->uvmap.data(), fs->nuv);
for (auto uv_idx : uvmap_range) { ... }

// Use std::string_view for string parameters
static GCstr * keepstr(std::string_view str);
```

## Quick Reference

**File Locations:**
- Parser source: `src/fluid/luajit-2.1/src/parser/`
- Bytecode reference: `src/fluid/luajit-2.1/BYTECODE.md` (instruction matrix, control-flow semantics)
- Fluid tests: `src/fluid/tests/`
- Troubleshooting Guide: `src/fluid/luajit-2.1/src/parser/TROUBLESHOOTING.md`

**Common Tasks:**
- Adding an operator: See "Implementing Binary Logical Operators" and "Single-Character Token Recognition" sections
- Register allocation: See "Troubleshooting Register Allocation" and "Register Management" sections
- Understanding concepts: See `parse_concepts.h` for type constraints
- Expression builders: See `parse_types.h` for constexpr factories

**Key Headers:**
- `parse_types.h` - Core types and expression builders
- `parse_concepts.h` - C++20 type constraints
- `parse_raii.h` - RAII guards
- `parse_internal.h` - Function declarations and templates
