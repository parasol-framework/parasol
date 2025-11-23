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

bool OperatorEmitter::fold_constant_arith(BinOpr opr, ValueSlot e1, ValueUse e2)
{
   return foldarith(opr, e1.raw(), e2.raw()) != 0;
}

//********************************************************************************************************************
// Emit unary operator
// Facade wrapper over legacy bcemit_unop() function

void OperatorEmitter::emit_unary(int op, ValueSlot operand)
{
   bcemit_unop(this->func_state, BCOp(op), operand.raw());
}

//********************************************************************************************************************
// Emit bitwise NOT operator (~)
// Calls bit.bnot library function

void OperatorEmitter::emit_bitnot(ValueSlot operand)
{
   bcemit_unary_bit_call(this->func_state, "bnot", operand.raw());
}

//********************************************************************************************************************
// Prepare left operand for binary operation
// Facade wrapper over legacy bcemit_binop_left() function
// MUST be called before evaluating right operand to prevent register clobbering

void OperatorEmitter::emit_binop_left(BinOpr opr, ValueSlot left)
{
   bcemit_binop_left(this->func_state, opr, left.raw());
}

//********************************************************************************************************************
// Emit arithmetic binary operator
// Facade wrapper over legacy bcemit_arith() function

void OperatorEmitter::emit_binary_arith(BinOpr opr, ValueSlot left, ValueUse right)
{
   bcemit_arith(this->func_state, opr, left.raw(), right.raw());
}

//********************************************************************************************************************
// Emit comparison operator
// Facade wrapper over legacy bcemit_comp() function

void OperatorEmitter::emit_comparison(BinOpr opr, ValueSlot left, ValueUse right)
{
   bcemit_comp(this->func_state, opr, left.raw(), right.raw());
}

//********************************************************************************************************************
// Emit bitwise binary operator
// Bitwise operators use the same emission as arithmetic operators

void OperatorEmitter::emit_binary_bitwise(BinOpr opr, ValueSlot left, ValueUse right)
{
   // Bitwise operators use the same emission as arithmetic operators
   bcemit_arith(this->func_state, opr, left.raw(), right.raw());
}

//********************************************************************************************************************
// Prepare logical AND operator (called BEFORE RHS evaluation)
// Stage 2: CFG-based implementation using ControlFlowGraph

void OperatorEmitter::prepare_logical_and(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();

   // AND short-circuit logic: if left is false, skip RHS and return left (false)
   // If left is true, evaluate RHS and return RHS result

   // Discharge left operand to appropriate form
   ExpressionValue left_val(this->func_state, *left_desc);
   left_val.discharge();
   *left_desc = left_val.legacy();

   BCPos pc;

   // Handle constant folding
   if (left_desc->k IS ExpKind::Str or left_desc->k IS ExpKind::Num or left_desc->k IS ExpKind::True) {
      // Left is truthy constant - no jump needed, will evaluate RHS
      pc = NO_JMP;
   }
   else if (left_desc->k IS ExpKind::Jmp) {
      // Left is already a jump expression - invert condition
      invertcond(this->func_state, left_desc);
      pc = left_desc->u.s.info;
   }
   else if (left_desc->k IS ExpKind::False or left_desc->k IS ExpKind::Nil) {
      // Left is falsey constant - load to register and jump to skip RHS
      expr_toreg_nobranch(this->func_state, left_desc, NO_REG);
      pc = bcemit_jmp(this->func_state);
   }
   else {
      // Runtime value - emit conditional branch (jump if false)
      pc = bcemit_branch(this->func_state, left_desc, 0);
   }

   // Set up CFG edges for short-circuit behavior
   ControlFlowEdge false_edge = this->cfg->make_false_edge(left_desc->f);
   false_edge.append(pc);
   left_desc->f = false_edge.head();

   ControlFlowEdge true_edge = this->cfg->make_true_edge(left_desc->t);
   true_edge.patch_here();
   left_desc->t = NO_JMP;
}

//********************************************************************************************************************
// Complete logical AND operator (called AFTER RHS evaluation)
// Stage 2: CFG-based implementation using ControlFlowGraph

void OperatorEmitter::complete_logical_and(ValueSlot left, ValueUse right)
{
   ExpDesc* left_desc = left.raw();
   ExpDesc* right_desc = right.raw();

   // At this point:
   // - left->f contains jumps for "left is false" path
   // - right has been evaluated
   // - We need to merge the false paths and return right's result

   FuncState* fs = this->func_state;  // For lj_assertFS macro
   lj_assertFS(left_desc->t IS NO_JMP, "jump list not closed");

   // Discharge right operand
   ExpressionValue right_val(this->func_state, *right_desc);
   right_val.discharge();
   *right_desc = right_val.legacy();

   // Merge false paths: both "left is false" and "right is false" go to same target
   ControlFlowEdge false_edge = this->cfg->make_false_edge(right_desc->f);
   false_edge.append(left_desc->f);
   right_desc->f = false_edge.head();

   // Result is right's value
   *left_desc = *right_desc;
}

//********************************************************************************************************************
// Prepare logical OR operator (called BEFORE RHS evaluation)
// Stage 3: CFG-based implementation using ControlFlowGraph

void OperatorEmitter::prepare_logical_or(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();

   // OR short-circuit logic: if left is true, skip RHS and return left (true)
   // If left is false, evaluate RHS and return RHS result

   // Discharge left operand to appropriate form
   ExpressionValue left_val(this->func_state, *left_desc);
   left_val.discharge();
   *left_desc = left_val.legacy();

   BCPos pc;

   // Handle constant folding
   if (left_desc->k IS ExpKind::Nil or left_desc->k IS ExpKind::False) {
      // Left is falsey constant - no jump needed, will evaluate RHS
      pc = NO_JMP;
   }
   else if (left_desc->k IS ExpKind::Jmp) {
      // Left is already a jump expression - use as-is
      pc = left_desc->u.s.info;
   }
   else if (left_desc->k IS ExpKind::Str or left_desc->k IS ExpKind::Num or left_desc->k IS ExpKind::True) {
      // Left is truthy constant - load to register and jump to skip RHS
      expr_toreg_nobranch(this->func_state, left_desc, NO_REG);
      pc = bcemit_jmp(this->func_state);
   }
   else {
      // Runtime value - emit conditional branch (jump if true)
      pc = bcemit_branch(this->func_state, left_desc, 1);
   }

   // Set up CFG edges for short-circuit behavior
   ControlFlowEdge true_edge = this->cfg->make_true_edge(left_desc->t);
   true_edge.append(pc);
   left_desc->t = true_edge.head();

   ControlFlowEdge false_edge = this->cfg->make_false_edge(left_desc->f);
   false_edge.patch_here();
   left_desc->f = NO_JMP;
}

//********************************************************************************************************************
// Complete logical OR operator (called AFTER RHS evaluation)
// Stage 3: CFG-based implementation using ControlFlowGraph

void OperatorEmitter::complete_logical_or(ValueSlot left, ValueUse right)
{
   ExpDesc* left_desc = left.raw();
   ExpDesc* right_desc = right.raw();

   // At this point:
   // - left->t contains jumps for "left is true" path
   // - right has been evaluated
   // - We need to merge the true paths and return right's result

   FuncState* fs = this->func_state;  // For lj_assertFS macro
   lj_assertFS(left_desc->f IS NO_JMP, "jump list not closed");

   // Discharge right operand
   ExpressionValue right_val(this->func_state, *right_desc);
   right_val.discharge();
   *right_desc = right_val.legacy();

   // Merge true paths: both "left is true" and "right is true" go to same target
   ControlFlowEdge true_edge = this->cfg->make_true_edge(right_desc->t);
   true_edge.append(left_desc->t);
   right_desc->t = true_edge.head();

   // Result is right's value
   *left_desc = *right_desc;
}

//********************************************************************************************************************
// Prepare IF_EMPTY (??) operator (called BEFORE RHS evaluation)
// Stage 4: CFG-based implementation with extended falsey semantics

void OperatorEmitter::prepare_if_empty(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();

   // IF_EMPTY short-circuit: if left is truthy, skip RHS and return left
   // Extended falsey: nil, false, 0, "" (all trigger RHS evaluation)

   // Discharge left operand
   ExpressionValue left_val(this->func_state, *left_desc);
   left_val.discharge();
   *left_desc = left_val.legacy();

   BCPos pc;

   // Handle constant folding for known falsey values
   if (left_desc->k IS ExpKind::Nil or left_desc->k IS ExpKind::False)
      pc = NO_JMP;  // Falsey constant - will evaluate RHS
   else if (left_desc->k IS ExpKind::Num and expr_numiszero(left_desc))
      pc = NO_JMP;  // Zero is falsey - will evaluate RHS
   else if (left_desc->k IS ExpKind::Str and left_desc->u.sval and left_desc->u.sval->len IS 0)
      pc = NO_JMP;  // Empty string is falsey - will evaluate RHS
   else if (left_desc->k IS ExpKind::Jmp)
      pc = left_desc->u.s.info;
   else if (left_desc->k IS ExpKind::Str or left_desc->k IS ExpKind::Num or left_desc->k IS ExpKind::True) {
      // Truthy constant - load to register and skip RHS
      RegisterAllocator allocator(this->func_state);
      allocator.reserve(1);
      expr_toreg_nobranch(this->func_state, left_desc, this->func_state->freereg - 1);
      pc = bcemit_jmp(this->func_state);
   }
   else {
      // Runtime value - need extended falsey checks in complete phase
      // Reserve register for RHS and mark with HasRhsReg flag
      if (!expr_isk_nojump(left_desc)) {
         ExpressionValue left_inner(this->func_state, *left_desc);
         RegisterAllocator allocator(this->func_state);
         BCReg src_reg = left_inner.discharge_to_any_reg(allocator);
         *left_desc = left_inner.legacy();
         BCReg rhs_reg = this->func_state->freereg;
         ExprFlag saved_flags = left_desc->flags;
         allocator.reserve(1);
         expr_init(left_desc, ExpKind::NonReloc, src_reg);
         left_desc->u.s.aux = rhs_reg;
         left_desc->flags = saved_flags | ExprFlag::HasRhsReg;
      }
      pc = NO_JMP;  // No jump yet - extended checks happen in complete phase
   }

   // Set up CFG edges
   ControlFlowEdge true_edge = this->cfg->make_true_edge(left_desc->t);
   true_edge.append(pc);
   left_desc->t = true_edge.head();

   ControlFlowEdge false_edge = this->cfg->make_false_edge(left_desc->f);
   false_edge.patch_here();
   left_desc->f = NO_JMP;
}

//********************************************************************************************************************
// Complete IF_EMPTY (??) operator (called AFTER RHS evaluation)
// Stage 4: CFG-based implementation with extended falsey checks

void OperatorEmitter::complete_if_empty(ValueSlot left, ValueUse right)
{
   ExpDesc* left_desc = left.raw();
   ExpDesc* right_desc = right.raw();

   FuncState* fs = this->func_state;  // For macros and brevity
   lj_assertFS(left_desc->f IS NO_JMP, "jump list not closed");

   // If left->t has jumps, LHS is truthy - patch and return LHS
   if (left_desc->t != NO_JMP) {
      ControlFlowEdge true_edge = this->cfg->make_true_edge(left_desc->t);
      true_edge.patch_to(fs->pc);
      left_desc->t = NO_JMP;

      // Ensure LHS is in a register for the result
      if (left_desc->k != ExpKind::NonReloc and left_desc->k != ExpKind::Relocable) {
         if (expr_isk(left_desc)) {
            RegisterAllocator allocator(fs);
            allocator.reserve(1);
            expr_toreg_nobranch(fs, left_desc, fs->freereg - 1);
         }
         else {
            ExpressionValue left_alt(fs, *left_desc);
            RegisterAllocator allocator(fs);
            left_alt.discharge_to_any_reg(allocator);
            *left_desc = left_alt.legacy();
         }
      }
      return;
   }

   // LHS is falsey OR needs runtime checks
   BCReg rhs_reg = NO_REG;
   if (expr_consume_flag(left_desc, ExprFlag::HasRhsReg)) {
      rhs_reg = BCReg(left_desc->u.s.aux);
   }

   ExpressionValue left_discharge(fs, *left_desc);
   left_discharge.discharge();
   *left_desc = left_discharge.legacy();

   if (left_desc->k IS ExpKind::NonReloc or left_desc->k IS ExpKind::Relocable) {
      // Runtime value - emit extended falsey checks (nil, false, 0, "")
      ExpressionValue left_runtime(fs, *left_desc);
      RegisterAllocator allocator(fs);
      BCReg reg = left_runtime.discharge_to_any_reg(allocator);
      *left_desc = left_runtime.legacy();

      // Create test expressions for extended falsey values
      ExpDesc nilv = make_nil_expr();
      ExpDesc falsev = make_bool_expr(false);
      ExpDesc zerov = make_num_expr(0.0);
      ExpDesc emptyv = make_interned_string_expr(fs->ls->intern_empty_string());

      // Extended falsey check sequence - each equality check jumps to RHS evaluation if matched
      // This implements the ?? operator's extended falsey semantics: nil, false, 0, ""
      // Pattern: ISEQ* + JMP sequence, all jumps collected and patched to RHS evaluation point

      bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));  // Compare to nil
      BCPos check_nil = bcemit_jmp(fs);                            // Jump if equal

      bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev))); // Compare to false
      BCPos check_false = bcemit_jmp(fs);                           // Jump if equal

      bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov))); // Compare to 0
      BCPos check_zero = bcemit_jmp(fs);                               // Jump if equal

      bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv))); // Compare to ""
      BCPos check_empty = bcemit_jmp(fs);                                // Jump if equal

      // Determine destination register
      BCReg dest_reg;
      if (rhs_reg IS NO_REG) {
         dest_reg = fs->freereg;
         allocator.reserve(1);
      }
      else {
         dest_reg = rhs_reg;
         if (dest_reg >= fs->freereg) fs->freereg = dest_reg + 1;
      }

      // Preserve LHS value for truthy path (will be result if none of the checks match)
      bcemit_AD(fs, BC_MOV, dest_reg, reg);

      // If all checks fail (LHS is truthy), skip RHS evaluation
      BCPos skip = bcemit_jmp(fs);

      // Control flow merge point: all falsey checks jump here to evaluate RHS
      // This is where we emit RHS bytecode when LHS matches any extended falsey value
      ControlFlowEdge nil_edge = this->cfg->make_unconditional(check_nil);
      nil_edge.patch_to(fs->pc);
      ControlFlowEdge false_edge = this->cfg->make_unconditional(check_false);
      false_edge.patch_to(fs->pc);
      ControlFlowEdge zero_edge = this->cfg->make_unconditional(check_zero);
      zero_edge.patch_to(fs->pc);
      ControlFlowEdge empty_edge = this->cfg->make_unconditional(check_empty);
      empty_edge.patch_to(fs->pc);

      // Evaluate RHS into dest_reg
      ExpressionValue right_val(fs, *right_desc);
      right_val.to_reg(allocator, dest_reg);
      *right_desc = right_val.legacy();

      // Copy result back to original register if needed
      if (dest_reg != reg) {
         bcemit_AD(fs, BC_MOV, reg, dest_reg);
      }

      // Patch skip jump to here (end of operation)
      ControlFlowEdge skip_edge = this->cfg->make_unconditional(skip);
      skip_edge.patch_to(fs->pc);

      // Set result to LHS register
      ExprFlag saved_flags = left_desc->flags;
      expr_init(left_desc, ExpKind::NonReloc, reg);
      left_desc->flags = saved_flags;

      // Clean up scratch register if used
      if (dest_reg != reg and dest_reg >= fs->nactvar and fs->freereg > dest_reg) {
         fs->freereg = dest_reg;
      }
   }
   else {
      // LHS is compile-time falsey - just use RHS
      *left_desc = *right_desc;
   }
}

//********************************************************************************************************************
// CONCAT operator - preparation phase
// Discharges left operand to next consecutive register for BC_CAT chaining

void OperatorEmitter::prepare_concat(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();
   FuncState* fs = this->func_state;

   // CONCAT requires operands in consecutive registers for BC_CAT instruction
   // The BC_CAT instruction format is: BC_CAT dest, start_reg, end_reg
   // It concatenates all values from start_reg to end_reg

   RegisterAllocator allocator(fs);
   ExpressionValue left_val(fs, *left_desc);
   left_val.to_next_reg(allocator);
   *left_desc = left_val.legacy();
}

//********************************************************************************************************************
// CONCAT operator - completion phase
// Emits BC_CAT instruction with support for chaining multiple concatenations

void OperatorEmitter::complete_concat(ValueSlot left, ValueUse right)
{
   ExpDesc* left_desc = left.raw();
   ExpDesc* right_desc = right.raw();

   FuncState* fs = this->func_state;
   RegisterAllocator allocator(fs);

   // First, convert right operand to val form
   ExpressionValue right_toval(fs, *right_desc);
   right_toval.to_val();
   *right_desc = right_toval.legacy();

   // Check if right operand is already a BC_CAT instruction (for chaining)
   // If so, extend it; otherwise create new BC_CAT
   if (right_desc->k IS ExpKind::Relocable and bc_op(*bcptr(fs, right_desc)) IS BC_CAT) {
      // Chaining case: "a".."b".."c"
      // The previous BC_CAT starts at e1->u.s.info and we extend it
      lj_assertFS(left_desc->u.s.info IS bc_b(*bcptr(fs, right_desc)) - 1, "bad CAT stack layout");
      expr_free(fs, left_desc);
      setbc_b(bcptr(fs, right_desc), left_desc->u.s.info);
      left_desc->u.s.info = right_desc->u.s.info;
   }
   else {
      // New concatenation: emit BC_CAT instruction
      ExpressionValue right_val(fs, *right_desc);
      right_val.to_next_reg(allocator);
      *right_desc = right_val.legacy();

      expr_free(fs, right_desc);
      expr_free(fs, left_desc);

      // Emit BC_CAT: concatenate registers from left->u.s.info to right->u.s.info
      left_desc->u.s.info = bcemit_ABC(fs, BC_CAT, 0, left_desc->u.s.info, right_desc->u.s.info);
   }

   left_desc->k = ExpKind::Relocable;
}
