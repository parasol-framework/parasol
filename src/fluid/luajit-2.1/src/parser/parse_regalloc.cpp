// Lua parser - Register allocation and bytecode emission.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#include "parser/parse_regalloc.h"

#include <parasol/main.h>

static bool is_register_key(int32_t Aux)
{
   return Aux >= 0 and Aux <= BCMAX_C;
}

//********************************************************************************************************************
// Constructor

RegisterAllocator::RegisterAllocator(FuncState* State) : func_state(State)
{
}

void RegisterAllocator::bump(BCReg Count)
{
   BCReg target = this->func_state->freereg + Count;
   if (target > this->func_state->framesize) {
      if (target >= LJ_MAX_SLOTS) this->func_state->ls->err_syntax(ErrMsg::XSLOTS);
      this->func_state->framesize = uint8_t(target);
   }
}

BCReg RegisterAllocator::reserve_slots(BCReg Count)
{
   if (Count IS 0) return this->func_state->freereg;

   BCReg start = this->func_state->freereg;
   this->bump(Count);
   this->func_state->freereg += Count;
#if LJ_DEBUG
   this->trace_allocation(start, Count, "reserve_slots");
#endif
   return start;
}

void RegisterAllocator::reserve(BCReg Count)
{
   this->reserve_slots(Count);
}

AllocatedRegister RegisterAllocator::acquire()
{
   BCReg start = this->reserve_slots(1);
   return AllocatedRegister(this, start, start + 1);
}

RegisterSpan RegisterAllocator::reserve_span(BCReg Count)
{
   if (Count IS 0) return RegisterSpan();

   BCReg start = this->reserve_slots(Count);
   return RegisterSpan(this, start, Count, start + Count);
}

void RegisterAllocator::release_span_internal(BCReg Start, BCReg Count, BCReg ExpectedTop)
{
   if (Count IS 0) return;

   if (Start >= this->func_state->nactvar) {
#if LJ_DEBUG
      lj_assertFS(ExpectedTop IS this->func_state->freereg, "register depth mismatch");
      lj_assertFS(Start + Count IS ExpectedTop, "span size mismatch");
#endif
      this->func_state->freereg = ExpectedTop - Count;
#if LJ_DEBUG
      lj_assertFS(this->func_state->freereg IS Start, "bad regfree");
      this->trace_release(Start, Count, "release_span_internal");
#endif
   }
}

void RegisterAllocator::release(RegisterSpan& Span)
{
   if (Span.allocator_) {
      this->release_span_internal(Span.start_, Span.count_, Span.expected_top_);
      Span.allocator_ = nullptr;
   }
}

void RegisterAllocator::release(AllocatedRegister& Handle)
{
   if (Handle.allocator_) {
      this->release_span_internal(Handle.index_, 1, Handle.expected_top_);
      Handle.allocator_ = nullptr;
   }
}

void RegisterAllocator::release_register(BCReg Register)
{
   this->release_span_internal(Register, 1, this->func_state->freereg);
}

void RegisterAllocator::release_expression(ExpDesc* Expression)
{
   if (Expression->k IS ExpKind::NonReloc) {
      BCReg expected_top = Expression->u.s.info + 1;
      this->release_span_internal(Expression->u.s.info, 1, expected_top);
   }
}

TableOperandCopies RegisterAllocator::duplicate_table_operands(const ExpDesc& Expression)
{
   TableOperandCopies copies{};
   copies.duplicated = Expression;

   if (Expression.k IS ExpKind::Indexed) {
      uint32_t original_aux = Expression.u.s.aux;
      BCReg duplicate_count = 1;
      bool has_register_index = is_register_key(int32_t(original_aux));

      if (has_register_index) duplicate_count++;

      copies.reserved = this->reserve_span(duplicate_count);

      BCReg base_reg = copies.reserved.start();
      bcemit_AD(this->func_state, BC_MOV, base_reg, Expression.u.s.info);
      copies.duplicated.u.s.info = base_reg;

      if (has_register_index) {
         BCReg index_reg = BCReg(base_reg + 1);
         bcemit_AD(this->func_state, BC_MOV, index_reg, BCReg(original_aux));
         copies.duplicated.u.s.aux = index_reg;
      }
   }

   return copies;
}

void RegisterSpan::release()
{
   if (allocator_) allocator_->release(*this);
}

void AllocatedRegister::release()
{
   if (allocator_) allocator_->release(*this);
}

// Bump frame size.

static void bcreg_bump(FuncState* fs, BCReg n)
{
   RegisterAllocator allocator(fs);
   allocator.bump(n);
}

//********************************************************************************************************************
// Reserve registers.

static void bcreg_reserve(FuncState* fs, BCReg n)
{
   RegisterAllocator allocator(fs);
   allocator.reserve(n);
}

//********************************************************************************************************************
// Free register.

static void bcreg_free(FuncState* fs, BCReg reg)
{
   RegisterAllocator allocator(fs);
   allocator.release_register(reg);
}

//********************************************************************************************************************
// Free register for expression.

static void expr_free(FuncState* fs, ExpDesc* e)
{
   RegisterAllocator allocator(fs);
   allocator.release_expression(e);
}

//********************************************************************************************************************
// Emit bytecode instruction.

static BCPos bcemit_INS(FuncState* fs, BCIns ins)
{
   BCPos pc = fs->pc;
   LexState* ls = fs->ls;
   ControlFlowGraph cfg(fs);
   ControlFlowEdge pending = cfg.make_unconditional(fs->jpc);
   pending.patch_with_value(pc, NO_REG, pc);
   fs->jpc = NO_JMP;
   if (pc >= fs->bclim) [[unlikely]] {
      ptrdiff_t base = fs->bcbase - ls->bcstack;
      checklimit(fs, ls->sizebcstack, LJ_MAX_BCINS, "bytecode instructions");
      lj_mem_growvec(fs->L, ls->bcstack, ls->sizebcstack, LJ_MAX_BCINS, BCInsLine);
      fs->bclim = BCPos(ls->sizebcstack - base);
      fs->bcbase = ls->bcstack + base;
   }
   fs->bcbase[pc].ins = ins;
   fs->bcbase[pc].line = ls->lastline;
   fs->pc = pc + 1;
   return pc;
}

//********************************************************************************************************************
// Get pointer to bytecode instruction for expression.

[[nodiscard]] static inline BCIns* bcptr(FuncState* fs, const ExpDesc* e) {
   return &fs->bcbase[e->u.s.info].ins;
}

//********************************************************************************************************************
// Bytecode emitter for expressions

[[nodiscard]] static BCPos bcemit_jmp(FuncState* fs);

// Discharge non-constant expression to any register.

static void expr_discharge(FuncState* fs, ExpDesc* e)
{
   BCIns ins;
   if (e->k == ExpKind::Upval) {
      ins = BCINS_AD(BC_UGET, 0, e->u.s.info);
   }
   else if (e->k == ExpKind::Global) {
      // Check if trying to read blank identifier.
      if (is_blank_identifier(e->u.sval)) {
         lj_lex_error(fs->ls, fs->ls->tok, ErrMsg::XNEAR,
            "cannot read blank identifier");
      }
      ins = BCINS_AD(BC_GGET, 0, const_str(fs, e));
   }
   else if (e->k == ExpKind::Indexed) {
      BCReg rc = e->u.s.aux;
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
   else if (e->k == ExpKind::Call) {
      e->u.s.info = e->u.s.aux;
      e->k = ExpKind::NonReloc;
      return;
   }
   else if (e->k == ExpKind::Local) {
      e->k = ExpKind::NonReloc;
      return;
   }
   else return;

   e->u.s.info = bcemit_INS(fs, ins);
   e->k = ExpKind::Relocable;
}

//********************************************************************************************************************
// Emit bytecode to set a range of registers to nil.

static void bcemit_nil(FuncState* fs, BCReg from, BCReg n)
{
   if (fs->pc > fs->lasttarget) {  // No jumps to current position?
      BCIns* ip = &fs->bcbase[fs->pc - 1].ins;
      BCReg pto, pfrom = bc_a(*ip);
      switch (bc_op(*ip)) {  // Try to merge with the previous instruction.
      case BC_KPRI:
         if (bc_d(*ip) != ~LJ_TNIL) break;
         if (from == pfrom) {
            if (n == 1) return;
         }
         else if (from == pfrom + 1) {
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
   bcemit_INS(fs, n == 1 ? BCINS_AD(BC_KPRI, from, ExpKind::Nil) :
      BCINS_AD(BC_KNIL, from, from + n - 1));
}

//********************************************************************************************************************
// Discharge an expression to a specific register. Ignore branches.

static void expr_toreg_nobranch(FuncState* fs, ExpDesc* e, BCReg reg)
{
   BCIns ins;
   expr_discharge(fs, e);
   if (e->k == ExpKind::Str) {
      ins = BCINS_AD(BC_KSTR, reg, const_str(fs, e));
   }
   else if (e->k == ExpKind::Num) {
#if LJ_DUALNUM
      cTValue* tv = expr_numtv(e);
      if (tvisint(tv) and checki16(intV(tv)))
         ins = BCINS_AD(BC_KSHORT, reg, BCReg(uint16_t(intV(tv))));
      else
#else
      lua_Number n = expr_numberV(e);
      int32_t k = lj_num2int(n);
      if (checki16(k) and n == lua_Number(k))
         ins = BCINS_AD(BC_KSHORT, reg, BCReg(uint16_t(k)));
      else
#endif
         ins = BCINS_AD(BC_KNUM, reg, const_num(fs, e));
#if LJ_HASFFI
   }
   else if (e->k == ExpKind::CData) {
      fs->flags |= PROTO_FFI;
      ins = BCINS_AD(BC_KCDATA, reg,
         const_gc(fs, obj2gco(cdataV(&e->u.nval)), LJ_TCDATA));
#endif
   }
   else if (e->k == ExpKind::Relocable) {
      setbc_a(bcptr(fs, e), reg);
      goto noins;
   }
   else if (e->k == ExpKind::NonReloc) {
      if (reg == e->u.s.info) goto noins;
      ins = BCINS_AD(BC_MOV, reg, e->u.s.info);
   }
   else if (e->k == ExpKind::Nil) {
      bcemit_nil(fs, reg, 1);
      goto noins;
   }
   else if (e->k <= ExpKind::True) {
      ins = BCINS_AD(BC_KPRI, reg, const_pri(e));
   }
   else {
      lj_assertFS(e->k == ExpKind::Void or e->k == ExpKind::Jmp, "bad expr type %d", static_cast<int>(e->k));
      return;
   }
   bcemit_INS(fs, ins);
noins:
   e->u.s.info = reg;
   e->k = ExpKind::NonReloc;
}

//********************************************************************************************************************
// Discharge an expression to a specific register.

static void expr_toreg(FuncState* fs, ExpDesc* e, BCReg reg)
{
   expr_toreg_nobranch(fs, e, reg);
   ControlFlowGraph cfg(fs);
   if (e->k IS ExpKind::Jmp) {
      ControlFlowEdge true_edge = cfg.make_true_edge(e->t);
      true_edge.append(e->u.s.info);
      e->t = true_edge.head();
   }
   if (expr_hasjump(e)) {  // Discharge expression with branches.
      BCPos jend, jfalse = NO_JMP, jtrue = NO_JMP;
      ControlFlowEdge true_edge = cfg.make_true_edge(e->t);
      ControlFlowEdge false_edge = cfg.make_false_edge(e->f);
      if (true_edge.produces_values() or false_edge.produces_values()) {
         BCPos jval = (e->k IS ExpKind::Jmp) ? NO_JMP : bcemit_jmp(fs);
         jfalse = bcemit_AD(fs, BC_KPRI, reg, BCReg(ExpKind::False));
         bcemit_AJ(fs, BC_JMP, fs->freereg, 1);
         jtrue = bcemit_AD(fs, BC_KPRI, reg, BCReg(ExpKind::True));
         ControlFlowEdge jval_edge = cfg.make_unconditional(jval);
         jval_edge.patch_here();
      }
      jend = fs->pc;
      fs->lasttarget = jend;
      false_edge.patch_with_value(jend, reg, jfalse);
      true_edge.patch_with_value(jend, reg, jtrue);
   }
   e->f = e->t = NO_JMP;
   e->u.s.info = reg;
   e->k = ExpKind::NonReloc;
}

//********************************************************************************************************************
// Discharge an expression to the next free register.

static void expr_tonextreg(FuncState* fs, ExpDesc* e)
{
   expr_discharge(fs, e);
   expr_free(fs, e);
   bcreg_reserve(fs, 1);
   expr_toreg(fs, e, fs->freereg - 1);
}

//********************************************************************************************************************
// Discharge an expression to any register.

static BCReg expr_toanyreg(FuncState* fs, ExpDesc* e)
{
   expr_discharge(fs, e);
   if (e->k == ExpKind::NonReloc) [[likely]] {
      if (!expr_hasjump(e)) [[likely]] return e->u.s.info;  // Already in a register.
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

static void expr_toval(FuncState* fs, ExpDesc* e)
{
   if (expr_hasjump(e)) [[unlikely]]
      expr_toanyreg(fs, e);
   else [[likely]]
      expr_discharge(fs, e);
}

//********************************************************************************************************************
// Emit store for LHS expression.

static void bcemit_store(FuncState* fs, ExpDesc* var, ExpDesc* e)
{
   BCIns ins;
   if (var->k == ExpKind::Local) {
      fs->ls->vstack[var->u.s.aux].info |= VarInfoFlag::VarReadWrite;
      expr_free(fs, e);
      expr_toreg(fs, e, var->u.s.info);
      return;
   }
   else if (var->k == ExpKind::Upval) {
      fs->ls->vstack[var->u.s.aux].info |= VarInfoFlag::VarReadWrite;
      expr_toval(fs, e);
      if (e->k <= ExpKind::True) ins = BCINS_AD(BC_USETP, var->u.s.info, const_pri(e));
      else if (e->k == ExpKind::Str) ins = BCINS_AD(BC_USETS, var->u.s.info, const_str(fs, e));
      else if (e->k == ExpKind::Num) ins = BCINS_AD(BC_USETN, var->u.s.info, const_num(fs, e));
      else ins = BCINS_AD(BC_USETV, var->u.s.info, expr_toanyreg(fs, e));
   }
   else if (var->k == ExpKind::Global) {
      BCReg ra = expr_toanyreg(fs, e);
      ins = BCINS_AD(BC_GSET, ra, const_str(fs, var));
   }
   else {
      BCReg ra, rc;
      lj_assertFS(var->k == ExpKind::Indexed, "bad expr type %d", static_cast<int>(var->k));
      ra = expr_toanyreg(fs, e);
      rc = var->u.s.aux;
      if (int32_t(rc) < 0) ins = BCINS_ABC(BC_TSETS, ra, var->u.s.info, ~rc);
      else if (rc > BCMAX_C) ins = BCINS_ABC(BC_TSETB, ra, var->u.s.info, rc - (BCMAX_C + 1));
      else {
#ifdef LUA_USE_ASSERT
         // Free late alloced key reg to avoid assert on free of value reg.
         // This can only happen when called from expr_table().
         if (e->k == ExpKind::NonReloc and ra >= fs->nactvar and rc >= ra)
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

static void bcemit_method(FuncState* fs, ExpDesc* e, ExpDesc* key)
{
   BCReg idx, func, obj = expr_toanyreg(fs, e);
   expr_free(fs, e);
   func = fs->freereg;
   bcemit_AD(fs, BC_MOV, func + 1 + LJ_FR2, obj);  // Copy object to 1st argument.
   lj_assertFS(expr_isstrk(key), "bad usage");
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

[[nodiscard]] static BCPos bcemit_jmp(FuncState* fs)
{
   BCPos jpc = fs->jpc;
   BCPos j = fs->pc - 1;
   BCIns* ip = &fs->bcbase[j].ins;
   fs->jpc = NO_JMP;
   if (int32_t(j) >= int32_t(fs->lasttarget) and bc_op(*ip) == BC_UCLO) {
      setbc_j(ip, NO_JMP);
      fs->lasttarget = j + 1;
   }
   else j = bcemit_AJ(fs, BC_JMP, fs->freereg, NO_JMP);
   ControlFlowGraph cfg(fs);
   ControlFlowEdge edge = cfg.make_unconditional(j);
   edge.append(jpc);
   return edge.head();
}

//********************************************************************************************************************
// Invert branch condition of bytecode instruction.

static void invertcond(FuncState* fs, ExpDesc* e)
{
   BCIns* ip = &fs->bcbase[e->u.s.info - 1].ins;
   setbc_op(ip, bc_op(*ip) ^ 1);
}

//********************************************************************************************************************
// Emit conditional branch.

[[nodiscard]] static BCPos bcemit_branch(FuncState* fs, ExpDesc* e, int cond)
{
   BCPos pc;

   if (e->k == ExpKind::Relocable) {
      BCIns* ip = bcptr(fs, e);
      if (bc_op(*ip) == BC_NOT) {
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

static void bcemit_branch_t(FuncState* fs, ExpDesc* e)
{
   BCPos pc;
   expr_discharge(fs, e);
   if (e->k == ExpKind::Str or e->k == ExpKind::Num or e->k == ExpKind::True) pc = NO_JMP;  // Never jump.
   else if (e->k == ExpKind::Jmp) invertcond(fs, e), pc = e->u.s.info;
   else if (e->k == ExpKind::False or e->k == ExpKind::Nil) expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
   else pc = bcemit_branch(fs, e, 0);
   ControlFlowGraph cfg(fs);
   ControlFlowEdge false_edge = cfg.make_false_edge(e->f);
   false_edge.append(pc);
   e->f = false_edge.head();
   ControlFlowEdge true_edge = cfg.make_true_edge(e->t);
   true_edge.patch_here();
   e->t = NO_JMP;
}

//********************************************************************************************************************
// Emit branch on false condition.

static void bcemit_branch_f(FuncState* fs, ExpDesc* e)
{
   BCPos pc;
   expr_discharge(fs, e);

   if (e->k == ExpKind::Nil or e->k == ExpKind::False) pc = NO_JMP;  // Never jump.
   else if (e->k == ExpKind::Jmp) pc = e->u.s.info;
   else if (e->k == ExpKind::Str or e->k == ExpKind::Num or e->k == ExpKind::True) expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
   else pc = bcemit_branch(fs, e, 1);

   ControlFlowGraph cfg(fs);
   ControlFlowEdge true_edge = cfg.make_true_edge(e->t);
   true_edge.append(pc);
   e->t = true_edge.head();
   ControlFlowEdge false_edge = cfg.make_false_edge(e->f);
   false_edge.patch_here();
   e->f = NO_JMP;
}

//********************************************************************************************************************
// Debug verification and tracing methods (Phase 3 Stage 5)

#if LJ_DEBUG

void RegisterAllocator::verify_no_leaks(const char* Context) const
{
   BCReg nactvar = this->func_state->nactvar;
   BCReg freereg = this->func_state->freereg;

   if (freereg > nactvar) {
      lj_assertFS(false, "register leak at %s: %d temporary registers not released (nactvar=%d, freereg=%d)",
         Context, int(freereg - nactvar), int(nactvar), int(freereg));
   }
}

void RegisterAllocator::trace_allocation(BCReg Start, BCReg Count, const char* Context) const
{
   // Lightweight allocation tracing - can be enabled with LJ_TRACE_REGALLOC
#if defined(LJ_TRACE_REGALLOC)
   pf::Log log("Parser");
   log.trace("[REGALLOC] alloc reg %d..%d (%d slots) at %s",
      int(Start), int(Start + Count - 1), int(Count), Context);
#else
   (void)Start; (void)Count; (void)Context;
#endif
}

void RegisterAllocator::trace_release(BCReg Start, BCReg Count, const char* Context) const
{
   // Lightweight release tracing - can be enabled with LJ_TRACE_REGALLOC
#if defined(LJ_TRACE_REGALLOC)
   pf::Log log("Parser");
   log.trace("[REGALLOC] release reg %d..%d (%d slots) at %s",
      int(Start), int(Start + Count - 1), int(Count), Context);
#else
   (void)Start; (void)Count; (void)Context;
#endif
}

#endif
