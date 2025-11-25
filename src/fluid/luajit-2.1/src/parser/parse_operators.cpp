// Lua parser - Operator bytecode emission.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#include "parser/operator_emitter.h"
#include "parser/parse_control_flow.h"

//********************************************************************************************************************
// Bytecode emitter for operators

// Try constant-folding of arithmetic operators.

[[nodiscard]] int foldarith(BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   TValue o;
   lua_Number n;
   if (!e1->is_num_constant_nojump() or !e2->is_num_constant_nojump()) [[likely]] return 0;
   n = lj_vm_foldarith(e1->number_value(), e2->number_value(), int(opr) - OPR_ADD);
   setnumV(&o, n);
   if (tvisnan(&o) or tvismzero(&o)) [[unlikely]] return 0;  // Avoid NaN and -0 as consts.
   if (LJ_DUALNUM) {
      int32_t k = lj_num2int(n);
      if (lua_Number(k) IS n) {
         setintV(&e1->u.nval, k);
         return 1;
      }
   }
   setnumV(&e1->u.nval, n);
   return 1;
}

//********************************************************************************************************************
// Emit arithmetic operator.

void bcemit_arith(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   RegisterAllocator allocator(fs);
   BCREG rb, rc, t;
   uint32_t op;
   if (foldarith(opr, e1, e2)) return;

   if (opr IS OPR_POW) {
      op = BC_POW;
      ExpressionValue e2_value(fs, *e2);
      rc = e2_value.discharge_to_any_reg(allocator);
      *e2 = e2_value.legacy();
      ExpressionValue e1_value(fs, *e1);
      rb = e1_value.discharge_to_any_reg(allocator);
      *e1 = e1_value.legacy();
   }
   else {
      op = opr - OPR_ADD + BC_ADDVV;
      // Must discharge 2nd operand first since ExpKind::Indexed might free regs.
      ExpressionValue e2_toval(fs, *e2);
      e2_toval.to_val();
      *e2 = e2_toval.legacy();
      if (e2->is_num_constant() and (rc = const_num(fs, e2)) <= BCMAX_C) op -= BC_ADDVV - BC_ADDVN;
      else {
         ExpressionValue e2_value(fs, *e2);
         rc = e2_value.discharge_to_any_reg(allocator);
         *e2 = e2_value.legacy();
      }

      // 1st operand discharged by bcemit_binop_left, but need KNUM/KSHORT.

      fs->assert(e1->is_num_constant() or e1->k IS ExpKind::NonReloc, "bad expr type %d", e1->k);
      ExpressionValue e1_toval(fs, *e1);
      e1_toval.to_val();
      *e1 = e1_toval.legacy();

      // Avoid two consts to satisfy bytecode constraints.

      if (e1->is_num_constant() and (not e2->is_num_constant()) and (t = const_num(fs, e1)) <= BCMAX_B) {
         rb = rc; rc = t; op -= BC_ADDVV - BC_ADDNV;
      }
      else {
         ExpressionValue e1_value(fs, *e1);
         rb = e1_value.discharge_to_any_reg(allocator);
         *e1 = e1_value.legacy();
      }
   }

   // Release operand registers through allocator
   allocator.release_expression(e2);
   allocator.release_expression(e1);
   e1->u.s.info = bcemit_ABC(fs, op, 0, rb, rc);
   e1->k = ExpKind::Relocable;
}

//********************************************************************************************************************
// Emit comparison operator.

void bcemit_comp(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
   RegisterAllocator allocator(fs);
   ExpDesc *eret = e1;
   BCIns ins;
   BCREG cmp_reg_a = NO_REG, cmp_reg_b = NO_REG;  // Track registers used by comparison
   ExpressionValue e1_toval_pre(fs, *e1);

   e1_toval_pre.to_val();
   *e1 = e1_toval_pre.legacy();
   if (opr IS OPR_EQ or opr IS OPR_NE) {
      BCOp op = opr IS OPR_EQ ? BC_ISEQV : BC_ISNEV;
      BCREG ra;

      if (e1->is_constant()) { e1 = e2; e2 = eret; }  // Need constant in 2nd arg.
      ExpressionValue e1_value(fs, *e1);
      ra = e1_value.discharge_to_any_reg(allocator);  // First arg must be in a reg.
      *e1 = e1_value.legacy();
      cmp_reg_a = ra;
      ExpressionValue e2_toval(fs, *e2);
      e2_toval.to_val();
      *e2 = e2_toval.legacy();

      switch (e2->k) {
         case ExpKind::Nil:
         case ExpKind::False:
         case ExpKind::True:
            ins = BCINS_AD(op + (BC_ISEQP - BC_ISEQV), ra, const_pri(e2));
            break;
         case ExpKind::Str:
            ins = BCINS_AD(op + (BC_ISEQS - BC_ISEQV), ra, const_str(fs, e2));
            break;
         case ExpKind::Num:
            ins = BCINS_AD(op + (BC_ISEQN - BC_ISEQV), ra, const_num(fs, e2));
            break;
         default: {
            ExpressionValue e2_value(fs, *e2);
            BCREG rb = e2_value.discharge_to_any_reg(allocator);
            *e2 = e2_value.legacy();
            cmp_reg_b = rb;
            ins = BCINS_AD(op, ra, rb);
            break;
         }
      }
   }
   else {
      uint32_t op = opr - OPR_LT + BC_ISLT;
      BCREG ra, rd;
      if ((op - BC_ISLT) & 1) {  // GT -> LT, GE -> LE
         e1 = e2; e2 = eret;  // Swap operands.
         op = ((op - BC_ISLT) ^ 3) + BC_ISLT;
         ExpressionValue e1_toval_swap(fs, *e1);
         e1_toval_swap.to_val();
         *e1 = e1_toval_swap.legacy();
         ExpressionValue e1_value(fs, *e1);
         ra = e1_value.discharge_to_any_reg(allocator);
         *e1 = e1_value.legacy();
         ExpressionValue e2_value(fs, *e2);
         rd = e2_value.discharge_to_any_reg(allocator);
         *e2 = e2_value.legacy();
      }
      else {
         ExpressionValue e2_value(fs, *e2);
         rd = e2_value.discharge_to_any_reg(allocator);
         *e2 = e2_value.legacy();
         ExpressionValue e1_value(fs, *e1);
         ra = e1_value.discharge_to_any_reg(allocator);
         *e1 = e1_value.legacy();
      }
      cmp_reg_a = ra;
      cmp_reg_b = rd;
      ins = BCINS_AD(op, ra, rd);
   }

   // Emit the comparison instruction now that operands are prepared.
   bcemit_INS(fs, ins);

   // Explicitly release operand registers through the allocator.
   // Release in LIFO order (highest register first) to maximise the chance of
   // collapsing freereg when both operands are adjacent temporaries.
   if (cmp_reg_b != NO_REG and cmp_reg_b > cmp_reg_a) {
      allocator.release_register(cmp_reg_b);
      allocator.release_register(cmp_reg_a);
   }
   else {
      allocator.release_register(cmp_reg_a);
      if (cmp_reg_b != NO_REG) allocator.release_register(cmp_reg_b);
   }

   // Produce a Jmp expression as the result of the comparison, preserving
   // existing short-circuit and conditional semantics.
   eret->u.s.info = bcemit_jmp(fs);
   eret->k = ExpKind::Jmp;
}


//********************************************************************************************************************
// Emit a call to a bit library function (bit.lshift, bit.rshift, etc.) at a specific base register.
//
// This function is used to implement C-style bitwise shift operators (<<, >>) by translating them
// into calls to LuaJIT's bit library functions. The base register is explicitly provided to allow
// chaining of multiple shift operations while reusing the same register for intermediate results.
//
// Register Layout (x64 with LJ_FR2=1):
//   base     - Function to call (bit.lshift, bit.rshift, etc.)
//   base+1   - Frame link register (LJ_FR2, not an argument)
//   base+2   - arg1: First operand (value to shift)
//   base+3   - arg2: Second operand (shift count)
//
// BC_CALL Instruction Format:
//   - A field: base register
//   - B field: Call type (2 for regular calls, 0 for varargs)
//   - C field: Argument count = freereg - base - LJ_FR2
//
// ExpKind::Call Handling (Multi-Return Functions):
//   When RHS is a ExpKind::Call (function call with multiple return values), standard Lua binary operator
//   semantics apply: only the first return value is used. The ExpKind::Call is discharged before being
//   passed as an argument. This matches the behavior of expressions like `x + f()` in Lua.
//
//   Note: Unlike function argument lists (which use BC_CALLM to forward all return values),
//   binary operators always restrict multi-return expressions to single values. This is a
//   fundamental Lua language semantic, not a limitation of this implementation.
//
// Parameters:
//   fs    - Function state for bytecode generation
//   fname - Name of bit library function (e.g., "lshift", "rshift")
//   fname_len - Length of fname string
//   lhs   - Left-hand side expression (value to shift)
//   rhs   - Right-hand side expression (shift count, may be ExpKind::Call)
//   base  - Base register for the call (allows register reuse for chaining)

static void bcemit_shift_call_at_base(FuncState* fs, std::string_view fname, ExpDesc* lhs, ExpDesc* rhs, BCREG base)
{
   RegisterAllocator allocator(fs);
   ExpDesc callee, key;
   BCREG arg1 = base + 1 + LJ_FR2;  // First argument register (after frame link if present)
   BCREG arg2 = arg1 + 1;            // Second argument register

   // Normalise both operands into registers before loading the callee.
   ExpressionValue lhs_toval(fs, *lhs);
   lhs_toval.to_val();
   *lhs = lhs_toval.legacy();
   ExpressionValue rhs_toval(fs, *rhs);
   rhs_toval.to_val();
   *rhs = rhs_toval.legacy();
   ExpressionValue lhs_value(fs, *lhs);
   lhs_value.to_reg(allocator, arg1);
   *lhs = lhs_value.legacy();
   ExpressionValue rhs_value(fs, *rhs);
   rhs_value.to_reg(allocator, arg2);
   *rhs = rhs_value.legacy();

   // Now load bit.[lshift|rshift|...] into the base register
   callee.init(ExpKind::Global, 0);
   callee.u.sval = fs->ls->keepstr("bit");
   ExpressionValue callee_value(fs, callee);
   callee_value.discharge_to_any_reg(allocator);
   callee = callee_value.legacy();
   key.init(ExpKind::Str, 0);
   key.u.sval = fs->ls->keepstr(fname);
   expr_index(fs, &callee, &key);
   ExpressionValue callee_toval(fs, callee);
   callee_toval.to_val();
   callee = callee_toval.legacy();
   ExpressionValue callee_value2(fs, callee);
   callee_value2.to_reg(allocator, base);
   callee = callee_value2.legacy();

   // Emit CALL instruction
   fs->freereg = arg2 + 1;  // Ensure freereg covers all arguments
   lhs->k = ExpKind::Call;
   lhs->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
   lhs->u.s.aux = base;
   fs->freereg = base + 1;

   ExpressionValue lhs_value_discharge(fs, *lhs);
   lhs_value_discharge.discharge();
   *lhs = lhs_value_discharge.legacy();
   fs->assert(lhs->k IS ExpKind::NonReloc and lhs->u.s.info IS base, "bitwise result not in base register");
}

//********************************************************************************************************************

static void bcemit_bit_call(FuncState* fs, std::string_view fname, ExpDesc* lhs, ExpDesc* rhs)
{
   RegisterAllocator allocator(fs);
   // Allocate a base register for the call
   // Check if either operand is already at the top of the stack to avoid orphaning registers
   // when chaining operations (e.g., 1 | 2 | 4 produces AST: (1 | 2) | 4, so LHS is the previous result)
   BCREG base;
   if (rhs->k IS ExpKind::NonReloc and rhs->u.s.info >= fs->nactvar and rhs->u.s.info + 1 IS fs->freereg) {
      // RHS is at the top - reuse its register to avoid orphaning
      base = rhs->u.s.info;
   }
   else if (lhs->k IS ExpKind::NonReloc and lhs->u.s.info >= fs->nactvar and lhs->u.s.info + 1 IS fs->freereg) {
      // LHS is at the top - reuse its register to avoid orphaning
      base = lhs->u.s.info;
   }
   else base = fs->freereg;

   allocator.reserve(1);  // Reserve for callee
   if (LJ_FR2) allocator.reserve(1);
   allocator.reserve(2);  // Reserve for arguments
   fs->assert(!fname.empty(), "bitlib name missing for bitwise operator");
   bcemit_shift_call_at_base(fs, fname, lhs, rhs, base);
}

//********************************************************************************************************************
// Emit unary bit library call (e.g., bit.bnot).
// Exported for use by OperatorEmitter facade

void bcemit_unary_bit_call(FuncState* fs, std::string_view fname, ExpDesc* arg)
{
   RegisterAllocator allocator(fs);
   ExpDesc callee, key;
   BCREG base = fs->freereg;
   BCREG arg_reg = base + 1 + LJ_FR2;

   allocator.reserve(1);  // Reserve for callee
   if (LJ_FR2) allocator.reserve(1);  // Reserve for frame link on x64

   // Place argument in register.
   ExpressionValue arg_toval(fs, *arg);
   arg_toval.to_val();
   *arg = arg_toval.legacy();
   ExpressionValue arg_value(fs, *arg);
   arg_value.to_reg(allocator, arg_reg);
   *arg = arg_value.legacy();

   // Ensure freereg accounts for argument register so it's not clobbered.
   if (fs->freereg <= arg_reg) fs->freereg = arg_reg + 1;

   // Load bit.fname into base register.
   callee.init(ExpKind::Global, 0);
   callee.u.sval = fs->ls->keepstr("bit");
   ExpressionValue callee_value(fs, callee);
   callee_value.discharge_to_any_reg(allocator);
   callee = callee_value.legacy();
   key.init(ExpKind::Str, 0);
   key.u.sval = fs->ls->keepstr(fname);
   expr_index(fs, &callee, &key);
   ExpressionValue callee_toval2(fs, callee);
   callee_toval2.to_val();
   callee = callee_toval2.legacy();
   ExpressionValue callee_value2(fs, callee);
   callee_value2.to_reg(allocator, base);
   callee = callee_value2.legacy();

   // Emit CALL instruction.
   fs->freereg = arg_reg + 1;
   arg->k = ExpKind::Call;
   arg->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
   arg->u.s.aux = base;
   fs->freereg = base + 1;

   // Discharge result to register.
   ExpressionValue arg_value_discharge(fs, *arg);
   arg_value_discharge.discharge();
   *arg = arg_value_discharge.legacy();
   fs->assert(arg->k IS ExpKind::NonReloc and arg->u.s.info IS base, "bitwise result not in base register");
}

//********************************************************************************************************************
// Emit unary operator.

void bcemit_unop(FuncState* fs, BCOp op, ExpDesc* e)
{
   RegisterAllocator allocator(fs);

   if (op IS BC_NOT) {
      // Swap true and false lists.
      { BCPOS temp = e->f; e->f = e->t; e->t = temp; }
      ControlFlowGraph cfg(fs);
      ControlFlowEdge false_edge = cfg.make_false_edge(e->f);
      false_edge.drop_values();
      ControlFlowEdge true_edge = cfg.make_true_edge(e->t);
      true_edge.drop_values();
      ExpressionValue e_value(fs, *e);
      e_value.discharge();
      *e = e_value.legacy();
      if (e->k IS ExpKind::Nil or e->k IS ExpKind::False) {
         e->k = ExpKind::True;
         return;
      }
      else if (e->is_constant() or (LJ_HASFFI and e->k IS ExpKind::CData)) {
         e->k = ExpKind::False;
         return;
      }
      else if (e->k IS ExpKind::Jmp) {
         invertcond(fs, e);
         return;
      }
      else if (e->k IS ExpKind::Relocable) {
         allocator.reserve(1);
         setbc_a(bcptr(fs, e), fs->freereg - 1);
         e->u.s.info = fs->freereg - 1;
         e->k = ExpKind::NonReloc;
      }
      else fs->assert(e->k IS ExpKind::NonReloc, "bad expr type %d", int(e->k));
   }
   else {
      fs->assert(op IS BC_UNM or op IS BC_LEN, "bad unop %d", op);
      if (op IS BC_UNM and not e->has_jump()) {  // Constant-fold negations.
#if LJ_HASFFI
         if (e->k IS ExpKind::CData) {  // Fold in-place since cdata is not interned.
            GCcdata* cd = cdataV(&e->u.nval);
            int64_t* p = (int64_t*)cdataptr(cd);
            if (cd->ctypeid IS CTID_COMPLEX_DOUBLE) p[1] ^= (int64_t)U64x(80000000, 00000000);
            else *p = -*p;
            return;
         }
         else
#endif
            if (e->is_num_constant() and !e->is_num_zero()) {  // Avoid folding to -0.
               TValue* o = e->num_tv();
               if (tvisint(o)) {
                  int32_t k = intV(o);
                  if (k IS -k) setnumV(o, -lua_Number(k));
                  else setintV(o, -k);
                  return;
               }
               else {
                  o->u64 ^= U64x(80000000, 00000000);
                  return;
               }
            }
      }
      ExpressionValue e_value(fs, *e);
      e_value.discharge_to_any_reg(allocator);
      *e = e_value.legacy();
   }
   expr_free(fs, e);
   e->u.s.info = bcemit_AD(fs, op, 0, e->u.s.info);
   e->k = ExpKind::Relocable;
}
