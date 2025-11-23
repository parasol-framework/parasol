// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#include "parser/parse_control_flow.h"

#include <parasol/main.h>

#include "parser/parse_internal.h"

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
   size_t index = this->edges.size() - 1;
#if LJ_DEBUG
   this->trace_edge_creation(Kind, Head, index);
#endif
   return ControlFlowEdge(this, index);
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
#if LJ_DEBUG
      this->trace_edge_append(Index, Head);
#endif
      return;
   }

   entry.head = JumpListView(this->func_state, entry.head).append(Head);
#if LJ_DEBUG
   this->trace_edge_append(Index, Head);
#endif
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
   if (entry.resolved) return;
   if (entry.head IS NO_JMP) {
      this->mark_resolved(Index);
      return;
   }
#if LJ_DEBUG
   this->trace_edge_patch(Index, Target);
#endif
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

void ControlFlowGraph::patch_edge_with_value(size_t Index, BCPos ValueTarget, BCReg Register, BCPos DefaultTarget)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) {
      this->mark_resolved(Index);
      return;
   }

   BCPos list = entry.head;
   while (not(list IS NO_JMP)) {
      BCPos next_pc = next_in_chain(this->func_state, list);
      if (this->patch_test_register(list, Register)) {
         this->patch_instruction(list, ValueTarget);
      }
      else {
         this->patch_instruction(list, DefaultTarget);
      }
      list = next_pc;
   }
   this->mark_resolved(Index);
}

bool ControlFlowGraph::edge_produces_values(size_t Index) const
{
   if (Index >= this->edges.size()) return false;
   const EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) return false;

   for (BCPos list = entry.head; not(list IS NO_JMP); list = next_in_chain(this->func_state, list)) {
      BCIns prior = this->func_state->bcbase[list >= 1 ? list - 1 : list].ins;
      if (not(bc_op(prior) IS BC_ISTC or bc_op(prior) IS BC_ISFC or bc_a(prior) IS NO_REG)) {
         return true;
      }
   }
   return false;
}

void ControlFlowGraph::drop_edge_values(size_t Index)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) return;

   for (BCPos list = entry.head; not(list IS NO_JMP); list = next_in_chain(this->func_state, list)) {
      (void)this->patch_test_register(list, NO_REG);
   }
}

BCPos ControlFlowGraph::next_in_chain(FuncState* State, BCPos Position)
{
   ptrdiff_t delta = bc_j(State->bcbase[Position].ins);
   if (BCPos(delta) IS NO_JMP) return NO_JMP;
   return BCPos((ptrdiff_t(Position) + 1) + delta);
}

bool ControlFlowGraph::patch_test_register(BCPos Position, BCReg Register) const
{
   FuncState* fs = this->func_state;
   BCInsLine* line = &fs->bcbase[Position >= 1 ? Position - 1 : Position];
   BCOp op = bc_op(line->ins);

   if (op IS BC_ISTC or op IS BC_ISFC) {
      if (Register != NO_REG and Register != bc_d(line->ins)) {
         setbc_a(&line->ins, Register);
      }
      else {
         setbc_op(&line->ins, op + (BC_IST - BC_ISTC));
         setbc_a(&line->ins, 0);
      }
   }
   else if (bc_a(line->ins) IS NO_REG) {
      if (Register IS NO_REG) {
         line->ins = BCINS_AJ(BC_JMP, bc_a(fs->bcbase[Position].ins), 0);
      }
      else {
         setbc_a(&line->ins, Register);
         if (Register >= bc_a(line[1].ins)) {
            setbc_a(&line[1].ins, Register + 1);
         }
      }
   }
   else {
      return false;
   }
   return true;
}

void ControlFlowGraph::patch_instruction(BCPos Position, BCPos Destination) const
{
   FuncState* fs = this->func_state;
   BCIns* instruction = &fs->bcbase[Position].ins;
   BCPos offset = Destination - (Position + 1) + BCBIAS_J;
   lj_assertFS(not(Destination IS NO_JMP), "uninitialized jump target");
   if (offset > BCMAX_D) {
      fs->ls->err_syntax(ErrMsg::XJUMP);
   }
   setbc_d(instruction, offset);
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

//********************************************************************************************************************
// Debug tracing methods

#if LJ_DEBUG

void ControlFlowGraph::trace_edge_creation(ControlFlowEdgeKind Kind, BCPos Head, size_t Index) const
{
#if defined(LJ_TRACE_CFG)
   const char* kind_name = "unknown";
   switch (Kind) {
      case ControlFlowEdgeKind::Unconditional: kind_name = "unconditional"; break;
      case ControlFlowEdgeKind::True: kind_name = "true"; break;
      case ControlFlowEdgeKind::False: kind_name = "false"; break;
      case ControlFlowEdgeKind::Break: kind_name = "break"; break;
      case ControlFlowEdgeKind::Continue: kind_name = "continue"; break;
   }
   pf::Log log("Parser");
   log.trace("[CFG] create edge #%zu kind=%s head=%d", Index, kind_name, int(Head));
#else
   (void)Kind; (void)Head; (void)Index;
#endif
}

void ControlFlowGraph::trace_edge_patch(size_t Index, BCPos Target) const
{
#if defined(LJ_TRACE_CFG)
   pf::Log log("Parser");
   log.trace("[CFG] patch edge #%zu to target=%d", Index, int(Target));
#else
   (void)Index; (void)Target;
#endif
}

void ControlFlowGraph::trace_edge_append(size_t Index, BCPos Head) const
{
#if defined(LJ_TRACE_CFG)
   pf::Log log("Parser");
   log.trace("[CFG] append to edge #%zu head=%d", Index, int(Head));
#else
   (void)Index; (void)Head;
#endif
}

#endif

