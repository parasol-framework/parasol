/*
** Lua parser - Constant management and jump list handling.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

// Add a number constant.

[[nodiscard]] static BCReg const_num(FuncState* fs, ExpDesc* e)
{
   lua_State* L = fs->L;
   TValue* o;
   lj_assertFS(expr_isnumk(e), "bad usage");
   o = lj_tab_set(L, fs->kt, &e->u.nval);
   if (tvhaskslot(o)) return tvkslot(o);
   o->u64 = fs->nkn;
   return fs->nkn++;
}

// Add a GC object constant.

[[nodiscard]] static BCReg const_gc(FuncState* fs, GCobj* gc, uint32_t itype)
{
   lua_State* L = fs->L;
   TValue key, * o;
   setgcV(L, &key, gc, itype);
   // NOBARRIER: the key is new or kept alive.
   o = lj_tab_set(L, fs->kt, &key);
   if (tvhaskslot(o)) return tvkslot(o);
   o->u64 = fs->nkgc;
   return fs->nkgc++;
}

// Add a string constant.

[[nodiscard]] static BCReg const_str(FuncState* fs, ExpDesc* e)
{
   lj_assertFS(expr_isstrk(e) or e->k == ExpKind::Global, "bad usage");
   return const_gc(fs, obj2gco(e->u.sval), LJ_TSTR);
}

// Anchor string constant to avoid GC.

GCstr* lj_parse_keepstr(LexState* ls, const char* str, size_t len)
{
   // NOBARRIER: the key is new or kept alive.
   lua_State* L = ls->L;
   GCstr* s = lj_str_new(L, str, len);
   TValue* tv = lj_tab_setstr(L, ls->fs->kt, s);
   if (tvisnil(tv)) setboolV(tv, 1);
   lj_gc_check(L);
   return s;
}

#if LJ_HASFFI
// Anchor cdata to avoid GC.
void lj_parse_keepcdata(LexState* ls, TValue* tv, GCcdata* cd)
{
   // NOBARRIER: the key is new or kept alive.
   lua_State* L = ls->L;
   setcdataV(L, tv, cd);
   setboolV(lj_tab_set(L, ls->fs->kt, tv), 1);
}
#endif

// -- Jump list handling

// Get next element in jump list.

[[nodiscard]] static BCPos jmp_next(FuncState* fs, BCPos pc)
{
   ptrdiff_t delta = bc_j(fs->bcbase[pc].ins);
   if (BCPos(delta) == NO_JMP) return NO_JMP;
   else return BCPos((ptrdiff_t(pc) + 1) + delta);
}

// Check if any of the instructions on the jump list produce no value.

[[nodiscard]] static int jmp_novalue(FuncState* fs, BCPos list)
{
   for (; list != NO_JMP; list = jmp_next(fs, list)) {
      BCIns p = fs->bcbase[list >= 1 ? list - 1 : list].ins;
      if (!(bc_op(p) == BC_ISTC or bc_op(p) == BC_ISFC or bc_a(p) == NO_REG))
         return 1;
   }
   return 0;
}

// Patch register of test instructions.

[[nodiscard]] static int jmp_patchtestreg(FuncState* fs, BCPos pc, BCReg reg)
{
   BCInsLine* ilp = &fs->bcbase[pc >= 1 ? pc - 1 : pc];
   BCOp op = bc_op(ilp->ins);
   if (op == BC_ISTC or op == BC_ISFC) {
      if (reg != NO_REG and reg != bc_d(ilp->ins)) {
         setbc_a(&ilp->ins, reg);
      }
      else {  // Nothing to store or already in the right register.
         setbc_op(&ilp->ins, op + (BC_IST - BC_ISTC));
         setbc_a(&ilp->ins, 0);
      }
   }
   else if (bc_a(ilp->ins) == NO_REG) {
      if (reg == NO_REG) {
         ilp->ins = BCINS_AJ(BC_JMP, bc_a(fs->bcbase[pc].ins), 0);
      }
      else {
         setbc_a(&ilp->ins, reg);
         if (reg >= bc_a(ilp[1].ins))
            setbc_a(&ilp[1].ins, reg + 1);
      }
   }
   else return 0;  // Cannot patch other instructions.
   return 1;
}

// Drop values for all instructions on jump list.

static void jmp_dropval(FuncState* fs, BCPos list)
{
   for (; list != NO_JMP; list = jmp_next(fs, list))
      (void)jmp_patchtestreg(fs, list, NO_REG);
}

// Patch jump instruction to target.

static void jmp_patchins(FuncState* fs, BCPos pc, BCPos dest)
{
   BCIns* jmp = &fs->bcbase[pc].ins;
   BCPos offset = dest - (pc + 1) + BCBIAS_J;
   lj_assertFS(dest != NO_JMP, "uninitialized jump target");
   if (offset > BCMAX_D)
      err_syntax(fs->ls, LJ_ERR_XJUMP);
   setbc_d(jmp, offset);
}

// Append to jump list.

static void jmp_append(FuncState* fs, BCPos* l1, BCPos l2)
{
   if (l2 == NO_JMP) return;
   else if (*l1 == NO_JMP) *l1 = l2;
   else {
      BCPos list = *l1;
      BCPos next;
      while ((next = jmp_next(fs, list)) != NO_JMP)  // Find last element.
         list = next;
      jmp_patchins(fs, list, l2);
   }
}

// Patch jump list and preserve produced values.

static void jmp_patchval(FuncState* fs, BCPos list, BCPos vtarget,
   BCReg reg, BCPos dtarget)
{
   while (list != NO_JMP) {
      BCPos next = jmp_next(fs, list);
      if (jmp_patchtestreg(fs, list, reg)) jmp_patchins(fs, list, vtarget);  // Jump to target with value.
      else jmp_patchins(fs, list, dtarget);  // Jump to default target.
      list = next;
   }
}

// Jump to following instruction. Append to list of pending jumps.

static void jmp_tohere(FuncState* fs, BCPos list)
{
   fs->lasttarget = fs->pc;
   jmp_append(fs, &fs->jpc, list);
}

// Patch jump list to target.

static void jmp_patch(FuncState* fs, BCPos list, BCPos target)
{
   if (target == fs->pc) {
      jmp_tohere(fs, list);
   }
   else {
      lj_assertFS(target < fs->pc, "bad jump target");
      jmp_patchval(fs, list, target, NO_REG, target);
   }
}
