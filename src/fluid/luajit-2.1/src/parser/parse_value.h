// Expression value wrapper for parser emission pipeline.
// Copyright (C) 2025 Paul Manias

#pragma once

#include "parser/parse_types.h"

class RegisterAllocator;
class ControlFlowGraph;
class ControlFlowEdge;

class ExpressionValue {
public:
   ExpressionValue();
   ExpressionValue(const ExpDesc& Descriptor);
   ExpressionValue(FuncState* State, const ExpDesc& Descriptor);

   ExpressionValue(const ExpressionValue&) = default;
   ExpressionValue& operator=(const ExpressionValue&) = default;
   ExpressionValue(ExpressionValue&&) noexcept = default;
   ExpressionValue& operator=(ExpressionValue&&) noexcept = default;

   static ExpressionValue make_nil(FuncState* State);
   static ExpressionValue make_bool(FuncState* State, bool Value);
   static ExpressionValue make_number(FuncState* State, lua_Number Value);
   static ExpressionValue make_string(FuncState* State, GCstr* Value);

   [[nodiscard]] bool has_jump() const;
   [[nodiscard]] bool is_constant() const;
   [[nodiscard]] bool is_constant_nojump() const;
   [[nodiscard]] bool is_number_constant() const;
   [[nodiscard]] bool is_number_constant_nojump() const;
   [[nodiscard]] bool is_string_constant() const;

   [[nodiscard]] bool has_flag(ExprFlag Flag) const;
   void set_flag(ExprFlag Flag);
   void clear_flag(ExprFlag Flag);
   [[nodiscard]] bool consume_flag(ExprFlag Flag);

   [[nodiscard]] ControlFlowEdge true_jumps(ControlFlowGraph& Graph) const;
   [[nodiscard]] ControlFlowEdge false_jumps(ControlFlowGraph& Graph) const;
   void set_true_jumps(const ControlFlowEdge& Edge);
   void set_false_jumps(const ControlFlowEdge& Edge);
   void set_jump_heads(BCPos TrueHead, BCPos FalseHead);

   [[nodiscard]] BCReg to_reg(RegisterAllocator& Allocator, BCReg Slot);
   [[nodiscard]] BCReg to_any_reg(RegisterAllocator& Allocator);
   [[nodiscard]] BCReg to_next_reg(RegisterAllocator& Allocator);
   void discharge();
   void discharge(ControlFlowGraph& Graph);
   void release(RegisterAllocator& Allocator);

   [[nodiscard]] ExpDesc& legacy();
   [[nodiscard]] const ExpDesc& legacy() const;
   [[nodiscard]] FuncState* state() const;

   operator ExpDesc&();
   operator const ExpDesc&() const;

private:
   FuncState* func_state;
   ExpDesc descriptor;
};

