// Control flow helpers for parser expression management.
// Copyright (C) 2025 Paul Manias

#include "parser/parse_control_flow.h"
#include <parasol/main.h>
#include "parser/parse_internal.h"

//********************************************************************************************************************

ControlFlowEdge ControlFlowGraph::make_edge(ControlFlowEdgeKind Kind, BCPOS Head)
{
   EdgeEntry entry;
   entry.head = Head;
   entry.kind = Kind;
   this->edges.push_back(entry);
   size_t index = this->edges.size() - 1;
   this->trace_edge_creation(Kind, Head, index);
   return ControlFlowEdge(this, index);
}

//********************************************************************************************************************

void ControlFlowGraph::append_edge(size_t Index, BCPOS Head)
{
   if (Index >= this->edges.size()) return;
   if (Head IS NO_JMP) return;

   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) {
      entry.head = Head;
      this->trace_edge_append(Index, Head);
      return;
   }

   entry.head = JumpListView(this->func_state, entry.head).append(Head);
   this->trace_edge_append(Index, Head);
}

//********************************************************************************************************************

void ControlFlowGraph::patch_edge(size_t Index, BCPOS Target)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry &entry = this->edges[Index];
   if (entry.resolved) return;
   if (entry.head IS NO_JMP) {
      this->mark_resolved(Index);
      return;
   }
   this->trace_edge_patch(Index, Target);
   JumpListView(this->func_state, entry.head).patch_to(Target);
   this->mark_resolved(Index);
}

//********************************************************************************************************************

void ControlFlowGraph::patch_edge_head(size_t Index, BCPOS Destination)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry &entry = this->edges[Index];
   if (entry.head IS NO_JMP) return;
   JumpListView(this->func_state, entry.head).patch_head(Destination);
   this->mark_resolved(Index);
}

//********************************************************************************************************************

void ControlFlowGraph::patch_edge_with_value(size_t Index, BCPOS ValueTarget, BCREG Register, BCPOS DefaultTarget)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry &entry = this->edges[Index];
   if (entry.head IS NO_JMP) {
      this->mark_resolved(Index);
      return;
   }

   BCPOS list = entry.head;
   while (not(list IS NO_JMP)) {
      BCPOS next_pc = next_in_chain(this->func_state, list);
      if (this->patch_test_register(list, Register)) this->patch_instruction(list, ValueTarget);
      else this->patch_instruction(list, DefaultTarget);

      list = next_pc;
   }
   this->mark_resolved(Index);
}

//********************************************************************************************************************

bool ControlFlowGraph::edge_produces_values(size_t Index) const
{
   if (Index >= this->edges.size()) return false;
   const EdgeEntry &entry = this->edges[Index];
   if (entry.head IS NO_JMP) return false;

   for (BCPOS list = entry.head; not(list IS NO_JMP); list = next_in_chain(this->func_state, list)) {
      BCIns prior = this->func_state->bcbase[list >= 1 ? list - 1 : list].ins;
      if (not(bc_op(prior) IS BC_ISTC or bc_op(prior) IS BC_ISFC or bc_a(prior) IS NO_REG)) {
         return true;
      }
   }
   return false;
}

//********************************************************************************************************************

void ControlFlowGraph::drop_edge_values(size_t Index)
{
   if (Index >= this->edges.size()) return;
   EdgeEntry& entry = this->edges[Index];
   if (entry.head IS NO_JMP) return;

   for (BCPOS list = entry.head; not(list IS NO_JMP); list = next_in_chain(this->func_state, list)) {
      (void)this->patch_test_register(list, NO_REG);
   }
}

//********************************************************************************************************************

BCPOS ControlFlowGraph::next_in_chain(FuncState* State, BCPOS Position)
{
   ptrdiff_t delta = bc_j(State->bcbase[Position].ins);
   if (BCPOS(delta) IS NO_JMP) return NO_JMP;
   return BCPOS((ptrdiff_t(Position) + 1) + delta);
}

//********************************************************************************************************************

bool ControlFlowGraph::patch_test_register(BCPOS Position, BCREG Register) const
{
   FuncState *fs = this->func_state;
   BCInsLine *line = &fs->bcbase[Position >= 1 ? Position - 1 : Position];
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
      if (Register IS NO_REG) line->ins = BCINS_AJ(BC_JMP, bc_a(fs->bcbase[Position].ins), 0);
      else {
         setbc_a(&line->ins, Register);
         if (Register >= bc_a(line[1].ins)) setbc_a(&line[1].ins, Register + 1);
      }
   }
   else return false;
   return true;
}

void ControlFlowGraph::patch_instruction(BCPOS Position, BCPOS Destination) const
{
   FuncState* fs = this->func_state;
   BCIns* instruction = &fs->bcbase[Position].ins;
   BCPOS offset = Destination - (Position + 1) + BCBIAS_J;
   fs->assert(not(Destination IS NO_JMP), "uninitialized jump target");
   if (offset > BCMAX_D) fs->ls->err_syntax(ErrMsg::XJUMP);
   setbc_d(instruction, offset);
}

//********************************************************************************************************************

void ControlFlowGraph::finalize() const
{
   if (GetResource(RES::LOG_LEVEL) >= 4) {
      for (size_t i = 0; i < this->edges.size(); ++i) {
         if (not this->edges[i].resolved and not(this->edges[i].head IS NO_JMP)) {
            pf::Log("Parser").error("Unresolved control-flow edge kind=%d head=%d", int(this->edges[i].kind), int(this->edges[i].head));
         }
      }
   }
}

//********************************************************************************************************************
// Debug tracing methods

void ControlFlowGraph::trace_edge_creation(ControlFlowEdgeKind Kind, BCPOS Head, size_t Index) const
{
   auto prv = (prvFluid *)this->func_state->L->Script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_CFG) != JOF::NIL) {
      CSTRING kind_name = "unknown";
      switch (Kind) {
         case ControlFlowEdgeKind::Unconditional: kind_name = "unconditional"; break;
         case ControlFlowEdgeKind::True: kind_name = "true"; break;
         case ControlFlowEdgeKind::False: kind_name = "false"; break;
         case ControlFlowEdgeKind::Break: kind_name = "break"; break;
         case ControlFlowEdgeKind::Continue: kind_name = "continue"; break;
      }
      pf::Log("Parser").msg("[%d] cfg: create edge #%" PRId64 " kind=%s head=%d", this->func_state->ls->linenumber, Index, kind_name, int(Head));
   }
}

//********************************************************************************************************************

void ControlFlowGraph::trace_edge_patch(size_t Index, BCPOS Target) const
{
   auto prv = (prvFluid *)this->func_state->L->Script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_CFG) != JOF::NIL) {
      pf::Log("Parser").msg("[%d] cfg: patch edge #%" PRId64 " to target=%d", this->func_state->ls->linenumber, Index, int(Target));
   }
}

//********************************************************************************************************************

void ControlFlowGraph::trace_edge_append(size_t Index, BCPOS Head) const
{
   auto prv = (prvFluid *)this->func_state->L->Script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_CFG) != JOF::NIL) {
      pf::Log("Parser").msg("[%d] cfg: append to edge #%" PRId64 " head=%d", this->func_state->ls->linenumber, Index, int(Head));
   }
}
