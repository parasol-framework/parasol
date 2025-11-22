// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#include "parser/parse_control_flow.h"

#include "parser/parse_internal.h"

ControlFlowEdge::ControlFlowEdge() : graph(nullptr), index(0)
{
}

ControlFlowEdge::ControlFlowEdge(ControlFlowGraph* Graph, size_t Index) : graph(Graph), index(Index)
{
}

bool ControlFlowEdge::empty() const
{
   if (not this->graph) return true;
   return this->graph->edge_head(this->index) IS NO_JMP;
}

bool ControlFlowEdge::valid() const
{
   return this->graph != nullptr;
}

ControlFlowEdgeKind ControlFlowEdge::kind() const
{
   return this->graph ? this->graph->edge_kind(this->index) : ControlFlowEdgeKind::Unconditional;
}

BCPos ControlFlowEdge::head() const
{
   return this->graph ? this->graph->edge_head(this->index) : NO_JMP;
}

FuncState* ControlFlowEdge::state() const
{
   return this->graph ? this->graph->state() : nullptr;
}

void ControlFlowEdge::append(BCPos Other) const
{
   if (this->graph) this->graph->append_edge(this->index, Other);
}

void ControlFlowEdge::append(const ControlFlowEdge& Other) const
{
   if (this->graph) this->graph->append_edge(this->index, Other);
}

void ControlFlowEdge::patch_here() const
{
   if (this->graph) this->graph->patch_edge(this->index, this->graph->state()->pc);
}

void ControlFlowEdge::patch_to(BCPos Target) const
{
   if (this->graph) this->graph->patch_edge(this->index, Target);
}

void ControlFlowEdge::patch_head(BCPos Destination) const
{
   if (this->graph) this->graph->patch_edge_head(this->index, Destination);
}

ControlFlowGraph::ControlFlowGraph() : func_state(nullptr)
{
}

ControlFlowGraph::ControlFlowGraph(FuncState* State) : func_state(State)
{
}

void ControlFlowGraph::reset(FuncState* State)
{
   this->func_state = State;
   this->edges.clear();
}

bool ControlFlowGraph::valid() const
{
   return this->func_state != nullptr;
}

FuncState* ControlFlowGraph::state() const
{
   return this->func_state;
}

ControlFlowEdge ControlFlowGraph::make_edge(ControlFlowEdgeKind Kind, BCPos Head)
{
   EdgeEntry entry;
   entry.head = Head;
   entry.kind = Kind;
   this->edges.push_back(entry);
   return ControlFlowEdge(this, this->edges.size() - 1);
}

ControlFlowEdge ControlFlowGraph::make_unconditional(BCPos Head)
{
   return this->make_edge(ControlFlowEdgeKind::Unconditional, Head);
}

ControlFlowEdge ControlFlowGraph::make_true_edge(BCPos Head)
{
   return this->make_edge(ControlFlowEdgeKind::True, Head);
}

ControlFlowEdge ControlFlowGraph::make_false_edge(BCPos Head)
{
   return this->make_edge(ControlFlowEdgeKind::False, Head);
}

ControlFlowEdge ControlFlowGraph::make_break_edge(BCPos Head)
{
   return this->make_edge(ControlFlowEdgeKind::Break, Head);
}

ControlFlowEdge ControlFlowGraph::make_continue_edge(BCPos Head)
{
   return this->make_edge(ControlFlowEdgeKind::Continue, Head);
}

BCPos ControlFlowGraph::edge_head(size_t Index) const
{
   if (Index >= this->edges.size()) return NO_JMP;
   return this->edges[Index].head;
}

ControlFlowEdgeKind ControlFlowGraph::edge_kind(size_t Index) const
{
   if (Index >= this->edges.size()) return ControlFlowEdgeKind::Unconditional;
   return this->edges[Index].kind;
}

bool ControlFlowGraph::edge_resolved(size_t Index) const
{
   if (Index >= this->edges.size()) return true;
   return this->edges[Index].resolved;
}

void ControlFlowGraph::set_edge_head(size_t Index, BCPos Head)
{
   if (Index >= this->edges.size()) return;
   this->edges[Index].head = Head;
}

void ControlFlowGraph::mark_resolved(size_t Index)
{
   if (Index >= this->edges.size()) return;
   this->edges[Index].resolved = true;
}

void ControlFlowGraph::append_edge(size_t Index, BCPos Head)
{
   if (Index >= this->edges.size()) return;
   if (Head IS NO_JMP) return;

   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) {
      entry.head = Head;
      return;
   }

   entry.head = JumpListView(this->func_state, entry.head).append(Head);
}

void ControlFlowGraph::append_edge(size_t Index, const ControlFlowEdge& Other)
{
   if (Index >= this->edges.size() or not Other.valid() or Other.graph != this) return;
   this->append_edge(Index, Other.head());
   this->mark_resolved(Other.index);
}

void ControlFlowGraph::patch_edge(size_t Index, BCPos Target)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) {
      this->mark_resolved(Index);
      return;
   }
   JumpListView(this->func_state, entry.head).patch_to(Target);
   this->mark_resolved(Index);
}

void ControlFlowGraph::patch_edge_head(size_t Index, BCPos Destination)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) return;
   JumpListView(this->func_state, entry.head).patch_head(Destination);
   this->mark_resolved(Index);
}

void ControlFlowGraph::finalize() const
{
#if LJ_DEBUG
   for (size_t i = 0; i < this->edges.size(); ++i) {
      if (not this->edges[i].resolved and not(this->edges[i].head IS NO_JMP)) {
         lj_assertFS(false, "unresolved control-flow edge kind=%d head=%d", int(this->edges[i].kind),
            int(this->edges[i].head));
      }
   }
#endif
}

