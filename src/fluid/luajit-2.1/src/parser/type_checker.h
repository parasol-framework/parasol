#pragma once

#include <optional>
#include <string>
#include <vector>

#include "parser/parser_diagnostics.h"
#include "ast_nodes.h"

struct InferredType {
   FluidType primary = FluidType::Any;
   bool is_constant = false;
   bool is_nullable = false;
   bool is_fixed = false;  // Type is locked, cannot change

   [[nodiscard]] bool matches(FluidType Expected) const
   {
      if (Expected IS FluidType::Any) return true;
      if (this->primary IS FluidType::Any) return true;
      return this->primary IS Expected;
   }
};

struct TypeDiagnostic {
   SourceSpan location{};
   std::string message;
   FluidType expected = FluidType::Any;
   FluidType actual = FluidType::Any;
   ParserErrorCode code = ParserErrorCode::TypeMismatchArgument;
};

// Context for tracking function return type validation during type analysis
struct FunctionContext {
   const FunctionExprPayload* function = nullptr;  // The function being analysed
   FunctionReturnTypes expected_returns{};         // Declared or inferred return types
   bool return_type_inferred = false;              // True once first return statement sets types
   GCstr* function_name = nullptr;                 // Function name (for recursive detection)
};

class TypeCheckScope {
public:
   void declare_parameter(GCstr* Name, FluidType Type);
   void declare_local(GCstr* Name, const InferredType& Type);
   void declare_function(GCstr* Name, const FunctionExprPayload* Function);
   void fix_local_type(GCstr* Name, FluidType Type);

   [[nodiscard]] std::optional<FluidType> lookup_parameter_type(GCstr* Name) const;
   [[nodiscard]] std::optional<InferredType> lookup_local_type(GCstr* Name) const;
   [[nodiscard]] const FunctionExprPayload* lookup_function(GCstr* Name) const;

private:
   struct VariableInfo {
      GCstr* name = nullptr;
      InferredType type{};
      bool is_parameter = false;
      const FunctionExprPayload* function = nullptr;
   };

   std::vector<VariableInfo> variables_{};
};
