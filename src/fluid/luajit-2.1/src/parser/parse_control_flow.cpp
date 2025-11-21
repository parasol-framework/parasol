// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#include "parser/parse_control_flow.h"

#include "parser/parse_internal.h"

JumpHandle::JumpHandle() : func_state(nullptr), list_head(NO_JMP)
{
}

JumpHandle::JumpHandle(FuncState* State)
   : func_state(State), list_head(NO_JMP)
{
}

JumpHandle::JumpHandle(FuncState* State, BCPos Head) : func_state(State), list_head(Head)
{
}

bool JumpHandle::empty() const
{
   return this->list_head IS NO_JMP;
}

void JumpHandle::append(BCPos Other)
{
   if (not this->func_state) return;
   this->list_head = JumpListView(this->func_state, this->list_head).append(Other);
}

void JumpHandle::append(const JumpHandle& Other)
{
   this->append(Other.list_head);
}

void JumpHandle::patch_here() const
{
   if (this->func_state) this->patch_to(this->func_state->pc);
}

void JumpHandle::patch_to(BCPos Target) const
{
   if (this->func_state and not this->empty()) {
      JumpListView(this->func_state, this->list_head).patch_to(Target);
   }
}

void JumpHandle::patch_head(BCPos Destination) const
{
   if (this->func_state and not this->empty()) {
      JumpListView(this->func_state, this->list_head).patch_head(Destination);
   }
}

BCPos JumpHandle::head() const
{
   return this->list_head;
}

FuncState* JumpHandle::state() const
{
   return this->func_state;
}

ControlFlowGraph::ControlFlowGraph() : func_state(nullptr)
{
}

ControlFlowGraph::ControlFlowGraph(FuncState* State) : func_state(State)
{
}

bool ControlFlowGraph::valid() const
{
   return this->func_state != nullptr;
}

FuncState* ControlFlowGraph::state() const
{
   return this->func_state;
}

JumpHandle ControlFlowGraph::make_handle(BCPos Head) const
{
   return JumpHandle(this->func_state, Head);
}

