// Parser advice system for Fluid.
// Copyright (C) 2025 Paul Manias

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "token_types.h"

// Categories of advice messages.

enum class AdviceCategory : uint8_t {
   TypeSafety,
   Performance,
   CodeQuality,
   BestPractice,
   Style,
   ParasolSpecific
};

// Get the display name for an advice category.

[[nodiscard]] const char* category_name(AdviceCategory Cat);

//********************************************************************************************************************
// Individual advice message with location information.

struct ParserAdvice {
   uint8_t priority;           // 1 = critical, 2 = medium, 3 = low
   AdviceCategory category;
   std::string message;
   Token token;                // Location information

   [[nodiscard]] std::string to_string(std::string_view Filename) const;
};

//********************************************************************************************************************
// Collects and filters advice messages based on the configured level.

class AdviceEmitter {
public:
   explicit AdviceEmitter(uint8_t Level) : level(Level) {}

   // Returns true if advice at given priority should be emitted.
   [[nodiscard]] bool should_emit(uint8_t Priority) const {
      #ifdef INCLUDE_ADVICE
         return level > 0 and Priority <= level;
      #else
         return false;
      #endif
   }

   // Emit an advice message if it passes the priority filter.
   void emit(const ParserAdvice &Advice, std::string_view Filename = {});

   // Convenience method for emitting advice with all fields specified.
   void emit(uint8_t Priority, AdviceCategory, std::string, const Token &, std::string_view = {});

   [[nodiscard]] const std::vector<ParserAdvice>& entries() const { return advice; }
   [[nodiscard]] bool has_advice() const { return not advice.empty(); }
   [[nodiscard]] size_t count() const { return advice.size(); }

private:
   uint8_t level;
   std::vector<ParserAdvice> advice;
};
