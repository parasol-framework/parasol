// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#include "parser/parse_control_flow.h"
#include "parser/parse_internal.h"

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

