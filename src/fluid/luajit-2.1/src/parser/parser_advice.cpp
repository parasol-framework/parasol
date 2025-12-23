// Parser advice system for Fluid.
// Copyright (C) 2025 Paul Manias

#include "parser_advice.h"

#include <format>
#include <cstdio>

//********************************************************************************************************************

const char* category_name(AdviceCategory Cat)
{
   switch (Cat) {
      case AdviceCategory::TypeSafety:      return "type-safety";
      case AdviceCategory::Performance:     return "performance";
      case AdviceCategory::CodeQuality:     return "code-quality";
      case AdviceCategory::BestPractice:    return "best-practice";
      case AdviceCategory::Style:           return "style";
      case AdviceCategory::ParasolSpecific: return "parasol";
   }
   return "unknown";
}

//********************************************************************************************************************
// Format advice message for output.

std::string ParserAdvice::to_string(std::string_view Filename) const
{
   SourceSpan span = token.span();
   if (Filename.starts_with("=")) Filename.remove_prefix(1);
   else if (Filename.starts_with("@")) Filename.remove_prefix(1);
   return std::format("[ADVICE] {}:{}:{}: {}: {}", Filename, span.line, span.column, category_name(category), message);
}

//********************************************************************************************************************
// Emit an advice message if it passes the priority filter.

void AdviceEmitter::emit(const ParserAdvice &Advice, std::string_view Filename)
{
   if (should_emit(Advice.priority)) {
      advice.push_back(Advice);
      printf("%s\n", Advice.to_string(Filename).c_str());
   }
}

//********************************************************************************************************************
// Convenience method for emitting advice with all fields specified.

void AdviceEmitter::emit(uint8_t Priority, AdviceCategory Category, std::string Message, const Token &Location,
   std::string_view Filename)
{
   ParserAdvice advice_item {
      .priority = Priority,
      .category = Category,
      .message = std::move(Message),
      .token = Location
   };
   emit(advice_item, Filename);
}
