// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#pragma once

#include "parser/parse_types.h"

class FuncState;

class JumpHandle {
public:
   JumpHandle();
   explicit JumpHandle(FuncState* State);
   JumpHandle(FuncState* State, BCPos Head);

   [[nodiscard]] bool empty() const;
   void append(BCPos Other);
   void append(const JumpHandle& Other);
   void patch_here() const;
   void patch_to(BCPos Target) const;
   void patch_head(BCPos Destination) const;
   [[nodiscard]] BCPos head() const;
   [[nodiscard]] FuncState* state() const;

private:
   FuncState* func_state;
   BCPos list_head;
};

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

