// Copyright (C) 2025 Paul Manias

#define LUA_CORE

#include "lj_obj.h"
#include "bytecode/lj_bc.h"
#include "parser/lexer.h"

#include <parasol/main.h>

#include "parser/operator_emitter.h"
#include "parser/parse_internal.h"

// Legacy helpers are declared as extern in parse_internal.h

//********************************************************************************************************************
// Helper: Check if operator tracing is enabled

inline bool should_trace_operators(FuncState* fs)
{
   auto prv = (prvFluid *)fs->L->Script->ChildPrivate;
   return (prv->JitOptions & JOF::TRACE_OPERATORS) != JOF::NIL;
}

//********************************************************************************************************************
// Helper: Get operator name for logging

static CSTRING get_binop_name(BinOpr opr)
{
   switch (opr) {
      case OPR_ADD: return "+";
      case OPR_SUB: return "-";
      case OPR_MUL: return "*";
      case OPR_DIV: return "/";
      case OPR_MOD: return "%";
      case OPR_POW: return "^";
      case OPR_CONCAT: return "..";
      case OPR_EQ: return "is";
      case OPR_NE: return "!=";
      case OPR_LT: return "<";
      case OPR_LE: return "<=";
      case OPR_GT: return ">";
      case OPR_GE: return ">=";
      case OPR_AND: return "and";
      case OPR_OR: return "or";
      default: return "?";
   }
}

static CSTRING get_unop_name(BCOp op)
{
   switch (op) {
      case BC_UNM: return "unary -";
      case BC_NOT: return "not";
      case BC_LEN: return "#";
      default: return "?";
   }
}

//********************************************************************************************************************

static CSTRING get_expkind_name(ExpKind k)
{
   switch (k) {
      case ExpKind::Void: return "void";
      case ExpKind::Nil: return "nil";
      case ExpKind::True: return "true";
      case ExpKind::False: return "false";
      case ExpKind::Num: return "num";
      case ExpKind::Str: return "str";
      case ExpKind::CData: return "cdata";
      case ExpKind::Local: return "local";
      case ExpKind::Upval: return "upval";
      case ExpKind::Global: return "global";
      case ExpKind::Indexed: return "indexed";
      case ExpKind::Call: return "call";
      case ExpKind::NonReloc: return "nonreloc";
      case ExpKind::Relocable: return "relocable";
      case ExpKind::Jmp: return "jmp";
      default: return "?";
   }
}

//********************************************************************************************************************

OperatorEmitter::OperatorEmitter(FuncState* State, RegisterAllocator* Allocator, ControlFlowGraph* Cfg)
   : func_state(State), allocator(Allocator), cfg(Cfg)
{
}

//********************************************************************************************************************
// Emit unary operator
// Facade wrapper over legacy bcemit_unop() function

void OperatorEmitter::emit_unary(int op, ValueSlot operand)
{
   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator %s: operand kind=%s", this->func_state->ls->linenumber,
         get_unop_name(BCOp(op)), get_expkind_name(operand.kind()));
   }

   bcemit_unop(this->func_state, BCOp(op), operand.raw());
}

//********************************************************************************************************************
// Emit bitwise NOT operator (~)
// Calls bit.bnot library function

void OperatorEmitter::emit_bitnot(ValueSlot operand)
{
   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator ~: calling bit.bnot, operand kind=%s", this->func_state->ls->linenumber,
         get_expkind_name(operand.kind()));
   }

   bcemit_unary_bit_call(this->func_state, "bnot", operand.raw());
}

//********************************************************************************************************************
// Prepare left operand for binary operation
// MUST be called before evaluating right operand to prevent register clobbering
//
// Logical operators (AND, OR, IF_EMPTY, CONCAT) use specialized prepare_* methods instead.

void OperatorEmitter::emit_binop_left(BinOpr opr, ValueSlot left)
{
   RegisterAllocator local_alloc(this->func_state);
   ExpDesc *e = left.raw();

   if (opr IS OPR_EQ or opr IS OPR_NE) {
      // Comparison operators (EQ, NE): discharge to register unless it's a constant/jump
      if (not e->is_constant_nojump()) {
         ExpressionValue e_value(this->func_state, *e);
         e_value.discharge_to_any_reg(local_alloc);
         *e = e_value.legacy();
      }
   }
   else {
      // Arithmetic and bitwise operators: discharge to register unless it's a numeric constant/jump
      if (not e->is_num_constant_nojump()) {
         ExpressionValue e_value(this->func_state, *e);
         e_value.discharge_to_any_reg(local_alloc);
         *e = e_value.legacy();
      }
   }
}

//********************************************************************************************************************
// Emit arithmetic binary operator
// Facade wrapper over legacy bcemit_arith() function

void OperatorEmitter::emit_binary_arith(BinOpr opr, ValueSlot left, ExpDesc right)
{
   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator %s: left kind=%s, right kind=%s", this->func_state->ls->linenumber,
         get_binop_name(opr), get_expkind_name(left.kind()), get_expkind_name(right.k));
   }

   bcemit_arith(this->func_state, opr, left.raw(), &right);
}

//********************************************************************************************************************
// Emit comparison operator
// Facade wrapper over legacy bcemit_comp() function

void OperatorEmitter::emit_comparison(BinOpr opr, ValueSlot left, ExpDesc right)
{
   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator %s: left kind=%s, right kind=%s", this->func_state->ls->linenumber,
         get_binop_name(opr), get_expkind_name(left.kind()), get_expkind_name(right.k));
   }

   bcemit_comp(this->func_state, opr, left.raw(), &right);
}

//********************************************************************************************************************
// Emit bitwise binary operator
// Bitwise operators emit function calls to bit.* library (bit.lshift, bit.band, etc.)

void OperatorEmitter::emit_binary_bitwise(BinOpr opr, ValueSlot left, ExpDesc right)
{
   CSTRING op_name = priority[opr].name;
   size_t op_name_len = priority[opr].name_len;

   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator %s: calling bit.%.*s, left kind=%s, right kind=%s",
         this->func_state->ls->linenumber, get_binop_name(opr), int(op_name_len), op_name, get_expkind_name(left.kind()),
         get_expkind_name(right.k));
   }

   bcemit_bit_call(this->func_state, std::string_view(op_name, op_name_len), left.raw(), &right);
}

//********************************************************************************************************************
// Prepare logical AND operator (called BEFORE RHS evaluation)

void OperatorEmitter::prepare_logical_and(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();

   // AND short-circuit logic: if left is false, skip RHS and return left (false)
   // If left is true, evaluate RHS and return RHS result

   // Discharge left operand to appropriate form
   ExpressionValue left_val(this->func_state, *left_desc);
   left_val.discharge();
   *left_desc = left_val.legacy();

   BCPOS pc;
   bool will_skip_rhs = false;

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
      will_skip_rhs = true;
   }
   else {
      // Runtime value - emit conditional branch (jump if false)
      pc = bcemit_branch(this->func_state, left_desc, 0);
   }

   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator and: prepare left kind=%s, %s", this->func_state->ls->linenumber, get_expkind_name(left_desc->k),
         will_skip_rhs ? "will skip RHS (constant false)" : "will evaluate RHS");
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

void OperatorEmitter::complete_logical_and(ValueSlot left, ExpDesc right)
{
   ExpDesc *left_desc = left.raw();
   ExpDesc *right_desc = &right;

   // At this point:
   // - left->f contains jumps for "left is false" path
   // - right has been evaluated
   // - We need to merge the false paths and return right's result

   FuncState *fs = this->func_state;  // For lj_assertFS macro
   fs->assert(left_desc->t IS NO_JMP, "jump list not closed");

   // Discharge right operand
   ExpressionValue right_val(this->func_state, *right_desc);
   right_val.discharge();
   *right_desc = right_val.legacy();

   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator and: complete right kind=%s, merging false paths", this->func_state->ls->linenumber,
         get_expkind_name(right_desc->k));
   }

   // Merge false paths: both "left is false" and "right is false" go to same target
   ControlFlowEdge false_edge = this->cfg->make_false_edge(right_desc->f);
   false_edge.append(left_desc->f);
   right_desc->f = false_edge.head();

   // Result is right's value
   *left_desc = *right_desc;
}

//********************************************************************************************************************
// Prepare logical OR operator (called BEFORE RHS evaluation)
// CFG-based implementation using ControlFlowGraph

void OperatorEmitter::prepare_logical_or(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();

   // OR short-circuit logic: if left is true, skip RHS and return left (true)
   // If left is false, evaluate RHS and return RHS result

   // Discharge left operand to appropriate form
   ExpressionValue left_val(this->func_state, *left_desc);
   left_val.discharge();
   *left_desc = left_val.legacy();

   BCPOS pc;
   bool will_skip_rhs = false;

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
      will_skip_rhs = true;
   }
   else { // Runtime value - emit conditional branch (jump if true)
      pc = bcemit_branch(this->func_state, left_desc, 1);
   }

   if (should_trace_operators(this->func_state)) {
      pf::Log("Parser").msg("[%d] operator or: prepare left kind=%s, %s", this->func_state->ls->linenumber,
         get_expkind_name(left_desc->k), will_skip_rhs ? "will skip RHS (constant true)" : "will evaluate RHS");
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
// CFG-based implementation using ControlFlowGraph

void OperatorEmitter::complete_logical_or(ValueSlot left, ExpDesc right)
{
   ExpDesc *left_desc = left.raw();
   ExpDesc *right_desc = &right;

   // At this point:
   // - left->t contains jumps for "left is true" path
   // - right has been evaluated
   // - We need to merge the true paths and return right's result

   FuncState *fs = this->func_state;  // For lj_assertFS macro
   fs->assert(left_desc->f IS NO_JMP, "jump list not closed");

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
// CFG-based implementation with extended falsey semantics

void OperatorEmitter::prepare_if_empty(ValueSlot left)
{
   ExpDesc* left_desc = left.raw();

   // IF_EMPTY short-circuit: if left is truthy, skip RHS and return left
   // Extended falsey: nil, false, 0, "" (all trigger RHS evaluation)

   // Discharge left operand
   ExpressionValue left_val(this->func_state, *left_desc);
   left_val.discharge();
   *left_desc = left_val.legacy();

   BCPOS pc;

   // Handle constant folding for known falsey values
   if (left_desc->k IS ExpKind::Nil or left_desc->k IS ExpKind::False)
      pc = NO_JMP;  // Falsey constant - will evaluate RHS
   else if (left_desc->k IS ExpKind::Num and left_desc->is_num_zero())
      pc = NO_JMP;  // Zero is falsey - will evaluate RHS
   else if (left_desc->k IS ExpKind::Str and left_desc->u.sval and left_desc->u.sval->len IS 0)
      pc = NO_JMP;  // Empty string is falsey - will evaluate RHS
   else if (left_desc->k IS ExpKind::Jmp)
      pc = left_desc->u.s.info;
   else if (left_desc->k IS ExpKind::Str or left_desc->k IS ExpKind::Num or left_desc->k IS ExpKind::True) {
      // Truthy constant - load to register and skip RHS
      RegisterAllocator local_alloc(this->func_state);
      local_alloc.reserve(1);
      expr_toreg_nobranch(this->func_state, left_desc, this->func_state->freereg - 1);
      pc = bcemit_jmp(this->func_state);
   }
   else {
      // Runtime value - emit extended falsey checks NOW (before RHS evaluation)
      // This implements proper short-circuit semantics
      if (!left_desc->is_constant_nojump()) {
         ExpressionValue left_inner(this->func_state, *left_desc);
         RegisterAllocator local_alloc(this->func_state);
         BCREG reg = left_inner.discharge_to_any_reg(local_alloc);
         *left_desc = left_inner.legacy();

         // Create test expressions for extended falsey values
         ExpDesc nilv(ExpKind::Nil);
         ExpDesc falsev(ExpKind::False);
         ExpDesc zerov(ExpKind::Num);
         setnumV(&zerov.u.nval, 0.0);
         ExpDesc emptyv(ExpKind::Str);
         emptyv.u.sval = this->func_state->ls->intern_empty_string();

         // Extended falsey check sequence
         // ISEQ* skips the JMP when values ARE equal (falsey), executes JMP when NOT equal (truthy)
         // Strategy: When value is truthy, NO checks match → all JMPs execute → skip RHS
         //          When value is falsey, ONE check matches → that JMP skipped → fall through to RHS

         bcemit_INS(this->func_state, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
         BCPOS check_nil = bcemit_jmp(this->func_state);

         bcemit_INS(this->func_state, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
         BCPOS check_false = bcemit_jmp(this->func_state);

         bcemit_INS(this->func_state, BCINS_AD(BC_ISEQN, reg, const_num(this->func_state, &zerov)));
         BCPOS check_zero = bcemit_jmp(this->func_state);

         bcemit_INS(this->func_state, BCINS_AD(BC_ISEQS, reg, const_str(this->func_state, &emptyv)));
         BCPOS check_empty = bcemit_jmp(this->func_state);

         // RHS will be emitted after this prepare phase
         // The jumps above will skip RHS when value is truthy (all JMPs execute)
         // Fall through to RHS when value is falsey (one JMP is skipped)

         // Collect all these jumps - they should skip RHS when value is truthy
         pc = check_nil;
         ControlFlowEdge skip_rhs = this->cfg->make_true_edge(pc);
         skip_rhs.append(check_false);
         skip_rhs.append(check_zero);
         skip_rhs.append(check_empty);
         pc = skip_rhs.head();

         // Mark that we need to preserve LHS value and reserve register for RHS
         BCREG rhs_reg = this->func_state->freereg;
         ExprFlag saved_flags = left_desc->flags;
         local_alloc.reserve(1);
         left_desc->init(ExpKind::NonReloc, reg);
         left_desc->u.s.aux = rhs_reg;
         left_desc->flags = saved_flags | ExprFlag::HasRhsReg;
      }
      else {
         pc = NO_JMP;
      }
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
// Extended falsey checks are now emitted in prepare phase for proper short-circuit semantics

void OperatorEmitter::complete_if_empty(ValueSlot left, ExpDesc right)
{
   ExpDesc *left_desc = left.raw();
   ExpDesc *right_desc = &right;

   FuncState* fs = this->func_state;
   fs->assert(left_desc->f IS NO_JMP, "jump list not closed");

   // If left->t has jumps, those are from the extended falsey checks in prepare phase
   // They skip RHS evaluation when LHS is truthy - we need to:
   // 1. Emit RHS materialization code (for falsey path)
   // 2. Patch the truthy jumps to skip all of that

   if (left_desc->t != NO_JMP) {
      // Get the RHS register if one was reserved
      BCREG rhs_reg = NO_REG;
      BCREG lhs_reg = left_desc->u.s.info;
      if (expr_consume_flag(left_desc, ExprFlag::HasRhsReg)) {
         rhs_reg = BCREG(left_desc->u.s.aux);
      }

      // RHS has been evaluated - store it in the reserved register (or allocate one)
      RegisterAllocator local_alloc(fs);
      BCREG dest_reg;
      if (rhs_reg IS NO_REG) {
         dest_reg = fs->freereg;
         local_alloc.reserve(1);
      }
      else {
         dest_reg = rhs_reg;
         if (dest_reg >= fs->freereg) fs->freereg = dest_reg + 1;
      }

      ExpressionValue right_val(fs, *right_desc);
      right_val.to_reg(local_alloc, dest_reg);
      *right_desc = right_val.legacy();

      // Copy RHS result to LHS register (where the result should be)
      if (dest_reg != lhs_reg) bcemit_AD(fs, BC_MOV, lhs_reg, dest_reg);

      // NOW patch the truthy-skip jumps to jump HERE (past all RHS materialization)
      ControlFlowEdge true_edge = this->cfg->make_true_edge(left_desc->t);
      true_edge.patch_to(fs->pc);
      left_desc->t = NO_JMP;

      // Result is in LHS register
      ExprFlag saved_flags = left_desc->flags;
      left_desc->init(ExpKind::NonReloc, lhs_reg);
      left_desc->flags = saved_flags;

      // Clean up scratch register
      if ((dest_reg != lhs_reg) and (dest_reg >= fs->nactvar) and (fs->freereg > dest_reg)) {
         fs->freereg = dest_reg;
      }

      if ((lhs_reg >= fs->nactvar) and (fs->freereg > lhs_reg + 1)) {
         fs->freereg = lhs_reg + 1;
      }
   }
   else {
      // LHS is compile-time falsey - just use RHS
      ExpressionValue right_val(fs, *right_desc);
      right_val.discharge();
      *right_desc = right_val.legacy();
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

   RegisterAllocator local_alloc(fs);
   ExpressionValue left_val(fs, *left_desc);
   left_val.to_next_reg(local_alloc);
   *left_desc = left_val.legacy();
}

//********************************************************************************************************************
// CONCAT operator - completion phase
// Emits BC_CAT instruction with support for chaining multiple concatenations

void OperatorEmitter::complete_concat(ValueSlot left, ExpDesc right)
{
   ExpDesc *left_desc = left.raw();
   ExpDesc *right_desc = &right;

   FuncState *fs = this->func_state;
   RegisterAllocator local_alloc(fs);

   // First, convert right operand to val form
   ExpressionValue right_toval(fs, *right_desc);
   right_toval.to_val();
   *right_desc = right_toval.legacy();

   // Check if right operand is already a BC_CAT instruction (for chaining)
   // If so, extend it; otherwise create new BC_CAT
   if (right_desc->k IS ExpKind::Relocable and bc_op(*bcptr(fs, right_desc)) IS BC_CAT) {
      // Chaining case: "a".."b".."c"
      // The previous BC_CAT starts at e1->u.s.info and we extend it
      fs->assert(left_desc->u.s.info IS bc_b(*bcptr(fs, right_desc)) - 1, "bad CAT stack layout");
      expr_free(fs, left_desc);
      setbc_b(bcptr(fs, right_desc), left_desc->u.s.info);
      left_desc->u.s.info = right_desc->u.s.info;
   }
   else {
      // New concatenation: emit BC_CAT instruction
      ExpressionValue right_val(fs, *right_desc);
      right_val.to_next_reg(local_alloc);
      *right_desc = right_val.legacy();

      expr_free(fs, right_desc);
      expr_free(fs, left_desc);

      // Emit BC_CAT: concatenate registers from left->u.s.info to right->u.s.info
      left_desc->u.s.info = bcemit_ABC(fs, BC_CAT, 0, left_desc->u.s.info, right_desc->u.s.info);
   }

   left_desc->k = ExpKind::Relocable;
}

//********************************************************************************************************************
// Presence check operator (x?)
// Returns boolean: true if value is truthy, false if falsey (nil, false, 0, "")

void OperatorEmitter::emit_presence_check(ValueSlot operand)
{
   ExpDesc* e = operand.raw();
   FuncState* fs = this->func_state;

   // Discharge the operand first
   ExpressionValue e_value(fs, *e);
   e_value.discharge();
   *e = e_value.legacy();

   // Handle compile-time constants
   if (e->k IS ExpKind::Nil or e->k IS ExpKind::False) {
      e->init(ExpKind::False, 0);  // Falsey constant
      return;
   }

   if (e->k IS ExpKind::Num and e->is_num_zero()) {
      e->init(ExpKind::False, 0);  // Zero is falsey
      return;
   }

   if (e->k IS ExpKind::Str and e->u.sval and e->u.sval->len IS 0) {
      e->init(ExpKind::False, 0);  // Empty string is falsey
      return;
   }

   if (e->k IS ExpKind::True or (e->k IS ExpKind::Num and !e->is_num_zero()) or
       (e->k IS ExpKind::Str and e->u.sval and e->u.sval->len > 0)) {
      e->init(ExpKind::True, 0);  // Truthy constant
      return;
   }

   // Runtime value - emit extended falsey checks
   RegisterAllocator local_alloc(fs);
   ExpressionValue e_runtime(fs, *e);
   BCREG reg = e_runtime.discharge_to_any_reg(local_alloc);
   *e = e_runtime.legacy();

   // Create test expressions
   ExpDesc nilv(ExpKind::Nil);
   ExpDesc falsev(ExpKind::False);
   ExpDesc zerov(0.0);
   ExpDesc emptyv(fs->ls->intern_empty_string());

   // Emit equality checks for extended falsey values
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
   BCPOS check_nil = bcemit_jmp(fs);

   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
   BCPOS check_false = bcemit_jmp(fs);

   bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
   BCPOS check_zero = bcemit_jmp(fs);

   bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
   BCPOS check_empty = bcemit_jmp(fs);

   expr_free(fs, e);  // Free the expression register

   // Reserve register for result
   BCREG dest = fs->freereg;
   local_alloc.reserve(1);

   // Value is truthy - load true
   bcemit_AD(fs, BC_KPRI, dest, BCREG(ExpKind::True));
   BCPOS jmp_false_branch = bcemit_jmp(fs);

   // False branch: patch all falsey jumps here and load false
   BCPOS false_pos = fs->pc;
   ControlFlowEdge nil_edge = this->cfg->make_unconditional(check_nil);
   nil_edge.patch_to(false_pos);
   ControlFlowEdge false_edge_check = this->cfg->make_unconditional(check_false);
   false_edge_check.patch_to(false_pos);
   ControlFlowEdge zero_edge = this->cfg->make_unconditional(check_zero);
   zero_edge.patch_to(false_pos);
   ControlFlowEdge empty_edge = this->cfg->make_unconditional(check_empty);
   empty_edge.patch_to(false_pos);

   bcemit_AD(fs, BC_KPRI, dest, BCREG(ExpKind::False));

   // Patch skip jump to after false load
   ControlFlowEdge skip_edge = this->cfg->make_unconditional(jmp_false_branch);
   skip_edge.patch_to(fs->pc);

   e->init(ExpKind::NonReloc, dest);
}
