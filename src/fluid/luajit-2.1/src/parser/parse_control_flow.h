// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#pragma once

#include <vector>

#include "parser/parse_types.h"

class FuncState;

enum class ControlFlowEdgeKind {
   Unconditional,
   True,
   False,
   Break,
   Continue,
};

class ControlFlowGraph;

class ControlFlowEdge {
public:
   ControlFlowEdge();
   ControlFlowEdge(ControlFlowGraph* Graph, size_t Index);

   [[nodiscard]] bool empty() const;
   [[nodiscard]] bool valid() const;
   [[nodiscard]] ControlFlowEdgeKind kind() const;
   [[nodiscard]] BCPos head() const;
   [[nodiscard]] FuncState* state() const;

   void append(BCPos Other) const;
   void append(const ControlFlowEdge& Other) const;
   void patch_here() const;
   void patch_to(BCPos Target) const;
   void patch_head(BCPos Destination) const;
   void patch_with_value(BCPos ValueTarget, BCReg Register, BCPos DefaultTarget) const;
   [[nodiscard]] bool produces_values() const;
   void drop_values() const;

private:
   friend class ControlFlowGraph;

   ControlFlowGraph* graph;
   size_t index;
};

class ControlFlowGraph {
public:
   ControlFlowGraph();
   explicit ControlFlowGraph(FuncState* State);

   void reset(FuncState* State);

   [[nodiscard]] bool valid() const;
   [[nodiscard]] FuncState* state() const;

   [[nodiscard]] ControlFlowEdge make_edge(ControlFlowEdgeKind Kind, BCPos Head = NO_JMP);
   [[nodiscard]] ControlFlowEdge make_unconditional(BCPos Head = NO_JMP);
   [[nodiscard]] ControlFlowEdge make_true_edge(BCPos Head = NO_JMP);
   [[nodiscard]] ControlFlowEdge make_false_edge(BCPos Head = NO_JMP);
   [[nodiscard]] ControlFlowEdge make_break_edge(BCPos Head = NO_JMP);
   [[nodiscard]] ControlFlowEdge make_continue_edge(BCPos Head = NO_JMP);

   void finalize() const;

private:
   friend class ControlFlowEdge;

   struct EdgeEntry {
      BCPos head = NO_JMP;
      ControlFlowEdgeKind kind = ControlFlowEdgeKind::Unconditional;
      bool resolved = false;
   };

   [[nodiscard]] BCPos edge_head(size_t Index) const;
   [[nodiscard]] ControlFlowEdgeKind edge_kind(size_t Index) const;
   [[nodiscard]] bool edge_resolved(size_t Index) const;
   void set_edge_head(size_t Index, BCPos Head);
   void mark_resolved(size_t Index);
   void append_edge(size_t Index, BCPos Head);
   void append_edge(size_t Index, const ControlFlowEdge& Other);
   void patch_edge(size_t Index, BCPos Target);
   void patch_edge_head(size_t Index, BCPos Destination);
   void patch_edge_with_value(size_t Index, BCPos ValueTarget, BCReg Register, BCPos DefaultTarget);
   [[nodiscard]] bool edge_produces_values(size_t Index) const;
   void drop_edge_values(size_t Index);

   [[nodiscard]] static BCPos next_in_chain(FuncState* State, BCPos Position);
   [[nodiscard]] bool patch_test_register(BCPos Position, BCReg Register) const;
   void patch_instruction(BCPos Position, BCPos Destination) const;

   FuncState* func_state;
   std::vector<EdgeEntry> edges;
};

