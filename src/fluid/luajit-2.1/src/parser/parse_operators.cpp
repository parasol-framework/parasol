// Lua parser - Operator bytecode emission.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

//********************************************************************************************************************
// Bytecode emitter for operators

// Try constant-folding of arithmetic operators.

[[nodiscard]] static int foldarith(BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   TValue o;
   lua_Number n;
   if (!expr_isnumk_nojump(e1) or !expr_isnumk_nojump(e2)) [[likely]] return 0;
   n = lj_vm_foldarith(expr_numberV(e1), expr_numberV(e2), int(opr) - OPR_ADD);
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

static void bcemit_arith(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   BCReg rb, rc, t;
   uint32_t op;
   if (foldarith(opr, e1, e2)) return;

   if (opr IS OPR_POW) {
      op = BC_POW;
      rc = expr_toanyreg(fs, e2);
      rb = expr_toanyreg(fs, e1);
   }
   else {
      op = opr - OPR_ADD + BC_ADDVV;
      // Must discharge 2nd operand first since ExpKind::Indexed might free regs.
      expr_toval(fs, e2);
      if (expr_isnumk(e2) and (rc = const_num(fs, e2)) <= BCMAX_C) op -= BC_ADDVV - BC_ADDVN;
      else rc = expr_toanyreg(fs, e2);

      // 1st operand discharged by bcemit_binop_left, but need KNUM/KSHORT.

      lj_assertFS(expr_isnumk(e1) or e1->k IS ExpKind::NonReloc, "bad expr type %d", e1->k);
      expr_toval(fs, e1);

      // Avoid two consts to satisfy bytecode constraints.

      if (expr_isnumk(e1) and !expr_isnumk(e2) and
         (t = const_num(fs, e1)) <= BCMAX_B) {
         rb = rc; rc = t; op -= BC_ADDVV - BC_ADDNV;
      }
      else rb = expr_toanyreg(fs, e1);
   }

   // Using expr_free might cause asserts if the order is wrong.
   if (e1->k IS ExpKind::NonReloc and e1->u.s.info >= fs->nactvar) fs->freereg--;
   if (e2->k IS ExpKind::NonReloc and e2->u.s.info >= fs->nactvar) fs->freereg--;
   e1->u.s.info = bcemit_ABC(fs, op, 0, rb, rc);
   e1->k = ExpKind::Relocable;
}

//********************************************************************************************************************
// Emit comparison operator.

static void bcemit_comp(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   ExpDesc* eret = e1;
   BCIns ins;
   expr_toval(fs, e1);
   if (opr IS OPR_EQ or opr IS OPR_NE) {
      BCOp op = opr IS OPR_EQ ? BC_ISEQV : BC_ISNEV;
      BCReg ra;
      if (expr_isk(e1)) { e1 = e2; e2 = eret; }  // Need constant in 2nd arg.
      ra = expr_toanyreg(fs, e1);  // First arg must be in a reg.
      expr_toval(fs, e2);
      switch (e2->k) {
      case ExpKind::Nil: case ExpKind::False: case ExpKind::True:
         ins = BCINS_AD(op + (BC_ISEQP - BC_ISEQV), ra, const_pri(e2));
         break;
      case ExpKind::Str:
         ins = BCINS_AD(op + (BC_ISEQS - BC_ISEQV), ra, const_str(fs, e2));
         break;
      case ExpKind::Num:
         ins = BCINS_AD(op + (BC_ISEQN - BC_ISEQV), ra, const_num(fs, e2));
         break;
      default:
         ins = BCINS_AD(op, ra, expr_toanyreg(fs, e2));
         break;
      }
   }
   else {
      uint32_t op = opr - OPR_LT + BC_ISLT;
      BCReg ra, rd;
      if ((op - BC_ISLT) & 1) {  // GT -> LT, GE -> LE
         e1 = e2; e2 = eret;  // Swap operands.
         op = ((op - BC_ISLT) ^ 3) + BC_ISLT;
         expr_toval(fs, e1);
         ra = expr_toanyreg(fs, e1);
         rd = expr_toanyreg(fs, e2);
      }
      else {
         rd = expr_toanyreg(fs, e2);
         ra = expr_toanyreg(fs, e1);
      }
      ins = BCINS_AD(op, ra, rd);
   }
   // Using expr_free might cause asserts if the order is wrong.
   if (e1->k IS ExpKind::NonReloc and e1->u.s.info >= fs->nactvar) fs->freereg--;
   if (e2->k IS ExpKind::NonReloc and e2->u.s.info >= fs->nactvar) fs->freereg--;
   bcemit_INS(fs, ins);
   eret->u.s.info = bcemit_jmp(fs);
   eret->k = ExpKind::Jmp;
}

//********************************************************************************************************************
// Fixup left side of binary operator.

static void bcemit_binop_left(FuncState* fs, BinOpr op, ExpDesc* e)
{
   if (op IS OPR_AND) {
      bcemit_branch_t(fs, e);
   }
   else if (op IS OPR_OR) {
      bcemit_branch_f(fs, e);
   }
   else if (op IS OPR_IF_EMPTY) {
      // For ?, handle extended falsey checks - only set up jumps for compile-time constants
      BCPos pc;

      expr_discharge(fs, e);
      // Extended falsey: nil, false, 0, ""
      if (e->k IS ExpKind::Nil or e->k IS ExpKind::False)
         pc = NO_JMP;  // Never jump - these are falsey, evaluate RHS
      else if (e->k IS ExpKind::Num and expr_numiszero(e))
         pc = NO_JMP;  // Zero is falsey, evaluate RHS
      else if (e->k IS ExpKind::Str and e->u.sval and e->u.sval->len IS 0)
         pc = NO_JMP;  // Empty string is falsey, evaluate RHS
      else if (e->k IS ExpKind::Jmp)
         pc = e->u.s.info;
      else if (e->k IS ExpKind::Str or e->k IS ExpKind::Num or e->k IS ExpKind::True) {
         // Truthy constant - load to register and emit jump to skip RHS
         bcreg_reserve(fs, 1);
         expr_toreg_nobranch(fs, e, fs->freereg - 1);
         pc = bcemit_jmp(fs);
      }
      else {
         // Runtime value - do NOT use bcemit_branch() as it uses standard Lua truthiness
         // Ensure the value resides in a dedicated register so that RHS evaluation cannot
         // clobber it. Keep a copy of the original value even if the source lives in an
         // active local slot.

         if (!expr_isk_nojump(e)) {
            BCReg src_reg = expr_toanyreg(fs, e);
            BCReg rhs_reg = fs->freereg;
            ExprFlag saved_flags = e->flags;
            bcreg_reserve(fs, 1);
            expr_init(e, ExpKind::NonReloc, src_reg);
            e->u.s.aux = rhs_reg;
            e->flags = saved_flags | ExprFlag::HasRhsReg;
         }
         pc = NO_JMP;  // No jump - will check extended falsey in bcemit_binop()
      }
      e->t = JumpListView(fs, e->t).append(pc);
      JumpListView(fs, e->f).patch_to_here();
      e->f = NO_JMP;
   }
   else if (op IS OPR_CONCAT) {
      expr_tonextreg(fs, e);
   }
   else if (op IS OPR_EQ or op IS OPR_NE) {
      if (!expr_isk_nojump(e)) expr_toanyreg(fs, e);
   }
   else {
      if (!expr_isnumk_nojump(e)) expr_toanyreg(fs, e);
   }
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

static void bcemit_shift_call_at_base(FuncState* fs, std::string_view fname, ExpDesc* lhs, ExpDesc* rhs, BCReg base)
{
   ExpDesc callee, key;
   BCReg arg1 = base + 1 + LJ_FR2;  // First argument register (after frame link if present)
   BCReg arg2 = arg1 + 1;            // Second argument register

   // Normalise both operands into registers before loading the callee.
   expr_toval(fs, lhs);
   expr_toval(fs, rhs);
   expr_toreg(fs, lhs, arg1);
   expr_toreg(fs, rhs, arg2);

   // Now load bit.[lshift|rshift|...] into the base register
   expr_init(&callee, ExpKind::Global, 0);
   callee.u.sval = fs->ls->keepstr("bit");
   expr_toanyreg(fs, &callee);
   expr_init(&key, ExpKind::Str, 0);
   key.u.sval = fs->ls->keepstr(fname);
   expr_index(fs, &callee, &key);
   expr_toval(fs, &callee);
   expr_toreg(fs, &callee, base);

   // Emit CALL instruction
   fs->freereg = arg2 + 1;  // Ensure freereg covers all arguments
   lhs->k = ExpKind::Call;
   lhs->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
   lhs->u.s.aux = base;
   fs->freereg = base + 1;

   expr_discharge(fs, lhs);
   lj_assertFS(lhs->k IS ExpKind::NonReloc and lhs->u.s.info IS base, "bitwise result not in base register");
}

//********************************************************************************************************************

static void bcemit_bit_call(FuncState* fs, std::string_view fname, ExpDesc* lhs, ExpDesc* rhs)
{
   // Allocate a base register for the call
   // Check if either operand is already at the top of the stack to avoid orphaning registers
   // when chaining operations (e.g., 1 | 2 | 4 produces AST: (1 | 2) | 4, so LHS is the previous result)
   BCReg base;
   if (rhs->k IS ExpKind::NonReloc and rhs->u.s.info >= fs->nactvar and
       rhs->u.s.info + 1 IS fs->freereg) {
      // RHS is at the top - reuse its register to avoid orphaning
      base = rhs->u.s.info;
   }
   else if (lhs->k IS ExpKind::NonReloc and lhs->u.s.info >= fs->nactvar and
            lhs->u.s.info + 1 IS fs->freereg) {
      // LHS is at the top - reuse its register to avoid orphaning
      base = lhs->u.s.info;
   }
   else {
      base = fs->freereg;
   }
   bcreg_reserve(fs, 1);  // Reserve for callee
   if (LJ_FR2) bcreg_reserve(fs, 1);
   bcreg_reserve(fs, 2);  // Reserve for arguments
   lj_assertFS(!fname.empty(), "bitlib name missing for bitwise operator");
   bcemit_shift_call_at_base(fs, fname, lhs, rhs, base);
}

//********************************************************************************************************************
// Emit unary bit library call (e.g., bit.bnot).

static void bcemit_unary_bit_call(FuncState* fs, std::string_view fname, ExpDesc* arg)
{
   ExpDesc callee, key;
   BCReg base = fs->freereg;
   BCReg arg_reg = base + 1 + LJ_FR2;

   bcreg_reserve(fs, 1);  // Reserve for callee
   if (LJ_FR2) bcreg_reserve(fs, 1);  // Reserve for frame link on x64

   // Place argument in register.
   expr_toval(fs, arg);
   expr_toreg(fs, arg, arg_reg);

   // Ensure freereg accounts for argument register so it's not clobbered.
   if (fs->freereg <= arg_reg) fs->freereg = arg_reg + 1;

   // Load bit.fname into base register.
   expr_init(&callee, ExpKind::Global, 0);
   callee.u.sval = fs->ls->keepstr("bit");
   expr_toanyreg(fs, &callee);
   expr_init(&key, ExpKind::Str, 0);
   key.u.sval = fs->ls->keepstr(fname);
   expr_index(fs, &callee, &key);
   expr_toval(fs, &callee);
   expr_toreg(fs, &callee, base);

   // Emit CALL instruction.
   fs->freereg = arg_reg + 1;
   arg->k = ExpKind::Call;
   arg->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
   arg->u.s.aux = base;
   fs->freereg = base + 1;

   // Discharge result to register.
   expr_discharge(fs, arg);
   lj_assertFS(arg->k IS ExpKind::NonReloc and arg->u.s.info IS base, "bitwise result not in base register");
}

//********************************************************************************************************************
// Emit bytecode for postfix presence check operator (x?).
// Returns boolean: true if value is truthy (extended falsey semantics),
// false if value is falsey (nil, false, 0, "").

static void bcemit_presence_check(FuncState* fs, ExpDesc* e)
{
   expr_discharge(fs, e);

   // Handle compile-time constants
   if (e->k IS ExpKind::Nil or e->k IS ExpKind::False) { // Falsey constant - set to false
      expr_init(e, ExpKind::False, 0);
      return;
   }

   if (e->k IS ExpKind::Num and expr_numiszero(e)) { // Zero is falsey - set to false
      expr_init(e, ExpKind::False, 0);
      return;
   }

   if (e->k IS ExpKind::Str and e->u.sval and e->u.sval->len IS 0) {
      // Empty string is falsey - set to false
      expr_init(e, ExpKind::False, 0);
      return;
   }

   if (e->k IS ExpKind::True or (e->k IS ExpKind::Num and !expr_numiszero(e)) or
      (e->k IS ExpKind::Str and e->u.sval and e->u.sval->len > 0)) {
      // Truthy constant - set to true
      expr_init(e, ExpKind::True, 0);
      return;
   }

   // Runtime value - emit checks
   // Follow `?` pattern: use BC_ISEQP/BC_ISEQN/BC_ISEQS, patch jumps to false branch

   // Bytecode semantics: BC_ISEQP/BC_ISEQN/BC_ISEQS skip the next instruction when values ARE equal.
   // Pattern: BC_ISEQP reg, ExpKind::Nil + JMP means:
   //   - If reg IS nil: skip JMP, continue to next check
   //   - If reg != nil: execute JMP, jump to target (patched to false branch)
   // By chaining multiple checks and patching all JMPs to the same false branch:
   //   - Falsey values: matching check skips its JMP, execution continues (reaches truthy branch)
   //   - Truthy values: all checks fail, first JMP executes, jumps to false branch

   BCReg reg = expr_toanyreg(fs, e);
   ExpDesc nilv = make_nil_expr();
   ExpDesc falsev = make_bool_expr(false);
   ExpDesc zerov = make_num_expr(0.0);
   ExpDesc emptyv = make_interned_string_expr(fs->ls->intern_empty_string());
   BCPos jmp_false_branch;
   BCPos check_nil, check_false, check_zero, check_empty;

   // Check for nil
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
   check_nil = bcemit_jmp(fs);

   // Check for false
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
   check_false = bcemit_jmp(fs);

   // Check for zero
   bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
   check_zero = bcemit_jmp(fs);

   // Check for empty string
   bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
   check_empty = bcemit_jmp(fs);

   // Reserve a register for the result
   BCReg dest = fs->freereg;
   bcreg_reserve(fs, 1);

   // Free the old expression register after reserving new one
   expr_free(fs, e);

   // If all checks pass (value is truthy), load true
   bcemit_AD(fs, BC_KPRI, dest, BCReg(ExpKind::True));
   jmp_false_branch = bcemit_jmp(fs);

   // False branch: patch all falsey jumps here and load false
   BCPos false_pos = fs->pc;
   JumpListView(fs, check_nil).patch_to(false_pos);
   JumpListView(fs, check_false).patch_to(false_pos);
   JumpListView(fs, check_zero).patch_to(false_pos);
   JumpListView(fs, check_empty).patch_to(false_pos);
   bcemit_AD(fs, BC_KPRI, dest, BCReg(ExpKind::False));

   // Patch skip jump to after false load
   JumpListView(fs, jmp_false_branch).patch_to(fs->pc);

   expr_init(e, ExpKind::NonReloc, dest);
}

//********************************************************************************************************************
// Emit binary operator.

static void bcemit_binop(FuncState* fs, BinOpr op, ExpDesc* e1, ExpDesc* e2)
{
   if (op <= OPR_POW) {
      bcemit_arith(fs, op, e1, e2);
   }
   else if (op IS OPR_AND) {
      lj_assertFS(e1->t IS NO_JMP, "jump list not closed");
      expr_discharge(fs, e2);
      e2->f = JumpListView(fs, e2->f).append(e1->f);
      *e1 = *e2;
   }
   else if (op IS OPR_OR) {
      lj_assertFS(e1->f IS NO_JMP, "jump list not closed");
      expr_discharge(fs, e2);
      e2->t = JumpListView(fs, e2->t).append(e1->t);
      *e1 = *e2;
   }
   else if (op IS OPR_IF_EMPTY) {
      lj_assertFS(e1->f IS NO_JMP, "jump list not closed");

      // bcemit_binop_left() already set up jumps in e1->t for truthy LHS
      // If e1->t has jumps, LHS is truthy - patch jumps to skip RHS, return LHS

      if (e1->t != NO_JMP) {
         // Patch jumps to skip RHS
         JumpListView(fs, e1->t).patch_to(fs->pc);
         e1->t = NO_JMP;

         // LHS is truthy - no need to evaluate RHS
         // bcemit_binop_left() already loaded truthy constants to a register
         // Just ensure expression is properly set up

         if (e1->k != ExpKind::NonReloc and e1->k != ExpKind::Relocable) {
            if (expr_isk(e1)) { // Constant - load to register
               bcreg_reserve(fs, 1);
               expr_toreg_nobranch(fs, e1, fs->freereg - 1);
            }
            else expr_toanyreg(fs, e1);
         }
      }
      else {
         // LHS is falsey (no jumps) OR runtime value - need to check
         BCReg rhs_reg = NO_REG;
         if (expr_consume_flag(e1, ExprFlag::HasRhsReg)) {
            rhs_reg = BCReg(e1->u.s.aux);
         }

         expr_discharge(fs, e1);

         if (e1->k IS ExpKind::NonReloc or e1->k IS ExpKind::Relocable) {
            // Runtime value - emit extended falsey checks
            BCReg reg = expr_toanyreg(fs, e1);
            ExpDesc nilv = make_nil_expr();
            ExpDesc falsev = make_bool_expr(false);
            ExpDesc zerov = make_num_expr(0.0);
            ExpDesc emptyv = make_interned_string_expr(fs->ls->intern_empty_string());
            BCPos skip;
            BCPos check_nil, check_false, check_zero, check_empty;
            BCReg dest_reg;

            // Check for nil
            bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
            check_nil = bcemit_jmp(fs);

            // Check for false
            bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
            check_false = bcemit_jmp(fs);

            // Check for zero
            bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
            check_zero = bcemit_jmp(fs);

            // Check for empty string
            bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
            check_empty = bcemit_jmp(fs);

            if (rhs_reg IS NO_REG) {
               dest_reg = fs->freereg;
               bcreg_reserve(fs, 1);
            }
            else {
               dest_reg = rhs_reg;
               if (dest_reg >= fs->freereg) fs->freereg = dest_reg + 1;
            }

            // Preserve original value for truthy path before emitting skip jump.
            bcemit_AD(fs, BC_MOV, dest_reg, reg);

            // If all checks pass (value is truthy), skip RHS
            skip = bcemit_jmp(fs);

            // Patch falsey checks to jump to RHS evaluation
            JumpListView(fs, check_nil).patch_to(fs->pc);
            JumpListView(fs, check_false).patch_to(fs->pc);
            JumpListView(fs, check_zero).patch_to(fs->pc);
            JumpListView(fs, check_empty).patch_to(fs->pc);

            // Evaluate RHS
            expr_toreg(fs, e2, dest_reg);
            if (dest_reg != reg) {
               // Copy the fallback result back into the original slot so callers
               // (assignments, returns) continue to observe the same register
               // they used for the LHS. This mirrors the ternary operator,
               // which always delivers its result in the condition register.
               bcemit_AD(fs, BC_MOV, reg, dest_reg);
            }
            JumpListView(fs, skip).patch_to(fs->pc);
            ExprFlag saved_flags = e1->flags;  // Save flags before expr_init

            expr_init(e1, ExpKind::NonReloc, reg);
            e1->flags = saved_flags;  // Restore flags after expr_init

            // Collapse any scratch register reserved for the RHS when it is no longer
            // needed. Returning the allocator to the original base mirrors the ternary
            // operator semantics and prevents the optional from leaking an extra
            // argument slot when used in function-call contexts.

            if (dest_reg != reg and dest_reg >= fs->nactvar and fs->freereg > dest_reg) {
               fs->freereg = dest_reg;
            }

            if (reg >= fs->nactvar and fs->freereg > reg + 1) {
               fs->freereg = reg + 1;
            }
         }
         else { // Constant falsey value - evaluate RHS directly
            expr_discharge(fs, e2);
            *e1 = *e2;
         }
      }
   }
   else if ((op IS OPR_SHL) or (op IS OPR_SHR) or (op IS OPR_BAND) or (op IS OPR_BOR) or (op IS OPR_BXOR)) {
      bcemit_bit_call(fs, std::string_view(priority[op].name, priority[op].name_len), e1, e2);
   }
   else if (op IS OPR_CONCAT) {
      expr_toval(fs, e2);
      if (e2->k IS ExpKind::Relocable and bc_op(*bcptr(fs, e2)) IS BC_CAT) {
         lj_assertFS(e1->u.s.info IS bc_b(*bcptr(fs, e2)) - 1, "bad CAT stack layout");
         expr_free(fs, e1);
         setbc_b(bcptr(fs, e2), e1->u.s.info);
         e1->u.s.info = e2->u.s.info;
      }
      else {
         expr_tonextreg(fs, e2);
         expr_free(fs, e2);
         expr_free(fs, e1);
         e1->u.s.info = bcemit_ABC(fs, BC_CAT, 0, e1->u.s.info, e2->u.s.info);
      }
      e1->k = ExpKind::Relocable;
   }
   else {
      lj_assertFS(op IS OPR_NE or op IS OPR_EQ or op IS OPR_LT or op IS OPR_GE or op IS OPR_LE or op IS OPR_GT, "bad binop %d", op);
      bcemit_comp(fs, op, e1, e2);
   }
}

//********************************************************************************************************************
// Emit unary operator.

static void bcemit_unop(FuncState* fs, BCOp op, ExpDesc* e)
{
   if (op IS BC_NOT) {
      // Swap true and false lists.
      { BCPos temp = e->f; e->f = e->t; e->t = temp; }
      JumpListView(fs, e->f).drop_values();
      JumpListView(fs, e->t).drop_values();
      expr_discharge(fs, e);
      if (e->k IS ExpKind::Nil or e->k IS ExpKind::False) {
         e->k = ExpKind::True;
         return;
      }
      else if (expr_isk(e) or (LJ_HASFFI and e->k IS ExpKind::CData)) {
         e->k = ExpKind::False;
         return;
      }
      else if (e->k IS ExpKind::Jmp) {
         invertcond(fs, e);
         return;
      }
      else if (e->k IS ExpKind::Relocable) {
         bcreg_reserve(fs, 1);
         setbc_a(bcptr(fs, e), fs->freereg - 1);
         e->u.s.info = fs->freereg - 1;
         e->k = ExpKind::NonReloc;
      }
      else lj_assertFS(e->k IS ExpKind::NonReloc, "bad expr type %d", int(e->k));
   }
   else {
      lj_assertFS(op IS BC_UNM or op IS BC_LEN, "bad unop %d", op);
      if (op IS BC_UNM and not expr_hasjump(e)) {  // Constant-fold negations.
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
            if (expr_isnumk(e) and !expr_numiszero(e)) {  // Avoid folding to -0.
               TValue* o = expr_numtv(e);
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
      expr_toanyreg(fs, e);
   }
   expr_free(fs, e);
   e->u.s.info = bcemit_AD(fs, op, 0, e->u.s.info);
   e->k = ExpKind::Relocable;
}
