// Lua parser - Register allocation and bytecode emission.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#include "parser/parse_regalloc.h"

#include <parasol/main.h>

[[nodiscard]] BCPOS bcemit_jmp(FuncState *);

inline bool is_register_key(int32_t Aux)
{
   return (Aux >= 0) and (Aux <= BCMAX_C);
}

//********************************************************************************************************************
// Register allocation methods

void RegisterAllocator::bump(BCReg Count)
{
   BCREG target = this->func_state->freereg + Count.raw();
   if (target > this->func_state->framesize) {
      if (target >= LJ_MAX_SLOTS) this->func_state->ls->err_syntax(ErrMsg::XSLOTS);
      this->func_state->framesize = uint8_t(target);
   }
}

BCReg RegisterAllocator::reserve_slots(BCReg Count)
{
   if (not Count.raw()) return BCReg(this->func_state->freereg);

   BCReg start = BCReg(this->func_state->freereg);
   this->bump(Count);
   this->func_state->freereg += Count.raw();
   this->trace_allocation(start, Count, "reserve_slots");
   return start;
}

RegisterSpan RegisterAllocator::reserve_span(BCReg Count)
{
   if (not Count.raw()) return RegisterSpan();

   BCReg start = this->reserve_slots(Count);
   return RegisterSpan(this, start, Count, start + Count.raw());
}

RegisterSpan RegisterAllocator::reserve_span_soft(BCReg Count)
{
   if (not Count.raw()) return RegisterSpan();

   BCReg start = this->reserve_slots(Count);
   // ExpectedTop=0 marks this as a "soft" span: the allocator will not enforce
   // strict RAII checks or adjust freereg when the span is released. Callers
   // using soft spans are responsible for collapsing freereg themselves (for
   // example, by restoring freereg to nactvar at the end of an assignment).
   return RegisterSpan(this, start, Count, BCReg(0));
}

void RegisterAllocator::release_span_internal(BCReg Start, BCReg Count, BCReg ExpectedTop)
{
   if (not Count.raw()) return;

   if (this->func_state->is_temp_register(Start)) {
      // Soft spans (ExpectedTop == 0) are used in contexts where the caller
      // explicitly manages freereg (e.g. assignment emitters that duplicate
      // table operands and later restore freereg to nactvar). For these spans
      // we do not enforce RAII invariants or adjust freereg here.
      if (ExpectedTop.raw() IS 0) {
         this->trace_release(Start, Count, "release_span_internal_soft");
         return;
      }

      if (this->func_state->freereg > ExpectedTop.raw()) {
         pf::Log("Parser").warning("Register depth mismatch, %d != %d, function @ line %d - "
            "RegisterSpan was created with freereg=%d but released as %d. "
            "This indicates intermediate operations modified freereg or cleanup is out of order.",
            ExpectedTop.raw(), this->func_state->freereg, this->func_state->linedefined,
            ExpectedTop.raw(), this->func_state->freereg);
      }

      // Check for span size mismatch
      if (Start.raw() + Count.raw() != ExpectedTop.raw()) {
         pf::Log("Parser").warning("Span size mismatch: start=%u count=%u expected_top=%u at line %d",
            Start.raw(), Count.raw(), ExpectedTop.raw(), this->func_state->linedefined);
      }

      this->func_state->freereg = ExpectedTop.raw() - Count.raw();

      // Check that after release, freereg equals the span start
      if (this->func_state->freereg != Start.raw()) {
         pf::Log("Parser").warning("Bad regfree: freereg=%u should equal start=%u at line %d",
            this->func_state->freereg, Start.raw(), this->func_state->linedefined);
      }

      this->trace_release(Start, Count, "release_span_internal");
   }
}

void RegisterAllocator::release(RegisterSpan &Span)
{
   if (Span.allocator_) {
      this->release_span_internal(Span.start_, Span.count_, Span.expected_top_);
      Span.allocator_ = nullptr;
   }
}

void RegisterAllocator::release(AllocatedRegister &Handle)
{
   if (Handle.allocator_) {
      this->release_span_internal(Handle.index_, BCReg(1), Handle.expected_top_);
      Handle.allocator_ = nullptr;
   }
}

void RegisterAllocator::release_register(BCReg Register)
{
   BCReg expected_top = Register + BCREG(1);
   if (this->func_state->is_temp_register(Register) and expected_top.raw() IS this->func_state->freereg) {
      this->release_span_internal(Register, BCReg(1), expected_top);
   }
}

void RegisterAllocator::release_expression(ExpDesc *Expression)
{
   if (Expression->k IS ExpKind::NonReloc) {
      BCReg reg = BCReg(Expression->u.s.info);
      BCReg expected_top = reg + BCREG(1);
      if (this->func_state->is_temp_register(reg) and expected_top.raw() IS this->func_state->freereg) {
         this->release_span_internal(reg, BCReg(1), expected_top);
      }
   }
}

void RegisterAllocator::collapse_freereg(BCReg ResultReg)
{
   BCREG target = ResultReg.raw() + 1;
   if (target < this->func_state->nactvar) target = this->func_state->nactvar;

   while (this->func_state->freereg > target) {
      BCREG previous = this->func_state->freereg;
      BCREG top = previous - 1;
      this->release_register(BCReg(top));
      if (this->func_state->freereg IS previous) break;
   }
}

TableOperandCopies RegisterAllocator::duplicate_table_operands(const ExpDesc &Expression)
{
   TableOperandCopies copies{};
   copies.duplicated = Expression;

   if (Expression.k IS ExpKind::Indexed) {
      uint32_t original_aux = Expression.u.s.aux;
      BCREG duplicate_count = 1;
      bool has_register_index = is_register_key(int32_t(original_aux));

      if (has_register_index) duplicate_count++;

      // Use a soft span here because assignment/update emitters that rely on
      // these duplicates manage freereg explicitly (they collapse freereg back
      // to nactvar after completing the operation). Enforcing strict RAII
      // invariants for this span would produce false-positive warnings in
      // perfectly valid patterns like:
      //   t[i] = t[i] | f(i)
      // where additional temporaries are allocated above the duplicated base
      // and later dropped by restoring freereg.

      copies.reserved = this->reserve_span_soft(BCReg(duplicate_count));

      BCREG base_reg = copies.reserved.start().raw();
      bcemit_AD(this->func_state, BC_MOV, base_reg, Expression.u.s.info);
      copies.duplicated.u.s.info = base_reg;

      if (has_register_index) {
         BCREG index_reg = BCREG(base_reg + 1);
         bcemit_AD(this->func_state, BC_MOV, index_reg, BCREG(original_aux));
         copies.duplicated.u.s.aux = index_reg;
      }
   }

   return copies;
}

//********************************************************************************************************************
// Bump frame size.

inline void bcreg_bump(FuncState *fs, BCREG n)
{
   RegisterAllocator allocator(fs);
   allocator.bump(BCReg(n));
}

//********************************************************************************************************************
// Reserve registers.

inline void bcreg_reserve(FuncState *fs, BCREG n)
{
   RegisterAllocator allocator(fs);
   allocator.reserve(BCReg(n));
}

//********************************************************************************************************************
// Free register.

inline void bcreg_free(FuncState *fs, BCREG reg)
{
   RegisterAllocator allocator(fs);
   allocator.release_register(BCReg(reg));
}

//********************************************************************************************************************
// Free register for expression.

inline void expr_free(FuncState *fs, ExpDesc *e)
{
   RegisterAllocator allocator(fs);
   allocator.release_expression(e);
}

//********************************************************************************************************************
// Emit bytecode instruction.
// Exported for use by OperatorEmitter facade

BCPOS bcemit_INS(FuncState *fs, BCIns ins)
{
   BCPOS pc = fs->pc;
   LexState* ls = fs->ls;
   ControlFlowGraph cfg(fs);
   ControlFlowEdge pending = cfg.make_unconditional(fs->pending_jmp());
   pending.patch_with_value(BCPos(pc), BCReg(NO_REG), BCPos(pc));
   fs->clear_pending_jumps();

   if (pc >= fs->bclim) [[unlikely]] {
      ptrdiff_t base = fs->bcbase - ls->bcstack;
      checklimit(fs, ls->sizebcstack, LJ_MAX_BCINS, "bytecode instructions");
      lj_mem_growvec(fs->L, ls->bcstack, ls->sizebcstack, LJ_MAX_BCINS, BCInsLine);
      fs->bclim = BCPOS(ls->sizebcstack - base);
      fs->bcbase = ls->bcstack + base;
   }

   fs->bcbase[pc].ins = ins;
   fs->bcbase[pc].line = ls->lastline;
   fs->pc = pc + 1;
   return pc;
}

//********************************************************************************************************************
// Bytecode emitter for expressions

// Discharge non-constant expression to any register.

static void expr_discharge(FuncState *fs, ExpDesc *e)
{
   BCIns ins;
   if (e->k IS ExpKind::Upval) {
      ins = BCINS_AD(BC_UGET, 0, e->u.s.info);
   }
   else if (e->k IS ExpKind::Global or e->k IS ExpKind::Unscoped) {
      // Check if trying to read blank identifier.
      if (is_blank_identifier(e->u.sval)) {
         lj_lex_error(fs->ls, fs->ls->tok, ErrMsg::XNEAR,
            "cannot read blank identifier");
      }
      // For reads, both Global and Unscoped resolve to global table lookup
      ins = BCINS_AD(BC_GGET, 0, const_str(fs, e));
   }
   else if (e->k IS ExpKind::Indexed) {
      BCREG rc = e->u.s.aux;
      if (int32_t(rc) < 0) {
         ins = BCINS_ABC(BC_TGETS, 0, e->u.s.info, ~rc);
      }
      else if (rc > BCMAX_C) {
         ins = BCINS_ABC(BC_TGETB, 0, e->u.s.info, rc - (BCMAX_C + 1));
      }
      else {
         bcreg_free(fs, rc);
         ins = BCINS_ABC(BC_TGETV, 0, e->u.s.info, rc);
      }
      bcreg_free(fs, e->u.s.info);
   }
   else if (e->k IS ExpKind::Call) {
      e->u.s.info = e->u.s.aux;
      e->k = ExpKind::NonReloc;
      return;
   }
   else if (e->k IS ExpKind::Local) {
      e->k = ExpKind::NonReloc;
      return;
   }
   else return;

   e->u.s.info = bcemit_INS(fs, ins);
   e->k = ExpKind::Relocable;
}

//********************************************************************************************************************
// Emit bytecode to set a range of registers to nil.

static void bcemit_nil(FuncState *fs, BCREG from, BCREG n)
{
   if (fs->pc > fs->lasttarget) {  // No jumps to current position?
      BCIns* ip = &fs->last_instruction().ins;
      BCREG pto, pfrom = bc_a(*ip);
      switch (bc_op(*ip)) {  // Try to merge with the previous instruction.
         case BC_KPRI:
            if (bc_d(*ip) != ~LJ_TNIL) break;
            if (from IS pfrom) {
               if (n IS 1) return;
            }
            else if (from IS pfrom + 1) {
               from = pfrom;
               n++;
            }
            else break;
            *ip = BCINS_AD(BC_KNIL, from, from + n - 1);  // Replace KPRI.
            return;
         case BC_KNIL:
            pto = bc_d(*ip);
            if (pfrom <= from and from <= pto + 1) {  // Can we connect both ranges?
               if (from + n - 1 > pto) setbc_d(ip, from + n - 1);  // Patch previous instruction range.
               return;
            }
            break;
         default:
            break;
      }
   }

   // Emit new instruction or replace old instruction.
   bcemit_INS(fs, n IS 1 ? BCINS_AD(BC_KPRI, from, ExpKind::Nil) :
      BCINS_AD(BC_KNIL, from, from + n - 1));
}

//********************************************************************************************************************
// Discharge an expression to a specific register. Ignore branches.

static void expr_toreg_nobranch(FuncState *fs, ExpDesc *e, BCREG reg)
{
   BCIns ins;
   expr_discharge(fs, e);
   if (e->k IS ExpKind::Str) {
      ins = BCINS_AD(BC_KSTR, reg, const_str(fs, e));
   }
   else if (e->k IS ExpKind::Num) {
#if LJ_DUALNUM
      cTValue* tv = e->num_tv();
      if (tvisint(tv) and checki16(intV(tv)))
         ins = BCINS_AD(BC_KSHORT, reg, BCREG(uint16_t(intV(tv))));
      else
#else
      lua_Number n = e->number_value();
      int32_t k = lj_num2int(n);
      if (checki16(k) and n IS lua_Number(k))
         ins = BCINS_AD(BC_KSHORT, reg, BCREG(uint16_t(k)));
      else
#endif
         ins = BCINS_AD(BC_KNUM, reg, const_num(fs, e));
#if LJ_HASFFI
   }
   else if (e->k IS ExpKind::CData) {
      fs->flags |= PROTO_FFI;
      ins = BCINS_AD(BC_KCDATA, reg,
         const_gc(fs, obj2gco(cdataV(&e->u.nval)), LJ_TCDATA));
#endif
   }
   else if (e->k IS ExpKind::Relocable) {
      setbc_a(bcptr(fs, e), reg);
      goto noins;
   }
   else if (e->k IS ExpKind::NonReloc) {
      if (reg IS e->u.s.info) goto noins;
      ins = BCINS_AD(BC_MOV, reg, e->u.s.info);
   }
   else if (e->k IS ExpKind::Nil) {
      bcemit_nil(fs, reg, 1);
      goto noins;
   }
   else if (e->k <= ExpKind::True) {
      ins = BCINS_AD(BC_KPRI, reg, const_pri(e));
   }
   else {
      fs->assert(e->k IS ExpKind::Void or e->k IS ExpKind::Jmp, "bad expr type %d", int(e->k));
      return;
   }

   bcemit_INS(fs, ins);

noins:
   e->u.s.info = reg;
   e->k = ExpKind::NonReloc;
}

//********************************************************************************************************************
// Discharge an expression to a specific register.

static void expr_toreg(FuncState *fs, ExpDesc *e, BCREG reg)
{
   expr_toreg_nobranch(fs, e, reg);
   ControlFlowGraph cfg(fs);

   if (e->k IS ExpKind::Jmp) {
      ControlFlowEdge true_edge = cfg.make_true_edge(BCPos(e->t));
      true_edge.append(BCPos(e->u.s.info));
      e->t = true_edge.head().raw();
   }

   if (e->has_jump()) {  // Discharge expression with branches.
      BCPOS jend, jfalse = NO_JMP, jtrue = NO_JMP;
      ControlFlowEdge true_edge = cfg.make_true_edge(BCPos(e->t));
      ControlFlowEdge false_edge = cfg.make_false_edge(BCPos(e->f));

      if (true_edge.produces_values() or false_edge.produces_values()) {
         BCPOS jval = (e->k IS ExpKind::Jmp) ? NO_JMP : bcemit_jmp(fs);
         jfalse = bcemit_AD(fs, BC_KPRI, reg, BCREG(ExpKind::False));
         bcemit_AJ(fs, BC_JMP, fs->freereg, 1);
         jtrue = bcemit_AD(fs, BC_KPRI, reg, BCREG(ExpKind::True));
         ControlFlowEdge jval_edge = cfg.make_unconditional(BCPos(jval));
         jval_edge.patch_here();
      }

      jend = fs->pc;
      fs->lasttarget = jend;
      false_edge.patch_with_value(BCPos(jend), BCReg(reg), BCPos(jfalse));
      true_edge.patch_with_value(BCPos(jend), BCReg(reg), BCPos(jtrue));
   }

   e->f = e->t = NO_JMP;
   e->u.s.info = reg;
   e->k = ExpKind::NonReloc;
}

//********************************************************************************************************************
// Discharge an expression to the next free register.

static void expr_tonextreg(FuncState *fs, ExpDesc *e)
{
   expr_discharge(fs, e);
   expr_free(fs, e);
   bcreg_reserve(fs, 1);
   expr_toreg(fs, e, fs->freereg - 1);
}

//********************************************************************************************************************
// Discharge an expression to any register.

static BCREG expr_toanyreg(FuncState *fs, ExpDesc *e)
{
   expr_discharge(fs, e);
   if (e->k IS ExpKind::NonReloc) [[likely]] {
      if (!e->has_jump()) [[likely]] return e->u.s.info;  // Already in a register.
      if (e->u.s.info >= fs->nactvar) {
         expr_toreg(fs, e, e->u.s.info);  // Discharge to temp. register.
         return e->u.s.info;
      }
   }
   expr_tonextreg(fs, e);  // Discharge to next register.
   return e->u.s.info;
}

//********************************************************************************************************************
// Partially discharge expression to a value.

static void expr_toval(FuncState *fs, ExpDesc *e)
{
   if (e->has_jump()) [[unlikely]]
      expr_toanyreg(fs, e);
   else [[likely]]
      expr_discharge(fs, e);
}

//********************************************************************************************************************
// Emit store for LHS expression.

static void bcemit_store(FuncState *fs, ExpDesc* var, ExpDesc *e)
{
   BCIns ins;
   if (var->k IS ExpKind::Local) {
      fs->ls->vstack[var->u.s.aux].info |= VarInfoFlag::VarReadWrite;
      expr_free(fs, e);
      expr_toreg(fs, e, var->u.s.info);
      return;
   }
   else if (var->k IS ExpKind::Upval) {
      fs->ls->vstack[var->u.s.aux].info |= VarInfoFlag::VarReadWrite;
      expr_toval(fs, e);
      if (e->k <= ExpKind::True) ins = BCINS_AD(BC_USETP, var->u.s.info, const_pri(e));
      else if (e->k IS ExpKind::Str) ins = BCINS_AD(BC_USETS, var->u.s.info, const_str(fs, e));
      else if (e->k IS ExpKind::Num) ins = BCINS_AD(BC_USETN, var->u.s.info, const_num(fs, e));
      else ins = BCINS_AD(BC_USETV, var->u.s.info, expr_toanyreg(fs, e));
   }
   else if (var->k IS ExpKind::Global or var->k IS ExpKind::Unscoped) {
      // Unscoped should normally be resolved in emit_lvalue_expr(), but handle it here defensively
      BCREG ra = expr_toanyreg(fs, e);
      ins = BCINS_AD(BC_GSET, ra, const_str(fs, var));
   }
   else {
      BCREG ra, rc;
      fs->assert(var->k IS ExpKind::Indexed, "bad expr type %d", int(var->k));
      ra = expr_toanyreg(fs, e);
      rc = var->u.s.aux;
      if (int32_t(rc) < 0) ins = BCINS_ABC(BC_TSETS, ra, var->u.s.info, ~rc);
      else if (rc > BCMAX_C) ins = BCINS_ABC(BC_TSETB, ra, var->u.s.info, rc - (BCMAX_C + 1));
      else {
#ifdef LUA_USE_ASSERT
         // Free late alloced key reg to avoid assert on free of value reg.
         // This can only happen when called from expr_table().
         if (e->k IS ExpKind::NonReloc and ra >= fs->nactvar and rc >= ra)
            bcreg_free(fs, rc);
#endif
         ins = BCINS_ABC(BC_TSETV, ra, var->u.s.info, rc);
      }
   }
   bcemit_INS(fs, ins);
   expr_free(fs, e);
}

//********************************************************************************************************************
// Emit method lookup expression.

static void bcemit_method(FuncState *fs, ExpDesc *e, ExpDesc *key)
{
   BCREG idx, func, obj = expr_toanyreg(fs, e);
   expr_free(fs, e);
   func = fs->freereg;
   bcemit_AD(fs, BC_MOV, func + 1 + LJ_FR2, obj);  // Copy object to 1st argument.
   fs->assert(key->is_str_constant(), "bad usage");
   idx = const_str(fs, key);
   if (idx <= BCMAX_C) {
      bcreg_reserve(fs, 2 + LJ_FR2);
      bcemit_ABC(fs, BC_TGETS, func, obj, idx);
   }
   else {
      bcreg_reserve(fs, 3 + LJ_FR2);
      bcemit_AD(fs, BC_KSTR, func + 2 + LJ_FR2, idx);
      bcemit_ABC(fs, BC_TGETV, func, obj, func + 2 + LJ_FR2);
      fs->freereg--;
   }
   e->u.s.info = func;
   e->k = ExpKind::NonReloc;
}

//********************************************************************************************************************
// Emit unconditional branch.

[[nodiscard]] BCPOS bcemit_jmp(FuncState *fs)
{
   BCPOS jpc = fs->jpc;
   BCPOS j = fs->pc - 1;
   BCIns* ip = &fs->bcbase[j].ins;
   fs->clear_pending_jumps();
   if (int32_t(j) >= int32_t(fs->lasttarget) and bc_op(*ip) IS BC_UCLO) {
      setbc_j(ip, NO_JMP);
      fs->lasttarget = j + 1;
   }
   else j = bcemit_AJ(fs, BC_JMP, fs->freereg, NO_JMP);
   ControlFlowGraph cfg(fs);
   ControlFlowEdge edge = cfg.make_unconditional(BCPos(j));
   edge.append(BCPos(jpc));
   return edge.head().raw();
}

//********************************************************************************************************************
// Invert branch condition of bytecode instruction.

inline void invertcond(FuncState *fs, ExpDesc *e)
{
   BCIns* ip = &fs->bytecode_at(BCPos(e->u.s.info - 1)).ins;
   setbc_op(ip, bc_op(*ip) ^ 1);
}

//********************************************************************************************************************
// Emit conditional branch.

[[nodiscard]] BCPOS bcemit_branch(FuncState *fs, ExpDesc *e, int cond)
{
   BCPOS pc;

   if (e->k IS ExpKind::Relocable) {
      BCIns* ip = bcptr(fs, e);
      if (bc_op(*ip) IS BC_NOT) {
         *ip = BCINS_AD(cond ? BC_ISF : BC_IST, 0, bc_d(*ip));
         return bcemit_jmp(fs);
      }
   }

   if (e->k != ExpKind::NonReloc) {
      bcreg_reserve(fs, 1);
      expr_toreg_nobranch(fs, e, fs->freereg - 1);
   }

   bcemit_AD(fs, cond ? BC_ISTC : BC_ISFC, NO_REG, e->u.s.info);
   pc = bcemit_jmp(fs);
   expr_free(fs, e);
   return pc;
}

//********************************************************************************************************************
// Emit branch on true condition.

static void bcemit_branch_t(FuncState *fs, ExpDesc *e)
{
   BCPOS pc;
   expr_discharge(fs, e);
   if (e->k IS ExpKind::Str or e->k IS ExpKind::Num or e->k IS ExpKind::True) pc = NO_JMP;  // Never jump.
   else if (e->k IS ExpKind::Jmp) invertcond(fs, e), pc = e->u.s.info;
   else if (e->k IS ExpKind::False or e->k IS ExpKind::Nil) expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
   else pc = bcemit_branch(fs, e, 0);
   ControlFlowGraph cfg(fs);
   ControlFlowEdge false_edge = cfg.make_false_edge(BCPos(e->f));
   false_edge.append(BCPos(pc));
   e->f = false_edge.head().raw();
   ControlFlowEdge true_edge = cfg.make_true_edge(BCPos(e->t));
   true_edge.patch_here();
   e->t = NO_JMP;
}

//********************************************************************************************************************
// Debug verification and tracing methods

void RegisterAllocator::verify_no_leaks(const char* Context) const
{
   BCREG nactvar = this->func_state->nactvar;
   BCREG freereg = this->func_state->freereg;

   if (freereg > nactvar) {
      pf::Log("Parser").warning("Register leak at %s: %d temporary registers not released (nactvar=%d, freereg=%d)",
         Context, int(freereg - nactvar), int(nactvar), int(freereg));
   }
}

void RegisterAllocator::trace_allocation(BCReg Start, BCReg Count, const char* Context) const
{
   auto prv = (prvFluid *)this->func_state->L->script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_REGISTERS) != JOF::NIL) {
      pf::Log("Parser").msg("Regalloc: reserve R%d..R%d (%d slots) at %s",
         int(Start.raw()), int(Start.raw() + Count.raw() - 1), int(Count.raw()), Context);
   }
}

void RegisterAllocator::trace_release(BCReg Start, BCReg Count, const char* Context) const
{
   auto prv = (prvFluid *)this->func_state->L->script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_REGISTERS) != JOF::NIL) {
      pf::Log("Parser").msg("Regalloc: release R%d..R%d (%d slots) at %s",
         int(Start.raw()), int(Start.raw() + Count.raw() - 1), int(Count.raw()), Context);
   }
}

//********************************************************************************************************************
// Expression management methods - migrated from static functions to RegisterAllocator methods

void RegisterAllocator::discharge(ExpDesc& Expression)
{
   expr_discharge(this->func_state, &Expression);
}

void RegisterAllocator::discharge_to_register(ExpDesc& Expression, BCReg Target)
{
   expr_toreg(this->func_state, &Expression, Target.raw());
}

void RegisterAllocator::discharge_to_register_nobranch(ExpDesc& Expression, BCReg Target)
{
   expr_toreg_nobranch(this->func_state, &Expression, Target.raw());
}

void RegisterAllocator::discharge_to_next_register(ExpDesc& Expression)
{
   expr_tonextreg(this->func_state, &Expression);
}

BCReg RegisterAllocator::discharge_to_any_register(ExpDesc& Expression)
{
   return BCReg(expr_toanyreg(this->func_state, &Expression));
}

void RegisterAllocator::discharge_to_value(ExpDesc& Expression)
{
   expr_toval(this->func_state, &Expression);
}

void RegisterAllocator::store_value(ExpDesc& Variable, ExpDesc& Value)
{
   bcemit_store(this->func_state, &Variable, &Value);
}

void RegisterAllocator::emit_nil_range(BCReg Start, BCReg Count)
{
   bcemit_nil(this->func_state, Start.raw(), Count.raw());
}
