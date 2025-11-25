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
   constexpr ControlFlowEdge();
   constexpr ControlFlowEdge(ControlFlowGraph* Graph, size_t Index);

   [[nodiscard]] inline bool empty() const;
   [[nodiscard]] constexpr bool valid() const noexcept;
   [[nodiscard]] inline ControlFlowEdgeKind kind() const;
   [[nodiscard]] inline BCPOS head() const;
   [[nodiscard]] inline FuncState* state() const;

   inline void append(BCPOS Other) const;
   inline void append(const ControlFlowEdge& Other) const;
   inline void patch_here() const;
   inline void patch_to(BCPOS Target) const;
   inline void patch_head(BCPOS Destination) const;
   inline void patch_with_value(BCPOS ValueTarget, BCREG Register, BCPOS DefaultTarget) const;
   [[nodiscard]] inline bool produces_values() const;
   inline void drop_values() const;

private:
   friend class ControlFlowGraph;

   ControlFlowGraph* graph;
   size_t index;
};

class ControlFlowGraph {
public:
   constexpr ControlFlowGraph() noexcept : func_state(nullptr) { }
   explicit constexpr ControlFlowGraph(FuncState* State) noexcept : func_state(State) { }

   [[nodiscard]] ControlFlowEdge make_edge(ControlFlowEdgeKind Kind, BCPOS Head = NO_JMP);

   void finalize() const;

   // Debug tracing methods
   void trace_edge_creation(ControlFlowEdgeKind Kind, BCPOS Head, size_t Index) const;
   void trace_edge_patch(size_t Index, BCPOS Target) const;
   void trace_edge_append(size_t Index, BCPOS Head) const;

   inline void reset(FuncState* State) {
      this->func_state = State;
      this->edges.clear();
   }

   [[nodiscard]] constexpr bool valid() const noexcept { return this->func_state != nullptr; }

   [[nodiscard]] constexpr FuncState* state() const noexcept { return this->func_state; }
   [[nodiscard]] inline ControlFlowEdge make_unconditional(BCPOS Head = NO_JMP) { return this->make_edge(ControlFlowEdgeKind::Unconditional, Head); }
   [[nodiscard]] inline ControlFlowEdge make_true_edge(BCPOS Head = NO_JMP) { return this->make_edge(ControlFlowEdgeKind::True, Head); }
   [[nodiscard]] inline ControlFlowEdge make_false_edge(BCPOS Head = NO_JMP) { return this->make_edge(ControlFlowEdgeKind::False, Head); }
   [[nodiscard]] inline ControlFlowEdge make_break_edge(BCPOS Head = NO_JMP) { return this->make_edge(ControlFlowEdgeKind::Break, Head); }
   [[nodiscard]] inline ControlFlowEdge make_continue_edge(BCPOS Head = NO_JMP) { return this->make_edge(ControlFlowEdgeKind::Continue, Head); }

private:
   friend class ControlFlowEdge;

   struct EdgeEntry {
      BCPOS head = NO_JMP;
      ControlFlowEdgeKind kind = ControlFlowEdgeKind::Unconditional;
      bool resolved = false;
   };

   void append_edge(size_t Index, BCPOS Head);
   void patch_edge(size_t Index, BCPOS Target);
   void patch_edge_head(size_t Index, BCPOS Destination);
   void patch_edge_with_value(size_t Index, BCPOS ValueTarget, BCREG Register, BCPOS DefaultTarget);
   [[nodiscard]] bool edge_produces_values(size_t Index) const;
   void drop_edge_values(size_t Index);
   [[nodiscard]] static BCPOS next_in_chain(FuncState* State, BCPOS Position);
   [[nodiscard]] bool patch_test_register(BCPOS Position, BCREG Register) const;
   void patch_instruction(BCPOS Position, BCPOS Destination) const;

   inline void append_edge(size_t Index, const ControlFlowEdge& Other) {
      if (Index >= this->edges.size() or not Other.valid() or Other.graph != this) return;
      this->append_edge(Index, Other.head());
      this->mark_resolved(Other.index);
   }

   [[nodiscard]] inline BCPOS edge_head(size_t Index) const {
      if (Index >= this->edges.size()) return NO_JMP;
      return this->edges[Index].head;
   }

   inline ControlFlowEdgeKind edge_kind(size_t Index) const {
      if (Index >= this->edges.size()) return ControlFlowEdgeKind::Unconditional;
      return this->edges[Index].kind;
   }

   inline bool edge_resolved(size_t Index) const {
      if (Index >= this->edges.size()) return true;
      return this->edges[Index].resolved;
   }

   inline void set_edge_head(size_t Index, BCPOS Head) {
      if (Index >= this->edges.size()) return;
      this->edges[Index].head = Head;
   }

   inline void mark_resolved(size_t Index) {
      if (Index >= this->edges.size()) return;
      this->edges[Index].resolved = true;
   }

   FuncState* func_state;
   std::vector<EdgeEntry> edges;
};

//********************************************************************************************************************
// ControlFlowEdge inline implementations

constexpr ControlFlowEdge::ControlFlowEdge() : graph(nullptr), index(0)
{
}

constexpr ControlFlowEdge::ControlFlowEdge(ControlFlowGraph* Graph, size_t Index) : graph(Graph), index(Index)
{
}

inline bool ControlFlowEdge::empty() const
{
   if (not this->graph) return true;
   return this->graph->edge_head(this->index) IS NO_JMP;
}

constexpr bool ControlFlowEdge::valid() const noexcept
{
   return this->graph != nullptr;
}

inline ControlFlowEdgeKind ControlFlowEdge::kind() const
{
   return this->graph ? this->graph->edge_kind(this->index) : ControlFlowEdgeKind::Unconditional;
}

inline BCPOS ControlFlowEdge::head() const
{
   return this->graph ? this->graph->edge_head(this->index) : NO_JMP;
}

inline FuncState* ControlFlowEdge::state() const
{
   return this->graph ? this->graph->state() : nullptr;
}

inline void ControlFlowEdge::append(BCPOS Other) const
{
   if (this->graph) this->graph->append_edge(this->index, Other);
}

inline void ControlFlowEdge::append(const ControlFlowEdge& Other) const
{
   if (this->graph) this->graph->append_edge(this->index, Other);
}

inline void ControlFlowEdge::patch_here() const
{
   if (this->graph) this->graph->patch_edge(this->index, this->graph->state()->pc);
}

inline void ControlFlowEdge::patch_to(BCPOS Target) const
{
   if (this->graph) this->graph->patch_edge(this->index, Target);
}

inline void ControlFlowEdge::patch_head(BCPOS Destination) const
{
   if (this->graph) this->graph->patch_edge_head(this->index, Destination);
}

inline void ControlFlowEdge::patch_with_value(BCPOS ValueTarget, BCREG Register, BCPOS DefaultTarget) const
{
   if (this->graph) this->graph->patch_edge_with_value(this->index, ValueTarget, Register, DefaultTarget);
}

inline bool ControlFlowEdge::produces_values() const
{
   return this->graph ? this->graph->edge_produces_values(this->index) : false;
}

inline void ControlFlowEdge::drop_values() const
{
   if (this->graph) this->graph->drop_edge_values(this->index);
}
