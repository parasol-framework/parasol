#include "parser/type_checker.h"

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

void TypeCheckScope::declare_local(GCstr* Name, const InferredType& Type)
{
   VariableInfo info;
   info.name = Name;
   info.type = Type;
   info.is_parameter = false;
   this->variables_.push_back(info);
}

void TypeCheckScope::declare_function(GCstr* Name, const FunctionExprPayload* Function)
{
   if (not Function) return;

   VariableInfo info;
   info.name = Name;
   info.type.primary = FluidType::Func;
   info.is_parameter = false;
   info.function = Function;
   this->variables_.push_back(info);
}

std::optional<FluidType> TypeCheckScope::lookup_parameter_type(GCstr* Name) const
{
   for (auto it = this->variables_.rbegin(); it != this->variables_.rend(); ++it) {
      if (it->name IS Name and it->is_parameter) return it->type.primary;
   }
   return std::nullopt;
}

std::optional<InferredType> TypeCheckScope::lookup_local_type(GCstr* Name) const
{
   for (auto it = this->variables_.rbegin(); it != this->variables_.rend(); ++it) {
      if (it->name IS Name) return it->type;
   }
   return std::nullopt;
}

const FunctionExprPayload* TypeCheckScope::lookup_function(GCstr* Name) const
{
   for (auto it = this->variables_.rbegin(); it != this->variables_.rend(); ++it) {
      if (it->name IS Name and it->function) return it->function;
   }
   return nullptr;
}
