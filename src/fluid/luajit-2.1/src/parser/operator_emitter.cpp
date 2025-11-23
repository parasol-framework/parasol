// Copyright (C) 2025 Paul Manias

#define LUA_CORE

#include "lj_obj.h"
#include "bytecode/lj_bc.h"
#include "parser/lj_lex.h"

#include <parasol/main.h>

#include "parser/operator_emitter.h"
#include "parser/parse_internal.h"

// Legacy helpers are declared as extern in parse_internal.h

OperatorEmitter::OperatorEmitter(FuncState* State, RegisterAllocator* Allocator, ControlFlowGraph* Cfg)
   : func_state(State), allocator(Allocator), cfg(Cfg)
{
}

//********************************************************************************************************************
// Constant folding for arithmetic operators
// Facade wrapper over legacy foldarith() function

bool OperatorEmitter::fold_constant_arith(BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   return foldarith(opr, e1, e2) != 0;
}

//********************************************************************************************************************
// Emit unary operator
// Facade wrapper over legacy bcemit_unop() function

void OperatorEmitter::emit_unary(int op, ExpDesc* operand)
{
   bcemit_unop(this->func_state, BCOp(op), operand);
}

//********************************************************************************************************************
// Prepare left operand for binary operation
// Facade wrapper over legacy bcemit_binop_left() function
// MUST be called before evaluating right operand to prevent register clobbering

void OperatorEmitter::emit_binop_left(BinOpr opr, ExpDesc* left)
{
   bcemit_binop_left(this->func_state, opr, left);
}

//********************************************************************************************************************
// Emit arithmetic binary operator
// Facade wrapper over legacy bcemit_arith() function

void OperatorEmitter::emit_binary_arith(BinOpr opr, ExpDesc* left, ExpDesc* right)
{
   bcemit_arith(this->func_state, opr, left, right);
}

//********************************************************************************************************************
// Emit comparison operator
// Facade wrapper over legacy bcemit_comp() function

void OperatorEmitter::emit_comparison(BinOpr opr, ExpDesc* left, ExpDesc* right)
{
   bcemit_comp(this->func_state, opr, left, right);
}

//********************************************************************************************************************
// Emit bitwise binary operator
// Bitwise operators use the same emission as arithmetic operators

void OperatorEmitter::emit_binary_bitwise(BinOpr opr, ExpDesc* left, ExpDesc* right)
{
   // Bitwise operators use the same emission as arithmetic operators
   bcemit_arith(this->func_state, opr, left, right);
}

//********************************************************************************************************************
// Prepare logical AND operator (called BEFORE RHS evaluation)
// Stage 2: CFG-based implementation using ControlFlowGraph

void OperatorEmitter::prepare_logical_and(ExpDesc* left)
{
   // AND short-circuit logic: if left is false, skip RHS and return left (false)
   // If left is true, evaluate RHS and return RHS result

   // Discharge left operand to appropriate form
   ExpressionValue left_val(this->func_state, *left);
   left_val.discharge();
   *left = left_val.legacy();

   BCPos pc;

   // Handle constant folding
   if (left->k IS ExpKind::Str or left->k IS ExpKind::Num or left->k IS ExpKind::True) {
      // Left is truthy constant - no jump needed, will evaluate RHS
      pc = NO_JMP;
   }
   else if (left->k IS ExpKind::Jmp) {
      // Left is already a jump expression - invert condition
      invertcond(this->func_state, left);
      pc = left->u.s.info;
   }
   else if (left->k IS ExpKind::False or left->k IS ExpKind::Nil) {
      // Left is falsey constant - load to register and jump to skip RHS
      expr_toreg_nobranch(this->func_state, left, NO_REG);
      pc = bcemit_jmp(this->func_state);
   }
   else {
      // Runtime value - emit conditional branch (jump if false)
      pc = bcemit_branch(this->func_state, left, 0);
   }

   // Set up CFG edges for short-circuit behavior
   ControlFlowEdge false_edge = this->cfg->make_false_edge(left->f);
   false_edge.append(pc);
   left->f = false_edge.head();

   ControlFlowEdge true_edge = this->cfg->make_true_edge(left->t);
   true_edge.patch_here();
   left->t = NO_JMP;
}

//********************************************************************************************************************
// Complete logical AND operator (called AFTER RHS evaluation)
// Stage 2: CFG-based implementation using ControlFlowGraph

void OperatorEmitter::complete_logical_and(ExpDesc* left, ExpDesc* right)
{
   // At this point:
   // - left->f contains jumps for "left is false" path
   // - right has been evaluated
   // - We need to merge the false paths and return right's result

   FuncState* fs = this->func_state;  // For lj_assertFS macro
   lj_assertFS(left->t IS NO_JMP, "jump list not closed");

   // Discharge right operand
   ExpressionValue right_val(this->func_state, *right);
   right_val.discharge();
   *right = right_val.legacy();

   // Merge false paths: both "left is false" and "right is false" go to same target
   ControlFlowEdge false_edge = this->cfg->make_false_edge(right->f);
   false_edge.append(left->f);
   right->f = false_edge.head();

   // Result is right's value
   *left = *right;
}

//********************************************************************************************************************
// Prepare logical OR operator (called BEFORE RHS evaluation)
// TODO: Replace with CFG-based implementation (Stage 3)

void OperatorEmitter::prepare_logical_or(ExpDesc* left)
{
   // Stage 1: Legacy wrapper - preserve existing behavior
   bcemit_binop_left(this->func_state, OPR_OR, left);
}

//********************************************************************************************************************
// Complete logical OR operator (called AFTER RHS evaluation)
// TODO: Replace with CFG-based implementation (Stage 3)

void OperatorEmitter::complete_logical_or(ExpDesc* left, ExpDesc* right)
{
   // Stage 1: Legacy wrapper - preserve existing behavior
   bcemit_binop(this->func_state, OPR_OR, left, right);
}

//********************************************************************************************************************
// Prepare IF_EMPTY (??) operator (called BEFORE RHS evaluation)
// TODO: Replace with CFG-based implementation (Stage 4)

void OperatorEmitter::prepare_if_empty(ExpDesc* left)
{
   // Stage 1: Legacy wrapper - preserve existing behavior
   bcemit_binop_left(this->func_state, OPR_IF_EMPTY, left);
}

//********************************************************************************************************************
// Complete IF_EMPTY (??) operator (called AFTER RHS evaluation)
// TODO: Replace with CFG-based implementation (Stage 4)

void OperatorEmitter::complete_if_empty(ExpDesc* left, ExpDesc* right)
{
   // Stage 1: Legacy wrapper - preserve existing behavior
   bcemit_binop(this->func_state, OPR_IF_EMPTY, left, right);
}
