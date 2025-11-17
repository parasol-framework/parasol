// Control-flow helper for managing pending jump lists.

#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "parser/parse_types.h"

enum class ControlFlowEdgeKind : uint8_t {
   TrueBranch,
   FalseBranch,
   Unconditional
};

struct ControlFlowEdgeHandle {
   std::size_t index = std::numeric_limits<std::size_t>::max();

   [[nodiscard]] bool is_valid() const
   {
      return this->index != std::numeric_limits<std::size_t>::max();
   }
};

struct ControlFlowEdge {
   ControlFlowEdgeKind kind = ControlFlowEdgeKind::Unconditional;
   BCPos head = NO_JMP;
};

class ControlFlowGraph {
public:
   explicit ControlFlowGraph(FuncState& FuncState);

   ControlFlowEdgeHandle add_edge(BCPos Head, ControlFlowEdgeKind Kind);
   void patch_edge(ControlFlowEdgeHandle Handle, BCPos Destination) const;
   void patch_edge_to_current(ControlFlowEdgeHandle Handle) const;
   void drop_edge(ControlFlowEdgeHandle Handle) const;
   void clear();

private:
   FuncState* func_state = nullptr;
   std::vector<ControlFlowEdge> edges;
};

