// Expression value wrapper for parser emission pipeline.
// Copyright (C) 2025 Paul Manias

#include "parser/parse_value.h"

#include "parser/parse_control_flow.h"
#include "parser/parse_internal.h"
#include "parser/parse_regalloc.h"

ExpressionValue::ExpressionValue() : func_state(nullptr), descriptor(make_nil_expr())
{
}

ExpressionValue::ExpressionValue(const ExpDesc& Descriptor)
   : func_state(nullptr), descriptor(Descriptor)
{
}

ExpressionValue::ExpressionValue(FuncState* State, const ExpDesc& Descriptor)
   : func_state(State), descriptor(Descriptor)
{
}

ExpressionValue ExpressionValue::make_nil(FuncState* State)
{
   return ExpressionValue(State, make_nil_expr());
}

ExpressionValue ExpressionValue::make_bool(FuncState* State, bool Value)
{
   return ExpressionValue(State, make_bool_expr(Value));
}

ExpressionValue ExpressionValue::make_number(FuncState* State, lua_Number Value)
{
   return ExpressionValue(State, make_num_expr(Value));
}

ExpressionValue ExpressionValue::make_string(FuncState* State, GCstr* Value)
{
   return ExpressionValue(State, make_interned_string_expr(Value));
}

bool ExpressionValue::has_jump() const
{
   return this->descriptor.t != this->descriptor.f;
}

bool ExpressionValue::is_constant() const
{
   return expr_isk(&this->descriptor);
}

bool ExpressionValue::is_constant_nojump() const
{
   return expr_isk_nojump(&this->descriptor);
}

bool ExpressionValue::is_number_constant() const
{
   return expr_isnumk(&this->descriptor);
}

bool ExpressionValue::is_number_constant_nojump() const
{
   return expr_isnumk_nojump(&this->descriptor);
}

bool ExpressionValue::is_string_constant() const
{
   return expr_isstrk(&this->descriptor);
}

bool ExpressionValue::has_flag(ExprFlag Flag) const
{
   return (this->descriptor.flags & Flag) != ExprFlag::None;
}

void ExpressionValue::set_flag(ExprFlag Flag)
{
   this->descriptor.flags |= Flag;
}

void ExpressionValue::clear_flag(ExprFlag Flag)
{
   this->descriptor.flags &= ~Flag;
}

bool ExpressionValue::consume_flag(ExprFlag Flag)
{
   if ((this->descriptor.flags & Flag) != ExprFlag::None) {
      this->descriptor.flags &= ~Flag;
      return true;
   }
   return false;
}

ControlFlowEdge ExpressionValue::true_jumps(ControlFlowGraph& Graph) const
{
   return Graph.make_true_edge(this->descriptor.t);
}

ControlFlowEdge ExpressionValue::false_jumps(ControlFlowGraph& Graph) const
{
   return Graph.make_false_edge(this->descriptor.f);
}

void ExpressionValue::set_true_jumps(const ControlFlowEdge& Edge)
{
   this->descriptor.t = Edge.head();
}

void ExpressionValue::set_false_jumps(const ControlFlowEdge& Edge)
{
   this->descriptor.f = Edge.head();
}

void ExpressionValue::set_jump_heads(BCPos TrueHead, BCPos FalseHead)
{
   this->descriptor.t = TrueHead;
   this->descriptor.f = FalseHead;
}

BCReg ExpressionValue::to_reg(RegisterAllocator& Allocator, BCReg Slot)
{
   expr_toreg(Allocator.state(), &this->descriptor, Slot);
   return this->descriptor.u.s.info;
}

BCReg ExpressionValue::to_any_reg(RegisterAllocator& Allocator)
{
   return expr_toanyreg(Allocator.state(), &this->descriptor);
}

BCReg ExpressionValue::to_next_reg(RegisterAllocator& Allocator)
{
   expr_tonextreg(Allocator.state(), &this->descriptor);
   return this->descriptor.u.s.info;
}

void ExpressionValue::discharge()
{
   expr_discharge(this->func_state, &this->descriptor);
}

void ExpressionValue::discharge(ControlFlowGraph& Graph)
{
   expr_discharge(Graph.state(), &this->descriptor);
}

void ExpressionValue::release(RegisterAllocator& Allocator)
{
   Allocator.release_expression(&this->descriptor);
}

ExpDesc& ExpressionValue::legacy()
{
   return this->descriptor;
}

const ExpDesc& ExpressionValue::legacy() const
{
   return this->descriptor;
}

FuncState* ExpressionValue::state() const
{
   return this->func_state;
}

ExpressionValue::operator ExpDesc&()
{
   return this->descriptor;
}

ExpressionValue::operator const ExpDesc&() const
{
   return this->descriptor;
}

// Phase 1.1 helper methods for gradual migration

void ExpressionValue::to_val()
{
   expr_toval(this->func_state, &this->descriptor);
}

void ExpressionValue::discharge_nobranch(RegisterAllocator& Allocator, BCReg Reg)
{
   expr_toreg_nobranch(Allocator.state(), &this->descriptor, Reg);
}

void ExpressionValue::store_to(RegisterAllocator& Allocator, ExpressionValue& Target)
{
   bcemit_store(Allocator.state(), &Target.descriptor, &this->descriptor);
}

BCReg ExpressionValue::discharge_to_any_reg(RegisterAllocator& Allocator)
{
   this->discharge();
   return this->to_any_reg(Allocator);
}

