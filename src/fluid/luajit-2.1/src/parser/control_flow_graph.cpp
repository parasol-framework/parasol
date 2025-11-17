#include "parser/control_flow_graph.h"

#include "parser/parse_internal.h"

ControlFlowGraph::ControlFlowGraph(FuncState& FuncState)
   : func_state(&FuncState)
{
}

ControlFlowEdgeHandle ControlFlowGraph::add_edge(BCPos Head, ControlFlowEdgeKind Kind)
{
   ControlFlowEdgeHandle handle;
   handle.index = this->edges.size();
   this->edges.push_back({Kind, Head});
   return handle;
}

void ControlFlowGraph::patch_edge(ControlFlowEdgeHandle Handle, BCPos Destination) const
{
   if (!Handle.is_valid() or Handle.index >= this->edges.size())
      return;
   const ControlFlowEdge& entry = this->edges[Handle.index];
   JumpListView(this->func_state, entry.head).patch_to(Destination);
}

void ControlFlowGraph::patch_edge_to_current(ControlFlowEdgeHandle Handle) const
{
   this->patch_edge(Handle, this->func_state->pc);
}

void ControlFlowGraph::drop_edge(ControlFlowEdgeHandle Handle) const
{
   if (!Handle.is_valid() or Handle.index >= this->edges.size())
      return;
   JumpListView(this->func_state, this->edges[Handle.index].head).patch_to_here();
}

void ControlFlowGraph::clear()
{
   this->edges.clear();
}

