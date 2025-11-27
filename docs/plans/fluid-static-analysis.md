# Fluid Static Analysis: Parameter Type Guards

This document outlines the implementation plan for adding compile-time static analysis of parameter type guards
in Fluid function declarations.

## Overview

**Syntax:** `function name(param1, param2:type, param3:type)`

**Examples:**
```lua
function process(Path:str, Count:num, Options:table)
   -- Path must be a string
   -- Count must be a number
   -- Options must be a table
end

function mixed(Untyped, Typed:bool)
   -- Untyped accepts any value
   -- Typed must be a boolean
end
```

**Scope:** This plan covers compile-time static analysis only. Runtime checks (Option B) and JIT optimisation
(Option D) are deferred to future work.

---

## Type System

### Supported Type Names

| Shorthand | Full Name   | Lua Type        | Notes                              |
|-----------|-------------|-----------------|-----------------------------------|
| `num`     | number      | `number`        | Integers and floats               |
| `str`     | string      | `string`        | String values                     |
| `bool`    | boolean     | `boolean`       | `true` or `false`                 |
| `table`   | table       | `table`         | Tables (arrays and dictionaries)  |
| `func`    | function    | `function`      | Callable functions                |
| `nil`     | nil         | `nil`           | Nil value (rarely used for params)|
| `any`     | any         | (all types)     | Explicit "no constraint"          |
| `thread`  | thread      | `thread`        | Coroutines                        |
| `cdata`   | cdata       | `cdata`         | FFI cdata (if FFI enabled)        |
| `obj`     | object      | `userdata`      | Parasol objects (userdata)        |

### Type Representation

```cpp
enum class FluidType : uint8_t {
   Any = 0,     // No type constraint (default for untyped parameters)
   Nil,
   Bool,
   Num,
   Str,
   Table,
   Func,
   Thread,
   CData,
   Object       // Parasol userdata
};
```

---

## Implementation Phases

### Phase 1: Lexer Support

**File:** `src/fluid/luajit-2.1/src/parser/lexer.cpp`

The colon character `:` is already a valid token. The lexer requires no changes for basic syntax support.

**Consideration:** The parser must disambiguate between:
- Method syntax: `obj:method()`
- Type annotation: `param:type`

This disambiguation is handled contextually in the parser (inside parameter lists, `:` followed by an
identifier is a type annotation).

---

### Phase 2: AST Extensions

**File:** `src/fluid/luajit-2.1/src/parser/ast_nodes.h`

#### 2.1 Add FluidType Enum

```cpp
// Parameter type annotation for static analysis
enum class FluidType : uint8_t {
   Any = 0,     // No type constraint (default)
   Nil,
   Bool,
   Num,
   Str,
   Table,
   Func,
   Thread,
   CData,
   Object       // Parasol userdata
};

// Convert type name string to FluidType
[[nodiscard]] FluidType parse_type_name(std::string_view Name);

// Convert FluidType to display string
[[nodiscard]] std::string_view type_name(FluidType Type);
```

#### 2.2 Extend FunctionParameter

```cpp
struct FunctionParameter {
   Identifier name;
   FluidType type = FluidType::Any;  // NEW: Optional type constraint
   bool is_self = false;
};
```

---

### Phase 3: Parser Changes

**File:** `src/fluid/luajit-2.1/src/parser/ast_builder.cpp`

#### 3.1 Modify `parse_parameter_list()`

Current implementation (lines 927-951):

```cpp
ParserResult<AstBuilder::ParameterListResult> AstBuilder::parse_parameter_list(bool allow_optional)
{
   ParameterListResult result;
   // ... existing code ...
   do {
      if (this->ctx.check(TokenKind::Dots)) {
         this->ctx.tokens().advance();
         result.is_vararg = true;
         break;
      }
      auto name = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not name.ok()) return ParserResult<ParameterListResult>::failure(name.error_ref());

      FunctionParameter param;
      param.name = make_identifier(name.value_ref());
      result.parameters.push_back(param);
   } while (this->ctx.match(TokenKind::Comma).ok());
   // ...
}
```

Modified implementation:

```cpp
ParserResult<AstBuilder::ParameterListResult> AstBuilder::parse_parameter_list(bool allow_optional)
{
   ParameterListResult result;
   // ... existing code ...
   do {
      if (this->ctx.check(TokenKind::Dots)) {
         this->ctx.tokens().advance();
         result.is_vararg = true;
         break;
      }
      auto name = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
      if (not name.ok()) return ParserResult<ParameterListResult>::failure(name.error_ref());

      FunctionParameter param;
      param.name = make_identifier(name.value_ref());

      // NEW: Check for type annotation
      if (this->ctx.match(TokenKind::Colon).ok()) {
         auto type_name = this->ctx.expect_identifier(ParserErrorCode::ExpectedTypeName);
         if (not type_name.ok()) {
            return ParserResult<ParameterListResult>::failure(type_name.error_ref());
         }
         param.type = parse_type_name(type_name.value_ref().symbol);
         if (param.type IS FluidType::Any) {
            // Unknown type name - emit warning or error
            this->ctx.report_error(ParserErrorCode::UnknownTypeName, type_name.value_ref().span);
         }
      }

      result.parameters.push_back(param);
   } while (this->ctx.match(TokenKind::Comma).ok());
   // ...
}
```

#### 3.2 Add Type Name Parser

**File:** `src/fluid/luajit-2.1/src/parser/ast_nodes.cpp`

```cpp
FluidType parse_type_name(GCstr* Name)
{
   if (not Name) return FluidType::Any;

   std::string_view sv(strdata(Name), Name->len);

   // Shorthand names (preferred)
   if (sv IS "num")    return FluidType::Num;
   if (sv IS "str")    return FluidType::Str;
   if (sv IS "bool")   return FluidType::Bool;
   if (sv IS "table")  return FluidType::Table;
   if (sv IS "func")   return FluidType::Func;
   if (sv IS "nil")    return FluidType::Nil;
   if (sv IS "any")    return FluidType::Any;
   if (sv IS "thread") return FluidType::Thread;
   if (sv IS "cdata")  return FluidType::CData;
   if (sv IS "obj")    return FluidType::Object;

   // Full names (also accepted)
   if (sv IS "number")   return FluidType::Num;
   if (sv IS "string")   return FluidType::Str;
   if (sv IS "boolean")  return FluidType::Bool;
   if (sv IS "function") return FluidType::Func;
   if (sv IS "object")   return FluidType::Object;
   if (sv IS "userdata") return FluidType::Object;

   return FluidType::Any;  // Unknown type
}

std::string_view type_name(FluidType Type)
{
   switch (Type) {
      case FluidType::Any:    return "any";
      case FluidType::Nil:    return "nil";
      case FluidType::Bool:   return "bool";
      case FluidType::Num:    return "num";
      case FluidType::Str:    return "str";
      case FluidType::Table:  return "table";
      case FluidType::Func:   return "func";
      case FluidType::Thread: return "thread";
      case FluidType::CData:  return "cdata";
      case FluidType::Object: return "obj";
   }
   return "unknown";
}
```

---

### Phase 4: Static Analysis Infrastructure

**New File:** `src/fluid/luajit-2.1/src/parser/type_checker.h`

```cpp
#pragma once

#include "ast_nodes.h"
#include <optional>
#include <vector>

// Represents the inferred type of an expression
struct InferredType {
   FluidType primary = FluidType::Any;
   bool is_constant = false;
   bool is_nullable = false;  // Could be nil

   [[nodiscard]] bool matches(FluidType Expected) const {
      if (Expected IS FluidType::Any) return true;
      if (this->primary IS FluidType::Any) return true;  // Unknown type, assume OK
      return this->primary IS Expected;
   }
};

// Type mismatch diagnostic
struct TypeDiagnostic {
   SourceSpan location;
   std::string message;
   FluidType expected;
   FluidType actual;
};

// Type checking context for a function scope
class TypeCheckScope {
public:
   void declare_parameter(GCstr* Name, FluidType Type);
   void declare_local(GCstr* Name, InferredType Type);

   [[nodiscard]] std::optional<FluidType> lookup_parameter_type(GCstr* Name) const;
   [[nodiscard]] std::optional<InferredType> lookup_local_type(GCstr* Name) const;

private:
   struct VariableInfo {
      GCstr* name;
      InferredType type;
      bool is_parameter;
   };
   std::vector<VariableInfo> variables_;
};
```

**New File:** `src/fluid/luajit-2.1/src/parser/type_checker.cpp`

```cpp
#include "type_checker.h"

void TypeCheckScope::declare_parameter(GCstr* Name, FluidType Type)
{
   VariableInfo info;
   info.name = Name;
   info.type.primary = Type;
   info.type.is_constant = false;
   info.type.is_nullable = false;
   info.is_parameter = true;
   this->variables_.push_back(info);
}

void TypeCheckScope::declare_local(GCstr* Name, InferredType Type)
{
   VariableInfo info;
   info.name = Name;
   info.type = Type;
   info.is_parameter = false;
   this->variables_.push_back(info);
}

std::optional<FluidType> TypeCheckScope::lookup_parameter_type(GCstr* Name) const
{
   for (auto it = this->variables_.rbegin(); it != this->variables_.rend(); ++it) {
      if (it->name IS Name and it->is_parameter) {
         return it->type.primary;
      }
   }
   return std::nullopt;
}

std::optional<InferredType> TypeCheckScope::lookup_local_type(GCstr* Name) const
{
   for (auto it = this->variables_.rbegin(); it != this->variables_.rend(); ++it) {
      if (it->name IS Name) {
         return it->type;
      }
   }
   return std::nullopt;
}
```

---

### Phase 5: Type Inference Engine

**File:** `src/fluid/luajit-2.1/src/parser/type_checker.cpp` (continued)

#### 5.1 Expression Type Inference

```cpp
// Infer the type of a literal expression
InferredType infer_literal_type(const LiteralValue& Literal)
{
   InferredType result;
   result.is_constant = true;

   switch (Literal.kind) {
      case LiteralKind::Nil:
         result.primary = FluidType::Nil;
         result.is_nullable = true;
         break;
      case LiteralKind::Boolean:
         result.primary = FluidType::Bool;
         break;
      case LiteralKind::Number:
         result.primary = FluidType::Num;
         break;
      case LiteralKind::String:
         result.primary = FluidType::Str;
         break;
      case LiteralKind::CData:
         result.primary = FluidType::CData;
         break;
   }
   return result;
}

// Infer the type of an expression node
InferredType infer_expression_type(const ExprNode& Expr, const TypeCheckScope& Scope)
{
   InferredType result;

   switch (Expr.kind) {
      case AstNodeKind::LiteralExpr: {
         auto* payload = std::get_if<LiteralExprPayload>(&Expr.data);
         if (payload) return infer_literal_type(payload->value);
         break;
      }

      case AstNodeKind::IdentifierExpr: {
         auto* payload = std::get_if<IdentifierExprPayload>(&Expr.data);
         if (payload) {
            auto type = Scope.lookup_local_type(payload->name.identifier.symbol);
            if (type) return *type;
         }
         break;
      }

      case AstNodeKind::TableExpr:
         result.primary = FluidType::Table;
         break;

      case AstNodeKind::FunctionExpr:
         result.primary = FluidType::Func;
         break;

      // For complex expressions (calls, binary ops), return Any
      // Future work: more sophisticated inference
      default:
         result.primary = FluidType::Any;
         break;
   }

   return result;
}
```

---

### Phase 6: Static Analysis Pass

**File:** `src/fluid/luajit-2.1/src/parser/type_analysis.cpp`

#### 6.1 Function Call Argument Checking

The primary use case for static analysis is checking function call arguments against declared parameter types.

```cpp
class TypeAnalyser {
public:
   TypeAnalyser(ParserContext& Context) : ctx_(Context) {}

   void analyse_call(const CallExprPayload& Call, const FunctionExprPayload* Target);
   void analyse_function(const FunctionExprPayload& Function);
   void analyse_statement(const StmtNode& Statement);

   [[nodiscard]] const std::vector<TypeDiagnostic>& diagnostics() const { return diagnostics_; }

private:
   void check_argument_type(const ExprNode& Argument, FluidType Expected, size_t Index);
   void report_mismatch(SourceSpan Location, FluidType Expected, FluidType Actual, size_t ArgIndex);

   ParserContext& ctx_;
   TypeCheckScope current_scope_;
   std::vector<TypeDiagnostic> diagnostics_;
};

void TypeAnalyser::check_argument_type(const ExprNode& Argument, FluidType Expected, size_t Index)
{
   if (Expected IS FluidType::Any) return;  // No constraint

   InferredType actual = infer_expression_type(Argument, this->current_scope_);

   if (not actual.matches(Expected)) {
      this->report_mismatch(Argument.span, Expected, actual.primary, Index);
   }
}

void TypeAnalyser::report_mismatch(SourceSpan Location, FluidType Expected, FluidType Actual, size_t ArgIndex)
{
   TypeDiagnostic diag;
   diag.location = Location;
   diag.expected = Expected;
   diag.actual = Actual;
   diag.message = std::format("argument {} expects type '{}', got '{}'",
                               ArgIndex + 1, type_name(Expected), type_name(Actual));
   this->diagnostics_.push_back(diag);
}
```

#### 6.2 Integration with AST Pipeline

**File:** `src/fluid/luajit-2.1/src/parser/parser.cpp`

Add type analysis as an optional pass in `run_ast_pipeline()`:

```cpp
void run_ast_pipeline(ParserContext& Context, ParserProfiler& Profiler)
{
   // ... existing parsing phases ...

   // NEW: Type analysis pass (after AST is built, before IR emission)
   if (Context.config().enable_type_analysis) {
      Profiler.start_phase("type_analysis");
      TypeAnalyser analyser(Context);
      analyser.analyse_module(Context.ast_root());

      for (const auto& diag : analyser.diagnostics()) {
         Context.report_warning(diag.location, diag.message);
      }
      Profiler.end_phase("type_analysis");
   }

   // ... IR emission ...
}
```

---

### Phase 7: Error Reporting

**File:** `src/fluid/luajit-2.1/src/parser/parser_errors.h`

Add new error codes:

```cpp
enum class ParserErrorCode : uint16_t {
   // ... existing codes ...

   // Type annotation errors
   ExpectedTypeName,
   UnknownTypeName,

   // Type mismatch warnings
   TypeMismatchArgument,
   TypeMismatchAssignment,
   TypeMismatchReturn
};
```

Error messages:

```cpp
inline constexpr std::array TYPE_ERROR_MESSAGES = {
   "expected type name after ':'",
   "unknown type name '{}'; expected one of: num, str, bool, table, func, nil, any",
   "type mismatch: argument {} expects '{}', got '{}'",
   "type mismatch: cannot assign '{}' to variable of type '{}'",
   "type mismatch: function returns '{}', expected '{}'"
};
```

---

## Testing Strategy

### Unit Tests

**File:** `src/fluid/tests/test_type_annotations.fluid`

```lua
-- Test: Basic type annotation parsing
function test_basic_parsing()
   -- Should parse without error
   local function typed(A:num, B:str, C:table)
      return A
   end
   assert(typed(1, "hello", {}) is 1)
end

-- Test: Mixed typed and untyped parameters
function test_mixed_parameters()
   local function mixed(Untyped, Typed:bool)
      return Typed
   end
   assert(mixed("anything", true) is true)
end

-- Test: All supported types
function test_all_types()
   local function all_types(
      N:num,
      S:str,
      B:bool,
      T:table,
      F:func,
      A:any
   )
      return true
   end
   assert(all_types(1, "s", true, {}, function() end, nil))
end
```

### Static Analysis Tests

```lua
-- Test: Type mismatch detection (compile-time warning expected)
function test_type_mismatch_warning()
   local function expects_number(X:num)
      return X * 2
   end

   -- This should trigger a compile-time warning:
   -- expects_number("not a number")

   -- This should pass:
   expects_number(42)
end
```

---

## Configuration

### Parser Configuration

**File:** `src/fluid/luajit-2.1/src/parser/parser_context.h`

```cpp
struct ParserConfig {
   // ... existing fields ...

   bool enable_type_analysis = true;       // Enable static type checking
   bool type_errors_are_fatal = false;     // Treat type mismatches as errors vs warnings
   bool infer_local_types = true;          // Track types of local variables
};
```

### Command-Line Options

```
--type-check          Enable static type analysis (default: on)
--strict-types        Treat type mismatches as errors
--no-type-check       Disable static type analysis
```

---

## Future Work

### Deferred to Later Phases

1. **Runtime Checks (Option B):** Lazy parameter validation on first access
2. **JIT Integration (Option D):** Type hints for trace specialisation
3. **Return Type Annotations:** `function name():type`
4. **Generic Types:** `function map(Items:table<T>, Func:func):table<T>`
5. **Union Types:** `param:num|str`
6. **Nullable Types:** `param:str?` (equivalent to `str|nil`)
7. **Object Type Refinement:** `param:obj<Vector>` for specific Parasol classes

### Potential Enhancements

- IDE integration via Language Server Protocol
- Type stub files for external libraries
- Gradual typing with `@strict` annotation for files
- Type inference for return values

---

## Implementation Checklist

- [ ] **Phase 1:** Verify lexer handles `:` correctly in parameter context
- [ ] **Phase 2:** Add `FluidType` enum to `ast_nodes.h`
- [ ] **Phase 2:** Extend `FunctionParameter` with `type` field
- [ ] **Phase 3:** Implement `parse_type_name()` function
- [ ] **Phase 3:** Modify `parse_parameter_list()` to handle type annotations
- [ ] **Phase 4:** Create `type_checker.h` with `TypeCheckScope` class
- [ ] **Phase 5:** Implement expression type inference
- [ ] **Phase 6:** Create `TypeAnalyser` class
- [ ] **Phase 6:** Integrate type analysis into AST pipeline
- [ ] **Phase 7:** Add error codes and messages
- [ ] **Testing:** Create `test_type_annotations.fluid`
- [ ] **Testing:** Verify all type shorthand names work
- [ ] **Documentation:** Update Fluid reference manual

---

## References

- `src/fluid/luajit-2.1/src/parser/ast_nodes.h` - AST node definitions
- `src/fluid/luajit-2.1/src/parser/ast_builder.cpp` - Parameter list parsing
- `src/fluid/luajit-2.1/src/parser/lexer.h` - Token definitions
- `docs/plans/fluid-annotations-proposal.md` - Related annotation system
