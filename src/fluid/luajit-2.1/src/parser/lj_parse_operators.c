/*
** Lua parser - Operator bytecode emission.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim || adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

// -- Bytecode emitter for operators --------------------------------------

// Try constant-folding of arithmetic operators.
static int foldarith(BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   TValue o;
   lua_Number n;
   if (!expr_isnumk_nojump(e1) || !expr_isnumk_nojump(e2)) return 0;
   n = lj_vm_foldarith(expr_numberV(e1), expr_numberV(e2), (int)opr - OPR_ADD);
   setnumV(&o, n);
   if (tvisnan(&o) || tvismzero(&o)) return 0;  // Avoid NaN && -0 as consts.
   if (LJ_DUALNUM) {
      int32_t k = lj_num2int(n);
      if ((lua_Number)k == n) {
         setintV(&e1->u.nval, k);
         return 1;
      }
   }
   setnumV(&e1->u.nval, n);
   return 1;
}

// Emit arithmetic operator.
static void bcemit_arith(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   BCReg rb, rc, t;
   uint32_t op;
   if (foldarith(opr, e1, e2))
      return;
   if (opr == OPR_POW) {
      op = BC_POW;
      rc = expr_toanyreg(fs, e2);
      rb = expr_toanyreg(fs, e1);
   }
   else {
      op = opr - OPR_ADD + BC_ADDVV;
      // Must discharge 2nd operand first since VINDEXED might free regs.
      expr_toval(fs, e2);
      if (expr_isnumk(e2) && (rc = const_num(fs, e2)) <= BCMAX_C)
         op -= BC_ADDVV - BC_ADDVN;
      else
         rc = expr_toanyreg(fs, e2);
      // 1st operand discharged by bcemit_binop_left, but need KNUM/KSHORT.
      lj_assertFS(expr_isnumk(e1) || e1->k == VNONRELOC,
         "bad expr type %d", e1->k);
      expr_toval(fs, e1);
      // Avoid two consts to satisfy bytecode constraints.
      if (expr_isnumk(e1) && !expr_isnumk(e2) &&
         (t = const_num(fs, e1)) <= BCMAX_B) {
         rb = rc; rc = t; op -= BC_ADDVV - BC_ADDNV;
      }
      else {
         rb = expr_toanyreg(fs, e1);
      }
   }
   // Using expr_free might cause asserts if the order is wrong.
   if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
   if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
   e1->u.s.info = bcemit_ABC(fs, op, 0, rb, rc);
   e1->k = VRELOCABLE;
}

// Emit comparison operator.
static void bcemit_comp(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2)
{
   ExpDesc* eret = e1;
   BCIns ins;
   expr_toval(fs, e1);
   if (opr == OPR_EQ || opr == OPR_NE) {
      BCOp op = opr == OPR_EQ ? BC_ISEQV : BC_ISNEV;
      BCReg ra;
      if (expr_isk(e1)) { e1 = e2; e2 = eret; }  // Need constant in 2nd arg.
      ra = expr_toanyreg(fs, e1);  // First arg must be in a reg.
      expr_toval(fs, e2);
      switch (e2->k) {
      case VKNIL: case VKFALSE: case VKTRUE:
         ins = BCINS_AD(op + (BC_ISEQP - BC_ISEQV), ra, const_pri(e2));
         break;
      case VKSTR:
         ins = BCINS_AD(op + (BC_ISEQS - BC_ISEQV), ra, const_str(fs, e2));
         break;
      case VKNUM:
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
   if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
   if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
   bcemit_INS(fs, ins);
   eret->u.s.info = bcemit_jmp(fs);
   eret->k = VJMP;
}

// Fixup left side of binary operator.
static void bcemit_binop_left(FuncState* fs, BinOpr op, ExpDesc* e)
{
   if (op == OPR_AND) {
      bcemit_branch_t(fs, e);
   }
   else if (op == OPR_OR) {
      bcemit_branch_f(fs, e);
   }
   else if (op == OPR_IF_EMPTY) {
      // For ?, handle extended falsey checks - only set up jumps for compile-time constants
      BCPos pc;

      // Save whether we had SAFE_NAV_CHAIN_FLAG but DON'T clear it yet.
      // We need to preserve flag state through the register reservation logic below.
      uint8_t had_safe_nav = (e->flags & SAFE_NAV_CHAIN_FLAG) != 0;

      expr_discharge(fs, e);
      // Extended falsey: nil, false, 0, ""
      if (e->k == VKNIL || e->k == VKFALSE)
         pc = NO_JMP;  // Never jump - these are falsey, evaluate RHS
      else if (e->k == VKNUM && expr_numiszero(e))
         pc = NO_JMP;  // Zero is falsey, evaluate RHS
      else if (e->k == VKSTR && e->u.sval && e->u.sval->len == 0)
         pc = NO_JMP;  // Empty string is falsey, evaluate RHS
      else if (e->k == VJMP)
         pc = e->u.s.info;
      else if (e->k == VKSTR || e->k == VKNUM || e->k == VKTRUE) {
         // Truthy constant - load to register && emit jump to skip RHS
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
            printf("DEBUG: [bcemit_binop_left] Before expr_toanyreg: e->k=%d, e->t=%d, e->f=%d\n", e->k, e->t, e->f);
            BCReg src_reg = expr_toanyreg(fs, e);
            printf("DEBUG: [bcemit_binop_left] After expr_toanyreg: e->k=%d, e->t=%d, e->f=%d, src_reg=%d, freereg=%d\n", e->k, e->t, e->f, src_reg, fs->freereg);
            uint8_t flags = e->flags;

            // CRITICAL FIX: Reserve the RHS register BEFORE capturing it.
            // This ensures rhs_reg is always a fresh register that doesn't alias with src_reg.
            // When safe navigation collapses freereg, we need to ensure rhs_reg != src_reg.
            bcreg_reserve(fs, 1);
            BCReg rhs_reg = fs->freereg - 1;  // Use the register we just reserved
            printf("DEBUG: After first bcreg_reserve: flags=0x%02x, src_reg=%d, rhs_reg=%d, freereg=%d\n",
                   flags, src_reg, rhs_reg, fs->freereg);

            // If rhs_reg still aliases src_reg (due to freereg collapse), reserve another register
            if (rhs_reg <= src_reg) {
               printf("DEBUG: rhs_reg <= src_reg, reserving another register\n");
               bcreg_reserve(fs, 1);
               rhs_reg = fs->freereg - 1;
               printf("DEBUG: After second bcreg_reserve: rhs_reg=%d, freereg=%d\n", rhs_reg, fs->freereg);
            }

            expr_init(e, VNONRELOC, src_reg);
            e->u.s.aux = rhs_reg;
            // Clear both SAFE_NAV flags since we've captured the register.
            // The flags have served their purpose and must not interfere with register cleanup.
            e->flags = (flags & ~(SAFE_NAV_CHAIN_FLAG | EXP_SAFE_NAV_RESULT_FLAG)) | EXP_HAS_RHS_REG_FLAG;
         }
         pc = NO_JMP;  // No jump - will check extended falsey in bcemit_binop()
      }
      // For constant cases, also clear both safe nav flags now that processing is complete
      if (had_safe_nav) {
         e->flags &= ~(SAFE_NAV_CHAIN_FLAG | EXP_SAFE_NAV_RESULT_FLAG);
      }
      jmp_append(fs, &e->t, pc);
      jmp_tohere(fs, e->f);
      e->f = NO_JMP;
   }
   else if (op == OPR_CONCAT) {
      expr_tonextreg(fs, e);
   }
   else if (op == OPR_EQ || op == OPR_NE) {
      if (!expr_isk_nojump(e)) expr_toanyreg(fs, e);
   }
   else {
      if (!expr_isnumk_nojump(e)) expr_toanyreg(fs, e);
   }
}

/* Emit a call to a bit library function (bit.lshift, bit.rshift, etc.) at a specific base register.
**
** This function is used to implement C-style bitwise shift operators (<<, >>) by translating them
** into calls to LuaJIT's bit library functions. The base register is explicitly provided to allow
** chaining of multiple shift operations while reusing the same register for intermediate results.
**
** Register Layout (x64 with LJ_FR2=1):
**   base     - Function to call (bit.lshift, bit.rshift, etc.)
**   base+1   - Frame link register (LJ_FR2, not an argument)
**   base+2   - arg1: First operand (value to shift)
**   base+3   - arg2: Second operand (shift count)
**
** BC_CALL Instruction Format:
**   - A field: base register
**   - B field: Call type (2 for regular calls, 0 for varargs)
**   - C field: Argument count = freereg - base - LJ_FR2
**
** VCALL Handling (Multi-Return Functions):
**   When RHS is a VCALL (function call with multiple return values), standard Lua binary operator
**   semantics apply: only the first return value is used. The VCALL is discharged before being
**   passed as an argument. This matches the behavior of expressions like `x + f()` in Lua.
**
**   Note: Unlike function argument lists (which use BC_CALLM to forward all return values),
**   binary operators always restrict multi-return expressions to single values. This is a
**   fundamental Lua language semantic, not a limitation of this implementation.
**
** Parameters:
**   fs    - Function state for bytecode generation
**   fname - Name of bit library function (e.g., "lshift", "rshift")
**   fname_len - Length of fname string
**   lhs   - Left-hand side expression (value to shift)
**   rhs   - Right-hand side expression (shift count, may be VCALL)
**   base  - Base register for the call (allows register reuse for chaining)
*/
static void bcemit_shift_call_at_base(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* lhs, ExpDesc* rhs, BCReg base)
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
   expr_init(&callee, VGLOBAL, 0);
   callee.u.sval = lj_parse_keepstr(fs->ls, "bit", 3);
   expr_toanyreg(fs, &callee);
   expr_init(&key, VKSTR, 0);
   key.u.sval = lj_parse_keepstr(fs->ls, fname, fname_len);
   expr_index(fs, &callee, &key);
   expr_toval(fs, &callee);
   expr_toreg(fs, &callee, base);

   // Emit CALL instruction
   fs->freereg = arg2 + 1;  // Ensure freereg covers all arguments
   lhs->k = VCALL;
   lhs->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
   lhs->u.s.aux = base;
   fs->freereg = base + 1;

   expr_discharge(fs, lhs);
   lj_assertFS(lhs->k == VNONRELOC && lhs->u.s.info == base, "bitwise result not in base register");
}

static void bcemit_bit_call(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* lhs, ExpDesc* rhs)
{
   // Allocate a base register for the call
   BCReg base = fs->freereg;
   bcreg_reserve(fs, 1);  // Reserve for callee
   if (LJ_FR2) bcreg_reserve(fs, 1);
   bcreg_reserve(fs, 2);  // Reserve for arguments
   lj_assertFS(fname != NULL, "bitlib name missing for bitwise operator");
   bcemit_shift_call_at_base(fs, fname, fname_len, lhs, rhs, base);
}

// Emit unary bit library call (e.g., bit.bnot).
static void bcemit_unary_bit_call(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* arg)
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
   expr_init(&callee, VGLOBAL, 0);
   callee.u.sval = lj_parse_keepstr(fs->ls, "bit", 3);
   expr_toanyreg(fs, &callee);
   expr_init(&key, VKSTR, 0);
   key.u.sval = lj_parse_keepstr(fs->ls, fname, fname_len);
   expr_index(fs, &callee, &key);
   expr_toval(fs, &callee);
   expr_toreg(fs, &callee, base);

   // Emit CALL instruction.
   fs->freereg = arg_reg + 1;
   arg->k = VCALL;
   arg->u.s.info = bcemit_INS(fs, BCINS_ABC(BC_CALL, base, 2, fs->freereg - base - LJ_FR2));
   arg->u.s.aux = base;
   fs->freereg = base + 1;

   // Discharge result to register.
   expr_discharge(fs, arg);
   lj_assertFS(arg->k == VNONRELOC && arg->u.s.info == base, "bitwise result not in base register");
}

/* Emit bytecode for postfix presence check operator (x?).
** Returns boolean: true if value is truthy (extended falsey semantics),
** false if value is falsey (nil, false, 0, "").
*/
static void bcemit_presence_check(FuncState* fs, ExpDesc* e)
{
   expr_discharge(fs, e);

   // Handle compile-time constants
   if (e->k == VKNIL || e->k == VKFALSE) {
      // Falsey constant - set to false
      expr_init(e, VKFALSE, 0);
      return;
   }

   if (e->k == VKNUM && expr_numiszero(e)) {
      // Zero is falsey - set to false
      expr_init(e, VKFALSE, 0);
      return;
   }

   if (e->k == VKSTR && e->u.sval && e->u.sval->len == 0) {
      // Empty string is falsey - set to false
      expr_init(e, VKFALSE, 0);
      return;
   }

   if (e->k == VKTRUE || (e->k == VKNUM && !expr_numiszero(e)) ||
      (e->k == VKSTR && e->u.sval && e->u.sval->len > 0)) {
      // Truthy constant - set to true
      expr_init(e, VKTRUE, 0);
      return;
   }

   // Runtime value - emit checks
   // Follow `?` pattern: use BC_ISEQP/BC_ISEQN/BC_ISEQS, patch jumps to false branch
   /*
    * Bytecode semantics: BC_ISEQP/BC_ISEQN/BC_ISEQS skip the next instruction when values ARE equal.
    * Pattern: BC_ISEQP reg, VKNIL + JMP means:
    *   - If reg == nil: skip JMP, continue to next check
    *   - If reg != nil: execute JMP, jump to target (patched to false branch)
    * By chaining multiple checks && patching all JMPs to the same false branch:
    *   - Falsey values: matching check skips its JMP, execution continues (reaches truthy branch)
    *   - Truthy values: all checks fail, first JMP executes, jumps to false branch
    */
   BCReg reg = expr_toanyreg(fs, e);
   ExpDesc nilv, falsev, zerov, emptyv;
   BCPos jmp_false_branch;
   BCPos check_nil, check_false, check_zero, check_empty;

   // Check for nil
   expr_init(&nilv, VKNIL, 0);
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
   check_nil = bcemit_jmp(fs);

   // Check for false
   expr_init(&falsev, VKFALSE, 0);
   bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
   check_false = bcemit_jmp(fs);

   // Check for zero
   expr_init(&zerov, VKNUM, 0);
   setnumV(&zerov.u.nval, 0.0);
   bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
   check_zero = bcemit_jmp(fs);

   // Check for empty string
   expr_init(&emptyv, VKSTR, 0);
   emptyv.u.sval = lj_parse_keepstr(fs->ls, "", 0);
   bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
   check_empty = bcemit_jmp(fs);

   // Reserve a register for the result
   BCReg dest = fs->freereg;
   bcreg_reserve(fs, 1);

   // Free the old expression register after reserving new one
   expr_free(fs, e);

   // If all checks pass (value is truthy), load true
   bcemit_AD(fs, BC_KPRI, dest, VKTRUE);
   jmp_false_branch = bcemit_jmp(fs);

   // False branch: patch all falsey jumps here && load false
   BCPos false_pos = fs->pc;
   jmp_patch(fs, check_nil, false_pos);
   jmp_patch(fs, check_false, false_pos);
   jmp_patch(fs, check_zero, false_pos);
   jmp_patch(fs, check_empty, false_pos);
   bcemit_AD(fs, BC_KPRI, dest, VKFALSE);

   // Patch skip jump to after false load
   jmp_patch(fs, jmp_false_branch, fs->pc);

   expr_init(e, VNONRELOC, dest);
}

// Emit binary operator.

static void bcemit_binop(FuncState* fs, BinOpr op, ExpDesc* e1, ExpDesc* e2)
{
   if (op <= OPR_POW) {
      bcemit_arith(fs, op, e1, e2);
   }
   else if (op == OPR_AND) {
      lj_assertFS(e1->t == NO_JMP, "jump list not closed");
      expr_discharge(fs, e2);
      jmp_append(fs, &e2->f, e1->f);
      *e1 = *e2;
   }
   else if (op == OPR_OR) {
      lj_assertFS(e1->f == NO_JMP, "jump list not closed");
      expr_discharge(fs, e2);
      jmp_append(fs, &e2->t, e1->t);
      *e1 = *e2;
   }
   else if (op == OPR_IF_EMPTY) {
      lj_assertFS(e1->f == NO_JMP, "jump list not closed");

      // DEFENSIVE: Ensure safe navigation flags are consumed.
      // These should already be consumed by bcemit_binop_left(), but we're defensive here
      // in case the flags somehow persist through other code paths.
      e1->flags &= ~(SAFE_NAV_CHAIN_FLAG | EXP_SAFE_NAV_RESULT_FLAG);

      // bcemit_binop_left() already set up jumps in e1->t for truthy LHS
      // If e1->t has jumps, LHS is truthy - patch jumps to skip RHS, return LHS
      if (e1->t != NO_JMP) {
         // Patch jumps to skip RHS
         jmp_patch(fs, e1->t, fs->pc);
         e1->t = NO_JMP;
         // LHS is truthy - no need to evaluate RHS
         // bcemit_binop_left() already loaded truthy constants to a register
         // Just ensure expression is properly set up
         if (e1->k != VNONRELOC && e1->k != VRELOCABLE) {
            if (expr_isk(e1)) {
               // Constant - load to register
               bcreg_reserve(fs, 1);
               expr_toreg_nobranch(fs, e1, fs->freereg - 1);
            }
            else {
               expr_toanyreg(fs, e1);
            }
         }
      }
      else {
         // LHS is falsey (no jumps) OR runtime value - need to check
         BCReg rhs_reg = NO_REG;
         if (e1->flags & EXP_HAS_RHS_REG_FLAG) {
            rhs_reg = (BCReg)e1->u.s.aux;
            e1->flags &= ~EXP_HAS_RHS_REG_FLAG;
         }

         printf("DEBUG: [bcemit_binop OPR_IF_EMPTY] Before expr_discharge: e1->k=%d, e1->t=%d, e1->f=%d\n", e1->k, e1->t, e1->f);
         expr_discharge(fs, e1);
         printf("DEBUG: [bcemit_binop OPR_IF_EMPTY] After expr_discharge: e1->k=%d, e1->t=%d, e1->f=%d\n", e1->k, e1->t, e1->f);
         
         if (e1->k == VNONRELOC || e1->k == VRELOCABLE) {
            // Runtime value - emit extended falsey checks
            BCReg reg = expr_toanyreg(fs, e1);
            ExpDesc nilv, falsev, zerov, emptyv;
            BCPos skip;
            BCPos check_nil, check_false, check_zero, check_empty;
            BCReg dest_reg;

            // Check for nil
            expr_init(&nilv, VKNIL, 0);
            printf("DEBUG: Emitting ISEQP nil at PC=%d for reg=%d\n", fs->pc, reg);
            bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&nilv)));
            check_nil = bcemit_jmp(fs);
            printf("DEBUG: Emitted nil check JMP at PC=%d (check_nil=%d)\n", check_nil, check_nil);

            // Check for false
            expr_init(&falsev, VKFALSE, 0);
            printf("DEBUG: Emitting ISEQP false at PC=%d for reg=%d\n", fs->pc, reg);
            bcemit_INS(fs, BCINS_AD(BC_ISEQP, reg, const_pri(&falsev)));
            check_false = bcemit_jmp(fs);
            printf("DEBUG: Emitted false check JMP at PC=%d (check_false=%d)\n", check_false, check_false);

            // Check for zero
            expr_init(&zerov, VKNUM, 0);
            setnumV(&zerov.u.nval, 0.0);
            bcemit_INS(fs, BCINS_AD(BC_ISEQN, reg, const_num(fs, &zerov)));
            check_zero = bcemit_jmp(fs);

            // Check for empty string
            expr_init(&emptyv, VKSTR, 0);
            emptyv.u.sval = lj_parse_keepstr(fs->ls, "", 0);
            bcemit_INS(fs, BCINS_AD(BC_ISEQS, reg, const_str(fs, &emptyv)));
            check_empty = bcemit_jmp(fs);

            if (rhs_reg == NO_REG) {
               dest_reg = fs->freereg;
               bcreg_reserve(fs, 1);
               printf("DEBUG: No RHS_REG, allocating dest_reg=%d, freereg now=%d\n", dest_reg, fs->freereg);
            }
            else {
               dest_reg = rhs_reg;
               if (dest_reg >= fs->freereg) fs->freereg = dest_reg + 1;
               printf("DEBUG: Using RHS_REG=%d as dest_reg, freereg now=%d\n", dest_reg, fs->freereg);
            }

            printf("DEBUG: Before skip JMP at PC=%d: reg=%d, dest_reg=%d\n", fs->pc, reg, dest_reg);

            // If all checks pass (value is truthy), skip RHS
            skip = bcemit_jmp(fs);
            printf("DEBUG: Skip JMP emitted at PC=%d (skip=%d), now at PC=%d\n", skip, skip, fs->pc);

            // Patch falsey checks to jump to RHS evaluation
            printf("DEBUG: Patching checks to jump to PC=%d: check_nil=%d, check_false=%d, check_zero=%d, check_empty=%d\n",
                   fs->pc, check_nil, check_false, check_zero, check_empty);
            jmp_patch(fs, check_nil, fs->pc);
            jmp_patch(fs, check_false, fs->pc);
            jmp_patch(fs, check_zero, fs->pc);
            jmp_patch(fs, check_empty, fs->pc);
            printf("DEBUG: After patching, still at PC=%d\n", fs->pc);

            printf("DEBUG: About to evaluate RHS at PC=%d into dest_reg=%d, e2->k=%d\n", fs->pc, dest_reg, e2->k);
            // Evaluate RHS
            expr_toreg(fs, e2, dest_reg);
            printf("DEBUG: RHS evaluated at PC=%d, dest_reg=%d, reg=%d, need copy=%d, e2->k=%d, e2->u.s.info=%d\n",
                   fs->pc, dest_reg, reg, dest_reg != reg, e2->k, e2->k == VNONRELOC ? e2->u.s.info : 999);
            if (dest_reg != reg) {
               // Copy the fallback result back into the original slot so callers
               // (assignments, returns) continue to observe the same register
               // they used for the LHS. This mirrors the ternary operator,
               // which always delivers its result in the condition register.
               printf("DEBUG: Copying from dest_reg=%d to reg=%d, emitting MOV at PC=%d\n", dest_reg, reg, fs->pc);
               bcemit_AD(fs, BC_MOV, reg, dest_reg);
               printf("DEBUG: MOV emitted, now at PC=%d\n", fs->pc);
            }
            printf("DEBUG: Before jmp_patch(skip=%d, fs->pc=%d)\n", skip, fs->pc);
            jmp_patch(fs, skip, fs->pc);
            printf("DEBUG: After jmp_patch\n");
            uint8_t saved_flags = e1->flags;  // Save flags before expr_init

            expr_init(e1, VNONRELOC, reg);
            e1->flags = saved_flags;  // Restore flags after expr_init

            /*
            ** Collapse any scratch register reserved for the RHS when it is no longer
            ** needed. Returning the allocator to the original base mirrors the ternary
            ** operator semantics and prevents the optional from leaking an extra
            ** argument slot when used in function-call contexts.
            */
            if (dest_reg != reg && dest_reg >= fs->nactvar && fs->freereg > dest_reg) {
               fs->freereg = dest_reg;
            }
            if (reg >= fs->nactvar && fs->freereg > reg + 1) {
               fs->freereg = reg + 1;
            }
         }
         else { // Constant falsey value - evaluate RHS directly
            expr_discharge(fs, e2);
            *e1 = *e2;
         }
      }
   }
   else if ((op == OPR_SHL) || (op == OPR_SHR) || (op == OPR_BAND) || (op == OPR_BOR) || (op == OPR_BXOR)) {
      bcemit_bit_call(fs, priority[op].name, (MSize)priority[op].name_len, e1, e2);
   }
   else if (op == OPR_CONCAT) {
      expr_toval(fs, e2);
      if (e2->k == VRELOCABLE && bc_op(*bcptr(fs, e2)) == BC_CAT) {
         lj_assertFS(e1->u.s.info == bc_b(*bcptr(fs, e2)) - 1,
            "bad CAT stack layout");
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
      e1->k = VRELOCABLE;
   }
   else {
      lj_assertFS(op == OPR_NE || op == OPR_EQ ||
         op == OPR_LT || op == OPR_GE || op == OPR_LE || op == OPR_GT,
         "bad binop %d", op);
      bcemit_comp(fs, op, e1, e2);
   }
}

// Emit unary operator.

static void bcemit_unop(FuncState* fs, BCOp op, ExpDesc* e)
{
   if (op == BC_NOT) {
      // Swap true && false lists.
      { BCPos temp = e->f; e->f = e->t; e->t = temp; }
      jmp_dropval(fs, e->f);
      jmp_dropval(fs, e->t);
      expr_discharge(fs, e);
      if (e->k == VKNIL || e->k == VKFALSE) {
         e->k = VKTRUE;
         return;
      }
      else if (expr_isk(e) || (LJ_HASFFI && e->k == VKCDATA)) {
         e->k = VKFALSE;
         return;
      }
      else if (e->k == VJMP) {
         invertcond(fs, e);
         return;
      }
      else if (e->k == VRELOCABLE) {
         bcreg_reserve(fs, 1);
         setbc_a(bcptr(fs, e), fs->freereg - 1);
         e->u.s.info = fs->freereg - 1;
         e->k = VNONRELOC;
      }
      else {
         lj_assertFS(e->k == VNONRELOC, "bad expr type %d", e->k);
      }
   }
   else {
      lj_assertFS(op == BC_UNM || op == BC_LEN, "bad unop %d", op);
      if (op == BC_UNM && !expr_hasjump(e)) {  // Constant-fold negations.
#if LJ_HASFFI
         if (e->k == VKCDATA) {  // Fold in-place since cdata is not interned.
            GCcdata* cd = cdataV(&e->u.nval);
            int64_t* p = (int64_t*)cdataptr(cd);
            if (cd->ctypeid == CTID_COMPLEX_DOUBLE)
               p[1] ^= (int64_t)U64x(80000000, 00000000);
            else
               *p = -*p;
            return;
         }
         else
#endif
            if (expr_isnumk(e) && !expr_numiszero(e)) {  // Avoid folding to -0.
               TValue* o = expr_numtv(e);
               if (tvisint(o)) {
                  int32_t k = intV(o);
                  if (k == -k)
                     setnumV(o, -(lua_Number)k);
                  else
                     setintV(o, -k);
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
   e->k = VRELOCABLE;
}
