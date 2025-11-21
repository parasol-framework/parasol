// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#pragma once

#include "parser/parse_types.h"

class FuncState;

// JumpHandle is defined in ir_emitter.h; do not duplicate here.
class ControlFlowGraph {
public:
   ControlFlowGraph();
   explicit ControlFlowGraph(FuncState* State);

   [[nodiscard]] bool valid() const;
   [[nodiscard]] FuncState* state() const;
   [[nodiscard]] JumpHandle make_handle(BCPos Head) const;

private:
   FuncState* func_state;
};

